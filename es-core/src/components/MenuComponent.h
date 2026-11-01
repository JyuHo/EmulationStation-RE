#pragma once

#include "components/NinePatchComponent.h"
#include <cassert>
#include "components/ComponentList.h"
#include "components/TextComponent.h"
#include "components/ComponentGrid.h"
#include "MenuTheme.h"
#include "Util.h"

#include <optional>

class ButtonComponent;
class ImageComponent;

std::shared_ptr<ComponentGrid> makeButtonGrid(
	Window* window, const std::vector<std::shared_ptr<ButtonComponent>>& buttons);
std::shared_ptr<ImageComponent> makeArrow(Window* window);

// Entry icon of a menu row (Recalbox Menu.cpp:38,309-337 @10.1.1): a square of side 0.8 x the
// row height, centered vertically, at `x` from the left edge of the row; the label moves right
// by `textOffset`.
struct MenuIconGeometry
{
	float side;
	float x;
	float textOffset;
};
MenuIconGeometry menuIconGeometry(float rowHeight, float screenWidth);

// Path of the theme image for this entry icon; empty when there is no icon or the theme gives none
std::string menuIconPath(const es::theme::MenuTheme& theme, std::optional<es::theme::MenuIcon> icon);

// Adds the label of a menu row, after the icon when there is one: icon, gap of `gap` pixels,
// label. Without an icon the row gets the label alone, as before 11f4.
void addIconAndLabel(ComponentListRow& row, Window* window, const std::shared_ptr<GuiComponent>& label,
	const std::shared_ptr<GuiComponent>& icon, float gap);

// Label row of a menu entry: label in `font` and `color`, preceded by the theme icon of `icon`
void addMenuEntryLabel(ComponentListRow& row, Window* window, const std::string& label,
	const std::shared_ptr<Font>& font, unsigned int color, std::optional<es::theme::MenuIcon> icon);

// Text of a section header (ItemHeader.h:146-153): capitals, open mark after the label when
// aligned left, before it otherwise
std::string sectionHeaderText(const std::string& label, Alignment alignment);

// Lines of a section header in a row of `width` (ItemHeader.h:20-22,99-121): up to two
// segments {x, width}, `lineHeight` pixels high in the middle of the row; the text lies
// `textMargin` from the edge it is aligned to.
struct SectionHeaderLayout
{
	glm::vec2 lines[2];
	int lineCount;
	int lineHeight;
	float textMargin;
};
SectionHeaderLayout sectionHeaderLayout(
	float width, float labelWidth, Alignment alignment, float screenWidth, float screenHeight);

#define TITLE_VERT_PADDING (Renderer::getScreenHeight() * 0.0637f)

class MenuComponent : public GuiComponent
{
public:
	MenuComponent(
		Window* window, const std::string& title, const std::shared_ptr<Font>& titleFont = Font::get(FONT_SIZE_LARGE));

	void onSizeChanged() override;

	inline void addRow(const ComponentListRow& row, bool setCursorHere = false)
	{
		mList->addRow(row, setCursorHere);
		updateSize();
	}

	inline void addWithLabel(const std::string& label, const std::shared_ptr<GuiComponent>& comp,
		bool setCursorHere = false, bool invert_when_selected = true)
	{
		ComponentListRow row;
		row.addElement(
			std::make_shared<TextComponent>(mWindow, strToUpper(label), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
		row.addElement(comp, false, invert_when_selected);
		addRow(row, setCursorHere);
	}

	// Section header row (11f4): selectable, no action
	void addSection(const std::string& label);

	void addButton(const std::string& label, const std::string& helpText, const std::function<void()>& callback);

	void setTitle(const std::string& title, const std::shared_ptr<Font>& font);

	inline void setCursorToList() { mGrid.setCursorTo(mList); }
	inline void setCursorToButtons()
	{
		assert(mButtonGrid);
		mGrid.setCursorTo(mButtonGrid);
	}

	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void updateSize();
	void updateGrid();
	float getButtonGridHeight() const;

	NinePatchComponent mBackground;
	ComponentGrid mGrid;

	std::shared_ptr<TextComponent> mTitle;
	std::shared_ptr<ComponentList> mList;
	std::shared_ptr<ComponentGrid> mButtonGrid;
	std::vector<std::shared_ptr<ButtonComponent>> mButtons;
};
