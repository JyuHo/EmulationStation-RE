#include "EmulationStation.h"
#include "utils/I18n.h"

#include <format>
#include "guis/GuiMenu.h"
#include "Window.h"
#include "Sound.h"
#include "MusicPlayer.h"
#include "Log.h"
#include "platform.h"
#include "Settings.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSettings.h"
#include "guis/GuiScraperStart.h"
#include "guis/GuiDetectDevice.h"
#include "views/ViewController.h"

#include "components/ButtonComponent.h"
#include "components/SwitchComponent.h"
#include "components/SliderComponent.h"
#include "components/TextComponent.h"
#include "components/OptionListComponent.h"
#include "components/MenuComponent.h"
#include "VolumeControl.h"
#include "scrapers/GamesDBScraper.h"
#include "scrapers/TheArchiveScraper.h"

GuiMenu::GuiMenu(Window* window) :
	GuiComponent(window),
	mMenu(window, _U("Main menu")),
	mVersion(window),
	mLanguage(Settings::getInstance()->getString("Language"))
{
	// MAIN MENU

	// SCRAPER >
	// SOUND SETTINGS >
	// UI SETTINGS >
	// CONFIGURE INPUT >
	// QUIT >

	// [version]

	auto openScrapeNow = [this] {
		mWindow->pushGui(std::make_unique<GuiScraperStart>(mWindow));
	};
	addEntry(_U("Scraper"), 0x777777FF, true, [this, openScrapeNow] {
		auto s = std::make_unique<GuiSettings>(mWindow, _U("Scraper"));

		// scrape from
		auto scraper_list = std::make_shared<OptionListComponent<std::string>>(mWindow, _U("Scrape from"), false);
		std::vector<std::string> scrapers = getScraperList();
		for(auto it = scrapers.begin(); it != scrapers.end(); it++)
			scraper_list->add(*it, *it, *it == Settings::getInstance()->getString("Scraper"));

		s->addWithLabel(_U("Scrape from"), scraper_list);
		s->addSaveFunc([scraper_list] { Settings::getInstance()->setString("Scraper", scraper_list->getSelected()); });

		// scrape ratings
		auto scrape_ratings = std::make_shared<SwitchComponent>(mWindow);
		scrape_ratings->setState(Settings::getInstance()->getBool("ScrapeRatings"));
		s->addWithLabel(_U("Scrape ratings"), scrape_ratings);
		s->addSaveFunc(
			[scrape_ratings] { Settings::getInstance()->setBool("ScrapeRatings", scrape_ratings->getState()); });

		// scrape now
		ComponentListRow row;
		std::function<void()> openAndSave = openScrapeNow;
		openAndSave = [&s, openAndSave] {
			s->save();
			openAndSave();
		};
		row.makeAcceptInputHandler(openAndSave);

		auto scrape_now =
			std::make_shared<TextComponent>(mWindow, _U("Scrape now"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
		auto bracket = makeArrow(mWindow);
		row.addElement(scrape_now, true);
		row.addElement(bracket, false);
		s->addRow(row);

		mWindow->pushGui(std::move(s));
	});

	addEntry(_U("Sound settings"), 0x777777FF, true, [this] {
		auto s = std::make_unique<GuiSettings>(mWindow, _U("Sound settings"));

		// sound card driven by the system volume
		auto audio_card = std::make_shared<OptionListComponent<std::string>>(mWindow, _U("Audio card"), false);
		const std::string current_card = Settings::getInstance()->getString("AudioCard");
		bool card_listed = current_card.empty();
		audio_card->add(_U("Automatic"), "", current_card.empty());
		for(const auto& [id, name] : VolumeControl::listCards())
		{
			audio_card->add(I18n::toUpperUtf8(name), id, id == current_card);
			card_listed = card_listed || id == current_card;
		}
		if(!card_listed) // set by hand ("default", "hw:1,0"...) or unplugged
			audio_card->add(current_card, current_card, true);
		s->addWithLabel(_U("Audio card"), audio_card);
		s->addSaveFunc([audio_card] {
			const std::string card = audio_card->getSelected();
			if(card == Settings::getInstance()->getString("AudioCard"))
				return;
			Settings::getInstance()->setString("AudioCard", card);
			auto& control = VolumeControl::getInstance();
			control->deinit();
			control->init();
			// the volume chosen before follows to the new card
			if(const int saved = Settings::getInstance()->getInt("SystemVolume"); saved >= 0)
				control->setVolume(saved);
		});

		// volume
		auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		const int system_volume = VolumeControl::getInstance()->getVolume();
		volume->setValue((float)system_volume);
		s->addWithLabel(_U("System volume"), volume);
		s->addSaveFunc([volume, system_volume] {
			const int chosen = (int)round(volume->getValue());
			if(chosen == system_volume)
				return; // untouched: the volume set outside ES stays
			VolumeControl::getInstance()->setVolume(chosen);
			Settings::getInstance()->setInt("SystemVolume", chosen); // applied again at the next start
		});

		// disable sounds
		auto sounds_enabled = std::make_shared<SwitchComponent>(mWindow);
		sounds_enabled->setState(Settings::getInstance()->getBool("EnableSounds"));
		s->addWithLabel(_U("Enable sounds"), sounds_enabled);
		s->addSaveFunc(
			[sounds_enabled] { Settings::getInstance()->setBool("EnableSounds", sounds_enabled->getState()); });

		// background music
		auto music_enabled = std::make_shared<SwitchComponent>(mWindow);
		music_enabled->setState(Settings::getInstance()->getBool("BackgroundMusic"));
		s->addWithLabel(_U("Background music"), music_enabled);
		auto music_volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		music_volume->setValue((float)Settings::getInstance()->getInt("MusicVolume"));
		s->addWithLabel(_U("Music volume"), music_volume);
		s->addSaveFunc([music_enabled, music_volume] {
			Settings::getInstance()->setBool("BackgroundMusic", music_enabled->getState());
			Settings::getInstance()->setInt("MusicVolume", (int)round(music_volume->getValue()));
			MusicPlayer::getInstance().applySettings(); // the volume fades to its new value
		});

		// sound of the videos
		auto video_audio = std::make_shared<SwitchComponent>(mWindow);
		video_audio->setState(Settings::getInstance()->getBool("VideoAudio"));
		s->addWithLabel(_U("Video sound"), video_audio);
		s->addSaveFunc([video_audio] { Settings::getInstance()->setBool("VideoAudio", video_audio->getState()); });

		mWindow->pushGui(std::move(s));
	});

	addEntry(_U("UI settings"), 0x777777FF, true, [this] {
		auto s = std::make_unique<GuiSettings>(mWindow, _U("UI settings"));

		// screensaver time
		auto screensaver_time = std::make_shared<SliderComponent>(mWindow, 0.f, 30.f, 1.f, "m");
		screensaver_time->setValue((float)(Settings::getInstance()->getInt("ScreenSaverTime") / (1000 * 60)));
		s->addWithLabel(_U("Screensaver after"), screensaver_time);
		s->addSaveFunc([screensaver_time] {
			Settings::getInstance()->setInt("ScreenSaverTime", (int)round(screensaver_time->getValue()) * (1000 * 60));
		});

		// screensaver behavior
		auto screensaver_behavior =
			std::make_shared<OptionListComponent<std::string>>(mWindow, _U("Screensaver behavior"), false);
		std::vector<std::string> screensavers;
		screensavers.emplace_back(N_("dim"));
		screensavers.emplace_back(N_("black"));
		screensavers.emplace_back(N_("gameclip")); // videos of the games, black without any
		for(auto it = screensavers.begin(); it != screensavers.end(); it++)
			screensaver_behavior->add(_(*it), *it, Settings::getInstance()->getString("ScreenSaverBehavior") == *it);
		s->addWithLabel(_U("Screensaver behavior"), screensaver_behavior);
		s->addSaveFunc([screensaver_behavior] {
			Settings::getInstance()->setString("ScreenSaverBehavior", screensaver_behavior->getSelected());
		});

		// framerate
		auto framerate = std::make_shared<SwitchComponent>(mWindow);
		framerate->setState(Settings::getInstance()->getBool("DrawFramerate"));
		s->addWithLabel(_U("Show framerate"), framerate);
		s->addSaveFunc([framerate] { Settings::getInstance()->setBool("DrawFramerate", framerate->getState()); });

		// show help
		auto show_help = std::make_shared<SwitchComponent>(mWindow);
		show_help->setState(Settings::getInstance()->getBool("ShowHelpPrompts"));
		s->addWithLabel(_U("On-screen help"), show_help);
		s->addSaveFunc([show_help] { Settings::getInstance()->setBool("ShowHelpPrompts", show_help->getState()); });

		// quick system select (left/right in game list view)
		auto quick_sys_select = std::make_shared<SwitchComponent>(mWindow);
		quick_sys_select->setState(Settings::getInstance()->getBool("QuickSystemSelect"));
		s->addWithLabel(_U("Quick system select"), quick_sys_select);
		s->addSaveFunc([quick_sys_select] {
			Settings::getInstance()->setBool("QuickSystemSelect", quick_sys_select->getState());
		});

		// transition style
		auto transition_style = std::make_shared<OptionListComponent<std::string>>(mWindow, _U("Transition style"), false);
		std::vector<std::string> transitions;
		transitions.emplace_back(N_("fade"));
		transitions.emplace_back(N_("slide"));
		for(auto it = transitions.begin(); it != transitions.end(); it++)
			transition_style->add(_(*it), *it, Settings::getInstance()->getString("TransitionStyle") == *it);
		s->addWithLabel(_U("Transition style"), transition_style);
		s->addSaveFunc([transition_style] {
			Settings::getInstance()->setString("TransitionStyle", transition_style->getSelected());
		});

		// theme set
		auto themeSets = ThemeData::getThemeSets();

		if(!themeSets.empty())
		{
			auto selectedSet = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
			if(selectedSet == themeSets.end())
				selectedSet = themeSets.begin();

			auto theme_set = std::make_shared<OptionListComponent<std::string>>(mWindow, _U("Theme set"), false);
			for(auto it = themeSets.begin(); it != themeSets.end(); it++)
				theme_set->add(it->first, it->first, it == selectedSet);
			s->addWithLabel(_U("Theme set"), theme_set);

			s->addSaveFunc([theme_set] {
				bool needReload = false;
				if(Settings::getInstance()->getString("ThemeSet") != theme_set->getSelected())
					needReload = true;

				Settings::getInstance()->setString("ThemeSet", theme_set->getSelected());

				if(needReload)
				{
					ViewController::get()
						->reloadAll(); // TODO - replace this with some sort of signal-based implementation
					// the music of the new theme set
					MusicPlayer::getInstance().stop();
					MusicPlayer::getInstance().init();
					MusicPlayer::getInstance().play();
				}
			});
		}

		// language: names in their own language, catalogs in locale/<code>/LC_MESSAGES
		static const std::pair<const char*, const char*> kLanguages[] = {
			{"en", "English"}, {"fr", "Français"}, {"de", "Deutsch"}, {"es", "Español"}, {"it", "Italiano"}};
		auto language = std::make_shared<OptionListComponent<std::string>>(mWindow, _U("Language"), false);
		const std::string current = Settings::getInstance()->getString("Language");
		bool found = false;
		for(const auto& [code, name] : kLanguages)
		{
			// fr_FR selects fr
			const bool selected = !found && current.starts_with(code);
			found = found || selected;
			language->add(name, code, selected);
		}
		if(!found)
			language->add(current, current, true);
		s->addWithLabel(_U("Language"), language);
		s->addSaveFunc([language] {
			if(Settings::getInstance()->getString("Language") == language->getSelected())
				return;
			Settings::getInstance()->setString("Language", language->getSelected());
			I18n::init(language->getSelected());
			ViewController::get()->reloadAll(); // views and help prompts in the new language
		});

		mWindow->pushGui(std::move(s));
	});

	addEntry(_U("Configure input"), 0x777777FF, true,
		[this] { mWindow->pushGui(std::make_unique<GuiDetectDevice>(mWindow, false, nullptr)); });

	addEntry(_U("Quit"), 0x777777FF, true, [this] {
		auto s = std::make_unique<GuiSettings>(mWindow, _U("Quit"));

		Window* window = mWindow;

		ComponentListRow row;
		row.makeAcceptInputHandler([window] {
			window->pushGui(std::make_unique<GuiMsgBox>(
				window, _U("Really restart?"), _U("Yes"),
				[] {
					if(runRestartCommand() != 0)
						LOG(LogWarning) << "Restart terminated with non-zero result!";
				},
				_U("No"), nullptr));
		});
		row.addElement(
			std::make_shared<TextComponent>(window, _U("Restart system"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
		s->addRow(row);

		row.elements.clear();
		row.makeAcceptInputHandler([window] {
			window->pushGui(std::make_unique<GuiMsgBox>(
				window, _U("Really shut down?"), _U("Yes"),
				[] {
					if(runShutdownCommand() != 0)
						LOG(LogWarning) << "Shutdown terminated with non-zero result!";
				},
				_U("No"), nullptr));
		});
		row.addElement(
			std::make_shared<TextComponent>(window, _U("Shut down system"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
		s->addRow(row);

		if(Settings::getInstance()->getBool("ShowExit"))
		{
			row.elements.clear();
			row.makeAcceptInputHandler([window] {
				window->pushGui(std::make_unique<GuiMsgBox>(
					window, _U("Really quit?"), _U("Yes"),
					[] {
						SDL_Event ev{};
						ev.type = SDL_EVENT_QUIT;
						SDL_PushEvent(&ev);
					},
					_U("No"), nullptr));
			});
			row.addElement(std::make_shared<TextComponent>(
							   window, _U("Quit EmulationStation"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF),
				true);
			s->addRow(row);
		}

		mWindow->pushGui(std::move(s));
	});

	mVersion.setFont(Font::get(FONT_SIZE_SMALL));
	mVersion.setColor(0xC6C6C6FF);
	const std::string version = strToUpper(PROGRAM_VERSION_STRING);
	mVersion.setText(std::vformat(_U("EmulationStation v{}"), std::make_format_args(version)));
	mVersion.setAlignment(ALIGN_CENTER);

	addChild(&mMenu);
	addChild(&mVersion);

	setSize(mMenu.getSize());
	setPosition((Renderer::getScreenWidth() - mSize.x) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiMenu::onSizeChanged()
{
	mVersion.setSize(mSize.x, 0);
	mVersion.setPosition(0, mSize.y - mVersion.getSize().y);
}

void GuiMenu::addEntry(const std::string& name, unsigned int color, bool add_arrow, const std::function<void()>& func)
{
	std::shared_ptr<Font> font = Font::get(FONT_SIZE_MEDIUM);

	// populate the list
	ComponentListRow row;
	row.addElement(std::make_shared<TextComponent>(mWindow, name, font, color), true);

	if(add_arrow)
	{
		std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);
		row.addElement(bracket, false);
	}

	row.makeAcceptInputHandler(func);

	mMenu.addRow(row);
}

bool GuiMenu::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	if((config->isMappedTo("b", input) || config->isMappedTo("start", input)) && input.value != 0)
	{
		delete this;
		return true;
	}

	return false;
}

void GuiMenu::update(int deltaTime)
{
	// back from the UI settings with a new language: this menu still holds the old strings
	if(Settings::getInstance()->getString("Language") != mLanguage)
	{
		Window* window = mWindow;
		delete this;
		window->pushGui(std::make_unique<GuiMenu>(window));
		return;
	}
	GuiComponent::update(deltaTime);
}

std::vector<HelpPrompt> GuiMenu::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.emplace_back("up/down", _("choose"));
	prompts.emplace_back("a", _("select"));
	prompts.emplace_back("start", _("close"));
	return prompts;
}
