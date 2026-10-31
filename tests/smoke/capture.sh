#!/usr/bin/env bash
# Scripted screenshots for visual regression checks.
#
# Usage: tests/smoke/capture.sh <emulationstation binary> <output directory>
#
# Starts ES under Xvfb with the fixture in tests/data/capture (two systems, a theme
# with SVG logos, origins and sounds, a gamelist with box art), drives it with xdotool and
# writes one PNG per screen. Compare two runs with tests/smoke/compare.sh.
set -euo pipefail

binary="$(realpath "$1")"
outdir="$(mkdir -p "$2" && realpath "$2")"
fixture="$(dirname "$(realpath "$0")")/../data/capture"

for tool in xvfb-run xdotool import; do
	if ! command -v "$tool" >/dev/null; then
		echo "FAIL: $tool is required" >&2
		exit 1
	fi
done

home="$(mktemp -d)"
fakehome="$(mktemp -d)"
trap 'rm -rf "$home" "$fakehome"' EXIT

mkdir -p "$home/config/emulationstation" "$home/data/emulationstation" "$home/state/emulationstation" \
	"$home/cache/emulationstation"
cp -r "$fixture/roms" "$home/roms"
cp -r "$fixture/themes" "$fixture/gamelists" "$home/data/emulationstation/"

cat > "$home/config/emulationstation/es_systems.cfg" <<XML
<systemList>
	<system>
		<name>nes</name>
		<fullname>Nintendo Entertainment System</fullname>
		<path>$home/roms/nes</path>
		<extension>.nes</extension>
		<command>true</command>
		<platform>nes</platform>
		<theme>nes</theme>
	</system>
	<system>
		<name>snes</name>
		<fullname>Super Nintendo</fullname>
		<path>$home/roms/snes</path>
		<extension>.sfc</extension>
		<command>true</command>
		<platform>snes</platform>
		<theme>snes</theme>
	</system>
</systemList>
XML

# Keyboard mapping (same keys as InputManager::loadDefaultKBConfig): skips the
# "no gamepad" welcome screen so that the views behind it can be captured.
cat > "$home/config/emulationstation/es_input.cfg" <<'XML'
<?xml version="1.0"?>
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
XML

# no sound card under Xvfb: the theme sounds play on SDL's dummy driver
export SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy
# OpenGL debug output (KHR_debug extension) in release builds too: a GL error is an [ERROR] line
export ES_GL_DEBUG=1

status=0
HOME="$fakehome" ES_HOME="$home" xvfb-run -a -s '-screen 0 1280x720x24' bash -c '
	set -u
	out="$2"
	"$1" --windowed --resolution 1280 720 &
	pid=$!
	sleep 8
	win="$(xdotool search --sync --onlyvisible --pid "$pid" | head -n 1)"
	xdotool windowfocus --sync "$win" 2>/dev/null || true
	shot() { import -window root "$out/$1.png"; }
	key() { xdotool key --window "$win" "$1"; sleep "${2:-2}"; }

	shot 01-system-nes
	key Right; shot 02-system-snes
	key Left
	key Return; shot 03-gamelist-detailed
	key Down; shot 04-gamelist-second
	key F1; shot 05-menu
	key Down 0.5; key Down; key Return; shot 06-ui-settings
	key Escape 1; key Escape 1; key Escape
	# text input: gamelist options, metadata editor, then typing into the name field
	key Return; key F2; shot 07-gamelist-options
	# (the metadata editor itself shows the absolute, random, path of the fixture: no screenshot)
	key Down 0.5; key Down 0.5; key Return
	key Return 1; key Return 1
	xdotool type --window "$win" --delay 100 "Abc"; sleep 2; shot 08-text-input
	key Escape 1; key Escape 1; key Escape 1; key Escape 1; key Escape
	# game launch (the command is `true`): ES destroys the window and the OpenGL context, then
	# recreates them and reloads every texture. Back in the system view to capture (the
	# gamelist would show the play count and the "last played" time).
	key Escape 1; key Escape 1
	key Return; key Return 8
	win="$(xdotool search --sync --onlyvisible --pid "$pid" | head -n 1)"
	xdotool windowfocus --sync "$win" 2>/dev/null || true
	key Escape 3; shot 09-game-return

	if ! kill -0 "$pid" 2>/dev/null; then wait "$pid"; echo "emulationstation exited early with code $?"; exit 1; fi
	kill "$pid"
	code=0; wait "$pid" || code=$?
	if [ "$code" -ne 0 ]; then echo "emulationstation exited with code $code after SIGTERM"; exit 1; fi
' _ "$binary" "$outdir" || status=$?

log="$home/state/emulationstation/es_log.txt"
if [ "$status" -ne 0 ] || grep -q ' \[ERROR\] ' "$log" 2>/dev/null; then
	echo "----- es_log.txt -----"
	cat "$log" 2>/dev/null || echo "(no log written)"
	echo "FAIL: capture run did not complete cleanly"
	exit 1
fi
grep -E '(VERSION|RENDERER|VENDOR): |OpenGL' "$log" || true
ls "$outdir"/*.png
echo "PASS"
