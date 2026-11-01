#include "guis/GuiSettings.h"
#include "utils/I18n.h"
#include "Window.h"
#include "Settings.h"
#include "views/ViewController.h"

GuiSettings::GuiSettings(Window* window, const std::string& title) : GuiComponent(window), mMenu(window, title)
{
	addChild(&mMenu);

	mMenu.addButton(_("Back"), _("go back"), [this] { delete this; });

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	mMenu.setPosition((mSize.x - mMenu.getSize().x) / 2, Renderer::getScreenHeight() * 0.15f);
}

GuiSettings::~GuiSettings()
{
	save();
}

void GuiSettings::save()
{
	if(!mSaveFuncs.size())
		return;

	for(auto it = mSaveFuncs.begin(); it != mSaveFuncs.end(); it++)
		(*it)();

	Settings::getInstance()->saveFile();
}

bool GuiSettings::input(InputConfig* config, Input input)
{
	if(config->isMappedTo("b", input) && input.value != 0)
	{
		delete this;
		return true;
	}

	if(config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		Window* window = mWindow;
		while(window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();
		return true;
	}

	return GuiComponent::input(config, input);
}

std::vector<HelpPrompt> GuiSettings::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();

	prompts.emplace_back("b", _("back"));
	prompts.emplace_back("start", _("close"));

	return prompts;
}
