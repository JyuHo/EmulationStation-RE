#ifndef _AUDIOMANAGER_H_
#define _AUDIOMANAGER_H_

#include <vector>
#include <memory>

#include <SDL3/SDL_audio.h>

#include "Sound.h"

class AudioManager
{
	static SDL_AudioSpec sAudioFormat;
	static SDL_AudioStream* sStream;
	static std::vector<std::shared_ptr<Sound>> sSoundVector;
	static std::shared_ptr<AudioManager> sInstance;

	static void SDLCALL mixAudio(void* userdata, SDL_AudioStream* stream, int additional, int total);

	AudioManager();

public:
	static std::shared_ptr<AudioManager>& getInstance();

	void init();
	void deinit();

	void registerSound(std::shared_ptr<Sound>& sound);
	void unregisterSound(std::shared_ptr<Sound>& sound);

	void play();
	void stop();

	// the mixer runs on SDL's audio thread: lock around any change to what it reads
	static void lock();
	static void unlock();

	// output format of the mixer: 44.1 kHz, 16-bit, stereo
	static const SDL_AudioSpec& getFormat() { return sAudioFormat; }

	virtual ~AudioManager();
};

#endif
