// Settings reads used by the theme options (docs/THEMES.md sections 8 and 9).
#include "Settings.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Settings::getStringOr falls back without an error", "[Settings][11e2]")
{
	Settings* settings = Settings::getInstance();
	CHECK(settings->getStringOr("NoSuchSettingForTheTest", "fallback") == "fallback");
	settings->setString("SettingsTestKey", "value");
	CHECK(settings->getStringOr("SettingsTestKey", "fallback") == "value");
}

TEST_CASE("Settings::getStringsWithPrefix lists the string settings of a prefix", "[Settings][11e2]")
{
	Settings* settings = Settings::getInstance();
	settings->setString("ThemeOption.test-set.colorset", "red");
	settings->setString("ThemeOption.test-set.layout", "compact");
	settings->setString("ThemeOption.other-set.colorset", "blue");
	const auto found = settings->getStringsWithPrefix("ThemeOption.test-set.");
	REQUIRE(found.size() == 2);
	CHECK(found[0] == std::pair<std::string, std::string>{"ThemeOption.test-set.colorset", "red"});
	CHECK(found[1] == std::pair<std::string, std::string>{"ThemeOption.test-set.layout", "compact"});
}
