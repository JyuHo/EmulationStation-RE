#pragma once

#include <string>
#include <vector>
#include <SDL3/SDL_audio.h>

class MusicPlayer
{
public:
	static MusicPlayer& getInstance();

	void init();
	void deinit();

	void play();
	void stop();
	void pause();
	void resume();

	void update();

	bool isPlaying() const;

private:
	MusicPlayer();

	std::vector<std::string> mTracks;
	std::vector<float> mPcm;
	SDL_AudioStream* mStream = nullptr;
	bool mPlaying = false;
	size_t mCurrent = 0;

	void scanTracks();
	bool loadTrack(const std::string& path);
	void playCurrent();
};
