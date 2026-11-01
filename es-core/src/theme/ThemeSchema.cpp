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

// Format 3 elements and their properties (ThemeData::sElementMap legacy).
const PS imageProps[] = {
	{"pos", PropertyType::NormalizedPair}, {"size", PropertyType::NormalizedPair},
	{"maxSize", PropertyType::NormalizedPair}, {"origin", PropertyType::NormalizedPair},
	{"path", PropertyType::Path}, {"tile", PropertyType::Boolean},
	{"color", PropertyType::Color}, {"zIndex", PropertyType::Float},
	{"rotation", PropertyType::Float}, {"rotationOrigin", PropertyType::NormalizedPair},
};
const PS videoProps[] = {
	{"pos", PropertyType::NormalizedPair}, {"size", PropertyType::NormalizedPair},
	{"maxSize", PropertyType::NormalizedPair}, {"origin", PropertyType::NormalizedPair},
	{"default", PropertyType::Path}, {"delay", PropertyType::Float},
	{"showSnapshotNoVideo", PropertyType::Boolean}, {"showSnapshotDelay", PropertyType::Boolean},
};
const PS textProps[] = {
	{"pos", PropertyType::NormalizedPair}, {"size", PropertyType::NormalizedPair},
	{"text", PropertyType::String}, {"color", PropertyType::Color},
	{"fontPath", PropertyType::Path}, {"fontSize", PropertyType::Float},
	{"alignment", PropertyType::String}, {"forceUppercase", PropertyType::Boolean},
	{"lineSpacing", PropertyType::Float},
};
const PS textlistProps[] = {
	{"pos", PropertyType::NormalizedPair}, {"size", PropertyType::NormalizedPair},
	{"selectorColor", PropertyType::Color}, {"selectedColor", PropertyType::Color},
	{"primaryColor", PropertyType::Color}, {"secondaryColor", PropertyType::Color},
	{"fontPath", PropertyType::Path}, {"fontSize", PropertyType::Float},
	{"scrollSound", PropertyType::Path}, {"alignment", PropertyType::String},
	{"horizontalMargin", PropertyType::Float}, {"forceUppercase", PropertyType::Boolean},
	{"lineSpacing", PropertyType::Float},
};
const PS containerProps[] = {
	{"pos", PropertyType::NormalizedPair}, {"size", PropertyType::NormalizedPair},
};
const PS ninepatchProps[] = {
	{"pos", PropertyType::NormalizedPair}, {"size", PropertyType::NormalizedPair},
	{"path", PropertyType::Path},
};
const PS datetimeProps[] = {
	{"pos", PropertyType::NormalizedPair}, {"size", PropertyType::NormalizedPair},
	{"color", PropertyType::Color}, {"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float}, {"forceUppercase", PropertyType::Boolean},
};
const PS ratingProps[] = {
	{"pos", PropertyType::NormalizedPair}, {"size", PropertyType::NormalizedPair},
	{"filledPath", PropertyType::Path}, {"unfilledPath", PropertyType::Path},
};
const PS soundProps[] = {{"path", PropertyType::Path}};
const PS helpsystemProps[] = {
	{"pos", PropertyType::NormalizedPair}, {"textColor", PropertyType::Color},
	{"iconColor", PropertyType::Color}, {"fontPath", PropertyType::Path},
	{"fontSize", PropertyType::Float},
};
const PS genericProps[] = {{"pos", PropertyType::NormalizedPair}, {"size", PropertyType::NormalizedPair}};

const ElementSpec elementTable[] = {
	{"image", imageProps, true},
	{"video", videoProps, false},
	{"text", textProps, true},
	{"textlist", textlistProps, false},
	{"container", containerProps, false},
	{"ninepatch", ninepatchProps, false},
	{"datetime", datetimeProps, false},
	{"rating", ratingProps, false},
	{"sound", soundProps, false},
	{"helpsystem", helpsystemProps, false},
	{"carousel", genericProps, false},
	{"box", genericProps, false},
	{"scrollText", textProps, false},
	{"markdown", textProps, false},
	{"logoList", genericProps, false},
	{"carouselList", genericProps, false},
	{"gridList", genericProps, false},
	{"menuBackground", genericProps, false},
	{"menuIcons", genericProps, false},
	{"menuSwitch", genericProps, false},
	{"menuSlider", genericProps, false},
	{"menuButton", genericProps, false},
	{"menuText", genericProps, false},
	{"menuTextSmall", genericProps, false},
	{"menuSection", genericProps, false},
	{"menuSize", genericProps, false},
	{"keyboard", genericProps, false},
	{"polymorphic", genericProps, false},
};
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
	case PropertyType::PathList:
	{
		PathList pl;
		pl.paths.push_back(std::string(raw));
		return pl;
	}
	}
	return std::unexpected(std::string("unknown type"));
}

} // namespace es::theme
