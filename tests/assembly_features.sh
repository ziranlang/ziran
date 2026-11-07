#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

cat > "$work/count.zi" <<'ZI'
Pair :: struct { bits: u64; count: u64; }
#program_export
Count :: (value: u64) -> s64 #asm("x86_64+popcnt", "popcnt %[value], %[out]", "=r", "r", "cc", "") #asm("aarch64+simd", "fmov d0, %[value]\n\tcnt v0.8b, v0.8b\n\taddv b0, v0.8b\n\tumov %w[out], v0.b[0]", "=r", "r", "v0", "") {
    total: s64
    bits := value
    while bits != 0 { total += cast(s64)(bits & 1); bits >>= 1 }
    return total
}
#program_export
Count32 :: (value: u32) -> u32 #asm("x86+popcnt", "popcnt %[value], %[out]", "=r", "r", "cc", "") #asm("x86_64+popcnt", "popcnt %[value], %[out]", "=r", "r", "cc", "") {
    total: u32
    bits := value
    while bits != 0 { total += bits & 1; bits >>= 1 }
    return total
}
#program_export
Both :: (value: u64) -> Pair #asm("x86_64+popcnt", "movq %[value], %[out_bits]\n\tpopcnt %[value], %[out_count]", "=&r;=r", "r", "cc", "") {
    return Pair.{value, cast(u64)Count(value)}
}
#program_export
Store :: (value: *s64) #asm("x86_64+popcnt", "addq $1, (%[value])", "", "r", "cc;memory", "volatile") {
    value.* += 1
}
#program_export
VectorIdentity :: (value: u64) -> u64 #asm("x86_64+avx2", "vpxor %%ymm0, %%ymm0, %%ymm0\n\tmovq %[value], %[out]", "=r", "r", "ymm0", "") {
    return value
}
#program_export
Extract :: (value: u64, mask: u64) -> u64 #asm("x86_64+bmi2+popcnt", "pext %[mask], %[value], %[out]", "=r", "r;r", "", "") {
    result: u64
    bits := value
    remaining := mask
    next: u64 = 1
    while remaining != 0 {
        if (remaining & 1) != 0 {
            if (bits & 1) != 0 { result |= next }
            next <<= 1
        }
        remaining >>= 1
        bits >>= 1
    }
    return result
}
#program_export
Answer :: () -> s64 {
    if Extract(180, 170) != 12 { return 1 }
    pair := Both(240)
    if pair.bits != 240 || pair.count != 4 || Count32(4294967295) != 32 { return 2 }
    value: s64 = 0
    Store(*value)
    if value != 1 { return 3 }
    return Count(4398046511103)
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/count.zi"
cat > "$work/host.c" <<'C'
#include "count.h"
#include <assert.h>
#include <stdint.h>
static unsigned count(uint64_t bits) {
    unsigned result = 0;
    while(bits) { result += bits & 1; bits >>= 1; }
    return result;
}
static uint64_t extract(uint64_t value, uint64_t mask) {
    uint64_t result = 0, next = 1;
    for(unsigned i = 0; i < 64; i++)
        if(mask & (UINT64_C(1) << i)) {
            if(value & (UINT64_C(1) << i)) result |= next;
            next <<= 1;
        }
    return result;
}
int main(void) {
    uint64_t state = UINT64_C(0x123456789abcdef);
    for(int i = 0; i < 1000; i++) {
        state = state * UINT64_C(6364136223846793005) + 1;
        uint64_t mask = (state << 17) | (state >> 47);
        assert(Count(state) == count(state));
        assert(Count32((uint32_t)state) == count((uint32_t)state));
        assert(VectorIdentity(state) == state);
        assert(Extract(state, mask) == extract(state, mask));
        Pair pair = Both(state);
        assert(pair.bits == state && pair.count == count(state));
    }
    assert(Count(0) == 0 && Count(UINT64_MAX) == 64);
    assert(Extract(UINT64_MAX, UINT64_MAX) == UINT64_MAX);
    assert(Answer() == 42);
}
C
cat > "$work/no-features.h" <<'C'
#define __builtin_cpu_init() ((void)0)
#define __builtin_cpu_supports(feature) 0
C
for form in source saved; do
    root=$work; input=$work/count.zi
    if test "$form" = saved; then root=$work/ir; input=$root/count.zir; fi
    "$ziran" bundle --root "$root" --entry count:Answer -o "$work/$form.zx" "$input"
    test "$("$ziran" run "$work/$form.zx")" = 42
    output=$work/$form-c
    "$ziran" build --target=c --root "$root" -o "$output" "$input"
    compiler=${CC:-cc}; suffix=c; standard=c11
    for optimization in 0 2; do
        for mode in native missing disabled; do
            set --
            if test "$mode" = missing; then set -- -include "$work/no-features.h"; fi
            if test "$mode" = disabled; then set -- -DZIRAN_NO_ASM; fi
            isa_flags=
            if test "$(uname -m)" = x86_64; then isa_flags='-march=x86-64 -mno-popcnt -mno-avx -mno-bmi2'; fi
            "$compiler" -std="$standard" -O"$optimization" $isa_flags "$@" -I"$repo/include" -I"$output" \
                "$output/count.$suffix" "$work/host.$suffix" -o "$output/run"
            "$output/run"
        done
    done
    if test -n "${ANDROID_NDK_HOME:-}"; then
        tool=clang
        for triple in aarch64-linux-android24 armv7a-linux-androideabi24 x86_64-linux-android24 i686-linux-android24; do
            "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/$triple-$tool" \
                -std="$standard" -O2 -I"$repo/include" -I"$output" \
                "$output/count.$suffix" "$work/host.$suffix" -o "$output/$triple"
        done
    fi
    "$ziran" build --target=go --pkg main --root "$root" -o "$work/$form-go" "$input"
    printf 'package main\nfunc main() { if Count_Answer() != 42 { panic("CPU fallback") } }\n' > "$work/$form-go/main.go"
    (cd "$work/$form-go" && GO111MODULE=off go build -o run . && ./run)
done
cmp "$work/source.zx" "$work/saved.zx"
cmp "$work/source-c/count.c" "$work/saved-c/count.c"

cat > "$work/main.zi" <<'ZI'
#program_export
main :: () #asm("x86_64+popcnt", "", "", "", "", "volatile") {}
ZI
"$ziran" build --target=c --root "$work" -o "$work/main-c" "$work/main.zi"
compiler=${CC:-cc}; suffix=c; standard=c11
"$compiler" -std="$standard" -O2 -I"$repo/include" -I"$work/main-c" \
    "$work/main-c/main.$suffix" -o "$work/main-c/run"
"$work/main-c/run"

# Deliberate marker results test variant priority, feature conjunctions and
# baseline selection. Production assembly must agree with its portable body.
cat > "$work/selection.zi" <<'ZI'
#program_export
Pick :: (array_result_0_0: s64) -> s64 #asm("x86_64+popcnt+bmi2", "movq $3, %[out]", "=r", "r", "", "") #asm("x86_64+popcnt", "movq $2, %[out]", "=r", "r", "", "") #asm("x86_64", "movq $1, %[out]", "=r", "r", "", "") {
    unused array_result_0_0
    return 0
}
ziran_assembly_0_0_0 :: () -> s64 { return 0 }
ziran_assembly_0_0_1: s64;
ziran_assembly_0_0_2 :: 0
ZI
cat > "$work/mock.h" <<'C'
#include <string.h>
extern unsigned queries;
static int mock_feature(const char *name) {
    __atomic_fetch_add(&queries, 1, __ATOMIC_RELAXED);
    unsigned mask = FEATURES;
    return ((!strcmp(name, "popcnt") && (mask & 1)) ||
            (!strcmp(name, "bmi2") && (mask & 2)));
}
#define __builtin_cpu_init() ((void)0)
#define __builtin_cpu_supports(feature) mock_feature(feature)
C
cat > "$work/selection-host.c" <<'C'
#include "selection.h"
#include <assert.h>
#include <pthread.h>
unsigned queries;
static void *run(void *unused) {
    (void)unused;
    int expected = 0;
#if defined(__x86_64__) && !defined(ZIRAN_NO_ASM)
    expected = FEATURES == 3 ? 3 : (FEATURES & 1) ? 2 : 1;
#endif
    for(int i = 0; i < 1000; i++) assert(Pick(9) == expected);
    return 0;
}
int main(void) {
    pthread_t workers[4];
    for(int i = 0; i < 4; i++) assert(!pthread_create(&workers[i], 0, run, 0));
    for(int i = 0; i < 4; i++) assert(!pthread_join(workers[i], 0));
    unsigned previous = queries;
    run(0);
    assert(queries == previous); /* repeated calls do not query CPU features */
}
C
output=$work/selection-c
"$ziran" build --target=c --root "$work" -o "$output" "$work/selection.zi"
compiler=${CC:-cc}; suffix=c; standard=c11
for mask in 0 1 2 3; do
    "$compiler" -std="$standard" -O2 -DFEATURES="$mask" -include "$work/mock.h" \
        -I"$repo/include" -I"$output" "$output/selection.$suffix" "$work/selection-host.$suffix" \
        -pthread -o "$output/run"
    "$output/run"
done

# ZIRAN_SIMD_TIER hides x86 vector features beyond the forced tier, as it
# lowers #target clones: on a CPU with every feature, each tier picks its own
# variant, any forced tier passes over AVX-512, and other values force none.
if test "$(uname -m)" = x86_64; then
    cat > "$work/tier.zi" <<'ZI'
#program_export
Tier :: () -> s32 #asm("x86_64+avx512f", "movl $5, %[out]", "=r", "", "", "") #asm("x86_64+avx2+fma", "movl $4, %[out]", "=r", "", "", "") #asm("x86_64+sse4.1", "movl $3, %[out]", "=r", "", "", "") #asm("x86_64+ssse3+popcnt", "movl $2, %[out]", "=r", "", "", "") #asm("x86_64", "movl $1, %[out]", "=r", "", "", "") { return 0 }
ZI
    cat > "$work/every-feature.h" <<'C'
#define __builtin_cpu_init() ((void)0)
#define __builtin_cpu_supports(feature) 1
C
    printf '#include "tier.h"\n#include <stdio.h>\nint main(void) { printf("%%d\\n", Tier()); return 0; }\n' > "$work/tier-host.c"
    output=$work/tier-c
    "$ziran" build --target=c --root "$work" -o "$output" "$work/tier.zi"
    "${CC:-cc}" -std=c11 -O2 -include "$work/every-feature.h" -I"$repo/include" -I"$output" \
        "$output/tier.c" "$work/tier-host.c" -o "$output/run"
    test "$(unset ZIRAN_SIMD_TIER; "$output/run")" = 5
    for forced in 1:1 2:2 3:3 4:4 5:5 0:5 12:5 :5; do
        picked=$(ZIRAN_SIMD_TIER=${forced%:*} "$output/run")
        test "$picked" = "${forced#*:}" ||
            { echo "ZIRAN_SIMD_TIER=${forced%:*} picked $picked" >&2; exit 1; }
    done
fi

# Compile every advertised runtime feature, including the corresponding
# target attribute and detection primitive, on its destination architecture.
python3 - "$work/matrix.zi" <<'PY'
from pathlib import Path
import sys
features = ['popcnt', 'sse2', 'ssse3', 'sse4.1', 'sse4.2', 'avx', 'avx2',
            'avx512f', 'bmi', 'bmi2', 'lzcnt', 'fma', 'aes', 'pclmul']
targets = [arch+'+'+feature for arch in ['x86', 'x86_64'] for feature in features]
targets += ['aarch64+'+f for f in ['aes', 'crc32', 'pmull', 'sha2', 'atomics', 'simd']]
targets += ['aarch64+aes+crc32', 'arm+neon']
Path(sys.argv[1]).write_text('\n'.join(
    f'F{i} :: () #asm("{target}", "", "", "", "", "") {{}}' for i, target in enumerate(targets)))
PY
"$ziran" build --target=c --root "$work" -o "$work/matrix" "$work/matrix.zi"
"${CC:-cc}" -O2 -Werror -I"$repo/include" -I"$work/matrix" \
    -c "$work/matrix/matrix.c" -o "$work/matrix/native.o"
if test -n "${ANDROID_NDK_HOME:-}"; then
    for triple in aarch64-linux-android24 armv7a-linux-androideabi24 i686-linux-android24; do
        "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/$triple-clang" \
            -O2 -Werror -I"$repo/include" -I"$work/matrix" \
            -c "$work/matrix/matrix.c" -o "$work/matrix/$triple.o"
    done
fi

# ARMv7 NEON is selected from the kernel's AT_HWCAP. A CPU without NEON takes
# the arm baseline and ZIRAN_NO_ASM the portable body. The loads use vld1
# without an alignment qualifier, so the unaligned buffer must not fault.
cat > "$work/neon.zi" <<'ZI'
#program_export
NeonPath :: () -> s32 #asm("arm+neon", "mov %[out], #2", "=r", "", "", "") #asm("arm", "mov %[out], #1", "=r", "", "", "") { return 0 }
#program_export
SumChunks :: (data: *u8, chunks: u32) -> u32 #asm("arm+neon", "mov r12, %[data]\n\tmovs r3, %[chunks]\n\tvmov.i32 q1, #0\n\tbeq 2f\n1:\n\tvld1.8 {d0, d1}, [r12]!\n\tvpaddl.u8 q0, q0\n\tvpadal.u16 q1, q0\n\tsubs r3, r3, #1\n\tbne 1b\n2:\n\tvadd.i32 d2, d2, d3\n\tvpadd.i32 d2, d2, d2\n\tvmov.32 %[out], d2[0]", "=r", "r;r", "r3;r12;d0;d1;d2;d3;cc;memory", "") {
    total: u32
    i: u32 = 0
    while i < chunks * 16 { total += cast(u32)data[i]; i += 1 }
    return total
}
#program_export
BlockSum :: (data: *u8, chunks: u32) -> u32 {
    total: u32
    #asm("arm+neon", "mov r12, %[data]\n\tmovs r3, %[chunks]\n\tvmov.i32 q1, #0\n\tbeq 2f\n1:\n\tvld1.8 {d0, d1}, [r12]!\n\tvpaddl.u8 q0, q0\n\tvpadal.u16 q1, q0\n\tsubs r3, r3, #1\n\tbne 1b\n2:\n\tvadd.i32 d2, d2, d3\n\tvpadd.i32 d2, d2, d2\n\tvmov.32 %[total], d2[0]", "=r:total", "r:data;r:chunks", "r3;r12;d0;d1;d2;d3;cc;memory", "") {
        i: u32 = 0
        while i < chunks * 16 { total += cast(u32)data[i]; i += 1 }
    }
    return total
}
#program_export
Answer :: () -> s64 { return 40 + cast(s64)NeonPath() }
ZI
cat > "$work/neon-host.c" <<'C'
#include "neon.h"
#include <stdint.h>
#include <stdio.h>
int main(void) {
    static uint8_t buffer[16 * 9 + 1];
    for(unsigned i = 0; i < sizeof(buffer); i++) buffer[i] = (uint8_t)(i * 37 + 11);
    for(uint32_t chunks = 0; chunks <= 9; chunks++) {
        uint32_t expected = 0;
        for(uint32_t i = 0; i < chunks * 16; i++) expected += buffer[1 + i];
        if(SumChunks(buffer + 1, chunks) != expected || BlockSum(buffer + 1, chunks) != expected) {
            printf("sum mismatch at %u chunks\n", (unsigned)chunks);
            return 1;
        }
    }
    printf("%d\n", (int)NeonPath());
    return 0;
}
C
"$ziran" ir --root "$work" -o "$work/neon-ir" "$work/neon.zi"
"$ziran" build --target=c --root "$work" -o "$work/neon-c" "$work/neon.zi"
"$ziran" build --target=c --root "$work/neon-ir" -o "$work/neon-saved-c" "$work/neon-ir/neon.zir"
cmp "$work/neon-c/neon.c" "$work/neon-saved-c/neon.c"
"$ziran" bundle --root "$work" --entry neon:Answer -o "$work/neon.zx" "$work/neon.zi"
test "$("$ziran" run "$work/neon.zx")" = 40
"${CC:-cc}" -std=c11 -O2 -Werror -I"$repo/include" -I"$work/neon-c" \
    "$work/neon-c/neon.c" "$work/neon-host.c" -o "$work/neon-native"
case $(uname -m) in
    arm*) "$work/neon-native" >/dev/null ;;
    *) test "$("$work/neon-native")" = 0 ;;
esac
# Both compilers, ARM and Thumb-2 callers, run under qemu-arm. Its masked CPU
# drops NEON together with the 32 double registers, as QEMU requires.
qemu_arm=${ZIRAN_QEMU_ARM:-qemu-arm}
arm_ran=0
if command -v "$qemu_arm" >/dev/null 2>&1; then
    printf 'int main(void) { return 0; }\n' > "$work/arm-probe.c"
    for compiler in "${ZIRAN_ARM_CC:-arm-linux-gnueabihf-gcc}" "${ZIRAN_ARM_CLANG:-clang --target=arm-linux-gnueabihf}"; do
        # Unquoted on purpose: a compiler command may carry its target option.
        if ! $compiler -static "$work/arm-probe.c" -o "$work/arm-probe" >/dev/null 2>&1 ||
           ! "$qemu_arm" "$work/arm-probe"; then
            echo "assembly-features: skipping ARMv7 runs with $compiler"
            continue
        fi
        $compiler -O2 -Werror -I"$repo/include" -I"$work/matrix" \
            -c "$work/matrix/matrix.c" -o "$work/matrix/arm.o"
        for mode in -mthumb -marm; do
            $compiler $mode -std=c11 -O2 -static -Werror -I"$repo/include" -I"$work/neon-c" \
                "$work/neon-c/neon.c" "$work/neon-host.c" -o "$work/neon-arm"
            test "$("$qemu_arm" -cpu cortex-a15 "$work/neon-arm")" = 2
            test "$("$qemu_arm" -cpu cortex-a15,neon=off,vfp-d32=off "$work/neon-arm")" = 1
        done
        $compiler -std=c11 -O2 -static -Werror -DZIRAN_NO_ASM -I"$repo/include" -I"$work/neon-c" \
            "$work/neon-c/neon.c" "$work/neon-host.c" -o "$work/neon-arm"
        test "$("$qemu_arm" -cpu cortex-a15 "$work/neon-arm")" = 0
        arm_ran=$((arm_ran + 1))
    done
else
    echo "assembly-features: $qemu_arm is unavailable; skipping ARMv7 NEON runs"
fi
if test "${ARM_REQUIRED:-0}" = 1 && test "$arm_ran" != 2; then
    echo 'assembly-features: ARM_REQUIRED=1 needs qemu-arm and both ARMv7 compilers' >&2
    exit 1
fi
if test -n "${ANDROID_NDK_HOME:-}"; then
    "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi24-clang" \
        -std=c11 -O2 -Werror -I"$repo/include" -I"$work/neon-c" \
        "$work/neon-c/neon.c" "$work/neon-host.c" -o "$work/neon-android"
fi

reject() {
    expected=$1
    cat > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/error" 2>&1; then
        echo 'invalid feature selector was accepted' >&2; exit 1
    fi
    grep -Fq "$expected" "$work/error" || { cat "$work/error"; exit 1; }
}
reject 'unsupported #asm CPU feature' <<'ZI'
F :: () #asm("x86_64+unknown", "", "", "", "", "") {}
ZI
reject 'unsupported #asm CPU feature' <<'ZI'
F :: () #asm("aarch64+avx2", "", "", "", "", "") {}
ZI
reject 'unsupported #asm CPU feature' <<'ZI'
F :: () #asm("arm+simd", "", "", "", "", "") {}
ZI
reject 'unsupported #asm CPU feature' <<'ZI'
F :: () #asm("aarch64+neon", "", "", "", "", "") {}
ZI
reject 'unsupported #asm CPU feature' <<'ZI'
F :: () #asm("wasm32+neon", "", "", "", "", "") {}
ZI
reject 'unsupported #asm CPU feature' <<'ZI'
F :: () #asm("x86_64+simd128", "", "", "", "", "") {}
ZI
reject 'shadowed by an earlier' <<'ZI'
F :: () #asm("wasm32", "", "", "", "", "") #asm("wasm32+simd128", "", "", "", "", "") {}
ZI
reject 'duplicate #asm CPU feature' <<'ZI'
F :: () #asm("x86_64+popcnt+popcnt", "", "", "", "", "") {}
ZI
reject 'duplicate or too many' <<'ZI'
F :: () #asm("x86_64+popcnt+bmi2", "", "", "", "", "") #asm("x86_64+bmi2+popcnt", "", "", "", "", "") {}
ZI
reject 'shadowed by an earlier' <<'ZI'
F :: () #asm("x86_64", "", "", "", "", "") #asm("x86_64+popcnt", "", "", "", "", "") {}
ZI
reject 'shadowed by an earlier' <<'ZI'
F :: () #asm("x86_64+popcnt", "", "", "", "", "") #asm("x86_64+popcnt+bmi2", "", "", "", "", "") {}
ZI
