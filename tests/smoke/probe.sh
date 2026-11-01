#!/usr/bin/env bash
# Checks the color of one pixel of a PNG.
#
# Usage: tests/smoke/probe.sh <png> <x> <y> <RRGGBB> [tolerance]
#
# Fails when a channel differs from the expected value by more than the tolerance (0 to 255,
# default 0). Prints one line, OK or FAIL, with the color read.
set -euo pipefail

if [ "$#" -lt 4 ]; then
	echo "usage: $0 <png> <x> <y> <RRGGBB> [tolerance]" >&2
	exit 2
fi
png="$1" x="$2" y="$3" expected="${4#\#}" tolerance="${5:-0}"

if command -v magick >/dev/null; then im=(magick); else im=(convert); fi
# 8 bits per channel, whatever the depth of the file: "r,g,b"
rgb="$("${im[@]}" "$png" -depth 8 -format "%[fx:int(255*p{$x,$y}.r+0.5)],%[fx:int(255*p{$x,$y}.g+0.5)],%[fx:int(255*p{$x,$y}.b+0.5)]" info:)"
IFS=, read -r r g b <<<"$rgb"
got="$(printf '%02X%02X%02X' "$r" "$g" "$b")"

status=OK
for i in 0 1 2; do
	a=$((16#${got:$((i * 2)):2}))
	e=$((16#${expected:$((i * 2)):2}))
	d=$((a > e ? a - e : e - a))
	if [ "$d" -gt "$tolerance" ]; then status=FAIL; fi
done
echo "$status $(basename "$png") ($x,$y): #$got, expected #$expected (tolerance $tolerance)"
[ "$status" = OK ]
