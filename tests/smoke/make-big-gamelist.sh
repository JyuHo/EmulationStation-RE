#!/usr/bin/env bash
# Builds a portable ES home with one large NES gamelist, for the performance check of the
# renderer (PROMPT.md 13.8): <count> empty ROMs, each with a box art and metadata, and the
# theme of the capture fixture (detailed view). Generated files, never committed.
#
# Usage: tests/smoke/make-big-gamelist.sh <directory> [count]   (count: 1000 by default)
# Then:  emulationstation --home <directory> --resolution 1920 1080 --vsync 0 --draw-framerate
set -euo pipefail

dir="$(mkdir -p "$1" && realpath "$1")"
count="${2:-1000}"
fixture="$(dirname "$(realpath "$0")")/../data/capture"

roms="$dir/roms/nes"
mkdir -p "$roms/images" "$dir/config/emulationstation" "$dir/data/emulationstation" \
	"$dir/state/emulationstation" "$dir/cache/emulationstation"
cp "$fixture/roms/nes/images/alpha.png" "$fixture/roms/nes/images/beta.png" "$roms/images/"
rm -rf "$dir/data/emulationstation/themes"
cp -r "$fixture/themes" "$dir/data/emulationstation/"

cat > "$dir/config/emulationstation/es_systems.cfg" <<XML
<systemList>
	<system>
		<name>nes</name>
		<fullname>Nintendo Entertainment System</fullname>
		<path>$roms</path>
		<extension>.nes</extension>
		<command>true</command>
		<platform>nes</platform>
		<theme>nes</theme>
	</system>
</systemList>
XML

{
	echo '<?xml version="1.0"?>'
	echo '<gameList>'
	for i in $(seq -w 1 "$count"); do
		: > "$roms/Game $i.nes"
		if [ $((10#$i % 2)) -eq 0 ]; then image=alpha; else image=beta; fi
		cat <<XML
	<game>
		<path>./Game $i.nes</path>
		<name>Game $i</name>
		<desc>Generated game $i of $count, for the renderer performance check.</desc>
		<image>./images/$image.png</image>
		<rating>0.$((10#$i % 10))</rating>
		<releasedate>19900101T000000</releasedate>
		<developer>Generated</developer>
		<genre>Benchmark</genre>
		<players>1</players>
	</game>
XML
	done
	echo '</gameList>'
} > "$roms/gamelist.xml"

echo "$count games in $roms"
echo "Run: emulationstation --home $dir --resolution 1920 1080 --vsync 0 --draw-framerate"
