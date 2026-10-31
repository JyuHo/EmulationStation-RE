#ifndef _INPUTMANAGER_H_
#define _INPUTMANAGER_H_

#include <SDL3/SDL.h>
#include <vector>
#include <map>
#include <memory>
#include <string>

class InputConfig;
class Window;

//you should only ever instantiate one of these, by the way
class InputManager
{
private:
	InputManager();

	static std::unique_ptr<InputManager> sInstance;

	static const int DEADZONE = 23000;

	void loadDefaultKBConfig();

	std::map<SDL_JoystickID, SDL_Joystick*> mJoysticks;
	std::map<SDL_JoystickID, SDL_Gamepad*> mGamepads; // the joysticks SDL recognizes as gamepads
	std::map<SDL_JoystickID, std::unique_ptr<InputConfig>> mInputConfigs;
	std::unique_ptr<InputConfig> mKeyboardInputConfig;

	std::map<SDL_JoystickID, std::unique_ptr<int[]>> mPrevAxisValues;

	bool initialized() const;

	void addJoystick(SDL_JoystickID id);
	void removeJoystick(SDL_JoystickID id);
	bool loadInputConfig(InputConfig* config); // returns true if successfully loaded, false if not (or didn't exist)
	static std::string getJoystickGUIDString(SDL_Joystick* joystick);

public:
	virtual ~InputManager();

	static InputManager* getInstance();

	void writeDeviceConfig(InputConfig* config);
	static std::string getConfigPath();

	// Fills config with the joystick inputs behind the standard gamepad buttons (A = south button,
	// or east one if swapAB). Returns false, with config left empty, if a button has no binding.
	static bool mapGamepadDefaults(InputConfig& config, SDL_Gamepad* gamepad, bool swapAB);

	void init();
	void deinit();

	int getNumJoysticks();
	int getButtonCountByDevice(int deviceId);
	int getNumConfiguredDevices();

	std::string getDeviceGUIDString(int deviceId);

	InputConfig* getInputConfigByDevice(int deviceId);

	bool parseEvent(const SDL_Event& ev, Window* window);
};

#endif
