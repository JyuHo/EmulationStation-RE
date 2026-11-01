#include "components/SliderComponent.h"
#include "utils/I18n.h"
#include <assert.h>
#include "renderer/Renderer.h"
#include "resources/Font.h"
#include "Log.h"
#include "Util.h"

#define MOVE_REPEAT_DELAY 500
#define MOVE_REPEAT_RATE 40

SliderComponent::SliderComponent(Window* window, float min, float max, float increment, const std::string& suffix) :
	GuiComponent(window),
	mMin(min),
	mMax(max),
	mSingleIncrement(increment),
	mMoveRate(0),
	mKnob(window),
	mSuffix(suffix)
{
	assert((min - max) != 0);

	// some sane default value
	mValue = (max + min) / 2;

	mKnob.setOrigin(0.5f, 0.5f);
	mKnob.setImage(":/slider_knob.svg");

	setSize(Renderer::getScreenWidth() * 0.15f, Font::get(FONT_SIZE_MEDIUM)->getLetterHeight());
}

SliderInput classifySliderInput(InputConfig* config, Input input)
{
	// the control behind left/right, whatever its current value
	Input control = input;
	control.value = 0;
	if(!config->isMappedTo("left", control) && !config->isMappedTo("right", control))
		return SliderInput::Other;

	if(input.value != 0 && config->isMappedTo("left", input))
		return SliderInput::Left;
	if(input.value != 0 && config->isMappedTo("right", input))
		return SliderInput::Right;
	return SliderInput::Released;
}

bool SliderComponent::input(InputConfig* config, Input input)
{
	const SliderInput kind = classifySliderInput(config, input);
	switch(kind)
	{
	case SliderInput::Left:
	case SliderInput::Right: {
		const float step = kind == SliderInput::Left ? -mSingleIncrement : mSingleIncrement;
		setValue(mValue + step);
		mMoveRate = step;
		mMoveAccumulator = -MOVE_REPEAT_DELAY;
		return true;
	}
	case SliderInput::Released:
		// stop repeating but don't consume the event: a d-pad hat sends a single "centered"
		// release for its four directions, and swallowing it hid the release of up/down from
		// the list, which then kept scrolling past the slider on its own
		mMoveRate = 0;
		break;
	case SliderInput::Other: break;
	}
	return GuiComponent::input(config, input);
}

void SliderComponent::onFocusLost()
{
	// a release that reaches another row must not leave the slider moving when it comes back
	mMoveRate = 0;
}

void SliderComponent::update(int deltaTime)
{
	if(mMoveRate != 0)
	{
		mMoveAccumulator += deltaTime;
		while(mMoveAccumulator >= MOVE_REPEAT_RATE)
		{
			setValue(mValue + mMoveRate);
			mMoveAccumulator -= MOVE_REPEAT_RATE;
		}
	}

	GuiComponent::update(deltaTime);
}

void SliderComponent::render(const glm::mat4& parentTrans)
{
	glm::mat4 trans = roundMatrix(parentTrans * getTransform());
	Renderer::setMatrix(trans);

	// render suffix
	if(mValueCache)
		mFont->renderTextCache(mValueCache.get());

	float width = mSize.x - mKnob.getSize().x - (mValueCache ? mValueCache->metrics.size.x + 4 : 0);

	//render line
	const float lineWidth = 2;
	Renderer::drawRect(mKnob.getSize().x / 2, mSize.y / 2 - lineWidth / 2, width, lineWidth, 0x777777FF);

	//render knob
	mKnob.render(trans);

	GuiComponent::renderChildren(trans);
}

void SliderComponent::setValue(float value)
{
	mValue = value;
	if(mValue < mMin)
		mValue = mMin;
	else if(mValue > mMax)
		mValue = mMax;

	onValueChanged();
}

float SliderComponent::getValue()
{
	return mValue;
}

void SliderComponent::onSizeChanged()
{
	if(!mSuffix.empty())
		mFont = Font::get((int)(mSize.y), FONT_PATH_LIGHT);

	onValueChanged();
}

void SliderComponent::onValueChanged()
{
	// update suffix textcache
	if(mFont)
	{
		std::stringstream ss;
		ss << std::fixed;
		ss.precision(0);
		ss << mValue;
		ss << mSuffix;
		const std::string val = ss.str();

		ss.str("");
		ss.clear();
		ss << std::fixed;
		ss.precision(0);
		ss << mMax;
		ss << mSuffix;
		const std::string max = ss.str();

		glm::vec2 textSize = mFont->sizeText(max);
		mValueCache = std::shared_ptr<TextCache>(
			mFont->buildTextCache(val, mSize.x - textSize.x, (mSize.y - textSize.y) / 2, 0x777777FF));
		mValueCache->metrics.size[0] = textSize.x; // fudge the width
	}

	// update knob position/size
	mKnob.setResize(0, mSize.y * 0.7f);
	float lineLength = mSize.x - mKnob.getSize().x - (mValueCache ? mValueCache->metrics.size.x + 4 : 0);
	mKnob.setPosition(((mValue + mMin) / mMax) * lineLength + mKnob.getSize().x / 2, mSize.y / 2);
}

std::vector<HelpPrompt> SliderComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.emplace_back("left/right", _("change"));
	return prompts;
}
