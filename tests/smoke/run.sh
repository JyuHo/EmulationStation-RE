#!/usr/bin/env bash
set -euo pipefail

binary="$(realpath "$1")"
seconds="${2:-6}"
screenshot="${3:-}"

home="$(mktemp -d)"
trap 'rm -rf "$home"' EXIT

mkdir -p "$home/config/emulationstation" "$home/data/emulationstation" "$home/state/emulationstation" "$home/cache/emulationstation" "$home/roms/nes"
touch "$home/roms/nes/Test Game.nes"
cat > "$home/config/emulationstation/es_systems.cfg" <<XML
<systemList>
	<system>
		<name>nes</name>
		<fullname>Nintendo Entertainment System</fullname>
		<path>$home/roms/nes</path>
		<extension>.nes</extension>
		<command>true</command>
		<platform>nes</platform>
	</system>
</systemList>
XML

status=0
ES_HOME="$home" xvfb-run -a -s '-screen 0 1280x720x24' bash -c '
	"$1" --windowed --resolution 1280 720 &
	pid=$!
	sleep "$2"
	if [ -n "$3" ] && command -v import >/dev/null; then import -window root "$3" || true; fi
	if ! kill -0 "$pid" 2>/dev/null; then wait "$pid"; echo "emulationstation exited early with code $?"; exit 1; fi
	kill "$pid"; wait "$pid" || true
' _ "$binary" "$seconds" "$screenshot" || status=$?

log="$home/state/emulationstation/es_log.txt"
echo "----- es_log.txt -----"
cat "$log" 2>/dev/null || echo "(no log written)"
echo "----------------------"

if [ "$status" -ne 0 ]; then
	echo "FAIL: emulationstation did not keep running"
	exit 1
fi
if [ ! -s "$log" ]; then
	echo "FAIL: no log written"
	exit 1
fi
if grep -q ' \[ERROR\] ' "$log"; then
	echo "FAIL: errors were logged"
	exit 1
fi
echo "PASS"
