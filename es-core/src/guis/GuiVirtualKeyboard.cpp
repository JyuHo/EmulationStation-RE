#include "guis/GuiVirtualKeyboard.h"

#include "Settings.h"
#include "Window.h"
#include "components/MenuComponent.h"
#include "components/TextComponent.h"
#include "guis/GuiTextEditPopup.h"
#include "renderer/Renderer.h"
#include "utils/I18n.h"

namespace {

// sizes, proportional to the screen
constexpr float kWidth = 0.7f;		  // of the screen width
constexpr float kKeyHeight = 0.075f;  // of the screen height
constexpr float kGap = 0.004f;		  // between keys, of the screen width
constexpr float kPadding = 0.02f;	  // inside the frame, of the screen width
constexpr float kKeyFont = 0.045f;	  // of the screen height
constexpr float kTitleFont = 0.06f;	  // of the screen height
constexpr float kEditFont = 0.05f;	  // of the screen height
constexpr unsigned int kEditBox = 0x00000040; // shade behind the typed text

using Kind = VirtualKeyboardModel::KeyKind;

} // namespace

GuiVirtualKeyboard::Colors GuiVirtualKeyboard::colorsFrom(const es::theme::KeyboardTheme& t)
{
	Colors c;
	c.key = t.keyColor.value_or(c.key);
	c.keySelected = t.keySelectedColor.value_or(c.keySelected);
	c.keyText = t.keyTextColor.value_or(c.keyText);
	c.keyDisabled = t.keyDisabledColor.value_or(c.keyDisabled);
	c.keyModifier = t.keyModifierColor.value_or(c.keyModifier);
	c.keyModifierLocked = t.keyModifierLockedColor.value_or(c.keyModifierLocked);
	c.title = t.keyTitleColor.value_or(c.title);
	c.editText = t.keyEditTextColor.value_or(c.editText);
	return c;
}

GuiVirtualKeyboard::GuiVirtualKeyboard(Window* window, const std::string& title, const std::string& initValue,
	const std::function<void(const std::string&)>& okCallback, const std::string& themeView) :
	GuiComponent(window), mModel(initValue), mOkCallback(okCallback), mBackground(window, ":/frame.png")
{
	// colors and font read when the keyboard opens, in the view it opens over
	es::theme::KeyboardTheme theme;
	if(const auto& main = mWindow->getMainTheme())
		theme = es::theme::KeyboardTheme::fromTheme(*main, themeView);
	mColors = colorsFrom(theme);
	const float h = (float)Renderer::getScreenHeight();
	auto font = [&](float size) {
		return es::theme::menuFontOr(
			theme.fontPath ? std::optional<es::theme::MenuFont>(es::theme::MenuFont{*theme.fontPath, size}) : std::nullopt,
			Font::get((int)(size * h)));
	};

	applyMenuFrame(mBackground, mWindow->getMenuTheme());
	addChild(&mBackground);

	mTitle = std::make_shared<TextComponent>(mWindow, strToUpper(title), font(kTitleFont), mColors.title, ALIGN_CENTER);
	addChild(mTitle.get());
	mEdit = std::make_shared<TextComponent>(mWindow, "", font(kEditFont), mColors.editText, ALIGN_CENTER);
	addChild(mEdit.get());

	auto keyFont = font(kKeyFont);
	for(const auto& row : mModel.rows())
	{
		mKeyLabels.emplace_back();
		for(size_t i = 0; i < row.size(); i++)
		{
			auto label = std::make_shared<TextComponent>(mWindow, "", keyFont, mColors.keyText, ALIGN_CENTER);
			mKeyLabels.back().push_back(label);
			addChild(label.get());
		}
	}
	updateLabels();

	const float w = (float)Renderer::getScreenWidth();
	mPadding = w * kPadding;
	mGap = w * kGap;
	mKeyHeight = h * kKeyHeight;
	const float width = w * kWidth;
	const float height = mPadding * 4 + mTitle->getFont()->getHeight() + mEdit->getFont()->getHeight() +
		(float)mModel.rows().size() * (mKeyHeight + mGap);
	setSize(width, height);
	setPosition((w - width) / 2, (h - height) / 2);
}

void GuiVirtualKeyboard::updateLabels()
{
	static const char* const kSpecial[] = {nullptr, N_("Shift"), N_("Space"), N_("Delete"), N_("OK"), N_("Cancel")};
	const auto& rows = mModel.rows();
	for(size_t r = 0; r < rows.size(); r++)
		for(size_t i = 0; i < rows[r].size(); i++)
		{
			const auto& key = rows[r][i];
			auto& label = mKeyLabels[r][i];
			if(key.kind == Kind::Char)
				label->setText(mModel.displayLabel(key));
			else
				label->setText(_U(kSpecial[(int)key.kind]));
			unsigned int color = mColors.keyText;
			if(key.kind == Kind::Shift && mModel.shift() == VirtualKeyboardModel::Shift::Once)
				color = mColors.keyModifier;
			else if(key.kind == Kind::Shift && mModel.shift() == VirtualKeyboardModel::Shift::Locked)
				color = mColors.keyModifierLocked;
			label->setColor(color);
		}
	mEdit->setText(mModel.text());
}

void GuiVirtualKeyboard::onSizeChanged()
{
	mBackground.fitTo(mSize, glm::vec3(0.0f), glm::vec2(-32, -32));
	const float inner = mSize.x - 2 * mPadding;
	mUnit = inner / VirtualKeyboardModel::kColumns;

	mTitle->setSize(inner, mTitle->getFont()->getHeight());
	mTitle->setPosition(mPadding, mPadding);
	const float editTop = mPadding * 2 + mTitle->getSize().y;
	mEdit->setSize(inner, mEdit->getFont()->getHeight());
	mEdit->setPosition(mPadding, editTop);
	mKeyTop = editTop + mEdit->getSize().y + mPadding;

	const auto& rows = mModel.rows();
	for(size_t r = 0; r < rows.size(); r++)
		for(size_t i = 0; i < rows[r].size(); i++)
		{
			const auto& key = rows[r][i];
			auto& label = mKeyLabels[r][i];
			label->setSize(key.width * mUnit - mGap, mKeyHeight);
			label->setPosition(mPadding + key.start * mUnit + mGap / 2, mKeyTop + r * (mKeyHeight + mGap));
		}
}

void GuiVirtualKeyboard::render(const glm::mat4& parentTrans)
{
	const glm::mat4 trans = parentTrans * getTransform();
	mBackground.render(trans);

	Renderer::setMatrix(trans);
	Renderer::drawRect(mPadding, mEdit->getPosition().y, mSize.x - 2 * mPadding, mEdit->getSize().y, kEditBox);
	const auto& rows = mModel.rows();
	for(size_t r = 0; r < rows.size(); r++)
		for(size_t i = 0; i < rows[r].size(); i++)
		{
			const auto& key = rows[r][i];
			const bool selected = (int)r == mModel.row() && (int)i == mModel.column();
			Renderer::drawRect(mPadding + key.start * mUnit + mGap / 2, mKeyTop + r * (mKeyHeight + mGap),
				key.width * mUnit - mGap, mKeyHeight, selected ? mColors.keySelected : mColors.key);
		}

	mTitle->render(trans);
	mEdit->render(trans);
	for(const auto& row : mKeyLabels)
		for(const auto& label : row)
			label->render(trans);
}

void GuiVirtualKeyboard::accept()
{
	const std::string text = mModel.text();
	auto callback = mOkCallback;
	mWindow->closeGui(this);
	if(callback)
		callback(text);
}

bool GuiVirtualKeyboard::input(InputConfig* config, Input input)
{
	if(input.value == 0)
		return false;
	if(config->isMappedTo("left", input)) mModel.move(-1, 0);
	else if(config->isMappedTo("right", input)) mModel.move(1, 0);
	else if(config->isMappedTo("up", input)) mModel.move(0, -1);
	else if(config->isMappedTo("down", input)) mModel.move(0, 1);
	else if(config->isMappedTo("a", input))
	{
		switch(mModel.press())
		{
			case VirtualKeyboardModel::Result::Ok: accept(); return true;
			case VirtualKeyboardModel::Result::Cancel: mWindow->closeGui(this); return true;
			case VirtualKeyboardModel::Result::None: break;
		}
		updateLabels();
	}
	else if(config->isMappedTo("b", input))
	{
		mWindow->closeGui(this);
		return true;
	}
	else if(config->isMappedTo("y", input))
	{
		mModel.backspace();
		updateLabels();
	}
	else if(config->isMappedTo("start", input))
	{
		accept();
		return true;
	}
	else
		return false;
	return true;
}

std::vector<HelpPrompt> GuiVirtualKeyboard::getHelpPrompts()
{
	return {{"up/down/left/right", _("move")}, {"a", _("press")}, {"b", _("cancel")}, {"y", _("delete")},
		{"start", _("ok")}};
}

void openTextInput(Window* window, const std::string& title, const std::string& initValue,
	const std::function<void(const std::string&)>& okCallback, bool multiLine, const std::string& themeView,
	const std::string& acceptBtnText)
{
	if(Settings::getInstance()->getBool("VirtualKeyboard"))
		window->pushGui(std::make_unique<GuiVirtualKeyboard>(window, title, initValue, okCallback, themeView));
	else
		window->pushGui(
			std::make_unique<GuiTextEditPopup>(window, title, initValue, okCallback, multiLine, acceptBtnText));
}
