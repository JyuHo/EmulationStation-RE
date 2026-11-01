#!/usr/bin/env bash
# Downloads the reference Recalbox theme of phase 11 (PROMPT.md 14.4) into <directory>, at a
# pinned commit, for the manual checks of the theme engine. The theme is under CC BY-NC-ND 4.0:
# it is NEVER committed to this repository, not even as a test fixture (rule 14.2-R3).
#
# Usage: tests/smoke/fetch-reference-theme.sh <directory> [theme]   (theme: recalbox-next)
# Result: <directory>/<theme>/theme.xml
set -euo pipefail

# recalbox/recalbox-themes, master on 2026-02-05. Change it only in a PR that says why.
commit="febce03c483cca4fdcd873938e974e0a52930418"
theme="${2:-recalbox-next}"
dir="$(realpath -m "$1")"
repo="$(realpath "$(dirname "$0")/../..")"

case "$dir/" in
"$repo"/*)
	echo "Refusing to download into the repository: pick a directory outside of it." >&2
	exit 1
	;;
esac
mkdir -p "$dir"

url="https://gitlab.com/api/v4/projects/recalbox%2Frecalbox-themes/repository/archive.tar.gz?sha=${commit}&path=themes/${theme}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
curl -fsSL -o "$tmp/theme.tar.gz" "$url"
tar -xzf "$tmp/theme.tar.gz" -C "$tmp"
rm -rf "${dir:?}/$theme"
mv "$tmp"/*/themes/"$theme" "$dir/$theme"
echo "$dir/$theme (recalbox-themes@${commit:0:7}, CC BY-NC-ND 4.0, do not commit)"
