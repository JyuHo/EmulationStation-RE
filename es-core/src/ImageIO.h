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
		std::vector<std::uint8_t> rgba;
	};

	struct Image
	{
		int width = 0;
		int height = 0;
		std::vector<std::uint8_t> rgba; // first frame (or only frame)
		std::vector<Frame> frames;        // empty for static images
	};

	// Decodes PNG, JPEG, BMP, TGA, GIF (all frames) or WebP (static or animated)
	// into 32-bit RGBA. Rows are returned top-down (ImageIO convention for the new
	// facade); the legacy loader returned bottom-up. On failure returns an error.
	[[nodiscard]] static std::expected<Image, std::string> load(std::span<const std::byte> data);

	// Legacy API, kept for callers not yet migrated. Decodes into 32-bit RGBA,
	// bottom-up rows. Returns an empty vector on failure.
	static std::vector<unsigned char> loadFromMemoryRGBA32(
		const unsigned char* data, const size_t size, size_t& width, size_t& height);
	static void flipPixelsVert(unsigned char* imagePx, const size_t& width, const size_t& height);

	// Resizes the image file at path in place, keeping its format (PNG, JPEG or WebP;
	// other formats are rewritten as PNG data). Pass 0 for maxWidth or maxHeight to keep
	// the aspect ratio.
	static bool resizeImageFile(const std::string& path, int maxWidth, int maxHeight);
};