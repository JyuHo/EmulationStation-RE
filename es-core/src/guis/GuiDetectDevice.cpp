#include "guis/GuiDetectDevice.h"
#include "Window.h"
#include "Renderer.h"
#include "resources/Font.h"
#include "guis/GuiInputConfig.h"
#include "components/TextComponent.h"
#include <iostream>
#include <string>
#include <sstream>
#include "Util.h"

#define HOLD_TIME 1000


GuiDetectDevice::GuiDetectDevice(Window* window, bool firstRun, const std::function<void()>& doneCallback) :
	GuiComponent(window), mFirstRun(firstRun), mBackground(window, ":/frame.png"), mGrid(window, glm::ivec2(1, 5))
{
	mHoldingConfig = nullptr;
	mHoldTime = 0;
	mDoneCallback = doneCallback;

	addChild(&mBackground);
	addChild(&mGrid);

	// title
	mTitle = std::make_shared<TextComponent>(
		mWindow, firstRun ? "WELCOME" : "CONFIGURE INPUT", Font::get(FONT_SIZE_LARGE), 0x555555FF, ALIGN_CENTER);
	mGrid.setEntry(mTitle, glm::ivec2(0, 0), false, true, glm::ivec2(1, 1), GridFlags::BORDER_BOTTOM);

	// device info
	std::stringstream deviceInfo;
	int numDevices = InputManager::getInstance()->getNumJoysticks();

	if(numDevices > 0)
		deviceInfo << numDevices << " GAMEPAD" << (numDevices > 1 ? "S" : "") << " DETECTED";
	else
		deviceInfo << "NO GAMEPADS DETECTED";
	mDeviceInfo = std::make_shared<TextComponent>(
		mWindow, deviceInfo.str(), Font::get(FONT_SIZE_SMALL), 0x999999FF, ALIGN_CENTER);
	mGrid.setEntry(mDeviceInfo, glm::ivec2(0, 1), false, true);

	// message
	mMsg1 = std::make_shared<TextComponent>(
		mWindow, "HOLD A BUTTON ON YOUR DEVICE TO CONFIGURE IT.", Font::get(FONT_SIZE_SMALL), 0x777777FF, ALIGN_CENTER);
	mGrid.setEntry(mMsg1, glm::ivec2(0, 2), false, true);

	const char* msg2str = firstRun ? "PRESS F4 TO QUIT AT ANY TIME." : "PRESS ESC TO CANCEL.";
	mMsg2 = std::make_shared<TextComponent>(mWindow, msg2str, Font::get(FONT_SIZE_SMALL), 0x777777FF, ALIGN_CENTER);
	mGrid.setEntry(mMsg2, glm::ivec2(0, 3), false, true);

	// currently held device
	mDeviceHeld = std::make_shared<TextComponent>(mWindow, "", Font::get(FONT_SIZE_MEDIUM), 0xFFFFFFFF, ALIGN_CENTER);
	mGrid.setEntry(mDeviceHeld, glm::ivec2(0, 4), false, true);

	setSize(Renderer::getScreenWidth() * 0.6f, Renderer::getScreenHeight() * 0.5f);
	setPosition((Renderer::getScreenWidth() - mSize.x) / 2, (Renderer::getScreenHeight() - mSize.y) / 2);
}

void GuiDetectDevice::onSizeChanged()
{
	mBackground.fitTo(mSize, glm::vec3(0.0f), glm::vec2(-32, -32));

	// grid
	mGrid.setSize(mSize);
	mGrid.setRowHeightPerc(0, mTitle->getFont()->getHeight() / mSize.y);
	//mGrid.setRowHeightPerc(1, mDeviceInfo->getFont()->getHeight() / mSize.y);
	mGrid.setRowHeightPerc(2, mMsg1->getFont()->getHeight() / mSize.y);
	mGrid.setRowHeightPerc(3, mMsg2->getFont()->getHeight() / mSize.y);
	//mGrid.setRowHeightPerc(4, mDeviceHeld->getFont()->getHeight() / mSize.y);
}

bool GuiDetectDevice::input(InputConfig* config, Input input)
{
	if(!mFirstRun && input.device == DEVICE_KEYBOARD && input.type == TYPE_KEY && input.value &&
		input.id == SDLK_ESCAPE)
	{
		// cancel configuring
		delete this;
		return true;
	}

	if(input.type == TYPE_BUTTON || input.type == TYPE_KEY)
	{
		if(input.value && mHoldingConfig == nullptr)
		{
			// started holding
			mHoldingConfig = config;
			mHoldTime = HOLD_TIME;
			mDeviceHeld->setText(strToUpper(config->getDeviceName()));
		}
		else if(!input.value && mHoldingConfig == config)
		{
			// cancel
			mHoldingConfig = nullptr;
			mDeviceHeld->setText("");
		}
	}

	return true;
}

void GuiDetectDevice::update(int deltaTime)
{
	if(mHoldingConfig)
	{
		mHoldTime -= deltaTime;
		const float t = (float)mHoldTime / HOLD_TIME;
		unsigned int c = (unsigned char)(t * 255);
		mDeviceHeld->setColor((c << 24) | (c << 16) | (c << 8) | 0xFF);
		if(mHoldTime <= 0)
		{
			// picked one!
			mWindow->pushGui(std::make_unique<GuiInputConfig>(mWindow, mHoldingConfig, true, mDoneCallback));
			delete this;
		}
	}
}
