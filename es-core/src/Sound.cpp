#include "Sound.h"

#include "AudioManager.h"
#include "Log.h"
#include "Settings.h"
#include "ThemeData.h"
#include "resources/ResourceManager.h"

std::map< std::string, std::shared_ptr<Sound> > Sound::sMap;

std::shared_ptr<Sound> Sound::get(const std::string& path)
{
	std::string file = ResourceManager::getInstance()->getResourcePath(path);

	auto it = sMap.find(file);
	if (it != sMap.cend())
		return it->second;

	std::shared_ptr<Sound> sound = std::shared_ptr<Sound>(new Sound(file));

	if (AudioManager::isInitialized())
	{
		AudioManager::getInstance()->registerSound(sound);
		sMap[file] = sound;
	}

	return sound;
}

std::shared_ptr<Sound> Sound::getFromTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element)
{
	LOG(LogInfo) << " req sound [" << view << "." << element << "]";

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "sound");
	if (!elem || !elem->has("path"))
	{
		LOG(LogInfo) << "   (missing)";
		return get("");
	}

	return get(elem->get<std::string>("path"));
}

Sound::Sound(const std::string & path) : mSampleData(nullptr), mPlayingChannel(-1)
{
	loadFile(path);
}

Sound::~Sound()
{
	deinit();
}

void Sound::loadFile(const std::string & path)
{
	mPath = path;
	init();
}

void Sound::init()
{
	deinit();

	if (!AudioManager::isInitialized())
		return;

	if (mPath.empty() || !Utils::FileSystem::exists(mPath))
		return;

	if (!Settings::getInstance()->getBool("EnableSounds"))
		return;

	// decode the whole file to the mixer format
	mSampleData = Audio::Mixer::loadSample(mPath);
	if (mSampleData == nullptr)
	{
		LOG(LogError) << "Error loading sound \"" << mPath << "\"";
		return;
	}
}

void Sound::deinit()
{
	if (mSampleData == nullptr)
		return;

	stop();
	mSampleData = nullptr;	
}

void Sound::play()
{
	if (mSampleData == nullptr)
		return;

	if (!Settings::getInstance()->getBool("EnableSounds"))
		return;

	mPlayingChannel = Audio::Mixer::getInstance().playSample(mSampleData);
}

bool Sound::isPlaying() const
{	
	return mPlayingChannel >= 0 && Audio::Mixer::getInstance().isVoicePlaying(mPlayingChannel);
}

void Sound::stop()
{
	if (mPlayingChannel < 0)
		return;

	Audio::Mixer::getInstance().stopVoice(mPlayingChannel);
	mPlayingChannel = -1;
}
