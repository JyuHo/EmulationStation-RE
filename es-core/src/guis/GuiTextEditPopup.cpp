#include "Window.h"
#include "guis/GuiTextEditPopup.h"
#include "utils/I18n.h"
#include "components/MenuComponent.h"

GuiTextEditPopup::GuiTextEditPopup(Window* window, const std::string& title, const std::string& initValue,
	const std::function<void(const std::string&)>& okCallback, bool multiLine, const std::string& acceptBtnText) :
	GuiComponent(window), mBackground(window, ":/frame.png"), mGrid(window, glm::ivec2(1, 3)), mMultiLine(multiLine)
{
	applyMenuFrame(mBackground, mWindow->getMenuTheme()); // theme frame (11f2)
	addChild(&mBackground);
	addChild(&mGrid);

	mTitle = std::make_shared<TextComponent>(
		mWindow, strToUpper(title), es::theme::menuFontOr(mWindow->getMenuTheme().title.font, Font::get(FONT_SIZE_LARGE)),
		mWindow->getMenuTheme().title.color.value_or(0x555555FF), ALIGN_CENTER);

	mText = std::make_shared<TextEditComponent>(mWindow);
	mText->setValue(initValue);

	if(!multiLine)
		mText->setCursor(initValue.size());

	std::vector<std::shared_ptr<ButtonComponent>> buttons;
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, acceptBtnText.empty() ? _U("OK") : acceptBtnText,
		acceptBtnText.empty() ? _("ok") : I18n::toUpperUtf8(acceptBtnText), [this, okCallback] {
		okCallback(mText->getValue());
		mWindow->closeGui(this);
	}));
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _U("Cancel"), _("discard changes"), [this] { mWindow->closeGui(this); }));

	mButtonGrid = makeButtonGrid(mWindow, buttons);

	mGrid.setEntry(mTitle, glm::ivec2(0, 0), false, true);
	mGrid.setEntry(
		mText, glm::ivec2(0, 1), true, false, glm::ivec2(1, 1), GridFlags::BORDER_TOP | GridFlags::BORDER_BOTTOM);
	mGrid.setEntry(mButtonGrid, glm::ivec2(0, 2), true, false);

	float textHeight = mText->getFont()->getHeight();
	if(multiLine)
		textHeight *= 6;
	mText->setSize(0, textHeight);

	setSize(
		Renderer::getScreenWidth() * 0.5f, mTitle->getFont()->getHeight() + textHeight + mButtonGrid->getSize().y + 40);
	setPosition((Renderer::getScreenWidth() - mSize.x) / 2, (Renderer::getScreenHeight() - mSize.y) / 2);
}

void GuiTextEditPopup::onSizeChanged()
{
	mBackground.fitTo(mSize, glm::vec3(0.0f), glm::vec2(-32, -32));

	mText->setSize(mSize.x - 40, mText->getSize().y);

	// update grid
	mGrid.setRowHeightPerc(0, mTitle->getFont()->getHeight() / mSize.y);
	mGrid.setRowHeightPerc(2, mButtonGrid->getSize().y / mSize.y);
	mGrid.setSize(mSize);
}

bool GuiTextEditPopup::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	// pressing back when not text editing closes us
	if(config->isMappedTo("b", input) && input.value)
	{
		mWindow->closeGui(this);
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiTextEditPopup::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mGrid.getHelpPrompts();
	prompts.emplace_back("b", _("back"));
	return prompts;
}
