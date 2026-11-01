#!/usr/bin/env bash
# Screenshots of the theme features, checked by pixel probes (PROMPT.md 14.8).
#
# Usage: tests/smoke/capture-themes.sh <emulationstation binary> <output directory>
#
# Starts ES under Xvfb with the fixture in tests/data/capture-themes: one system per feature,
# each with an original format 4 theme made of plain rectangles (tests/data/capture-themes/
# themes/probes). Takes one PNG per system view, then reads chosen pixels with probe.sh.
# Prints one line per probe; fails if a probe, the theme check or the run fails.
set -euo pipefail

binary="$(realpath "$1")"
outdir="$(mkdir -p "$2" && realpath "$2")"
here="$(dirname "$(realpath "$0")")"
fixture="$here/../data/capture-themes"
systems=(zindex zswap rotation rorigin disabled)

for tool in xvfb-run xdotool import; do
	if ! command -v "$tool" >/dev/null; then
		echo "FAIL: $tool is required" >&2
		exit 1
	fi
done

# every property of the fixture must be known: no warning
for system in "${systems[@]}"; do
	report="$("$binary" --check-theme "$fixture/themes/probes" "$system" 2>&1 || true)"
	if ! grep -q '^0 error(s), 0 warning(s)$' <<<"$report"; then
		echo "$report"
		echo "FAIL: --check-theme $system"
		exit 1
	fi
done

home="$(mktemp -d)"
fakehome="$(mktemp -d)"
trap 'rm -rf "$home" "$fakehome"' EXIT

mkdir -p "$home/config/emulationstation" "$home/data/emulationstation" "$home/state/emulationstation" \
	"$home/cache/emulationstation"
cp -r "$fixture/roms" "$home/roms"
cp -r "$fixture/themes" "$home/data/emulationstation/"

{
	echo "<systemList>"
	for system in "${systems[@]}"; do
		cat <<XML
	<system>
		<name>$system</name>
		<fullname>$system</fullname>
		<path>$home/roms/$system</path>
		<extension>.rom</extension>
		<command>true</command>
		<theme>$system</theme>
	</system>
XML
	done
	echo "</systemList>"
} > "$home/config/emulationstation/es_systems.cfg"

# keyboard mapping, as in capture.sh: skips the "no gamepad" welcome screen
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

# as in capture.sh: dummy audio, OpenGL errors logged as [ERROR], Xvfb only (PROMPT.md 0.3)
export SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy
export ES_GL_DEBUG=1
unset WAYLAND_DISPLAY
export SDL_VIDEO_DRIVER=x11

status=0
HOME="$fakehome" ES_HOME="$home" xvfb-run -a -s '-screen 0 1280x720x24' bash -c '
	set -u
	out="$2"; shift 2
	"$0" --windowed --resolution 1280 720 --debug 2>"$ES_HOME/stderr.txt" &
	pid=$!
	sleep 8
	win="$(xdotool search --sync --onlyvisible --pid "$pid" | head -n 1)"
	xdotool windowfocus --sync "$win" 2>/dev/null || true
	n=1
	for system in "$@"; do
		[ "$n" -gt 1 ] && { xdotool key --window "$win" Right; sleep 2; }
		import -window root "$out/$(printf %02d "$n")-$system.png"
		n=$((n + 1))
	done
	if ! kill -0 "$pid" 2>/dev/null; then wait "$pid"; echo "emulationstation exited early with code $?"; exit 1; fi
	kill "$pid"
	code=0; wait "$pid" || code=$?
	if [ "$code" -ne 0 ]; then echo "emulationstation exited with code $code after SIGTERM"; exit 1; fi
' "$binary" "$outdir" "${systems[@]}" || status=$?

log="$home/state/emulationstation/es_log.txt"
if [ "$status" -ne 0 ] || grep -q ' \[ERROR\] ' "$log" 2>/dev/null; then
	echo "----- es_log.txt -----"
	cat "$log" 2>/dev/null || echo "(no log written)"
	echo "FAIL: capture run did not complete cleanly"
	exit 1
fi

# Probes: <screen> <x> <y> <RRGGBB>. The rectangles are plain images, probed away from their
# edges (linear filtering), hence a small tolerance.
failed=0
probe() { "$here/probe.sh" "$outdir/$1.png" "$2" "$3" "$4" 4 || failed=$((failed + 1)); }
for screen in 01-zindex 02-zswap 03-rotation 04-rorigin 05-disabled; do
	probe "$screen" 1088 72 00FFFF # control rectangle: the screen is drawn
done
# zIndex: red (20) over blue (15) where they overlap, though declared first
probe 01-zindex 320 126 FF0000
probe 01-zindex 192 54 FF0000
probe 01-zindex 448 198 0000FF
# zIndex exchanged: blue over red
probe 02-zswap 320 126 0000FF
# rotation 90 around the center (320,54): vertical bar from y -138 to 246
probe 03-rotation 320 200 00FF00
probe 03-rotation 160 54 404040
# rotation 90 around the top-left corner (128,36): bar from x 92 to 128, y 36 to 292
probe 04-rorigin 110 150 00FF00
probe 04-rorigin 256 54 404040
# disabled: the yellow rectangle is not drawn
probe 05-disabled 256 108 404040

ls "$outdir"/*.png
if [ "$failed" -ne 0 ]; then
	echo "FAIL: $failed probe(s)"
	exit 1
fi
echo "PASS"
