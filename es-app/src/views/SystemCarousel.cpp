#include "views/SystemCarousel.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace es::carousel {

Carousel defaults(glm::vec2 screen)
{
	Carousel c;
	c.size = {screen.x, 0.2325f * screen.y};
	c.pos = {0.0f, 0.5f * (screen.y - c.size.y)};
	c.logoSize = {0.25f * std::max(screen.x, screen.y), 0.155f * std::min(screen.x, screen.y)};
	return c;
}

Type parseType(const std::string& value)
{
	if(value == "vertical")
		return Type::Vertical;
	if(value == "vertical_wheel")
		return Type::VerticalWheel;
	return Type::Horizontal;
}

void parseAlignment(const std::string& value, HAlign& h, VAlign& v)
{
	struct A
	{
		HAlign h;
		VAlign v;
	};
	static const std::map<std::string, A, std::less<>> kNames = {
		{"left", {HAlign::Left, VAlign::Center}},
		{"right", {HAlign::Right, VAlign::Center}},
		{"top", {HAlign::Center, VAlign::Top}},
		{"bottom", {HAlign::Center, VAlign::Bottom}},
		{"topleft", {HAlign::Left, VAlign::Top}},
		{"topcenter", {HAlign::Center, VAlign::Top}},
		{"topright", {HAlign::Right, VAlign::Top}},
		{"centerleft", {HAlign::Left, VAlign::Center}},
		{"center", {HAlign::Center, VAlign::Center}},
		{"centerright", {HAlign::Right, VAlign::Center}},
		{"bottomleft", {HAlign::Left, VAlign::Bottom}},
		{"bottomcenter", {HAlign::Center, VAlign::Bottom}},
		{"bottomright", {HAlign::Right, VAlign::Bottom}},
	};
	std::string lower = value;
	std::ranges::transform(lower, lower.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
	const auto it = kNames.find(lower);
	const A a = it != kNames.end() ? it->second : A{HAlign::Left, VAlign::Center};
	h = a.h;
	v = a.v;
}

Carousel fromElement(const ThemeData::ThemeElement* elem, glm::vec2 screen)
{
	Carousel c = defaults(screen);
	if(!elem)
		return c;
	auto vec = [&](const char* name, glm::vec2& out, glm::vec2 factor) {
		if(elem->has(name))
			out = elem->get<glm::vec2>(name) * factor;
	};
	auto color = [&](const char* name, unsigned int& out) {
		if(elem->has(name))
			out = elem->get<unsigned int>(name);
	};
	auto real = [&](const char* name, float& out, float factor) {
		if(elem->has(name))
			out = elem->get<float>(name) * factor;
	};
	auto flag = [&](const char* name, bool& out) {
		if(elem->has(name))
			out = elem->get<bool>(name);
	};

	if(elem->has("type"))
		c.type = parseType(elem->get<std::string>("type"));
	vec("size", c.size, screen);
	vec("pos", c.pos, screen);
	vec("origin", c.origin, glm::vec2(1.0f));
	color("color", c.color);
	real("logoScale", c.logoScale, 1.0f);
	vec("logoSize", c.logoSize, screen);
	flag("textOnly", c.textOnly);
	flag("forceUppercase", c.forceUppercase);
	color("selectorColor", c.selectorColor);
	color("selectedColor", c.selectedColor);
	color("primaryColor", c.primaryColor);
	color("secondaryColor", c.secondaryColor);
	real("selectorHeight", c.selectorHeight, screen.y);
	real("selectorOffsetX", c.selectorOffsetX, screen.x);
	real("selectorOffsetY", c.selectorOffsetY, screen.y);
	real("textOffsetX", c.textOffsetX, screen.x);
	real("lineSpacing", c.lineSpacing, 1.0f);
	real("horizontalMargin", c.horizontalMargin, screen.x);
	if(elem->has("maxLogoCount"))
		c.maxLogoCount = (int)std::lround(elem->get<float>("maxLogoCount"));
	real("zIndex", c.zIndex, 1.0f);
	real("logoRotation", c.logoRotation, 1.0f);
	vec("logoRotationOrigin", c.logoRotationOrigin, glm::vec2(1.0f));
	if(elem->has("logoAlignment"))
		parseAlignment(elem->get<std::string>("logoAlignment"), c.hAlign, c.vAlign);
	if(elem->has("fontPath"))
		c.fontPath = elem->get<std::string>("fontPath");
	if(elem->has("fontSize"))
		c.fontSize = elem->get<float>("fontSize");
	color("fontColor", c.fontColor);
	if(elem->has("defaultTransition"))
		c.defaultTransition = elem->get<std::string>("defaultTransition");
	return c;
}

glm::vec2 slotPosition(const Carousel& c, float camOffset, int i)
{
	glm::vec2 spacing(0.0f); // includes the logo itself
	glm::vec2 off(0.0f);
	const int count = std::max(1, c.maxLogoCount);
	auto across = [&](float extent, float logo, bool start, bool end) {
		if(start)
			return logo / 10;
		if(end)
			return extent - logo * 1.1f;
		return (extent - logo) / 2;
	};
	switch(c.type)
	{
		case Type::Horizontal:
			spacing.x = (c.size.x - c.logoSize.x * (float)count) / (float)count + c.logoSize.x;
			off.x = (c.size.x - c.logoSize.x) / 2 - camOffset * spacing.x;
			off.y = across(c.size.y, c.logoSize.y, c.vAlign == VAlign::Top, c.vAlign == VAlign::Bottom);
			break;
		case Type::Vertical:
			spacing.y = (c.size.y - c.logoSize.y * (float)count) / (float)count + c.logoSize.y;
			[[fallthrough]];
		case Type::VerticalWheel: // every logo on the same slot, turned around logoRotationOrigin
			off.y = (c.size.y - c.logoSize.y) / 2 - camOffset * spacing.y;
			off.x = across(c.size.x, c.logoSize.x, c.hAlign == HAlign::Left, c.hAlign == HAlign::Right);
			break;
	}
	return glm::vec2((float)i) * spacing + off;
}

glm::vec2 logoOrigin(const Carousel& c)
{
	if(c.type == Type::Horizontal)
		return {0.5f, c.vAlign == VAlign::Top ? 0.0f : c.vAlign == VAlign::Bottom ? 1.0f : 0.5f};
	return {c.hAlign == HAlign::Left ? 0.0f : c.hAlign == HAlign::Right ? 1.0f : 0.5f, 0.5f};
}

float logoScaleFactor(const Carousel& c, float distance)
{
	float scale = 1.0f + (c.logoScale - 1.0f) * (1.0f - std::fabs(distance));
	scale = std::min(c.logoScale, std::max(1.0f, scale));
	return c.logoScale != 0.0f ? scale / c.logoScale : 1.0f;
}

unsigned char logoOpacity(float distance)
{
	const int opacity = (int)std::lround(0x80 + (0xFF - 0x80) * (1.0f - std::fabs(distance)));
	return (unsigned char)std::clamp(opacity, 0x80, 0xFF);
}

float logoRotationAt(const Carousel& c, float distance)
{
	return c.type == Type::VerticalWheel ? c.logoRotation * distance : 0.0f;
}

glm::ivec2 visibleRange(const Carousel& c, float camOffset, int entryCount)
{
	const int center = (int)camOffset;
	const int logoCount = std::max(1, std::min(c.maxLogoCount, entryCount));
	const int buffer = logoCount == 1 ? 0 : 1;
	return {center - logoCount / 2 - buffer, center + logoCount / 2 + buffer};
}

int wrapIndex(int index, int count)
{
	if(count <= 0)
		return 0;
	index %= count;
	return index < 0 ? index + count : index;
}

TextListLayout textListLayout(const Carousel& c, float fontHeight, int cursor, int entryCount)
{
	TextListLayout l;
	l.rowHeight = fontHeight * c.lineSpacing;
	if(l.rowHeight <= 0.0f)
		return l;
	const float screenCount = c.size.y / l.rowHeight;
	float start = std::max(0.0f, (float)cursor - screenCount / 2.0f);
	if((float)entryCount > screenCount)
		start = std::min(start, (float)entryCount - screenCount);
	else
		start = 0.0f;
	l.startOffset = start;
	l.firstRow = (int)start;
	l.rowCount = (int)screenCount + 3; // rows firstRow .. firstRow + screenCount + 2
	l.selectorHeight = c.selectorHeight > 0.0f ? c.selectorHeight : l.rowHeight;
	l.selectorY = ((float)cursor - start) * l.rowHeight + c.selectorOffsetY;
	return l;
}

unsigned int textListColor(const Carousel& c, int index, int cursor)
{
	if(index == cursor)
		return c.selectedColor;
	return (index & 1) != 0 ? c.secondaryColor : c.primaryColor;
}

unsigned int textListRowTint(int index)
{
	return (index & 1) != 0 ? 0xFFFFFF08 : 0x00000008;
}

namespace {

const ThemeData::ThemeElement* carouselElement(const ThemeData* mainTheme)
{
	return mainTheme ? mainTheme->getElement("system", "systemcarousel", "carousel") : nullptr;
}

} // namespace

std::string transitionStyle(const ThemeData* mainTheme, const std::string& setting)
{
	const ThemeData::ThemeElement* elem = carouselElement(mainTheme);
	if(elem && elem->has("defaultTransition"))
	{
		const std::string value = elem->get<std::string>("defaultTransition");
		if(value == "fade" || value == "instant")
			return value;
		return "slide";
	}
	return setting;
}

bool isThemed(const ThemeData* mainTheme)
{
	return carouselElement(mainTheme) != nullptr ||
		(mainTheme && mainTheme->getElement("system", "systemInfo", "text") != nullptr);
}

} // namespace es::carousel
