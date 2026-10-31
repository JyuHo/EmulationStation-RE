#!/usr/bin/env bash
# Compares two sets of screenshots made by tests/smoke/capture.sh.
#
# Usage: tests/smoke/compare.sh <reference directory> <new directory> [fuzz] [max pixels]
#
# A pixel counts as different when a channel differs by more than <fuzz> (default 2%,
# for anti-aliasing). A screen fails when more than <max pixels> pixels differ (default 0).
# For each failing screen, <new directory>/diff-<name>.png highlights the differences.
set -euo pipefail

refdir="$1"
newdir="$2"
fuzz="${3:-2%}"
maxpixels="${4:-0}"

if ! command -v compare >/dev/null; then
	echo "FAIL: ImageMagick compare is required" >&2
	exit 1
fi

status=0
shopt -s nullglob
refs=("$refdir"/*.png)
if [ "${#refs[@]}" -eq 0 ]; then
	echo "FAIL: no reference screenshot in $refdir" >&2
	exit 1
fi

for ref in "${refs[@]}"; do
	name="$(basename "$ref")"
	new="$newdir/$name"
	if [ ! -f "$new" ]; then
		echo "MISSING  $name"
		status=1
		continue
	fi
	# compare prints the metric on stderr and exits 1 when the images differ
	pixels="$(compare -metric AE -fuzz "$fuzz" "$ref" "$new" "$newdir/diff-$name" 2>&1 >/dev/null || true)"
	pixels="${pixels%% *}"
	if [[ ! "$pixels" =~ ^[0-9.e+]+$ ]]; then
		echo "ERROR    $name: $pixels"
		status=1
	elif awk -v p="$pixels" -v m="$maxpixels" 'BEGIN { exit !(p > m) }'; then
		echo "DIFFERS  $name: $pixels pixels"
		status=1
	else
		echo "OK       $name: $pixels pixels"
		rm -f "$newdir/diff-$name"
	fi
done

if [ "$status" -ne 0 ]; then
	echo "FAIL"
	exit 1
fi
echo "PASS"
