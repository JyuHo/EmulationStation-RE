#include "InputManager.h"
#include "utils/Paths.h"
#include <cassert>
#include "InputConfig.h"
#include "Settings.h"
#include "Window.h"
#include "Log.h"
#include "pugixml/pugixml.hpp"
#include <algorithm>
#include <filesystem>
#include "platform.h"
#include "Renderer.h"

#define KEYBOARD_GUID_STRING "-1"

// Joysticks are identified by their SDL_JoystickID (instance ID), which stays the same while the
// device is plugged in. InputConfig's "device ID" is that instance ID, or DEVICE_KEYBOARD (-1);
// SDL never hands out 0 or IDs beyond INT_MAX in practice, so an int holds both.
// The joystick GUID (vendor, product, version...) identifies the model across runs and is what
// es_input.cfg uses to find the configuration of a device.

namespace fs = std::filesystem;

std::unique_ptr<InputManager> InputManager::sInstance;

InputManager::InputManager() = default;

InputManager::~InputManager()
{
	deinit();
}

InputManager* InputManager::getInstance()
{
	if(!sInstance)
		sInstance.reset(new InputManager()); // private constructor: std::make_unique cannot reach it

	return sInstance.get();
}

void InputManager::init()
{
	if(initialized())
		deinit();

	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
		Settings::getInstance()->getBool("BackgroundJoystickInput") ? "1" : "0");
	if(!SDL_InitSubSystem(SDL_INIT_JOYSTICK))
		LOG(LogError) << "Error initializing SDL joysticks: " << SDL_GetError();
	SDL_SetJoystickEventsEnabled(true);

	// first, open all currently present joysticks
	int numJoysticks = 0;
	SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks);
	for(int i = 0; i < numJoysticks; i++)
	{
		addJoystick(joysticks[i]);
	}
	SDL_free(joysticks);

	mKeyboardInputConfig = std::make_unique<InputConfig>(DEVICE_KEYBOARD, "Keyboard", KEYBOARD_GUID_STRING);
	loadInputConfig(mKeyboardInputConfig.get());
}

void InputManager::addJoystick(SDL_JoystickID joyId)
{
	if(mJoysticks.contains(joyId))
		return; // already opened by init(), its "added" event comes afterwards

	// open joystick & add to our list
	SDL_Joystick* joy = SDL_OpenJoystick(joyId);
	if(joy == nullptr)
	{
		LOG(LogError) << "Could not open joystick (instance ID: " << joyId << "): " << SDL_GetError();
		return;
	}

	// add it to our list so we can close it again later
	mJoysticks[joyId] = joy;

	const std::string guid = getJoystickGUIDString(joy);
	const char* name = SDL_GetJoystickName(joy);
	const std::string deviceName = name ? name : "";

	// create the InputConfig
	mInputConfigs[joyId] = std::make_unique<InputConfig>((int)joyId, deviceName, guid);
	if(!loadInputConfig(mInputConfigs[joyId].get()))
	{
		LOG(LogInfo) << "Added unconfigured joystick " << deviceName << " (GUID: " << guid << ", instance ID: " << joyId
					 << ").";
	}
	else
	{
		LOG(LogInfo) << "Added known joystick " << deviceName << " (instance ID: " << joyId << ")";
	}

	// set up the prevAxisValues
	int numAxes = std::max(SDL_GetNumJoystickAxes(joy), 0);
	mPrevAxisValues[joyId] = std::make_unique<int[]>(numAxes);
	std::fill(mPrevAxisValues[joyId].get(), mPrevAxisValues[joyId].get() + numAxes, 0); //initialize array to 0
}

void InputManager::removeJoystick(SDL_JoystickID joyId)
{
	assert(joyId != 0);

	// delete old prevAxisValues
	mPrevAxisValues.erase(joyId);

	// delete old InputConfig
	mInputConfigs.erase(joyId);

	// close the joystick
	auto joyIt = mJoysticks.find(joyId);
	if(joyIt != mJoysticks.end())
	{
		SDL_CloseJoystick(joyIt->second);
		mJoysticks.erase(joyIt);
	}
	else
	{
		LOG(LogError) << "Could not find joystick to close (instance ID: " << joyId << ")";
	}
}

void InputManager::deinit()
{
	if(!initialized())
		return;

	for(auto iter = mJoysticks.begin(); iter != mJoysticks.end(); iter++)
	{
		SDL_CloseJoystick(iter->second);
	}
	mJoysticks.clear();

	mInputConfigs.clear();

	mPrevAxisValues.clear();

	mKeyboardInputConfig.reset();

	SDL_SetJoystickEventsEnabled(false);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

int InputManager::getNumJoysticks()
{
	return mJoysticks.size();
}
int InputManager::getButtonCountByDevice(int id)
{
	if(id == DEVICE_KEYBOARD)
		return 120; //it's a lot, okay.

	auto it = mJoysticks.find((SDL_JoystickID)id);
	return it == mJoysticks.end() ? 0 : SDL_GetNumJoystickButtons(it->second);
}

InputConfig* InputManager::getInputConfigByDevice(int device)
{
	if(device == DEVICE_KEYBOARD)
		return mKeyboardInputConfig.get();
	else
		return mInputConfigs[(SDL_JoystickID)device].get();
}

bool InputManager::parseEvent(const SDL_Event& ev, Window* window)
{
	bool causedEvent = false;
	switch(ev.type)
	{
	case SDL_EVENT_JOYSTICK_AXIS_MOTION: {
		const int device = (int)ev.jaxis.which;
		//if it switched boundaries
		if((abs(ev.jaxis.value) > DEADZONE) != (abs(mPrevAxisValues[ev.jaxis.which][ev.jaxis.axis]) > DEADZONE))
		{
			int normValue;
			if(abs(ev.jaxis.value) <= DEADZONE)
				normValue = 0;
			else if(ev.jaxis.value > 0)
				normValue = 1;
			else
				normValue = -1;

			window->input(getInputConfigByDevice(device), Input(device, TYPE_AXIS, ev.jaxis.axis, normValue, false));
			causedEvent = true;
		}

		mPrevAxisValues[ev.jaxis.which][ev.jaxis.axis] = ev.jaxis.value;
		return causedEvent;
	}

	case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
	case SDL_EVENT_JOYSTICK_BUTTON_UP:
		window->input(getInputConfigByDevice((int)ev.jbutton.which),
			Input((int)ev.jbutton.which, TYPE_BUTTON, ev.jbutton.button, ev.jbutton.down ? 1 : 0, false));
		return true;

	case SDL_EVENT_JOYSTICK_HAT_MOTION:
		window->input(getInputConfigByDevice((int)ev.jhat.which),
			Input((int)ev.jhat.which, TYPE_HAT, ev.jhat.hat, ev.jhat.value, false));
		return true;

	case SDL_EVENT_KEY_DOWN:
		if(ev.key.key == SDLK_BACKSPACE && Renderer::getSDLWindow() && SDL_TextInputActive(Renderer::getSDLWindow()))
		{
			window->textInput("\b");
		}

		if(ev.key.repeat)
			return false;

		if(ev.key.key == SDLK_F4)
		{
			SDL_Event quit{};
			quit.type = SDL_EVENT_QUIT;
			SDL_PushEvent(&quit); // SDL copies the event
			return false;
		}

		window->input(
			getInputConfigByDevice(DEVICE_KEYBOARD), Input(DEVICE_KEYBOARD, TYPE_KEY, (int)ev.key.key, 1, false));
		return true;

	case SDL_EVENT_KEY_UP:
		window->input(
			getInputConfigByDevice(DEVICE_KEYBOARD), Input(DEVICE_KEYBOARD, TYPE_KEY, (int)ev.key.key, 0, false));
		return true;

	case SDL_EVENT_TEXT_INPUT: window->textInput(ev.text.text); break;

	case SDL_EVENT_JOYSTICK_ADDED: addJoystick(ev.jdevice.which); return true;

	case SDL_EVENT_JOYSTICK_REMOVED: removeJoystick(ev.jdevice.which); return false;
	}

	return false;
}

bool InputManager::loadInputConfig(InputConfig* config)
{
	std::string path = getConfigPath();
	if(!fs::exists(path))
		return false;

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());

	if(!res)
	{
		LOG(LogError) << "Error parsing input config: " << res.description();
		return false;
	}

	pugi::xml_node root = doc.child("inputList");
	if(!root)
		return false;

	pugi::xml_node configNode =
		root.find_child_by_attribute("inputConfig", "deviceGUID", config->getDeviceGUIDString().c_str());
	if(!configNode)
		configNode = root.find_child_by_attribute("inputConfig", "deviceName", config->getDeviceName().c_str());
	if(!configNode)
		return false;

	config->loadFromXML(configNode);
	return true;
}

//used in an "emergency" where no keyboard config could be loaded from the inputmanager config file
//allows the user to select to reconfigure in menus if this happens without having to delete es_input.cfg manually
void InputManager::loadDefaultKBConfig()
{
	InputConfig* cfg = getInputConfigByDevice(DEVICE_KEYBOARD);

	cfg->clear();
	cfg->mapInput("up", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_UP, 1, true));
	cfg->mapInput("down", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_DOWN, 1, true));
	cfg->mapInput("left", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFT, 1, true));
	cfg->mapInput("right", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHT, 1, true));

	cfg->mapInput("a", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RETURN, 1, true));
	cfg->mapInput("b", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_ESCAPE, 1, true));
	cfg->mapInput("start", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_F1, 1, true));
	cfg->mapInput("select", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_F2, 1, true));

	cfg->mapInput("pageup", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHTBRACKET, 1, true));
	cfg->mapInput("pagedown", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFTBRACKET, 1, true));
}

void InputManager::writeDeviceConfig(InputConfig* config)
{
	assert(initialized());

	std::string path = getConfigPath();

	pugi::xml_document doc;

	if(fs::exists(path))
	{
		// merge files
		pugi::xml_parse_result result = doc.load_file(path.c_str());
		if(!result)
		{
			LOG(LogError) << "Error parsing input config: " << result.description();
		}
		else
		{
			// successfully loaded, delete the old entry if it exists
			pugi::xml_node root = doc.child("inputList");
			if(root)
			{
				pugi::xml_node oldEntry =
					root.find_child_by_attribute("inputConfig", "deviceGUID", config->getDeviceGUIDString().c_str());
				if(oldEntry)
					root.remove_child(oldEntry);
				oldEntry = root.find_child_by_attribute("inputConfig", "deviceName", config->getDeviceName().c_str());
				if(oldEntry)
					root.remove_child(oldEntry);
			}
		}
	}

	pugi::xml_node root = doc.child("inputList");
	if(!root)
		root = doc.append_child("inputList");

	config->writeToXML(root);
	doc.save_file(path.c_str());
}

std::string InputManager::getConfigPath()
{
	return es::paths::userFile(es::paths::Kind::Config, "es_input.cfg").string();
}

bool InputManager::initialized() const
{
	return mKeyboardInputConfig != nullptr;
}

int InputManager::getNumConfiguredDevices()
{
	int num = 0;
	for(auto it = mInputConfigs.begin(); it != mInputConfigs.end(); it++)
	{
		if(it->second->isConfigured())
			num++;
	}

	if(mKeyboardInputConfig->isConfigured())
		num++;

	return num;
}

std::string InputManager::getDeviceGUIDString(int deviceId)
{
	if(deviceId == DEVICE_KEYBOARD)
		return KEYBOARD_GUID_STRING;

	auto it = mJoysticks.find((SDL_JoystickID)deviceId);
	if(it == mJoysticks.end())
	{
		LOG(LogError) << "getDeviceGUIDString - deviceId " << deviceId << " not found!";
		return "something went horribly wrong";
	}

	return getJoystickGUIDString(it->second);
}

std::string InputManager::getJoystickGUIDString(SDL_Joystick* joystick)
{
	char guid[33]; // 32 hex digits
	SDL_GUIDToString(SDL_GetJoystickGUID(joystick), guid, sizeof(guid));
	return guid;
}
