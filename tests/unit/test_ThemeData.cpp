#include "ThemeData.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ThemeData loads a format 3 theme", "[ThemeData]")
{
	ThemeData theme;
	REQUIRE_NOTHROW(theme.loadFile(ES_TEST_DATA_DIR "/themes/minimal.xml"));

	const ThemeData::ThemeElement* text = theme.getElement("detailed", "md_description", "text");
	REQUIRE(text != nullptr);
	CHECK(text->get<unsigned int>("color") == 0x11223344u);
	CHECK(text->get<float>("fontSize") == 0.025f);

	const ThemeData::ThemeElement* image = theme.getElement("detailed", "md_image", "image");
	REQUIRE(image != nullptr);
	CHECK(image->get<glm::vec2>("pos").x == 0.1f);
	CHECK(image->get<glm::vec2>("maxSize").y == 0.5f);
}

TEST_CASE("ThemeData rejects invalid themes", "[ThemeData]")
{
	ThemeData theme;
	CHECK_THROWS(theme.loadFile(ES_TEST_DATA_DIR "/themes/no-version.xml"));
	CHECK_THROWS(theme.loadFile(ES_TEST_DATA_DIR "/themes/does-not-exist.xml"));
}

TEST_CASE("ThemeData resolves ./ paths and includes relative to the theme file", "[ThemeData]")
{
	const std::string dir = ES_TEST_DATA_DIR "/capture/themes/capture";
	ThemeData theme;
	REQUIRE_NOTHROW(theme.loadFile(dir + "/nes/theme.xml"));

	// "./../art/logo-nes.svg" in nes/theme.xml
	const ThemeData::ThemeElement* logo = theme.getElement("system", "logo", "image");
	REQUIRE(logo != nullptr);
	CHECK(logo->get<std::string>("path") == dir + "/nes/../art/logo-nes.svg");

	// "./art/background.png" in common.xml, included as "./../common.xml"
	const ThemeData::ThemeElement* background = theme.getElement("basic", "background", "image");
	REQUIRE(background != nullptr);
	CHECK(background->get<std::string>("path") == dir + "/nes/../art/background.png");
}

TEST_CASE("ThemeData reads the video element", "[ThemeData]")
{
	ThemeData theme;
	REQUIRE_NOTHROW(theme.loadFile(ES_TEST_DATA_DIR "/themes/video.xml"));

	const ThemeData::ThemeElement* video = theme.getElement("detailed", "md_video", "video");
	REQUIRE(video != nullptr);
	CHECK(video->get<glm::vec2>("pos") == glm::vec2(0.25f, 0.4f));
	CHECK(video->get<glm::vec2>("maxSize") == glm::vec2(0.45f, 0.4f));
	CHECK(video->get<glm::vec2>("origin") == glm::vec2(0.5f, 0.5f));
	CHECK(video->get<std::string>("default") == ES_TEST_DATA_DIR "/themes/default.mp4");
	CHECK(video->get<float>("delay") == 1.5f);
	CHECK(video->get<bool>("showSnapshotNoVideo"));
	CHECK(video->get<bool>("showSnapshotDelay"));
}
