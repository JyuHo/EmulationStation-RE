#include "video/VideoPlayer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>

namespace {
std::string fixture(const std::string& name)
{
	return (std::filesystem::path(ES_TEST_DATA_DIR) / "video" / name).string();
}

// Manual clock for silent videos.
struct FakeClock
{
	std::atomic<double> now = 0;
	std::function<double()> function()
	{
		return [this] {
			return now.load();
		};
	}
};

// Collects the sound; "plays" it only when the test says so.
class FakeSink final : public VideoAudioSink
{
public:
	void put(std::span<const float> samples) override
	{
		std::scoped_lock lock(mMutex);
		mQueued += samples.size() / 2;
		mTotal += samples.size() / 2;
	}
	double queuedSeconds() const override
	{
		std::scoped_lock lock(mMutex);
		return static_cast<double>(mQueued) / 48000.0;
	}
	void clear() override
	{
		std::scoped_lock lock(mMutex);
		mQueued = 0;
	}
	void setPaused(bool paused) override { mPaused = paused; }

	// the speakers play `seconds` of what is queued
	void play(double seconds)
	{
		std::scoped_lock lock(mMutex);
		const auto frames = static_cast<std::size_t>(seconds * 48000.0);
		mQueued -= std::min(mQueued, frames);
	}
	std::size_t total() const
	{
		std::scoped_lock lock(mMutex);
		return mTotal;
	}
	bool paused() const { return mPaused; }

private:
	mutable std::mutex mMutex;
	std::size_t mQueued = 0;
	std::size_t mTotal = 0;
	std::atomic<bool> mPaused = false;
};

VideoPlayer::Options withClock(FakeClock& clock)
{
	VideoPlayer::Options options;
	options.clock = clock.function();
	return options;
}

VideoPlayer::Options withSink(FakeSink& sink)
{
	VideoPlayer::Options options;
	options.audio = &sink;
	return options;
}

// Polls currentFrame() until it returns a frame (the decoding threads need a moment).
const VideoPlayer::Frame* waitFrame(VideoPlayer& player)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while(std::chrono::steady_clock::now() < deadline)
	{
		if(const VideoPlayer::Frame* frame = player.currentFrame())
			return frame;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	return nullptr;
}
} // namespace

// tests/data/video: clip.mp4 (64x48, 10 i/s, 2 s, H.264 + AAC 440 Hz), silent.mp4 (1 s,
// no sound); see tests/data/video/README.md
TEST_CASE("VideoPlayer shows the frames at their time and drops the late ones", "[VideoPlayer]")
{
	FakeClock clock;
	VideoPlayer player(withClock(clock));
	REQUIRE(player.open(fixture("silent.mp4")));
	CHECK(player.width() == 64);
	CHECK(player.height() == 48);
	CHECK_FALSE(player.hasAudio());

	// the first frame shows at once
	const VideoPlayer::Frame* first = waitFrame(player);
	REQUIRE(first);
	CHECK(first->pts == Catch::Approx(0.0));
	CHECK(first->y.size() == 64 * 48);
	CHECK(first->u.size() == 32 * 24);
	CHECK(first->v.size() == 32 * 24);

	// nothing new before the next frame is due
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	CHECK(player.currentFrame() == nullptr);

	// 0.35 s later: frames 1 and 2 are late, frame 3 (0.3 s) shows
	clock.now = 0.35;
	const VideoPlayer::Frame* third = waitFrame(player);
	REQUIRE(third);
	CHECK(third->pts == Catch::Approx(0.3));

	// past the end: the last frame, then finished
	clock.now = 5.0;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	double lastPts = 0;
	while(!player.finished() && std::chrono::steady_clock::now() < deadline)
	{
		if(const VideoPlayer::Frame* frame = player.currentFrame())
			lastPts = frame->pts;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	CHECK(player.finished());
	CHECK(lastPts == Catch::Approx(0.9));
}

TEST_CASE("VideoPlayer pauses its clock", "[VideoPlayer]")
{
	FakeClock clock;
	VideoPlayer player(withClock(clock));
	REQUIRE(player.open(fixture("silent.mp4")));
	REQUIRE(waitFrame(player));

	clock.now = 0.2;
	player.pause();
	CHECK(player.isPaused());
	clock.now = 3.0;
	CHECK(player.position() == Catch::Approx(0.2));
	player.resume();
	CHECK(player.position() == Catch::Approx(0.2));
	clock.now = 3.1;
	CHECK(player.position() == Catch::Approx(0.3));
}

TEST_CASE("VideoPlayer follows the audio clock", "[VideoPlayer]")
{
	FakeSink sink;
	VideoPlayer player(withSink(sink));
	REQUIRE(player.open(fixture("clip.mp4")));
	REQUIRE(player.hasAudio());
	REQUIRE(waitFrame(player));

	// the audio thread fills the sink a little ahead, then waits for the speakers
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while(sink.queuedSeconds() < 0.2 && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	CHECK(sink.queuedSeconds() < 0.5);
	CHECK(player.position() < 0.05); // nothing played yet

	// the speakers play 0.5 s: the clock and the picture follow
	for(int i = 0; i < 50; i++)
	{
		sink.play(0.01);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	CHECK(player.position() == Catch::Approx(0.5).margin(0.05));
	const VideoPlayer::Frame* frame = waitFrame(player);
	REQUIRE(frame);
	CHECK(frame->pts <= player.position());
	CHECK(frame->pts >= 0.4);

	player.pause();
	CHECK(sink.paused());
	player.resume();
	CHECK_FALSE(sink.paused());
}

TEST_CASE("VideoPlayer closes at once, whatever its threads are doing", "[VideoPlayer]")
{
	FakeSink sink;
	// quick navigation through a game list: open, show a frame or not, go
	for(int i = 0; i < 20; i++)
	{
		VideoPlayer player(withSink(sink));
		REQUIRE(player.open(fixture(i % 2 ? "clip.mp4" : "silent.mp4")));
		if(i % 3 == 0)
			(void)waitFrame(player);
	}
	VideoPlayer player(withSink(sink));
	REQUIRE(player.open(fixture("clip.mp4")));
	player.close();
	CHECK_FALSE(player.isOpen());
	CHECK(player.currentFrame() == nullptr);
}

TEST_CASE("VideoPlayer reports files it cannot play", "[VideoPlayer]")
{
	VideoPlayer player;
	CHECK_FALSE(player.open(fixture("missing.mp4")));
	CHECK_FALSE(player.open((std::filesystem::path(ES_TEST_DATA_DIR) / "capture/gamelists/nes/gamelist.xml").string()));
	CHECK_FALSE(player.isOpen());
}
