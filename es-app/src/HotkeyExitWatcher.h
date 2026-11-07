#pragma once

#include <es-launcher/LaunchContext.h>

#include <vector>

struct SDL_Joystick;

// Watches the players' pads while an emulator runs, for the emulators that cannot quit on
// hotkey + Start by themselves (es::launcher::Command::closeOnHotkey). ES's own input handling
// is stopped during a game: this opens the pads again through SDL, reads them with the mapping
// the player made in ES, and lets go of them when it is destroyed.
class HotkeyExitWatcher
{
public:
	explicit HotkeyExitWatcher(const std::vector<es::launcher::Controller>& controllers);
	~HotkeyExitWatcher();

	HotkeyExitWatcher(const HotkeyExitWatcher&) = delete;
	HotkeyExitWatcher& operator=(const HotkeyExitWatcher&) = delete;

	// True once a player has held the hotkey and pressed Start for two polls in a row.
	bool exitRequested();

private:
	struct Pad
	{
		SDL_Joystick* joystick = nullptr;
		es::launcher::InputBinding hotkey;
		es::launcher::InputBinding start;
	};

	std::vector<Pad> mPads;
	int mPressedPolls = 0;
	bool mInitialized = false;
};
