#pragma once

#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "renderer/Renderer.h"

// Minimal video player: decodes a video file with FFmpeg and exposes frames as RGBA.
// Audio is not handled in this first step.
class VideoPlayer
{
public:
	VideoPlayer();
	~VideoPlayer();

	bool open(const std::string& path);
	void close();

	bool isOpen() const { return mFormat != nullptr; }

	// Advance by deltaTime (ms). Returns true if a new frame is available.
	bool update(int deltaTime);

	// Current frame as RGBA (bottom-up). Empty if none.
	const std::vector<unsigned char>& frame() const { return mFrame; }
	int width() const { return mWidth; }
	int height() const { return mHeight; }

private:
	struct AVFormatContext* mFormat = nullptr;
	struct AVCodecContext* mCodec = nullptr;
	int mVideoStream = -1;
	double mFrameRate = 30.0;
	double mAccumulator = 0.0;
	std::vector<unsigned char> mFrame;
	int mWidth = 0;
	int mHeight = 0;
};
