#pragma once
#ifndef ES_CORE_SOUND_H
#define ES_CORE_SOUND_H

#include "audio/AudioMixer.h"
#include <map>
#include <memory>
#include <string>

class ThemeData;

class Sound
{
	std::string mPath;
	Audio::SamplePtr mSampleData;
	int mPlayingChannel;

public:
	static std::shared_ptr<Sound> get(const std::string& path);
	static std::shared_ptr<Sound> getFromTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& elem);

	~Sound();

	void init();
	void deinit();

	void loadFile(const std::string & path);

	void play();
	bool isPlaying() const;
	void stop();

private:
	Sound(const std::string & path = "");
	static std::map< std::string, std::shared_ptr<Sound> > sMap;
};

#endif // ES_CORE_SOUND_H
