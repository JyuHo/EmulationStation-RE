#include "Sound.h"
#include "audio/AudioDecoder.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

#include "AudioManager.h"
#include "Log.h"
#include "ThemeData.h"
#include "Util.h"
#include "utils/Paths.h"

#include <filesystem>

std::map<std::string, std::shared_ptr<Sound>> Sound::sMap;

Sound::Sound(const std::string& path) : mPath(path), mStream(nullptr), mPlaying(false) {}

Sound::~Sound()
{
	deinit();
}

std::shared_ptr<Sound> Sound::get(const std::string& path)
{
	auto it = sMap.find(path);
	if(it != sMap.end() && it->second)
		return it->second;

	std::shared_ptr<Sound> snd(new Sound(path));
	snd->init();
	sMap[path] = snd;
	AudioManager::getInstance()->registerSound(snd);
	return snd;
}

std::shared_ptr<Sound> Sound::getFromTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element)
{
	if(!theme)
		return get("");

	const ThemeData::ThemeElement* e = theme->getElement(view, element, "sound");
	if(!e || !e->has("path"))
		return get("");

	return get(e->get<std::string>("path"));
}

void Sound::init()
{
	loadFile(mPath);
}

void Sound::deinit()
{
	stop();
	if(mStream)
	{
		SDL_DestroyAudioStream(mStream);
		mStream = nullptr;
	}
}

void Sound::loadFile(const std::string& path)
{
	mPcm.clear();
	mPlaying = false;

	if(path.empty())
		return;

	auto pcm = AudioDecoder::decodeFile(path);
	if(!pcm)
	{
		LOG(LogError) << "Sound: cannot open " << path << ": " << pcm.error();
		return;
	}
	mPcm = std::move(*pcm);
}

void Sound::play()
{
	if(mPcm.empty())
		return;

	stop();

	SDL_AudioSpec srcSpec{SDL_AUDIO_F32, 2, 48000};
	mStream = SDL_CreateAudioStream(&srcSpec, &AudioManager::getFormat());
	if(!mStream)
	{
		LOG(LogError) << "Sound: cannot create stream: " << SDL_GetError();
		return;
	}
	if(!SDL_BindAudioStream(AudioManager::getDevice(), mStream))
	{
		LOG(LogError) << "Sound: cannot bind stream: " << SDL_GetError();
		SDL_DestroyAudioStream(mStream);
		mStream = nullptr;
		return;
	}
	SDL_PutAudioStreamData(mStream, mPcm.data(), (int)(mPcm.size() * sizeof(float)));
	SDL_ResumeAudioDevice(AudioManager::getDevice());
	mPlaying = true;
}

bool Sound::isPlaying() const
{
	if(!mStream)
		return false;
	if(SDL_GetAudioStreamQueued(mStream) > 0)
		return true;
	// finished
	const_cast<Sound*>(this)->stop();
	return false;
}

void Sound::stop()
{
	if(mStream)
	{
		SDL_UnbindAudioStream(mStream);
		SDL_DestroyAudioStream(mStream);
		mStream = nullptr;
	}
	mPlaying = false;
}

const Uint8* Sound::getData() const
{
	if(mPcm.empty())
		return nullptr;
	return reinterpret_cast<const Uint8*>(mPcm.data());
}

Uint32 Sound::getPosition() const
{
	return 0;
}

void Sound::setPosition(Uint32 /*newPosition*/)
{
}

Uint32 Sound::getLength() const
{
	return (Uint32)(mPcm.size() * sizeof(float));
}

Uint32 Sound::getLengthMS() const
{
	if(mPcm.empty()) return 0;
	return (Uint32)((mPcm.size() / 2) * 1000 / 48000);
}
