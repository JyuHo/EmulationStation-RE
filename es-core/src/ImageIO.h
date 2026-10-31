#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

class ImageIO
{
public:
	struct Frame
	{
		int durationMs = 0;
		std::vector<std::uint8_t> rgba; // width * height * 4 bytes, rows bottom-up
	};

	struct Image
	{
		int width = 0;
		int height = 0;
		std::vector<std::uint8_t> rgba; // first frame, rows bottom-up
		std::vector<Frame> frames;		// every frame of an animation (2 or more); empty for a still image

		[[nodiscard]] bool isAnimated() const { return !frames.empty(); }
	};

	// Decodes PNG, JPEG, BMP, TGA, GIF (all frames) or WebP (still or animated, through
	// libwebpdemux) into 32-bit RGBA. The format is detected from the signature. Rows are
	// bottom-up, the convention of every texture in the renderer. SVG is not handled here:
	// SVGResource rasterizes it at the displayed size.
	// Thread-safe: called from the texture loader threads.
	[[nodiscard]] static std::expected<Image, std::string> load(std::span<const std::byte> data);

	// First frame only, rows bottom-up. Returns an empty vector on failure.
	static std::vector<unsigned char> loadFromMemoryRGBA32(
		const unsigned char* data, const size_t size, size_t& width, size_t& height);
	static void flipPixelsVert(unsigned char* imagePx, const size_t& width, const size_t& height);

	// Resizes the image file at path in place, keeping its format (PNG, JPEG or WebP;
	// other formats are rewritten as PNG data). Pass 0 for maxWidth or maxHeight to keep
	// the aspect ratio.
	static bool resizeImageFile(const std::string& path, int maxWidth, int maxHeight);
};
