#!/usr/bin/env bash
# Probe a pixel color in a PNG. Usage: probe.sh <png> <x> <y> <RRGGBB> [tolerance]
set -euo pipefail
png="$1"; x="$2"; y="$3"; expected="$3" 2>/dev/null || true
expected="$4"; tol="${5:-0}"
# read pixel
pixel=$(convert "$png" -format '%[pixel:p{'"$x"','"$y"'}]' info: 2>/dev/null || magick "$png" -format '%[pixel:p{'"$x"','"$y"'}]' info:)
# pixel format: srgb(r,g,b) or #RRGGBB
echo "pixel=$pixel expected=$expected tol=$tol"
# simple check: extract hex
hex=$(echo "$pixel" | grep -oE '#[0-9A-Fa-f]{6}' | head -1)
if [ -z "$hex" ]; then
	# try rgb
	r=$(echo "$pixel" | grep -oE 'srgb\([0-9]+,[0-9]+,[0-9]+\)' | sed 's/srgb(//;s/)//')
	IFS=',' read -r rr gg bb <<< "$r"
	hex=$(printf '#%02X%02X%02X' "$rr" "$gg" "$bb")
fi
hex=${hex#\#}
exp=${expected#\#}
# compare channels
for i in 0 1 2; do
	a=$((16#${hex:$((i*2)):2}))
	b=$((16#${exp:$((i*2)):2}))
	d=$((a-b)); if [ $d -lt 0 ]; then d=$((-d)); fi
	if [ $d -gt $tol ]; then
		echo "FAIL probe $png ($x,$y): got #$hex expected #$exp (tol $tol)" >&2
		exit 1
	fi
done
echo "OK probe $png ($x,$y) #$hex ~ #$exp"