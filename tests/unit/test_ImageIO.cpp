#include "ImageIO.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <webp/encode.h>

#include "stb/stb_image_write.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <string>
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

namespace {
std::vector<char> readFixture(const std::string& name)
{
	std::ifstream in(std::filesystem::path(ES_TEST_DATA_DIR) / "images" / name, std::ios::binary);
	REQUIRE(in);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::expected<ImageIO::Image, std::string> loadFixture(const std::string& name)
{
	const std::vector<char> bytes = readFixture(name);
	return ImageIO::load(std::as_bytes(std::span(bytes)));
}

// RGBA of pixel (x, y), y counted from the top of the picture; rows are stored bottom-up.
std::array<int, 4> pixelAt(const std::vector<std::uint8_t>& rgba, int width, int height, int x, int y)
{
	const std::size_t i = (std::size_t(height - 1 - y) * width + x) * 4;
	return {rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]};
}

bool near(const std::array<int, 4>& a, const std::array<int, 4>& b, int tolerance = 0)
{
	for(int c = 0; c < 4; c++)
		if(std::abs(a[c] - b[c]) > tolerance)
			return false;
	return true;
}

constexpr std::array<int, 4> kRed{255, 0, 0, 255};
constexpr std::array<int, 4> kGreen{0, 255, 0, 255};
constexpr std::array<int, 4> kBlue{0, 0, 255, 255};
} // namespace

// tests/data/images: top half red, bottom half blue (still images); red, green, blue
// frames of 50, 100 and 200 ms (animations). See tests/data/images/README.md.
TEST_CASE("ImageIO::load decodes still images bottom-up", "[ImageIO]")
{
	const auto [name, width, height, tolerance] = GENERATE(table<std::string, int, int, int>({
		{"still.png", 4, 2, 0}, {"still.webp", 4, 2, 0}, {"still.jpg", 16, 16, 8}, // lossy
	}));
	CAPTURE(name);

	const auto image = loadFixture(name);
	REQUIRE(image.has_value());
	CHECK(image->width == width);
	CHECK(image->height == height);
	CHECK_FALSE(image->isAnimated());
	REQUIRE(image->rgba.size() == std::size_t(width) * height * 4);
	CHECK(near(pixelAt(image->rgba, width, height, 0, 0), kRed, tolerance));
	CHECK(near(pixelAt(image->rgba, width, height, width - 1, height - 1), kBlue, tolerance));
	// first row in memory = bottom of the picture (OpenGL convention of the renderer)
	CHECK(near({image->rgba[0], image->rgba[1], image->rgba[2], image->rgba[3]}, kBlue, tolerance));
}

TEST_CASE("ImageIO::load decodes every frame of animated GIF and WebP", "[ImageIO]")
{
	const std::string name = GENERATE("animated.gif", "animated.webp");
	CAPTURE(name);

	const auto image = loadFixture(name);
	REQUIRE(image.has_value());
	CHECK(image->width == 4);
	CHECK(image->height == 4);
	REQUIRE(image->isAnimated());
	REQUIRE(image->frames.size() == 3);

	const std::array<std::array<int, 4>, 3> colors{kRed, kGreen, kBlue};
	const std::array<int, 3> durations{50, 100, 200};
	for(std::size_t i = 0; i < 3; i++)
	{
		CAPTURE(i);
		CHECK(image->frames[i].durationMs == durations[i]);
		REQUIRE(image->frames[i].rgba.size() == 4 * 4 * 4);
		CHECK(near(pixelAt(image->frames[i].rgba, 4, 4, 0, 0), colors[i]));
		CHECK(near(pixelAt(image->frames[i].rgba, 4, 4, 3, 3), colors[i]));
	}
	// the still image of an animation is its first frame
	CHECK(image->rgba == image->frames[0].rgba);
}

TEST_CASE("ImageIO::load reports errors instead of crashing", "[ImageIO]")
{
	SECTION("empty data")
	{
		CHECK_FALSE(ImageIO::load({}).has_value());
	}
	SECTION("SVG is left to SVGResource")
	{
		const auto image = loadFixture("logo.svg");
		REQUIRE_FALSE(image.has_value());
		CHECK(image.error().find("SVG") != std::string::npos);
	}
	SECTION("truncated files")
	{
		const std::string name = GENERATE("still.png", "still.jpg", "still.webp", "animated.gif", "animated.webp");
		CAPTURE(name);
		std::vector<char> bytes = readFixture(name);
		bytes.resize(bytes.size() / 2);
		const auto image = ImageIO::load(std::as_bytes(std::span(bytes)));
		// stb_image decodes what it can of a truncated JPEG; the others must fail
		if(image)
			CHECK(image->rgba.size() == std::size_t(image->width) * image->height * 4);
		else
			CHECK_FALSE(image.error().empty());
	}
}

TEST_CASE("loadFromMemoryRGBA32 returns the first frame of an animation", "[ImageIO]")
{
	const std::vector<char> bytes = readFixture("animated.gif");
	size_t width = 0, height = 0;
	const auto rgba = ImageIO::loadFromMemoryRGBA32(
		reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), width, height);
	REQUIRE(rgba.size() == 4 * 4 * 4);
	CHECK(width == 4);
	CHECK(height == 4);
	CHECK(near(pixelAt(rgba, 4, 4, 0, 0), kRed));
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
