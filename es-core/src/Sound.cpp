#include "Sound.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include "AudioManager.h"
#include "Log.h"
#include "ThemeData.h"
#include "Util.h"
#include "utils/Paths.h"

#include <filesystem>

std::map<std::string, std::shared_ptr<Sound>> Sound::sMap;

Sound::Sound(const std::string& path) : mPath(path), mStream(nullptr), mPlaying(false) {}

Sound::~Sound()
{
	deinit();
}

std::shared_ptr<Sound> Sound::get(const std::string& path)
{
	auto it = sMap.find(path);
	if(it != sMap.end() && it->second)
		return it->second;

	std::shared_ptr<Sound> snd(new Sound(path));
	snd->init();
	sMap[path] = snd;
	AudioManager::getInstance()->registerSound(snd);
	return snd;
}

std::shared_ptr<Sound> Sound::getFromTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element)
{
	if(!theme)
		return get("");

	const ThemeData::ThemeElement* e = theme->getElement(view, element, "sound");
	if(!e || !e->has("path"))
		return get("");

	return get(e->get<std::string>("path"));
}

void Sound::init()
{
	loadFile(mPath);
}

void Sound::deinit()
{
	stop();
	if(mStream)
	{
		SDL_DestroyAudioStream(mStream);
		mStream = nullptr;
	}
}

void Sound::loadFile(const std::string& path)
{
	mPcm.clear();
	mPlaying = false;

	if(path.empty())
		return;

	AVFormatContext* fmt = nullptr;
	if(avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0)
	{
		LOG(LogError) << "Sound: cannot open " << path;
		return;
	}
	if(avformat_find_stream_info(fmt, nullptr) < 0)
	{
		avformat_close_input(&fmt);
		return;
	}

	int audioIndex = -1;
	for(unsigned int i = 0; i < fmt->nb_streams; i++)
	{
		if(fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audioIndex = (int)i;
			break;
		}
	}
	if(audioIndex < 0)
	{
		avformat_close_input(&fmt);
		return;
	}

	AVCodecParameters* cp = fmt->streams[audioIndex]->codecpar;
	const AVCodec* codec = avcodec_find_decoder(cp->codec_id);
	if(!codec)
	{
		avformat_close_input(&fmt);
		return;
	}
	AVCodecContext* ctx = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(ctx, cp);
	if(avcodec_open2(ctx, codec, nullptr) < 0)
	{
		avcodec_free_context(&ctx);
		avformat_close_input(&fmt);
		return;
	}

	AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
	SwrContext* swr = nullptr;
	swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_FLT, 48000,
		&ctx->ch_layout, ctx->sample_fmt, ctx->sample_rate, 0, nullptr);
	if(!swr || swr_init(swr) < 0)
	{
		if(swr) swr_free(&swr);
		avcodec_free_context(&ctx);
		avformat_close_input(&fmt);
		return;
	}

	AVPacket* pkt = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();

	auto convertFrame = [&](AVFrame* f) {
		int outSamples = swr_get_out_samples(swr, f->nb_samples);
		if(outSamples <= 0) return;
		std::vector<float> buf(outSamples * 2);
		uint8_t* outPtr = reinterpret_cast<uint8_t*>(buf.data());
		int converted = swr_convert(swr, &outPtr, outSamples, (const uint8_t**)f->data, f->nb_samples);
		if(converted > 0)
			mPcm.insert(mPcm.end(), buf.begin(), buf.begin() + converted * 2);
	};

	while(av_read_frame(fmt, pkt) >= 0)
	{
		if(pkt->stream_index == audioIndex)
		{
			if(avcodec_send_packet(ctx, pkt) >= 0)
			{
				while(avcodec_receive_frame(ctx, frame) >= 0)
					convertFrame(frame);
			}
		}
		av_packet_unref(pkt);
	}
	avcodec_send_packet(ctx, nullptr);
	while(avcodec_receive_frame(ctx, frame) >= 0)
		convertFrame(frame);

	av_frame_free(&frame);
	av_packet_free(&pkt);
	swr_free(&swr);
	avcodec_free_context(&ctx);
	avformat_close_input(&fmt);
}

void Sound::play()
{
	if(mPcm.empty())
		return;

	stop();

	SDL_AudioSpec srcSpec{SDL_AUDIO_F32, 2, 48000};
	mStream = SDL_CreateAudioStream(&srcSpec, &AudioManager::getFormat());
	if(!mStream)
	{
		LOG(LogError) << "Sound: cannot create stream: " << SDL_GetError();
		return;
	}
	if(!SDL_BindAudioStream(AudioManager::getDevice(), mStream))
	{
		LOG(LogError) << "Sound: cannot bind stream: " << SDL_GetError();
		SDL_DestroyAudioStream(mStream);
		mStream = nullptr;
		return;
	}
	SDL_PutAudioStreamData(mStream, mPcm.data(), (int)(mPcm.size() * sizeof(float)));
	SDL_ResumeAudioDevice(AudioManager::getDevice());
	mPlaying = true;
}

bool Sound::isPlaying() const
{
	if(!mStream)
		return false;
	if(SDL_GetAudioStreamQueued(mStream) > 0)
		return true;
	// finished
	const_cast<Sound*>(this)->stop();
	return false;
}

void Sound::stop()
{
	if(mStream)
	{
		SDL_UnbindAudioStream(mStream);
		SDL_DestroyAudioStream(mStream);
		mStream = nullptr;
	}
	mPlaying = false;
}

const Uint8* Sound::getData() const
{
	if(mPcm.empty())
		return nullptr;
	return reinterpret_cast<const Uint8*>(mPcm.data());
}

Uint32 Sound::getPosition() const
{
	return 0;
}

void Sound::setPosition(Uint32 /*newPosition*/)
{
}

Uint32 Sound::getLength() const
{
	return (Uint32)(mPcm.size() * sizeof(float));
}

Uint32 Sound::getLengthMS() const
{
	if(mPcm.empty()) return 0;
	return (Uint32)((mPcm.size() / 2) * 1000 / 48000);
}
