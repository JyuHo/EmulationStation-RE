#pragma once

#include "video/VideoPlayer.h"

struct SDL_AudioStream;

// VideoAudioSink on the AudioManager's device: one SDL_AudioStream bound next to the
// streams of the sounds (no manual mixing). Pausing unbinds the stream, which keeps its
// queued sound: the navigation sounds keep playing.
class SdlAudioSink final : public VideoAudioSink
{
public:
	SdlAudioSink();
	~SdlAudioSink() override;

	SdlAudioSink(const SdlAudioSink&) = delete;
	SdlAudioSink& operator=(const SdlAudioSink&) = delete;

	[[nodiscard]] bool isValid() const { return mStream != nullptr; }

	void put(std::span<const float> samples) override;
	[[nodiscard]] double queuedSeconds() const override;
	void clear() override;
	void setPaused(bool paused) override;

private:
	SDL_AudioStream* mStream = nullptr;
	bool mBound = false;
};
