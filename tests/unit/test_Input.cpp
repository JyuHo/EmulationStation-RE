// Characterization of the input layer before the SDL3 migration (phase 5): what reaches the
// GUI for each SDL event, and the es_input.cfg format, which must stay readable.
#include "InputConfig.h"
#include "InputManager.h"
#include "Settings.h"
#include "Window.h"
#include "utils/Paths.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <SDL3/SDL.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Same mapping as tests/smoke/capture.sh: arrows, Return, Escape, F1, F2, ']' and '['.
constexpr const char* kKeyboardConfig = R"(<?xml version="1.0"?>
<inputList>
	<inputConfig type="keyboard" deviceName="Keyboard" deviceGUID="-1">
		<input name="up" type="key" id="1073741906" value="1" />
		<input name="down" type="key" id="1073741905" value="1" />
		<input name="left" type="key" id="1073741904" value="1" />
		<input name="right" type="key" id="1073741903" value="1" />
		<input name="a" type="key" id="13" value="1" />
		<input name="b" type="key" id="27" value="1" />
		<input name="start" type="key" id="1073741882" value="1" />
		<input name="select" type="key" id="1073741883" value="1" />
		<input name="pageup" type="key" id="93" value="1" />
		<input name="pagedown" type="key" id="91" value="1" />
	</inputConfig>
</inputList>
)";

struct RecordingGui : GuiComponent
{
	explicit RecordingGui(Window* window) : GuiComponent(window) {}
	bool input(InputConfig* /*config*/, Input input) override
	{
		inputs.push_back(input);
		return true;
	}
	void textInput(const char* text) override { this->text += text; }

	std::vector<Input> inputs;
	std::string text;
};

// A portable ES home with an optional es_input.cfg; InputManager is initialized on it and
// torn down at the end of the test.
struct InputFixture
{
	explicit InputFixture(const char* inputConfig = nullptr)
	{
		home = fs::temp_directory_path() / ("es-test-input-" + std::to_string(::getpid()));
		fs::remove_all(home);
		fs::create_directories(home / "config/emulationstation");
		if(inputConfig)
			std::ofstream(home / "config/emulationstation/es_input.cfg") << inputConfig;
		es::paths::setHome(home);
		InputManager::getInstance()->init();
		auto gui = std::make_unique<RecordingGui>(&window);
		recorder = gui.get();
		window.pushGui(std::move(gui));
	}

	~InputFixture()
	{
		InputManager::getInstance()->deinit();
		es::paths::clearHome();
		fs::remove_all(home);
	}

	// Dispatches every pending event the way main() does; returns true if a quit event was seen.
	bool pump()
	{
		bool quit = false;
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			if(event.type == SDL_EVENT_QUIT)
				quit = true;
			InputManager::getInstance()->parseEvent(event, &window);
		}
		return quit;
	}

	void pushKey(Uint32 type, SDL_Keycode key, bool repeat = false)
	{
		SDL_Event event{};
		event.type = type;
		event.key.key = key;
		event.key.repeat = repeat;
		event.key.down = type == SDL_EVENT_KEY_DOWN;
		REQUIRE(SDL_PushEvent(&event));
	}

	fs::path home;
	Window window;
	RecordingGui* recorder = nullptr;
};

bool sameInput(const Input& a, const Input& b)
{
	return a.device == b.device && a.type == b.type && a.id == b.id && a.value == b.value;
}

} // namespace

TEST_CASE("es_input.cfg keyboard mappings use SDL keycodes", "[Input]")
{
	InputFixture fixture(kKeyboardConfig);
	InputConfig* keyboard = InputManager::getInstance()->getInputConfigByDevice(DEVICE_KEYBOARD);
	REQUIRE(keyboard != nullptr);
	CHECK(keyboard->isConfigured());
	CHECK(InputManager::getInstance()->getNumConfiguredDevices() == 1);

	CHECK(keyboard->isMappedTo("up", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_UP, 1, false)));
	CHECK(keyboard->isMappedTo("down", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_DOWN, 1, false)));
	CHECK(keyboard->isMappedTo("left", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFT, 1, false)));
	CHECK(keyboard->isMappedTo("right", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHT, 1, false)));
	CHECK(keyboard->isMappedTo("a", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RETURN, 1, false)));
	CHECK(keyboard->isMappedTo("b", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_ESCAPE, 1, false)));
	CHECK(keyboard->isMappedTo("start", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_F1, 1, false)));
	CHECK(keyboard->isMappedTo("select", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_F2, 1, false)));
	CHECK(keyboard->isMappedTo("pageup", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHTBRACKET, 1, false)));
	CHECK(keyboard->isMappedTo("pagedown", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFTBRACKET, 1, false)));
	CHECK(keyboard->getMappedTo(Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RETURN, 1, false)) ==
		std::vector<std::string>{"a"});
}

TEST_CASE("writing the keyboard configuration keeps the file format", "[Input]")
{
	InputFixture fixture(kKeyboardConfig);
	InputManager::getInstance()->writeDeviceConfig(
		InputManager::getInstance()->getInputConfigByDevice(DEVICE_KEYBOARD));

	pugi::xml_document doc;
	REQUIRE(doc.load_file(InputManager::getConfigPath().c_str()));
	pugi::xml_node config = doc.child("inputList").child("inputConfig");
	REQUIRE(config);
	CHECK(std::string(config.attribute("type").value()) == "keyboard");
	CHECK(std::string(config.attribute("deviceName").value()) == "Keyboard");
	CHECK(std::string(config.attribute("deviceGUID").value()) == "-1");
	pugi::xml_node up = config.find_child_by_attribute("input", "name", "up");
	REQUIRE(up);
	CHECK(std::string(up.attribute("type").value()) == "key");
	CHECK(std::string(up.attribute("id").value()) == "1073741906");
	CHECK(std::string(up.attribute("value").value()) == "1");
	CHECK(!config.next_sibling("inputConfig"));
}

TEST_CASE("inputs describe themselves for the configuration screens", "[Input]")
{
	CHECK(Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RETURN, 1, true).string() == "Key Return");
	CHECK(Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_UP, 1, true).string() == "Key Up");
	CHECK(Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_A, 1, true).string() == "Key A");
	CHECK(Input(0, TYPE_BUTTON, 3, 1, true).string() == "Button 3");
	CHECK(Input(0, TYPE_AXIS, 1, 1, true).string() == "Axis 1+");
	CHECK(Input(0, TYPE_AXIS, 1, -1, true).string() == "Axis 1-");
	CHECK(Input(0, TYPE_HAT, 0, SDL_HAT_UP, true).string() == "Hat 0 up");
	CHECK(Input(0, TYPE_HAT, 0, SDL_HAT_LEFT, true).string() == "Hat 0 left");
}

TEST_CASE("key events reach the GUI on top of the window", "[Input]")
{
	InputFixture fixture(kKeyboardConfig);
	fixture.pushKey(SDL_EVENT_KEY_DOWN, SDLK_RETURN);
	fixture.pushKey(SDL_EVENT_KEY_DOWN, SDLK_RETURN, true); // key repeat is ignored
	fixture.pushKey(SDL_EVENT_KEY_UP, SDLK_RETURN);
	CHECK(!fixture.pump());

	const auto& inputs = fixture.recorder->inputs;
	REQUIRE(inputs.size() == 2);
	CHECK(sameInput(inputs[0], Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RETURN, 1, false)));
	CHECK(sameInput(inputs[1], Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RETURN, 0, false)));
}

TEST_CASE("F4 quits instead of reaching the GUI", "[Input]")
{
	InputFixture fixture(kKeyboardConfig);
	fixture.pushKey(SDL_EVENT_KEY_DOWN, SDLK_F4);
	// the quit event is pushed while the queue is being read: it may only show up on the next frame
	const bool quit = fixture.pump();
	CHECK((fixture.pump() || quit));
	CHECK(fixture.recorder->inputs.empty());
}

TEST_CASE("text input events reach the GUI as UTF-8", "[Input]")
{
	InputFixture fixture(kKeyboardConfig);
	SDL_Event event{};
	event.type = SDL_EVENT_TEXT_INPUT;
	event.text.text = "é";
	InputManager::getInstance()->parseEvent(event, &fixture.window);
	CHECK(fixture.recorder->text == "é");
}

TEST_CASE("a joystick can be plugged, used and unplugged", "[Input]")
{
	InputFixture fixture(kKeyboardConfig);
	InputManager* im = InputManager::getInstance();
	const int before = im->getNumJoysticks();

	SDL_VirtualJoystickDesc desc;
	SDL_INIT_INTERFACE(&desc);
	desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
	desc.naxes = 2;
	desc.nbuttons = 4;
	desc.nhats = 1;
	const SDL_JoystickID virtualId = SDL_AttachVirtualJoystick(&desc);
	REQUIRE(virtualId != 0);
	fixture.pump();
	REQUIRE(im->getNumJoysticks() == before + 1);

	SDL_Joystick* joystick = SDL_OpenJoystick(virtualId);
	REQUIRE(joystick != nullptr);
	const int id = (int)SDL_GetJoystickID(joystick);
	InputConfig* config = im->getInputConfigByDevice(id);
	REQUIRE(config != nullptr);
	CHECK(config->getDeviceId() == id);
	CHECK(!config->isConfigured()); // not in es_input.cfg, and too few buttons for a default gamepad mapping
	CHECK(im->getDeviceGUIDString(id) == config->getDeviceGUIDString());
	CHECK(config->getDeviceGUIDString().size() == 32);
	CHECK(im->getButtonCountByDevice(id) == 4);

	auto& inputs = fixture.recorder->inputs;
	auto expectOnly = [&](const Input& expected) {
		fixture.pump();
		REQUIRE(inputs.size() == 1);
		CHECK(sameInput(inputs[0], expected));
		inputs.clear();
	};

	// axes become -1/0/1 around a dead zone, and only boundary crossings are reported
	SDL_SetJoystickVirtualAxis(joystick, 1, 30000);
	expectOnly(Input(id, TYPE_AXIS, 1, 1, false));
	SDL_SetJoystickVirtualAxis(joystick, 1, 25000);
	fixture.pump();
	CHECK(inputs.empty());
	SDL_SetJoystickVirtualAxis(joystick, 1, 1000);
	expectOnly(Input(id, TYPE_AXIS, 1, 0, false));
	SDL_SetJoystickVirtualAxis(joystick, 1, -30000);
	expectOnly(Input(id, TYPE_AXIS, 1, -1, false));

	SDL_SetJoystickVirtualButton(joystick, 2, true);
	expectOnly(Input(id, TYPE_BUTTON, 2, 1, false));
	SDL_SetJoystickVirtualButton(joystick, 2, false);
	expectOnly(Input(id, TYPE_BUTTON, 2, 0, false));

	SDL_SetJoystickVirtualHat(joystick, 0, SDL_HAT_LEFT);
	expectOnly(Input(id, TYPE_HAT, 0, SDL_HAT_LEFT, false));
	SDL_SetJoystickVirtualHat(joystick, 0, SDL_HAT_CENTERED);
	expectOnly(Input(id, TYPE_HAT, 0, SDL_HAT_CENTERED, false));

	SDL_CloseJoystick(joystick);
	REQUIRE(SDL_DetachVirtualJoystick(virtualId));
	fixture.pump();
	CHECK(im->getNumJoysticks() == before);
	// unplugging recenters the axis that was still pushed
	REQUIRE(inputs.size() == 1);
	CHECK(sameInput(inputs[0], Input(id, TYPE_AXIS, 1, 0, false)));
}

namespace {

// A virtual gamepad whose joystick button i is gamepad button i (SOUTH = 0 ... DPAD_RIGHT = 14).
SDL_JoystickID attachVirtualGamepad()
{
	SDL_VirtualJoystickDesc desc;
	SDL_INIT_INTERFACE(&desc);
	desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
	desc.nbuttons = SDL_GAMEPAD_BUTTON_DPAD_RIGHT + 1;
	desc.button_mask = (1u << desc.nbuttons) - 1;
	desc.name = "ES test gamepad";
	return SDL_AttachVirtualJoystick(&desc);
}

} // namespace

TEST_CASE("a recognized gamepad gets a default configuration", "[Input]")
{
	const bool swapAB = GENERATE(false, true);
	InputFixture fixture(kKeyboardConfig);
	Settings::getInstance()->setBool("GamepadSwapAB", swapAB);
	InputManager* im = InputManager::getInstance();

	const SDL_JoystickID virtualId = attachVirtualGamepad();
	REQUIRE(virtualId != 0);
	fixture.pump();
	REQUIRE(SDL_IsGamepad(virtualId));

	const int id = (int)virtualId;
	InputConfig* config = im->getInputConfigByDevice(id);
	REQUIRE(config != nullptr);
	CHECK(config->isConfigured());
	CHECK(im->getNumConfiguredDevices() == 2); // keyboard and gamepad

	auto button = [id](SDL_GamepadButton b) {
		return Input(id, TYPE_BUTTON, b, 1, false);
	};
	CHECK(config->isMappedTo("a", button(swapAB ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_SOUTH)));
	CHECK(config->isMappedTo("b", button(swapAB ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST)));
	CHECK(config->isMappedTo("start", button(SDL_GAMEPAD_BUTTON_START)));
	CHECK(config->isMappedTo("select", button(SDL_GAMEPAD_BUTTON_BACK)));
	CHECK(config->isMappedTo("pageup", button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)));
	CHECK(config->isMappedTo("pagedown", button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)));
	CHECK(config->isMappedTo("up", button(SDL_GAMEPAD_BUTTON_DPAD_UP)));
	CHECK(config->isMappedTo("down", button(SDL_GAMEPAD_BUTTON_DPAD_DOWN)));
	CHECK(config->isMappedTo("left", button(SDL_GAMEPAD_BUTTON_DPAD_LEFT)));
	CHECK(config->isMappedTo("right", button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)));

	// its buttons arrive as joystick inputs, which the GUI reads through the configuration
	SDL_Joystick* joystick = SDL_OpenJoystick(virtualId);
	REQUIRE(joystick != nullptr);
	SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true);
	fixture.pump();
	auto& inputs = fixture.recorder->inputs;
	REQUIRE(inputs.size() == 1);
	CHECK(config->isMappedTo(swapAB ? "b" : "a", inputs[0]));
	SDL_CloseJoystick(joystick);

	REQUIRE(SDL_DetachVirtualJoystick(virtualId));
	fixture.pump();
	CHECK(im->getNumConfiguredDevices() == 1);
}

TEST_CASE("es_input.cfg wins over the default gamepad configuration", "[Input]")
{
	InputFixture fixture(kKeyboardConfig);
	InputManager* im = InputManager::getInstance();
	const SDL_JoystickID virtualId = attachVirtualGamepad();
	REQUIRE(virtualId != 0);
	fixture.pump();

	// the user maps "a" to the north button and saves
	InputConfig* config = im->getInputConfigByDevice((int)virtualId);
	REQUIRE(config != nullptr);
	config->mapInput("a", Input((int)virtualId, TYPE_BUTTON, SDL_GAMEPAD_BUTTON_NORTH, 1, true));
	im->writeDeviceConfig(config);

	// plugged again: the saved configuration is used
	REQUIRE(SDL_DetachVirtualJoystick(virtualId));
	fixture.pump();
	const SDL_JoystickID again = attachVirtualGamepad();
	REQUIRE(again != 0);
	fixture.pump();
	config = im->getInputConfigByDevice((int)again);
	REQUIRE(config != nullptr);
	CHECK(config->isMappedTo("a", Input((int)again, TYPE_BUTTON, SDL_GAMEPAD_BUTTON_NORTH, 1, false)));
	CHECK(!config->isMappedTo("a", Input((int)again, TYPE_BUTTON, SDL_GAMEPAD_BUTTON_SOUTH, 1, false)));
	REQUIRE(SDL_DetachVirtualJoystick(again));
	fixture.pump();
}

TEST_CASE("gamecontrollerdb.txt in the configuration directory adds mappings", "[Input]")
{
	// a mapping for a made-up device: "a" on button 5, d-pad on hat 0
	const std::string guid = "03000000aaaa0000bbbb000000000000";
	const std::string mapping = guid +
		",ES test pad,a:b5,b:b4,x:b3,y:b2,back:b1,start:b0,leftshoulder:b6,"
		"rightshoulder:b7,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,platform:Linux,";
	const fs::path home = fs::temp_directory_path() / ("es-test-gcdb-" + std::to_string(::getpid()));
	fs::remove_all(home);
	fs::create_directories(home / "config/emulationstation");
	std::ofstream(home / "config/emulationstation/gamecontrollerdb.txt") << mapping << "\n";
	es::paths::setHome(home);

	InputManager::getInstance()->init();
	char* found = SDL_GetGamepadMappingForGUID(SDL_StringToGUID(guid.c_str()));
	REQUIRE(found != nullptr);
	CHECK(std::string(found).starts_with(guid + ",ES test pad,"));
	SDL_free(found);
	InputManager::getInstance()->deinit();

	es::paths::clearHome();
	fs::remove_all(home);
}
