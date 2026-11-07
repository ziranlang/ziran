#!/bin/sh
# A real kernel through #asm: AV1's 8-bit 8x8 inverse DCT at 16-bit lanes
# (tests/spec/asm_dct8.zi). Each build transforms random and edge-case
# blocks and compares them with AV1's s64 reference; ZIRAN_ASM_CHECK also
# runs every variant beside the portable body. NDK clang assembles the
# AArch64 and ARMv7 NEON variants.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

python3 "$repo/tests/asm_dct8_templates.py" "$repo/tests/spec/asm_dct8.zi"
cp "$repo/tests/spec/asm_dct8.zi" "$work/dct8.zi"
"$ziran" build --target=c --root "$work" -o "$work/c" "$work/dct8.zi"

cat > "$work/host.c" <<'C'
#include "dct8.h"
#include <stdio.h>
int main(void) { printf("%lld\n", (long long)Verify(7, 20000)); return 0; }
C
compiler=${CC:-cc}
run() {
    "$compiler" -std=c11 -O3 -Wall -Werror "$@" -I"$work/c" -I"$repo/include" "$work/c/dct8.c" "$work/host.c" -o "$work/run"
    "$work/run"
}
# The portable body, then every build that can take a variant, transforms
# the same blocks the same way.
expected=$(run -DZIRAN_NO_ASM)
test "$expected" -gt 10000
test "$(run)" = "$expected"
test "$(run -DZIRAN_ASM_CHECK)" = "$expected"
test "$(ZIRAN_SIMD_TIER=1 "$work/run")" = "$expected"

ndk=${ANDROID_NDK_HOME:-}
if test -n "$ndk"; then
    for triple in aarch64-linux-android24 armv7a-linux-androideabi24; do
        for mode in "" -DZIRAN_ASM_CHECK; do
            "$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/$triple-clang" -std=c11 -O3 -Wall -Werror $mode \
                -I"$work/c" -I"$repo/include" -c "$work/c/dct8.c" -o "$work/$triple.o"
        done
    done
else
    echo "asm-dct8: ANDROID_NDK_HOME is unset; skipping NDK assembly"
fi
echo "The #asm 8x8 inverse DCT matched AV1's reference and its portable body on $expected blocks"
