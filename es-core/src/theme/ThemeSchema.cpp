#include "theme/ThemeSchema.h"

#include "utils/Paths.h"

#include "Util.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace es::theme {

namespace {
using PS = PropertySpec;

// Element types and their properties: docs/THEMES.md section 11 (Recalbox 10.1.1
// ThemeSupport.cpp, plus the format 3 properties of Aloshi that Recalbox dropped). Same order as
// the document; test_ThemeSchema.cpp compares both.
const PS imageProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"maxSize", PropertyType::NormalizedPair},
	{"keepratio", PropertyType::Boolean},
	{"origin", PropertyType::NormalizedPair},
	{"rotation", PropertyType::Float},
	{"rotationOrigin", PropertyType::NormalizedPair},
	{"path", PropertyType::Path},
	{"tile", PropertyType::Boolean},
	{"color", PropertyType::Color},
	{"colorTop", PropertyType::Color},
	{"colorBottom", PropertyType::Color},
	{"colorLeft", PropertyType::Color},
	{"colorRight", PropertyType::Color},
	{"colorTopLeft", PropertyType::Color},
	{"colorTopRight", PropertyType::Color},
	{"colorBottomLeft", PropertyType::Color},
	{"colorBottomRight", PropertyType::Color},
	{"zIndex", PropertyType::Float},
	{"disabled", PropertyType::Boolean},
	{"reflection", PropertyType::NormalizedPair},
};
const PS boxProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"origin", PropertyType::NormalizedPair},
	{"rotation", PropertyType::Float},
	{"rotationOrigin", PropertyType::NormalizedPair},
	{"color", PropertyType::Color},
	{"colorTop", PropertyType::Color},
	{"colorBottom", PropertyType::Color},
	{"colorLeft", PropertyType::Color},
	{"colorRight", PropertyType::Color},
	{"colorTopLeft", PropertyType::Color},
	{"colorTopRight", PropertyType::Color},
	{"colorBottomLeft", PropertyType::Color},
	{"colorBottomRight", PropertyType::Color},
	{"zIndex", PropertyType::Float},
	{"disabled", PropertyType::Boolean},
};
const PS videoProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"maxSize", PropertyType::NormalizedPair},
	{"keepratio", PropertyType::Boolean},
	{"origin", PropertyType::NormalizedPair},
	{"rotation", PropertyType::Float},
	{"rotationOrigin", PropertyType::NormalizedPair},
	{"path", PropertyType::Path},
	{"zIndex", PropertyType::Float},
	{"disabled", PropertyType::Boolean},
	{"animations", PropertyType::String},
	{"loops", PropertyType::Float},
	{"delay", PropertyType::Float},
	{"reflection", PropertyType::NormalizedPair},
	{"link", PropertyType::String},
	{"default", PropertyType::Path},
	{"showSnapshotNoVideo", PropertyType::Boolean},
	{"showSnapshotDelay", PropertyType::Boolean},
};
const PS textProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"origin", PropertyType::NormalizedPair},
	{"rotation", PropertyType::Float},
	{"rotationOrigin", PropertyType::NormalizedPair},
	{"text", PropertyType::String},
	{"path", PropertyType::Path},
	{"backgroundColor", PropertyType::Color},
	{"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float},
	{"fontStyle", PropertyType::String},
	{"color", PropertyType::Color},
	{"alignment", PropertyType::String},
	{"forceUppercase", PropertyType::Boolean},
	{"lineSpacing", PropertyType::Float},
	{"multiline", PropertyType::Boolean},
	{"zIndex", PropertyType::Float},
	{"disabled", PropertyType::Boolean},
};
const PS scrolltextProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"origin", PropertyType::NormalizedPair},
	{"rotation", PropertyType::Float},
	{"rotationOrigin", PropertyType::NormalizedPair},
	{"text", PropertyType::String},
	{"path", PropertyType::Path},
	{"backgroundColor", PropertyType::Color},
	{"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float},
	{"fontStyle", PropertyType::String},
	{"color", PropertyType::Color},
	{"alignment", PropertyType::String},
	{"forceUppercase", PropertyType::Boolean},
	{"zIndex", PropertyType::Float},
	{"disabled", PropertyType::Boolean},
};
const PS markdownProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"origin", PropertyType::NormalizedPair},
	{"rotation", PropertyType::Float},
	{"rotationOrigin", PropertyType::NormalizedPair},
	{"text", PropertyType::String},
	{"path", PropertyType::Path},
	{"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float},
	{"color", PropertyType::Color},
	{"alignment", PropertyType::String},
	{"forceUppercase", PropertyType::Boolean},
	{"lineSpacing", PropertyType::Float},
	{"zIndex", PropertyType::Float},
	{"disabled", PropertyType::Boolean},
};
const PS textlistProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"origin", PropertyType::NormalizedPair},
	{"selectorHeight", PropertyType::Float},
	{"selectorOffsetY", PropertyType::Float},
	{"selectorColor", PropertyType::Color},
	{"selectorImagePath", PropertyType::Path},
	{"selectorImageTile", PropertyType::Boolean},
	{"selectedColor", PropertyType::Color},
	{"primaryColor", PropertyType::Color},
	{"secondaryColor", PropertyType::Color},
	{"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float},
	{"scrollSound", PropertyType::Path},
	{"alignment", PropertyType::String},
	{"horizontalMargin", PropertyType::Float},
	{"forceUppercase", PropertyType::Boolean},
	{"lineSpacing", PropertyType::Float},
	{"zIndex", PropertyType::Float},
};
const PS containerProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"origin", PropertyType::NormalizedPair},
	{"zIndex", PropertyType::Float},
	{"disabled", PropertyType::Boolean},
};
const PS ninepatchProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"path", PropertyType::Path},
	{"zIndex", PropertyType::Float},
	{"disabled", PropertyType::Boolean},
};
const PS datetimeProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"origin", PropertyType::NormalizedPair},
	{"color", PropertyType::Color},
	{"backgroundColor", PropertyType::Color},
	{"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float},
	{"alignment", PropertyType::String},
	{"forceUppercase", PropertyType::Boolean},
	{"zIndex", PropertyType::Float},
	{"disabled", PropertyType::Boolean},
	{"display", PropertyType::String},
};
const PS ratingProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"origin", PropertyType::NormalizedPair},
	{"rotation", PropertyType::Float},
	{"rotationOrigin", PropertyType::NormalizedPair},
	{"filledPath", PropertyType::Path},
	{"unfilledPath", PropertyType::Path},
	{"zIndex", PropertyType::Float},
	{"disabled", PropertyType::Boolean},
};
const PS soundProps[] = {
	{"path", PropertyType::Path},
};
const PS helpsystemProps[] = {
	{"pos", PropertyType::NormalizedPair},
	{"size", PropertyType::NormalizedPair},
	{"textColor", PropertyType::Color},
	{"iconColor", PropertyType::Color},
	{"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float},
	{"iconUpDown", PropertyType::Path},
	{"iconLeftRight", PropertyType::Path},
	{"iconUpDownLeftRight", PropertyType::Path},
	{"iconA", PropertyType::Path},
	{"iconB", PropertyType::Path},
	{"iconX", PropertyType::Path},
	{"iconY", PropertyType::Path},
	{"iconL", PropertyType::Path},
	{"iconL2", PropertyType::Path},
	{"iconL2R2", PropertyType::Path},
	{"iconL3", PropertyType::Path},
	{"iconL3R3", PropertyType::Path},
	{"iconLR", PropertyType::Path},
	{"iconR", PropertyType::Path},
	{"iconR2", PropertyType::Path},
	{"iconR3", PropertyType::Path},
	{"iconStart", PropertyType::Path},
	{"iconSelect", PropertyType::Path},
	{"iconHotkey", PropertyType::Path},
	{"iconHkA", PropertyType::Path},
	{"iconHkB", PropertyType::Path},
	{"iconHkL", PropertyType::Path},
	{"iconHkR", PropertyType::Path},
	{"iconHkX", PropertyType::Path},
	{"iconHkY", PropertyType::Path},
	{"iconHkLeftRight", PropertyType::Path},
	{"iconJ1UpDownLeftRight", PropertyType::Path},
	{"iconJ1UpDown", PropertyType::Path},
	{"iconJ1LeftRight", PropertyType::Path},
	{"iconJ2UpDownLeftRight", PropertyType::Path},
	{"iconJ2UpDown", PropertyType::Path},
	{"iconJ2LeftRight", PropertyType::Path},
};
const PS carouselProps[] = {
	{"type", PropertyType::String},
	{"size", PropertyType::NormalizedPair},
	{"pos", PropertyType::NormalizedPair},
	{"origin", PropertyType::NormalizedPair},
	{"color", PropertyType::Color},
	{"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float},
	{"fontColor", PropertyType::Color},
	{"logoScale", PropertyType::Float},
	{"logoRotation", PropertyType::Float},
	{"logoRotationOrigin", PropertyType::NormalizedPair},
	{"logoSize", PropertyType::NormalizedPair},
	{"textOnly", PropertyType::Boolean},
	{"forceUppercase", PropertyType::Boolean},
	{"logoAlignment", PropertyType::String},
	{"maxLogoCount", PropertyType::Float},
	{"selectorColor", PropertyType::Color},
	{"selectedColor", PropertyType::Color},
	{"primaryColor", PropertyType::Color},
	{"secondaryColor", PropertyType::Color},
	{"selectorHeight", PropertyType::Float},
	{"selectorOffsetX", PropertyType::Float},
	{"selectorOffsetY", PropertyType::Float},
	{"textOffsetX", PropertyType::Float},
	{"lineSpacing", PropertyType::Float},
	{"horizontalMargin", PropertyType::Float},
	{"defaultTransition", PropertyType::String},
	{"zIndex", PropertyType::Float},
};
const PS menuBackgroundProps[] = {
	{"color", PropertyType::Color},
	{"path", PropertyType::Path},
	{"fadePath", PropertyType::Path},
};
const PS menuIconsProps[] = {
	{"iconKodi", PropertyType::Path},
	{"iconSystem", PropertyType::Path},
	{"iconUpdates", PropertyType::Path},
	{"iconControllers", PropertyType::Path},
	{"iconGames", PropertyType::Path},
	{"iconUI", PropertyType::Path},
	{"iconSound", PropertyType::Path},
	{"iconNetwork", PropertyType::Path},
	{"iconScraper", PropertyType::Path},
	{"iconAdvanced", PropertyType::Path},
	{"iconQuit", PropertyType::Path},
	{"iconRestart", PropertyType::Path},
	{"iconShutdown", PropertyType::Path},
	{"iconFastShutdown", PropertyType::Path},
	{"iconLicense", PropertyType::Path},
	{"iconList", PropertyType::Path},
	{"iconBios", PropertyType::Path},
	{"iconRecalboxRGBDual", PropertyType::Path},
	{"iconTate", PropertyType::Path},
	{"iconArcade", PropertyType::Path},
	{"iconDownload", PropertyType::Path},
	{"iconThemes", PropertyType::Path},
	{"iconCardReader", PropertyType::Path},
};
const PS menuSwitchProps[] = {
	{"pathOn", PropertyType::Path},
	{"pathOff", PropertyType::Path},
};
const PS menuSliderProps[] = {
	{"path", PropertyType::Path},
};
const PS menuButtonProps[] = {
	{"path", PropertyType::Path},
	{"filledPath", PropertyType::Path},
};
const PS menuTextProps[] = {
	{"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float},
	{"color", PropertyType::Color},
	{"separatorColor", PropertyType::Color},
	{"selectedColor", PropertyType::Color},
	{"selectorColor", PropertyType::Color},
};
const PS menuTextSmallProps[] = {
	{"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float},
	{"color", PropertyType::Color},
	{"selectedColor", PropertyType::Color},
	{"selectorColor", PropertyType::Color},
};
const PS menuSectionProps[] = {
	{"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float},
	{"color", PropertyType::Color},
	{"selectedColor", PropertyType::Color},
	{"alignment", PropertyType::String},
};
const PS menuSizeProps[] = {
	{"height", PropertyType::Float},
};
const PS keyboardProps[] = {
	{"keyColor", PropertyType::Color},
	{"keySelectedColor", PropertyType::Color},
	{"keyTextColor", PropertyType::Color},
	{"keyDisabledColor", PropertyType::Color},
	{"keyModifierColor", PropertyType::Color},
	{"keyModifierLockedColor", PropertyType::Color},
	{"keyTitleColor", PropertyType::Color},
	{"keyEditTextColor", PropertyType::Color},
	{"fontPath", PropertyType::Path},
};

const ElementSpec elementTable[] = {
	{"image", imageProps, true},
	{"box", boxProps, false},
	{"video", videoProps, false},
	{"text", textProps, true},
	{"scrolltext", scrolltextProps, false},
	{"markdown", markdownProps, false},
	{"textlist", textlistProps, false},
	{"container", containerProps, false},
	{"ninepatch", ninepatchProps, false},
	{"datetime", datetimeProps, false},
	{"rating", ratingProps, false},
	{"sound", soundProps, false},
	{"helpsystem", helpsystemProps, false},
	{"carousel", carouselProps, false},
	{"menuBackground", menuBackgroundProps, false},
	{"menuIcons", menuIconsProps, false},
	{"menuSwitch", menuSwitchProps, false},
	{"menuSlider", menuSliderProps, false},
	{"menuButton", menuButtonProps, false},
	{"menuText", menuTextProps, false},
	{"menuTextSmall", menuTextSmallProps, false},
	{"menuSection", menuSectionProps, false},
	{"menuSize", menuSizeProps, false},
	{"keyboard", keyboardProps, false},
};
}

std::span<const ElementSpec> allElements()
{
	return elementTable;
}

const ElementSpec* findElement(std::string_view type)
{
	for(const auto& e : elementTable)
		if(e.type == type) return &e;
	return nullptr;
}

const PropertySpec* findProperty(const ElementSpec& element, std::string_view name)
{
	for(const auto& p : element.properties)
		if(p.name == name) return &p;
	return nullptr;
}

std::expected<PropertyValue, std::string> parseProperty(
	PropertyType type, std::string_view raw, const std::filesystem::path& declaringFile)
{
	// format 3 read values with pugixml, which ignores the spaces and new lines around them
	if(type != PropertyType::String)
	{
		const auto first = raw.find_first_not_of(" \t\r\n");
		raw = first == std::string_view::npos ? std::string_view()
											  : raw.substr(first, raw.find_last_not_of(" \t\r\n") - first + 1);
	}
	switch(type)
	{
	case PropertyType::String:
		return std::string(raw);
	case PropertyType::Boolean:
	{
		std::string v(raw);
		for(char& c : v) c = (char)std::tolower((unsigned char)c);
		if(v == "true" || v == "1") return true;
		if(v == "false" || v == "0") return false;
		return std::unexpected(std::string("invalid boolean"));
	}
	case PropertyType::Float:
	{
		std::string v(raw);
		float val = 0;
		auto res = std::from_chars(v.data(), v.data() + v.size(), val);
		if(res.ec != std::errc() || res.ptr != v.data() + v.size())
			return std::unexpected(std::string("invalid float"));
		return val;
	}
	case PropertyType::Color:
	{
		std::string v(raw);
		if(v.size() == 6 || v.size() == 8)
		{
			unsigned int val = 0;
			for(char c : v)
			{
				val <<= 4;
				if(c >= '0' && c <= '9') val |= (c - '0');
				else if(c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
				else if(c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
				else return std::unexpected(std::string("invalid color"));
			}
			if(v.size() == 6) val = (val << 8) | 0xFF;
			return val;
		}
		return std::unexpected(std::string("invalid color"));
	}
	case PropertyType::NormalizedPair:
	{
		std::string v(raw);
		// split on whitespace
		size_t pos = v.find_first_of(" \t");
		if(pos == std::string::npos) return std::unexpected(std::string("invalid pair"));
		std::string xs = v.substr(0, pos);
		std::string ys = v.substr(v.find_first_not_of(" \t", pos));
		float x = 0, y = 0;
		auto rx = std::from_chars(xs.data(), xs.data() + xs.size(), x);
		auto ry = std::from_chars(ys.data(), ys.data() + ys.size(), y);
		if(rx.ec != std::errc() || ry.ec != std::errc()) return std::unexpected(std::string("invalid pair"));
		return glm::vec2(x, y);
	}
	case PropertyType::Path:
	{
		std::string v(raw);
		// trim
		// handle ./
		if(v.rfind("./", 0) == 0)
		{
			return (declaringFile.parent_path() / v.substr(2)).generic_string();
		}
		if(v.rfind("~", 0) == 0)
			return es::paths::expandUser(v).generic_string();
		return v;
	}
	}
	return std::unexpected(std::string("unknown type"));
}

} // namespace es::theme
