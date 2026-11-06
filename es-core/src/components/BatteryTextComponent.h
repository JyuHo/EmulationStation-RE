#pragma once

#ifndef ES_CORE_COMPONENTS_BATTTEXT_COMPONENT_H
#define ES_CORE_COMPONENTS_BATTTEXT_COMPONENT_H

#include "GuiComponent.h"
#include "components/TextComponent.h"
#include "utils/Platform.h"
#include "watchers/WatchersManager.h"

class Window;

class BatteryTextComponent : public TextComponent, IWatcherNotify
{
public:
	BatteryTextComponent(Window* window);
	~BatteryTextComponent() override;

	std::string getThemeTypeName() override { return "batteryText"; }

	void update(int deltaTime) override;

	void OnWatcherChanged(IWatcher* component) override;

private:
	Utils::Platform::BatteryInformation mBatteryInfo;
	bool mDirty;
};

#endif // ES_CORE_COMPONENTS_BATTTEXT_COMPONENT_H
