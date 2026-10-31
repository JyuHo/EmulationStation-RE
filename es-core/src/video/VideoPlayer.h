#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

struct AVFormatContext;
struct AVPacket;

// Where a VideoPlayer sends its sound: interleaved stereo float at 48 kHz. Called from the
// audio decoding thread.
class VideoAudioSink
{
public:
	virtual ~VideoAudioSink() = default;
	virtual void put(std::span<const float> samples) = 0;
	// seconds of sound queued and not played yet
	[[nodiscard]] virtual double queuedSeconds() const = 0;
	virtual void clear() = 0;
	virtual void setPaused(bool paused) = 0;
};

// Plays a video file with FFmpeg (plan, phase 9):
//   demux thread         av_read_frame -> bounded packet queues (video, audio)
//   video thread         decoding -> up to kMaxFrames YUV 4:2:0 frames
//   audio thread         decoding -> swr_convert (float stereo 48 kHz) -> VideoAudioSink
// The main thread calls currentFrame() every frame: it returns the newest frame due at the
// clock and drops the late ones. The clock is the audio (what the sink has played) when the
// video has sound, the steady clock otherwise. Every thread is a std::jthread stopped
// through its std::stop_token by close() or the destructor, so leaving a game list while a
// video plays never blocks nor leaks.
class VideoPlayer
{
public:
	struct Frame
	{
		int width = 0;
		int height = 0;
		// planes of a YUV 4:2:0 picture, rows top-down and tightly packed: y is width x
		// height, u and v are (width + 1) / 2 x (height + 1) / 2
		std::vector<std::uint8_t> y, u, v;
		double pts = 0; // seconds
	};

	struct Options
	{
		VideoAudioSink* audio = nullptr; // nullptr: the sound track is ignored
		// clock of a silent video, in seconds (tests); default: steady clock
		std::function<double()> clock;
	};

	VideoPlayer();
	explicit VideoPlayer(Options options);
	~VideoPlayer();

	VideoPlayer(const VideoPlayer&) = delete;
	VideoPlayer& operator=(const VideoPlayer&) = delete;

	std::expected<void, std::string> open(const std::string& path);
	void close();
	[[nodiscard]] bool isOpen() const { return mOpen; }

	void pause();
	void resume();
	[[nodiscard]] bool isPaused() const { return mPaused; }

	// Main thread. The frame to show now, or nullptr when the one returned last is still
	// current. Valid until the next call.
	const Frame* currentFrame();
	// Every frame was shown (the component restarts the video to loop it).
	[[nodiscard]] bool finished() const;
	// Position of the clock, in seconds since the start.
	[[nodiscard]] double position() const;
	[[nodiscard]] int width() const { return mWidth; }
	[[nodiscard]] int height() const { return mHeight; }
	[[nodiscard]] bool hasAudio() const { return mHasAudio; }

	static constexpr std::size_t kMaxFrames = 4;
	static constexpr std::size_t kMaxPackets = 64;

private:
	struct PacketQueue;
	struct Decoder;

	void demux(std::stop_token stop);
	void decodeVideo(std::stop_token stop);
	void decodeAudio(std::stop_token stop);
	double steadySeconds() const;

	Options mOptions;
	bool mOpen = false;
	bool mHasAudio = false;
	bool mPaused = false;
	int mWidth = 0;
	int mHeight = 0;

	AVFormatContext* mFormat = nullptr;
	std::unique_ptr<Decoder> mVideo;
	std::unique_ptr<Decoder> mAudio;
	std::unique_ptr<PacketQueue> mVideoPackets;
	std::unique_ptr<PacketQueue> mAudioPackets;

	// decoded frames, shared with the video thread
	mutable std::mutex mFramesMutex;
	std::condition_variable_any mFramesChanged;
	std::deque<Frame> mFrames;
	bool mVideoDone = false; // the video thread decoded the last frame
	Frame mShown;
	bool mShownAny = false;

	// audio clock: sound handed to the sink so far, in seconds of the video's time line
	mutable std::mutex mAudioMutex;
	double mAudioEnd = 0;
	bool mAudioDone = false;

	// clock of a silent video
	std::chrono::steady_clock::time_point mStart;
	std::chrono::steady_clock::time_point mPausedAt;
	double mClockStart = 0;	   // Options::clock at open()
	double mClockPausedAt = 0; // Options::clock at pause()

	std::vector<std::jthread> mThreads; // last: stopped before the rest is destroyed
};
