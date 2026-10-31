#include "AudioManager.h"

#include <SDL3/SDL.h>
#include "Log.h"

std::vector<std::shared_ptr<Sound>> AudioManager::sSoundVector;
SDL_AudioSpec AudioManager::sAudioFormat{SDL_AUDIO_F32, 2, 48000};
SDL_AudioDeviceID AudioManager::sDevice = 0;
std::shared_ptr<AudioManager> AudioManager::sInstance;

AudioManager::AudioManager()
{
	init();
}

AudioManager::~AudioManager()
{
	deinit();
}

std::shared_ptr<AudioManager>& AudioManager::getInstance()
{
	if(sInstance == nullptr)
	{
		sInstance = std::shared_ptr<AudioManager>(new AudioManager);
	}
	return sInstance;
}

void AudioManager::shutdown()
{
	sInstance.reset();
}

void AudioManager::init()
{
	if(!SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		LOG(LogError) << "Error initializing SDL audio!\n" << SDL_GetError();
		return;
	}

	for(auto& s : sSoundVector)
		if(s && s->isPlaying())
			s->stop();

	sDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &sAudioFormat);
	if(sDevice == 0)
	{
		LOG(LogError) << "AudioManager Error - Unable to open SDL audio: " << SDL_GetError();
	}
}

void AudioManager::deinit()
{
	stop();
	if(sDevice != 0)
	{
		SDL_CloseAudioDevice(sDevice);
		sDevice = 0;
	}
	if(SDL_WasInit(SDL_INIT_AUDIO) != 0)
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void AudioManager::registerSound(std::shared_ptr<Sound>& sound)
{
	getInstance();
	sSoundVector.push_back(sound);
}

void AudioManager::unregisterSound(std::shared_ptr<Sound>& sound)
{
	getInstance();
	for(size_t i = 0; i < sSoundVector.size(); i++)
	{
		if(sSoundVector.at(i) == sound)
		{
			sSoundVector[i]->stop();
			sSoundVector.erase(sSoundVector.begin() + i);
			return;
		}
	}
	LOG(LogError) << "AudioManager Error - tried to unregister a sound that wasn't registered!";
}

void AudioManager::play()
{
	getInstance();
	if(sDevice != 0)
		SDL_ResumeAudioDevice(sDevice);
}

void AudioManager::stop()
{
	for(auto& s : sSoundVector)
		if(s)
			s->stop();
	if(sDevice != 0)
		SDL_PauseAudioDevice(sDevice);
}

void AudioManager::lock()
{
	// no-op: SDL3 device locking not available; streams are individually locked if needed
}

void AudioManager::unlock()
{
}
