// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.ppsspp), Copyright (c) 2026 The Batocera Team.

#include "ppsspp/PpssppGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"
#include "util/Gamepad.h"

#include <algorithm>
#include <cctype>

namespace es::launcher::ppsspp
{
	namespace fs = std::filesystem;
	using util::IniFile;
	using util::PadControl;

	ES_LAUNCHER_REGISTER_GENERATOR("ppsspp", PpssppGenerator)

	namespace
	{
		// PPSSPP sends every pad as pad 0 (SDL/SDLJoystick.cpp)
		constexpr int padDevice = 10;

		// PPSSPP's key codes for the SDL gamepad controls (Common/Input/KeyCodes.h)
		int keyCode(PadControl control)
		{
			switch (control)
			{
			case PadControl::South: return 189;          // NKCODE_BUTTON_2
			case PadControl::East: return 190;           // NKCODE_BUTTON_3
			case PadControl::West: return 191;           // NKCODE_BUTTON_4
			case PadControl::North: return 188;          // NKCODE_BUTTON_1
			case PadControl::Back: return 196;           // NKCODE_BUTTON_9
			case PadControl::Start: return 197;          // NKCODE_BUTTON_10
			case PadControl::Guide: return 4;            // NKCODE_BACK
			case PadControl::LeftStick: return 198;      // NKCODE_BUTTON_THUMBL
			case PadControl::RightStick: return 199;     // NKCODE_BUTTON_THUMBR
			case PadControl::LeftShoulder: return 193;   // NKCODE_BUTTON_6
			case PadControl::RightShoulder: return 192;  // NKCODE_BUTTON_5
			case PadControl::DPadUp: return 19;
			case PadControl::DPadDown: return 20;
			case PadControl::DPadLeft: return 21;
			case PadControl::DPadRight: return 22;
			case PadControl::LeftTrigger: return 4008;   // axis 4, positive
			case PadControl::RightTrigger: return 4010;  // axis 5, positive
			}
			return 0;
		}

		// An SDL gamepad axis direction: AXIS_BIND_NKCODE_START + axis * 2, + 1 when negative
		constexpr int axisCode(int axis, bool negative) { return 4000 + axis * 2 + (negative ? 1 : 0); }

		std::string binding(int code) { return std::to_string(padDevice) + "-" + std::to_string(code); }
		std::string binding(PadControl control) { return binding(keyCode(control)); }

		const char* hotkeyName(util::HotkeyAction action)
		{
			using util::HotkeyAction;
			switch (action)
			{
			case HotkeyAction::Exit: return "Exit App";
			case HotkeyAction::Menu: return "Pause";
			case HotkeyAction::Reset: return "Reset";
			case HotkeyAction::LoadState: return "Load State";
			case HotkeyAction::SaveState: return "Save State";
			case HotkeyAction::NextSlot: return "Next Slot";
			case HotkeyAction::PreviousSlot: return "Previous Slot";
			case HotkeyAction::Rewind: return "Rewind";
			case HotkeyAction::FastForward: return "Fast-forward";
			case HotkeyAction::Screenshot: return "Screenshot";
			}
			return "";
		}

		// PPSSPP writes True and False
		const char* boolString(bool value) { return value ? "True" : "False"; }

		std::string upper(std::string s)
		{
			std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
			return s;
		}
	}

	fs::path PpssppGenerator::memoryStick(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("ppsspp") / "ppsspp";
	}

	void PpssppGenerator::writeSettings(const LaunchContext& ctx, IniFile& ini)
	{
		const Settings& s = ctx.settings;

		// [Graphics]: OpenGL unless Vulkan is asked for, whatever the spelling ("vulkan" for RetroArch)
		ini.set("Graphics", "GraphicsBackend", upper(s.getString("gfxbackend")) == "VULKAN" ? "VULKAN" : "OPENGL");
		ini.set("Graphics", "InternalResolution", s.getString("internal_resolution", "1"));
		ini.set("Graphics", "MultiSampleLevel", s.getString("msaa", "0"));
		ini.set("Graphics", "SoftwareRenderer", "False");
		ini.set("Graphics", "FullScreen", "True");
		ini.set("Graphics", "VerticalSync", boolString(s.getBool("vsync", false)));
		ini.set("Graphics", "FrameSkip", s.getString("frameskip", "0"));
		ini.set("Graphics", "AutoFrameSkip", boolString(s.getBool("autoframeskip", false)));
		ini.set("Graphics", "SkipBufferEffects", boolString(s.getBool("skip_buffer_effects", false)));
		ini.set("Graphics", "DisableRangeCulling", boolString(s.getBool("disable_culling", false)));
		ini.set("Graphics", "SkipGPUReadbackMode", s.getString("skip_gpu_readbacks", "0"));
		ini.set("Graphics", "TextureBackoffCache", boolString(s.getBool("lazy_texture_caching", false)));
		ini.set("Graphics", "SplineBezierQuality", s.getString("curves_quality", "2"));
		ini.set("Graphics", "RenderDuplicateFrames", boolString(s.getBool("duplicate_frames", false)));
		ini.set("Graphics", "InflightFrames", s.getString("buffer_graphics", "3"));
		ini.set("Graphics", "HardwareTransform", "True");
		ini.set("Graphics", "SoftwareSkinning", boolString(s.getBool("software_skinning", true)));
		ini.set("Graphics", "HardwareTessellation", boolString(s.getBool("hardware_tessellation", false)));
		ini.set("Graphics", "TexScalingType", s.getString("texture_scaling_type", "0"));
		ini.set("Graphics", "TexScalingLevel", s.getString("texture_scaling_level", "1"));
		ini.set("Graphics", "TexDeposterize", boolString(s.getBool("texture_deposterize", false)));
		ini.set("Graphics", "AnisotropyLevel", s.getString("anisotropic_filtering", "4"));
		ini.set("Graphics", "TextureFiltering", s.getString("texture_filtering", "1"));
		ini.set("Graphics", "Smart2DTexFiltering", boolString(s.getBool("smart_2d", false)));
		ini.set("Graphics", "iShowStatusFlags", s.getBool("showFPS", false) ? "6" : "0");
		ini.set("Graphics", "DisplayIntegerScale", "False");

		// [SystemParam]
		std::string nickname = s.getString("netplay.nickname", "Player");
		if (s.getBool("retroachievements", false) && s.isSet("retroachievements.username"))
			nickname = s.getString("retroachievements.username");
		ini.set("SystemParam", "NickName", nickname);
		ini.set("SystemParam", "EncryptSave", "False");   // saves can move between machines
		ini.set("SystemParam", "MemStickSize", "32");

		// [General]
		ini.set("General", "FirstRun", "False");
		ini.set("General", "RewindSnapshotInterval", s.getBool("rewind", false) ? "5" : "0");
		ini.set("General", "EnableCheats", boolString(s.getBool("enable_cheats", false)));
		ini.set("General", "CheckForNewVersion", "False");
		ini.set("General", "AskForExitConfirmationAfterSeconds", "0");   // Exit App asks otherwise
		if (ctx.saveState.slot)
			ini.set("General", "StateSlot", std::to_string(*ctx.saveState.slot));

		// [Upgrade]: no update messages
		ini.set("Upgrade", "UpgradeMessage", "");
		ini.set("Upgrade", "UpgradeVersion", "");
		ini.set("Upgrade", "DismissedVersion", "");

		// [Achievements]: the token is written by configure()
		if (s.getBool("retroachievements", false))
		{
			ini.set("Achievements", "AchievementsEnable", "True");
			ini.set("Achievements", "AchievementsUserName", s.getString("retroachievements.username"));
			ini.set("Achievements", "AchievementsChallengeMode", boolString(s.getBool("retroachievements.hardcore", false)));
			ini.set("Achievements", "AchievementsEncoreMode", boolString(s.getBool("retroachievements.encore", false)));
			ini.set("Achievements", "AchievementsUnofficial", boolString(s.getBool("retroachievements.unofficial", false)));
			ini.set("Achievements", "AchievementsSoundEffects", "True");
		}
		else
		{
			ini.set("Achievements", "AchievementsEnable", "False");
			ini.set("Achievements", "AchievementsChallengeMode", "False");
		}

		// [Network]: ad hoc play, off unless asked
		bool network = s.getBool("network_enable", false);
		ini.set("Network", "EnableWlan", boolString(network));
		if (network)
		{
			std::string mode = s.getString("lan_adhoc_mode", "off");
			ini.set("Network", "EnableAdhocServer", boolString(mode == "host"));
			std::string server = s.getString("adhoc_server");
			if (server.empty())
				ini.remove("Network", "proAdhocServer");
			else if (server != "__manual__")
				ini.set("Network", "proAdhocServer", server);
			ini.set("Network", "PortOffset", s.getString("adhoc_port_offset", "10000"));
			ini.set("Network", "EnableUPnP", boolString(s.getBool("upnp_enable", false)));
		}

		// ppsspp.<section>.<key> = value, as the player wrote it
		for (const auto& [sectionKey, value] : s.withPrefix("ppsspp."))
		{
			size_t dot = sectionKey.find('.');
			if (dot != std::string::npos && dot > 0 && dot + 1 < sectionKey.size())
				ini.set(sectionKey.substr(0, dot), sectionKey.substr(dot + 1), value);
		}
	}

	void PpssppGenerator::writeControls(const LaunchContext& ctx, IniFile& controls)
	{
		// SDL_GAMECONTROLLERCONFIG makes every pad look the same: one mapping for all
		constexpr std::pair<const char*, PadControl> buttons[] = {
			{ "Cross", PadControl::South }, { "Circle", PadControl::East },
			{ "Square", PadControl::West }, { "Triangle", PadControl::North },
			{ "Start", PadControl::Start }, { "Select", PadControl::Back },
			{ "L", PadControl::LeftShoulder }, { "R", PadControl::RightShoulder },
			{ "Up", PadControl::DPadUp }, { "Down", PadControl::DPadDown },
			{ "Left", PadControl::DPadLeft }, { "Right", PadControl::DPadRight },
		};
		const char* section = "ControlMapping";

		for (const auto& [key, control] : buttons)
			controls.set(section, key, binding(control));

		controls.set(section, "An.Left", binding(axisCode(0, true)));
		controls.set(section, "An.Right", binding(axisCode(0, false)));
		controls.set(section, "An.Up", binding(axisCode(1, true)));
		controls.set(section, "An.Down", binding(axisCode(1, false)));
		controls.set(section, "RightAn.Left", binding(axisCode(2, true)));
		controls.set(section, "RightAn.Right", binding(axisCode(2, false)));
		controls.set(section, "RightAn.Up", binding(axisCode(3, true)));
		controls.set(section, "RightAn.Down", binding(axisCode(3, false)));

		// hotkeys: two keys pressed together are written key1:key2
		const Controller* pad = util::firstPlayer(ctx);
		std::string hotkey = binding(pad != nullptr ? util::hotkeyControl(*pad) : PadControl::Guide);
		for (const auto& [action, control] : util::hotkeys)
			controls.set(section, hotkeyName(action), hotkey + ":" + binding(control));
	}

	Command PpssppGenerator::configure(const LaunchContext& ctx)
	{
		fs::path home = ctx.paths.configFor("ppsspp");
		fs::path saves = ctx.paths.saves / "psp";

		auto program = util::emulatorCommand(ctx, "ppsspp", { "PPSSPPSDL", "ppsspp", "PPSSPPQt" }, {
			home, saves, ctx.paths.screenshots, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "PPSSPP is not installed: install it from the emulator catalog or with your package manager.");

		fs::path psp = memoryStick(ctx) / "PSP";
		fs::path system = psp / "SYSTEM";

		IniFile ini = IniFile::load(system / "ppsspp.ini");
		writeSettings(ctx, ini);
		IniFile controls = IniFile::load(system / "controls.ini");
		writeControls(ctx, controls);

		try
		{
			ini.save(system / "ppsspp.ini");
			controls.save(system / "controls.ini");

			std::string token = ctx.settings.getString("retroachievements.token");
			if (ctx.settings.getBool("retroachievements", false) && !token.empty())
				util::writeFileAtomically(system / "ppsspp_retroachievements.dat", token);

			// the saves where ES keeps them; a folder the player already has stays
			util::linkDirectory(psp / "SAVEDATA", saves / "SAVEDATA");
			util::linkDirectory(psp / "PPSSPP_STATE", saves / "PPSSPP_STATE");
			util::linkDirectory(psp / "SCREENSHOT", ctx.paths.screenshots);
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the PPSSPP configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.argv.push_back(ctx.game.rom.string());
		cmd.argv.push_back("--fullscreen");
		if (!ctx.saveState.file.empty())
			cmd.argv.push_back("--state=" + ctx.saveState.file.string());

		cmd.env["XDG_CONFIG_HOME"] = home.string();
		cmd.env["SDL_GAMECONTROLLERCONFIG"] = util::gameControllerConfig(ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
