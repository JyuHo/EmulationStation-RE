#pragma once

#include "ThemeData.h"

#include <map>
#include <optional>
#include <string>

namespace es::theme {

enum class MenuIcon
{
	Kodi, System, Updates, Controllers, Games, UI, Sound, Network, Scraper, Advanced, Quit, Restart, Shutdown,
	FastShutdown, License, Lists, Bios, RecalboxRGBDual, Tate, Arcade, Download, Themes, CardReader, Count
};

struct MenuFont
{
	std::string path;
	float size = 0.0f;
};

struct MenuTextStyle
{
	std::optional<MenuFont> font;
	std::optional<unsigned int> color;
	std::optional<unsigned int> selectedColor;
	std::optional<unsigned int> selectorColor;
	std::optional<unsigned int> separatorColor;
};

struct KeyboardTheme
{
	std::optional<unsigned int> keyColor;
	std::optional<unsigned int> keySelectedColor;
	std::optional<unsigned int> keyTextColor;
	std::optional<unsigned int> keyDisabledColor;
	std::optional<unsigned int> keyModifierColor;
	std::optional<unsigned int> keyModifierLockedColor;
	std::optional<unsigned int> keyTitleColor;
	std::optional<unsigned int> keyEditTextColor;
	std::optional<std::string> fontPath;

	static KeyboardTheme fromTheme(const ThemeData& theme, const std::string& view);
};

struct MenuTheme
{
	std::optional<float> height;
	std::optional<std::string> backgroundPath;
	std::optional<std::string> fadePath;
	std::optional<unsigned int> backgroundColor;

	MenuTextStyle title;
	MenuTextStyle footer;
	MenuTextStyle text;
	MenuTextStyle textSmall;
	MenuTextStyle section;
	std::optional<int> sectionAlignment; // 0 left, 1 right, 2 center

	std::optional<std::string> buttonPath;
	std::optional<std::string> buttonFilledPath;
	std::optional<std::string> switchOnPath;
	std::optional<std::string> switchOffPath;
	std::optional<std::string> sliderKnobPath;

	std::map<MenuIcon, std::string> icons;

	static MenuTheme fromTheme(const ThemeData& theme);
};

} // namespace es::theme
