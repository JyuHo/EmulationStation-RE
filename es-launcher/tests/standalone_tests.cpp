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

#include <algorithm>
#include <string>

using namespace tests;
using es::launcher::util::IniFile;

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
	for (const char* name : { "duckstation", "pcsx2", "ppsspp", "dolphin" })
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

void tests::runStandaloneTests()
{
	testIniFile();
	testGamepad();
	testRegistry();
	testDuckStation();
	testPcsx2();
	testPpsspp();
	testDolphin();
}
