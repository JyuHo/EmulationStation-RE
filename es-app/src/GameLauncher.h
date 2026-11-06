#pragma once

#include <es-launcher/Launcher.h>

#include <memory>
#include <string>

class FileData;
struct LaunchGameOptions;

// Bridge between ES and es-launcher: picks the emulator and core for a launch and, when
// es-launcher has a generator for that emulator, builds its LaunchContext from ES state.
// Games whose emulator has no generator keep the es_systems.cfg command.
namespace GameLauncher
{
	struct Selection
	{
		std::string emulator;
		std::string core;
	};

	// The emulator and core this launch will use: the game or system choice, overridden by a
	// save state made with another emulator, or by the core a netplay host asked for.
	Selection selectEmulator(FileData* game, const LaunchGameOptions& options);

	// A Launch for this game, or nullptr when es-launcher has no generator for the emulator.
	// Must run before the window is deinitialized: it reads the controllers from SDL.
	std::unique_ptr<es::launcher::Launch> create(FileData* game, const LaunchGameOptions& options);
}
