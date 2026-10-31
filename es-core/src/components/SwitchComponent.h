#pragma once

#include "GuiComponent.h"
#include "components/ImageComponent.h"

// A very simple "on/off" switch.
// Should hopefully be switched to use images instead of text in the future.
class SwitchComponent : public GuiComponent
{
public:
	SwitchComponent(Window* window, bool state = false);

	bool input(InputConfig* config, Input input) override;
	void render(const glm::mat4& parentTrans) override;
	void onSizeChanged() override;

	bool getState() const;
	void setState(bool state);

	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void onStateChanged();

	ImageComponent mImage;
	bool mState;
};
