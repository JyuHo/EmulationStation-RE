// AudioDecoder (streaming FFmpeg decoding) and the background music played from it.
// Runs on SDL's dummy audio driver: no sound card is needed.
#include "AudioManager.h"
#include "MusicPlayer.h"
#include "Settings.h"
#include "audio/AudioDecoder.h"
#include "utils/Paths.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

// 0.2 s of 440 Hz, 22050 Hz mono 8-bit
const std::string kBeep = ES_TEST_DATA_DIR "/capture/themes/capture/sounds/beep.wav";
// 2 s, AAC 44.1 kHz in an MP4
const std::string kClip = ES_TEST_DATA_DIR "/video/clip.mp4";
const std::string kSilent = ES_TEST_DATA_DIR "/video/silent.mp4";

struct MusicFixture
{
	MusicFixture()
	{
		setenv("SDL_AUDIODRIVER", "dummy", 1);
		setenv("SDL_AUDIO_DRIVER", "dummy", 1);
		home = fs::temp_directory_path() / ("es-test-music-" + std::to_string(::getpid()));
		fs::remove_all(home);
		es::paths::setHome(home);
		fs::create_directories(es::paths::userDir(es::paths::Kind::Data) / "music" / "sub");
		fs::copy_file(kBeep, es::paths::userDir(es::paths::Kind::Data) / "music" / "a.wav");
		fs::copy_file(kBeep, es::paths::userDir(es::paths::Kind::Data) / "music" / "sub" / "b.WAV");
		fs::copy_file(kBeep, es::paths::userDir(es::paths::Kind::Data) / "music" / "notes.txt");
		AudioManager::getInstance()->init();
	}
	~MusicFixture()
	{
		MusicPlayer::getInstance().deinit();
		MusicPlayer::getInstance().resume();
		Settings::getInstance()->setBool("BackgroundMusic", true);
		Settings::getInstance()->setInt("MusicVolume", 50);
		AudioManager::getInstance()->deinit();
		es::paths::clearHome();
		fs::remove_all(home);
	}
	fs::path home;
};

// Calls update() like the main loop until done() or the timeout.
template <typename Done> bool runUntil(Done done, std::chrono::milliseconds timeout)
{
	const auto end = std::chrono::steady_clock::now() + timeout;
	while(!done())
	{
		if(std::chrono::steady_clock::now() > end)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		MusicPlayer::getInstance().update();
	}
	return true;
}

} // namespace

TEST_CASE("AudioDecoder converts to 48 kHz float stereo", "[Music]")
{
	auto pcm = AudioDecoder::decodeFile(kBeep);
	REQUIRE(pcm);
	const std::size_t frames = pcm->size() / 2;
	CHECK(frames >= 9500); // 0.2 s at 48 kHz: 9600
	CHECK(frames <= 9700);
	// mono duplicated on both channels
	for(std::size_t i = 0; i < pcm->size(); i += 2)
		REQUIRE((*pcm)[i] == (*pcm)[i + 1]);

	auto clip = AudioDecoder::decodeFile(kClip);
	REQUIRE(clip);
	CHECK(clip->size() / 2 > 47000 * 2);
	CHECK(clip->size() / 2 < 49000 * 2);
}

TEST_CASE("AudioDecoder streams the same samples as a whole decoding", "[Music]")
{
	auto whole = AudioDecoder::decodeFile(kClip);
	REQUIRE(whole);

	AudioDecoder decoder;
	REQUIRE(decoder.open(kClip));
	std::vector<float> streamed;
	while(decoder.read(streamed, 1000) > 0)
	{
	}
	CHECK(decoder.atEnd());
	CHECK(streamed == *whole);
	CHECK(decoder.read(streamed, 1000) == 0);
}

TEST_CASE("AudioDecoder reports files without sound", "[Music]")
{
	AudioDecoder decoder;
	CHECK_FALSE(decoder.open(kSilent));
	CHECK_FALSE(decoder.open(ES_TEST_DATA_DIR "/does-not-exist.ogg"));
	CHECK(decoder.atEnd());
	std::vector<float> out;
	CHECK(decoder.read(out, 10) == 0);
}

TEST_CASE("the music tracks are never the same twice in a row", "[Music]")
{
	std::mt19937 random(7);
	std::optional<std::size_t> previous;
	for(int i = 0; i < 200; i++)
	{
		const std::size_t index = MusicPlayer::pick(3, previous, random);
		REQUIRE(index < 3);
		REQUIRE((!previous || index != *previous));
		previous = index;
	}
	CHECK(MusicPlayer::pick(1, 0, random) == 0);
}

TEST_CASE("the background music plays the music folder, fades and follows the settings", "[Music]")
{
	MusicFixture fixture;
	MusicPlayer& music = MusicPlayer::getInstance();
	Settings::getInstance()->setInt("MusicVolume", 80);

	music.init();
	REQUIRE(music.tracks().size() == 2); // .wav and .WAV, not .txt; sub-folders too
	CHECK(music.musicFolders().front() == es::paths::userDir(es::paths::Kind::Data) / "music");

	SECTION("off: nothing plays")
	{
		Settings::getInstance()->setBool("BackgroundMusic", false);
		music.play();
		CHECK_FALSE(music.isPlaying());
		Settings::getInstance()->setBool("BackgroundMusic", true);
		music.applySettings();
		CHECK(music.isPlaying());
	}

	SECTION("fades in, pauses, resumes, changes track")
	{
		music.play();
		REQUIRE(music.isPlaying());
		CHECK(music.gain() == 0.0f);
		REQUIRE(runUntil([&] { return music.gain() == 0.8f; }, std::chrono::seconds(5)));

		music.pause();
		REQUIRE(runUntil([&] { return music.gain() == 0.0f; }, std::chrono::seconds(5)));
		CHECK(music.isPlaying()); // paused, not stopped
		music.resume();
		REQUIRE(runUntil([&] { return music.gain() == 0.8f; }, std::chrono::seconds(5)));

		// 0.2 s tracks: a few changes, never twice the same in a row
		std::vector<std::string> played;
		runUntil(
			[&] {
				const std::string track = music.currentTrack();
				if(!track.empty() && (played.empty() || played.back() != track))
					played.push_back(track);
				return played.size() >= 3;
			},
			std::chrono::seconds(10));
		CHECK(played.size() >= 3);
		CHECK(std::set<std::string>(played.begin(), played.end()).size() == 2);

		music.stop();
		CHECK_FALSE(music.isPlaying());
		CHECK(music.currentTrack().empty());
	}
}
