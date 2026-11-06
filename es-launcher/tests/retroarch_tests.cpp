// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "Check.h"
#include "es-launcher/Launcher.h"
#include "retroarch/ConfigFile.h"
#include "retroarch/Cores.h"
#include "retroarch/RetroArchGenerator.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using namespace es::launcher;
using namespace es::launcher::retroarch;
namespace fs = std::filesystem;

namespace
{
	// A temporary directory removed at the end of the test
	struct TempDir
	{
		fs::path path;

		TempDir()
		{
			std::string tmpl = (fs::temp_directory_path() / "es-launcher-test-XXXXXX").string();
			path = mkdtemp(tmpl.data());
		}
		~TempDir()
		{
			std::error_code ec;
			fs::remove_all(path, ec);
		}
	};

	void writeFile(const fs::path& path, const std::string& content, bool executable = false)
	{
		fs::create_directories(path.parent_path());
		std::ofstream(path) << content;
		if (executable)
			fs::permissions(path, fs::perms::owner_all);
	}

	std::string readFile(const fs::path& path)
	{
		std::ifstream in(path);
		return std::string(std::istreambuf_iterator<char>(in), {});
	}

	Controller makePad(int player, int index)
	{
		Controller pad;
		pad.player = player;
		pad.deviceIndex = index;
		pad.guid = "030000005e0400008e02000010010000";
		pad.name = "Xbox 360 Controller";
		pad.inputs = {
			{ "a", { InputBinding::Type::Button, 0, 1 } },
			{ "b", { InputBinding::Type::Button, 1, 1 } },
			{ "x", { InputBinding::Type::Button, 2, 1 } },
			{ "y", { InputBinding::Type::Button, 3, 1 } },
			{ "pageup", { InputBinding::Type::Button, 4, 1 } },
			{ "pagedown", { InputBinding::Type::Button, 5, 1 } },
			{ "l2", { InputBinding::Type::Axis, 2, 1 } },
			{ "r2", { InputBinding::Type::Axis, 5, 1 } },
			{ "select", { InputBinding::Type::Button, 6, 1 } },
			{ "start", { InputBinding::Type::Button, 7, 1 } },
			{ "hotkey", { InputBinding::Type::Button, 8, 1 } },
			{ "up", { InputBinding::Type::Hat, 0, 1 } },
			{ "down", { InputBinding::Type::Hat, 0, 4 } },
			{ "left", { InputBinding::Type::Hat, 0, 8 } },
			{ "right", { InputBinding::Type::Hat, 0, 2 } },
			{ "joystick1up", { InputBinding::Type::Axis, 1, -1 } },
			{ "joystick1left", { InputBinding::Type::Axis, 0, -1 } },
		};
		return pad;
	}
}

static void testConfigFile()
{
	TempDir tmp;
	fs::path file = tmp.path / "a" / "retroarch.cfg";
	writeFile(file, "# comment\nvideo_driver = \"gl\"\nmenu_driver=ozone\n\nbroken line\naudio_volume = \"-3.0\"\n");

	ConfigFile config = ConfigFile::load(file);
	CHECK(config.size() == 3);
	CHECK(config.get("video_driver") == "gl");
	CHECK(config.get("menu_driver") == "ozone");
	CHECK(config.get("audio_volume") == "-3.0");

	config.set("video_driver", "glcore");      // keeps its place
	config.set("video_fullscreen", true);      // appended
	config.remove("menu_driver");
	config.save(file);

	CHECK(readFile(file) == "video_driver = \"glcore\"\naudio_volume = \"-3.0\"\nvideo_fullscreen = \"true\"\n");
	CHECK(!fs::exists(file.string() + ".tmp"));

	CHECK(ConfigFile::load(tmp.path / "missing.cfg").size() == 0);
}

static void testCoreNames()
{
	CHECK(coreLibraryName("mupen64plus-next") == "mupen64plus_next_libretro.so");
	CHECK(coreLibraryName("snes9x") == "snes9x_libretro.so");
}

static void testControllers()
{
	LaunchContext ctx;
	ctx.core = "snes9x";
	ctx.game.system = "snes";
	ctx.controllers = { makePad(1, 0), makePad(2, 3) };

	ConfigFile config;
	config.set("input_player5_a_btn", "9");    // a pad from a previous launch
	config.set("input_player1_a", "x");        // keyboard binding, forgotten too
	RetroArchGenerator::setControllers(ctx, config);

	CHECK(!config.get("input_player5_a_btn"));
	CHECK(!config.get("input_player1_a"));
	CHECK(config.get("input_player1_a_btn") == "0");
	CHECK(config.get("input_player1_l_btn") == "4");
	CHECK(config.get("input_player1_l2_axis") == "+2");
	CHECK(config.get("input_player1_up_btn") == "h0up");
	CHECK(config.get("input_player1_right_btn") == "h0right");
	CHECK(config.get("input_player1_l_y_minus_axis") == "-1");
	CHECK(config.get("input_player1_l_y_plus_axis") == "+1");
	CHECK(config.get("input_player1_joypad_index") == "0");
	CHECK(config.get("input_player2_joypad_index") == "3");
	CHECK(config.get("input_player1_analog_dpad_mode") == "1");   // hat d-pad: stick drives it too
	CHECK(config.get("input_enable_hotkey_btn") == "8");
	CHECK(config.get("input_exit_emulator_btn") == "7");
	CHECK(config.get("input_menu_toggle_btn") == "1");
	CHECK(config.get("input_libretro_device_p1") == "1");
	CHECK(config.get("input_libretro_device_p2") == "1");

	// fight stick layout
	ctx.settings = Settings({ { "altlayout", "fightstick" } }, {}, {});
	RetroArchGenerator::setControllers(ctx, config);
	CHECK(config.get("input_player1_l2_btn") == "4");
	CHECK(config.get("input_player1_l_btn") == "5");

	// N64: analog stick stays analog, Z moves to L1 on pads without a right trigger
	LaunchContext n64;
	n64.core = "mupen64plus-next";
	n64.game.system = "n64";
	Controller pad = makePad(1, 0);
	pad.inputs.erase("r2");
	n64.controllers = { pad };
	ConfigFile c2;
	RetroArchGenerator::setControllers(n64, c2);
	CHECK(c2.get("input_player1_analog_dpad_mode") == "0");
	CHECK(c2.get("input_player1_l2_btn") == "4");
}

static void testVideoAndStates()
{
	LaunchContext ctx;
	ctx.architecture = Architecture::AArch64;
	ctx.settings = Settings({ { "ratio", "16/9" }, { "smooth", "1" }, { "autosave", "0" } }, { { "runahead", "2" } }, {});
	ctx.saveState.slot = 3;

	ConfigFile config;
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	RetroArchGenerator::setSaveStates(ctx, config);

	CHECK(config.get("video_driver") == "gl");
	CHECK(config.get("aspect_ratio_index") == "1");
	CHECK(config.get("video_smooth") == "true");
	CHECK(config.get("run_ahead_enabled") == "true");
	CHECK(config.get("run_ahead_frames") == "2");
	CHECK(config.get("state_slot") == "3");
	CHECK(config.get("savestate_auto_load") == "false");

	ctx.settings = Settings({ { "ratio", "nonsense" } }, {}, {});
	ctx.architecture = Architecture::X86_64;
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("aspect_ratio_index") == "22");   // core ratio
	CHECK(config.get("video_driver") == "glcore");
}

static void testFullLaunch()
{
	TempDir tmp;
	fs::path bin = tmp.path / "bin";
	fs::path cores = tmp.path / "cores";
	writeFile(bin / "retroarch", "#!/bin/sh\n", true);
	writeFile(cores / "gambatte_libretro.so", "");

	std::string oldPath = std::getenv("PATH") ? std::getenv("PATH") : "";
	setenv("PATH", bin.c_str(), 1);
	setenv("ES_LIBRETRO_CORES_DIR", cores.c_str(), 1);

	LaunchContext ctx;
	ctx.emulator = "libretro";
	ctx.core = "gambatte";
	ctx.game.system = "gb";
	ctx.game.rom = "/roms/gb/Tetris (World).gb";
	ctx.paths.bios = tmp.path / "bios";
	ctx.paths.saves = tmp.path / "saves";
	ctx.paths.screenshots = tmp.path / "shots";
	ctx.paths.cache = tmp.path / "cache";
	ctx.paths.emulatorConfigs = tmp.path / "emulators";
	ctx.settings = Settings({ { "retroarch.menu_driver", "rgui" }, { "retroarchcore.gambatte_up_down_allowed", "enabled" } },
	                        { { "gb_colorization", "GB - SmartColor" } }, {});
	ctx.controllers = { makePad(1, 0) };

	fs::path configDir = tmp.path / "emulators" / "retroarch";
	writeFile(configDir / "retroarch.cfg", "menu_driver = \"ozone\"\nvideo_vsync = \"false\"\n");
	writeFile(configDir / "gb.cfg", "video_shader_enable = \"true\"\n");

	Launch launch(ctx);
	CHECK(launch.handled());
	Command cmd = launch.prepare();

	CHECK(cmd.argv.size() == 8);
	CHECK(cmd.argv[0] == (bin / "retroarch").string());
	CHECK(cmd.argv[1] == "-L");
	CHECK(cmd.argv[2] == (cores / "gambatte_libretro.so").string());
	CHECK(cmd.argv[4] == (configDir / "retroarch.cfg").string());
	CHECK(cmd.argv[5] == "--appendconfig");
	CHECK(cmd.argv[6] == (configDir / "gb.cfg").string());
	CHECK(cmd.argv[7] == "/roms/gb/Tetris (World).gb");

	ConfigFile written = ConfigFile::load(configDir / "retroarch.cfg");
	CHECK(written.get("video_vsync") == "false");        // the player's own setting survives
	CHECK(written.get("menu_driver") == "rgui");         // retroarch.* option wins
	CHECK(written.get("savefile_directory") == (tmp.path / "saves" / "gb").string());
	CHECK(written.get("system_directory") == (tmp.path / "bios").string());
	CHECK(written.get("libretro_directory") == cores.string());
	CHECK(fs::is_directory(tmp.path / "saves" / "gb"));

	ConfigFile options = ConfigFile::load(configDir / "retroarch-core-options.cfg");
	CHECK(options.get("gambatte_gb_colorization") == "auto");
	CHECK(options.get("gambatte_up_down_allowed") == "enabled");

	// missing core
	LaunchContext missing = ctx;
	missing.core = "mesen";
	try
	{
		Launch(missing).prepare();
		CHECK(!"a missing core must throw");
	}
	catch (const LaunchError& e)
	{
		CHECK(e.code() == ErrorCode::CoreMissing);
	}

	// netplay client and a save state to load
	LaunchContext net = ctx;
	net.netplay = Netplay{ Netplay::Mode::Client, "192.168.1.20", 55436, "pw", "" };
	net.saveState.slot = 2;
	net.saveState.file = "/saves/gb/Tetris.state2";
	Command netCmd = Launch(net).prepare();
	auto has = [&](std::initializer_list<std::string> seq) {
		return std::ranges::search(netCmd.argv, seq).begin() != netCmd.argv.end();
	};
	CHECK(has({ "--connect", "192.168.1.20" }));
	CHECK(has({ "--port", "55436" }));
	CHECK(has({ "-e", "2" }));
	CHECK(netCmd.argv.back() == "/roms/gb/Tetris (World).gb");

	// RetroArch not installed
	setenv("PATH", (tmp.path / "nothing").c_str(), 1);
	try
	{
		Launch(ctx).prepare();
		CHECK(!"a missing RetroArch must throw");
	}
	catch (const LaunchError& e)
	{
		CHECK(e.code() == ErrorCode::EmulatorMissing);
	}

	setenv("PATH", oldPath.c_str(), 1);
	unsetenv("ES_LIBRETRO_CORES_DIR");
}

void tests::runRetroArchTests()
{
	testConfigFile();
	testCoreNames();
	testControllers();
	testVideoAndStates();
	testFullLaunch();
}
