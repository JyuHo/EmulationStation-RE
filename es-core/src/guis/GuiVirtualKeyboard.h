#pragma once

#include "GuiComponent.h"
#include "MenuTheme.h"
#include "components/NinePatchComponent.h"
#include "guis/VirtualKeyboardModel.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class TextComponent;

// On-screen keyboard (phase 11f5), themed by the "keyboard" element of the main theme in the view
// it opens over (docs/THEMES.md, "keyboard"). Original layout and drawing of this project.
class GuiVirtualKeyboard : public GuiComponent
{
public:
	GuiVirtualKeyboard(Window* window, const std::string& title, const std::string& initValue,
		const std::function<void(const std::string&)>& okCallback, const std::string& themeView);

	bool input(InputConfig* config, Input input) override;
	void render(const glm::mat4& parentTrans) override;
	void onSizeChanged() override;
	std::vector<HelpPrompt> getHelpPrompts() override;

	// colors without a theme value: those of Recalbox (VirtualKeyboardTheme.h:17-24 @10.1.1)
	struct Colors
	{
		unsigned int key = 0x303030F0;
		unsigned int keySelected = 0xC02020FF;
		unsigned int keyText = 0xFFFFFFFF;
		unsigned int keyDisabled = 0x707070FF;
		unsigned int keyModifier = 0x4488FFFF;
		unsigned int keyModifierLocked = 0x224488FF;
		unsigned int title = 0xFFFF80FF;
		unsigned int editText = 0xFFFFFFFF;
	};
	static Colors colorsFrom(const es::theme::KeyboardTheme& theme);

private:
	void updateLabels();
	void accept();

	VirtualKeyboardModel mModel;
	std::function<void(const std::string&)> mOkCallback;
	Colors mColors;
	NinePatchComponent mBackground;
	std::shared_ptr<TextComponent> mTitle;
	std::shared_ptr<TextComponent> mEdit;
	std::vector<std::vector<std::shared_ptr<TextComponent>>> mKeyLabels;
	float mKeyTop = 0.0f;  // y of the first row of keys
	float mKeyHeight = 0.0f;
	float mUnit = 0.0f;	   // width of one column unit
	float mGap = 0.0f;
	float mPadding = 0.0f;
};

// Opens the text input of the user's choice: the on-screen keyboard when the VirtualKeyboard
// setting is on, else GuiTextEditPopup, with the same title, value and callback. `themeView` is
// the view the keyboard opens over (ViewController::keyboardThemeView).
void openTextInput(Window* window, const std::string& title, const std::string& initValue,
	const std::function<void(const std::string&)>& okCallback, bool multiLine, const std::string& themeView,
	const std::string& acceptBtnText = {});
