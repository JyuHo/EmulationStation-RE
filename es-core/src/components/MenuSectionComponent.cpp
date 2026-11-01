#include "components/MenuSectionComponent.h"
#include "components/MenuComponent.h"
#include "components/TextComponent.h"
#include "renderer/Renderer.h"
#include "Window.h"

MenuSectionComponent::MenuSectionComponent(Window* window, const std::string& label) : GuiComponent(window)
{
	const es::theme::MenuTheme& mt = mWindow->getMenuTheme();
	// absent values: those of menutext, copied by MenuTheme (MenuThemeData.cpp:49), else ES's
	mFont = mt.section.font
		? Font::get((int)(mt.section.font->size * Renderer::getScreenHeight()), mt.section.font->path)
		: Font::get(FONT_SIZE_MEDIUM);
	mAlignment = mt.sectionAlignment.value_or(ALIGN_CENTER);
	mColor = mt.section.color.value_or(0x777777FF);
	mSelectedColor = mt.section.selectedColor.value_or(0xFFFFFFFF);

	const std::string text = sectionHeaderText(label, mAlignment);
	mLabelWidth = mFont->sizeText(text).x;
	mText = std::make_shared<TextComponent>(mWindow, text, mFont, mColor, mAlignment);
	addChild(mText.get());
	setSize(0, mFont->getHeight());
}

bool MenuSectionComponent::drawnAfterSelector(const Window* window)
{
	return window->getMenuTheme().themedSelector();
}

void MenuSectionComponent::onSizeChanged()
{
	const SectionHeaderLayout l = sectionHeaderLayout(
		mSize.x, mLabelWidth, mAlignment, Renderer::getScreenWidth(), Renderer::getScreenHeight());
	mText->setSize(mSize.x - 2 * l.textMargin, mSize.y);
	mText->setPosition(l.textMargin, 0);
}

void MenuSectionComponent::setColor(unsigned int color)
{
	mSelected = color != mColor;
	applyColor();
}

void MenuSectionComponent::applyColor()
{
	mText->setColor(mSelected ? mSelectedColor : mColor);
}

void MenuSectionComponent::render(const glm::mat4& parentTrans)
{
	const glm::mat4 trans = roundMatrix(parentTrans * getTransform());
	Renderer::setMatrix(trans);
	const unsigned int color = mSelected ? mSelectedColor : mColor;
	const SectionHeaderLayout l = sectionHeaderLayout(
		mSize.x, mLabelWidth, mAlignment, Renderer::getScreenWidth(), Renderer::getScreenHeight());
	const int y = (int)(mSize.y / 2) - l.lineHeight / 2;
	for(int i = 0; i < l.lineCount; i++)
		Renderer::drawRect((int)l.lines[i].x, y, (int)l.lines[i].y, l.lineHeight, color);
	GuiComponent::renderChildren(trans);
}
