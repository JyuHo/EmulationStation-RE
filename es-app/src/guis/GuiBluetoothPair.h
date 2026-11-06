#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"

#include <thread>

class GuiBluetoothPair : public MenuComponent
{
public:
	GuiBluetoothPair(Window* window);
	~GuiBluetoothPair() override;

	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	void updateSize() override;

	void render(const Transform4x4f &parentTrans) override;
	void update(int deltaTime) override;

private:
	std::thread* mThread;
	void		 loadDevicesAsync();

	void		 onPairDevice(const std::string& macAddress);

	std::function<void(std::string)> mSaveFunction;

	BusyComponent mBusyAnim;

	bool mIsPairing;

	static GuiBluetoothPair*	Instance;
};
