#!/bin/sh
# A #parallel body reads bindings from its function: a local fixed array, a
# slice parameter, a record parameter and a scalar, including inside larger
# expressions. Every read feeds an index, so a binding the native worker
# failed to capture, or captured wrongly, fails a bounds check. C builds,
# natively run bundles and the portable VM must all run it.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

cat > "$work/captures.zi" <<'ZI'
Item :: struct {
    scale: s64
    offset: s64
}
Check :: (values: []s64, item: Item, limit: s64) -> s64 {
    table: [8]s64
    i := 0
    while i < 8 {
        table[i] = i * item.scale + item.offset
        i += 1
    }
    probe: [4]s64
    #parallel for 0..7 {
        slot := table[it] - values[it] * item.scale + limit
        seen := probe[slot]
        if seen != 0 { seen = 0 }
    }
    return 42
}
#program_export
Answer :: () -> s64 {
    values: [8]s64
    i := 0
    while i < 8 {
        values[i] = i
        i += 1
    }
    return Check(values[:], Item.{scale = 3, offset = 1}, 2)
}
ZI

"$ziran" bundle --root "$work" --entry captures:Answer -o "$work/captures.zx" "$work/captures.zi"
for threads in 1 4; do
    test "$(ZIRAN_NATIVE_CACHE=0 ZIRAN_PAR_THREADS=$threads "$ziran" run "$work/captures.zx")" = 42
done
"$ziran" build --target=c --entry captures:Answer --root "$work" -o "$work/c" "$work/captures.zi"
cat > "$work/c/main.c" <<'C'
#include "captures.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
"${CC:-cc}" -std=c11 -pedantic-errors -pthread -I"$repo/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/c/app"
for threads in 1 2 8; do
    ZIRAN_PAR_THREADS=$threads "$work/c/app"
done
echo "parallel bodies read local, parameter and record bindings"
