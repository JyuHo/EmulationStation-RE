#include "components/ButtonComponent.h"
#include "renderer/Renderer.h"
#include "Window.h"
#include "Util.h"
#include "Log.h"

ButtonComponent::ButtonComponent(
	Window* window, const std::string& text, const std::string& helpText, const std::function<void()>& func) :
	GuiComponent(window),
	mFont(Font::get(FONT_SIZE_MEDIUM)),
	mFocused(false),
	mEnabled(true),
	mTextColorFocused(0xFFFFFFFF),
	mTextColorUnfocused(0x777777FF),
	mBox(window, ":/button.png")
{
	const auto& mt = mWindow->getMenuTheme();
	if(mt.buttonPath) mBox.setImagePath(*mt.buttonPath);
	if(mt.text.font) mFont = Font::get((unsigned int)(mt.text.font->size * Renderer::getScreenHeight()), mt.text.font->path.c_str());
	if(mt.text.color) mTextColorUnfocused = *mt.text.color;
	if(mt.text.selectedColor) mTextColorFocused = *mt.text.selectedColor;

	setPressedFunc(func);
	setText(text, helpText);
	updateImage();
}

void ButtonComponent::onSizeChanged()
{
	mBox.fitTo(mSize, glm::vec3(0.0f), glm::vec2(-32, -32));
}

void ButtonComponent::setPressedFunc(std::function<void()> f)
{
	mPressedFunc = f;
}

bool ButtonComponent::input(InputConfig* config, Input input)
{
	if(config->isMappedTo("a", input) && input.value != 0)
	{
		if(mPressedFunc && mEnabled)
			mPressedFunc();
		return true;
	}

	return GuiComponent::input(config, input);
}

void ButtonComponent::setText(const std::string& text, const std::string& helpText)
{
	mText = strToUpper(text);
	mHelpText = helpText;

	mTextCache = mFont->buildTextCache(mText, 0, 0, getCurTextColor());

	float minWidth = mFont->sizeText("DELETE").x + 12;
	setSize(std::max(mTextCache->metrics.size.x + 12, minWidth), mTextCache->metrics.size.y);

	updateHelpPrompts();
}

void ButtonComponent::onFocusGained()
{
	mFocused = true;
	updateImage();
}

void ButtonComponent::onFocusLost()
{
	mFocused = false;
	updateImage();
}

void ButtonComponent::setEnabled(bool enabled)
{
	mEnabled = enabled;
	updateImage();
}

void ButtonComponent::updateImage()
{
	if(!mEnabled || !mPressedFunc)
	{
		mBox.setImagePath(":/button_filled.png");
		mBox.setCenterColor(0x770000FF);
		mBox.setEdgeColor(0x770000FF);
		return;
	}

	mBox.setCenterColor(0xFFFFFFFF);
	mBox.setEdgeColor(0xFFFFFFFF);
	mBox.setImagePath(mFocused ? ":/button_filled.png" : ":/button.png");
}

void ButtonComponent::render(const glm::mat4& parentTrans)
{
	glm::mat4 trans = roundMatrix(parentTrans * getTransform());

	mBox.render(trans);

	if(mTextCache)
	{
		glm::vec3 centerOffset(
			(mSize.x - mTextCache->metrics.size.x) / 2, (mSize.y - mTextCache->metrics.size.y) / 2, 0);
		centerOffset = roundVector(centerOffset);
		trans = glm::translate(trans, centerOffset);

		Renderer::setMatrix(trans);
		mTextCache->setColor(getCurTextColor());
		mFont->renderTextCache(mTextCache.get());
		trans = glm::translate(trans, -centerOffset);
	}

	renderChildren(trans);
}

unsigned int ButtonComponent::getCurTextColor() const
{
	if(!mFocused)
		return mTextColorUnfocused;
	else
		return mTextColorFocused;
}

std::vector<HelpPrompt> ButtonComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.emplace_back("a", mHelpText.empty() ? mText.c_str() : mHelpText.c_str());
	return prompts;
}
