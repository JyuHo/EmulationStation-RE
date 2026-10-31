#include "video/SdlAudioSink.h"

#include "AudioManager.h"
#include "Log.h"

#include <SDL3/SDL_audio.h>

namespace {
constexpr SDL_AudioSpec kSource{SDL_AUDIO_F32, 2, 48000};
constexpr double kBytesPerSecond = 48000.0 * 2 * sizeof(float);
} // namespace

SdlAudioSink::SdlAudioSink()
{
	AudioManager::getInstance(); // opens the device
	if(AudioManager::getDevice() == 0)
		return;
	mStream = SDL_CreateAudioStream(&kSource, &AudioManager::getFormat());
	if(!mStream)
	{
		Log::write(LogError, "Video sound: cannot create an audio stream: {}", SDL_GetError());
		return;
	}
	setPaused(false);
}

SdlAudioSink::~SdlAudioSink()
{
	if(mStream)
		SDL_DestroyAudioStream(mStream); // unbinds it
}

void SdlAudioSink::put(std::span<const float> samples)
{
	if(mStream)
		SDL_PutAudioStreamData(mStream, samples.data(), static_cast<int>(samples.size_bytes()));
}

double SdlAudioSink::queuedSeconds() const
{
	if(!mStream)
		return 0;
	const int queued = SDL_GetAudioStreamQueued(mStream);
	return queued > 0 ? queued / kBytesPerSecond : 0;
}

void SdlAudioSink::clear()
{
	if(mStream)
		SDL_ClearAudioStream(mStream);
}

void SdlAudioSink::setPaused(bool paused)
{
	if(!mStream || mBound == !paused)
		return;
	if(paused)
		SDL_UnbindAudioStream(mStream);
	else if(SDL_BindAudioStream(AudioManager::getDevice(), mStream))
		SDL_ResumeAudioDevice(AudioManager::getDevice());
	else
	{
		Log::write(LogError, "Video sound: cannot bind the audio stream: {}", SDL_GetError());
		return;
	}
	mBound = !paused;
}
