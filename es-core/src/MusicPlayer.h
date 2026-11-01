#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

struct SDL_AudioStream;

// Background music (plan, phase 8, as in Recalbox): the tracks of the music/ folders
// ($XDG_DATA_HOME/emulationstation/music, then $XDG_DATA_DIRS) and of the current theme set,
// played at random, never the same twice in a row. A thread decodes the track a little at a
// time (AudioDecoder) into an SDL_AudioStream bound to the AudioManager's device, next to the
// navigation sounds and the videos. Starting, pausing (a video with sound) and resuming fade
// the volume. Settings: BackgroundMusic, MusicVolume (0-100), from the Sound menu.
class MusicPlayer
{
public:
	static MusicPlayer& getInstance();

	void init();   // scans the tracks
	void deinit(); // stops at once (a game is launched, ES exits)

	void play(); // when BackgroundMusic is on and there are tracks
	void stop();
	void pause();  // fades out, keeps the position
	void resume(); // fades in
	// Main thread, every frame: the fades.
	void update();
	// BackgroundMusic or MusicVolume changed.
	void applySettings();

	[[nodiscard]] bool isPlaying() const { return mStream != nullptr; }
	[[nodiscard]] bool isPaused() const { return mPaused; }
	[[nodiscard]] float gain() const { return mGain; }
	[[nodiscard]] const std::vector<std::string>& tracks() const { return mTracks; }
	[[nodiscard]] std::string currentTrack() const;

	// Where the tracks are looked for, in order.
	static std::vector<std::filesystem::path> musicFolders();
	// A random index below count, other than previous when there is a choice.
	static std::size_t pick(std::size_t count, std::optional<std::size_t> previous, std::mt19937& random);

	static constexpr double kFadeSeconds = 1.0;
	static constexpr double kQueuedSeconds = 0.5; // decoded ahead

private:
	MusicPlayer();

	void scanTracks();
	void run(std::stop_token stop);
	[[nodiscard]] float targetGain() const;

	std::vector<std::string> mTracks;
	SDL_AudioStream* mStream = nullptr;
	bool mPaused = false;
	float mGain = 0.0f;
	std::chrono::steady_clock::time_point mLastUpdate;

	mutable std::mutex mMutex; // mCurrent, mRandom: shared with the thread
	std::optional<std::size_t> mCurrent;
	std::mt19937 mRandom;

	std::jthread mThread; // last: stopped before the rest is destroyed
};
