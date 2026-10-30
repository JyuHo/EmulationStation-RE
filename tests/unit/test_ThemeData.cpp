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
	CHECK(image->get<Eigen::Vector2f>("pos").x() == 0.1f);
	CHECK(image->get<Eigen::Vector2f>("maxSize").y() == 0.5f);
}

TEST_CASE("ThemeData rejects invalid themes", "[ThemeData]")
{
	ThemeData theme;
	CHECK_THROWS(theme.loadFile(ES_TEST_DATA_DIR "/themes/no-version.xml"));
	CHECK_THROWS(theme.loadFile(ES_TEST_DATA_DIR "/themes/does-not-exist.xml"));
}
