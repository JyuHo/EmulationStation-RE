// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "eden/EdenGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"
#include "util/Gamepad.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace es::launcher::eden
{
	namespace fs = std::filesystem;
	using util::IniFile;
	using util::PadControl;

	ES_LAUNCHER_REGISTER_GENERATOR("eden", EdenGenerator)

	namespace
	{
		constexpr int maxPlayers = 8;

		// Eden button -> ES input
		constexpr std::pair<const char*, const char*> buttons[] = {
			{ "button_a", "a" }, { "button_b", "b" }, { "button_x", "x" }, { "button_y", "y" },
			{ "button_lstick", "l3" }, { "button_rstick", "r3" }, { "button_l", "pageup" }, { "button_r", "pagedown" },
			{ "button_zl", "l2" }, { "button_zr", "r2" }, { "button_plus", "start" }, { "button_minus", "select" },
			{ "button_dleft", "left" }, { "button_dup", "up" }, { "button_dright", "right" }, { "button_ddown", "down" },
		};

		// Switch system language and region by locale (Eden's Language and Region indexes)
		struct Locale
		{
			const char* code;
			int language;
			int region;
		};
		constexpr Locale locales[] = {
			{ "ja_JP", 0, 0 }, { "en_US", 1, 1 }, { "fr_FR", 2, 2 }, { "de_DE", 3, 2 }, { "it_IT", 4, 2 },
			{ "es_ES", 5, 2 }, { "zh_CN", 15, 4 }, { "ko_KR", 7, 5 }, { "nl_NL", 8, 2 }, { "pt_PT", 9, 2 },
			{ "ru_RU", 10, 2 }, { "zh_TW", 16, 6 }, { "en_GB", 12, 2 }, { "fr_CA", 13, 1 }, { "es_MX", 14, 1 },
			{ "pt_BR", 17, 1 }, { "en_AU", 12, 3 }, { "fr_BE", 2, 2 }, { "fr_CH", 2, 2 }, { "de_CH", 3, 2 },
		};

		const Locale* locale()
		{
			const char* lang = std::getenv("LANG");
			std::string_view code = lang != nullptr ? std::string_view(lang).substr(0, 5) : std::string_view();
			for (const Locale& l : locales)
				if (code == l.code)
					return &l;
			return nullptr;
		}

		// Eden's controller name of a button, for its shortcuts ("Home+Plus")
		const char* shortcutName(PadControl control)
		{
			switch (control)
			{
			case PadControl::South: return "B";
			case PadControl::East: return "A";
			case PadControl::West: return "Y";
			case PadControl::North: return "X";
			case PadControl::Back: return "Minus";
			case PadControl::Guide: return "Home";
			case PadControl::Start: return "Plus";
			case PadControl::LeftStick: return "Left_Stick";
			case PadControl::RightStick: return "Right_Stick";
			case PadControl::LeftShoulder: return "L";
			case PadControl::RightShoulder: return "R";
			case PadControl::DPadUp: return "Dpad_Up";
			case PadControl::DPadDown: return "Dpad_Down";
			case PadControl::DPadLeft: return "Dpad_Left";
			case PadControl::DPadRight: return "Dpad_Right";
			case PadControl::LeftTrigger: return "ZL";
			case PadControl::RightTrigger: return "ZR";
			}
			return "";
		}

		// A Qt setting of Eden: the value, and \default=false so Eden reads it
		void setQt(IniFile& ini, std::string_view section, const std::string& key, std::string_view value)
		{
			ini.set(section, key, value);
			ini.set(section, key + "\\default", "false");
		}

		const char* boolString(bool value) { return value ? "true" : "false"; }

		struct Device
		{
			std::string guid;
			int port;
		};

		// Eden finds a pad by GUID, numbered among those of the same GUID in device order
		Device device(const LaunchContext& ctx, const Controller& pad)
		{
			std::string guid = EdenGenerator::edenGuid(pad.guid);
			auto before = std::ranges::count_if(ctx.controllers, [&](const Controller& other) {
				return EdenGenerator::edenGuid(other.guid) == guid && other.deviceIndex < pad.deviceIndex;
			});
			return { guid, static_cast<int>(before) };
		}

		std::string prefix(const Device& device)
		{
			return "engine:sdl,port:" + std::to_string(device.port) + ",guid:" + device.guid;
		}

		const char* hatDirection(int value)
		{
			switch (value)
			{
			case 1: return "up";
			case 2: return "right";
			case 4: return "down";
			case 8: return "left";
			default: return "";
			}
		}

		std::string buttonBinding(const Device& device, const InputBinding* binding)
		{
			if (binding == nullptr)
				return {};

			std::string id = std::to_string(binding->id);
			switch (binding->type)
			{
			case InputBinding::Type::Button:
				return prefix(device) + ",button:" + id;
			case InputBinding::Type::Hat:
				return prefix(device) + ",hat:" + id + ",direction:" + hatDirection(binding->value);
			case InputBinding::Type::Axis:
				return prefix(device) + ",axis:" + id + ",threshold:0.500000,invert:" + (binding->value < 0 ? "-" : "+");
			case InputBinding::Type::Key:
				break;
			}
			return {};
		}

		// "joystick1": its left and up inputs give the axes; "+" when left and up are negative,
		// as on every pad SDL knows
		std::string stickBinding(const Device& device, const Controller& pad, std::string_view stick)
		{
			const InputBinding* x = pad.find(std::string(stick) + "left");
			const InputBinding* y = pad.find(std::string(stick) + "up");
			if (x == nullptr || y == nullptr || x->type != InputBinding::Type::Axis || y->type != InputBinding::Type::Axis)
				return {};
			return prefix(device) + ",axis_x:" + std::to_string(x->id) + ",axis_y:" + std::to_string(y->id)
				+ ",offset_x:0.000000,offset_y:0.000000"
				+ ",invert_x:" + (x->value < 0 ? "+" : "-") + ",invert_y:" + (y->value < 0 ? "+" : "-")
				+ ",deadzone:0.150000,range:1.000000";
		}

		fs::path dataDirectory(const LaunchContext& ctx)
		{
			return ctx.paths.saves / "switch" / "eden";
		}
	}

	std::string EdenGenerator::edenGuid(const std::string& sdlGuid)
	{
		std::string guid = sdlGuid;
		std::ranges::transform(guid, guid.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (guid.size() == 32)
			guid.replace(4, 4, "0000");
		return guid;
	}

	fs::path EdenGenerator::settingsPath(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("eden") / "eden" / "qt-config.ini";
	}

	void EdenGenerator::writeSettings(const LaunchContext& ctx, IniFile& ini)
	{
		const Settings& s = ctx.settings;

		// [Renderer]
		setQt(ini, "Renderer", "backend", s.getString("eden_backend", "1"));
		setQt(ini, "Renderer", "resolution_setup", s.getString("eden_resolution", "3"));
		setQt(ini, "Renderer", "use_vsync", s.getString("eden_vsync", "2"));
		setQt(ini, "Renderer", "aspect_ratio", s.getString("eden_ratio", "0"));
		setQt(ini, "Renderer", "anti_aliasing", s.getString("eden_antialiasing", "0"));
		setQt(ini, "Renderer", "scaling_filter", s.getString("eden_scaling", "1"));
		setQt(ini, "Renderer", "gpu_accuracy", s.getString("eden_accuracy", "1"));

		// [System]: docked by default, as on a TV
		setQt(ini, "System", "use_docked_mode", s.getBool("eden_docked", true) ? "1" : "0");
		if (const Locale* l = locale())
		{
			setQt(ini, "System", "language_index", std::to_string(l->language));
			setQt(ini, "System", "region_index", std::to_string(l->region));
		}

		// [UI]
		setQt(ini, "UI", "fullscreen", "true");
		setQt(ini, "UI", "firstStart", "false");
		setQt(ini, "UI", "confirmStop", "2");   // never ask
		setQt(ini, "UI", "check_for_updates", "false");
		setQt(ini, "UI", "enable_discord_presence", "false");
		setQt(ini, "UI", "hideInactiveMouse", "true");
		setQt(ini, "UI", "select_user_on_boot", "false");
		setQt(ini, "UI", "singleWindowMode", "true");
		ini.set("UI", "Screenshots\\screenshot_path", ctx.paths.screenshots.string());

		// [Controls]
		for (int i = 0; i < maxPlayers; ++i)
		{
			std::string player = "player_" + std::to_string(i) + "_";
			const Controller* pad = i < static_cast<int>(ctx.controllers.size()) ? &ctx.controllers[i] : nullptr;
			setQt(ini, "Controls", player + "connected", boolString(pad != nullptr));
			if (pad == nullptr)
				continue;

			Device dev = device(ctx, *pad);
			setQt(ini, "Controls", player + "type", "0");   // Pro Controller
			setQt(ini, "Controls", player + "vibration_enabled", boolString(s.getBool("eden_rumble" + std::to_string(i + 1), true)));
			for (const auto& [button, input] : buttons)
				setQt(ini, "Controls", player + button, "\"" + buttonBinding(dev, pad->find(input)) + "\"");

			// Home is the hotkey, when it is a button of its own
			bool ownHotkey = util::hotkeyControl(*pad) == PadControl::Guide;
			setQt(ini, "Controls", player + "button_home", "\"" + (ownHotkey ? buttonBinding(dev, pad->find("hotkey")) : std::string()) + "\"");
			setQt(ini, "Controls", player + "button_screenshot", "\"\"");
			setQt(ini, "Controls", player + "lstick", "\"" + stickBinding(dev, *pad, "joystick1") + "\"");
			setQt(ini, "Controls", player + "rstick", "\"" + stickBinding(dev, *pad, "joystick2") + "\"");
		}

		// Shortcuts: player 1's hotkey with the button of each action, as for RetroArch
		const Controller* first = util::firstPlayer(ctx);
		std::string hotkey = shortcutName(first != nullptr ? util::hotkeyControl(*first) : PadControl::Guide);
		auto chord = [&](util::HotkeyAction action) {
			for (const util::Hotkey& h : util::hotkeys)
				if (h.action == action)
					return hotkey + "+" + shortcutName(h.control);
			return std::string();
		};
		const std::pair<const char*, std::string> shortcuts[] = {
			{ "Exit%20Eden", chord(util::HotkeyAction::Exit) },
			{ "Continue\\Pause%20Emulation", chord(util::HotkeyAction::Menu) },
			{ "Restart%20Emulation", chord(util::HotkeyAction::Reset) },
			{ "Capture%20Screenshot", chord(util::HotkeyAction::Screenshot) },
			{ "Toggle%20Turbo%20Speed", chord(util::HotkeyAction::FastForward) },
			// Eden's own chords on those buttons
			{ "Fullscreen", "" }, { "Load\\Remove%20Amiibo", "" }, { "Change%20Adapting%20Filter", "" },
			{ "Audio%20Mute\\Unmute", "" },
		};
		for (const auto& [name, keys] : shortcuts)
			setQt(ini, "UI", std::string("Shortcuts\\Main%20Window\\") + name + "\\Controller_KeySeq", keys);
	}

	Command EdenGenerator::configure(const LaunchContext& ctx)
	{
		if (ctx.architecture != Architecture::X86_64)
			throw LaunchError(ErrorCode::Generic, "Eden only runs on x86_64 computers.");

		fs::path home = ctx.paths.configFor("eden");
		fs::path saves = ctx.paths.saves / "switch";
		fs::path keys = dataDirectory(ctx) / "keys";

		auto program = util::emulatorCommand(ctx, "eden", { "eden", "eden-qt" }, {
			home, saves, ctx.paths.bios, ctx.paths.screenshots, ctx.paths.cache, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "Eden is not installed: install it from the emulator catalog.");

		IniFile ini = IniFile::load(settingsPath(ctx));
		writeSettings(ctx, ini);
		try
		{
			for (const fs::path& dir : { dataDirectory(ctx), ctx.paths.screenshots })
				fs::create_directories(dir);
			ini.save(settingsPath(ctx));
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the Eden configuration: ") + e.what());
		}

		// the keys stay in the BIOS folder
		util::linkDirectory(keys, ctx.paths.bios / "switch");
		std::error_code ec;
		if (!fs::is_regular_file(keys / "prod.keys", ec))
			throw LaunchError(ErrorCode::Generic, "Switch keys missing: copy prod.keys into the switch folder of the BIOS folder.");

		Command cmd;
		cmd.argv = *program;
		cmd.env["XDG_CONFIG_HOME"] = home.string();
		cmd.env["XDG_DATA_HOME"] = saves.string();
		cmd.env["XDG_CACHE_HOME"] = ctx.paths.cache.string();
		cmd.env["SDL_GAMECONTROLLERCONFIG"] = util::gameControllerConfig(ctx.controllers);
		util::forwardEnvironment(cmd);

		cmd.argv.insert(cmd.argv.end(), { "-f", "-g", ctx.game.rom.string() });
		return cmd;
	}
}
