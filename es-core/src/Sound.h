#ifndef _SOUND_H_
#define _SOUND_H_

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <SDL3/SDL_audio.h>

class ThemeData;

class Sound
{
	std::string mPath;
	std::vector<float> mPcm; // interleaved stereo float32, 48 kHz
	SDL_AudioStream* mStream;
	bool mPlaying;

public:
	static std::shared_ptr<Sound> get(const std::string& path);
	static std::shared_ptr<Sound> getFromTheme(
		const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& elem);

	~Sound();

	void init();
	void deinit();

	void loadFile(const std::string& path);

	void play();
	bool isPlaying() const;
	void stop();

	const Uint8* getData() const;
	Uint32 getPosition() const;
	void setPosition(Uint32 newPosition);
	Uint32 getLength() const;
	Uint32 getLengthMS() const;

private:
	Sound(const std::string& path = "");
	static std::map<std::string, std::shared_ptr<Sound>> sMap;
};

#endif