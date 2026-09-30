#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/test"
work=$(mktemp -d "$repo/build/test/process-plan9.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
"$ziran" ir --root "$repo/tests/spec" --module-path "$repo/std" \
    -o "$work/ir" "$repo/tests/spec/process_plan9_test.zi"
"$ziran" build --target=plan9-c --root "$repo/tests/spec" --module-path "$repo/std" \
    -o "$work/source" "$repo/tests/spec/process_plan9_test.zi"
"$ziran" build --target=plan9-c --root "$work/ir" \
    -o "$work/saved" "$work/ir/process_plan9_test.zir"
for header in process_plan9 process_plan9_test; do
    cmp "$work/source/$header.h" "$work/saved/$header.h"
done
if rg -n '__extension__| = \{\};' "$work/source"/*.c "$work/source"/*.h; then
    echo 'plan9-c retained hosted empty-array syntax' >&2
    exit 1
fi
echo 'process-plan9: source and saved IR generation passed (native execution uses the Taiji guest gate)'
