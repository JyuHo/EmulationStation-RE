#include "ImageIO.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string_view>

#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

#include "Log.h"

namespace {
enum class ImageFormat
{
	Unknown,
	Png,
	Jpeg,
	WebP,
	Other
};

ImageFormat detectFormat(const unsigned char* data, size_t size)
{
	if(size >= 12 && std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WEBP", 4) == 0)
		return ImageFormat::WebP;
	if(size >= 8 && std::memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0)
		return ImageFormat::Png;
	if(size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
		return ImageFormat::Jpeg;
	return size > 0 ? ImageFormat::Other : ImageFormat::Unknown;
}

// Largest accepted canvas: keeps width * height * 4 * frames well inside size_t and int
// (stb_image takes int sizes) and refuses decompression bombs.
constexpr std::size_t kMaxPixels = std::size_t(16384) * 16384;
constexpr int kDefaultFrameMs = 100;

bool validSize(int width, int height)
{
	return width > 0 && height > 0 && std::size_t(width) * std::size_t(height) <= kMaxPixels;
}

// Browsers clamp frame delays of 0 or 10 ms to 100 ms: animations made for them expect it.
int frameDuration(int ms)
{
	return ms <= 10 ? kDefaultFrameMs : ms;
}

std::expected<ImageIO::Image, std::string> decodeWebP(const unsigned char* data, std::size_t size)
{
	ImageIO::Image image;
	WebPBitstreamFeatures features{};
	if(WebPGetFeatures(data, size, &features) != VP8_STATUS_OK)
		return std::unexpected(std::string("invalid WebP data"));

	if(!features.has_animation)
	{
		uint8_t* px = WebPDecodeRGBA(data, size, &image.width, &image.height);
		if(px == nullptr)
			return std::unexpected(std::string("WebP decoding failed"));
		if(validSize(image.width, image.height))
			image.rgba.assign(px, px + std::size_t(image.width) * image.height * 4);
		WebPFree(px);
		if(image.rgba.empty())
			return std::unexpected(std::string("WebP image too large"));
		return image;
	}

	WebPAnimDecoderOptions options;
	if(!WebPAnimDecoderOptionsInit(&options))
		return std::unexpected(std::string("libwebpdemux version mismatch"));
	options.color_mode = MODE_RGBA;
	options.use_threads = 0;
	const WebPData webpData{data, size};
	std::unique_ptr<WebPAnimDecoder, decltype(&WebPAnimDecoderDelete)> decoder(
		WebPAnimDecoderNew(&webpData, &options), &WebPAnimDecoderDelete);
	WebPAnimInfo info{};
	if(!decoder || !WebPAnimDecoderGetInfo(decoder.get(), &info))
		return std::unexpected(std::string("invalid animated WebP"));
	image.width = static_cast<int>(info.canvas_width);
	image.height = static_cast<int>(info.canvas_height);
	if(!validSize(image.width, image.height) || info.frame_count == 0 ||
		std::size_t(info.frame_count) > kMaxPixels / (std::size_t(image.width) * image.height))
		return std::unexpected(std::string("animated WebP too large"));

	const std::size_t frameBytes = std::size_t(image.width) * image.height * 4;
	int previous = 0;
	while(WebPAnimDecoderHasMoreFrames(decoder.get()))
	{
		uint8_t* px = nullptr;
		int timestamp = 0; // end of this frame, in ms since the start of the animation
		if(!WebPAnimDecoderGetNext(decoder.get(), &px, &timestamp))
			return std::unexpected(std::string("animated WebP decoding failed"));
		image.frames.push_back({frameDuration(timestamp - previous), std::vector<std::uint8_t>(px, px + frameBytes)});
		previous = timestamp;
	}
	if(image.frames.empty())
		return std::unexpected(std::string("animated WebP without frames"));
	return image;
}

std::expected<ImageIO::Image, std::string> decodeGif(const unsigned char* data, std::size_t size)
{
	ImageIO::Image image;
	int* delays = nullptr;
	int frames = 0, channels = 0;
	stbi_uc* px = stbi_load_gif_from_memory(
		data, static_cast<int>(size), &delays, &image.width, &image.height, &frames, &channels, 4);
	if(px == nullptr)
	{
		const char* reason = stbi_failure_reason();
		return std::unexpected(std::string("GIF decoding failed: ") + (reason ? reason : "unknown error"));
	}
	std::expected<ImageIO::Image, std::string> result = std::unexpected(std::string("GIF too large"));
	if(validSize(image.width, image.height) && frames > 0)
	{
		const std::size_t frameBytes = std::size_t(image.width) * image.height * 4;
		if(frames == 1)
			image.rgba.assign(px, px + frameBytes);
		else
			for(int i = 0; i < frames; i++)
				image.frames.push_back({frameDuration(delays ? delays[i] : 0),
					std::vector<std::uint8_t>(px + i * frameBytes, px + (i + 1) * frameBytes)});
		result = std::move(image);
	}
	stbi_image_free(px);
	stbi_image_free(delays);
	return result;
}

// Decodes every supported format; rows top-down, as the decoders produce them.
std::expected<ImageIO::Image, std::string> decodeTopDown(const unsigned char* data, std::size_t size)
{
	if(size == 0)
		return std::unexpected(std::string("empty image data"));
	// stb_image takes the size as an int
	if(size > std::size_t(std::numeric_limits<int>::max()))
		return std::unexpected(std::string("image file too large"));

	auto image = [&]() -> std::expected<ImageIO::Image, std::string> {
		if(size >= 12 && std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WEBP", 4) == 0)
			return decodeWebP(data, size);
		if(size >= 6 && (std::memcmp(data, "GIF87a", 6) == 0 || std::memcmp(data, "GIF89a", 6) == 0))
			return decodeGif(data, size);
		const std::string_view head(reinterpret_cast<const char*>(data), std::min<std::size_t>(size, 256));
		if(head.find("<svg") != std::string_view::npos || head.starts_with("<?xml"))
			return std::unexpected(std::string("SVG is rasterized by SVGResource, not ImageIO"));

		ImageIO::Image still;
		int channels = 0;
		stbi_uc* px = stbi_load_from_memory(data, static_cast<int>(size), &still.width, &still.height, &channels, 4);
		if(px == nullptr)
		{
			const char* reason = stbi_failure_reason();
			return std::unexpected(std::string("decoding failed: ") + (reason ? reason : "unknown error"));
		}
		if(validSize(still.width, still.height))
			still.rgba.assign(px, px + std::size_t(still.width) * still.height * 4);
		stbi_image_free(px);
		if(still.rgba.empty())
			return std::unexpected(std::string("image too large"));
		return still;
	}();

	// an animation's first frame is also its still image
	if(image && image->isAnimated())
		image->rgba = image->frames.front().rgba;
	return image;
}

// Area-averaging downscale (bilinear when upscaling), RGBA8.
std::vector<unsigned char> resizeRGBA(const std::vector<unsigned char>& src, int srcW, int srcH, int dstW, int dstH)
{
	std::vector<unsigned char> dst(static_cast<size_t>(dstW) * dstH * 4);
	const float scaleX = static_cast<float>(srcW) / dstW;
	const float scaleY = static_cast<float>(srcH) / dstH;

	for(int y = 0; y < dstH; y++)
	{
		const int y0 = std::min(static_cast<int>(y * scaleY), srcH - 1);
		const int y1 = std::clamp(static_cast<int>((y + 1) * scaleY), y0 + 1, srcH);
		for(int x = 0; x < dstW; x++)
		{
			const int x0 = std::min(static_cast<int>(x * scaleX), srcW - 1);
			const int x1 = std::clamp(static_cast<int>((x + 1) * scaleX), x0 + 1, srcW);
			unsigned int sum[4] = {0, 0, 0, 0};
			for(int sy = y0; sy < y1; sy++)
				for(int sx = x0; sx < x1; sx++)
					for(int c = 0; c < 4; c++)
						sum[c] += src[(static_cast<size_t>(sy) * srcW + sx) * 4 + c];
			const unsigned int count = static_cast<unsigned int>((y1 - y0) * (x1 - x0));
			for(int c = 0; c < 4; c++)
				dst[(static_cast<size_t>(y) * dstW + x) * 4 + c] = static_cast<unsigned char>(sum[c] / count);
		}
	}
	return dst;
}

void appendToVector(void* context, void* data, int size)
{
	auto* out = static_cast<std::vector<unsigned char>*>(context);
	auto* bytes = static_cast<unsigned char*>(data);
	out->insert(out->end(), bytes, bytes + size);
}
} // namespace

std::expected<ImageIO::Image, std::string> ImageIO::load(std::span<const std::byte> data)
{
	auto image = decodeTopDown(reinterpret_cast<const unsigned char*>(data.data()), data.size());
	if(!image)
		return image;
	const auto flip = [&](std::vector<std::uint8_t>& px) {
		flipPixelsVert(px.data(), image->width, image->height);
	};
	flip(image->rgba);
	for(Frame& frame : image->frames)
		flip(frame.rgba);
	return image;
}

std::vector<unsigned char> ImageIO::loadFromMemoryRGBA32(
	const unsigned char* data, const size_t size, size_t& width, size_t& height)
{
	width = 0;
	height = 0;

	auto image = load(std::as_bytes(std::span(data, size)));
	if(!image)
	{
		LOG(LogError) << "Error - Failed to load image from memory: " << image.error();
		return {};
	}

	width = static_cast<size_t>(image->width);
	height = static_cast<size_t>(image->height);
	return std::move(image->rgba);
}

void ImageIO::flipPixelsVert(unsigned char* imagePx, const size_t& width, const size_t& height)
{
	unsigned int temp;
	unsigned int* arr = (unsigned int*)imagePx;
	for(size_t y = 0; y < height / 2; y++)
	{
		for(size_t x = 0; x < width; x++)
		{
			temp = arr[x + (y * width)];
			arr[x + (y * width)] = arr[x + (height * width) - ((y + 1) * width)];
			arr[x + (height * width) - ((y + 1) * width)] = temp;
		}
	}
}

bool ImageIO::resizeImageFile(const std::string& path, int maxWidth, int maxHeight)
{
	if(maxWidth == 0 && maxHeight == 0)
		return true;

	std::vector<unsigned char> fileData;
	{
		std::ifstream in(path, std::ios::binary);
		if(!in)
		{
			LOG(LogError) << "Error - could not open image \"" << path << "\"!";
			return false;
		}
		fileData.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	auto image = decodeTopDown(fileData.data(), fileData.size());
	if(!image)
	{
		LOG(LogError) << "Error - could not decode image \"" << path << "\": " << image.error();
		return false;
	}
	const std::vector<unsigned char>& pixels = image->rgba;
	const int width = image->width;
	const int height = image->height;

	if(maxWidth == 0)
		maxWidth = static_cast<int>((static_cast<float>(maxHeight) / height) * width);
	else if(maxHeight == 0)
		maxHeight = static_cast<int>((static_cast<float>(maxWidth) / width) * height);
	maxWidth = std::max(maxWidth, 1);
	maxHeight = std::max(maxHeight, 1);

	const std::vector<unsigned char> resized = resizeRGBA(pixels, width, height, maxWidth, maxHeight);

	std::vector<unsigned char> encoded;
	bool ok = false;
	switch(detectFormat(fileData.data(), fileData.size()))
	{
	case ImageFormat::Jpeg:
		ok = stbi_write_jpg_to_func(appendToVector, &encoded, maxWidth, maxHeight, 4, resized.data(), 90) != 0;
		break;
	case ImageFormat::WebP: {
		uint8_t* out = nullptr;
		const size_t outSize = WebPEncodeRGBA(resized.data(), maxWidth, maxHeight, maxWidth * 4, 90.0f, &out);
		ok = outSize > 0;
		if(ok)
			encoded.assign(out, out + outSize);
		WebPFree(out);
	}
	break;
	default:
		ok =
			stbi_write_png_to_func(appendToVector, &encoded, maxWidth, maxHeight, 4, resized.data(), maxWidth * 4) != 0;
		break;
	}

	if(!ok)
	{
		LOG(LogError) << "Could not encode resized image \"" << path << "\"!";
		return false;
	}

	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
	if(!out)
	{
		LOG(LogError) << "Failed to save resized image!";
		return false;
	}
	return true;
}
