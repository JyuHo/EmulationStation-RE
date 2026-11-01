#!/usr/bin/env bash
# Checks that a rectangle of a PNG holds at least one pixel of a color.
#
# Usage: tests/smoke/probe-area.sh <png> <x> <y> <width> <height> <RRGGBB> [tolerance]
#
# For thin or text features (a 1 px line, a glyph stroke) whose exact position moves by a pixel
# or two with the FreeType version: one pixel would be fragile, a small area is not. Fails when
# no pixel of the area is within the tolerance (0 to 255, default 0) on every channel.
set -euo pipefail

if [ "$#" -lt 6 ]; then
	echo "usage: $0 <png> <x> <y> <width> <height> <RRGGBB> [tolerance]" >&2
	exit 2
fi
png="$1" x="$2" y="$3" w="$4" h="$5" expected="${6#\#}" tolerance="${7:-0}"

if command -v magick >/dev/null; then im=(magick); else im=(convert); fi
er=$((16#${expected:0:2})) eg=$((16#${expected:2:2})) eb=$((16#${expected:4:2}))
# whole output read (an early exit of awk would kill the converter with SIGPIPE, fatal under
# pipefail); one line per pixel after the header, "x,y: (r,g,b)  #RRGGBB ...", 8 bits per channel
found="$("${im[@]}" "$png" -crop "${w}x${h}+${x}+${y}" +repage -alpha off -depth 8 txt:- |
	awk -v x0="$x" -v y0="$y" -v er="$er" -v eg="$eg" -v eb="$eb" -v t="$tolerance" '
		function d(a, b) { return a > b ? a - b : b - a }
		NR > 1 && !found {
			match($0, /\([0-9]+,[0-9]+,[0-9]+/)
			split(substr($0, RSTART + 1, RLENGTH - 1), c, ",")
			if (d(c[1], er) <= t && d(c[2], eg) <= t && d(c[3], eb) <= t) { split($1, p, "[,:]"); print p[1] + x0 "," p[2] + y0; found = 1 }
		}')"

if [ -n "$found" ]; then
	echo "OK $(basename "$png") area ${w}x${h}+${x}+${y}: #$expected at ($found) (tolerance $tolerance)"
else
	echo "FAIL $(basename "$png") area ${w}x${h}+${x}+${y}: no #$expected (tolerance $tolerance)"
	exit 1
fi
