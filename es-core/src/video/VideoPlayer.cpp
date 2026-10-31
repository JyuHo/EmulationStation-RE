#include "video/VideoPlayer.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include "Log.h"
#include "AudioManager.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

VideoPlayer::VideoPlayer() = default;

VideoPlayer::~VideoPlayer()
{
	close();
}

bool VideoPlayer::open(const std::string& path)
{
	close();
	if(avformat_open_input(&mFormat, path.c_str(), nullptr, nullptr) < 0)
		return false;
	if(avformat_find_stream_info(mFormat, nullptr) < 0)
	{
		close();
		return false;
	}
	for(unsigned i = 0; i < mFormat->nb_streams; i++)
	{
		if(mFormat->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && mVideoStream < 0)
			mVideoStream = (int)i;
		if(mFormat->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && mAudioStream < 0)
			mAudioStream = (int)i;
	}
	if(mVideoStream < 0)
	{
		close();
		return false;
	}
	AVCodecParameters* cp = mFormat->streams[mVideoStream]->codecpar;
	const AVCodec* codec = avcodec_find_decoder(cp->codec_id);
	if(!codec) { close(); return false; }
	mCodec = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(mCodec, cp);
	if(avcodec_open2(mCodec, codec, nullptr) < 0) { close(); return false; }
	mWidth = mCodec->width;
	mHeight = mCodec->height;
	AVRational fr = mFormat->streams[mVideoStream]->avg_frame_rate;
	if(fr.den != 0) mFrameRate = av_q2d(fr);
	if(mFrameRate <= 0) mFrameRate = 30.0;
	mFrame.assign((size_t)mWidth * mHeight * 4, 0);

	// Audio
	if(mAudioStream >= 0)
	{
		AVCodecParameters* acp = mFormat->streams[mAudioStream]->codecpar;
		const AVCodec* acodec = avcodec_find_decoder(acp->codec_id);
		if(acodec)
		{
			mAudioCodec = avcodec_alloc_context3(acodec);
			avcodec_parameters_to_context(mAudioCodec, acp);
			if(avcodec_open2(mAudioCodec, acodec, nullptr) >= 0)
			{
				AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
				swr_alloc_set_opts2(&mSwr, &outLayout, AV_SAMPLE_FMT_FLT, 48000,
					&mAudioCodec->ch_layout, mAudioCodec->sample_fmt, mAudioCodec->sample_rate, 0, nullptr);
				if(mSwr) swr_init(mSwr);
				SDL_AudioSpec src{SDL_AUDIO_F32, 2, 48000};
				mAudioStreamSDL = SDL_CreateAudioStream(&src, &AudioManager::getFormat());
				if(mAudioStreamSDL)
					SDL_BindAudioStream(AudioManager::getDevice(), mAudioStreamSDL);
			}
		}
	}
	return true;
}

void VideoPlayer::close()
{
	if(mAudioStreamSDL) { SDL_UnbindAudioStream(mAudioStreamSDL); SDL_DestroyAudioStream(mAudioStreamSDL); mAudioStreamSDL = nullptr; }
	if(mSwr) { swr_free(&mSwr); mSwr = nullptr; }
	if(mAudioCodec) { avcodec_free_context(&mAudioCodec); mAudioCodec = nullptr; }
	if(mCodec) { avcodec_free_context(&mCodec); mCodec = nullptr; }
	if(mFormat) { avformat_close_input(&mFormat); mFormat = nullptr; }
	mVideoStream = -1;
	mAudioStream = -1;
	mFrame.clear();
	mAccumulator = 0;
}

bool VideoPlayer::update(int deltaTime)
{
	if(!mFormat || !mCodec) return false;
	mAccumulator += deltaTime;
	double frameTime = 1000.0 / mFrameRate;
	bool gotFrame = false;
	while(mAccumulator >= frameTime)
	{
		mAccumulator -= frameTime;
		bool frameDecoded = false;
		AVPacket* pkt = av_packet_alloc();
		while(av_read_frame(mFormat, pkt) >= 0)
		{
			if(pkt->stream_index == mAudioStream && mAudioCodec)
			{
				if(avcodec_send_packet(mAudioCodec, pkt) >= 0)
				{
					AVFrame* aframe = av_frame_alloc();
					while(avcodec_receive_frame(mAudioCodec, aframe) >= 0)
					{
						if(mSwr && mAudioStreamSDL)
						{
							int out = swr_get_out_samples(mSwr, aframe->nb_samples);
							if(out > 0)
							{
								std::vector<float> buf(out * 2);
								uint8_t* p = reinterpret_cast<uint8_t*>(buf.data());
								int c = swr_convert(mSwr, &p, out, (const uint8_t**)aframe->data, aframe->nb_samples);
								if(c > 0 && mAudioStreamSDL)
									SDL_PutAudioStreamData(mAudioStreamSDL, buf.data(), c * 2 * (int)sizeof(float));
							}
						}
					}
					av_frame_free(&aframe);
				}
				av_packet_unref(pkt);
				continue;
			}
			if(pkt->stream_index == mVideoStream)
			{
				if(avcodec_send_packet(mCodec, pkt) >= 0)
				{
					AVFrame* frame = av_frame_alloc();
					if(avcodec_receive_frame(mCodec, frame) >= 0)
					{
						SwsContext* sws = sws_getContext(mCodec->width, mCodec->height, mCodec->pix_fmt,
							mWidth, mHeight, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
						uint8_t* dst[1] = { mFrame.data() };
						int dstStride[1] = { mWidth * 4 };
						sws_scale(sws, frame->data, frame->linesize, 0, mCodec->height, dst, dstStride);
						sws_freeContext(sws);
						av_frame_free(&frame);
						frameDecoded = true;
					}
				}
				av_packet_unref(pkt);
				if(frameDecoded) break;
			}
			av_packet_unref(pkt);
		}
		av_packet_free(&pkt);
		if(frameDecoded) gotFrame = true;
		else break;
	}
	if(mAudioStreamSDL)
		SDL_ResumeAudioDevice(AudioManager::getDevice());
	return gotFrame;
}
