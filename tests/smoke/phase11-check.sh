#!/usr/bin/env bash
# Mechanical checks of PROMPT.md 14.2 for a phase 11 PR. Run from the repository root:
#
#   tests/smoke/phase11-check.sh <PR>        e.g. tests/smoke/phase11-check.sh 11d
#
# <PR> is the PR being written: its stubs and hidden tests, and those of every earlier PR, must
# be gone. Prints one line per check and exits 1 if any check fails. Its whole output goes into
# the "Vérifications effectuées" section of the PR (R14).
set -uo pipefail

order=(11a 11b 11c 11c2 11d 11e 11e2 11f 11f1 11f2 11f3 11f4 11f5 11g 11h 11i)
pr="${1:-}"
found=0
done_prs=()
for p in "${order[@]}"; do
	done_prs+=("$p")
	if [ "$p" = "$pr" ]; then found=1; break; fi
done
if [ "$found" -ne 1 ]; then
	echo "usage: $0 <PR>   (one of: ${order[*]})" >&2
	exit 2
fi

# base of the comparison checks (R5, R7, R9); in CI, fetch it first
base="${PHASE11_BASE:-origin/main}"
if ! git rev-parse --verify --quiet "$base" >/dev/null; then
	echo "FAIL  base $base not found: git fetch origin main, or set PHASE11_BASE" >&2
	exit 2
fi

status=0
check() { # check <description> <command producing the offending lines>
	local name="$1"; shift
	local out
	out="$("$@" 2>/dev/null || true)"
	if [ -n "$out" ]; then
		echo "FAIL  $name"
		echo "$out" | head -n 20 | sed 's/^/      /'
		status=1
	else
		echo "OK    $name"
	fi
}

for p in "${done_prs[@]}"; do
	check "R8: no PHASE11-STUB($p)" git grep -n "PHASE11-STUB($p)" -- es-core es-app tests ':!tests/smoke/phase11-check.sh'
	check "R7: no hidden test tagged [$p]" git grep -n "\[\.\]\[phase11\]\[$p\]" -- tests
done
check "R8: no TODO/FIXME in the theme engine" git grep -nE 'TODO|FIXME|XXX' -- es-core/src/theme es-core/src/ThemeData.cpp es-core/src/ThemeData.h
check "R3: no Recalbox theme file in the repository" bash -c "git ls-files | grep -iE 'recalbox-next|recalbox-themes'"
check "R3: no CC BY-NC-ND material" git grep -lI -e 'Bounitos' -e 'BY-NC-ND' -- ':!PROMPT.md' ':!docs/' ':!tests/smoke/fetch-reference-theme.sh' ':!tests/smoke/phase11-check.sh'
check "R15: no agent transcript or local path in docs" git grep -nE '<\|[a-z_]+\|>|/home/[a-z]|/Users/[A-Za-z]|C:\\\\Users' -- '*.md' ':!tests/smoke/phase11-check.sh'
check "R15: no PR written as open in the plan or THEMES.md" git grep -nE '#[0-9]+[^)]*(ouverte|en cours|open\b)' -- docs/MODERNIZATION_PLAN.md docs/THEMES.md
check "0.3: no locale-dependent number parsing in the theme engine" git grep -nE '\b(std::)?(stof|stod|stoi|stol|atof|atoi|strtof|strtod)\(' -- es-core/src/theme es-core/src/ThemeData.cpp
check "R5: capture fixture unchanged" git diff --stat "$base" -- tests/data/capture
check "R7: test_ThemeData.cpp only gains lines" bash -c "git diff -U0 '$base' -- tests/unit/test_ThemeData.cpp | grep -E '^-[^-]'"
check "R9: no rendu-modifié bypass in the workflow" bash -c "git diff -U0 '$base' -- .github | grep -E '^[+-].*(rendu-modifi|continue-on-error|if: false)'"
# R10/R14: from 11e2 on, a PR that changes code also adds its own tests, tagged [<PR>]
# (#86 and #87 were merged with none)
case "$pr" in
11a | 11b | 11c | 11c2 | 11d | 11e | 11f) ;;
*)
	# grep reads the whole diff: with -q it could exit first, and git diff, killed by SIGPIPE,
	# would fail the pipeline (pipefail)
	if git diff "$base" -- tests | grep -E '^\+TEST_CASE\(.*\['"$pr"'\]' >/dev/null; then
		echo "OK    R10: the PR adds TEST_CASEs tagged [$pr]"
	else
		echo "FAIL  R10: the PR adds no TEST_CASE tagged [$pr]"
		status=1
	fi
	;;
esac

if [ "$status" -eq 0 ]; then echo "PASS phase11-check $pr"; else echo "FAIL phase11-check $pr"; fi
exit "$status"
