es-launcher
===========

es-launcher configures an emulator for one game and returns the command that starts it.
It replaces, inside EmulationStation-RE, what Batocera does with its Python
`batocera-launch` (formerly `configgen`): write the emulator's configuration (controllers,
video, per-game options), then run it.

License
-------

es-launcher is licensed under the **GNU LGPL 3.0 or later** ([COPYING.LESSER](COPYING.LESSER),
which builds on [COPYING](COPYING)). The rest of EmulationStation-RE stays under the MIT license.

The generators are adapted from `batocera-launch`, which is LGPL 3.0 or later, so they stay
under that license. That is why es-launcher is its own component and is built as a shared
library (`libes-launcher.so`). Code may move from ES (MIT) into es-launcher, never the other
way round.

How ES uses it
--------------

es-launcher does not depend on es-core or es-app. Right before a launch, ES fills a
`LaunchContext` and hands it over:

```cpp
#include <es-launcher/Launcher.h>

es::launcher::LaunchContext ctx;
ctx.game.rom = "/home/me/roms/psx/game.chd";
ctx.game.system = "psx";
ctx.emulator = "duckstation";
ctx.settings = es::launcher::Settings(globalOptions, systemOptions, gameOptions);
ctx.controllers = ...;                 // from SDL3, in player order
ctx.paths.emulatorConfigs = ...;       // $XDG_CONFIG_HOME/emulationstation/emulators

es::launcher::Launch launch(std::move(ctx));
if (!launch.handled())
{
    // no generator for this emulator: run the es_systems.cfg command as before
}

try
{
    es::launcher::Command cmd = launch.prepare();
    // run cmd.argv with cmd.env in cmd.workingDirectory, wait for it to exit
    launch.finish();
}
catch (const es::launcher::LaunchError& e)
{
    // e.exitCode() is one of the codes FileData::getMessageFromExitCode() explains, e.what() the message
}
```

| Type | Role |
| --- | --- |
| `LaunchContext` | The game, the chosen emulator and core, the player's options, the controllers, the paths |
| `Settings` | Options looked up in the game, then system, then global layer; `auto` and empty mean "not set" |
| `Generator` | One per emulator: `configure()` writes its configuration and returns a `Command`, `cleanup()` runs after it exits |
| `Registry` | Generators by emulator name, as in `<emulator name="...">` of `es_systems.cfg` |
| `Command` | `argv`, extra environment variables and working directory of the emulator process |
| `LaunchError` | Thrown when the game cannot start; carries the exit code ES already translates (200 to 205, 250) |
| `Launch` | One launch from `prepare()` to `finish()` |

Generators
----------

| Emulator name (`es_systems.cfg`) | Generator | Notes |
| --- | --- | --- |
| `libretro`, `retroarch` | RetroArch | Core profiles for gambatte, mgba, melondsds, mesen, fceumm, snes9x, mupen64plus-next and flycast; any other core runs with the generic settings |
| `duckstation` | DuckStation | PlayStation |
| `pcsx2` | PCSX2 | PlayStation 2, x86_64 only |
| `ppsspp` | PPSSPP | PSP |
| `dolphin` | Dolphin | GameCube and Wii |
| `azahar` | Azahar | Nintendo 3DS |
| `cemu` | Cemu | Wii U, x86_64 only |
| `xemu` | xemu | Xbox, x86_64 only |
| `xenia-edge` | Xenia Edge | Xbox 360, x86_64 only |

Any other emulator keeps the `es_systems.cfg` command. Setting `eslauncher=0` in the ES
configuration (`global.eslauncher`, `<system>.eslauncher` or for one game) also keeps it.

### RetroArch

* Configuration: `$XDG_CONFIG_HOME/emulationstation/emulators/retroarch/retroarch.cfg` and
  `retroarch-core-options.cfg`. They are separate from `~/.config/retroarch`. ES rewrites the
  keys it manages at each launch and leaves the others as the player set them in the RetroArch menu.
* `<system>.cfg` and `<system>/<rom file name>.cfg` in the same folder are appended when they exist.
* Options `retroarch.<key>` and `retroarchcore.<key>` of the ES configuration are copied as they are.
* Cores are looked up in `$ES_LIBRETRO_CORES_DIR`, `~/.config/retroarch/cores`, then
  `/usr/lib/libretro` (Arch) and `/usr/lib/<arch>-linux-gnu/libretro` (Debian, Armbian).
* Saves and save states go to `<saves>/<system>/`, the BIOS folder is RetroArch's system folder.
* Hotkeys: hold the hotkey button of player 1, then Start quits, B opens the menu, Y/X save and load
  a state, Up/Down change the slot, Left rewinds, Right fast-forwards, A resets.

### Standalone emulators

Each one has its own folder in `$XDG_CONFIG_HOME/emulationstation/emulators/<name>/`, never the
player's own (`~/.config/dolphin-emu`...). ES rewrites the keys it manages at each launch and
leaves the others as the player set them in the emulator's menus.

| Emulator | Settings | Saves |
| --- | --- | --- |
| DuckStation | `duckstation/duckstation/settings.ini` (`XDG_CONFIG_HOME` is `duckstation/`) | memory cards in `<saves>/psx`, states in `<saves>/psx/duckstation` |
| PCSX2 | `pcsx2/PCSX2/inis/PCSX2.ini` (`XDG_CONFIG_HOME` is `pcsx2/`) | `<saves>/ps2/pcsx2` |
| PPSSPP | `ppsspp/ppsspp/PSP/SYSTEM/` (`XDG_CONFIG_HOME` is `ppsspp/`) | `PSP/SAVEDATA` and `PSP/PPSSPP_STATE` link to `<saves>/psp` |
| Dolphin | `dolphin/Config/` (started with `-u dolphin/`) | `GC/` and `Wii/` link to `<saves>/gamecube/GC` and `<saves>/wii/Wii`, `StateSaves/` to `<saves>/dolphin/StateSaves` |
| Azahar | `azahar/azahar-emu/qt-config.ini` (`XDG_CONFIG_HOME` is `azahar/`) | `XDG_DATA_HOME` is `<saves>/3ds` |
| Cemu | `cemu/Cemu/settings.xml`, `controllerProfiles/` | `XDG_DATA_HOME` is `<saves>/wiiu`, the Wii U storage `<saves>/wiiu/mlc01` |
| xemu | `xemu/xemu.toml` (started with `-config_path`) | hard disk and EEPROM in `<saves>/xbox` |
| Xenia Edge | `xenia-edge/Xenia/xenia-edge.config.toml` (`XDG_DATA_HOME` is `xenia-edge/`) | `<saves>/xbox360` |

* BIOS: DuckStation looks at the root of the BIOS folder, then `psx/` (scph1001.bin, scph5501.bin...);
  PCSX2 in `ps2/`, then the root; xemu needs `mcpx_1.0.bin` and `Complex_4627.bin` at the root,
  and copies `xbox_hdd.qcow2` from there to `<saves>/xbox` the first time. Cemu's `keys.txt` is
  linked from `cemu/keys.txt` or the root. A missing BIOS stops the launch with a message.
* Options keep the names of batocera-launch (`duckstation_gfxbackend`, `pcsx2_ratio`,
  `internal_resolution`...), so the same `es_features.cfg` entries work. For PPSSPP,
  `ppsspp.<section>.<key>` sets any key of `ppsspp.ini`.
* Pads: the emulators read them through SDL. ES passes its own mapping of each pad in
  `SDL_GAMECONTROLLERCONFIG`, so the generators bind SDL's gamepad names and any pad works the way
  it was mapped in ES. A Flatpak gets this environment through `--env=` options.
* Hotkeys: the same as RetroArch, bound in the emulator to player 1's hotkey held with the other
  button (Start quits, B opens the menu or pauses, Y/X save and load a state...), where the
  emulator has the action. Without a pad, the keyboard keys of batocera-launch. Azahar, Cemu,
  xemu and Xenia cannot bind actions to a pad, so their command sets `closeOnHotkey`: while the
  game runs, ES watches the pads itself and closes the emulator when a player holds the hotkey
  and presses Start (SIGTERM to its process group, SIGKILL after 5 seconds). `exithotkey=0`
  turns this off. Their other hotkeys stay in the emulator's own menus.
* Azahar binds player 1's pad input by input (button numbers and SDL GUID), as batocera-launch does.
* Wii: emulated Wii Remotes by default; `emulatedwiimotes=0` for real ones. `controller_mode` adds
  an extension: `cc` or `pro` (Classic Controller), `in` (Nunchuk), `side` (held sideways).

Writing a generator
-------------------

```cpp
#include <es-launcher/Generator.h>
#include <es-launcher/LaunchError.h>

class HatariGenerator : public es::launcher::Generator
{
public:
    es::launcher::Command configure(const es::launcher::LaunchContext& ctx) override
    {
        auto configDir = ctx.paths.configFor("hatari");
        // ... write configDir / "hatari.cfg" from ctx.settings and ctx.controllers ...
        return { { "hatari", "--fullscreen", "--disk-a", ctx.game.rom.string() } };
    }
};

ES_LAUNCHER_REGISTER_GENERATOR("hatari", HatariGenerator)
```

Rules for generators:

* Never hard-code a path: everything comes from `ctx.paths`.
* Find the emulator with `util::emulatorCommand(ctx, "<id>", "<binary>", { paths it reads or writes })`:
  it returns what ES's emulator catalog installed (an AppImage, or `flatpak run` with access to
  those paths), else the binary found in `PATH`. Append the emulator's arguments to it.
* Write only in `ctx.paths.configFor("<emulator>")`, never in the player's own emulator configuration.
* Throw `LaunchError` with a message the player can act on (which BIOS is missing, which package to install).

Building and testing
--------------------

es-launcher is built with the rest of ES. To build it alone and run its tests:

```bash
cmake -S es-launcher -B build-launcher
cmake --build build-launcher
ctest --test-dir build-launcher
```
