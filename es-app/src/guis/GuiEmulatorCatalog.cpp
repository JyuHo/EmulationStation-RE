#include "guis/GuiEmulatorCatalog.h"

#include "LocaleES.h"
#include "Window.h"
#include "catalog/CatalogInstaller.h"
#include "components/ButtonComponent.h"
#include "components/WebImageComponent.h"
#include "guis/GuiLoading.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSettings.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "views/ViewController.h"

#define WINDOW_WIDTH (float)Math::min(Renderer::getScreenHeight() * 1.125f, Renderer::getScreenWidth() * 0.90f)
#define ICON_DOWNLOAD _U(" ")

using namespace catalog;

namespace
{
	struct LoadResult
	{
		std::optional<Catalog> catalog;
		Installed installed;
		std::string error;
	};

	std::string stateText(State state)
	{
		switch (state)
		{
		case State::Installed: return _("INSTALLED");
		case State::UpdateAvailable: return _("UPDATE AVAILABLE");
		case State::System: return _("INSTALLED BY THE SYSTEM");
		default: return _("NOT INSTALLED");
		}
	}

	std::string buildText(const Build* build)
	{
		if (build == nullptr)
			return _("Requires Flatpak");

		if (build->type == BuildType::Flatpak)
			return "Flatpak  " + build->ref;

		std::string text = (build->type == BuildType::AppImage ? "AppImage" : _("Archive")) + std::string("  ") + build->version;
		if (build->size > 0)
			text += _U("    ") + Utils::FileSystem::megaBytesToString(build->size / (1024 * 1024));
		return text;
	}

	std::string systemsText(const std::vector<std::string>& systems)
	{
		if (systems.empty())
			return "";

		return _U("  ") + Utils::String::join(systems, ", ");
	}

	bool isPending(int install, int uninstall, const std::string& id)
	{
		return ContentInstaller::IsInQueue((ContentInstaller::ContentType)install, id) ||
			ContentInstaller::IsInQueue((ContentInstaller::ContentType)uninstall, id);
	}
}

GuiEmulatorCatalog::GuiEmulatorCatalog(Window* window)
	: GuiComponent(window), mReload(RELOAD_CATALOG), mTab(TAB_STANDALONE), mBackground(window, ":/frame.png"), mGrid(window, Vector2i(1, 4))
{
	addChild(&mBackground);
	addChild(&mGrid);

	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);
	mBackground.setPostProcessShader(theme->Background.menuShader);

	mHeaderGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i(1, 5));

	mTitle = std::make_shared<TextComponent>(mWindow, _("EMULATOR CATALOG"), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	mSubtitle = std::make_shared<TextComponent>(mWindow, _("SELECT EMULATORS AND CORES TO INSTALL/REMOVE"), theme->TextSmall.font, theme->TextSmall.color, ALIGN_CENTER);
	mHeaderGrid->setEntry(mTitle, Vector2i(0, 1), false, true);
	mHeaderGrid->setEntry(mSubtitle, Vector2i(0, 3), false, true);
	mGrid.setEntry(mHeaderGrid, Vector2i(0, 0), false, true);

	mTabs = std::make_shared<ComponentTab>(mWindow);
	mTabs->addTab(_("STANDALONE EMULATORS"));
	mTabs->addTab(_("RETROARCH CORES"));
	mTabs->setCursorChangedCallback([&](const CursorState& /*state*/)
	{
		if (mTab != mTabs->getCursorIndex())
		{
			mTab = mTabs->getCursorIndex();
			mReload = RELOAD_LIST;
		}
	});
	mGrid.setEntry(mTabs, Vector2i(0, 1), false, true, Vector2i(1, 1), GridFlags::BORDER_BOTTOM);

	mList = std::make_shared<ComponentList>(mWindow);
	mList->setUpdateType(ComponentListFlags::UpdateType::UPDATE_ALWAYS);
	mGrid.setEntry(mList, Vector2i(0, 2), true, true);

	std::vector<std::shared_ptr<ButtonComponent>> buttons;
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("BACK"), _("BACK"), [this] { delete this; }));
	mButtonGrid = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 3), true, false, Vector2i(1, 1), GridFlags::BORDER_TOP);

	mGrid.setUnhandledInputCallback([this](InputConfig* config, Input input) -> bool
	{
		if (config->isMappedLike("down", input)) { mGrid.setCursorTo(mList); mList->setCursorIndex(0); return true; }
		if (config->isMappedLike("up", input)) { mList->setCursorIndex(mList->size() - 1); mGrid.moveCursor(Vector2i(0, 1)); return true; }
		return false;
	});

	centerWindow();

	ContentInstaller::RegisterNotify(this);
}

GuiEmulatorCatalog::~GuiEmulatorCatalog()
{
	ContentInstaller::UnregisterNotify(this);
}

void GuiEmulatorCatalog::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
	mGrid.setSize(mSize);

	const float titleHeight = mTitle->getFont()->getLetterHeight();
	const float subtitleHeight = mSubtitle->getFont()->getLetterHeight();
	const float titleSubtitleSpacing = mSize.y() * 0.03f;

	mGrid.setRowHeight(0, titleHeight + titleSubtitleSpacing + subtitleHeight + (Renderer::getScreenHeight() * 0.05f));
	mGrid.setRowHeight(1, titleHeight * 2);
	mGrid.setRowHeight(3, mButtonGrid->getSize().y());

	mHeaderGrid->setRowHeight(1, titleHeight);
	mHeaderGrid->setRowHeight(2, titleSubtitleSpacing);
	mHeaderGrid->setRowHeight(3, subtitleHeight);
}

void GuiEmulatorCatalog::centerWindow()
{
	if (Renderer::ScreenSettings::fullScreenMenus())
		setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.875f);

	setPosition((Renderer::getScreenWidth() - getSize().x()) / 2, (Renderer::getScreenHeight() - getSize().y()) / 2);
}

void GuiEmulatorCatalog::OnContentInstalled(int contentType, std::string /*contentName*/, bool /*success*/)
{
	if (contentType >= ContentInstaller::CONTENT_EMULATOR_INSTALL && contentType <= ContentInstaller::CONTENT_CORE_UNINSTALL)
		mReload = RELOAD_INSTALLED;
}

void GuiEmulatorCatalog::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mReload == RELOAD_NONE)
		return;

	int reload = mReload;
	mReload = RELOAD_NONE;

	if (reload == RELOAD_CATALOG)
		loadCatalogAsync();
	else
	{
		if (reload == RELOAD_INSTALLED)
			mInstalled = CatalogInstaller::loadInstalled();

		loadList();
	}
}

void GuiEmulatorCatalog::loadCatalogAsync()
{
	mWindow->pushGui(new GuiLoading<LoadResult>(mWindow, _("PLEASE WAIT"),
		[](auto /*gui*/)
		{
			LoadResult result;
			result.catalog = CatalogInstaller::fetchCatalog(&result.error);
			result.installed = CatalogInstaller::loadInstalled();
			return result;
		},
		[this](LoadResult result)
		{
			mCatalog = std::move(result.catalog);
			mInstalled = std::move(result.installed);
			mError = result.error;
			loadList();
		}));
}

void GuiEmulatorCatalog::loadList()
{
	int idx = mList->getCursorIndex();
	mList->clear();

	auto theme = ThemeData::getMenuTheme();
	int count = 0;

	auto addRow = [&](std::shared_ptr<GuiEmulatorCatalogEntry> entry, bool pending, const std::function<void()>& onAccept)
	{
		ComponentListRow row;
		row.addElement(entry, true);
		if (!pending)
			row.makeAcceptInputHandler(onAccept);

		mList->addRow(row, count == idx);
		count++;
	};

	if (mCatalog && mTab == TAB_STANDALONE)
	{
		bool flatpak = CatalogInstaller::isFlatpakAvailable();

		for (auto& emu : mCatalog->emulators)
		{
			auto state = CatalogInstaller::state(emu, mInstalled);
			bool pending = isPending(ContentInstaller::CONTENT_EMULATOR_INSTALL, ContentInstaller::CONTENT_EMULATOR_UNINSTALL, emu.id);

			std::string details;
			if (pending)
				details = _("CURRENTLY IN DOWNLOAD QUEUE");
			else
			{
				details = stateText(state);
				if (!emu.description.empty())
					details += "  " + emu.description;
				details += "\r\n" + buildText(preferredBuild(emu, flatpak));
				auto systems = systemsText(emu.systems);
				if (!systems.empty())
					details += "\r\n" + systems;
			}

			auto entry = std::make_shared<GuiEmulatorCatalogEntry>(mWindow, emu.name, details, emu.logo, state, pending);
			std::string id = emu.id;
			addRow(entry, pending, [this, id]
			{
				if (mCatalog)
					if (auto e = mCatalog->findEmulator(id))
						showEmulator(*e);
			});
		}
	}
	else if (mCatalog && mTab == TAB_CORES)
	{
		for (auto& core : mCatalog->cores)
		{
			auto state = CatalogInstaller::state(core, mInstalled);
			bool pending = isPending(ContentInstaller::CONTENT_CORE_INSTALL, ContentInstaller::CONTENT_CORE_UNINSTALL, core.id);

			std::string details;
			if (pending)
				details = _("CURRENTLY IN DOWNLOAD QUEUE");
			else
			{
				details = stateText(state) + _U("    ") + core.version;
				auto systems = systemsText(core.systems);
				if (!systems.empty())
					details += "\r\n" + systems;
			}

			auto entry = std::make_shared<GuiEmulatorCatalogEntry>(mWindow, core.name, details, "", state, pending);
			std::string id = core.id;
			addRow(entry, pending, [this, id]
			{
				if (mCatalog)
					if (auto c = mCatalog->findCore(id))
						showCore(*c);
			});
		}
	}

	if (count == 0)
	{
		ComponentListRow row;
		row.selectable = false;
		auto text = std::make_shared<TextComponent>(mWindow, mCatalog ? _("No items") : _("Cannot load the catalog") + std::string(": ") + mError,
			theme->TextSmall.font, theme->Text.color, ALIGN_CENTER);
		row.addElement(text, true);
		mList->addRow(row, false, false);
	}

	centerWindow();
}

void GuiEmulatorCatalog::enqueue(ContentInstaller::ContentType type, const std::string& id, const std::string& name)
{
	char text[1024];
	snprintf(text, sizeof(text), _("'%s' ADDED TO DOWNLOAD QUEUE").c_str(), name.c_str());
	mWindow->displayNotificationMessage(ICON_DOWNLOAD + std::string(text));

	ContentInstaller::Enqueue(mWindow, type, id);
	mReload = RELOAD_LIST;
}

void GuiEmulatorCatalog::showEmulator(const catalog::Emulator& emulator)
{
	auto state = CatalogInstaller::state(emulator, mInstalled);
	auto build = preferredBuild(emulator, CatalogInstaller::isFlatpakAvailable());

	if (state == State::System)
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, emulator.name + "\n\n" +
			_("This emulator is installed by your distribution. Update or remove it with your package manager."), _("OK")));
		return;
	}

	if (build == nullptr && state == State::Available)
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, emulator.name + "\n\n" +
			_("On this machine, this emulator is only available as a Flatpak. Install Flatpak with your package manager first."), _("OK")));
		return;
	}

	auto msgBox = new GuiSettings(mWindow, emulator.name);
	msgBox->setSubTitle(emulator.homepage.empty() ? buildText(build) : emulator.homepage);
	msgBox->setTag("popup");

	std::string id = emulator.id;
	std::string name = emulator.name;

	if (state == State::Available || state == State::UpdateAvailable)
	{
		msgBox->addEntry(ICON_DOWNLOAD + (state == State::Available ? _("INSTALL") : _("UPDATE")), false, [this, msgBox, id, name]
		{
			enqueue(ContentInstaller::CONTENT_EMULATOR_INSTALL, id, name);
			msgBox->close();
		});
	}

	if (state == State::Installed || state == State::UpdateAvailable)
	{
		msgBox->addEntry(_U(" ") + _("REMOVE"), false, [this, msgBox, id, name]
		{
			enqueue(ContentInstaller::CONTENT_EMULATOR_UNINSTALL, id, name);
			msgBox->close();
		});
	}

	mWindow->pushGui(msgBox);
}

void GuiEmulatorCatalog::showCore(const catalog::Core& core)
{
	auto state = CatalogInstaller::state(core, mInstalled);

	if (state == State::System)
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, core.name + "\n\n" +
			_("This core is installed by your distribution. Update or remove it with your package manager."), _("OK")));
		return;
	}

	auto msgBox = new GuiSettings(mWindow, core.name);
	msgBox->setSubTitle(core.id + "_libretro  " + core.version);
	msgBox->setTag("popup");

	std::string id = core.id;
	std::string name = core.name;

	if (state == State::Available || state == State::UpdateAvailable)
	{
		msgBox->addEntry(ICON_DOWNLOAD + (state == State::Available ? _("INSTALL") : _("UPDATE")), false, [this, msgBox, id, name]
		{
			enqueue(ContentInstaller::CONTENT_CORE_INSTALL, id, name);
			msgBox->close();
		});
	}

	if (state == State::Installed || state == State::UpdateAvailable)
	{
		msgBox->addEntry(_U(" ") + _("REMOVE"), false, [this, msgBox, id, name]
		{
			enqueue(ContentInstaller::CONTENT_CORE_UNINSTALL, id, name);
			msgBox->close();
		});
	}

	mWindow->pushGui(msgBox);
}

bool GuiEmulatorCatalog::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	if (config->isMappedTo("start", input) && input.value != 0)
	{
		Window* window = mWindow;
		while (window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();

		return true;
	}

	if (mTabs->input(config, input))
		return true;

	return false;
}

std::vector<HelpPrompt> GuiEmulatorCatalog::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mList->getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}

//////////////////////////////////////////////////////////////
// GuiEmulatorCatalogEntry
//////////////////////////////////////////////////////////////

GuiEmulatorCatalogEntry::GuiEmulatorCatalogEntry(Window* window, const std::string& title, const std::string& details, const std::string& logo, State state, bool pending)
	: ComponentGrid(window, Vector2i(5, 4))
{
	auto theme = ThemeData::getMenuTheme();

	std::string icon;
	switch (state)
	{
	case State::Installed: icon = _U(""); break;       // check
	case State::UpdateAvailable: icon = _U(""); break; // refresh
	case State::System: icon = _U(""); break;          // info
	default: icon = _U(""); break;                     // download
	}

	if (pending)
		icon = _U("");

	mIcon = std::make_shared<TextComponent>(mWindow);
	mIcon->setColor(theme->Text.color);
	mIcon->setHorizontalAlignment(Alignment::ALIGN_CENTER);
	mIcon->setVerticalAlignment(Alignment::ALIGN_TOP);
	mIcon->setFont(theme->Text.font);
	mIcon->setText(icon);
	mIcon->setSize(theme->Text.font->getLetterHeight() * 1.5f, 0);

	mText = std::make_shared<TextComponent>(mWindow, title, theme->Text.font, theme->Text.color);
	mText->setLineSpacing(1.5);
	mText->setVerticalAlignment(ALIGN_TOP);

	mDetails = std::make_shared<TextComponent>(mWindow, details, theme->TextSmall.font, theme->Text.color);
	mDetails->setOpacity(192);

	setEntry(mIcon, Vector2i(0, 1), false, true);
	setEntry(mText, Vector2i(2, 1), false, true);
	setEntry(mDetails, Vector2i(2, 2), false, true);

	float windowWidth = WINDOW_WIDTH;

	float itemHeight = (theme->Text.font->getHeight() + theme->TextSmall.font->getHeight() * 3) * 1.15f;
	itemHeight = Math::max(itemHeight, Renderer::getScreenHeight() * 0.132f);

	// Logos are square-ish icons, unlike the 16:9 theme previews
	float logoWidth = logo.empty() ? 0.0f : Math::min((itemHeight / windowWidth), 0.24f);

	setColWidthPerc(0, 42.0f / windowWidth, false);
	setColWidthPerc(1, 0.015f, false);
	setColWidthPerc(3, logo.empty() ? 0.0f : 0.01f, false);
	setColWidthPerc(4, logoWidth, false);

	setRowHeightPerc(0, (itemHeight - mText->getSize().y() - mDetails->getSize().y()) / itemHeight / 2.0f, false);
	setRowHeightPerc(1, mText->getSize().y() / itemHeight, false);
	setRowHeightPerc(2, mDetails->getSize().y() / itemHeight, false);

	if (pending)
	{
		mIcon->setOpacity(120);
		mText->setOpacity(150);
		mDetails->setOpacity(120);
	}

	if (!logo.empty())
	{
		Vector2f maxSize(windowWidth * logoWidth * 0.85f, itemHeight * 0.75f);

		auto image = std::make_shared<WebImageComponent>(window, 86400); // logos rarely change: keep them a day
		image->setImage(logo, false, maxSize);
		image->setMaxSize(maxSize);
		mLogo = image;

		setEntry(mLogo, Vector2i(4, 0), false, false, Vector2i(1, 4));
	}

	setSize(Vector2f(0, itemHeight));
}

void GuiEmulatorCatalogEntry::setColor(unsigned int color)
{
	mIcon->setColor(color);
	mText->setColor(color);
	mDetails->setColor(color);
}
