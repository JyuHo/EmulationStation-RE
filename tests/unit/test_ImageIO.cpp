#include "ImageIO.h"

#include <catch2/catch_test_macros.hpp>
#include <webp/encode.h>

#include "stb/stb_image_write.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <vector>

namespace {
// 2x2 RGBA image, top row red, bottom row blue.
std::vector<unsigned char> makePixels()
{
	return {
		255,
		0,
		0,
		255,
		255,
		0,
		0,
		255,
		0,
		0,
		255,
		255,
		0,
		0,
		255,
		255,
	};
}

void appendToVector(void* context, void* data, int size)
{
	auto* out = static_cast<std::vector<unsigned char>*>(context);
	auto* bytes = static_cast<unsigned char*>(data);
	out->insert(out->end(), bytes, bytes + size);
}

std::vector<unsigned char> encodePng(int w, int h, const std::vector<unsigned char>& px)
{
	std::vector<unsigned char> out;
	stbi_write_png_to_func(appendToVector, &out, w, h, 4, px.data(), w * 4);
	return out;
}

std::vector<unsigned char> encodeWebpLossless(int w, int h, const std::vector<unsigned char>& px)
{
	uint8_t* data = nullptr;
	const size_t size = WebPEncodeLosslessRGBA(px.data(), w, h, w * 4, &data);
	std::vector<unsigned char> out(data, data + size);
	WebPFree(data);
	return out;
}
} // namespace

TEST_CASE("ImageIO decodes PNG bottom-up", "[ImageIO]")
{
	const auto png = encodePng(2, 2, makePixels());
	size_t w = 0, h = 0;
	const auto px = ImageIO::loadFromMemoryRGBA32(png.data(), png.size(), w, h);

	REQUIRE(w == 2);
	REQUIRE(h == 2);
	REQUIRE(px.size() == 16);
	// first returned row is the bottom (blue) row of the image
	CHECK(px[0] == 0);
	CHECK(px[2] == 255);
	CHECK(px[8] == 255);
	CHECK(px[10] == 0);
}

TEST_CASE("ImageIO decodes lossless WebP bottom-up", "[ImageIO]")
{
	const auto webp = encodeWebpLossless(2, 2, makePixels());
	size_t w = 0, h = 0;
	const auto px = ImageIO::loadFromMemoryRGBA32(webp.data(), webp.size(), w, h);

	REQUIRE(w == 2);
	REQUIRE(h == 2);
	CHECK(px[2] == 255); // blue first
	CHECK(px[8] == 255); // then red
}

TEST_CASE("ImageIO rejects garbage", "[ImageIO]")
{
	const unsigned char junk[] = {1, 2, 3, 4, 5, 6, 7, 8};
	size_t w = 1, h = 1;
	const auto px = ImageIO::loadFromMemoryRGBA32(junk, sizeof(junk), w, h);
	CHECK(px.empty());
	CHECK(w == 0);
	CHECK(h == 0);
}

TEST_CASE("ImageIO::load returns an image for PNG", "[ImageIO]")
{
	// reuse a generated PNG from the existing helper by writing one in memory
	// Build via stb_image_write to a buffer.
	std::vector<unsigned char> buf;
	auto writeFn = [](void* ctx, void* data, int size) {
		auto* v = static_cast<std::vector<unsigned char>*>(ctx);
		const auto* p = static_cast<const unsigned char*>(data);
		v->insert(v->end(), p, p + size);
	};
	const auto pixels = makePixels();
	stbi_write_png_to_func(writeFn, &buf, 2, 2, 4, pixels.data(), 2 * 4);
	REQUIRE(!buf.empty());

	std::vector<std::byte> bytes(buf.size());
	for(size_t i = 0; i < buf.size(); i++)
		bytes[i] = static_cast<std::byte>(buf[i]);
	const auto img = ImageIO::load(std::span<const std::byte>(bytes));
	REQUIRE(img.has_value());
	CHECK(img->width == 2);
	CHECK(img->height == 2);
	CHECK(img->rgba.size() == 2 * 2 * 4);
}

TEST_CASE("ImageIO resizes a file and keeps its format", "[ImageIO]")
{
	std::vector<unsigned char> big(64 * 32 * 4, 128);
	const auto path = std::filesystem::temp_directory_path() / "es-tests-resize.webp";
	{
		const auto webp = encodeWebpLossless(64, 32, big);
		std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(webp.data()), webp.size());
	}

	REQUIRE(ImageIO::resizeImageFile(path.string(), 16, 0));

	std::ifstream in(path, std::ios::binary);
	std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	REQUIRE(data.size() > 12);
	CHECK(std::string(data.begin() + 8, data.begin() + 12) == "WEBP");

	size_t w = 0, h = 0;
	ImageIO::loadFromMemoryRGBA32(data.data(), data.size(), w, h);
	CHECK(w == 16);
	CHECK(h == 8);

	std::filesystem::remove(path);
}
