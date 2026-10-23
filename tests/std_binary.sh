#!/bin/sh
# std/binary: little-endian fields and IEEE 754 bit decoding, checked in the
# portable VM from source and saved IR, and natively against the C
# compiler's own reading of the same bits over random and edge patterns.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
cd "$repo"

"$ziran" check --root tests/spec --module-path std tests/spec/std_binary.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" tests/spec/std_binary.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry std_binary:SelfTest -o "$work/source.zx" tests/spec/std_binary.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry std_binary:SelfTest -o "$work/saved.zx" "$work/ir/std_binary.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42

"$ziran" build --target=c --no-main --root tests/spec --module-path std -o "$work/c" tests/spec/std_binary.zi
cat > "$work/compare.c" <<'C'
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

double Float32FromBits(uint32_t); double Float64FromBits(uint64_t); int32_t SelfTest(void);

static uint64_t state = 88172645463325252ull;
static uint64_t next(void)
{
    state ^= state << 13; state ^= state >> 7; state ^= state << 17;
    return state;
}

static int same(double got, double want)
{
    if (isnan(want)) return isnan(got);
    return memcmp(&got, &want, sizeof got) == 0;
}

static int check32(uint32_t bits)
{
    float single;
    memcpy(&single, &bits, 4);
    if (same(Float32FromBits(bits), (double)single)) return 0;
    fprintf(stderr, "float32 bits %08x: got %.17g, want %.17g\n", bits, Float32FromBits(bits), (double)single);
    return 1;
}

static int check64(uint64_t bits)
{
    double value;
    memcpy(&value, &bits, 8);
    if (same(Float64FromBits(bits), value)) return 0;
    fprintf(stderr, "float64 bits %016llx: got %.17g, want %.17g\n", (unsigned long long)bits, Float64FromBits(bits), value);
    return 1;
}

int main(void)
{
    int failures = SelfTest() == 42 ? 0 : 1;
    const uint32_t edges32[] = {0, 1, 0x7fffff, 0x800000, 0x3f800000, 0x7f7fffff, 0x7f800000, 0x7fc00000,
        0x80000000, 0x80000001, 0xff7fffff, 0xff800000};
    const uint64_t edges64[] = {0, 1, 0xfffffffffffffull, 0x10000000000000ull, 0x3ff0000000000000ull,
        0x7fefffffffffffffull, 0x7ff0000000000000ull, 0x7ff8000000000000ull, 0x8000000000000000ull,
        0x8000000000000001ull, 0xffefffffffffffffull, 0xfff0000000000000ull};
    for (unsigned i = 0; i < sizeof edges32 / sizeof edges32[0] && failures < 10; i++) failures += check32(edges32[i]);
    for (unsigned i = 0; i < sizeof edges64 / sizeof edges64[0] && failures < 10; i++) failures += check64(edges64[i]);
    for (int i = 0; i < 1000000 && failures < 10; i++) {
        failures += check32((uint32_t)next());
        failures += check64(next());
    }
    return failures != 0;
}
C
cc -std=c11 -O2 -iquote "$work/c" "$work/c"/*.c "$work/compare.c" -lm -o "$work/compare"
"$work/compare"
echo 'std-binary-test-ok: fields and float bits match C over a million random patterns, portable bundle'
