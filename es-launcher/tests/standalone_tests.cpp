// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "Check.h"
#include "Helpers.h"
#include "dolphin/DolphinGenerator.h"
#include "duckstation/DuckStationGenerator.h"
#include "es-launcher/Launcher.h"
#include "pcsx2/Pcsx2Generator.h"
#include "ppsspp/PpssppGenerator.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"
#include "util/IniFile.h"
#include "azahar/AzaharGenerator.h"
#include "cemu/CemuGenerator.h"
#include "xemu/XemuGenerator.h"
#include "xenia/XeniaGenerator.h"
#include "rpcs3/Rpcs3Generator.h"
#include "eden/EdenGenerator.h"
#include "util/YamlFile.h"

#include <pugixml.hpp>

#include <algorithm>
#include <string>

using namespace tests;
using es::launcher::util::IniFile;
using es::launcher::util::YamlFile;

namespace
{
	bool contains(const std::vector<std::string>& argv, std::string_view word)
	{
		return std::ranges::find(argv, word) != argv.end();
	}

	// What every launch of these tests shares: folders in a temporary directory, one pad
	LaunchContext makeContext(const fs::path& root, std::string emulator, std::string system, fs::path rom)
	{
		LaunchContext ctx;
		ctx.emulator = std::move(emulator);
		ctx.game.system = std::move(system);
		ctx.game.rom = std::move(rom);
		ctx.paths.bios = root / "bios";
		ctx.paths.saves = root / "saves";
		ctx.paths.screenshots = root / "shots";
		ctx.paths.cache = root / "cache";
		ctx.paths.emulatorConfigs = root / "emulators";
		ctx.architecture = Architecture::X86_64;
		ctx.controllers = { makePad(1, 0) };
		return ctx;
	}

	bool throwsLaunchError(LaunchContext ctx, ErrorCode code)
	{
		try
		{
			Launch(std::move(ctx)).prepare();
		}
		catch (const LaunchError& e)
		{
			return e.code() == code;
		}
		return false;
	}
}

static void testIniFile()
{
	IniFile ini = IniFile::parse("\xEF\xBB\xBF; header\n[Main]\nA = 1\nB=two words \n\n[GPU]\n# comment\nRenderer = OpenGL\n");
	CHECK(ini.get("Main", "A") == "1");
	CHECK(ini.get("Main", "B") == "two words");
	CHECK(ini.get("GPU", "Renderer") == "OpenGL");
	CHECK(!ini.get("GPU", "renderer"));   // case-sensitive
	CHECK(!ini.get("Nope", "A"));

	ini.set("Main", "C", "3");
	ini.set("GPU", "Renderer", "Vulkan");
	ini.set("New", "Key", "value");
	ini.remove("Main", "A");
	CHECK(ini.toString() == "; header\n\n[Main]\nB = two words\nC = 3\n\n[GPU]\n# comment\nRenderer = Vulkan\n\n[New]\nKey = value\n");

	ini.removeSection("GPU");
	CHECK(!ini.hasSection("GPU"));
	CHECK(ini.toString() == "; header\n\n[Main]\nB = two words\nC = 3\n\n[New]\nKey = value\n");

	TempDir tmp;
	ini.save(tmp.path / "sub" / "a.ini");
	CHECK(IniFile::load(tmp.path / "sub" / "a.ini").get("New", "Key") == "value");
	CHECK(!fs::exists(tmp.path / "sub" / "a.ini.tmp"));
}

static void testGamepad()
{
	using namespace es::launcher::util;

	Controller pad = makePad(1, 0);
	CHECK(hotkeyControl(pad) == PadControl::Guide);
	CHECK(padControl("b") == PadControl::South);
	CHECK(padControl("a") == PadControl::East);
	CHECK(!padControl("joystick1up"));

	std::string line = gameControllerConfig({ pad });
	CHECK(line.starts_with("030000005e0400008e02000010010000,Xbox 360 Controller,platform:Linux,"));
	CHECK(line.find(",a:b1,") != std::string::npos);      // ES b is SDL a
	CHECK(line.find(",b:b0,") != std::string::npos);
	CHECK(line.find(",dpup:h0.1,") != std::string::npos);
	CHECK(line.find(",dpleft:h0.8,") != std::string::npos);
	CHECK(line.find(",lefty:a1,") != std::string::npos);
	CHECK(line.find(",lefttrigger:a2,") != std::string::npos);
	CHECK(line.find(",guide:b8,") != std::string::npos);
	CHECK(line.ends_with(","));

	// the hotkey on select: no guide, and select is the hotkey
	pad.inputs["hotkey"] = pad.inputs["select"];
	pad.name = "Pad, with a comma";
	CHECK(hotkeyControl(pad) == PadControl::Back);
	line = gameControllerConfig({ pad, makePad(2, 1) });
	CHECK(line.find("guide:b6") == std::string::npos);
	CHECK(line.find(",back:b6,") != std::string::npos);
	CHECK(line.find("Pad. with a comma,") != std::string::npos);
	CHECK(std::ranges::count(line, '\n') == 1);

	// keyboard keys mean nothing to SDL's gamepads
	Controller keyboard;
	keyboard.guid = "keyboard";
	keyboard.inputs = { { "a", { InputBinding::Type::Key, 97, 1 } } };
	CHECK(gameControllerConfig({ keyboard }) == "keyboard,,platform:Linux,");

	// a Flatpak gets the environment as options, before the application id
	Command cmd;
	cmd.argv = { "/usr/bin/flatpak", "run", "--filesystem=/roms", "org.ppsspp.PPSSPP", "game.iso" };
	cmd.env = { { "XDG_CONFIG_HOME", "/cfg" } };
	forwardEnvironment(cmd);
	CHECK((cmd.argv == std::vector<std::string>{ "/usr/bin/flatpak", "run", "--filesystem=/roms", "--env=XDG_CONFIG_HOME=/cfg", "org.ppsspp.PPSSPP", "game.iso" }));

	cmd.argv = { "/usr/bin/ppsspp", "game.iso" };
	forwardEnvironment(cmd);
	CHECK(cmd.argv.size() == 2);
}

static void testRegistry()
{
	for (const char* name : { "duckstation", "pcsx2", "ppsspp", "dolphin", "azahar", "cemu", "xemu", "xenia-edge" })
		CHECK(Registry::instance().contains(name));
}

static void testDuckStation()
{
	TempDir tmp;
	PathOverride path(tmp.path / "bin");
	writeFile(tmp.path / "bin" / "duckstation-qt", "#!/bin/sh\n", true);

	LaunchContext ctx = makeContext(tmp.path, "duckstation", "psx", "/roms/psx/Crash Bandicoot (USA).chd");
	ctx.settings = Settings({}, { { "duckstation_gfxbackend", "Vulkan" }, { "duckstation_antialiasing", "4-ssaa" } }, {});

	// no BIOS: the player is told
	CHECK(throwsLaunchError(ctx, ErrorCode::Generic));

	writeFile(tmp.path / "bios" / "SCPH5501.BIN", "");
	writeFile(tmp.path / "bios" / "scph5502.bin", "");

	fs::path data = tmp.path / "emulators" / "duckstation" / "duckstation";
	writeFile(data / "settings.ini", "[Main]\nPlayerOwn = yes\n[Pad5]\nType = AnalogController\n");

	Command cmd = Launch(ctx).prepare();
	CHECK(cmd.closeOnHotkey == false);
	CHECK(cmd.argv.front() == (tmp.path / "bin" / "duckstation-qt").string());
	CHECK(contains(cmd.argv, "-batch"));
	CHECK(cmd.argv[cmd.argv.size() - 2] == "--");
	CHECK(cmd.argv.back() == "/roms/psx/Crash Bandicoot (USA).chd");
	CHECK(cmd.env["XDG_CONFIG_HOME"] == (tmp.path / "emulators" / "duckstation").string());
	CHECK(cmd.env["SDL_GAMECONTROLLERCONFIG"].starts_with("030000005e0400008e02000010010000,"));

	IniFile ini = IniFile::load(data / "settings.ini");
	CHECK(ini.get("Main", "PlayerOwn") == "yes");
	CHECK(ini.get("BIOS", "SearchDirectory") == (tmp.path / "bios").string());
	CHECK(ini.get("BIOS", "PathNTSCU") == "SCPH5501.BIN");
	CHECK(ini.get("BIOS", "PathPAL") == "scph5502.bin");
	CHECK(!ini.get("BIOS", "PathNTSCJ"));
	CHECK(ini.get("GPU", "Renderer") == "Vulkan");
	CHECK(ini.get("GPU", "Multisamples") == "4");
	CHECK(ini.get("GPU", "PerSampleShading") == "true");
	CHECK(ini.get("MemoryCards", "Directory") == (tmp.path / "saves" / "psx").string());
	CHECK(ini.get("Pad1", "Type") == "DigitalController");
	CHECK(ini.get("Pad1", "Cross") == "SDL-0/A");
	CHECK(ini.get("Pad1", "LUp") == "SDL-0/-LeftY");
	CHECK(ini.get("Pad5", "Type") == "None");            // no phantom pad
	CHECK(ini.get("Hotkeys", "PowerOff") == "SDL-0/Guide & SDL-0/Start");
	CHECK(ini.get("Hotkeys", "OpenPauseMenu") == "SDL-0/Guide & SDL-0/A");
	CHECK(fs::is_directory(tmp.path / "saves" / "psx"));

	// an .m3u is rewritten with absolute paths
	writeFile(tmp.path / "roms" / "FF7.m3u", "FF7 (Disc 1).chd\n\n# comment\nFF7 (Disc 2).chd\n");
	ctx.game.rom = tmp.path / "roms" / "FF7.m3u";
	cmd = Launch(ctx).prepare();
	fs::path m3u = tmp.path / "emulators" / "duckstation" / "m3u" / "FF7.m3u";
	CHECK(cmd.argv.back() == m3u.string());
	CHECK(readFile(m3u) == (tmp.path / "roms" / "FF7 (Disc 1).chd").string() + "\n" + (tmp.path / "roms" / "FF7 (Disc 2).chd").string() + "\n");

	// without DuckStation
	writeFile(tmp.path / "bin" / "duckstation-qt", "", false);
	fs::permissions(tmp.path / "bin" / "duckstation-qt", fs::perms::owner_read);
	CHECK(throwsLaunchError(ctx, ErrorCode::EmulatorMissing));
}

static void testPcsx2()
{
	TempDir tmp;
	PathOverride path(tmp.path / "bin");
	writeFile(tmp.path / "bin" / "pcsx2-qt", "#!/bin/sh\n", true);

	LaunchContext ctx = makeContext(tmp.path, "pcsx2", "ps2", "/roms/ps2/Okami.iso");
	ctx.controllers = { makePad(1, 0), makePad(2, 1), makePad(3, 2) };
	ctx.settings = Settings({}, { { "pcsx2_multitap", "4" } }, {});

	CHECK(throwsLaunchError(ctx, ErrorCode::Generic));   // no BIOS

	// a small file is not a BIOS
	writeFile(tmp.path / "bios" / "ps2" / "readme.bin", "x");
	writeFile(tmp.path / "bios" / "ps2" / "scph39001.bin", "");
	fs::resize_file(tmp.path / "bios" / "ps2" / "scph39001.bin", 4 * 1024 * 1024);

	Command cmd = Launch(ctx).prepare();
	CHECK(cmd.closeOnHotkey == false);
	CHECK(cmd.argv.front() == (tmp.path / "bin" / "pcsx2-qt").string());
	CHECK(contains(cmd.argv, "-nogui"));
	CHECK(cmd.argv.back() == "/roms/ps2/Okami.iso");
	CHECK(cmd.env["XDG_CONFIG_HOME"] == (tmp.path / "emulators" / "pcsx2").string());

	IniFile ini = IniFile::load(tmp.path / "emulators" / "pcsx2" / "PCSX2" / "inis" / "PCSX2.ini");
	CHECK(ini.get("Folders", "Bios") == (tmp.path / "bios" / "ps2").string());
	CHECK(ini.get("Filenames", "BIOS") == "scph39001.bin");
	CHECK(ini.get("UI", "SetupWizardIncomplete") == "false");
	CHECK(ini.get("Pad", "MultitapPort1") == "true");
	CHECK(ini.get("Pad", "MultitapPort2") == "false");
	// players 2 and 3 are on the multitap of port 1: pads 3 and 4
	CHECK(ini.get("Pad1", "Cross") == "SDL-0/FaceSouth");
	CHECK(!ini.hasSection("Pad2"));
	CHECK(ini.get("Pad3", "Cross") == "SDL-1/FaceSouth");
	CHECK(ini.get("Pad4", "Cross") == "SDL-2/FaceSouth");
	CHECK(ini.get("Hotkeys", "ShutdownVM") == "SDL-0/Guide & SDL-0/Start");
	CHECK(!ini.get("Hotkeys", "Rewind"));

	// PCSX2 is x86_64 only
	ctx.architecture = Architecture::AArch64;
	CHECK(throwsLaunchError(ctx, ErrorCode::Generic));
}

static void testPpsspp()
{
	TempDir tmp;
	PathOverride path(tmp.path / "bin");
	writeFile(tmp.path / "bin" / "ppsspp", "#!/bin/sh\n", true);

	LaunchContext ctx = makeContext(tmp.path, "ppsspp", "psp", "/roms/psp/Patapon.iso");
	ctx.settings = Settings({ { "gfxbackend", "vulkan" }, { "retroachievements", "1" }, { "retroachievements.username", "jyu" },
	                          { "retroachievements.token", "secret" } },
	                        { { "ppsspp.Graphics.FrameRate", "60" } }, {});

	fs::path system = tmp.path / "emulators" / "ppsspp" / "ppsspp" / "PSP" / "SYSTEM";
	writeFile(system / "controls.ini", "[ControlMapping]\nCross = 1-52\nRapid Fire = 1-60\n");
	// a save folder the player already has stays where it is
	fs::create_directories(tmp.path / "emulators" / "ppsspp" / "ppsspp" / "PSP" / "PPSSPP_STATE");

	Command cmd = Launch(ctx).prepare();
	CHECK(cmd.closeOnHotkey == false);
	CHECK(cmd.argv.front() == (tmp.path / "bin" / "ppsspp").string());
	CHECK(cmd.argv[1] == "/roms/psp/Patapon.iso");
	CHECK(cmd.env["XDG_CONFIG_HOME"] == (tmp.path / "emulators" / "ppsspp").string());

	IniFile ini = IniFile::load(system / "ppsspp.ini");
	CHECK(ini.get("Graphics", "GraphicsBackend") == "VULKAN");
	CHECK(ini.get("Graphics", "FrameRate") == "60");
	CHECK(ini.get("SystemParam", "NickName") == "jyu");
	CHECK(ini.get("Achievements", "AchievementsEnable") == "True");
	CHECK(readFile(system / "ppsspp_retroachievements.dat") == "secret");

	IniFile controls = IniFile::load(system / "controls.ini");
	CHECK(controls.get("ControlMapping", "Cross") == "10-189");
	CHECK(controls.get("ControlMapping", "Circle") == "10-190");
	CHECK(controls.get("ControlMapping", "Rapid Fire") == "1-60");
	CHECK(controls.get("ControlMapping", "An.Up") == "10-4003");
	CHECK(controls.get("ControlMapping", "Exit App") == "10-4:10-197");

	fs::path psp = system.parent_path();
	CHECK(fs::is_symlink(psp / "SAVEDATA"));
	CHECK(fs::read_symlink(psp / "SAVEDATA") == tmp.path / "saves" / "psp" / "SAVEDATA");
	CHECK(fs::is_directory(tmp.path / "saves" / "psp" / "SAVEDATA"));
	CHECK(!fs::is_symlink(psp / "PPSSPP_STATE"));
}

static void testDolphin()
{
	TempDir tmp;
	PathOverride path(tmp.path / "bin");
	writeFile(tmp.path / "bin" / "dolphin-emu", "#!/bin/sh\n", true);

	LaunchContext ctx = makeContext(tmp.path, "dolphin", "gamecube", "/roms/gamecube/Pikmin.rvz");
	ctx.controllers = { makePad(1, 3), makePad(2, 1) };   // the same pad twice
	ctx.controllers[0].inputs["hotkey"] = ctx.controllers[0].inputs["select"];

	fs::path user = tmp.path / "emulators" / "dolphin";
	writeFile(user / "Config" / "Dolphin.ini", "[Interface]\nThemeName = Clean\n");

	Command cmd = Launch(ctx).prepare();
	CHECK(cmd.closeOnHotkey == false);
	CHECK(cmd.argv.front() == (tmp.path / "bin" / "dolphin-emu").string());
	CHECK(contains(cmd.argv, "-b"));
	CHECK(contains(cmd.argv, user.string()));
	CHECK(!contains(cmd.argv, "-C"));
	CHECK(cmd.argv[cmd.argv.size() - 2] == "-e");
	CHECK(cmd.argv.back() == "/roms/gamecube/Pikmin.rvz");

	IniFile dolphin = IniFile::load(user / "Config" / "Dolphin.ini");
	CHECK(dolphin.get("Interface", "ThemeName") == "Clean");
	CHECK(dolphin.get("Core", "GFXBackend") == "OGL");
	CHECK(dolphin.get("Core", "SIDevice0") == "6");
	CHECK(dolphin.get("Core", "SkipIPL") == "True");

	IniFile pads = IniFile::load(user / "Config" / "GCPadNew.ini");
	// player 1 has the higher device index: SDL opened it second
	CHECK(pads.get("GCPad1", "Device") == "SDL/1/Xbox 360 Controller");
	CHECK(pads.get("GCPad2", "Device") == "SDL/0/Xbox 360 Controller");
	CHECK(pads.get("GCPad1", "Buttons/A") == "`Button E`");
	CHECK(pads.get("GCPad1", "Main Stick/Up") == "`Left Y+`");
	CHECK(!pads.hasSection("GCPad3"));

	IniFile hotkeys = IniFile::load(user / "Config" / "Hotkeys.ini");
	CHECK(hotkeys.get("Hotkeys", "Device") == "SDL/1/Xbox 360 Controller");
	CHECK(hotkeys.get("Hotkeys", "General/Exit") == "@(`Back`+`Start`)");
	CHECK(!fs::exists(user / "Config" / "WiimoteNew.ini"));

	CHECK(fs::read_symlink(user / "GC") == tmp.path / "saves" / "gamecube" / "GC");
	CHECK(fs::read_symlink(user / "Wii") == tmp.path / "saves" / "wii" / "Wii");

	// the Wii: emulated Wii Remotes, no GameCube pad, its own settings on the command line
	ctx.game.system = "wii";
	ctx.game.rom = "/roms/wii/Zelda.rvz";
	ctx.settings = Settings({}, { { "controller_mode", "in" }, { "tv_mode", "1" } }, {});
	cmd = Launch(ctx).prepare();
	CHECK(contains(cmd.argv, "SYSCONF.IPL.AR=1"));

	IniFile wiimotes = IniFile::load(user / "Config" / "WiimoteNew.ini");
	CHECK(wiimotes.get("Wiimote1", "Source") == "1");
	CHECK(wiimotes.get("Wiimote1", "Extension") == "Nunchuk");
	CHECK(wiimotes.get("Wiimote1", "Nunchuk/Stick/Up") == "`Left Y+`");
	CHECK(wiimotes.get("Wiimote1", "IR/Up") == "`Right Y+`");
	CHECK(wiimotes.get("Wiimote3", "Source") == "0");
	dolphin = IniFile::load(user / "Config" / "Dolphin.ini");
	CHECK(dolphin.get("Core", "SIDevice0") == "0");

	// a GameCube emulator does not run PlayStation games
	ctx.game.system = "psx";
	CHECK(throwsLaunchError(ctx, ErrorCode::InvalidConfiguration));
}

static void testXemu()
{
	TempDir tmp;
	PathOverride path(tmp.path / "bin");
	writeFile(tmp.path / "bin" / "xemu", "#!/bin/sh\n", true);

	LaunchContext ctx = makeContext(tmp.path, "xemu", "xbox", "/roms/xbox/Halo \"CE\".iso");
	ctx.screenWidth = 1920;
	ctx.screenHeight = 1080;

	CHECK(throwsLaunchError(ctx, ErrorCode::Generic));   // no BIOS
	writeFile(tmp.path / "bios" / "mcpx_1.0.bin", "");
	writeFile(tmp.path / "bios" / "Complex_4627.bin", "");
	CHECK(throwsLaunchError(ctx, ErrorCode::Generic));   // no hard disk
	writeFile(tmp.path / "bios" / "xbox_hdd.qcow2", "disk");

	fs::path config = tmp.path / "emulators" / "xemu" / "xemu.toml";
	writeFile(config, "[input.bindings]\nport3 = \"old\"\n\n[general]\nuser_setting = true\n");

	Command cmd = Launch(ctx).prepare();
	CHECK(cmd.closeOnHotkey == true);
	CHECK((cmd.argv == std::vector<std::string>{ (tmp.path / "bin" / "xemu").string(), "-config_path", config.string() }));
	CHECK(readFile(tmp.path / "saves" / "xbox" / "xbox_hdd.qcow2") == "disk");
	CHECK(cmd.env["LC_NUMERIC"] == "C");

	IniFile toml = IniFile::load(config);
	CHECK(toml.get("general", "user_setting") == "true");
	CHECK(toml.get("sys.files", "dvd_path") == "\"/roms/xbox/Halo \\\"CE\\\".iso\"");
	CHECK(toml.get("sys.files", "bootrom_path") == "\"" + (tmp.path / "bios" / "mcpx_1.0.bin").string() + "\"");
	CHECK(toml.get("display.window", "startup_size") == "\"1920x1080\"");
	CHECK(toml.get("input.bindings", "port1") == "\"030000005e0400008e02000010010000\"");
	CHECK(!toml.get("input.bindings", "port3"));

	// the player's disk is never replaced
	writeFile(tmp.path / "saves" / "xbox" / "xbox_hdd.qcow2", "mine");
	Launch(ctx).prepare();
	CHECK(readFile(tmp.path / "saves" / "xbox" / "xbox_hdd.qcow2") == "mine");

	ctx.architecture = Architecture::AArch64;
	CHECK(throwsLaunchError(ctx, ErrorCode::Generic));
}

static void testXenia()
{
	TempDir tmp;
	PathOverride path(tmp.path / "bin");
	writeFile(tmp.path / "bin" / "xenia_edge", "#!/bin/sh\n", true);

	// a playlist names the game, relative to itself
	writeFile(tmp.path / "roms" / "Game.xbox360", "/Game/default.xex\n");
	writeFile(tmp.path / "roms" / "Game" / "default.xex", "");

	LaunchContext ctx = makeContext(tmp.path, "xenia-edge", "xbox360", tmp.path / "roms" / "Game.xbox360");
	ctx.settings = Settings({}, { { "xenia_edge_resolution_scale", "2" } }, {});

	Command cmd = Launch(ctx).prepare();
	CHECK(cmd.closeOnHotkey == true);
	CHECK(cmd.argv.size() == 2);
	CHECK(cmd.argv[1] == (tmp.path / "roms" / "Game" / "default.xex").string());
	CHECK(cmd.env["XDG_DATA_HOME"] == (tmp.path / "emulators" / "xenia-edge").string());

	IniFile toml = IniFile::load(tmp.path / "emulators" / "xenia-edge" / "Xenia" / "xenia-edge.config.toml");
	CHECK(toml.get("GPU", "draw_resolution_scale_x") == "2");
	CHECK(toml.get("GPU.Debug", "texture_cache_memory_limit_hard") == "768");
	CHECK(toml.get("Storage", "content_root") == "\"" + (tmp.path / "saves" / "xbox360").string() + "\"");
	CHECK(toml.get("HID", "hid") == "\"sdl\"");

	// a playlist naming nothing that exists
	writeFile(tmp.path / "roms" / "Gone.xbox360", "Gone/default.xex\n");
	ctx.game.rom = tmp.path / "roms" / "Gone.xbox360";
	CHECK(throwsLaunchError(ctx, ErrorCode::Generic));
}

static void testAzahar()
{
	TempDir tmp;
	PathOverride path(tmp.path / "bin");
	writeFile(tmp.path / "bin" / "azahar", "#!/bin/sh\n", true);

	LaunchContext ctx = makeContext(tmp.path, "azahar", "3ds", "/roms/3ds/Zelda.3ds");
	ctx.settings = Settings({}, { { "azahar_screen_layout", "1-true" } }, {});

	Command cmd = Launch(ctx).prepare();
	CHECK(cmd.closeOnHotkey == true);
	CHECK(cmd.argv.back() == "/roms/3ds/Zelda.3ds");
	CHECK(cmd.env["XDG_CONFIG_HOME"] == (tmp.path / "emulators" / "azahar").string());
	CHECK(cmd.env["XDG_DATA_HOME"] == (tmp.path / "saves" / "3ds").string());

	IniFile ini = IniFile::load(tmp.path / "emulators" / "azahar" / "azahar-emu" / "qt-config.ini");
	CHECK(ini.get("Layout", "layout_option") == "1");
	CHECK(ini.get("Layout", "swap_screen") == "true");
	CHECK(ini.get("Layout", "swap_screen\\default") == "false");
	CHECK(ini.get("Controls", "profiles\\size") == "1");
	std::string guid = "guid:030000005e0400008e02000010010000,port:0,engine:sdl";
	CHECK(ini.get("Controls", "profiles\\1\\button_a") == "\"button:0," + guid + "\"");
	CHECK(ini.get("Controls", "profiles\\1\\button_up") == "\"hat:0,direction:up," + guid + "\"");
	CHECK(ini.get("Controls", "profiles\\1\\button_zl") == "\"axis:2,direction:+,threshold:0.5," + guid + "\"");
	CHECK(ini.get("Controls", "profiles\\1\\circle_pad") == "\"axis_x:0,axis_y:1," + guid + "\"");
	CHECK(ini.get("Controls", "profiles\\1\\c_stick") == "\"\"");   // the test pad has no right stick
}

static void testCemu()
{
	TempDir tmp;
	PathOverride path(tmp.path / "bin");
	writeFile(tmp.path / "bin" / "cemu", "#!/bin/sh\n", true);
	writeFile(tmp.path / "bios" / "cemu" / "keys.txt", "keys");
	writeFile(tmp.path / "roms" / "Zelda" / "code" / "U-King.rpx", "");
	writeFile(tmp.path / "roms" / "Zelda" / "content" / "data.bin", "");

	LaunchContext ctx = makeContext(tmp.path, "cemu", "wiiu", tmp.path / "roms" / "Zelda");
	ctx.controllers = { makePad(1, 1), makePad(2, 0) };

	fs::path config = tmp.path / "emulators" / "cemu" / "Cemu";
	writeFile(config / "settings.xml", "<?xml version=\"1.0\"?>\n<content><GamePaths><Entry>/elsewhere</Entry></GamePaths><Graphic><api>0</api><Shaders>x</Shaders></Graphic></content>\n");
	writeFile(config / "controllerProfiles" / "controller5.xml", "old");

	Command cmd = Launch(ctx).prepare();
	CHECK(cmd.closeOnHotkey == true);
	CHECK(cmd.argv.back() == (tmp.path / "roms" / "Zelda" / "code" / "U-King.rpx").string());
	CHECK(contains(cmd.argv, "-f"));

	pugi::xml_document settings;
	CHECK(settings.load_file((config / "settings.xml").c_str()));
	pugi::xml_node content = settings.child("content");
	CHECK(std::string(content.child("mlc_path").text().get()) == (tmp.path / "saves" / "wiiu" / "mlc01").string());
	CHECK(std::string(content.child("Graphic").child("api").text().get()) == "1");
	CHECK(std::string(content.child("Graphic").child("Shaders").text().get()) == "x");   // kept
	CHECK(std::distance(content.child("GamePaths").children("Entry").begin(), content.child("GamePaths").children("Entry").end()) == 2);

	pugi::xml_document pad;
	CHECK(pad.load_file((config / "controllerProfiles" / "controller0.xml").c_str()));
	CHECK(std::string(pad.child("emulated_controller").child("type").text().get()) == "Wii U GamePad");
	// player 1 has the higher index of two identical pads: the second of that GUID
	CHECK(std::string(pad.child("emulated_controller").child("controller").child("uuid").text().get()) == "1_030000005e0400008e02000010010000");
	pad.load_file((config / "controllerProfiles" / "controller1.xml").c_str());
	CHECK(std::string(pad.child("emulated_controller").child("type").text().get()) == "Wii U Pro Controller");
	CHECK(!fs::exists(config / "controllerProfiles" / "controller5.xml"));

	CHECK(fs::read_symlink(tmp.path / "saves" / "wiiu" / "Cemu" / "keys.txt") == tmp.path / "bios" / "cemu" / "keys.txt");
}

static void testYamlFile()
{
	YamlFile yaml = YamlFile::parse(
		"Core:\n"
		"  PPU Decoder: Recompiler (LLVM)\n"
		"  Libraries Control: []\n"
		"  List:\n"
		"    - one\n"
		"    - two: 2\n"
		"      three: 3\n"
		"  Seq at key indent:\n"
		"  - a\n"
		"Video:\n"
		"  Renderer: Vulkan\n"
		"  Vulkan:\n"
		"    Adapter: \"My GPU: fast\"\n"
		"VFS:\n"
		"  /dev_hdd0/: /old/\n"
		"  Enable: true\n"
		"Empty:\n");
	CHECK(yaml.get({ "Core", "PPU Decoder" }) == "Recompiler (LLVM)");
	CHECK(yaml.get({ "Video", "Vulkan", "Adapter" }) == "My GPU: fast");
	CHECK(!yaml.get({ "Core", "List" }));   // kept as written
	CHECK((yaml.keys({ "VFS" }) == std::vector<std::string>{ "/dev_hdd0/", "Enable" }));

	yaml.set({ "Video", "Renderer" }, "OpenGL");
	yaml.set({ "Video", "Handler" }, "Null");
	yaml.set({ "Video", "Empty" }, "");
	yaml.set({ "Audio", "Master Volume" }, 100);
	yaml.set({ "Audio", "Enabled" }, true);
	yaml.remove({ "VFS", "/dev_hdd0/" });
	CHECK(yaml.toString() ==
		"Core:\n"
		"  PPU Decoder: Recompiler (LLVM)\n"
		"  Libraries Control: []\n"
		"  List:\n"
		"    - one\n"
		"    - two: 2\n"
		"      three: 3\n"
		"  Seq at key indent:\n"
		"  - a\n"
		"Video:\n"
		"  Renderer: OpenGL\n"
		"  Vulkan:\n"
		"    Adapter: \"My GPU: fast\"\n"
		"  Handler: \"Null\"\n"
		"  Empty: \"\"\n"
		"VFS:\n"
		"  Enable: true\n"
		"Empty:\n"
		"Audio:\n"
		"  Master Volume: 100\n"
		"  Enabled: true\n");

	// a re-read gives the same file
	CHECK(YamlFile::parse(yaml.toString()).toString() == yaml.toString());
	CHECK(YamlFile::quote("a \"b\"") == "a \"b\"");
	CHECK(YamlFile::quote("\"b\"") == "\"\\\"b\\\"\"");
	CHECK(YamlFile::quote("true") == "\"true\"");
}

static void testCommandBefore()
{
	Command cmd;
	cmd.argv = { "rpcs3", "game" };
	cmd.env = { { "A", "1" } };
	cmd.before = { { "rpcs3", "--installfw", "x" } };
	CHECK(cmd.toShellLine() == "{ A='1' 'rpcs3' '--installfw' 'x' ; A='1' 'rpcs3' 'game' ; }");
	cmd.workingDirectory = "/tmp";
	CHECK(cmd.toShellLine() == "cd '/tmp' && { A='1' 'rpcs3' '--installfw' 'x' ; A='1' 'rpcs3' 'game' ; }");
}

static void testRpcs3()
{
	TempDir tmp;
	PathOverride path(tmp.path / "bin");
	writeFile(tmp.path / "bin" / "rpcs3", "#!/bin/sh\n", true);
	writeFile(tmp.path / "roms" / "Demon.ps3" / "PS3_GAME" / "USRDIR" / "EBOOT.BIN", "");

	LaunchContext ctx = makeContext(tmp.path, "rpcs3", "ps3", tmp.path / "roms" / "Demon.ps3");
	ctx.controllers = { makePad(1, 0), makePad(2, 1) };
	ctx.settings = Settings({}, { { "rpcs3_framelimit", "45" }, { "rpcs3_shadermode", "Shader Recompiler" } }, {});

	// no firmware installed nor to install
	CHECK(throwsLaunchError(ctx, ErrorCode::Generic));

	writeFile(tmp.path / "bios" / "PS3UPDAT.PUP", "pup");
	fs::path config = tmp.path / "emulators" / "rpcs3" / "rpcs3";
	writeFile(config / "config.yml", "Core:\n  PPU Decoder: Interpreter\n  Libraries Control: []\nVideo:\n  VSync: true\nVFS:\n  /dev_hdd0/: /old/\n  Enable /host_root/: false\n");

	Command cmd = Launch(ctx).prepare();
	CHECK(cmd.closeOnHotkey == true);
	CHECK((cmd.argv == std::vector<std::string>{ (tmp.path / "bin" / "rpcs3").string(),
		(tmp.path / "roms" / "Demon.ps3" / "PS3_GAME" / "USRDIR" / "EBOOT.BIN").string(), "--no-gui" }));
	CHECK(cmd.before.size() == 1);
	CHECK((cmd.before[0] == std::vector<std::string>{ (tmp.path / "bin" / "rpcs3").string(), "--headless", "--installfw",
		(tmp.path / "bios" / "PS3UPDAT.PUP").string() }));
	CHECK(cmd.env["XDG_CONFIG_HOME"] == (tmp.path / "emulators" / "rpcs3").string());
	CHECK(fs::is_directory(tmp.path / "saves" / "ps3" / "dev_hdd0"));

	YamlFile yaml = YamlFile::load(config / "config.yml");
	CHECK(yaml.get({ "Core", "PPU Decoder" }) == "Recompiler (LLVM)");
	CHECK(yaml.toString().find("Libraries Control: []") != std::string::npos);   // kept
	CHECK(yaml.get({ "Video", "Frame limit" }) == "Off");
	CHECK(yaml.get({ "Video", "Second Frame Limit" }) == "45");
	CHECK(yaml.get({ "Video", "Shader Mode" }) == "Legacy Recompiler (single-threaded)");
	CHECK(!yaml.get({ "Video", "VSync" }));
	CHECK((yaml.keys({ "VFS" }) == std::vector<std::string>{ "Enable /host_root/" }));
	CHECK(yaml.get({ "Miscellaneous", "Exit RPCS3 when process finishes" }) == "true");

	YamlFile vfs = YamlFile::load(config / "vfs.yml");
	CHECK(vfs.get({ "/dev_hdd0/" }) == (tmp.path / "saves" / "ps3" / "dev_hdd0").string() + "/");
	CHECK(vfs.get({ "/dev_usb***/", "/dev_usb000", "Path" }) == "$(EmulatorDir)dev_usb000/");

	YamlFile input = YamlFile::load(config / "input_configs" / "global" / "Default.yml");
	CHECK(input.get({ "Player 1 Input", "Handler" }) == "SDL");
	CHECK(input.get({ "Player 1 Input", "Device" }) == "Xbox 360 Controller 1");
	CHECK(input.get({ "Player 2 Input", "Device" }) == "Xbox 360 Controller 2");
	CHECK(input.get({ "Player 1 Input", "Config", "Cross" }) == "South");
	CHECK(input.get({ "Player 1 Input", "Config", "Motion Sensor G", "Axis" }) == "RY");

	IniFile gui = IniFile::load(config / "GuiConfigs" / "CurrentSettings.ini");
	CHECK(gui.get("main_window", "infoBoxEnabledWelcome") == "false");

	// installed firmware: no install step; a .psn file names a game installed on dev_hdd0
	writeFile(config / "dev_flash" / "vsh" / "etc" / "version.txt", "release:04.92\n");
	writeFile(tmp.path / "saves" / "ps3" / "dev_hdd0" / "game" / "NPUB30001" / "USRDIR" / "EBOOT.BIN", "");
	writeFile(tmp.path / "roms" / "Flower.psn", "npub30001\n");
	ctx.game.rom = tmp.path / "roms" / "Flower.psn";
	ctx.settings = Settings({}, { { "rpcs3_gui", "1" } }, {});
	cmd = Launch(ctx).prepare();
	CHECK(cmd.before.empty());
	CHECK(cmd.argv.back() == (tmp.path / "saves" / "ps3" / "dev_hdd0" / "game" / "NPUB30001" / "USRDIR" / "EBOOT.BIN").string());

	// a game that is not there
	ctx.game.rom = tmp.path / "roms" / "Missing.ps3";
	CHECK(throwsLaunchError(ctx, ErrorCode::BadArguments));

	ctx.architecture = Architecture::AArch64;
	CHECK(throwsLaunchError(ctx, ErrorCode::Generic));
}

static void testEden()
{
	TempDir tmp;
	PathOverride path(tmp.path / "bin");
	writeFile(tmp.path / "bin" / "eden", "#!/bin/sh\n", true);

	LaunchContext ctx = makeContext(tmp.path, "eden", "switch", "/roms/switch/Zelda.nsp");
	ctx.controllers = { makePad(1, 1), makePad(2, 0) };
	ctx.controllers[0].guid = "03001234" "5e0400008e02000010010000";   // a name CRC, as SDL 3 makes them

	// no keys
	CHECK(throwsLaunchError(ctx, ErrorCode::Generic));
	writeFile(tmp.path / "bios" / "switch" / "prod.keys", "keys");

	Command cmd = Launch(ctx).prepare();
	CHECK(cmd.closeOnHotkey == false);   // Eden quits on hotkey + Start itself
	CHECK((std::vector<std::string>(cmd.argv.end() - 3, cmd.argv.end()) == std::vector<std::string>{ "-f", "-g", "/roms/switch/Zelda.nsp" }));
	CHECK(cmd.env["XDG_CONFIG_HOME"] == (tmp.path / "emulators" / "eden").string());
	CHECK(cmd.env["XDG_DATA_HOME"] == (tmp.path / "saves" / "switch").string());
	CHECK(fs::read_symlink(tmp.path / "saves" / "switch" / "eden" / "keys") == tmp.path / "bios" / "switch");

	IniFile ini = IniFile::load(tmp.path / "emulators" / "eden" / "eden" / "qt-config.ini");
	CHECK(ini.get("Renderer", "backend") == "1");
	CHECK(ini.get("Renderer", "backend\\default") == "false");
	CHECK(ini.get("UI", "fullscreen") == "true");
	CHECK(ini.get("UI", "Shortcuts\\Main%20Window\\Exit%20Eden\\Controller_KeySeq") == "Home+Plus");
	CHECK(ini.get("UI", "Shortcuts\\Main%20Window\\Continue\\Pause%20Emulation\\Controller_KeySeq") == "Home+B");
	CHECK(ini.get("UI", "Shortcuts\\Main%20Window\\Fullscreen\\Controller_KeySeq") == "");

	// the CRC is not part of Eden's GUID; player 1 is the second pad of that GUID
	std::string pad1 = "engine:sdl,port:1,guid:030000005e0400008e02000010010000";
	std::string pad2 = "engine:sdl,port:0,guid:030000005e0400008e02000010010000";
	CHECK(ini.get("Controls", "player_0_connected") == "true");
	CHECK(ini.get("Controls", "player_0_button_a") == "\"" + pad1 + ",button:0\"");
	CHECK(ini.get("Controls", "player_0_button_dup") == "\"" + pad1 + ",hat:0,direction:up\"");
	CHECK(ini.get("Controls", "player_0_button_zl") == "\"" + pad1 + ",axis:2,threshold:0.500000,invert:+\"");
	CHECK(ini.get("Controls", "player_0_button_home") == "\"" + pad1 + ",button:8\"");
	CHECK(ini.get("Controls", "player_0_lstick") == "\"" + pad1 + ",axis_x:0,axis_y:1,offset_x:0.000000,offset_y:0.000000,invert_x:+,invert_y:+,deadzone:0.150000,range:1.000000\"");
	CHECK(ini.get("Controls", "player_1_button_b") == "\"" + pad2 + ",button:1\"");
	CHECK(ini.get("Controls", "player_2_connected") == "false");

	// hotkey on select: the shortcuts use Minus, Home stays unbound
	ctx.controllers[0].inputs["hotkey"] = ctx.controllers[0].inputs["select"];
	Launch(ctx).prepare();
	ini = IniFile::load(tmp.path / "emulators" / "eden" / "eden" / "qt-config.ini");
	CHECK(ini.get("UI", "Shortcuts\\Main%20Window\\Exit%20Eden\\Controller_KeySeq") == "Minus+Plus");
	CHECK(ini.get("Controls", "player_0_button_home") == "\"\"");

	CHECK(es::launcher::eden::EdenGenerator::edenGuid("0300ABCD5E0400008E02000010010000") == "030000005e0400008e02000010010000");
}

void tests::runStandaloneTests()
{
	testIniFile();
	testGamepad();
	testRegistry();
	testDuckStation();
	testPcsx2();
	testPpsspp();
	testDolphin();
	testXemu();
	testXenia();
	testAzahar();
	testCemu();
	testYamlFile();
	testCommandBefore();
	testRpcs3();
	testEden();
}
