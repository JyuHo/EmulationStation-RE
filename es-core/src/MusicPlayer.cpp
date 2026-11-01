#include "MusicPlayer.h"

#include "AudioManager.h"
#include "Log.h"
#include "Settings.h"
#include "ThemeData.h"
#include "audio/AudioDecoder.h"
#include "utils/Paths.h"

#include <SDL3/SDL_audio.h>

#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace {

bool isMusicFile(const fs::path& path)
{
	std::string ext = path.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	static const std::vector<std::string> kExtensions = {".mp3", ".ogg", ".wav", ".flac", ".opus", ".m4a"};
	return std::find(kExtensions.begin(), kExtensions.end(), ext) != kExtensions.end();
}

} // namespace

MusicPlayer& MusicPlayer::getInstance()
{
	static MusicPlayer instance;
	return instance;
}

MusicPlayer::MusicPlayer() : mRandom(std::random_device{}()) {}

std::vector<fs::path> MusicPlayer::musicFolders()
{
	std::vector<fs::path> folders;
	folders.push_back(es::paths::userDir(es::paths::Kind::Data) / "music");
	for(const fs::path& base : es::paths::systemDirs(es::paths::Kind::Data))
		folders.push_back(base / "emulationstation" / "music");

	// the music of the current theme set
	const auto sets = ThemeData::getThemeSets();
	auto set = sets.find(Settings::getInstance()->getString("ThemeSet"));
	if(set == sets.end() && !sets.empty())
		set = sets.begin();
	if(set != sets.end())
		folders.push_back(set->second.path / "music");
	return folders;
}

void MusicPlayer::scanTracks()
{
	mTracks.clear();
	for(const fs::path& folder : musicFolders())
	{
		std::error_code error;
		if(!fs::is_directory(folder, error))
			continue;
		for(auto it = fs::recursive_directory_iterator(folder, fs::directory_options::skip_permission_denied, error);
			it != fs::recursive_directory_iterator(); it.increment(error))
		{
			if(error)
				break;
			if(it->is_regular_file(error) && isMusicFile(it->path()))
				mTracks.push_back(it->path().string());
		}
	}
	std::sort(mTracks.begin(), mTracks.end());
}

std::size_t MusicPlayer::pick(std::size_t count, std::optional<std::size_t> previous, std::mt19937& random)
{
	if(count <= 1)
		return 0;
	if(previous && *previous < count)
	{
		std::uniform_int_distribution<std::size_t> distribution(0, count - 2);
		const std::size_t index = distribution(random);
		return index >= *previous ? index + 1 : index;
	}
	std::uniform_int_distribution<std::size_t> distribution(0, count - 1);
	return distribution(random);
}

void MusicPlayer::init()
{
	scanTracks();
}

void MusicPlayer::deinit()
{
	stop();
}

std::string MusicPlayer::currentTrack() const
{
	std::lock_guard lock(mMutex);
	return mCurrent ? mTracks[*mCurrent] : std::string();
}

float MusicPlayer::targetGain() const
{
	if(mPaused)
		return 0.0f;
	return std::clamp(Settings::getInstance()->getInt("MusicVolume"), 0, 100) / 100.0f;
}

void MusicPlayer::play()
{
	if(mStream || !Settings::getInstance()->getBool("BackgroundMusic"))
		return;
	if(mTracks.empty())
		scanTracks();
	if(mTracks.empty() || AudioManager::getDevice() == 0)
		return;

	const SDL_AudioSpec source{SDL_AUDIO_F32, AudioDecoder::kChannels, AudioDecoder::kRate};
	mStream = SDL_CreateAudioStream(&source, &AudioManager::getFormat());
	if(!mStream)
	{
		Log::write(LogError, "Music: cannot create an audio stream: {}", SDL_GetError());
		return;
	}
	if(!SDL_BindAudioStream(AudioManager::getDevice(), mStream))
	{
		Log::write(LogError, "Music: cannot bind the audio stream: {}", SDL_GetError());
		SDL_DestroyAudioStream(mStream);
		mStream = nullptr;
		return;
	}
	// fades in
	mGain = 0.0f;
	SDL_SetAudioStreamGain(mStream, mGain);
	mLastUpdate = std::chrono::steady_clock::now();
	SDL_ResumeAudioDevice(AudioManager::getDevice());
	mThread = std::jthread([this](std::stop_token stop) { run(stop); });
}

void MusicPlayer::stop()
{
	mThread = {}; // joins
	if(mStream)
	{
		SDL_DestroyAudioStream(mStream); // unbinds it
		mStream = nullptr;
	}
	std::lock_guard lock(mMutex);
	mCurrent.reset();
}

void MusicPlayer::pause()
{
	mPaused = true;
}

void MusicPlayer::resume()
{
	mPaused = false;
}

void MusicPlayer::applySettings()
{
	if(!Settings::getInstance()->getBool("BackgroundMusic"))
		stop();
	else
		play();
}

void MusicPlayer::update()
{
	const auto now = std::chrono::steady_clock::now();
	const double elapsed = std::chrono::duration<double>(now - mLastUpdate).count();
	mLastUpdate = now;
	if(!mStream)
		return;

	const float target = targetGain();
	const float step = (float)(elapsed / kFadeSeconds);
	if(mGain < target)
		mGain = std::min(target, mGain + step);
	else if(mGain > target)
		mGain = std::max(target, mGain - step);
	SDL_SetAudioStreamGain(mStream, mGain);
}

// The decoding thread: keeps kQueuedSeconds of sound in the stream, track after track.
void MusicPlayer::run(std::stop_token stop)
{
	const int bytesPerSecond = AudioDecoder::kRate * AudioDecoder::kChannels * (int)sizeof(float);
	const int wanted = (int)(kQueuedSeconds * bytesPerSecond);
	std::vector<bool> broken(mTracks.size(), false);
	std::size_t brokenCount = 0;
	AudioDecoder decoder;
	bool open = false;
	std::vector<float> chunk;

	while(!stop.stop_requested())
	{
		if(!open)
		{
			if(brokenCount == mTracks.size())
				return; // nothing playable: silence
			std::size_t index;
			{
				std::lock_guard lock(mMutex);
				index = pick(mTracks.size(), mCurrent, mRandom);
				mCurrent = index;
			}
			if(broken[index])
				continue;
			if(auto opened = decoder.open(mTracks[index]); !opened)
			{
				Log::write(LogError, "Music: cannot play {}: {}", mTracks[index], opened.error());
				broken[index] = true;
				brokenCount++;
				continue;
			}
			Log::write(LogInfo, "Music: {}", mTracks[index]);
			open = true;
		}

		if(SDL_GetAudioStreamQueued(mStream) >= wanted)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			continue;
		}
		chunk.clear();
		if(decoder.read(chunk, AudioDecoder::kRate / 10) == 0)
		{
			open = false; // the next track follows without a gap
			continue;
		}
		SDL_PutAudioStreamData(mStream, chunk.data(), (int)(chunk.size() * sizeof(float)));
	}
}
