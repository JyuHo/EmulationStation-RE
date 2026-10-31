#ifndef _AUDIOMANAGER_H_
#define _AUDIOMANAGER_H_

#include <vector>
#include <memory>

#include <SDL3/SDL_audio.h>

#include "Sound.h"

class AudioManager
{
	static SDL_AudioSpec sAudioFormat;
	static SDL_AudioDeviceID sDevice;
	static std::vector<std::shared_ptr<Sound>> sSoundVector;
	static std::shared_ptr<AudioManager> sInstance;

	AudioManager();

public:
	static std::shared_ptr<AudioManager>& getInstance();

	void init();
	void deinit();

	void registerSound(std::shared_ptr<Sound>& sound);
	void unregisterSound(std::shared_ptr<Sound>& sound);

	void play();
	void stop();

	static void lock();
	static void unlock();

	static SDL_AudioDeviceID getDevice() { return sDevice; }
	static const SDL_AudioSpec& getFormat() { return sAudioFormat; }

	virtual ~AudioManager();
};

#endif