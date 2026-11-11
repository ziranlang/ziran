#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/global-reachability-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Shared imports and unused state are normal in whole application graphs.
# Locals must not trigger a complete global lookup for every declaration.
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" shared-globals
timeout 20 "$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
timeout 20 "$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zx" "$work/app.zi"
timeout 20 "$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zx" "$work/ir/app.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42
echo 'Application globals link through shared imports with source/saved parity'
