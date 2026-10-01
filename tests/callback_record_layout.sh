#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(dirname "${1:-$repo/build/bin/ziran}")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/packet.zi" <<'ZI'
Packet :: struct { guard: u64; value: s32; trailer: u64; }
ZI
cat > "$work/app.zi" <<'ZI'
#import "packet"
Visit :: #type (packet: *Packet) -> s32 #c_call;
Envelope :: struct { callback: Visit; }
native :: #system_library "native";
Submit :: (envelope: Envelope) -> s32 #foreign native;
Read :: (packet: *Packet) -> s32 { return packet.value }
#program_export
Answer :: () -> s32 {
    envelope: Envelope
    envelope.callback = Read
    return Submit(envelope)
}
ZI
"$bin/zi2c" --no-main --entry app:Answer --root "$work" -o "$work/c" "$work/app.zi"
cat > "$work/c/host.c" <<'C'
#include "app.h"
_Static_assert(sizeof(Packet) == 24, "Native callbacks must retain the complete record ABI");
int32_t Submit(Envelope envelope) {
    Packet packet = {99, 42, 1};
    return envelope.callback(&packet);
}
int main(void) { return Answer() == 42 ? 0 : 1; }
C
"${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/c" "$work"/c/*.c -o "$work/app"
"$work/app"
"$bin/zi2cpp" --no-main --entry app:Answer --root "$work" -o "$work/cpp" "$work/app.zi"
sed -e 's/app.h/app.hpp/' -e 's/_Static_assert/static_assert/' \
    -e 's/int32_t Submit/extern "C" int32_t Submit/' "$work/c/host.c" > "$work/cpp/host.cpp"
"${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$work/cpp" "$work"/cpp/*.cpp -o "$work/app-cpp"
"$work/app-cpp"
echo "Native callback records preserve unused fields and their C ABI"
