#!/bin/sh
# A real kernel through #asm: AV1's 8-bit 8x8 inverse DCT at 16-bit lanes
# (tests/spec/asm_dct8.zi). Each build transforms random and edge-case
# blocks and compares them with AV1's s64 reference; ZIRAN_ASM_CHECK also
# runs every variant beside the portable body. NDK clang assembles the
# AArch64 and ARMv7 NEON variants; qemu runs static Android binaries when
# available. GNU/Linux GCC and Clang also run ARM/Thumb and masked-NEON
# cases. ARM_REQUIRED=1 requires every ARM execution case.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

# #run generates the three templates from file-private procedures, one
# description of the transform, while the kernel compiles.
cp "$repo/tests/spec/asm_dct8.zi" "$work/dct8.zi"
"$ziran" build --target=c --root "$work" -o "$work/c" "$work/dct8.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/dct8.zi"
"$ziran" build --target=c --root "$work/ir" -o "$work/saved-c" "$work/ir/dct8.zir"
cmp "$work/c/dct8.c" "$work/saved-c/dct8.c"
cmp "$work/c/dct8.h" "$work/saved-c/dct8.h"

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

arm_ran=0
ndk=${ANDROID_NDK_HOME:-}
if test -n "$ndk"; then
    for triple in aarch64-linux-android24 armv7a-linux-androideabi24; do
        for mode in "" -DZIRAN_ASM_CHECK; do
            "$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/$triple-clang" -std=c11 -O3 -Wall -Werror $mode \
                -I"$work/c" -I"$repo/include" -c "$work/c/dct8.c" -o "$work/$triple.o"
        done
        case "$triple" in
            aarch64-*) emulator=${ZIRAN_QEMU_AARCH64:-qemu-aarch64}; cpu=cortex-a72; isa_flags= ;;
            *) emulator=${ZIRAN_QEMU_ARM:-qemu-arm}; cpu=cortex-a15; isa_flags=-mfpu=vfpv3-d16 ;;
        esac
        if command -v "$emulator" >/dev/null 2>&1; then
            # Bionic's static libc uses ELF TLS. API 29's crtbegin reserves
            # its TLS slots (64-byte alignment on AArch64, 32 on ARMv7).
            # Keep the assembly-only coverage above at API 24.
            runtime_triple=${triple%24}29
            for mode in "" -DZIRAN_ASM_CHECK -DZIRAN_NO_ASM; do
                "$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/$runtime_triple-clang" $isa_flags -static -std=c11 -O3 -Wall -Werror $mode \
                    -I"$work/c" -I"$repo/include" "$work/c/dct8.c" "$work/host.c" -o "$work/$runtime_triple"
                actual=$("$emulator" -cpu "$cpu" "$work/$runtime_triple") || {
                    echo "asm-dct8: $runtime_triple $mode execution failed" >&2; exit 1;
                }
                test "$actual" = "$expected"
            done
            arm_ran=$((arm_ran + 1))
            echo "asm-dct8: $runtime_triple native, checked and portable executions matched"
        else
            echo "asm-dct8: $emulator is unavailable; skipping $triple execution"
        fi
    done
else
    echo "asm-dct8: ANDROID_NDK_HOME is unset; skipping NDK assembly"
fi
# Android's static runtime itself requires NEON. Use GNU/Linux runtimes to
# check a baseline caller and CPU selection with NEON and the D32 bank masked.
qemu_arm=${ZIRAN_QEMU_ARM:-qemu-arm}
arm_compilers_ran=0
if command -v "$qemu_arm" >/dev/null 2>&1; then
    printf 'int main(void) { return 0; }\n' > "$work/arm-probe.c"
    for cross in "${ZIRAN_ARM_CC:-arm-linux-gnueabihf-gcc}" "${ZIRAN_ARM_CLANG:-clang --target=arm-linux-gnueabihf}"; do
        # Compiler commands may carry target and sysroot options.
        if ! $cross -static "$work/arm-probe.c" -o "$work/arm-probe" >/dev/null 2>&1 ||
           ! "$qemu_arm" "$work/arm-probe"; then
            echo "asm-dct8: skipping GNU/Linux ARM execution with $cross"
            continue
        fi
        for state in -marm -mthumb; do
            for mode in "" -DZIRAN_ASM_CHECK -DZIRAN_NO_ASM; do
                $cross $state -mfpu=vfpv3-d16 -static -std=c11 -O3 -Wall -Werror $mode \
                    -I"$work/c" -I"$repo/include" "$work/c/dct8.c" "$work/host.c" -o "$work/arm-run"
                test "$("$qemu_arm" -cpu cortex-a15 "$work/arm-run")" = "$expected"
                test "$("$qemu_arm" -cpu cortex-a15,neon=off,vfp-d32=off "$work/arm-run")" = "$expected"
            done
        done
        arm_compilers_ran=$((arm_compilers_ran + 1))
        echo "asm-dct8: $cross ARM/Thumb and masked NEON executions matched"
    done
fi
if test "${ARM_REQUIRED:-0}" = 1 && { test "$arm_ran" != 2 || test "$arm_compilers_ran" != 2; }; then
    echo 'asm-dct8: ARM_REQUIRED=1 needs the NDK, both emulators and both GNU/Linux ARM compilers' >&2
    exit 1
fi
echo "The #asm 8x8 inverse DCT matched AV1's reference and its portable body on $expected blocks"
