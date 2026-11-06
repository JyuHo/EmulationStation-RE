#pragma once
#ifndef ES_CORE_AUDIO_AUDIO_MIXER_H
#define ES_CORE_AUDIO_AUDIO_MIXER_H

#include <SDL3/SDL_audio.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Audio
{
	class Decoder;

	// A sound effect fully decoded to the mixer's output format.
	struct Sample
	{
		std::vector<float> frames; // interleaved stereo, MIXER_RATE Hz
	};

	using SamplePtr = std::shared_ptr<const Sample>;

	constexpr int MAX_VOLUME = 128; // same scale as SDL_mixer's MIX_MAX_VOLUME

	// Minimal mixer on top of an SDL3 audio stream: a few sound-effect voices
	// plus one streamed music track with fades and volume.
	// Replaces SDL_mixer, whose SDL3 release needs SDL 3.4 (Debian trixie ships 3.2).
	class Mixer
	{
	public:
		static Mixer& getInstance();

		bool open();
		void close();
		bool isOpen() const { return mStream != nullptr; }

		// Sound effects (wav, ogg, mp3, flac)
		static SamplePtr loadSample(const std::string& path);
		int  playSample(const SamplePtr& sample); // returns a voice id, or -1
		bool isVoicePlaying(int voice);
		void stopVoice(int voice);

		// Music (streamed from disk)
		bool playMusic(const std::string& path, int fadeInMs);
		void fadeOutMusic(int fadeOutMs); // blocks until the fade is done
		void stopMusic();
		bool isMusicPlaying();
		void setMusicVolume(int volume); // 0..MAX_VOLUME

		// True once after the current music reached its end on its own.
		// Polled from the main thread, so that nothing runs on the audio thread.
		bool takeMusicFinished() { return mMusicFinished.exchange(false); }

	private:
		Mixer() = default;
		~Mixer();
		Mixer(const Mixer&) = delete;
		Mixer& operator=(const Mixer&) = delete;

		static void SDLCALL audioCallback(void* userdata, SDL_AudioStream* stream, int additionalAmount, int totalAmount);
		void mix(float* out, int frames);
		int  mixMusic(float* out, int frames);

		struct Voice
		{
			SamplePtr sample;
			size_t    position = 0; // in floats
			int       id = 0;
		};

		struct Music
		{
			std::unique_ptr<Decoder> decoder;
			SDL_AudioStream*         converter = nullptr;
			bool                     decoderDone = false;
			float                    gain = 1.0f;      // current fade gain
			float                    gainStep = 0.0f;  // per output frame
			bool                     stopAfterFade = false;
		};

		SDL_AudioStream*   mStream = nullptr;
		std::mutex         mLock;
		std::vector<Voice> mVoices;
		int                mNextVoiceId = 1;
		std::unique_ptr<Music> mMusic;
		float              mMusicVolume = 1.0f;
		std::atomic<bool>  mMusicFinished { false };
		std::vector<float> mScratch;
	};

} // Audio::

#endif // ES_CORE_AUDIO_AUDIO_MIXER_H
