#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
mkdir -p "$repo/build/test"
work=$(mktemp -d "$repo/build/test/imported-callback-cast.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
cat > "$work/callbacks.zi" <<'ZI'
Callback :: #type () -> *u8 #c_call;
ZI
cat > "$work/other.zi" <<'ZI'
Callback :: #type (value: s32) -> s32 #c_call;
ZI
cat > "$work/app.zi" <<'ZI'
#import "callbacks"
Other :: #import "other";
Local :: #type (value: s32) -> s32 #c_call;
native :: #system_library "native";
GetRaw :: (which: s32) -> *void #foreign native;
#program_export
main :: () -> s32 {
    pointer: *void
    empty := cast(Callback)pointer
    if empty != null { return 1 }
    callback := cast(Callback)GetRaw(0)
    if callback == null || callback()[0] != 42 { return 2 }
    other := cast(Other.Callback)GetRaw(1)
    local := cast(Local)GetRaw(1)
    if other(41) != 42 || local(40) != 41 { return 3 }
    return 0
}
ZI
cat > "$work/host.c" <<'C'
#include <stdint.h>
static uint8_t *bytes(void) { static uint8_t result[] = {42, 0}; return result; }
static int32_t increment(int32_t value) { return value + 1; }
void *GetRaw(int32_t which) { return which ? (void *)increment : (void *)bytes; }
C
"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    root=$work
    input=$work/app.zi
    extension=zi
    if test "$form" = saved; then root=$work/ir; input=$root/app.zir; extension=zir; fi
    out="$work/$form"
    "$ziran" build --target=c --root "$root" -o "$out" "$input"
    "${CC:-cc}" -std=c11 -Wall -Werror -I"$out" "$out"/*.c "$work/host.c" -o "$out/run"
    "$out/run"
    for target in go zx; do
        if "$ziran" check "--target=$target" --root "$root" "$root/callbacks.$extension" > "$work/$form-$target.out" 2>&1; then
            echo "C callback unexpectedly accepted for $target ($form)" >&2
            exit 1
        fi
        grep -Fq 'target does not support #c_call procedure types' "$work/$form-$target.out"
    done
done
echo 'Imported, qualified and local C callback casts: source/saved native calls and Go/portable rejection passed'
