//EmulationStation, a graphical front-end for ROM browsing. Created by Alec "Aloshi" Lofquist.
//http://www.aloshi.com

#include <SDL3/SDL.h>
#include <iostream>
#include <iomanip>
#include "Renderer.h"
#include "views/ViewController.h"
#include "SystemData.h"
#include <filesystem>
#include "guis/GuiDetectDevice.h"
#include "guis/GuiMsgBox.h"
#include "AudioManager.h"
#include "platform.h"
#include "Log.h"
#include "Window.h"
#include "EmulationStation.h"
#include "Settings.h"
#include "utils/Paths.h"
#include "Util.h"
#include <cstdlib>
#include "ScraperCmdLine.h"
#include <sstream>
#include <clocale>
#include <vector>

namespace fs = std::filesystem;

bool scrape_cmdline = false;

bool parseArgs(int argc, char* argv[], unsigned int* width, unsigned int* height)
{
	for(int i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "--resolution") == 0)
		{
			if(i >= argc - 2)
			{
				std::cerr << "Invalid resolution supplied.";
				return false;
			}

			*width = parseInt(argv[i + 1]);
			*height = parseInt(argv[i + 2]);
			i += 2; // skip the argument value
		}
		else if(strcmp(argv[i], "--gamelist-only") == 0)
		{
			Settings::getInstance()->setBool("ParseGamelistOnly", true);
		}
		else if(strcmp(argv[i], "--ignore-gamelist") == 0)
		{
			Settings::getInstance()->setBool("IgnoreGamelist", true);
		}
		else if(strcmp(argv[i], "--draw-framerate") == 0)
		{
			Settings::getInstance()->setBool("DrawFramerate", true);
		}
		else if(strcmp(argv[i], "--no-exit") == 0)
		{
			Settings::getInstance()->setBool("ShowExit", false);
		}
		else if(strcmp(argv[i], "--debug") == 0)
		{
			Settings::getInstance()->setBool("Debug", true);
			Log::setReportingLevel(LogDebug);
		}
		else if(strcmp(argv[i], "--windowed") == 0)
		{
			Settings::getInstance()->setBool("Windowed", true);
		}
		else if(strcmp(argv[i], "--vsync") == 0)
		{
			bool vsync = (strcmp(argv[i + 1], "on") == 0 || strcmp(argv[i + 1], "1") == 0) ? true : false;
			Settings::getInstance()->setBool("VSync", vsync);
			i++; // skip vsync value
		}
		else if(strcmp(argv[i], "--home") == 0)
		{
			// handled by applyHomeOverride() before anything reads a file
			if(i >= argc - 1)
			{
				std::cerr << "Missing folder for --home.\n";
				return false;
			}
			i++;
		}
		else if(strcmp(argv[i], "--scrape") == 0)
		{
			scrape_cmdline = true;
		}
		else if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
		{
			std::cout << "EmulationStation, a graphical front-end for ROM browsing.\n"
						 "Written by Alec \"Aloshi\" Lofquist.\n"
						 "Version "
					  << PROGRAM_VERSION_STRING << ", built " << PROGRAM_BUILT_STRING
					  << "\n\n"
						 "Command line arguments:\n"
						 "--resolution [width] [height]	try and force a particular resolution\n"
						 "--gamelist-only			skip automatic game search, only read from gamelist.xml\n"
						 "--ignore-gamelist		ignore the gamelist (useful for troubleshooting)\n"
						 "--draw-framerate		display the framerate\n"
						 "--no-exit			don't show the exit option in the menu\n"
						 "--debug				more logging\n"
						 "--scrape			scrape using command line interface\n"
						 "--windowed			not fullscreen, should be used with --resolution\n"
						 "--vsync [1/on or 0/off]		turn vsync on or off (default is on)\n"
						 "--home [folder]			portable mode: keep every file in [folder] (same as ES_HOME)\n"
						 "--help, -h			summon a sentient, angry tuba\n\n"
						 "More information available in README.md.\n";
			return false; //exit after printing help
		}
	}

	return true;
}

// Must run before anything reads or writes a file (Settings is used while parsing arguments).
void applyHomeOverride(int argc, char* argv[])
{
	const char* envHome = std::getenv("ES_HOME");
	if(envHome && *envHome)
		es::paths::setHome(envHome);

	for(int i = 1; i < argc - 1; i++)
	{
		if(strcmp(argv[i], "--home") == 0)
			es::paths::setHome(argv[i + 1]);
	}
}

bool verifyHomeFolderExists()
{
	std::error_code ec;
	for(auto kind : {es::paths::Kind::Config, es::paths::Kind::Data, es::paths::Kind::State, es::paths::Kind::Cache})
	{
		const fs::path dir = es::paths::userDir(kind);
		if(!fs::is_directory(dir, ec))
		{
			std::cerr << "Could not create " << dir.string() << "\n";
			return false;
		}
	}
	return true;
}

// Returns true if everything is OK,
bool loadSystemConfigFile(const char** errorString)
{
	*errorString = nullptr;

	if(!SystemData::loadConfig())
	{
		LOG(LogError) << "Error while parsing systems configuration file!";
		*errorString = "IT LOOKS LIKE YOUR SYSTEMS CONFIGURATION FILE HAS NOT BEEN SET UP OR IS INVALID. YOU'LL NEED "
					   "TO DO THIS BY HAND, UNFORTUNATELY.\n\n"
					   "VISIT EMULATIONSTATION.ORG FOR MORE INFORMATION.";
		return false;
	}

	if(SystemData::sSystemVector.size() == 0)
	{
		LOG(LogError) << "No systems found! Does at least one system have a game present? (check that extensions "
						 "match!)\n(Also, make sure you've updated your es_systems.cfg for XML!)";
		*errorString = "WE CAN'T FIND ANY SYSTEMS!\n"
					   "CHECK THAT YOUR PATHS ARE CORRECT IN THE SYSTEMS CONFIGURATION FILE, "
					   "AND YOUR GAME DIRECTORY HAS AT LEAST ONE GAME WITH THE CORRECT EXTENSION.\n\n"
					   "VISIT EMULATIONSTATION.ORG FOR MORE INFORMATION.";
		return false;
	}

	return true;
}

//called on exit, assuming we get far enough to have the log initialized
void onExit()
{
	Log::close();
}

int main(int argc, char* argv[])
{
	unsigned int width = 0;
	unsigned int height = 0;

	applyUserLocale();

	applyHomeOverride(argc, argv);
	const std::vector<fs::path> migratedFiles = es::paths::migrateFromLegacy();

	if(!parseArgs(argc, argv, &width, &height))
		return 0;

	//if the XDG folders don't exist and cannot be created, bail
	if(!verifyHomeFolderExists())
		return 1;

	//start the logger
	Log::open();
	LOG(LogInfo) << "EmulationStation - v" << PROGRAM_VERSION_STRING << ", built " << PROGRAM_BUILT_STRING;
	for(const auto& file : migratedFiles)
		Log::write(LogInfo, "Copied legacy file to {}", file.string());

	//always close the log on exit
	atexit(&onExit);

	Window window;
	ViewController::init(&window);

	if(!scrape_cmdline)
	{
		if(!window.init(width, height))
		{
			LOG(LogError) << "Window failed to initialize!";
			return 1;
		}

		std::string glExts = (const char*)glGetString(GL_EXTENSIONS);
		LOG(LogInfo) << "Checking available OpenGL extensions...";
		LOG(LogInfo) << " ARB_texture_non_power_of_two: "
					 << (glExts.find("ARB_texture_non_power_of_two") != std::string::npos ? "ok" : "MISSING");

		window.renderLoadingScreen();
	}

	const char* errorMsg = nullptr;
	if(!loadSystemConfigFile(&errorMsg))
	{
		// something went terribly wrong
		if(errorMsg == nullptr)
		{
			LOG(LogError) << "Unknown error occured while parsing system config file.";
			if(!scrape_cmdline)
				Renderer::deinit();
			return 1;
		}

		// we can't handle es_systems.cfg file problems inside ES itself, so display the error message then quit
		window.pushGui(std::make_unique<GuiMsgBox>(&window, errorMsg, "QUIT", [] {
			SDL_Event quit{};
			quit.type = SDL_EVENT_QUIT;
			SDL_PushEvent(&quit); // SDL copies the event
		}));
	}

	//run the command line scraper then quit
	if(scrape_cmdline)
	{
		return run_scraper_cmdline();
	}

	//dont generate joystick events while we're loading (hopefully fixes "automatically started emulator" bug)
	SDL_SetJoystickEventsEnabled(false);

	// preload what we can right away instead of waiting for the user to select it
	// this makes for no delays when accessing content, but a longer startup time
	ViewController::get()->preload();

	//choose which GUI to open depending on if an input configuration already exists
	if(errorMsg == nullptr)
	{
		// a configured device comes from es_input.cfg, or is a gamepad SDL knows
		if(InputManager::getInstance()->getNumConfiguredDevices() > 0)
		{
			ViewController::get()->goToStart();
		}
		else
		{
			window.pushGui(std::make_unique<GuiDetectDevice>(&window, true, [] { ViewController::get()->goToStart(); }));
		}
	}

	//generate joystick events since we're done loading
	SDL_SetJoystickEventsEnabled(true);

	Uint64 lastTime = SDL_GetTicks();
	bool running = true;

	while(running)
	{
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
			case SDL_EVENT_JOYSTICK_HAT_MOTION:
			case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
			case SDL_EVENT_JOYSTICK_BUTTON_UP:
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
			case SDL_EVENT_JOYSTICK_AXIS_MOTION:
			case SDL_EVENT_TEXT_INPUT:
			case SDL_EVENT_TEXT_EDITING:
			case SDL_EVENT_JOYSTICK_ADDED:
			case SDL_EVENT_JOYSTICK_REMOVED: InputManager::getInstance()->parseEvent(event, &window); break;
			case SDL_EVENT_QUIT: running = false; break;
			}
		}

		if(window.isSleeping())
		{
			lastTime = SDL_GetTicks();
			SDL_Delay(
				1); // this doesn't need to be accurate, we're just giving up our CPU time until something wakes us up
			continue;
		}

		Uint64 curTime = SDL_GetTicks();
		int deltaTime = (int)(curTime - lastTime);
		lastTime = curTime;

		// cap deltaTime at 1000
		if(deltaTime > 1000 || deltaTime < 0)
			deltaTime = 1000;

		window.update(deltaTime);
		window.render();
		Renderer::swapBuffers();

		Log::flush();
	}

	while(window.peekGui() != ViewController::get())
		delete window.peekGui();
	window.deinit();

	SystemData::deleteSystems();

	LOG(LogInfo) << "EmulationStation cleanly shutting down.";

	return 0;
}
