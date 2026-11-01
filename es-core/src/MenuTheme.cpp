#include "MenuTheme.h"

#include "ThemeData.h"
#include "resources/Font.h"

namespace es::theme {

namespace {
const ThemeData::ThemeElement* elem(const ThemeData& t, const std::string& name, const std::string& type)
{
	return t.getElement("menu", name, type);
}

std::optional<MenuFont> readFont(const ThemeData::ThemeElement* e, float defaultSize)
{
	if(!e) return std::nullopt;
	bool has = e->has("fontPath") || e->has("fontSize");
	if(!has) return std::nullopt;
	MenuFont f;
	if(e->has("fontPath")) f.path = e->get<std::string>("fontPath"); else f.path = FONT_PATH_REGULAR;
	if(e->has("fontSize")) f.size = e->get<float>("fontSize"); else f.size = defaultSize;
	return f;
}

} // namespace

MenuTheme MenuTheme::fromTheme(const ThemeData& theme)
{
	MenuTheme mt;

	// menuSize
	if(const auto* e = elem(theme, "menuSize", "menuSize"))
		if(e->has("height")) mt.height = e->get<float>("height");

	// menubg
	if(const auto* e = elem(theme, "menubg", "menuBackground"))
	{
		if(e->has("path")) mt.backgroundPath = e->get<std::string>("path");
		if(e->has("fadePath")) mt.fadePath = e->get<std::string>("fadePath");
		if(e->has("color")) mt.backgroundColor = e->get<unsigned int>("color");
	}

	// menutitle
	if(const auto* e = elem(theme, "menutitle", "menuText"))
	{
		mt.title.font = readFont(e, 0.085f);
		if(e->has("color")) mt.title.color = e->get<unsigned int>("color");
	}

	// menufooter
	if(const auto* e = elem(theme, "menufooter", "menuText"))
	{
		mt.footer.font = readFont(e, 0.035f);
		if(e->has("color")) mt.footer.color = e->get<unsigned int>("color");
	}

	// menutext
	const ThemeData::ThemeElement* textElem = elem(theme, "menutext", "menuText");
	if(textElem)
	{
		mt.text.font = readFont(textElem, 0.045f);
		if(textElem->has("color")) mt.text.color = textElem->get<unsigned int>("color");
		if(textElem->has("selectedColor")) mt.text.selectedColor = textElem->get<unsigned int>("selectedColor");
		if(textElem->has("selectorColor")) mt.text.selectorColor = textElem->get<unsigned int>("selectorColor");
		if(textElem->has("separatorColor")) mt.text.separatorColor = textElem->get<unsigned int>("separatorColor");
	}

	// menusection: a copy of menutext taken right after menutext is read (MenuThemeData.cpp:49),
	// so before the menutextsmall particularity below
	mt.section = mt.text;
	if(const auto* e = elem(theme, "menusection", "menuSection"))
	{
		// a missing font property comes from the menutext font (:53)
		if(e->has("fontPath") || e->has("fontSize"))
		{
			MenuFont f = mt.text.font.value_or(MenuFont{FONT_PATH_REGULAR, 0.045f});
			if(e->has("fontPath")) f.path = e->get<std::string>("fontPath");
			if(e->has("fontSize")) f.size = e->get<float>("fontSize");
			mt.section.font = f;
		}
		if(e->has("color")) mt.section.color = e->get<unsigned int>("color");
		if(e->has("selectedColor")) mt.section.selectedColor = e->get<unsigned int>("selectedColor");
		if(e->has("alignment"))
		{
			// :56-62: "left", "right", anything else is centered
			const std::string a = e->get<std::string>("alignment");
			mt.sectionAlignment = a == "left" ? ALIGN_LEFT : a == "right" ? ALIGN_RIGHT : ALIGN_CENTER;
		}
	}

	// menutextsmall: inherits all of menutext (:65), then overrides
	mt.textSmall = mt.text;
	if(const auto* e = elem(theme, "menutextsmall", "menuTextSmall"))
	{
		if(e->has("fontPath") || e->has("fontSize"))
			mt.textSmall.font = readFont(e, 0.035f);
		if(e->has("color")) mt.textSmall.color = e->get<unsigned int>("color");
		// particularity of Recalbox's code (:72-73): both write the selected color of menutext
		if(e->has("selectedColor")) mt.text.selectedColor = e->get<unsigned int>("selectedColor");
		if(e->has("selectorColor")) mt.text.selectedColor = e->get<unsigned int>("selectorColor");
	}

	// menubutton
	if(const auto* e = elem(theme, "menubutton", "menuButton"))
	{
		if(e->has("path")) mt.buttonPath = e->get<std::string>("path");
		if(e->has("filledPath")) mt.buttonFilledPath = e->get<std::string>("filledPath");
	}

	// menuswitch
	if(const auto* e = elem(theme, "menuswitch", "menuSwitch"))
	{
		if(e->has("pathOn")) mt.switchOnPath = e->get<std::string>("pathOn");
		if(e->has("pathOff")) mt.switchOffPath = e->get<std::string>("pathOff");
	}

	// menuslider
	if(const auto* e = elem(theme, "menuslider", "menuSlider"))
	{
		if(e->has("path")) mt.sliderKnobPath = e->get<std::string>("path");
	}


	// menuicons: property "iconXxx" -> MenuIcon::Xxx
	if(const auto* e = elem(theme, "menuicons", "menuIcons"))
	{
		auto setIcon = [&](const char* prop, MenuIcon icon) {
			if(e->has(prop)) mt.icons[icon] = e->get<std::string>(prop);
		};
		setIcon("iconKodi", MenuIcon::Kodi);
		setIcon("iconSystem", MenuIcon::System);
		setIcon("iconUpdates", MenuIcon::Updates);
		setIcon("iconControllers", MenuIcon::Controllers);
		setIcon("iconGames", MenuIcon::Games);
		setIcon("iconUI", MenuIcon::UI);
		setIcon("iconSound", MenuIcon::Sound);
		setIcon("iconNetwork", MenuIcon::Network);
		setIcon("iconScraper", MenuIcon::Scraper);
		setIcon("iconAdvanced", MenuIcon::Advanced);
		setIcon("iconQuit", MenuIcon::Quit);
		setIcon("iconRestart", MenuIcon::Restart);
		setIcon("iconShutdown", MenuIcon::Shutdown);
		setIcon("iconFastShutdown", MenuIcon::FastShutdown);
		setIcon("iconLicense", MenuIcon::License);
		setIcon("iconBios", MenuIcon::Bios);
		setIcon("iconRecalboxRGBDual", MenuIcon::RecalboxRGBDual);
		setIcon("iconTate", MenuIcon::Tate);
		setIcon("iconArcade", MenuIcon::Arcade);
		setIcon("iconDownload", MenuIcon::Download);
		setIcon("iconThemes", MenuIcon::Themes);
		setIcon("iconCardReader", MenuIcon::CardReader);
		setIcon("iconList", MenuIcon::Lists); // the property is "iconList" (:108)
	}

	return mt;
}

KeyboardTheme KeyboardTheme::fromTheme(const ThemeData& theme, const std::string& view)
{
	KeyboardTheme kt;
	const ThemeData::ThemeElement* e = theme.getElement(view, "keyboard", "keyboard");
	if(!e) return kt;
	if(e->has("keyColor")) kt.keyColor = e->get<unsigned int>("keyColor");
	if(e->has("keySelectedColor")) kt.keySelectedColor = e->get<unsigned int>("keySelectedColor");
	if(e->has("keyTextColor")) kt.keyTextColor = e->get<unsigned int>("keyTextColor");
	if(e->has("keyDisabledColor")) kt.keyDisabledColor = e->get<unsigned int>("keyDisabledColor");
	if(e->has("keyModifierColor")) kt.keyModifierColor = e->get<unsigned int>("keyModifierColor");
	if(e->has("keyModifierLockedColor")) kt.keyModifierLockedColor = e->get<unsigned int>("keyModifierLockedColor");
	if(e->has("keyTitleColor")) kt.keyTitleColor = e->get<unsigned int>("keyTitleColor");
	if(e->has("keyEditTextColor")) kt.keyEditTextColor = e->get<unsigned int>("keyEditTextColor");
	if(e->has("fontPath")) kt.fontPath = e->get<std::string>("fontPath");
	return kt;
}

} // namespace es::theme
