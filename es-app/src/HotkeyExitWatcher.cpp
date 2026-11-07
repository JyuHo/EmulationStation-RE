#include "HotkeyExitWatcher.h"

#include "Log.h"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <string>

using es::launcher::Controller;
using es::launcher::InputBinding;

namespace
{
	bool sameInput(const InputBinding& a, const InputBinding& b)
	{
		return a.type == b.type && a.id == b.id && a.value == b.value;
	}

	bool isPressed(SDL_Joystick* joystick, const InputBinding& input)
	{
		switch (input.type)
		{
		case InputBinding::Type::Button:
			return SDL_GetJoystickButton(joystick, input.id);
		case InputBinding::Type::Hat:
			return (SDL_GetJoystickHat(joystick, input.id) & input.value) != 0;
		case InputBinding::Type::Axis:
		{
			int value = SDL_GetJoystickAxis(joystick, input.id);
			return input.value < 0 ? value < -16000 : value > 16000;
		}
		case InputBinding::Type::Key:
			break;
		}
		return false;
	}

	// The SDL joystick of a pad ES configured: by device path, else by GUID and index
	SDL_JoystickID findJoystick(const Controller& pad, const SDL_JoystickID* ids, int count)
	{
		for (int i = 0; i < count; ++i)
		{
			const char* path = SDL_GetJoystickPathForID(ids[i]);
			if (path != nullptr && !pad.devicePath.empty() && pad.devicePath == path)
				return ids[i];
		}

		for (int i = 0; i < count; ++i)
		{
			char guid[40];
			SDL_GUIDToString(SDL_GetJoystickGUIDForID(ids[i]), guid, sizeof(guid));
			if (pad.guid == guid && (pad.deviceIndex == i || pad.deviceIndex < 0))
				return ids[i];
		}

		return 0;
	}
}

HotkeyExitWatcher::HotkeyExitWatcher(const std::vector<Controller>& controllers)
{
	// the emulator has the focus, not ES
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	if (!SDL_InitSubSystem(SDL_INIT_JOYSTICK))
	{
		LOG(LogWarning) << "Hotkey exit: cannot open the pads: " << SDL_GetError();
		return;
	}
	mInitialized = true;

	int count = 0;
	SDL_JoystickID* ids = SDL_GetJoysticks(&count);

	for (const Controller& pad : controllers)
	{
		const InputBinding* hotkey = pad.find("hotkey");
		const InputBinding* start = pad.find("start");
		if (hotkey == nullptr || start == nullptr || sameInput(*hotkey, *start))
			continue;
		if (hotkey->type == InputBinding::Type::Key || start->type == InputBinding::Type::Key)
			continue;

		SDL_JoystickID id = ids != nullptr ? findJoystick(pad, ids, count) : 0;
		SDL_Joystick* joystick = id != 0 ? SDL_OpenJoystick(id) : nullptr;
		if (joystick == nullptr)
			continue;

		mPads.push_back({ joystick, *hotkey, *start });
	}

	SDL_free(ids);
	LOG(LogInfo) << "Hotkey exit: watching " << mPads.size() << " pad(s)";
}

HotkeyExitWatcher::~HotkeyExitWatcher()
{
	for (const Pad& pad : mPads)
		SDL_CloseJoystick(pad.joystick);

	if (mInitialized)
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

bool HotkeyExitWatcher::exitRequested()
{
	if (mPads.empty())
		return false;

	SDL_UpdateJoysticks();

	bool pressed = false;
	for (const Pad& pad : mPads)
		pressed = pressed || (isPressed(pad.joystick, pad.hotkey) && isPressed(pad.joystick, pad.start));

	// two polls in a row: a glitch in one reading does not close the game
	mPressedPolls = pressed ? mPressedPolls + 1 : 0;
	return mPressedPolls >= 2;
}
