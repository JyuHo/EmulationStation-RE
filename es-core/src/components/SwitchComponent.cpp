#include "SwitchComponent.h"
#include "utils/I18n.h"
#include "renderer/Renderer.h"
#include "resources/Font.h"
#include "Window.h"

SwitchComponent::SwitchComponent(Window* window, bool state) : GuiComponent(window), mImage(window), mState(state)
{
	const auto& mt = mWindow->getMenuTheme();
	mOffPath = mt.switchOffPath.value_or(":/off.svg");
	mOnPath = mt.switchOnPath.value_or(":/on.svg");
	mImage.setImage(mState ? mOnPath : mOffPath);
	// tint: text color, only when the theme gives one (Recalbox SwitchComponent.cpp:17-21)
	if(mt.text.color)
		mImage.setColorShift(*mt.text.color);
	mImage.setResize(0, Font::get(FONT_SIZE_MEDIUM)->getLetterHeight());
	mSize = mImage.getSize();
}

void SwitchComponent::onSizeChanged()
{
	mImage.setSize(mSize);
}

bool SwitchComponent::input(InputConfig* config, Input input)
{
	if(config->isMappedTo("a", input) && input.value)
	{
		mState = !mState;
		onStateChanged();
		return true;
	}

	return false;
}

void SwitchComponent::render(const glm::mat4& parentTrans)
{
	glm::mat4 trans = parentTrans * getTransform();

	mImage.render(trans);

	renderChildren(trans);
}

bool SwitchComponent::getState() const
{
	return mState;
}

void SwitchComponent::setState(bool state)
{
	mState = state;
	onStateChanged();
}

void SwitchComponent::onStateChanged()
{
	mImage.setImage(mState ? mOnPath : mOffPath);
}

std::vector<HelpPrompt> SwitchComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.emplace_back("a", _("change"));
	return prompts;
}
