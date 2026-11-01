// VideoComponent: the delay before playing, the default video, stopping when hidden.
// No renderer here: frames are never uploaded, only the playback state is checked.
#include "components/VideoComponent.h"
#include "AudioManager.h"
#include "Settings.h"
#include "Window.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

namespace {

const std::string kClip = ES_TEST_DATA_DIR "/video/clip.mp4";
const std::string kSilent = ES_TEST_DATA_DIR "/video/silent.mp4";
const std::string kMissing = ES_TEST_DATA_DIR "/video/missing.mp4";

// the sound would need an audio device
struct NoVideoSound
{
	NoVideoSound() { Settings::getInstance()->setBool("VideoAudio", false); }
	~NoVideoSound() { Settings::getInstance()->setBool("VideoAudio", true); }
};

} // namespace

TEST_CASE("a video starts once the delay has passed", "[VideoComponent]")
{
	NoVideoSound noSound;
	Window window;
	VideoComponent video(&window);
	video.setDelay(100);
	video.setVideo(kClip);

	CHECK(video.isWaiting());
	video.update(60);
	CHECK_FALSE(video.isPlaying());
	video.update(60);
	CHECK(video.isPlaying());
	CHECK_FALSE(video.isWaiting());

	// the same path keeps playing; another one waits again
	video.setVideo(kClip);
	CHECK(video.isPlaying());
	video.setVideo(kSilent);
	CHECK_FALSE(video.isPlaying());
	CHECK(video.isWaiting());

	video.clearVideo();
	CHECK_FALSE(video.isWaiting());
	video.update(1000);
	CHECK_FALSE(video.isPlaying());
}

TEST_CASE("a missing video plays the default one, if any", "[VideoComponent]")
{
	NoVideoSound noSound;
	Window window;
	VideoComponent video(&window);
	video.setDelay(0);

	video.setVideo(kMissing);
	CHECK_FALSE(video.isWaiting());
	video.update(16);
	CHECK_FALSE(video.isPlaying());

	video.setDefaultVideo(kSilent);
	video.setVideo("");
	video.update(16);
	CHECK(video.isPlaying());
}

TEST_CASE("a hidden video stops and starts again after the delay when shown", "[VideoComponent]")
{
	NoVideoSound noSound;
	Window window;
	GuiComponent view(&window);
	VideoComponent video(&window);
	view.addChild(&video);
	video.setDelay(50);
	video.setVideo(kClip);
	video.update(50);
	REQUIRE(video.isPlaying());

	view.onHide(); // forwarded to the children
	CHECK_FALSE(video.isPlaying());
	video.update(100); // hidden: the delay does not run
	CHECK_FALSE(video.isPlaying());

	view.onShow();
	video.update(30);
	CHECK_FALSE(video.isPlaying());
	video.update(30);
	CHECK(video.isPlaying());
	view.removeChild(&video);
}

TEST_CASE("a video fits its maximum size, keeping its aspect ratio", "[VideoComponent]")
{
	NoVideoSound noSound;
	Window window;
	VideoComponent video(&window);
	video.setMaxSize(320, 320);
	video.setOrigin(0.5f, 0.5f);
	video.setPosition(400, 300);
	CHECK(video.getSize() == glm::vec2(320, 320)); // nothing known yet: the box

	video.setDelay(0);
	video.setVideo(kClip); // 64 x 48
	video.update(16);
	REQUIRE(video.isPlaying());
	CHECK(video.getSize() == glm::vec2(320, 240));
	CHECK(video.getCenter() == glm::vec2(400, 300));

	video.setResize(128, 0); // a 0 keeps the aspect ratio
	CHECK(video.getSize() == glm::vec2(128, 96));
}

TEST_CASE("the music is paused while a video with sound plays", "[VideoComponent]")
{
	// SDL's dummy driver: no sound card needed
	setenv("SDL_AUDIODRIVER", "dummy", 1);
	setenv("SDL_AUDIO_DRIVER", "dummy", 1);
	REQUIRE(Settings::getInstance()->getBool("VideoAudio"));
	AudioManager::getInstance()->init(); // opens the device (closed by other tests)
	{
		Window window;
		VideoComponent video(&window);
		VideoComponent silent(&window);
		video.setDelay(0);
		silent.setDelay(0);

		silent.setVideo(kSilent);
		silent.update(16);
		REQUIRE(silent.isPlaying());
		CHECK(VideoComponent::audibleCount() == 0); // no sound track

		video.setVideo(kClip);
		video.update(16);
		REQUIRE(video.isPlaying());
		CHECK(VideoComponent::audibleCount() == 1);

		video.onHide();
		CHECK(VideoComponent::audibleCount() == 0);
		video.onShow();
		video.update(16);
		CHECK(VideoComponent::audibleCount() == 1);
	}
	CHECK(VideoComponent::audibleCount() == 0); // destroyed while playing
	AudioManager::getInstance()->deinit();
}
