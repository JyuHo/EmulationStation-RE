#include "audio/AudioDecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#include <algorithm>

struct AudioDecoder::State
{
	AVFormatContext* format = nullptr;
	AVCodecContext* codec = nullptr;
	SwrContext* resampler = nullptr;
	AVPacket* packet = nullptr;
	AVFrame* frame = nullptr;
	int stream = -1;
	bool demuxDone = false;	  // av_read_frame reached the end, the decoder was flushed
	bool decoderDone = false; // the decoder returned its last frame
	bool resamplerDone = false;
	std::vector<float> pending; // decoded, not read yet
	std::size_t pendingStart = 0;

	~State()
	{
		av_frame_free(&frame);
		av_packet_free(&packet);
		swr_free(&resampler);
		avcodec_free_context(&codec);
		avformat_close_input(&format);
	}
};

namespace {

std::string avError(int code)
{
	char text[AV_ERROR_MAX_STRING_SIZE] = {};
	av_strerror(code, text, sizeof(text));
	return text;
}

} // namespace

AudioDecoder::AudioDecoder() = default;
AudioDecoder::~AudioDecoder() = default;

std::expected<void, std::string> AudioDecoder::open(const std::string& path)
{
	auto state = std::make_unique<State>();
	if(int error = avformat_open_input(&state->format, path.c_str(), nullptr, nullptr); error < 0)
		return std::unexpected(avError(error));
	if(int error = avformat_find_stream_info(state->format, nullptr); error < 0)
		return std::unexpected(avError(error));

	const AVCodec* codec = nullptr;
	state->stream = av_find_best_stream(state->format, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
	if(state->stream < 0 || !codec)
		return std::unexpected("no sound track");

	state->codec = avcodec_alloc_context3(codec);
	if(!state->codec)
		return std::unexpected("out of memory");
	avcodec_parameters_to_context(state->codec, state->format->streams[state->stream]->codecpar);
	if(int error = avcodec_open2(state->codec, codec, nullptr); error < 0)
		return std::unexpected(avError(error));

	AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
	if(int error = swr_alloc_set_opts2(&state->resampler, &stereo, AV_SAMPLE_FMT_FLT, kRate, &state->codec->ch_layout,
		   state->codec->sample_fmt, state->codec->sample_rate, 0, nullptr);
		error < 0)
		return std::unexpected(avError(error));
	if(int error = swr_init(state->resampler); error < 0)
		return std::unexpected(avError(error));

	state->packet = av_packet_alloc();
	state->frame = av_frame_alloc();
	if(!state->packet || !state->frame)
		return std::unexpected("out of memory");

	mState = std::move(state);
	return {};
}

bool AudioDecoder::atEnd() const
{
	return !mState || (mState->resamplerDone && mState->pendingStart >= mState->pending.size());
}

// Decodes one more frame into pending; false when there is nothing left.
bool AudioDecoder::decodeMore()
{
	State& s = *mState;
	auto convert = [&](const AVFrame* frame) {
		const int room = swr_get_out_samples(s.resampler, frame ? frame->nb_samples : 0);
		if(room <= 0)
			return 0;
		const std::size_t old = s.pending.size();
		s.pending.resize(old + (std::size_t)room * kChannels);
		auto* out = reinterpret_cast<std::uint8_t*>(s.pending.data() + old);
		const int converted = swr_convert(s.resampler, &out, room,
			frame ? const_cast<const std::uint8_t**>(frame->extended_data) : nullptr, frame ? frame->nb_samples : 0);
		s.pending.resize(old + (std::size_t)std::max(converted, 0) * kChannels);
		return std::max(converted, 0);
	};

	while(!s.decoderDone)
	{
		const int received = avcodec_receive_frame(s.codec, s.frame);
		if(received == 0)
		{
			convert(s.frame);
			av_frame_unref(s.frame);
			return true;
		}
		if(received == AVERROR_EOF)
		{
			s.decoderDone = true;
			break;
		}
		if(received != AVERROR(EAGAIN))
		{
			s.decoderDone = true; // a broken file: stop here
			break;
		}

		// the decoder wants a packet
		if(s.demuxDone)
		{
			s.decoderDone = true;
			break;
		}
		const int read = av_read_frame(s.format, s.packet);
		if(read < 0)
		{
			s.demuxDone = true;
			avcodec_send_packet(s.codec, nullptr); // flush
			continue;
		}
		if(s.packet->stream_index == s.stream)
			avcodec_send_packet(s.codec, s.packet);
		av_packet_unref(s.packet);
	}

	// what the resampler still holds
	if(!s.resamplerDone)
	{
		if(convert(nullptr) > 0)
			return true;
		s.resamplerDone = true;
	}
	return false;
}

std::size_t AudioDecoder::read(std::vector<float>& out, std::size_t frames)
{
	if(!mState)
		return 0;
	State& s = *mState;
	std::size_t done = 0;
	while(done < frames)
	{
		if(s.pendingStart >= s.pending.size())
		{
			s.pending.clear();
			s.pendingStart = 0;
			if(!decodeMore())
				break;
			continue;
		}
		const std::size_t available = (s.pending.size() - s.pendingStart) / kChannels;
		const std::size_t take = std::min(available, frames - done);
		const auto first = s.pending.begin() + (std::ptrdiff_t)s.pendingStart;
		out.insert(out.end(), first, first + (std::ptrdiff_t)(take * kChannels));
		s.pendingStart += take * kChannels;
		done += take;
	}
	return done;
}

std::expected<std::vector<float>, std::string> AudioDecoder::decodeFile(const std::string& path)
{
	AudioDecoder decoder;
	if(auto opened = decoder.open(path); !opened)
		return std::unexpected(opened.error());
	std::vector<float> pcm;
	while(decoder.read(pcm, 1u << 16) > 0)
	{
	}
	return pcm;
}
