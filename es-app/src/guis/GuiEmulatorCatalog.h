#pragma once

// Emulator catalog: installs standalone emulators and RetroArch cores from the catalog of
// the es-re-catalog repository. Same layout as the theme downloader, with one tab per kind.

#include "GuiComponent.h"
#include "ContentInstaller.h"
#include "catalog/EmulatorCatalog.h"
#include "components/ComponentGrid.h"
#include "components/ComponentList.h"
#include "components/ComponentTab.h"
#include "components/ImageComponent.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"

#include <optional>
#include <string>

// One line of the list: state icon, name, details, and the logo on the right
class GuiEmulatorCatalogEntry : public ComponentGrid
{
public:
	GuiEmulatorCatalogEntry(Window* window, const std::string& title, const std::string& details, const std::string& logo, catalog::State state, bool pending);

	void setColor(unsigned int color) override;

private:
	std::shared_ptr<TextComponent>  mIcon;
	std::shared_ptr<TextComponent>  mText;
	std::shared_ptr<TextComponent>  mDetails;
	std::shared_ptr<ImageComponent> mLogo;
};

class GuiEmulatorCatalog : public GuiComponent, IContentInstalledNotify
{
public:
	GuiEmulatorCatalog(Window* window);
	~GuiEmulatorCatalog() override;

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;

	std::vector<HelpPrompt> getHelpPrompts() override;
	void onSizeChanged() override;

	void OnContentInstalled(int contentType, std::string contentName, bool success) override;

private:
	enum Tab { TAB_STANDALONE = 0, TAB_CORES = 1 };

	enum Reload
	{
		RELOAD_NONE = 0,
		RELOAD_CATALOG = 1,   // download the catalog again
		RELOAD_INSTALLED = 2, // something was installed or removed: read installed.json again
		RELOAD_LIST = 3,      // only the tab changed
	};

	void loadCatalogAsync();
	void loadList();
	void centerWindow();

	void showEmulator(const catalog::Emulator& emulator);
	void showCore(const catalog::Core& core);
	void enqueue(ContentInstaller::ContentType type, const std::string& id, const std::string& name);

	int  mReload;
	int  mTab;

	NinePatchComponent              mBackground;
	ComponentGrid                   mGrid;

	std::shared_ptr<TextComponent>  mTitle;
	std::shared_ptr<TextComponent>  mSubtitle;
	std::shared_ptr<ComponentList>  mList;
	std::shared_ptr<ComponentGrid>  mHeaderGrid;
	std::shared_ptr<ComponentGrid>  mButtonGrid;
	std::shared_ptr<ComponentTab>   mTabs;

	std::optional<catalog::Catalog> mCatalog;
	catalog::Installed              mInstalled;
	std::string                     mError;
};
