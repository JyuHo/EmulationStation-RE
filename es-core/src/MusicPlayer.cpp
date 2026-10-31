#include "MusicPlayer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#include "Log.h"
#include "AudioManager.h"
#include "Settings.h"
#include "utils/Paths.h"

#include <algorithm>
#include <filesystem>
#include <random>

namespace fs = std::filesystem;

MusicPlayer& MusicPlayer::getInstance()
{
	static MusicPlayer instance;
	return instance;
}

MusicPlayer::MusicPlayer() = default;

void MusicPlayer::scanTracks()
{
	mTracks.clear();
	std::vector<fs::path> dirs;
	dirs.push_back(es::paths::userDir(es::paths::Kind::Data) / "music");
	for(const auto& base : es::paths::systemDirs(es::paths::Kind::Data))
		dirs.push_back(base / "emulationstation" / "music");

	const std::vector<std::string> exts = {".mp3", ".ogg", ".wav", ".flac", ".opus", ".m4a"};
	for(const auto& d : dirs)
	{
		std::error_code ec;
		if(!fs::exists(d, ec)) continue;
		for(auto& entry : fs::recursive_directory_iterator(d, fs::directory_options::skip_permission_denied, ec))
		{
			if(!entry.is_regular_file()) continue;
			std::string ext = entry.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
			if(std::find(exts.begin(), exts.end(), ext) != exts.end())
				mTracks.push_back(entry.path().string());
		}
	}
}

bool MusicPlayer::loadTrack(const std::string& path)
{
	mPcm.clear();
	AVFormatContext* fmt = nullptr;
	if(avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0) return false;
	if(avformat_find_stream_info(fmt, nullptr) < 0) { avformat_close_input(&fmt); return false; }
	int ai = -1;
	for(unsigned i = 0; i < fmt->nb_streams; i++)
		if(fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) { ai = (int)i; break; }
	if(ai < 0) { avformat_close_input(&fmt); return false; }
	AVCodecParameters* cp = fmt->streams[ai]->codecpar;
	const AVCodec* codec = avcodec_find_decoder(cp->codec_id);
	if(!codec) { avformat_close_input(&fmt); return false; }
	AVCodecContext* ctx = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(ctx, cp);
	if(avcodec_open2(ctx, codec, nullptr) < 0) { avcodec_free_context(&ctx); avformat_close_input(&fmt); return false; }
	AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
	SwrContext* swr = nullptr;
	swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_FLT, 48000, &ctx->ch_layout, ctx->sample_fmt, ctx->sample_rate, 0, nullptr);
	if(!swr || swr_init(swr) < 0) { if(swr) swr_free(&swr); avcodec_free_context(&ctx); avformat_close_input(&fmt); return false; }
	AVPacket* pkt = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();
	auto conv = [&](AVFrame* f) {
		int out = swr_get_out_samples(swr, f->nb_samples);
		if(out <= 0) return;
		std::vector<float> buf(out * 2);
		uint8_t* p = reinterpret_cast<uint8_t*>(buf.data());
		int c = swr_convert(swr, &p, out, (const uint8_t**)f->data, f->nb_samples);
		if(c > 0) mPcm.insert(mPcm.end(), buf.begin(), buf.begin() + c * 2);
	};
	while(av_read_frame(fmt, pkt) >= 0)
	{
		if(pkt->stream_index == ai && avcodec_send_packet(ctx, pkt) >= 0)
			while(avcodec_receive_frame(ctx, frame) >= 0) conv(frame);
		av_packet_unref(pkt);
	}
	avcodec_send_packet(ctx, nullptr);
	while(avcodec_receive_frame(ctx, frame) >= 0) conv(frame);
	av_frame_free(&frame);
	av_packet_free(&pkt);
	swr_free(&swr);
	avcodec_free_context(&ctx);
	avformat_close_input(&fmt);
	return !mPcm.empty();
}

void MusicPlayer::init()
{
	scanTracks();
}

void MusicPlayer::deinit()
{
	stop();
}

void MusicPlayer::play()
{
	if(!Settings::getInstance()->getBool("BackgroundMusic")) return;
	if(mTracks.empty()) scanTracks();
	if(mTracks.empty()) return;
	if(mPlaying) return;
	mCurrent = std::rand() % mTracks.size();
	if(!loadTrack(mTracks[mCurrent])) return;
	playCurrent();
}

void MusicPlayer::playCurrent()
{
	if(mStream) { SDL_UnbindAudioStream(mStream); SDL_DestroyAudioStream(mStream); mStream = nullptr; }
	SDL_AudioSpec src{SDL_AUDIO_F32, 2, 48000};
	mStream = SDL_CreateAudioStream(&src, &AudioManager::getFormat());
	if(!mStream) return;
	if(!SDL_BindAudioStream(AudioManager::getDevice(), mStream)) { SDL_DestroyAudioStream(mStream); mStream = nullptr; return; }
	SDL_PutAudioStreamData(mStream, mPcm.data(), (int)(mPcm.size() * sizeof(float)));
	float gain = Settings::getInstance()->getInt("MusicVolume") / 100.0f;
	SDL_SetAudioStreamGain(mStream, gain);
	SDL_ResumeAudioDevice(AudioManager::getDevice());
	mPlaying = true;
}

void MusicPlayer::stop()
{
	if(mStream) { SDL_UnbindAudioStream(mStream); SDL_DestroyAudioStream(mStream); mStream = nullptr; }
	mPlaying = false;
}

void MusicPlayer::pause()
{
	if(mStream) SDL_SetAudioStreamGain(mStream, 0.0f);
}

void MusicPlayer::resume()
{
	if(mStream)
	{
		float gain = Settings::getInstance()->getInt("MusicVolume") / 100.0f;
		SDL_SetAudioStreamGain(mStream, gain);
	}
}

bool MusicPlayer::isPlaying() const
{
	return mPlaying && mStream && SDL_GetAudioStreamQueued(mStream) > 0;
}

void MusicPlayer::update()
{
	if(!mPlaying) return;
	if(mStream && SDL_GetAudioStreamQueued(mStream) == 0)
	{
		// track finished, advance
		stop();
		play();
	}
}
