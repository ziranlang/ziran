#!/bin/sh
# Raw native pointers expose complete record layouts, including nested arrays.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
cat > "$work/app.zi" <<'ZI'
Identity :: struct { prefix: u64; value: s32; trailer: u64; }
Identities :: struct { values: [2]Identity; }
Packet :: struct { guard: u64; identity: Identity; padding: [2]u64; length: s64; tail: u64; }
Output :: struct { guard: u64; value: s32; tail: u64; }
native :: #system_library "native";
ReadPacket :: () -> *void #foreign native;
ReadIdentities :: () -> *void #foreign native;
CheckOutput :: (memory: *void) -> s32 #foreign native;
#program_export
main :: () -> s32 {
    packet := cast(*Packet)ReadPacket()
    if packet.identity.value != 23 || packet.length != 12345 { return 1 }
    output: Output
    output.value = 42
    if CheckOutput(cast(*void)(*output)) != 1 { return 2 }
    identities := cast(*Identities)ReadIdentities()
    if identities.values[1].value != 41 { return 3 }
    return 0
}
ZI
cat > "$work/host.c" <<'C'
#include <stdint.h>
typedef struct { uint64_t prefix; int32_t value; uint64_t trailer; } NativeIdentity;
typedef struct {
    uint64_t guard;
    NativeIdentity identity;
    uint64_t padding[2];
    int64_t length;
    uint64_t tail;
} NativePacket;
typedef struct { uint64_t guard; int32_t value; uint64_t tail; } NativeOutput;
#ifdef __cplusplus
extern "C" {
#endif
void *ReadPacket(void) {
    static NativePacket packet = {99, {7, 23, 9}, {100, 101}, 12345, 1};
    return &packet;
}
void *ReadIdentities(void) {
    static NativeIdentity identities[2] = {{17, 31, 19}, {27, 41, 29}};
    return identities;
}
int32_t CheckOutput(void *memory) {
    NativeOutput *output = (NativeOutput *)memory;
    return output->guard == 0 && output->value == 42 && output->tail == 0;
}
#ifdef __cplusplus
}
#endif
C
"$ziran" ir --entry app:main --root "$work" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    input=$work/app.zi
    root=$work
    if test "$form" = saved; then input=$work/ir/app.zir; root=$work/ir; fi
    for target in c cpp; do
        output=$work/$form-$target
        "$ziran" build --target="$target" --entry app:main --root "$root" -o "$output" "$input"
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -I"$output" "$output"/*.c "$work/host.c" -o "$output/run"
        else
            "${CXX:-c++}" -std=c++17 -I"$output" "$output"/*.cpp -x c++ "$work/host.c" -o "$output/run"
        fi
        "$output/run"
    done
done
echo 'Raw native record pointers preserve complete layouts in C/C++ source and saved IR'
