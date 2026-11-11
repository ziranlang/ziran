#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/borrow-import-graph-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
# A shared import graph must be searched as a graph, rather than once per
# path. New/free have no foreign declaration to find in these modules.
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" borrow-graph
timeout 20 "$ziran" check --root "$work" "$work/main.zi"
timeout 20 "$ziran" bundle --root "$work" --entry main:Answer -o "$work/app.zx" "$work/main.zi"
test "$("$ziran" run "$work/app.zx")" = 42
printf '%s\n' 'Borrow checking terminates over shared import graphs'
