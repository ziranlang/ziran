#!/bin/sh
# C ABI labels must not be replaced by globals or constants from another module.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/state.zi" <<'ZI'
open: bool = true;
abs :: 40;
Value :: () -> s32 { return ifx open then abs else 0 }
ZI
cat > "$work/files.zi" <<'ZI'
libc :: #system_library "libc";
OpenFile :: (path: *u8, flags: s32) -> s32 #foreign libc "open";
CloseFile :: (fd: s32) -> s32 #foreign libc "close";
Magnitude :: (value: s32) -> s32 #foreign libc "abs";
Check :: () -> s32 {
    path: [2]u8 = .[47, 0]
    fd := OpenFile(*path[0], 0)
    if fd < 0 { return 100 }
    if CloseFile(fd) != 0 { return 200 }
    return Magnitude(-2)
}
ZI
cat > "$work/app.zi" <<'ZI'
State :: #import "state";
Files :: #import "files";
#program_export
Main :: () -> s32 {
    return State.Value() + Files.Check() - 42
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    input="$work/app.zi"
    if test "$form" = saved; then input="$work/ir/app.zir"; fi
    for mode in full entry; do
        output="$work/$form-$mode"
        if test "$mode" = entry; then
            "$ziran" build --target=c --entry app:Main --no-main --root "$work" -o "$output" "$input"
        else
            "$ziran" build --target=c --no-main --root "$work" -o "$output" "$input"
        fi
        cat > "$output/host.c" <<'C'
#include "app.h"
int main(void) { return Main(); }
C
        "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c -o "$output/program"
        "$output/program"
    done
done
echo 'Foreign C ABI calls survive colliding globals and constants from source and saved IR'
