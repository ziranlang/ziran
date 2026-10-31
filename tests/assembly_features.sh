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

# Compile every advertised runtime feature, including the corresponding
# target attribute and detection primitive, on its destination architecture.
python3 - "$work/matrix.zi" <<'PY'
from pathlib import Path
import sys
features = ['popcnt', 'sse2', 'ssse3', 'sse4.1', 'sse4.2', 'avx', 'avx2',
            'avx512f', 'bmi', 'bmi2', 'lzcnt', 'fma', 'aes', 'pclmul']
targets = [arch+'+'+feature for arch in ['x86', 'x86_64'] for feature in features]
targets += ['aarch64+'+f for f in ['aes', 'crc32', 'pmull', 'sha2', 'atomics', 'simd']]
targets += ['aarch64+aes+crc32']
Path(sys.argv[1]).write_text('\n'.join(
    f'F{i} :: () #asm("{target}", "", "", "", "", "") {{}}' for i, target in enumerate(targets)))
PY
"$ziran" build --target=c --root "$work" -o "$work/matrix" "$work/matrix.zi"
"${CC:-cc}" -O2 -Werror -I"$repo/include" -I"$work/matrix" \
    -c "$work/matrix/matrix.c" -o "$work/matrix/native.o"
if test -n "${ANDROID_NDK_HOME:-}"; then
    for triple in aarch64-linux-android24 i686-linux-android24; do
        "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/$triple-clang" \
            -O2 -Werror -I"$repo/include" -I"$work/matrix" \
            -c "$work/matrix/matrix.c" -o "$work/matrix/$triple.o"
    done
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
