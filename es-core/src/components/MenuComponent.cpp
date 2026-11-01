#include "components/MenuComponent.h"
#include "components/ButtonComponent.h"
#include "components/ImageComponent.h"
#include "components/MenuSectionComponent.h"
#include "Window.h"

#define BUTTON_GRID_VERT_PADDING 32
#define BUTTON_GRID_HORIZ_PADDING 10

#define TITLE_HEIGHT (mTitle->getFont()->getLetterHeight() + TITLE_VERT_PADDING)

MenuComponent::MenuComponent(Window* window, const std::string& title, const std::shared_ptr<Font>& titleFont) :
	GuiComponent(window), mBackground(window), mGrid(window, glm::ivec2(1, 3))
{
	addChild(&mBackground);
	addChild(&mGrid);

	mBackground.setImagePath(":/frame.png");
	const auto& mt = mWindow->getMenuTheme();
	if(mt.backgroundPath) mBackground.setImagePath(*mt.backgroundPath);
	if(mt.backgroundColor) mBackground.setCenterColor(*mt.backgroundColor);

	// set up title
	mTitle = std::make_shared<TextComponent>(mWindow);
	mTitle->setAlignment(ALIGN_CENTER);
	mTitle->setColor(mt.title.color.value_or(0x555555FF));
	if(mt.title.font) { /* font from theme not wired here */ }
	setTitle(title, titleFont);
	mGrid.setEntry(mTitle, glm::ivec2(0, 0), false);

	// set up list which will never change (externally, anyway)
	mList = std::make_shared<ComponentList>(mWindow);
	mGrid.setEntry(mList, glm::ivec2(0, 1), true);

	updateGrid();
	updateSize();

	mGrid.resetCursor();
}

void MenuComponent::setTitle(const std::string& title, const std::shared_ptr<Font>& font)
{
	mTitle->setText(strToUpper(title));
	mTitle->setFont(font);
}

float MenuComponent::getButtonGridHeight() const
{
	return (
		mButtonGrid ? mButtonGrid->getSize().y : Font::get(FONT_SIZE_MEDIUM)->getHeight() + BUTTON_GRID_VERT_PADDING);
}

void MenuComponent::updateSize()
{
	const auto& mt = mWindow->getMenuTheme();
	const float maxHeight = Renderer::getScreenHeight() * (mt.height.value_or(0.7f));
	float height = TITLE_HEIGHT + mList->getTotalRowHeight() + getButtonGridHeight() + 2;
	if(height > maxHeight)
	{
		height = TITLE_HEIGHT + getButtonGridHeight();
		int i = 0;
		while(i < mList->size())
		{
			float rowHeight = mList->getRowHeight(i);
			if(height + rowHeight < maxHeight)
				height += rowHeight;
			else
				break;
			i++;
		}
	}

	setSize(Renderer::getScreenWidth() * 0.5f, height);
}

void MenuComponent::onSizeChanged()
{
	mBackground.fitTo(mSize, glm::vec3(0.0f), glm::vec2(-32, -32));

	// update grid row/col sizes
	mGrid.setRowHeightPerc(0, TITLE_HEIGHT / mSize.y);
	mGrid.setRowHeightPerc(2, getButtonGridHeight() / mSize.y);

	mGrid.setSize(mSize);
}

void MenuComponent::addSection(const std::string& label)
{
	ComponentListRow row;
	row.addElement(std::make_shared<MenuSectionComponent>(mWindow, label), true,
		!MenuSectionComponent::drawnAfterSelector(mWindow));
	// selectable, no action (ItemHeader.h:58-69: folding is commented out)
	row.input_handler = [](InputConfig*, Input) { return false; };
	addRow(row);
}

void MenuComponent::addButton(
	const std::string& name, const std::string& helpText, const std::function<void()>& callback)
{
	mButtons.push_back(std::make_shared<ButtonComponent>(mWindow, strToUpper(name), helpText, callback));
	updateGrid();
	updateSize();
}

void MenuComponent::updateGrid()
{
	if(mButtonGrid)
		mGrid.removeEntry(mButtonGrid);

	mButtonGrid.reset();

	if(mButtons.size())
	{
		mButtonGrid = makeButtonGrid(mWindow, mButtons);
		mGrid.setEntry(mButtonGrid, glm::ivec2(0, 2), true, false);
	}
}

std::vector<HelpPrompt> MenuComponent::getHelpPrompts()
{
	return mGrid.getHelpPrompts();
}

std::shared_ptr<ComponentGrid> makeButtonGrid(
	Window* window, const std::vector<std::shared_ptr<ButtonComponent>>& buttons)
{
	std::shared_ptr<ComponentGrid> buttonGrid = std::make_shared<ComponentGrid>(window, glm::ivec2(buttons.size(), 2));

	float buttonGridWidth = (float)BUTTON_GRID_HORIZ_PADDING * buttons.size(); // initialize to padding
	for(int i = 0; i < (int)buttons.size(); i++)
	{
		buttonGrid->setEntry(buttons.at(i), glm::ivec2(i, 0), true, false);
		buttonGridWidth += buttons.at(i)->getSize().x;
	}
	for(unsigned int i = 0; i < buttons.size(); i++)
	{
		buttonGrid->setColWidthPerc(i, (buttons.at(i)->getSize().x + BUTTON_GRID_HORIZ_PADDING) / buttonGridWidth);
	}

	buttonGrid->setSize(buttonGridWidth, buttons.at(0)->getSize().y + BUTTON_GRID_VERT_PADDING + 2);
	buttonGrid->setRowHeightPerc(
		1, 2 / buttonGrid->getSize().y); // spacer row to deal with dropshadow to make buttons look centered

	return buttonGrid;
}

std::shared_ptr<ImageComponent> makeArrow(Window* window)
{
	auto bracket = std::make_shared<ImageComponent>(window);
	bracket->setImage(":/arrow.svg");
	bracket->setResize(0, round(Font::get(FONT_SIZE_MEDIUM)->getLetterHeight()));
	return bracket;
}

MenuIconGeometry menuIconGeometry(float rowHeight, float screenWidth)
{
	// margin 3 + width / 320 (Menu.cpp:38); side (int)(height x 0.8) (:331); label after the
	// side and the margin (:336)
	const float margin = 3.0f + (float)((int)screenWidth / 320);
	const float side = (float)(int)(rowHeight * 0.8f);
	return {side, margin, side + margin};
}

std::string menuIconPath(const es::theme::MenuTheme& theme, std::optional<es::theme::MenuIcon> icon)
{
	if(!icon)
		return {};
	const auto it = theme.icons.find(*icon);
	return it == theme.icons.end() ? std::string() : it->second;
}

void addIconAndLabel(ComponentListRow& row, Window* window, const std::shared_ptr<GuiComponent>& label,
	const std::shared_ptr<GuiComponent>& icon, float gap)
{
	if(icon)
	{
		row.addElement(icon, false);
		auto spacer = std::make_shared<GuiComponent>(window);
		spacer->setSize(gap, 0);
		row.addElement(spacer, false);
	}
	row.addElement(label, true);
}

void addMenuEntryLabel(ComponentListRow& row, Window* window, const std::string& label,
	const std::shared_ptr<Font>& font, unsigned int color, std::optional<es::theme::MenuIcon> icon)
{
	auto text = std::make_shared<TextComponent>(window, label, font, color);
	const es::theme::MenuTheme& mt = window->getMenuTheme();
	const std::string path = menuIconPath(mt, icon);
	if(path.empty())
	{
		addIconAndLabel(row, window, text, nullptr, 0);
		return;
	}
	const MenuIconGeometry g = menuIconGeometry(text->getSize().y, (float)Renderer::getScreenWidth());
	auto image = std::make_shared<ImageComponent>(window);
	image->setImage(path);
	image->setResize(g.side, g.side);
	// ComponentList puts the first element at its left padding: the origin moves the square to
	// g.x from the row edge
	image->setOrigin((ComponentList::kHorizontalPadding / 2 - g.x) / g.side, 0.0f);
	// tint: text color, selected color on the selected row (Menu.cpp:325)
	image->setColor(mt.text.color.value_or(0x777777FF));
	addIconAndLabel(row, window, text, image, g.textOffset - g.side);
}

std::string sectionHeaderText(const std::string& label, Alignment alignment)
{
	const std::string upper = strToUpper(label);
	return alignment == ALIGN_LEFT ? upper + "\xE2\x96\xBC " : "\xE2\x96\xBC " + upper;
}

SectionHeaderLayout sectionHeaderLayout(
	float width, float labelWidth, Alignment alignment, float screenWidth, float screenHeight)
{
	SectionHeaderLayout l{};
	const int w = (int)width;
	const int label = (int)labelWidth;
	const int gap = (int)screenWidth / 160;  // ItemHeader.h:20
	l.lineHeight = screenHeight <= 480 ? 1 : 2; // :22, :99
	l.textMargin = (float)(gap + 2);
	switch(alignment)
	{
		case ALIGN_LEFT: // :104-106
			l.lines[0] = {label + gap * 2 + 2, w - (label + gap * 2 + 2)};
			l.lineCount = 1;
			break;
		case ALIGN_RIGHT: // :110-112
			l.lines[0] = {2, w - (label + gap * 2 + 2)};
			l.lineCount = 1;
			break;
		case ALIGN_CENTER: // :116-119
		{
			const int length = (w - label) / 2 - (gap + 2);
			l.lines[0] = {2, length};
			l.lines[1] = {(w + label) / 2 + gap, length};
			l.lineCount = 2;
			break;
		}
	}
	return l;
}
