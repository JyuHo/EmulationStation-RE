#pragma once

#include <cstddef>
#include <string>
#include <vector>

class ImageIO
{
public:
	// Decodes a PNG, JPEG, BMP, TGA, GIF (first frame) or WebP image into 32-bit RGBA.
	// Rows are returned bottom-up (OpenGL convention), as the FreeImage-based loader did.
	// Returns an empty vector on failure.
	static std::vector<unsigned char> loadFromMemoryRGBA32(
		const unsigned char* data, const size_t size, size_t& width, size_t& height);
	static void flipPixelsVert(unsigned char* imagePx, const size_t& width, const size_t& height);

	// Resizes the image file at path in place, keeping its format (PNG, JPEG or WebP;
	// other formats are rewritten as PNG data). Pass 0 for maxWidth or maxHeight to keep
	// the aspect ratio.
	static bool resizeImageFile(const std::string& path, int maxWidth, int maxHeight);
};
