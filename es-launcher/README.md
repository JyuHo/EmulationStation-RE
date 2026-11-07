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
