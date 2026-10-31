#include "AudioManager.h"

#include <SDL3/SDL.h>
#include "Log.h"

std::vector<std::shared_ptr<Sound>> AudioManager::sSoundVector;
SDL_AudioSpec AudioManager::sAudioFormat{SDL_AUDIO_S16, 2, 44100};
SDL_AudioStream* AudioManager::sStream = nullptr;
std::shared_ptr<AudioManager> AudioManager::sInstance;

// Called by SDL with the stream locked, when the device needs "additional" more bytes.
void AudioManager::mixAudio(void* /*userdata*/, SDL_AudioStream* stream, int additional, int /*total*/)
{
	if(additional <= 0)
		return;

	static std::vector<Uint8> buffer;
	buffer.assign(additional, 0); // silence
	const Uint32 len = additional;

	bool anyPlaying = false;
	//iterate through all our samples
	for(const std::shared_ptr<Sound>& sound : sSoundVector)
	{
		if(sound->isPlaying())
		{
			anyPlaying = true;
			//calculate rest length of current sample
			Uint32 restLength = (sound->getLength() - sound->getPosition());
			if(restLength > len)
			{
				//if stream length is smaller than smaple lenght, clip it
				restLength = len;
			}
			//mix sample into stream
			SDL_MixAudio(
				buffer.data(), &(sound->getData()[sound->getPosition()]), sAudioFormat.format, restLength, 1.0f);
			//set new sound position. if this is at or beyond the end of the sample, it will stop automatically
			sound->setPosition(sound->getPosition() + restLength);
		}
	}

	// with nothing queued, the device plays silence: no need to send it
	if(anyPlaying)
		SDL_PutAudioStreamData(stream, buffer.data(), additional);
}

// SDL_Quit() (Renderer::deinit) destroys the stream: the AudioManager can outlive it at exit
static bool streamAlive(SDL_AudioStream*& stream)
{
	if(stream != nullptr && SDL_WasInit(SDL_INIT_AUDIO) == 0)
		stream = nullptr;
	return stream != nullptr;
}

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
	//check if an AudioManager instance is already created, if not create one
	if(sInstance == nullptr)
	{
		sInstance = std::shared_ptr<AudioManager>(new AudioManager);
	}
	return sInstance;
}

void AudioManager::init()
{
	if(!SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		LOG(LogError) << "Error initializing SDL audio!\n" << SDL_GetError();
		return;
	}

	//stop playing all Sounds
	for(unsigned int i = 0; i < sSoundVector.size(); i++)
	{
		if(sSoundVector.at(i)->isPlaying())
		{
			sSoundVector[i]->stop();
		}
	}

	//Open the default device with a stream in our format, fed by mixAudio; it starts paused
	sStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &sAudioFormat, mixAudio, nullptr);
	if(sStream == nullptr)
	{
		LOG(LogError) << "AudioManager Error - Unable to open SDL audio: " << SDL_GetError();
	}
}

void AudioManager::deinit()
{
	//stop all playback
	stop();
	//completely tear down SDL audio. else SDL hogs audio resources and emulators might fail to start...
	if(streamAlive(sStream))
	{
		SDL_DestroyAudioStream(sStream); // also closes the device
		sStream = nullptr;
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
	for(unsigned int i = 0; i < sSoundVector.size(); i++)
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

	//unpause audio, the mixer will figure out if samples need to be played...
	if(streamAlive(sStream))
		SDL_ResumeAudioStreamDevice(sStream);
}

void AudioManager::stop()
{
	//stop playing all Sounds
	for(unsigned int i = 0; i < sSoundVector.size(); i++)
	{
		if(sSoundVector.at(i)->isPlaying())
		{
			sSoundVector[i]->stop();
		}
	}
	//pause audio
	if(streamAlive(sStream))
		SDL_PauseAudioStreamDevice(sStream);
}

void AudioManager::lock()
{
	if(streamAlive(sStream))
		SDL_LockAudioStream(sStream);
}

void AudioManager::unlock()
{
	if(streamAlive(sStream))
		SDL_UnlockAudioStream(sStream);
}
