// Menu theme applied to the components (phase 11f2; docs/THEMES.md section 10, "Vue menu").
#include "MenuTheme.h"
#include "Window.h"
#include "components/ImageComponent.h"

#include <catch2/catch_test_macros.hpp>

using es::theme::MenuTheme;

TEST_CASE("setColor and getColor are virtual: image tint", "[MenuTheme][11f2]")
{
	// ComponentList recolors the selected row through GuiComponent*. TextComponent needs a
	// renderer (default font): covered by the probe of the selected label in capture-themes.sh.
	Window window;
	ImageComponent image(&window);
	GuiComponent* asImage = &image;
	CHECK(asImage->getColor() == 0xFFFFFFFF); // no tint
	asImage->setColor(0xFF0000FF);
	CHECK(image.getColor() == 0xFF0000FF);

	// a plain component has no color
	GuiComponent plain(&window);
	plain.setColor(0x123456FF);
	CHECK(plain.getColor() == 0);
}

TEST_CASE("themed selector bar: the 8 combinations of its three values", "[MenuTheme][11f2]")
{
	for(int mask = 0; mask < 8; mask++)
	{
		MenuTheme mt;
		if(mask & 1) mt.text.selectorColor = 0x0000FFFF;
		if(mask & 2) mt.text.selectedColor = 0xFF0000FF;
		if(mask & 4) mt.backgroundColor = 0x00FF00FF;
		mt.text.color = 0x123456FF;		 // not one of the three
		mt.text.separatorColor = 0xFFFF00FF; // neither
		CAPTURE(mask);
		CHECK(mt.themedSelector() == (mask != 0));
	}
}

TEST_CASE("menu font: the caller's font when the theme gives none", "[MenuTheme][11f2]")
{
	const std::shared_ptr<Font> none;
	CHECK(es::theme::menuFontOr(std::nullopt, none) == none);
}
