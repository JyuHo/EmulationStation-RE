#include "video/VideoPlayer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <cstring>
#include <format>

namespace {
constexpr int kSampleRate = 48000;
// the audio thread keeps this much sound ahead in the sink: enough against underruns,
// small enough for the clock to follow pause and close quickly
constexpr double kAudioAhead = 0.25;

std::string avError(int code)
{
	char text[AV_ERROR_MAX_STRING_SIZE] = {};
	av_strerror(code, text, sizeof(text));
	return text;
}

struct PacketDeleter
{
	void operator()(AVPacket* packet) const { av_packet_free(&packet); }
};
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

struct FrameDeleter
{
	void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
} // namespace

// Bounded queue between the demux thread and a decoding thread.
struct VideoPlayer::PacketQueue
{
	std::mutex mutex;
	std::condition_variable_any changed;
	std::deque<PacketPtr> packets;
	bool end = false; // no packet will come any more

	// false when stopped
	bool push(std::stop_token stop, PacketPtr packet)
	{
		std::unique_lock lock(mutex);
		if(!changed.wait(lock, stop, [&] { return packets.size() < kMaxPackets; }))
			return false;
		packets.push_back(std::move(packet));
		changed.notify_all();
		return true;
	}

	void finish()
	{
		std::scoped_lock lock(mutex);
		end = true;
		changed.notify_all();
	}

	// nullptr at the end of the stream or when stopped
	PacketPtr pop(std::stop_token stop)
	{
		std::unique_lock lock(mutex);
		if(!changed.wait(lock, stop, [&] { return !packets.empty() || end; }) || packets.empty())
			return nullptr;
		PacketPtr packet = std::move(packets.front());
		packets.pop_front();
		changed.notify_all();
		return packet;
	}
};

struct VideoPlayer::Decoder
{
	AVCodecContext* context = nullptr;
	int stream = -1;
	AVRational timeBase{0, 1};

	~Decoder() { avcodec_free_context(&context); }

	static std::expected<std::unique_ptr<Decoder>, std::string> open(AVFormatContext* format, int stream)
	{
		auto decoder = std::make_unique<Decoder>();
		const AVCodecParameters* parameters = format->streams[stream]->codecpar;
		const AVCodec* codec = avcodec_find_decoder(parameters->codec_id);
		if(!codec)
			return std::unexpected(std::format("no decoder for {}", avcodec_get_name(parameters->codec_id)));
		decoder->context = avcodec_alloc_context3(codec);
		if(!decoder->context)
			return std::unexpected(std::string("out of memory"));
		if(int error = avcodec_parameters_to_context(decoder->context, parameters); error < 0)
			return std::unexpected(avError(error));
		decoder->context->thread_count = 0; // FFmpeg picks the number of threads
		if(int error = avcodec_open2(decoder->context, codec, nullptr); error < 0)
			return std::unexpected(avError(error));
		decoder->stream = stream;
		decoder->timeBase = format->streams[stream]->time_base;
		return decoder;
	}

	double seconds(std::int64_t timestamp) const
	{
		return timestamp == AV_NOPTS_VALUE ? 0.0 : static_cast<double>(timestamp) * av_q2d(timeBase);
	}
};

VideoPlayer::VideoPlayer() = default;

VideoPlayer::VideoPlayer(Options options) : mOptions(std::move(options)) {}

VideoPlayer::~VideoPlayer()
{
	close();
}

std::expected<void, std::string> VideoPlayer::open(const std::string& path)
{
	close();

	if(int error = avformat_open_input(&mFormat, path.c_str(), nullptr, nullptr); error < 0)
		return std::unexpected(avError(error));
	if(int error = avformat_find_stream_info(mFormat, nullptr); error < 0)
	{
		close();
		return std::unexpected(avError(error));
	}

	const int videoStream = av_find_best_stream(mFormat, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if(videoStream < 0)
	{
		close();
		return std::unexpected(std::string("no video stream"));
	}
	auto video = Decoder::open(mFormat, videoStream);
	if(!video)
	{
		close();
		return std::unexpected(video.error());
	}
	mVideo = std::move(*video);
	mWidth = mVideo->context->width;
	mHeight = mVideo->context->height;

	if(mOptions.audio)
	{
		const int audioStream = av_find_best_stream(mFormat, AVMEDIA_TYPE_AUDIO, -1, videoStream, nullptr, 0);
		if(audioStream >= 0)
			if(auto audio = Decoder::open(mFormat, audioStream))
				mAudio = std::move(*audio);
		// a video whose sound cannot be decoded still plays, silent
	}
	mHasAudio = mAudio != nullptr;

	mVideoPackets = std::make_unique<PacketQueue>();
	mAudioPackets = std::make_unique<PacketQueue>();
	mFrames.clear();
	mVideoDone = false;
	mShownAny = false;
	mAudioEnd = 0;
	mAudioDone = !mHasAudio;
	mPaused = false;
	mStart = std::chrono::steady_clock::now();
	if(mOptions.clock)
		mClockStart = mOptions.clock();
	if(mHasAudio)
	{
		mOptions.audio->clear();
		mOptions.audio->setPaused(false);
	}
	mOpen = true;

	mThreads.emplace_back([this](std::stop_token stop) { demux(stop); });
	mThreads.emplace_back([this](std::stop_token stop) { decodeVideo(stop); });
	if(mHasAudio)
		mThreads.emplace_back([this](std::stop_token stop) { decodeAudio(stop); });
	return {};
}

void VideoPlayer::close()
{
	for(std::jthread& thread : mThreads)
		thread.request_stop();
	mThreads.clear(); // joins
	if(mHasAudio && mOptions.audio)
		mOptions.audio->clear();
	mVideo.reset();
	mAudio.reset();
	mVideoPackets.reset();
	mAudioPackets.reset();
	avformat_close_input(&mFormat);
	mFrames.clear();
	mOpen = false;
	mHasAudio = false;
}

void VideoPlayer::pause()
{
	if(!mOpen || mPaused)
		return;
	mPaused = true;
	mPausedAt = std::chrono::steady_clock::now();
	mClockPausedAt = mOptions.clock ? mOptions.clock() : 0;
	if(mHasAudio)
		mOptions.audio->setPaused(true);
}

void VideoPlayer::resume()
{
	if(!mOpen || !mPaused)
		return;
	mPaused = false;
	mStart += std::chrono::steady_clock::now() - mPausedAt;
	if(mOptions.clock)
		mClockStart += mOptions.clock() - mClockPausedAt;
	if(mHasAudio)
		mOptions.audio->setPaused(false);
}

double VideoPlayer::steadySeconds() const
{
	if(mOptions.clock)
		return (mPaused ? mClockPausedAt : mOptions.clock()) - mClockStart;
	const auto now = mPaused ? mPausedAt : std::chrono::steady_clock::now();
	return std::chrono::duration<double>(now - mStart).count();
}

double VideoPlayer::position() const
{
	if(!mHasAudio)
		return steadySeconds();
	std::scoped_lock lock(mAudioMutex);
	const double queued = mOptions.audio->queuedSeconds();
	if(mAudioDone && queued <= 0)
		return std::max(mAudioEnd, steadySeconds()); // sound track shorter than the picture
	return std::max(0.0, mAudioEnd - queued);
}

const VideoPlayer::Frame* VideoPlayer::currentFrame()
{
	if(!mOpen)
		return nullptr;
	const double now = position();

	std::optional<Frame> due;
	{
		std::scoped_lock lock(mFramesMutex);
		// the first frame shows at once; later ones at their time, the late ones dropped
		while(!mFrames.empty() && (!mShownAny || mFrames.front().pts <= now))
		{
			due = std::move(mFrames.front());
			mFrames.pop_front();
			mShownAny = true;
		}
	}
	if(!due)
		return nullptr;
	mFramesChanged.notify_all();
	mShown = std::move(*due);
	return &mShown;
}

bool VideoPlayer::finished() const
{
	std::scoped_lock lock(mFramesMutex);
	return mOpen && mVideoDone && mFrames.empty();
}

void VideoPlayer::demux(std::stop_token stop)
{
	PacketPtr packet(av_packet_alloc());
	while(!stop.stop_requested() && packet && av_read_frame(mFormat, packet.get()) >= 0)
	{
		PacketQueue* queue = packet->stream_index == mVideo->stream ? mVideoPackets.get()
			: (mAudio && packet->stream_index == mAudio->stream)	? mAudioPackets.get()
																	: nullptr;
		if(!queue)
		{
			av_packet_unref(packet.get());
			continue;
		}
		PacketPtr next(av_packet_alloc());
		if(!queue->push(stop, std::move(packet)))
			return;
		packet = std::move(next);
	}
	mVideoPackets->finish();
	mAudioPackets->finish();
}

void VideoPlayer::decodeVideo(std::stop_token stop)
{
	AVCodecContext* context = mVideo->context;
	FramePtr frame(av_frame_alloc());
	SwsContext* scaler = nullptr;
	bool draining = false;

	const auto deliver = [&]() -> bool {
		Frame out;
		out.width = frame->width;
		out.height = frame->height;
		out.pts = mVideo->seconds(frame->best_effort_timestamp);
		const int chromaWidth = (out.width + 1) / 2;
		const int chromaHeight = (out.height + 1) / 2;
		out.y.resize(static_cast<std::size_t>(out.width) * out.height);
		out.u.resize(static_cast<std::size_t>(chromaWidth) * chromaHeight);
		out.v.resize(out.u.size());
		std::uint8_t* planes[3] = {out.y.data(), out.u.data(), out.v.data()};
		const int strides[3] = {out.width, chromaWidth, chromaWidth};

		if(frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUVJ420P)
		{
			const int heights[3] = {out.height, chromaHeight, chromaHeight};
			for(int plane = 0; plane < 3; plane++)
				for(int row = 0; row < heights[plane]; row++)
					std::memcpy(planes[plane] + static_cast<std::size_t>(row) * strides[plane],
						frame->data[plane] + static_cast<std::ptrdiff_t>(row) * frame->linesize[plane],
						static_cast<std::size_t>(strides[plane]));
		}
		else
		{
			scaler =
				sws_getCachedContext(scaler, frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
					out.width, out.height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
			if(!scaler)
				return true; // unsupported pixel format: skip the frame
			sws_scale(scaler, frame->data, frame->linesize, 0, frame->height, planes, strides);
		}

		std::unique_lock lock(mFramesMutex);
		if(!mFramesChanged.wait(lock, stop, [&] { return mFrames.size() < kMaxFrames; }))
			return false;
		mFrames.push_back(std::move(out));
		return true;
	};

	while(!stop.stop_requested())
	{
		int result = avcodec_receive_frame(context, frame.get());
		if(result == 0)
		{
			const bool keepGoing = deliver();
			av_frame_unref(frame.get());
			if(!keepGoing)
				break;
			continue;
		}
		if(result == AVERROR_EOF)
			break;
		// AVERROR(EAGAIN): the decoder needs the next packet
		if(draining)
			break;
		PacketPtr packet = mVideoPackets->pop(stop);
		if(stop.stop_requested())
			break;
		if(!packet)
			draining = true;
		avcodec_send_packet(context, packet.get()); // nullptr starts draining
	}
	sws_freeContext(scaler);

	std::scoped_lock lock(mFramesMutex);
	mVideoDone = !stop.stop_requested();
}

void VideoPlayer::decodeAudio(std::stop_token stop)
{
	AVCodecContext* context = mAudio->context;
	FramePtr frame(av_frame_alloc());
	SwrContext* resampler = nullptr;
	AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
	if(swr_alloc_set_opts2(&resampler, &stereo, AV_SAMPLE_FMT_FLT, kSampleRate, &context->ch_layout,
		   context->sample_fmt, context->sample_rate, 0, nullptr) < 0 ||
		swr_init(resampler) < 0)
	{
		swr_free(&resampler);
		std::scoped_lock lock(mAudioMutex);
		mAudioDone = true;
		return;
	}

	std::vector<float> samples;
	bool started = false;
	bool draining = false;
	while(!stop.stop_requested())
	{
		int result = avcodec_receive_frame(context, frame.get());
		if(result == 0)
		{
			// stay at most kAudioAhead in front of the speakers
			while(!stop.stop_requested() && mOptions.audio->queuedSeconds() > kAudioAhead)
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			if(stop.stop_requested())
				break;

			const int capacity = swr_get_out_samples(resampler, frame->nb_samples);
			samples.resize(static_cast<std::size_t>(std::max(capacity, 0)) * 2);
			auto* out = reinterpret_cast<std::uint8_t*>(samples.data());
			const int converted = swr_convert(
				resampler, &out, capacity, const_cast<const std::uint8_t**>(frame->extended_data), frame->nb_samples);
			if(converted > 0)
			{
				std::scoped_lock lock(mAudioMutex);
				if(!started)
				{
					mAudioEnd = mAudio->seconds(frame->best_effort_timestamp);
					started = true;
				}
				mOptions.audio->put(std::span(samples.data(), static_cast<std::size_t>(converted) * 2));
				mAudioEnd += static_cast<double>(converted) / kSampleRate;
			}
			av_frame_unref(frame.get());
			continue;
		}
		if(result == AVERROR_EOF || draining)
			break;
		PacketPtr packet = mAudioPackets->pop(stop);
		if(stop.stop_requested())
			break;
		if(!packet)
			draining = true;
		avcodec_send_packet(context, packet.get());
	}
	swr_free(&resampler);

	std::scoped_lock lock(mAudioMutex);
	mAudioDone = true;
}
