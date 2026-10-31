// Characterization of the sound layer before the SDL3 migration (phase 5): WAV files are
// converted to 44.1 kHz 16-bit stereo and played through the software mixer.
// Runs on SDL's dummy audio driver: no sound card is needed.
#include "AudioManager.h"
#include "Sound.h"
#include "utils/Paths.h"

#include <catch2/catch_test_macros.hpp>
#include <SDL3/SDL.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

// 0.2 s of 440 Hz, 22050 Hz mono 8-bit (also a theme sound of the capture fixture)
const std::string kBeep = ES_TEST_DATA_DIR "/capture/themes/capture/sounds/beep.wav";
constexpr unsigned int kBeepFrames = 4410;

struct AudioFixture
{
	AudioFixture()
	{
		setenv("SDL_AUDIODRIVER", "dummy", 1);
		setenv("SDL_AUDIO_DRIVER", "dummy", 1);
		// Settings (read by Sound::play) must not touch the real configuration
		home = fs::temp_directory_path() / ("es-test-audio-" + std::to_string(::getpid()));
		fs::remove_all(home);
		es::paths::setHome(home);
	}
	~AudioFixture()
	{
		AudioManager::getInstance()->deinit();
		es::paths::clearHome();
		fs::remove_all(home);
	}
	fs::path home;
};

bool waitUntilStopped(const Sound& sound, std::chrono::milliseconds timeout)
{
	const auto end = std::chrono::steady_clock::now() + timeout;
	while(sound.isPlaying())
	{
		if(std::chrono::steady_clock::now() > end)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return true;
}

} // namespace

TEST_CASE("a WAV file is decoded to 48 kHz float stereo", "[Audio]")
{
	AudioFixture fixture;
	std::shared_ptr<Sound> sound = Sound::get(kBeep);
	REQUIRE(sound->getData() != nullptr);
	// 0.2 s resampled to 48 kHz, 2 channels, 4 bytes per sample (float32)
	CHECK(sound->getLength() == 76528);
	CHECK(sound->getPosition() == 0);
	CHECK(!sound->isPlaying());
	CHECK(Sound::get(kBeep) == sound); // sounds are shared by path
}

TEST_CASE("a missing or empty sound path gives a silent sound", "[Audio]")
{
	AudioFixture fixture;
	std::shared_ptr<Sound> none = Sound::get("");
	CHECK(none->getData() == nullptr);
	none->play();
	CHECK(!none->isPlaying());
}

TEST_CASE("a played sound runs to its end, and can be stopped", "[Audio]")
{
	AudioFixture fixture;
	std::shared_ptr<Sound> sound = Sound::get(kBeep);
	REQUIRE(sound->getData() != nullptr);

	sound->play();
	CHECK(sound->isPlaying());
	CHECK(waitUntilStopped(*sound, std::chrono::seconds(3)));
	CHECK(sound->getPosition() == 0);

	sound->play();
	sound->stop();
	CHECK(!sound->isPlaying());
	CHECK(sound->getPosition() == 0);
}
