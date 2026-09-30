#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/test"
work=$(mktemp -d "$repo/build/test/file-plan9.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
"$ziran" ir --root "$repo/tests/spec" --module-path "$repo/std" \
    -o "$work/ir" "$repo/tests/spec/file_plan9_test.zi"
"$ziran" build --target=plan9-c --root "$repo/tests/spec" --module-path "$repo/std" \
    -o "$work/source" "$repo/tests/spec/file_plan9_test.zi"
"$ziran" build --target=plan9-c --root "$work/ir" \
    -o "$work/saved" "$work/ir/file_plan9_test.zir"
for header in file_plan9 c_string file_plan9_test; do
    cmp "$work/source/$header.h" "$work/saved/$header.h"
done
rg -Fq '#include "zir_plan9_runtime.h"' "$work/source/file_plan9.h"
for symbol in open pread pwrite seek remove dirstat nulldir dirwstat; do
    if rg -q "^[^ ]+\\** $symbol\\(.*\\);$" "$work/source/file_plan9.c" "$work/source/file_plan9_test.c"; then
        echo "native libc symbol redeclared: $symbol" >&2
        exit 1
    fi
done
echo 'file-plan9: source and saved IR generation passed (native execution uses the Taiji guest gate)'
