EmulationStation
================

EmulationStation is a cross-platform graphical front-end for emulators with controller navigation.

Building
========

EmulationStation-RE targets Linux only, on two platforms:

* x86_64 (Arch Linux / CachyOS)
* Raspberry Pi 4 and later (Armbian trixie)

The code is built as C++23, so you need GCC 13+ or Clang 18+ and CMake 3.25+. You also need SDL 3.2+, FreeImage, FreeType, cURL, RapidJSON, LibVLC, ALSA and gettext. FriBidi, libudev and libcec are optional. Audio decoding (MP3, Ogg Vorbis, FLAC, WAV) is built in, so SDL_mixer is not needed. The `fonts-droid-fallback` package (or equivalent) provides fallback fonts for Chinese/Japanese/Korean characters at run time.

**On Arch Linux / CachyOS:**
```bash
sudo pacman -S --needed base-devel cmake ninja sdl3 freeimage freetype2 curl \
  rapidjson vlc alsa-lib gettext fribidi libcec
```

**On Armbian trixie / Debian:**
```bash
sudo apt-get install build-essential cmake ninja-build libsdl3-dev \
  libfreeimage-dev libfreetype-dev libcurl4-openssl-dev rapidjson-dev libvlc-dev \
  libasound2-dev libgles-dev libgl-dev libudev-dev libfribidi-dev libcec-dev gettext \
  fonts-droid-fallback
```

This repository uses a git submodule:

```bash
git clone --recursive https://github.com/JyuHo/emulationstation-re.git
```

or, in an existing clone, `git submodule update --init`.

Then configure and build:

```bash
# x86_64, desktop OpenGL 3.3 core
cmake -S . -B build -G Ninja -DGL=ON -DCMAKE_BUILD_TYPE=Release
# Raspberry Pi 4 and later, OpenGL ES 3.x
cmake -S . -B build -G Ninja -DGLES3=ON -DCMAKE_BUILD_TYPE=Release

cmake --build build
```

There is a single renderer, built either for desktop OpenGL 3.3 core (`GL`) or
for OpenGL ES 3.x (`GLES3`). When neither option is given, ARM builds pick
`GLES3` and the others pick `GL`.

Launching outside of Batocera
=============================

To launch Batocera EmulationStation, it is recommended to build it with [batocera.linux](https://github.com/batocera-linux/batocera.linux) instead. However, you can run a barebones version of ES (perhaps you want to tweak some menus without waiting for the entirety of Batocera to compile) if you provide the appropriate folder structure and files for it. **If you launch Batocera EmulationStation this way outside of Batocera, expect a lot of things to not actually be functional (eg. emulator launching, the background music player, the webserver, etc.), such things can only work with an environment set up identically to Batocera.** Before attempting this, it is recommended to have built batocera.linux at least once to have the appropriate configuration files for Batocera Emulationstation. They can also conveniently be taken from an existing Batocera install, from the same paths relative to root being `target`. For example: `/usr/share/emulationstation/`

Create the appropriate directories for ES's configuration files and resources, then copy them in from Batocera (instructions assuming you've already built batocera.linux):

```bash
sudo mkdir /etc/emulationstation
sudo chmod a+w /etc/emulationstation
cp -r ~/batocera.linux/output/x86_64/target/usr/share/emulationstation/ /etc/
```

(This would be where `/usr/share/emulationstation/` is used instead if copying from an existing Batocera install).

Then create the `/userdata` directory in your root, give it the correct permissions, and copy in the data from Batocera's `datainit` folder:

```bash
sudo mkdir /userdata
sudo chmod a+w /userdata
cp -r ~/batocera.linux/output/x86_64/target/usr/share/batocera/datainit/* /userdata
```

Then run `./emulationstation.sh` to start up EmulationStation. This will give you enough to navigate menus and change settings, however since we aren't truly inside of Batocera you'll find that none of the emulators launch and none of the secondary functions (such as the background music player) work.

If you want to see the full Batocera EmulationStation experience, it is recommended to build [batocera.linux](https://github.com/batocera-linux/batocera.linux) instead.

Configuring
===========

**~/.emulationstation/es_systems.cfg:**
When first run, an example systems configuration file will be created at `~/.emulationstation/es_systems.cfg`.  `~` is `$HOME`.  This example has some comments explaining how to write the configuration file. See the "Writing an es_systems.cfg" section for more information.

**Keep in mind you'll have to set up your emulator separately from EmulationStation if not launching from within Batocera!**

**~/.emulationstation/es_input.cfg:**
When you first start EmulationStation, you will be prompted to configure an input device. The process is thus:

1. Hold a button on the device you want to configure.  This includes the keyboard.

2. Press the buttons as they appear in the list.  Some inputs can be skipped by holding any button down for a few seconds.

3. You can review your mappings by pressing up and down, making any changes by pressing South (B on SNES).

4. Choose "SAVE" to save this device and close the input configuration screen.

The new configuration will be added to the `/etc/emulationstation/es_input.cfg` file.

**Both new and old devices can be (re)configured at any time by pressing the Start button and choosing "MAP CONTROLLER".** From here, you may unplug the device you used to open the menu and plug in a new one, if necessary. New devices will be appended to the existing input configuration file, so your old devices will remain configured.

**If your controller stops working, you can delete the `~/.emulationstation/es_input.cfg` file to make the input configuration screen re-appear on next run.**

You can use `--help` or `-h` to view a list of command-line options. Briefly outlined here:
```
--resolution [width] [height]   try and force a particular resolution
--gamelist-only                 skip automatic game search, only read from gamelist.xml
--ignore-gamelist               ignore the gamelist (useful for troubleshooting)
--draw-framerate                display the framerate
--no-exit                       don't show the exit option in the menu
--no-splash                     don't show the splash screen
--debug                         more logging
--scrape                        scrape using command line interface
--windowed                      not fullscreen, should be used with --resolution
--fullscreen-borderless			fullscreen, non exclusive.
--vsync [1/on or 0/off]         turn vsync on or off (default is on)
--max-vram [size]               Max VRAM to use in Mb before swapping. 0 for unlimited
--force-kid             		Force the UI mode to be Kid
--force-kiosk           		Force the UI mode to be Kiosk
--force-disable-filters         Force the UI to ignore applied filters in gamelist
--home							Force the .emulationstation folder
--help, -h                      summon a sentient, angry tuba
```

As long as ES hasn't frozen, you can always press F4 to close the application.

Writing an es_systems.cfg
=========================

Complete configuration instructions at [emulationstation.org](http://emulationstation.org/gettingstarted.html#config).

The `es_systems.cfg` file contains the system configuration data for EmulationStation, written in XML.  This tells EmulationStation what systems you have, what platform they correspond to (for scraping), and where the games are located.

ES will check the following location for its `es_systems.cfg` file:
* `/etc/emulationstation/es_systems.cfg`

The order EmulationStation displays systems reflects the order you define them in.

**NOTE:** A system *must* have at least one game present in its "path" directory, or ES will ignore it! If no valid systems are found, ES will report an error and quit!

See [SYSTEMS.md](SYSTEMS.md) for some live examples in EmulationStation.

The following "tags" are replaced by ES in launch commands:

`%ROM%`		- Replaced with absolute path to the selected ROM, with most Bash special characters escaped with a backslash.

`%BASENAME%`	- Replaced with the "base" name of the path to the selected ROM. For example, a path of "/foo/bar.rom", this tag would be "bar". This tag is useful for setting up AdvanceMAME.

`%ROM_RAW%`	- Replaced with the unescaped, absolute path to the selected ROM.  If your emulator is picky about paths, you might want to use this instead of %ROM%, but enclosed in quotes.

`%HOME%`	- Replaced with the home folder.

`%SYSTEM%`	- Replaced with the current selected system name.

`%EMULATOR%`	- Replaced with the current selected emulator.

`%CORE%`	- Replaced with the current selected core.

gamelist.xml
============

The `gamelist.xml` file for a system defines metadata for games, such as a name, image (like a screenshot or box art), description, release date, and rating.

If at least one game in a system has an image specified, ES will use the detailed view for that system (which displays metadata alongside the game list).

*You can use ES's [scraping](http://en.wikipedia.org/wiki/Web_scraping) tools to avoid creating a `gamelist.xml` by hand.*  There are two ways to run the scraper:

* **If you want to scrape multiple games:** press start to open the menu and choose the "SCRAPER" option.  Adjust your settings and press "SCRAPE NOW".
* **If you just want to scrape one game:** find the game on the game list in ES and press select.  Choose "EDIT THIS GAME'S METADATA" and then press the "SCRAPE" button at the bottom of the metadata editor.

You can also edit metadata within ES by using the metadata editor - just find the game you wish to edit on the gamelist, press Select, and choose "EDIT THIS GAME'S METADATA."

A command-line version of the scraper is also provided - just run emulationstation with `--scrape` *(currently broken)*.

The switch `--ignore-gamelist` can be used to ignore the gamelist and force ES to use the non-detailed view.

If you're writing a tool to generate or parse gamelist.xml files, you should check out [GAMELISTS.md](GAMELISTS.md) for more detailed documentation.


Themes
======

By default, EmulationStation will use ES-Carbon. Additional themes can be installed into `/etc/emulationstation/themes`.

If you want to know more about making your own themes (or editing existing ones), read [THEMES.md](THEMES.md) or check out the [wiki page](https://wiki.batocera.org/write_themes_for_emulationstation).

