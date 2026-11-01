#pragma once

#include "GuiComponent.h"
#include "components/ImageComponent.h"

class TextCache;
class Font;

// What an input does to a slider. Left/Right: pressed that way. Released: left/right let go,
// or their d-pad hat moved to up/down; the slider stops but lets the event through, so the
// list sees the release of up/down too. Other: not on the controls of left/right.
enum class SliderInput
{
	Left,
	Right,
	Released,
	Other
};
SliderInput classifySliderInput(InputConfig* config, Input input);

// Used to display/edit a value between some min and max values.
class SliderComponent : public GuiComponent
{
public:
	//Minimum value (far left of the slider), maximum value (far right of the slider), increment size (how much just pressing L/R moves by), unit to display (optional).
	SliderComponent(Window* window, float min, float max, float increment, const std::string& suffix = "");

	using GuiComponent::getValue;
	using GuiComponent::setValue;
	void setValue(float val);
	float getValue();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void onFocusLost() override;
	void render(const glm::mat4& parentTrans) override;

	void onSizeChanged() override;

	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void onValueChanged();

	float mMin, mMax;
	float mValue;
	float mSingleIncrement;
	float mMoveRate;
	int mMoveAccumulator = 0;

	ImageComponent mKnob;

	std::string mSuffix;
	std::shared_ptr<Font> mFont;
	std::shared_ptr<TextCache> mValueCache;
};
