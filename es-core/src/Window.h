#pragma once

#include "GuiComponent.h"
#include "MenuTheme.h"
#include <memory>
#include <vector>
#include "resources/Font.h"
#include "InputManager.h"
#include "renderer/Renderer.h"
#include "Screensaver.h"

class HelpComponent;
class ImageComponent;

class Window
{
public:
	Window();
	~Window();

	void pushGui(std::unique_ptr<GuiComponent> gui);
	// Unlinks gui from the stack without deleting it; called by ~GuiComponent.
	void removeGui(GuiComponent* gui);
	// Closes a GUI owned by the stack: unlinks it immediately and deletes it later
	// (at the start of the next update, or at destruction). Safe to call on a GUI
	// that is not on the stack.
	void closeGui(GuiComponent* gui);
	GuiComponent* peekGui();

	void textInput(const char* text);
	void input(InputConfig* config, Input input);
	void update(int deltaTime);
	void render();

	bool init(unsigned int width = 0, unsigned int height = 0);
	void deinit();

	void normalizeNextUpdate();

	inline bool isSleeping() const { return mSleeping; }
	bool getAllowSleep();
	void setAllowSleep(bool sleep);
	// shown while sleeping when ScreenSaverBehavior names it
	void setScreensaver(std::unique_ptr<Screensaver> screensaver);
	[[nodiscard]] bool isScreensaverActive() const { return mScreensaverActive; }
	// leaves the sleep, if any (before the audio device goes, at exit)
	void wake();

	void renderLoadingScreen();

	void renderHelpPromptsEarly(); // used to render HelpPrompts before a fade
	void setHelpPrompts(const std::vector<HelpPrompt>& prompts, const HelpStyle& style);

private:
	void sleep();
	void onSleep();
	void onWake();

	std::unique_ptr<HelpComponent> mHelp;
	std::unique_ptr<ImageComponent> mBackgroundOverlay;

	std::vector<std::unique_ptr<GuiComponent>> mGuiStack;
	std::vector<std::unique_ptr<GuiComponent>> mClosedGuis;

	std::vector<std::shared_ptr<Font>> mDefaultFonts;

	std::shared_ptr<const ThemeData> mMainTheme;
	es::theme::MenuTheme mMenuTheme;

public:
	const es::theme::MenuTheme& getMenuTheme() const { return mMenuTheme; }
	void setMainTheme(std::shared_ptr<const ThemeData> theme);

	int mFrameTimeElapsed;
	int mFrameCountElapsed;
	int mAverageDeltaTime;

	std::unique_ptr<TextCache> mFrameDataText;
	Renderer::DrawCallCount mLoggedDrawCalls; // last count written to the log

	bool mNormalizeNextUpdate;

	bool mAllowSleep;
	bool mSleeping;
	std::unique_ptr<Screensaver> mScreensaver;
	bool mScreensaverActive = false;
	unsigned int mTimeSinceLastInput;

	bool mRenderedHelpPrompts;
};
