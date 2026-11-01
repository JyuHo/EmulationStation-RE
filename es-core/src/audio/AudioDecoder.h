#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <vector>

// Decodes the first sound track of a file (WAV, OGG, MP3, FLAC, OPUS, M4A... anything FFmpeg
// reads) to interleaved stereo float at 48 kHz, the format of every SDL_AudioStream of ES
// (plan, phase 8). Reads on demand, so that a music track is streamed instead of being decoded
// whole when it starts.
class AudioDecoder
{
public:
	static constexpr int kRate = 48000;
	static constexpr int kChannels = 2;

	AudioDecoder();
	~AudioDecoder();
	AudioDecoder(const AudioDecoder&) = delete;
	AudioDecoder& operator=(const AudioDecoder&) = delete;

	std::expected<void, std::string> open(const std::string& path);
	// Appends up to `frames` stereo frames to out; returns how many, 0 at the end of the file.
	std::size_t read(std::vector<float>& out, std::size_t frames);
	[[nodiscard]] bool atEnd() const;

	// The whole file at once (navigation sounds).
	static std::expected<std::vector<float>, std::string> decodeFile(const std::string& path);

private:
	struct State;
	bool decodeMore();

	std::unique_ptr<State> mState;
};
