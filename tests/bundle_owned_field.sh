#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cat > "$work/provider.zi" <<'ZI'
#import "vec"
Result :: struct { bytes: Vec(u8); valid: bool }
Make :: () -> Result {
    bytes: Vec(u8); VecPush(bytes, cast(u8)41)
    return .{bytes, true}
}
ZI
cat > "$work/consumer.zi" <<'ZI'
#import "provider"
#import "vec"
// This unreachable function instantiates a local vector type. Pruning it
// changes the qualification needed by the imported record's bytes field.
Unused :: () -> Vec(u8) { local: Vec(u8); VecPush(local, cast(u8)0); return local }
#program_export
Answer :: () -> s64 {
    value := Make()
    if !value.valid { return 0 }
    return value.bytes[0] + value.bytes.count
}
ZI
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/consumer.zi"
"$ziran" bundle --root "$work" --module-path "$repo/std" --entry consumer:Answer -o "$work/source.zib" "$work/consumer.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry consumer:Answer -o "$work/saved.zib" "$work/ir/consumer.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42
echo 'Imported owned field stays canonical after pruning its local type shadow'
