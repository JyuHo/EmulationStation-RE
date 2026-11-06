#include "audio/AudioMixer.h"

#include "Log.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <thread>

#include "audio_decoders/dr_libs/dr_mp3.h"
#include "audio_decoders/dr_libs/dr_wav.h"

// Private copy of dr_flac : libchdr (inside libcheevos) exports its own drflac_* symbols
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define DRFLAC_API static
#define DR_FLAC_IMPLEMENTATION
#include "audio_decoders/dr_libs/dr_flac.h"
#pragma GCC diagnostic pop

#define STB_VORBIS_HEADER_ONLY
#include "audio_decoders/stb_vorbis/stb_vorbis.c"

namespace Audio
{
	static constexpr int MIXER_RATE     = 48000;
	static constexpr int MIXER_CHANNELS = 2;
	static constexpr int MAX_VOICES     = 16;
	static constexpr int CHUNK_FRAMES   = 1024;

	static const SDL_AudioSpec OUTPUT_SPEC = { SDL_AUDIO_F32, MIXER_CHANNELS, MIXER_RATE };

	//////////////////////////////////////////////////////////////////////////
	// Decoders : each one reads interleaved float frames in its native format

	class Decoder
	{
	public:
		virtual ~Decoder() = default;
		virtual size_t read(float* out, size_t frames) = 0; // returns frames read, 0 at the end

		SDL_AudioSpec spec { SDL_AUDIO_F32, 0, 0 };

		static std::unique_ptr<Decoder> open(const std::string& path);
	};

	class Mp3Decoder : public Decoder
	{
		drmp3 mMp3;
	public:
		bool init(const std::string& path)
		{
			if (!drmp3_init_file(&mMp3, path.c_str(), nullptr))
				return false;

			spec.channels = (int)mMp3.channels;
			spec.freq = (int)mMp3.sampleRate;
			return true;
		}
		~Mp3Decoder() override { drmp3_uninit(&mMp3); }
		size_t read(float* out, size_t frames) override { return (size_t)drmp3_read_pcm_frames_f32(&mMp3, frames, out); }
	};

	class FlacDecoder : public Decoder
	{
		drflac* mFlac = nullptr;
	public:
		bool init(const std::string& path)
		{
			mFlac = drflac_open_file(path.c_str(), nullptr);
			if (mFlac == nullptr)
				return false;

			spec.channels = (int)mFlac->channels;
			spec.freq = (int)mFlac->sampleRate;
			return true;
		}
		~FlacDecoder() override { if (mFlac) drflac_close(mFlac); }
		size_t read(float* out, size_t frames) override { return (size_t)drflac_read_pcm_frames_f32(mFlac, frames, out); }
	};

	class WavDecoder : public Decoder
	{
		drwav mWav;
		bool  mOpen = false;
	public:
		bool init(const std::string& path)
		{
			mOpen = drwav_init_file(&mWav, path.c_str(), nullptr);
			if (!mOpen)
				return false;

			spec.channels = (int)mWav.channels;
			spec.freq = (int)mWav.sampleRate;
			return true;
		}
		~WavDecoder() override { if (mOpen) drwav_uninit(&mWav); }
		size_t read(float* out, size_t frames) override { return (size_t)drwav_read_pcm_frames_f32(&mWav, frames, out); }
	};

	class VorbisDecoder : public Decoder
	{
		stb_vorbis* mVorbis = nullptr;
	public:
		bool init(const std::string& path)
		{
			int error = 0;
			mVorbis = stb_vorbis_open_filename(path.c_str(), &error, nullptr);
			if (mVorbis == nullptr)
				return false;

			stb_vorbis_info info = stb_vorbis_get_info(mVorbis);
			spec.channels = info.channels;
			spec.freq = (int)info.sample_rate;
			return true;
		}
		~VorbisDecoder() override { if (mVorbis) stb_vorbis_close(mVorbis); }
		size_t read(float* out, size_t frames) override
		{
			return (size_t)stb_vorbis_get_samples_float_interleaved(mVorbis, spec.channels, out, (int)(frames * spec.channels));
		}
	};

	template<typename T>
	static std::unique_ptr<Decoder> tryOpen(const std::string& path)
	{
		auto decoder = std::make_unique<T>();
		if (decoder->init(path) && decoder->spec.channels > 0 && decoder->spec.freq > 0)
			return decoder;

		return nullptr;
	}

	std::unique_ptr<Decoder> Decoder::open(const std::string& path)
	{
		const std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(path));

		std::unique_ptr<Decoder> decoder;
		if (ext == ".mp3")
			decoder = tryOpen<Mp3Decoder>(path);
		else if (ext == ".ogg" || ext == ".oga")
			decoder = tryOpen<VorbisDecoder>(path);
		else if (ext == ".flac")
			decoder = tryOpen<FlacDecoder>(path);
		else if (ext == ".wav")
			decoder = tryOpen<WavDecoder>(path);

		// Wrong or missing extension : probe every format
		if (!decoder) decoder = tryOpen<WavDecoder>(path);
		if (!decoder) decoder = tryOpen<VorbisDecoder>(path);
		if (!decoder) decoder = tryOpen<FlacDecoder>(path);
		if (!decoder) decoder = tryOpen<Mp3Decoder>(path);

		return decoder;
	}

	//////////////////////////////////////////////////////////////////////////
	// Mixer

	Mixer& Mixer::getInstance()
	{
		static Mixer instance;
		return instance;
	}

	Mixer::~Mixer()
	{
		close();
	}

	bool Mixer::open()
	{
		if (mStream != nullptr)
			return true;

		mStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &OUTPUT_SPEC, &Mixer::audioCallback, this);
		if (mStream == nullptr)
		{
			LOG(LogError) << "AudioMixer : unable to open the audio device : " << SDL_GetError();
			return false;
		}

		SDL_ResumeAudioStreamDevice(mStream);
		return true;
	}

	void Mixer::close()
	{
		if (mStream == nullptr)
			return;

		// Destroying the stream stops the callback before returning
		SDL_DestroyAudioStream(mStream);
		mStream = nullptr;

		std::lock_guard<std::mutex> lock(mLock);
		mVoices.clear();
		if (mMusic && mMusic->converter)
			SDL_DestroyAudioStream(mMusic->converter);
		mMusic.reset();
		mMusicFinished = false;
	}

	SamplePtr Mixer::loadSample(const std::string& path)
	{
		auto decoder = Decoder::open(path);
		if (!decoder)
			return nullptr;

		std::vector<float> pcm;
		std::vector<float> chunk((size_t)CHUNK_FRAMES * decoder->spec.channels);
		size_t frames;
		while ((frames = decoder->read(chunk.data(), CHUNK_FRAMES)) > 0)
			pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + frames * decoder->spec.channels);

		if (pcm.empty())
			return nullptr;

		Uint8* converted = nullptr;
		int convertedLength = 0;
		if (!SDL_ConvertAudioSamples(&decoder->spec, (const Uint8*)pcm.data(), (int)(pcm.size() * sizeof(float)), &OUTPUT_SPEC, &converted, &convertedLength))
		{
			LOG(LogError) << "AudioMixer : unable to convert \"" << path << "\" : " << SDL_GetError();
			return nullptr;
		}

		auto sample = std::make_shared<Sample>();
		sample->frames.assign((const float*)converted, (const float*)converted + convertedLength / sizeof(float));
		SDL_free(converted);

		return sample;
	}

	int Mixer::playSample(const SamplePtr& sample)
	{
		if (mStream == nullptr || sample == nullptr || sample->frames.empty())
			return -1;

		std::lock_guard<std::mutex> lock(mLock);

		if (mVoices.size() >= MAX_VOICES)
			mVoices.erase(mVoices.begin()); // drop the oldest voice

		Voice voice;
		voice.sample = sample;
		voice.id = mNextVoiceId++;
		if (mNextVoiceId <= 0)
			mNextVoiceId = 1;

		mVoices.push_back(voice);
		return voice.id;
	}

	bool Mixer::isVoicePlaying(int voice)
	{
		std::lock_guard<std::mutex> lock(mLock);
		return std::any_of(mVoices.cbegin(), mVoices.cend(), [voice](const Voice& v) { return v.id == voice; });
	}

	void Mixer::stopVoice(int voice)
	{
		std::lock_guard<std::mutex> lock(mLock);
		mVoices.erase(std::remove_if(mVoices.begin(), mVoices.end(), [voice](const Voice& v) { return v.id == voice; }), mVoices.end());
	}

	bool Mixer::playMusic(const std::string& path, int fadeInMs)
	{
		if (mStream == nullptr)
			return false;

		auto music = std::make_unique<Music>();
		music->decoder = Decoder::open(path);
		if (!music->decoder)
		{
			LOG(LogError) << "AudioMixer : unsupported or unreadable music file " << path;
			return false;
		}

		music->converter = SDL_CreateAudioStream(&music->decoder->spec, &OUTPUT_SPEC);
		if (music->converter == nullptr)
		{
			LOG(LogError) << "AudioMixer : " << SDL_GetError() << " for " << path;
			return false;
		}

		if (fadeInMs > 0)
		{
			music->gain = 0.0f;
			music->gainStep = 1.0f / (fadeInMs * (MIXER_RATE / 1000.0f));
		}

		std::unique_ptr<Music> previous;
		{
			std::lock_guard<std::mutex> lock(mLock);
			previous = std::move(mMusic);
			mMusic = std::move(music);
			mMusicFinished = false;
		}

		if (previous && previous->converter)
			SDL_DestroyAudioStream(previous->converter);

		return true;
	}

	void Mixer::fadeOutMusic(int fadeOutMs)
	{
		{
			std::lock_guard<std::mutex> lock(mLock);
			if (!mMusic)
				return;

			if (fadeOutMs <= 0 || mMusic->gain <= 0.0f)
			{
				mMusic->gain = 0.0f;
				mMusic->gainStep = -1.0f;
			}
			else
				mMusic->gainStep = -mMusic->gain / (fadeOutMs * (MIXER_RATE / 1000.0f));

			mMusic->stopAfterFade = true;
		}

		// The audio thread ends the music once the gain reaches 0
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(fadeOutMs, 0) + 250);
		while (isMusicPlaying() && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

		stopMusic();
	}

	void Mixer::stopMusic()
	{
		std::unique_ptr<Music> previous;
		{
			std::lock_guard<std::mutex> lock(mLock);
			previous = std::move(mMusic);
			mMusicFinished = false;
		}

		if (previous && previous->converter)
			SDL_DestroyAudioStream(previous->converter);
	}

	bool Mixer::isMusicPlaying()
	{
		std::lock_guard<std::mutex> lock(mLock);
		return mMusic != nullptr;
	}

	void Mixer::setMusicVolume(int volume)
	{
		std::lock_guard<std::mutex> lock(mLock);
		mMusicVolume = std::clamp(volume, 0, MAX_VOLUME) / (float)MAX_VOLUME;
	}

	void SDLCALL Mixer::audioCallback(void* userdata, SDL_AudioStream* stream, int additionalAmount, int /*totalAmount*/)
	{
		Mixer* mixer = static_cast<Mixer*>(userdata);

		const int frameSize = MIXER_CHANNELS * (int)sizeof(float);
		float buffer[CHUNK_FRAMES * MIXER_CHANNELS];

		while (additionalAmount > 0)
		{
			int frames = std::min(additionalAmount / frameSize, CHUNK_FRAMES);
			if (frames <= 0)
				frames = 1;

			mixer->mix(buffer, frames);
			SDL_PutAudioStreamData(stream, buffer, frames * frameSize);
			additionalAmount -= frames * frameSize;
		}
	}

	void Mixer::mix(float* out, int frames)
	{
		const int samples = frames * MIXER_CHANNELS;
		std::fill(out, out + samples, 0.0f);

		std::lock_guard<std::mutex> lock(mLock);

		if (mMusic)
			mixMusic(out, frames);

		for (auto it = mVoices.begin(); it != mVoices.end(); )
		{
			const auto& pcm = it->sample->frames;
			const size_t count = std::min((size_t)samples, pcm.size() - it->position);

			for (size_t i = 0; i < count; i++)
				out[i] += pcm[it->position + i];

			it->position += count;
			if (it->position >= pcm.size())
				it = mVoices.erase(it);
			else
				++it;
		}

		for (int i = 0; i < samples; i++)
			out[i] = std::clamp(out[i], -1.0f, 1.0f);
	}

	int Mixer::mixMusic(float* out, int frames)
	{
		Music& music = *mMusic;

		const int frameSize = MIXER_CHANNELS * (int)sizeof(float);
		const int wanted = frames * frameSize;
		const int decoderChannels = music.decoder->spec.channels;

		// Keep the converter fed with decoded frames
		while (!music.decoderDone && SDL_GetAudioStreamAvailable(music.converter) < wanted)
		{
			mScratch.resize((size_t)CHUNK_FRAMES * decoderChannels);
			size_t decoded = music.decoder->read(mScratch.data(), CHUNK_FRAMES);
			if (decoded == 0)
			{
				music.decoderDone = true;
				SDL_FlushAudioStream(music.converter);
				break;
			}

			SDL_PutAudioStreamData(music.converter, mScratch.data(), (int)(decoded * decoderChannels * sizeof(float)));
		}

		float musicBuffer[CHUNK_FRAMES * MIXER_CHANNELS];
		int got = SDL_GetAudioStreamData(music.converter, musicBuffer, std::min(wanted, (int)sizeof(musicBuffer)));
		int gotFrames = got > 0 ? got / frameSize : 0;

		bool fadedOut = false;
		for (int f = 0; f < gotFrames; f++)
		{
			const float gain = music.gain * mMusicVolume;
			out[f * 2]     += musicBuffer[f * 2] * gain;
			out[f * 2 + 1] += musicBuffer[f * 2 + 1] * gain;

			if (music.gainStep != 0.0f)
			{
				music.gain = std::clamp(music.gain + music.gainStep, 0.0f, 1.0f);
				if (music.gain >= 1.0f && music.gainStep > 0.0f)
					music.gainStep = 0.0f;
				else if (music.gain <= 0.0f && music.stopAfterFade)
				{
					fadedOut = true;
					break;
				}
			}
		}

		const bool ended = music.decoderDone && gotFrames < frames && SDL_GetAudioStreamAvailable(music.converter) <= 0;

		if (fadedOut || ended)
		{
			SDL_DestroyAudioStream(music.converter);
			mMusic.reset();

			if (ended && !fadedOut)
				mMusicFinished = true;
		}

		return gotFrames;
	}

} // Audio::
