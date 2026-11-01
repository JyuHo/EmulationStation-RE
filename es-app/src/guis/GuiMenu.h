#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include <functional>
#include <string>

class GuiMenu : public GuiComponent
{
public:
	GuiMenu(Window* window);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void onSizeChanged() override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void addEntry(const std::string& name, unsigned int color, bool add_arrow, const std::function<void()>& func);

	MenuComponent mMenu;
	TextComponent mVersion;
	std::string mLanguage; // the Language setting this menu was built in
};
