#include "video/VideoPlayer.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include "Log.h"

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
		if(mFormat->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			mVideoStream = (int)i;
			break;
		}
	}
	if(mVideoStream < 0)
	{
		close();
		return false;
	}
	AVCodecParameters* cp = mFormat->streams[mVideoStream]->codecpar;
	const AVCodec* codec = avcodec_find_decoder(cp->codec_id);
	if(!codec)
	{
		close();
		return false;
	}
	mCodec = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(mCodec, cp);
	if(avcodec_open2(mCodec, codec, nullptr) < 0)
	{
		close();
		return false;
	}
	mWidth = mCodec->width;
	mHeight = mCodec->height;
	AVRational fr = mFormat->streams[mVideoStream]->avg_frame_rate;
	if(fr.den != 0) mFrameRate = av_q2d(fr);
	if(mFrameRate <= 0) mFrameRate = 30.0;
	mFrame.assign((size_t)mWidth * mHeight * 4, 0);
	return true;
}

void VideoPlayer::close()
{
	if(mCodec) { avcodec_free_context(&mCodec); mCodec = nullptr; }
	if(mFormat) { avformat_close_input(&mFormat); mFormat = nullptr; }
	mVideoStream = -1;
	mFrame.clear();
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
		AVPacket* pkt = av_packet_alloc();
		bool frameDecoded = false;
		while(av_read_frame(mFormat, pkt) >= 0)
		{
			if(pkt->stream_index == mVideoStream)
			{
				if(avcodec_send_packet(mCodec, pkt) >= 0)
				{
					AVFrame* frame = av_frame_alloc();
					if(avcodec_receive_frame(mCodec, frame) >= 0)
					{
						// convert to RGBA
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
	return gotFrame;
}