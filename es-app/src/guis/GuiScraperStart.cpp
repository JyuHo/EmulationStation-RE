#include "guis/GuiScraperStart.h"
#include "utils/I18n.h"
#include "guis/GuiScraperMulti.h"
#include "guis/GuiMsgBox.h"
#include "views/ViewController.h"

#include "components/TextComponent.h"
#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"

GuiScraperStart::GuiScraperStart(Window* window) : GuiComponent(window), mMenu(window, _U("Scrape now"))
{
	addChild(&mMenu);

	// add filters (with first one selected)
	mFilters = std::make_shared<OptionListComponent<GameFilterFunc>>(mWindow, _U("Scrape these games"), false);
	mFilters->add(_("All games"), [](SystemData*, FileData*) -> bool { return true; }, false);
	mFilters->add(
		_("Only missing image"), [](SystemData*, FileData* g) -> bool { return g->metadata.get("image").empty(); }, true);
	mMenu.addWithLabel(_("Filter"), mFilters);

	//add systems (all with a platformid specified selected)
	mSystems = std::make_shared<OptionListComponent<SystemData*>>(mWindow, _U("Scrape these systems"), true);
	for(auto it = SystemData::sSystemVector.begin(); it != SystemData::sSystemVector.end(); it++)
	{
		if(!(*it)->hasPlatformId(PlatformIds::PLATFORM_IGNORE))
			mSystems->add((*it)->getFullName(), *it, !(*it)->getPlatformIds().empty());
	}
	mMenu.addWithLabel(_("Systems"), mSystems);

	mApproveResults = std::make_shared<SwitchComponent>(mWindow);
	mApproveResults->setState(true);
	mMenu.addWithLabel(_("User decides on conflicts"), mApproveResults);

	mMenu.addButton(_U("Start"), _("start"), std::bind(&GuiScraperStart::pressedStart, this));
	mMenu.addButton(_U("Back"), _("back"), [&] { mWindow->closeGui(this); });

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiScraperStart::pressedStart()
{
	std::vector<SystemData*> sys = mSystems->getSelectedObjects();
	for(auto it = sys.begin(); it != sys.end(); it++)
	{
		if((*it)->getPlatformIds().empty())
		{
			mWindow->pushGui(std::make_unique<GuiMsgBox>(mWindow,
				_U("Warning: some of your selected systems do not have a platform set. Results may be even "
				   "more inaccurate than usual!\nContinue anyway?"),
				_U("Yes"), std::bind(&GuiScraperStart::start, this), _U("No"), nullptr));
			return;
		}
	}

	start();
}

void GuiScraperStart::start()
{
	std::queue<ScraperSearchParams> searches = getSearches(mSystems->getSelectedObjects(), mFilters->getSelected());

	if(searches.empty())
	{
		mWindow->pushGui(std::make_unique<GuiMsgBox>(mWindow, _U("No games fit that criteria.")));
	}
	else
	{
		auto gsm = std::make_unique<GuiScraperMulti>(mWindow, searches, mApproveResults->getState());
		mWindow->pushGui(std::move(gsm));
		mWindow->closeGui(this);
	}
}

std::queue<ScraperSearchParams> GuiScraperStart::getSearches(std::vector<SystemData*> systems, GameFilterFunc selector)
{
	std::queue<ScraperSearchParams> queue;
	for(auto sys = systems.begin(); sys != systems.end(); sys++)
	{
		std::vector<FileData*> games = (*sys)->getRootFolder()->getFilesRecursive(GAME);
		for(auto game = games.begin(); game != games.end(); game++)
		{
			if(selector((*sys), (*game)))
			{
				ScraperSearchParams search;
				search.game = *game;
				search.system = *sys;

				queue.push(search);
			}
		}
	}

	return queue;
}

bool GuiScraperStart::input(InputConfig* config, Input input)
{
	bool consumed = GuiComponent::input(config, input);
	if(consumed)
		return true;

	if(input.value != 0 && config->isMappedTo("b", input))
	{
		mWindow->closeGui(this);
		return true;
	}

	if(config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		Window* window = mWindow;
		while(window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();
	}

	return false;
}

std::vector<HelpPrompt> GuiScraperStart::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.emplace_back("b", _("back"));
	prompts.emplace_back("start", _("close"));
	return prompts;
}
