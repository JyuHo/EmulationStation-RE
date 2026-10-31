#include "ImageIO.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

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

// Decodes to top-down RGBA. Returns false on failure.
bool decodeRGBA(const unsigned char* data, size_t size, std::vector<unsigned char>& out, int& width, int& height)
{
	if(detectFormat(data, size) == ImageFormat::WebP)
	{
		uint8_t* px = WebPDecodeRGBA(data, size, &width, &height);
		if(px == nullptr)
			return false;
		out.assign(px, px + static_cast<size_t>(width) * height * 4);
		WebPFree(px);
		return true;
	}

	int channels = 0;
	stbi_uc* px = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, 4);
	if(px == nullptr)
		return false;
	out.assign(px, px + static_cast<size_t>(width) * height * 4);
	stbi_image_free(px);
	return true;
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

std::vector<unsigned char> ImageIO::loadFromMemoryRGBA32(
	const unsigned char* data, const size_t size, size_t& width, size_t& height)
{
	width = 0;
	height = 0;

	std::vector<unsigned char> rawData;
	int w = 0, h = 0;
	if(!decodeRGBA(data, size, rawData, w, h))
	{
		LOG(LogError) << "Error - Failed to load image from memory: " << stbi_failure_reason();
		return {};
	}

	width = static_cast<size_t>(w);
	height = static_cast<size_t>(h);
	flipPixelsVert(rawData.data(), width, height);
	return rawData;
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

	std::vector<unsigned char> pixels;
	int width = 0, height = 0;
	if(!decodeRGBA(fileData.data(), fileData.size(), pixels, width, height))
	{
		LOG(LogError) << "Error - could not decode image \"" << path << "\"!";
		return false;
	}

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

std::expected<ImageIO::Image, std::string> ImageIO::load(std::span<const std::byte> data)
{
	ImageIO::Image result;
	const auto* bytes = reinterpret_cast<const unsigned char*>(data.data());
	const size_t size = data.size();
	if(size == 0)
		return std::unexpected(std::string("empty data"));

	if(size >= 12 && std::memcmp(bytes, "RIFF", 4) == 0 && std::memcmp(bytes + 8, "WEBP", 4) == 0)
	{
		WebPBitstreamFeatures features{};
		if(WebPGetFeatures(bytes, size, &features) != VP8_STATUS_OK)
			return std::unexpected(std::string("invalid WebP"));
		result.width = features.width;
		result.height = features.height;
		if(features.has_animation)
		{
			WebPAnimDecoderOptions decOpts{};
			WebPAnimDecoderOptionsInit(&decOpts);
			WebPData webpData{};
			webpData.bytes = bytes;
			webpData.size = size;
			WebPAnimDecoder* dec = WebPAnimDecoderNew(&webpData, &decOpts);
			if(!dec)
				return std::unexpected(std::string("WebP anim decode failed"));
			WebPAnimInfo info{};
			WebPAnimDecoderGetInfo(dec, &info);
			int timestamp = 0;
			uint8_t* framePtr = nullptr;
			int prevTimestamp = 0;
			while(WebPAnimDecoderGetNext(dec, &framePtr, &timestamp))
			{
				Frame f;
				f.durationMs = timestamp - prevTimestamp;
				if(f.durationMs <= 0) f.durationMs = 100;
				prevTimestamp = timestamp;
				f.rgba.assign(framePtr, framePtr + static_cast<size_t>(info.canvas_width) * info.canvas_height * 4);
				result.frames.push_back(std::move(f));
			}
			WebPAnimDecoderDelete(dec);
			if(!result.frames.empty())
			{
				result.rgba = result.frames.front().rgba;
				result.width = info.canvas_width;
				result.height = info.canvas_height;
			}
			if(result.frames.size() <= 1)
				result.frames.clear();
			return result;
		}
		uint8_t* px = WebPDecodeRGBA(bytes, size, &result.width, &result.height);
		if(!px)
			return std::unexpected(std::string("WebP decode failed"));
		result.rgba.assign(px, px + static_cast<size_t>(result.width) * result.height * 4);
		WebPFree(px);
		return result;
	}

	if(size >= 6 && (std::memcmp(bytes, "GIF87a", 6) == 0 || std::memcmp(bytes, "GIF89a", 6) == 0))
	{
		int* delays = nullptr;
		int x = 0, y = 0, z = 0, comp = 0;
		unsigned char* px = stbi_load_gif_from_memory(bytes, static_cast<int>(size), &delays, &x, &y, &z, &comp, 4);
		if(!px)
			return std::unexpected(std::string("GIF decode failed"));
		result.width = x;
		result.height = y;
		const int frameSize = x * y * 4;
		for(int i = 0; i < z; i++)
		{
			Frame f;
			f.durationMs = (delays ? delays[i] : 10) * 10;
			if(f.durationMs <= 0) f.durationMs = 100;
			f.rgba.assign(px + static_cast<size_t>(i) * frameSize, px + static_cast<size_t>(i + 1) * frameSize);
			result.frames.push_back(std::move(f));
		}
		stbi_image_free(px);
		if(!result.frames.empty())
			result.rgba = result.frames.front().rgba;
		if(result.frames.size() <= 1)
			result.frames.clear();
		return result;
	}

	{
		int w = 0, h = 0, ch = 0;
		stbi_uc* px = stbi_load_from_memory(bytes, static_cast<int>(size), &w, &h, &ch, 4);
		if(!px)
			return std::unexpected(std::string(stbi_failure_reason() ? stbi_failure_reason() : "decode failed"));
		result.width = w;
		result.height = h;
		result.rgba.assign(px, px + static_cast<size_t>(w) * h * 4);
		stbi_image_free(px);
		return result;
	}
}
