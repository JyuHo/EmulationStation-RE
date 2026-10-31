#include "Sound.h"
#include "AudioManager.h"
#include "Log.h"
#include "Settings.h"
#include "ThemeData.h"

#include <cstring>

std::map<std::string, std::shared_ptr<Sound>> Sound::sMap;

std::shared_ptr<Sound> Sound::get(const std::string& path)
{
	auto it = sMap.find(path);
	if(it != sMap.end())
		return it->second;

	std::shared_ptr<Sound> sound = std::shared_ptr<Sound>(new Sound(path));
	AudioManager::getInstance()->registerSound(sound);
	sMap[path] = sound;
	return sound;
}

std::shared_ptr<Sound> Sound::getFromTheme(
	const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element)
{
	LOG(LogInfo) << " req sound [" << view << "." << element << "]";

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "sound");
	if(!elem || !elem->has("path"))
	{
		LOG(LogInfo) << "   (missing)";
		return get("");
	}

	return get(elem->get<std::string>("path"));
}

Sound::Sound(const std::string& path) : mSampleData(nullptr), mSamplePos(0), mSampleLength(0), playing(false)
{
	loadFile(path);
}

Sound::~Sound()
{
	deinit();
}

void Sound::loadFile(const std::string& path)
{
	mPath = path;
	init();
}

void Sound::init()
{
	if(mSampleData != nullptr)
		deinit();

	if(mPath.empty())
		return;

	//load wav file via SDL
	SDL_AudioSpec wave;
	Uint8* data = nullptr;
	Uint32 dlen = 0;
	if(!SDL_LoadWAV(mPath.c_str(), &wave, &data, &dlen))
	{
		LOG(LogError) << "Error loading sound \"" << mPath << "\"!\n"
					  << "	" << SDL_GetError();
		return;
	}
	//convert to the mixer's format: stereo, 16bit, 44.1kHz
	const SDL_AudioSpec& format = AudioManager::getFormat();
	Uint8* converted = nullptr;
	int convertedLength = 0;
	if(!SDL_ConvertAudioSamples(&wave, data, (int)dlen, &format, &converted, &convertedLength))
	{
		LOG(LogError) << "Error converting sound \"" << mPath << "\" to 44.1kHz, 16bit, stereo format!\n"
					  << "	" << SDL_GetError();
	}
	else
	{
		//worked. set up member data
		auto buffer = std::make_unique<Uint8[]>(convertedLength);
		memcpy(buffer.get(), converted, convertedLength);
		AudioManager::lock();
		mSampleData = std::move(buffer);
		mSampleLength = convertedLength;
		mSamplePos = 0;
		AudioManager::unlock();
	}
	//free wav data now
	SDL_free(converted);
	SDL_free(data);
}

void Sound::deinit()
{
	playing = false;

	if(mSampleData)
	{
		AudioManager::lock();
		mSampleData.reset();
		mSampleLength = 0;
		mSamplePos = 0;
		AudioManager::unlock();
	}
}

void Sound::play()
{
	if(mSampleData == nullptr)
		return;

	if(!Settings::getInstance()->getBool("EnableSounds"))
		return;

	AudioManager::lock();
	if(playing)
	{
		//replay from start. rewind the sample to the beginning
		mSamplePos = 0;
	}
	else
	{
		//flag our sample as playing
		playing = true;
	}
	AudioManager::unlock();
	//tell the AudioManager to start playing samples
	AudioManager::getInstance()->play();
}

bool Sound::isPlaying() const
{
	return playing;
}

void Sound::stop()
{
	//flag our sample as playing and rewind its position
	AudioManager::lock();
	playing = false;
	mSamplePos = 0;
	AudioManager::unlock();
}

const Uint8* Sound::getData() const
{
	return mSampleData.get();
}

Uint32 Sound::getPosition() const
{
	return mSamplePos;
}

void Sound::setPosition(Uint32 newPosition)
{
	mSamplePos = newPosition;
	if(mSamplePos >= mSampleLength)
	{
		//got to or beyond the end of the sample. stop playing
		playing = false;
		mSamplePos = 0;
	}
}

Uint32 Sound::getLength() const
{
	return mSampleLength;
}

Uint32 Sound::getLengthMS() const
{
	//44100 samples per second, 2 channels (stereo)
	//I have no idea why the *0.75 is necessary, but otherwise it's inaccurate
	return (Uint32)((mSampleLength / 44100.0f / 2.0f * 0.75f) * 1000);
}
