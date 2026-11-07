// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch-dolphin, Copyright (c) 2026 The Batocera Team.

#include "dolphin/DolphinGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"
#include "util/Gamepad.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>

namespace es::launcher::dolphin
{
	namespace fs = std::filesystem;
	using util::IniFile;
	using util::PadControl;

	ES_LAUNCHER_REGISTER_GENERATOR("dolphin", DolphinGenerator)

	namespace
	{
		// An SDL gamepad control as Dolphin names it (InputCommon/ControllerInterface/SDL),
		// quoted for an expression
		std::string control(PadControl c)
		{
			switch (c)
			{
			case PadControl::South: return "`Button S`";
			case PadControl::East: return "`Button E`";
			case PadControl::West: return "`Button W`";
			case PadControl::North: return "`Button N`";
			case PadControl::Back: return "`Back`";
			case PadControl::Guide: return "`Guide`";
			case PadControl::Start: return "`Start`";
			case PadControl::LeftStick: return "`Thumb L`";
			case PadControl::RightStick: return "`Thumb R`";
			case PadControl::LeftShoulder: return "`Shoulder L`";
			case PadControl::RightShoulder: return "`Shoulder R`";
			case PadControl::DPadUp: return "`Pad N`";
			case PadControl::DPadDown: return "`Pad S`";
			case PadControl::DPadLeft: return "`Pad W`";
			case PadControl::DPadRight: return "`Pad E`";
			case PadControl::LeftTrigger: return "`Trigger L`";
			case PadControl::RightTrigger: return "`Trigger R`";
			}
			return "`Guide`";
		}

		// Dolphin follows XInput: up on a stick is Y+
		struct Stick
		{
			const char* up;
			const char* down;
			const char* left;
			const char* right;
		};
		constexpr Stick leftStick = { "`Left Y+`", "`Left Y-`", "`Left X-`", "`Left X+`" };
		constexpr Stick rightStick = { "`Right Y+`", "`Right Y-`", "`Right X-`", "`Right X+`" };

		void setStick(IniFile& ini, const std::string& section, const std::string& name, const Stick& stick)
		{
			ini.set(section, name + "/Up", stick.up);
			ini.set(section, name + "/Down", stick.down);
			ini.set(section, name + "/Left", stick.left);
			ini.set(section, name + "/Right", stick.right);
		}

		void setDPad(IniFile& ini, const std::string& section, const std::string& name)
		{
			ini.set(section, name + "/Up", control(PadControl::DPadUp));
			ini.set(section, name + "/Down", control(PadControl::DPadDown));
			ini.set(section, name + "/Left", control(PadControl::DPadLeft));
			ini.set(section, name + "/Right", control(PadControl::DPadRight));
		}

		// "SDL/<n>/<name>": n counts the pads of the same name. The name is the one
		// SDL_GAMECONTROLLERCONFIG gives the pad.
		std::string sdlName(const Controller& pad)
		{
			std::string name = pad.name;
			std::ranges::replace(name, ',', '.');
			return name;
		}

		// Dolphin numbers them in the order SDL opened them, the order of their device index
		std::vector<std::string> deviceNames(const std::vector<Controller>& controllers)
		{
			std::vector<std::string> names;
			for (const Controller& pad : controllers)
			{
				std::string name = sdlName(pad);
				auto before = std::ranges::count_if(controllers, [&](const Controller& other) {
					return sdlName(other) == name && other.deviceIndex < pad.deviceIndex;
				});
				names.push_back("SDL/" + std::to_string(before) + "/" + name);
			}
			return names;
		}

		const char* hotkeyName(util::HotkeyAction action)
		{
			using util::HotkeyAction;
			switch (action)
			{
			case HotkeyAction::Exit: return "General/Exit";
			case HotkeyAction::Menu: return "General/Toggle Pause";
			case HotkeyAction::Reset: return "General/Reset";
			case HotkeyAction::LoadState: return "Load State/Load from Selected Slot";
			case HotkeyAction::SaveState: return "Save State/Save to Selected Slot";
			case HotkeyAction::NextSlot: return "Other State Hotkeys/Increase Selected State Slot";
			case HotkeyAction::PreviousSlot: return "Other State Hotkeys/Decrease Selected State Slot";
			case HotkeyAction::FastForward: return "Emulation Speed/Disable Emulation Speed Limit";
			case HotkeyAction::Screenshot: return "General/Take Screenshot";
			case HotkeyAction::Rewind: return nullptr;   // Dolphin has none
			}
			return nullptr;
		}

		// Dolphin writes True and False
		const char* boolString(bool value) { return value ? "True" : "False"; }

		std::string lower(std::string s)
		{
			std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		std::string localeCode()
		{
			const char* lang = std::getenv("LANG");
			return lang != nullptr ? std::string(std::string_view(lang).substr(0, 5)) : std::string();
		}

		int gameCubeLanguage()
		{
			constexpr std::pair<const char*, int> languages[] = {
				{ "en_US", 0 }, { "de_DE", 1 }, { "fr_FR", 2 }, { "es_ES", 3 }, { "it_IT", 4 }, { "nl_NL", 5 },
			};
			std::string code = localeCode();
			for (const auto& [locale, value] : languages)
				if (code == locale)
					return value;
			return 0;
		}

		int wiiLanguage()
		{
			constexpr std::pair<const char*, int> languages[] = {
				{ "ja_JP", 0 }, { "en_US", 1 }, { "de_DE", 2 }, { "fr_FR", 3 }, { "es_ES", 4 },
				{ "it_IT", 5 }, { "nl_NL", 6 }, { "zh_CN", 7 }, { "zh_TW", 8 }, { "ko_KR", 9 },
			};
			std::string code = localeCode();
			for (const auto& [locale, value] : languages)
				if (code == locale)
					return value;
			return 1;
		}

		void setPadTuning(const LaunchContext& ctx, IniFile& ini, const std::string& section, int player)
		{
			std::string deadzone = ctx.settings.getString("deadzone_" + std::to_string(player), "5.0");
			ini.set(section, "Main Stick/Dead Zone", deadzone);
			ini.set(section, "C-Stick/Dead Zone", deadzone);

			std::string gate = ctx.settings.getString("jsgate_size_" + std::to_string(player));
			if (gate == "smaller")
			{
				ini.set(section, "Main Stick/Gate Size", "64.0");
				ini.set(section, "C-Stick/Gate Size", "56.0");
			}
			else if (gate == "larger")
			{
				ini.set(section, "Main Stick/Gate Size", "95.0");
				ini.set(section, "C-Stick/Gate Size", "88.0");
			}
		}
	}

	fs::path DolphinGenerator::userDirectory(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("dolphin");
	}

	bool DolphinGenerator::emulatedWiimotes(const LaunchContext& ctx)
	{
		return ctx.settings.getBool("emulatedwiimotes", true);
	}

	void DolphinGenerator::writeSettings(const LaunchContext& ctx, IniFile& dolphin, IniFile& gfx)
	{
		const Settings& s = ctx.settings;
		bool wii = ctx.game.system == "wii";

		// [General], [Interface], [Analytics], [Display]
		dolphin.set("General", "AutoIncrementSlot", boolString(s.getBool("incrementalsavestates", true)));
		dolphin.set("General", "ISOPath0", ctx.game.rom.parent_path().string());
		dolphin.set("General", "ISOPaths", "1");
		dolphin.set("Analytics", "PermissionAsked", "True");
		dolphin.set("Interface", "UsePanicHandlers", "False");
		dolphin.set("Interface", "ConfirmStop", "False");
		dolphin.set("Interface", "OnScreenDisplayMessages",
			boolString(s.getBool("ShowDpMsg", false) || s.getBool("retroachievements", false)));
		dolphin.remove("Display", "RenderToMain");
		dolphin.set("Display", "Fullscreen", "True");

		// [Core]
		dolphin.set("Core", "EnableCheats", boolString(s.getBool("enable_cheats", false)));
		dolphin.set("Core", "FastDiscSpeed", boolString(s.getBool("enable_fastdisc", false)));
		dolphin.set("Core", "CPUThread", boolString(s.getBool("dual_core", true)));
		dolphin.set("Core", "SyncGPU", boolString(s.getBool("gpu_sync", false)));
		dolphin.set("Core", "MMU", boolString(s.getBool("enable_mmu", false)));
		dolphin.set("Core", "SelectedLanguage", s.getString("gamecube_language", std::to_string(gameCubeLanguage())));
		dolphin.set("Core", "GFXBackend", lower(s.getString("gfxbackend")) == "vulkan" ? "Vulkan" : "OGL");
		dolphin.set("Core", "AutoDiscChange", "True");
		// the GameCube boot animation when asked for and its IPL is there
		bool ipl = false;
		for (const char* region : { "USA", "EUR", "JAP" })
		{
			std::error_code ec;
			ipl = ipl || fs::is_regular_file(userDirectory(ctx) / "GC" / region / "IPL.bin", ec);
		}
		dolphin.set("Core", "SkipIPL", boolString(!(s.getBool("dolphin_SkipIPL", false) && ipl)));
		dolphin.set("Core", "WiimoteContinuousScanning", boolString(wii && !emulatedWiimotes(ctx)));

		// the GameCube ports: a standard pad on each, none on the Wii when the pads are Wii Remotes
		for (int port = 0; port < 4; ++port)
		{
			std::string value = s.getString("dolphin_port_" + std::to_string(port + 1) + "_type");
			if (value == "6a" || value == "6b" || value == "6c")
				value = "6";
			if (value.empty())
				value = (wii && emulatedWiimotes(ctx)) ? "0" : "6";
			dolphin.set("Core", "SIDevice" + std::to_string(port), value);
		}

		// [DSP]: Dolby Pro Logic II needs the LLE audio
		bool dpl2 = s.getBool("dplii", false);
		dolphin.set("DSP", "Backend", "Cubeb");
		dolphin.set("Core", "DPL2Decoder", boolString(dpl2));
		dolphin.set("Core", "DSPHLE", boolString(!dpl2));
		dolphin.set("DSP", "EnableJIT", boolString(dpl2));

		// GFX.ini
		gfx.set("Settings", "AspectRatio", s.getString("dolphin_aspect_ratio", "0"));
		gfx.set("Settings", "ShowFPS", boolString(s.getBool("showFPS", false)));
		bool hires = s.getBool("hires_textures", false);
		gfx.set("Settings", "HiresTextures", boolString(hires));
		gfx.set("Settings", "CacheHiresTextures", boolString(hires));
		// a widescreen cheat is better than the hack
		gfx.set("Settings", "wideScreenHack", boolString(s.getBool("widescreen_hack", false) && !s.getBool("enable_cheats", false)));
		std::string ubershaders = s.getString("ubershaders", "no_ubershader");
		gfx.set("Settings", "ShaderCompilationMode", ubershaders == "no_ubershader" ? "0" : ubershaders);
		gfx.set("Settings", "WaitForShadersBeforeStarting",
			boolString(s.getBool("wait_for_shaders", false) && lower(s.getString("gfxbackend")) == "vulkan"));
		gfx.set("Settings", "InternalResolution", s.getString("internal_resolution", "1"));
		gfx.set("Settings", "MSAA", s.getString("antialiasing", "0"));
		gfx.set("Settings", "SSAA", boolString(s.getBool("use_ssaa", false)));
		gfx.set("Hardware", "VSync", boolString(s.getBool("vsync", true)));
		gfx.set("Enhancements", "MaxAnisotropy", s.getString("anisotropic_filtering", "0"));
		gfx.set("Hacks", "VISkip", boolString(s.getBool("vbi_hack", false)));
		gfx.set("Hacks", "FastTextureSampling", boolString(!s.getBool("manual_texture_sampling", false)));

		constexpr std::pair<const char*, const char*> perfHacks[] = {
			{ "Hacks", "BBoxEnable" }, { "Hacks", "DeferEFBCopies" }, { "Hacks", "EFBEmulateFormatChanges" },
			{ "Hacks", "EFBScaledCopy" }, { "Hacks", "EFBToTextureEnable" }, { "Hacks", "SkipDuplicateXFBs" },
			{ "Hacks", "XFBToTextureEnable" }, { "Enhancements", "ForceTextureFiltering" },
			{ "Enhancements", "ArbitraryMipmapDetection" }, { "Enhancements", "DisableCopyFilter" },
			{ "Enhancements", "ForceTrueColor" },
		};
		if (s.getBool("perf_hacks", false))
		{
			constexpr const char* values[] = { "False", "True", "False", "True", "True", "True", "True", "2", "True", "True", "True" };
			for (size_t i = 0; i < std::size(perfHacks); ++i)
				gfx.set(perfHacks[i].first, perfHacks[i].second, values[i]);
		}
		else
		{
			for (const auto& [section, key] : perfHacks)
				gfx.remove(section, key);
		}
	}

	void DolphinGenerator::writeGameCubePads(const LaunchContext& ctx, IniFile& ini)
	{
		auto devices = deviceNames(ctx.controllers);

		for (int player = 1; player <= 4; ++player)
			ini.removeSection("GCPad" + std::to_string(player));

		for (size_t i = 0; i < ctx.controllers.size() && i < 4; ++i)
		{
			std::string section = "GCPad" + std::to_string(i + 1);
			ini.set(section, "Device", devices[i]);

			// as batocera-launch: the GameCube A and B where the Nintendo layout has them
			ini.set(section, "Buttons/A", control(PadControl::East));
			ini.set(section, "Buttons/B", control(PadControl::South));
			ini.set(section, "Buttons/X", control(PadControl::North));
			ini.set(section, "Buttons/Y", control(PadControl::West));
			ini.set(section, "Buttons/Z", control(PadControl::RightShoulder) + " | " + control(PadControl::LeftShoulder));
			ini.set(section, "Buttons/Start", control(PadControl::Start));
			ini.set(section, "Triggers/L", control(PadControl::LeftTrigger));
			ini.set(section, "Triggers/R", control(PadControl::RightTrigger));
			ini.set(section, "Triggers/L-Analog", control(PadControl::LeftTrigger));
			ini.set(section, "Triggers/R-Analog", control(PadControl::RightTrigger));
			setDPad(ini, section, "D-Pad");
			setStick(ini, section, "Main Stick", leftStick);
			setStick(ini, section, "C-Stick", rightStick);
			if (ctx.settings.getBool("rumble", true))
				ini.set(section, "Rumble/Motor", "`Motor`");
			setPadTuning(ctx, ini, section, static_cast<int>(i) + 1);
		}
	}

	void DolphinGenerator::writeWiimotes(const LaunchContext& ctx, IniFile& ini)
	{
		auto devices = deviceNames(ctx.controllers);
		bool emulated = emulatedWiimotes(ctx);
		std::string mode = ctx.settings.getString("controller_mode");

		for (int player = 1; player <= 4; ++player)
		{
			std::string section = "Wiimote" + std::to_string(player);
			ini.removeSection(section);

			// 1: emulated, 2: a real Wii Remote, 0: none
			size_t index = static_cast<size_t>(player - 1);
			if (!emulated)
			{
				ini.set(section, "Source", "2");
				continue;
			}
			if (index >= ctx.controllers.size())
			{
				ini.set(section, "Source", "0");
				continue;
			}

			ini.set(section, "Source", "1");
			ini.set(section, "Device", devices[index]);

			if (mode == "cc" || mode == "pro")
			{
				ini.set(section, "Extension", "Classic");
				ini.set(section, "Classic/Buttons/A", control(PadControl::East));
				ini.set(section, "Classic/Buttons/B", control(PadControl::South));
				ini.set(section, "Classic/Buttons/X", control(PadControl::North));
				ini.set(section, "Classic/Buttons/Y", control(PadControl::West));
				ini.set(section, "Classic/Buttons/-", control(PadControl::Back));
				ini.set(section, "Classic/Buttons/+", control(PadControl::Start));
				setDPad(ini, section, "Classic/D-Pad");
				setStick(ini, section, "Classic/Left Stick", leftStick);
				setStick(ini, section, "Classic/Right Stick", rightStick);

				// the Classic Controller has its shoulders where the Pro has its triggers
				bool classic = mode == "cc";
				PadControl l = classic ? PadControl::LeftTrigger : PadControl::LeftShoulder;
				PadControl r = classic ? PadControl::RightTrigger : PadControl::RightShoulder;
				PadControl zl = classic ? PadControl::LeftShoulder : PadControl::LeftTrigger;
				PadControl zr = classic ? PadControl::RightShoulder : PadControl::RightTrigger;
				ini.set(section, "Classic/Triggers/L", control(l));
				ini.set(section, "Classic/Triggers/R", control(r));
				ini.set(section, "Classic/Buttons/ZL", control(zl));
				ini.set(section, "Classic/Buttons/ZR", control(zr));
				continue;
			}

			// the Wii Remote held upright: the pad's south button is A
			bool sideways = mode == "side";
			ini.set(section, "Buttons/A", control(sideways ? PadControl::West : PadControl::South));
			ini.set(section, "Buttons/B", control(sideways ? PadControl::North : PadControl::East));
			ini.set(section, "Buttons/1", control(sideways ? PadControl::South : PadControl::West));
			ini.set(section, "Buttons/2", control(sideways ? PadControl::East : PadControl::North));
			ini.set(section, "Buttons/-", control(PadControl::LeftShoulder));
			ini.set(section, "Buttons/+", control(PadControl::RightShoulder));
			ini.set(section, "Buttons/Home", control(PadControl::Back));
			setDPad(ini, section, "D-Pad");
			if (sideways)
				ini.set(section, "Options/Sideways Wiimote", "1");

			if (mode == "in")
			{
				// the Nunchuk takes the left stick and the triggers; the pointer moves to the right stick
				ini.set(section, "Extension", "Nunchuk");
				ini.set(section, "Nunchuk/Buttons/C", control(PadControl::LeftTrigger));
				ini.set(section, "Nunchuk/Buttons/Z", control(PadControl::RightTrigger));
				setStick(ini, section, "Nunchuk/Stick", leftStick);
				setStick(ini, section, "IR", rightStick);
			}
			else
			{
				ini.set(section, "Extension", "None");
				setStick(ini, section, "IR", leftStick);
				ini.set(section, "Tilt/Forward", rightStick.up);
				ini.set(section, "Tilt/Backward", rightStick.down);
				ini.set(section, "Tilt/Left", rightStick.left);
				ini.set(section, "Tilt/Right", rightStick.right);
				// shaking the remote (spin attacks...) on a trigger
				for (const char* axis : { "Shake/X", "Shake/Y", "Shake/Z" })
					ini.set(section, axis, control(sideways ? PadControl::LeftTrigger : PadControl::RightTrigger));
			}

			if (ctx.settings.getBool("rumble", true))
				ini.set(section, "Rumble/Motor", "`Motor`");
		}
	}

	void DolphinGenerator::writeHotkeys(const LaunchContext& ctx, IniFile& ini)
	{
		ini.removeSection("Hotkeys");

		const Controller* pad = util::firstPlayer(ctx);
		if (pad == nullptr)
		{
			// the keyboard keys batocera-launch uses
			ini.set("Hotkeys", "Device", "XInput2/0/Virtual core pointer");
			ini.set("Hotkeys", "General/Exit", "Escape");
			ini.set("Hotkeys", "General/Toggle Pause", "F10");
			ini.set("Hotkeys", "General/Take Screenshot", "F9");
			ini.set("Hotkeys", "Load State/Load from Selected Slot", "F8");
			ini.set("Hotkeys", "Save State/Save to Selected Slot", "F5");
			ini.set("Hotkeys", "Other State Hotkeys/Increase Selected State Slot", "@(Shift+F1)");
			ini.set("Hotkeys", "Other State Hotkeys/Decrease Selected State Slot", "@(Shift+F2)");
			ini.set("Hotkeys", "Emulation Speed/Disable Emulation Speed Limit", "Tab");
			return;
		}

		size_t index = static_cast<size_t>(pad - ctx.controllers.data());
		ini.set("Hotkeys", "Device", deviceNames(ctx.controllers)[index]);

		// @(a+b): b pressed while a is held
		std::string hotkey = control(util::hotkeyControl(*pad));
		for (const auto& [action, c] : util::hotkeys)
			if (const char* name = hotkeyName(action))
				ini.set("Hotkeys", name, "@(" + hotkey + "+" + control(c) + ")");
	}

	void DolphinGenerator::writeAchievements(const LaunchContext& ctx, IniFile& ini)
	{
		const Settings& s = ctx.settings;
		ini.removeSection("Achievements");

		if (!s.getBool("retroachievements", false))
		{
			ini.set("Achievements", "Enabled", "False");
			return;
		}

		ini.set("Achievements", "Enabled", "True");
		ini.set("Achievements", "Username", s.getString("retroachievements.username"));
		ini.set("Achievements", "ApiToken", s.getString("retroachievements.token"));
		ini.set("Achievements", "HardcoreEnabled", boolString(s.getBool("retroachievements.hardcore", false)));
		ini.set("Achievements", "EncoreEnabled", boolString(s.getBool("retroachievements.encore", false)));
		ini.set("Achievements", "ChallengeIndicatorsEnabled", boolString(s.getBool("retroachievements.challenge_indicators", false)));
		ini.set("Achievements", "LeaderboardTrackerEnabled", boolString(s.getBool("retroachievements.leaderboards", false)));
		ini.set("Achievements", "UnofficialEnabled", boolString(s.getBool("retroachievements.unofficial", false)));
		ini.set("Achievements", "ProgressEnabled", boolString(s.getBool("retroachievements.verbose", false)));
	}

	Command DolphinGenerator::configure(const LaunchContext& ctx)
	{
		if (ctx.game.system != "gamecube" && ctx.game.system != "wii")
			throw LaunchError(ErrorCode::InvalidConfiguration, "Dolphin runs GameCube and Wii games, not \"" + ctx.game.system + "\".");

		fs::path user = userDirectory(ctx);
		fs::path config = user / "Config";
		bool wii = ctx.game.system == "wii";

		auto program = util::emulatorCommand(ctx, "dolphin", { "dolphin-emu" }, {
			user, ctx.paths.saves, ctx.paths.screenshots, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "Dolphin is not installed: install it from the emulator catalog or with your package manager.");

		IniFile dolphin = IniFile::load(config / "Dolphin.ini");
		IniFile gfx = IniFile::load(config / "GFX.ini");
		writeSettings(ctx, dolphin, gfx);

		IniFile pads = IniFile::load(config / "GCPadNew.ini");
		writeGameCubePads(ctx, pads);
		IniFile wiimotes = IniFile::load(config / "WiimoteNew.ini");
		if (wii)
			writeWiimotes(ctx, wiimotes);

		IniFile hotkeys;
		writeHotkeys(ctx, hotkeys);
		IniFile achievements;
		writeAchievements(ctx, achievements);

		try
		{
			dolphin.save(config / "Dolphin.ini");
			gfx.save(config / "GFX.ini");
			pads.save(config / "GCPadNew.ini");
			if (wii)
				wiimotes.save(config / "WiimoteNew.ini");
			hotkeys.save(config / "Hotkeys.ini");
			achievements.save(config / "RetroAchievements.ini");

			// memory cards, the Wii's NAND and the states where ES keeps saves
			util::linkDirectory(user / "GC", ctx.paths.saves / "gamecube" / "GC");
			util::linkDirectory(user / "Wii", ctx.paths.saves / "wii" / "Wii");
			util::linkDirectory(user / "StateSaves", ctx.paths.saves / "dolphin" / "StateSaves");
			util::linkDirectory(user / "ScreenShots", ctx.paths.screenshots);
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the Dolphin configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		// -b: batch, Dolphin exits with the game
		cmd.argv.insert(cmd.argv.end(), { "-b", "-u", user.string() });

		if (wii)
		{
			// the Wii's own settings, written to its SYSCONF when the game boots
			cmd.argv.insert(cmd.argv.end(), {
				"-C", "SYSCONF.IPL.AR=" + std::string(ctx.settings.getString("tv_mode") == "1" ? "1" : "0"),
				"-C", "SYSCONF.IPL.LNG=" + ctx.settings.getString("wii_language", std::to_string(wiiLanguage())),
				"-C", "SYSCONF.BT.BAR=" + std::string(ctx.settings.getString("sensorbar_position") == "1" ? "1" : "0"),
			});
		}

		if (!ctx.saveState.file.empty())
			cmd.argv.push_back("--save_state=" + ctx.saveState.file.string());
		cmd.argv.insert(cmd.argv.end(), { "-e", ctx.game.rom.string() });

		cmd.env["SDL_GAMECONTROLLERCONFIG"] = util::gameControllerConfig(ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
