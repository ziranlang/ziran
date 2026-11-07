#!/bin/sh
# std/simd: lane semantics with known answers, and one fingerprint of every
# operation shape that each build agrees on (GCC and Clang native vectors,
# SSE4.1 and AVX2 instructions, the lane-array fallback that TinyCC and older
# compilers use, sanitizers, Go's lane loops, bundles on the native engine and
# the portable VM's reference lanes, wasm32 with and without SIMD128, and
# AArch64 and ARMv7 from the Android NDK), from source and from saved IR.
# Checking rejects mismatched vectors, out-of-range constants and operators on
# vectors.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Simd :: #import "simd"

Three :: 3

Answer :: () -> s64 {
    a := Simd.Make(s16x8, 1, -2, 3, -4, 30000, -30000, 7, -32768)
    b := Simd.Make(s16x8, 9, 8, -7, 6, 5000, -5000, 3, -1)
    sat := Simd.AddSat(a, b)
    if Simd.Lane(sat, 4) != 32767 || Simd.Lane(sat, 5) != -32768 || Simd.Lane(sat, 0) != 10 { return 1 }
    wrap := Simd.Add(a, b)
    if Simd.Lane(wrap, 4) != -30536 || Simd.Lane(wrap, 7) != 32767 { return 2 }
    if Simd.Lane(Simd.MulHighRound(a, b), 4) != 4578 { return 3 }
    narrow := Simd.Narrow(u8x16, Simd.Make(s16x8, -300, 300, 127, 0, 0, 0, 0, 0), b)
    if Simd.Lane(narrow, 0) != 0 || Simd.Lane(narrow, 1) != 255 || Simd.Lane(narrow, 2) != 127 ||
        Simd.Lane(narrow, 8) != 9 || Simd.Lane(narrow, 10) != 0 { return 4 }
    w := Simd.Make(s32x4, -100, 100, -2147483648, 2147483647)
    if Simd.Lane(Simd.ShiftRight(w, 5), 0) != -4 || Simd.Lane(Simd.ShiftRight(w, 33), 0) != -1 ||
        Simd.Lane(Simd.ShiftRight(w, 33), 1) != 0 || Simd.Lane(Simd.ShiftLeft(w, 40), 3) != 0 { return 5 }
    if Simd.Lane(Simd.ShiftRight(Simd.Reinterpret(u32x4, w), 28), 0) != 15 { return 6 }
    if Simd.Lane(Simd.ShiftRightRound(w, Three), 0) != -12 || Simd.Lane(Simd.ShiftRightRound(w, Three), 1) != 13 { return 7 }
    table := Simd.Make(u8x16, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115)
    picks := Simd.Make(u8x16, 15, 0, 16, 200, 3, 3, 3, 3, 1, 2, 3, 4, 5, 6, 7, 8)
    shuffled := Simd.ByteShuffle(table, picks)
    if Simd.Lane(shuffled, 0) != 115 || Simd.Lane(shuffled, 2) != 0 || Simd.Lane(shuffled, 3) != 0 { return 8 }
    aligned := Simd.AlignBytes(picks, table, 5)
    if Simd.Lane(aligned, 0) != 105 || Simd.Lane(aligned, 10) != 115 || Simd.Lane(aligned, 11) != 15 { return 9 }
    if Simd.Lane(Simd.ShiftBytesRight(table, Three), 0) != 103 || Simd.Lane(Simd.ShiftBytesRight(table, Three), 13) != 0 ||
        Simd.Lane(Simd.ShiftBytesLeft(table, 3), 2) != 0 || Simd.Lane(Simd.ShiftBytesLeft(table, 3), 3) != 100 { return 10 }
    blocks := Simd.Shuffle(Simd.Make(s64x4, 1, 2, 3, 4), 1, 0)
    if Simd.Lane(blocks, 0) != 2 || Simd.Lane(blocks, 1) != 1 || Simd.Lane(blocks, 2) != 4 || Simd.Lane(blocks, 3) != 3 { return 11 }
    mixed := Simd.InterleaveLow(table, picks)
    if Simd.Lane(mixed, 0) != 100 || Simd.Lane(mixed, 1) != 15 || Simd.Lane(mixed, 2) != 101 { return 12 }
    if Simd.Lane(Simd.WidenHigh(a), 0) != 30000 || Simd.Lane(Simd.WidenHigh(a), 3) != -32768 { return 13 }
    pairs := Simd.MulAddPairs(a, b)
    if Simd.Lane(pairs, 0) != -7 || Simd.Lane(pairs, 2) != 300000000 { return 14 }
    pixels := Simd.Splat(u8x16, 255)
    weights := Simd.Splat(s8x16, 127)
    if Simd.Lane(Simd.MulAddPairsSat(pixels, weights), 0) != 32767 { return 15 }
    if Simd.Lane(Simd.SumAbsDiff(table, Simd.Splat(u8x16, 100)), 0) != 28 { return 16 }
    if Simd.ReduceMaxIndex(Simd.Make(s16x8, 1, 9, 3, 9, 0, 0, 0, 0)) != 1 || Simd.ReduceAdd(b) != 18 ||
        Simd.ReduceMin(a) != -32768 || Simd.ReduceMax(a) != 30000 { return 17 }
    f := Simd.Make(f32x4, 1.75, -1.75, 3000000000.0, -3000000000.0)
    signed := Simd.Convert(s32x4, f)
    unsigned := Simd.Convert(u32x4, f)
    if Simd.Lane(signed, 0) != 1 || Simd.Lane(signed, 1) != -1 || Simd.Lane(signed, 2) != 2147483647 ||
        Simd.Lane(signed, 3) != -2147483648 { return 18 }
    if Simd.Lane(unsigned, 1) != 0 || Simd.Lane(unsigned, 2) != 3000000000 || Simd.Lane(unsigned, 3) != 0 { return 19 }
    half := Simd.Div(Simd.Make(f32x4, 1.5, -2.25, 9.0, 0.0), Simd.Splat(f32x4, 3.0))
    if Simd.Lane(half, 0) != 0.5 || Simd.Lane(half, 1) != -0.75 || Simd.Lane(half, 2) != 3.0 { return 20 }
    zeros := Simd.Equal(Simd.Make(f64x2, -0.0, 1.0), Simd.Zero(f64x2))
    if Simd.Lane(zeros, 0) != 18446744073709551615 || Simd.Lane(zeros, 1) != 0 { return 21 }
    negative := Simd.Less(a, Simd.Zero(s16x8))
    magnitude := Simd.Select(negative, Simd.Neg(a), a)
    if Simd.Lane(magnitude, 1) != 2 || Simd.Lane(magnitude, 4) != 30000 || Simd.Lane(magnitude, 7) != -32768 { return 22 }
    if !Simd.AllZero(Simd.Zero(u32x8)) || Simd.AllZero(Simd.WithLane(Simd.Zero(u32x8), 7, 1)) { return 23 }
    data: [12]s32
    for i: 0..11 { data[i] = cast(s32) (i * 10) }
    row := Simd.Load(s32x4, data, 5)
    if Simd.Lane(row, 0) != 50 || Simd.Lane(row, 3) != 80 { return 24 }
    low := Simd.LoadLow(s32x4, data[:], 10)
    if Simd.Lane(low, 0) != 100 || Simd.Lane(low, 1) != 110 || Simd.Lane(low, 2) != 0 { return 25 }
    Simd.Store(data[:], 8, Simd.Splat(s32x4, -1))
    if data[7] != 70 || data[8] != -1 || data[11] != -1 { return 26 }
    Simd.StoreLow(data[:], 0, Simd.Splat(s32x4, 5))
    if data[0] != 5 || data[1] != 5 || data[2] != 20 { return 27 }
    got := Simd.Gather(data, Simd.Make(s32x4, 3, 2, 1, 0))
    if Simd.Lane(got, 0) != 30 || Simd.Lane(got, 3) != 5 { return 28 }
    halves := Simd.Combine(Simd.Low(Simd.Widen(b)), Simd.High(Simd.Widen(b)))
    if Simd.Lane(halves, 7) != -1 || Simd.Lane(halves, 4) != 5000 { return 29 }
    signs := Simd.Sign(a, b)
    if Simd.Lane(signs, 2) != -3 || Simd.Lane(signs, 5) != 30000 || Simd.Lane(signs, 7) != -32768 ||
        Simd.Lane(Simd.Sign(a, Simd.Zero(s16x8)), 0) != 0 { return 30 }
    gaps := Simd.AbsDiff(a, b)
    if Simd.Lane(gaps, 1) != 10 || Simd.Lane(gaps, 4) != 25000 || Simd.Lane(gaps, 7) != 32767 ||
        Simd.Lane(Simd.AbsDiff(Simd.Splat(s16x8, 32767), Simd.Splat(s16x8, -32768)), 0) != 65535 ||
        Simd.Lane(Simd.AbsDiff(table, Simd.Splat(u8x16, 110)), 0) != 10 { return 31 }
    return 42
}

Shift :: 3

Mix :: (hash: u64, v: u8x16) -> u64 {
    bytes: [16]u8
    Simd.Store(bytes[:], 0, v)
    h := hash
    for i: 0..15 { h = (h ^ cast(u64) bytes[i]) * 1099511628211 }
    return h
}

Mix32 :: (hash: u64, v: u8x32) -> u64 {
    return Mix(Mix(hash, Simd.Low(v)), Simd.High(v))
}

MixValue :: (hash: u64, v: s64) -> u64 {
    return (hash ^ cast(u64) v) * 1099511628211
}

Arithmetic :: () -> u64 {
    a := Simd.Make(s16x8, 1, -2, 3, -4, 30000, -30000, 7, -32768)
    b := Simd.Make(s16x8, 9, 8, -7, 6, 5000, -5000, 3, -1)
    h: u64 = 14695981039346656037
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Add(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Sub(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Mul(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.AddSat(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.SubSat(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Min(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Max(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.MulHigh(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.MulHighRound(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Abs(a)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Neg(a)))
    u := Simd.Reinterpret(u8x16, a)
    v := Simd.Reinterpret(u8x16, b)
    h = Mix(h, Simd.Average(u, v))
    h = Mix(h, Simd.And(u, v))
    h = Mix(h, Simd.Or(u, v))
    h = Mix(h, Simd.Xor(u, v))
    h = Mix(h, Simd.AndNot(u, v))
    h = Mix(h, Simd.Not(u))
    f := Simd.Make(f32x4, 1.5, -2.25, 1000000.0, -0.0)
    g := Simd.Splat(f32x4, 3.0)
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Div(f, g)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Mul(f, g)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Abs(f)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Neg(f)))
    w := Simd.Make(s32x8, 1, 2, 3, 4, -5, -6, 2147483647, -2147483648)
    h = Mix32(h, Simd.Reinterpret(u8x32, Simd.Add(w, w)))
    h = Mix32(h, Simd.Reinterpret(u8x32, Simd.Max(w, Simd.Zero(s32x8))))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Sign(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.AbsDiff(a, b)))
    h = Mix32(h, Simd.Reinterpret(u8x32, Simd.Sign(w, Simd.Neg(w))))
    h = Mix32(h, Simd.Reinterpret(u8x32, Simd.AbsDiff(w, Simd.Neg(w))))
    return h
}

Compares :: () -> u64 {
    a := Simd.Make(s16x8, 1, -2, 3, -4, 5, -6, 7, -8)
    b := Simd.Splat(s16x8, 0)
    h: u64 = 1
    lt := Simd.Less(a, b)
    h = Mix(h, Simd.Reinterpret(u8x16, lt))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Greater(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Equal(a, a)))
    // A signed mask reinterprets to the helpers' unsigned mask.
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Select(lt, Simd.Neg(a), a)))
    f := Simd.Make(f64x2, 1.0, -1.0)
    m := Simd.Less(f, Simd.Zero(f64x2))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Select(m, Simd.Splat(f64x2, 9.0), f)))
    return h
}

Shifts :: (count: s64) -> u64 {
    a := Simd.Make(s32x4, -100, 100, -2147483648, 2147483647)
    u := Simd.Reinterpret(u32x4, a)
    h: u64 = 2
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.ShiftLeft(a, count)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.ShiftRight(a, count)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.ShiftRight(u, count)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.ShiftRightRound(a, Shift)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.ShiftLeft(a, 40)))
    counts := Simd.Make(u32x4, 0, 1, 31, 32)
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.ShiftLeftLanes(u, counts)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.ShiftRightLanes(u, counts)))
    bytes := Simd.Reinterpret(u8x16, a)
    h = Mix(h, Simd.ShiftBytesLeft(bytes, 3))
    h = Mix(h, Simd.ShiftBytesRight(bytes, Shift + 1))
    h = Mix(h, Simd.AlignBytes(bytes, Simd.Not(bytes), 5))
    return h
}

Lanes :: () -> u64 {
    a := Simd.Make(u16x8, 10, 11, 12, 13, 14, 15, 16, 17)
    h: u64 = 3
    h = MixValue(h, cast(s64) Simd.Lane(a, 5))
    b := Simd.WithLane(a, Shift, 999)
    h = Mix(h, Simd.Reinterpret(u8x16, b))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Shuffle(a, 7, 6, 5, 4, 0, 0, 1, Shift)))
    wide := Simd.Make(s64x4, 1, 2, 3, 4)
    h = Mix32(h, Simd.Reinterpret(u8x32, Simd.Shuffle(wide, 1, 0)))
    table := Simd.Make(u8x16, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115)
    index := Simd.Make(u8x16, 15, 0, 16, 200, 3, 3, 3, 3, 1, 2, 3, 4, 5, 6, 7, 8)
    h = Mix(h, Simd.ByteShuffle(table, index))
    h = Mix(h, Simd.InterleaveLow(table, index))
    h = Mix(h, Simd.InterleaveHigh(table, index))
    return h
}

Memory :: (data: []s16, raw: *s16) -> u64 {
    h: u64 = 4
    a := Simd.Load(s16x8, data, 0)
    b := Simd.Load(s16x8, data, 5)
    c := Simd.LoadLow(s16x8, data, 12)
    d := Simd.Load(s16x8, raw, 1)
    fixed: [10]s16
    for i: 0..9 { fixed[i] = cast(s16) (i * 7 - 20) }
    e := Simd.Load(s16x8, fixed, 2)
    h = Mix(h, Simd.Reinterpret(u8x16, a))
    h = Mix(h, Simd.Reinterpret(u8x16, b))
    h = Mix(h, Simd.Reinterpret(u8x16, c))
    h = Mix(h, Simd.Reinterpret(u8x16, d))
    h = Mix(h, Simd.Reinterpret(u8x16, e))
    out: [16]s16
    Simd.Store(out[:], 3, Simd.Add(a, b))
    Simd.StoreLow(out[:], 12, c)
    Simd.Store(raw, 0, e)
    for i: 0..15 { h = MixValue(h, cast(s64) out[i]) }
    for i: 0..7 { h = MixValue(h, cast(s64) data[i]) }
    numbers: [8]s32
    for i: 0..7 { numbers[i] = cast(s32) (i * i - 9) }
    picks := Simd.Make(s32x4, 7, 0, 3, 3)
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Gather(numbers, picks)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Gather(numbers[:], Simd.Reinterpret(u32x4, picks))))
    return h
}

Width :: () -> u64 {
    a := Simd.Make(s16x8, -300, 300, 127, -128, 255, 0, 32767, -32768)
    b := Simd.Make(s16x8, 1, 2, 3, 4, 5, 6, 7, 8)
    h: u64 = 5
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Narrow(s8x16, a, b)))
    h = Mix(h, Simd.Narrow(u8x16, a, b))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.WidenLow(a)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.WidenHigh(a)))
    h = Mix32(h, Simd.Reinterpret(u8x32, Simd.Widen(a)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.MulWideLow(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.MulWideHigh(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.MulAddPairs(a, b)))
    pixels := Simd.Reinterpret(u8x16, a)
    weights := Simd.Reinterpret(s8x16, b)
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.MulAddPairsSat(pixels, weights)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.AddPairs(a)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.SumAbsDiff(pixels, Simd.Reinterpret(u8x16, b))))
    h = MixValue(h, cast(s64) Simd.ReduceAdd(a))
    h = MixValue(h, cast(s64) Simd.ReduceMin(a))
    h = MixValue(h, cast(s64) Simd.ReduceMax(a))
    h = MixValue(h, Simd.ReduceMaxIndex(b))
    if Simd.AllZero(Simd.Zero(u8x16)) { h = MixValue(h, 1) }
    if Simd.AllZero(pixels) { h = MixValue(h, 2) }
    f := Simd.Make(f32x4, 1.75, -1.75, 3000000000.0, -3000000000.0)
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Convert(s32x4, f)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Convert(u32x4, f)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Convert(f32x4, Simd.Make(s32x4, 1, -1, 16777217, -7))))
    h = Mix32(h, Simd.Reinterpret(u8x32, Simd.Combine(a, b)))
    return h
}

Fingerprint :: () -> u64 {
    data: [16]s16
    for i: 0..15 { data[i] = cast(s16) (i * 1000 - 7000) }
    h := Arithmetic()
    h = MixValue(h, cast(s64) (Compares() >> 1))
    h = MixValue(h, cast(s64) (Shifts(5) >> 1))
    h = MixValue(h, cast(s64) (Shifts(33) >> 1))
    h = MixValue(h, cast(s64) (Lanes() >> 1))
    h = MixValue(h, cast(s64) (Memory(data[:], *data[2]) >> 1))
    h = MixValue(h, cast(s64) (Width() >> 1))
    return h
}

main :: () {
    print("% %\n", Answer(), Fingerprint())
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
expected=
# Bundles also run on the portable VM library, which evaluates every lane
# with the scalar reference.
vm_lib=${ZIRAN_VM_LIB:-$repo/build/libziran-vm.a}
if test -f "$vm_lib"; then
    cat > "$work/vm-run.c" <<'C'
#include "ziran_host.h"
int main(int argc, char **argv) {
    Bundle *bundle = argc == 2 ? BundleOpen(argv[1]) : 0;
    long long result = 0;
    int has_result = 0;
    int ok = bundle != 0 && BundleRun(bundle, 0, 0, &result, &has_result);
    BundleClose(bundle);
    return ok ? 0 : 1;
}
C
    "${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" "$work/vm-run.c" "$vm_lib" -lm -pthread \
        -o "$work/vm-run"
fi
clang=
for candidate in clang clang-21 clang-20 clang-19 clang-18; do
    if command -v "$candidate" >/dev/null 2>&1; then clang=$candidate; break; fi
done
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    output=$work/c-$input
    "$ziran" build --target=c --entry app:main --root "$root" -o "$output" "$module"
    printf '#include "app.h"\nint main(void) { app_main(); return 0; }\n' > "$output/main.c"
    set -- "-O2" "-O2 -DZIRAN_SIMD_SCALAR" "-O1 -D__TINYC__"
    case "$(uname -m)" in
        x86_64|amd64)
            set -- "$@" "-O2 -msse4.1"
            grep -qw avx2 /proc/cpuinfo 2>/dev/null && set -- "$@" "-O2 -mavx2"
            ;;
    esac
    compilers=${CC:-cc}
    test -n "$clang" && compilers="$compilers $clang"
    for compiler in $compilers; do
        for flags in "$@"; do
            # shellcheck disable=SC2086
            "$compiler" -std=c11 -Wno-psabi $flags -I"$output" "$output"/*.c -o "$output/app" -lm
            result=$("$output/app")
            case "$result" in "42 "*) ;; *) echo "$compiler $input $flags: $result" >&2; exit 1 ;; esac
            if test -z "$expected"; then expected=$result
            elif test "$result" != "$expected"; then
                echo "$compiler $input $flags: $result, expected $expected" >&2
                exit 1
            fi
        done
    done
    grep -q 'zi_add_sat_s16x8(' "$output/app.c"
    grep -q 'zi_simd_at(' "$output/app.c"
    # Go runs the same lanes through its port of the reference semantics.
    if command -v go >/dev/null 2>&1; then
        "$ziran" build --target=go --pkg main --entry app:main --exe --root "$root" \
            -o "$work/go-$input" "$module"
        result=$(cd "$work/go-$input" && GO111MODULE=off go run ./*.go)
        test "$result" = "$expected" || { echo "Go $input: $result, expected $expected" >&2; exit 1; }
        if command -v gofmt >/dev/null 2>&1; then
            test -z "$(gofmt -l "$work/go-$input/ziran_simd.go")"
        fi
    fi
    # A bundle runs on the host's native engine where it has one (TinyCC
    # compiles the portable C helpers), otherwise on the portable VM.
    "$ziran" bundle --root "$root" --entry app:main -o "$work/$input.zx" "$module"
    result=$(ZIRAN_NATIVE_CACHE="$work/cache" "$ziran" run "$work/$input.zx")
    test "$result" = "$expected" || { echo "zx $input: $result, expected $expected" >&2; exit 1; }
    if test -x "$work/vm-run"; then
        result=$("$work/vm-run" "$work/$input.zx")
        test "$result" = "$expected" || { echo "VM $input: $result, expected $expected" >&2; exit 1; }
    fi
done

if "${CC:-cc}" -fsanitize=address,undefined -x c -o "$work/probe" - <<'C' 2>/dev/null; then
int main(void) { return 0; }
C
    output=$work/c-source
    "${CC:-cc}" -std=c11 -Wno-psabi -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -I"$output" "$output"/*.c -o "$output/sanitized" -lm
    test "$(ASAN_OPTIONS=detect_leaks=0 "$output/sanitized")" = "$expected"
fi

# WebAssembly: the generated C built for wasm32 with -msimd128 uses SIMD128
# instructions and without it none, and both give the native lanes where
# wasmtime (WASMTIME) runs them. The C needs no libc beyond these stubs.
wasmcc=${ZIRAN_WASM_CC:-$clang}
objdump=
for candidate in llvm-objdump llvm-objdump-21 llvm-objdump-20 llvm-objdump-19 llvm-objdump-18; do
    if command -v "$candidate" >/dev/null 2>&1; then objdump=$candidate; break; fi
done
wasmtime=${WASMTIME:-$(command -v wasmtime || true)}
if test -n "$wasmcc" && printf 'int f(void) { return 0; }\n' |
    $wasmcc --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export=f -x c - \
        -o "$work/probe.wasm" 2>/dev/null; then
    sed '/^main :: () {$/,$d' "$work/app.zi" > "$work/wasm_app.zi"
    cat >> "$work/wasm_app.zi" <<'ZI'
Result :: () -> u64 { return Fingerprint() }
Check :: () -> s64 { return Answer() }
ZI
    "$ziran" build --target=c --root "$work" -o "$work/wasm-c" "$work/wasm_app.zi"
    mkdir -p "$work/freestanding"
    cat > "$work/freestanding/stdio.h" <<'C'
#pragma once
typedef struct FILE FILE;
#define stderr ((FILE *)0)
static inline int fprintf(FILE *file, const char *format, ...) { (void)file; (void)format; return 0; }
C
    printf '#pragma once\n_Noreturn static inline void abort(void) { __builtin_trap(); }\n' \
        > "$work/freestanding/stdlib.h"
    printf '#pragma once\n#define memcpy __builtin_memcpy\n#define memset __builtin_memset\n' \
        > "$work/freestanding/string.h"
    for mode in simd128 baseline; do
        flags=
        test "$mode" = simd128 && flags=-msimd128
        # shellcheck disable=SC2086
        $wasmcc --target=wasm32 -std=c11 -O2 -mbulk-memory -ffreestanding -nostdlib -Wno-psabi $flags \
            -isystem "$work/freestanding" -I"$work/wasm-c" -Wl,--no-entry \
            -Wl,--export=wasm_app_Result -Wl,--export=wasm_app_Check "$work"/wasm-c/*.c \
            -o "$work/$mode.wasm"
        if test -n "$objdump"; then
            count=$("$objdump" -d "$work/$mode.wasm" |
                grep -cE '(i8x16|i16x8|i32x4|i64x2|f32x4|f64x2|v128)\.' || true)
            if test "$mode" = simd128 && test "$count" -lt 100; then
                echo "wasm32 -msimd128: only $count SIMD128 instructions" >&2
                exit 1
            fi
            if test "$mode" = baseline && test "$count" != 0; then
                echo "wasm32 without -msimd128 holds $count SIMD128 instructions" >&2
                exit 1
            fi
        fi
        if test -n "$wasmtime"; then
            # wasmtime warns that --invoke with results is experimental.
            check=$("$wasmtime" run --invoke wasm_app_Check "$work/$mode.wasm" 2> "$work/wasmtime.err" | tail -n 1)
            # It prints an i64 result as signed.
            result=$(printf '%u' "$("$wasmtime" run --invoke wasm_app_Result "$work/$mode.wasm" 2>> "$work/wasmtime.err" | tail -n 1)")
            test "$check $result" = "$expected" ||
                { cat "$work/wasmtime.err" >&2; echo "wasm32 $mode: $check $result, expected $expected" >&2; exit 1; }
            echo "wasm32 $mode: $check $result"
        fi
    done
fi

# Android NDK (ANDROID_NDK_HOME): AArch64 and ARMv7 with NEON, ARMv7 with VFP
# only and the scalar lanes, run under QEMU_AARCH64 / QEMU_ARM or on the ARMv7
# device ARMV7_ADB_SERIAL names.
if test -n "${ANDROID_NDK_HOME:-}"; then
    bin=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin
    output=$work/c-source
    arm_run() {
        triple=$1 binary=$2
        case "$triple" in
            aarch64*) qemu=${QEMU_AARCH64:-} device= ;;
            *) qemu=${QEMU_ARM:-} device=${ARMV7_ADB_SERIAL:-} ;;
        esac
        if test -n "$qemu"; then
            "$qemu" "$binary"
        elif test -n "$device"; then
            remote=/data/local/tmp/ziran-simd-$$
            adb -s "$device" push "$binary" "$remote" >/dev/null
            if adb -s "$device" shell "chmod 755 $remote && $remote; status=\$?; rm -f $remote; exit \$status" \
                > "$work/arm.out"; then
                tr -d '\r' < "$work/arm.out"
            else
                tr -d '\r' < "$work/arm.out"
                return 1
            fi
        else
            return 2
        fi
    }
    for build in "aarch64-linux-android24:-O2" "aarch64-linux-android24:-O2 -DZIRAN_SIMD_SCALAR" \
        "armv7a-linux-androideabi24:-O2" "armv7a-linux-androideabi24:-O2 -mfpu=vfpv3-d16" \
        "armv7a-linux-androideabi24:-O2 -DZIRAN_SIMD_SCALAR"; do
        triple=${build%%:*} flags=${build#*:}
        # shellcheck disable=SC2086
        "$bin/$triple-clang" -std=c11 -static -Wno-psabi $flags -I"$output" "$output"/*.c -lm \
            -o "$work/$triple-app"
        if test "$flags" = -O2; then
            # The NEON build holds NEON instructions (q registers or .16b-style lanes).
            "$bin/llvm-objdump" -d "$work/$triple-app" | grep -qE 'v[a-z0-9]+\.[iuf]?[0-9]+[[:space:]]+q[0-9]|\.(16b|8h|4s|2d)' ||
                { echo "$triple: no NEON instructions" >&2; exit 1; }
        fi
        status=0
        result=$(arm_run "$triple" "$work/$triple-app") || status=$?
        if test "$status" = 2; then
            case "$triple" in
                aarch64*) echo "$triple $flags built; set QEMU_AARCH64 to run it" ;;
                *) echo "$triple $flags built; set QEMU_ARM or ARMV7_ADB_SERIAL to run it" ;;
            esac
            continue
        fi
        test "$status" = 0 && test "$result" = "$expected" ||
            { echo "$triple $flags: $result (status $status), expected $expected" >&2; exit 1; }
        echo "$triple $flags: $result"
    done
fi

# A slice access outside the data stops the program.
cat > "$work/outside.zi" <<'ZI'
Simd :: #import "simd"
main :: () {
    data: [6]s16
    v := Simd.Load(s16x8, data[:], 0)
    print("%\n", Simd.Lane(v, 0))
}
ZI
"$ziran" build --target=c --entry outside:main --root "$work" -o "$work/outside" "$work/outside.zi"
printf '#include "outside.h"\nint main(void) { outside_main(); return 0; }\n' > "$work/outside/main.c"
"${CC:-cc}" -std=c11 -Wno-psabi -O2 -I"$work/outside" "$work/outside"/*.c -o "$work/outside/app"
if "$work/outside/app" 2> "$work/outside.err"; then
    echo "an out-of-bounds SIMD load ran" >&2
    exit 1
fi
grep -q "out of bounds" "$work/outside.err"
if command -v go >/dev/null 2>&1; then
    "$ziran" build --target=go --pkg main --entry outside:main --exe --root "$work" \
        -o "$work/outside-go" "$work/outside.zi"
    if (cd "$work/outside-go" && GO111MODULE=off go run ./*.go) 2> "$work/outside-go.err"; then
        echo "an out-of-bounds SIMD load ran on Go" >&2
        exit 1
    fi
    grep -q "out of bounds" "$work/outside-go.err"
fi
"$ziran" bundle --root "$work" --entry outside:main -o "$work/outside.zx" "$work/outside.zi"
if ZIRAN_NATIVE_CACHE="$work/cache" "$ziran" run "$work/outside.zx" 2> "$work/outside-zx.err"; then
    echo "an out-of-bounds SIMD load ran in a bundle" >&2
    exit 1
fi
grep -q "out of bounds" "$work/outside-zx.err"
if test -x "$work/vm-run"; then
    if "$work/vm-run" "$work/outside.zx" 2> "$work/outside-vm.err"; then
        echo "an out-of-bounds SIMD load ran on the portable VM" >&2
        exit 1
    fi
    grep -q "out of bounds" "$work/outside-vm.err"
fi

# Vectors are values wherever they are kept: record fields, nested records,
# arrays and slices of vectors, pointers, globals, parameters and results.
# The portable VM holds a 128-bit vector inline in its value and a 256-bit
# one as an array of lanes; copies never share lanes.
cat > "$work/values.zi" <<'ZI'
Simd :: #import "simd"
Lanes :: struct { tag: s32; v: s32x4; wide: s16x16; f: f32x4 }
Box :: struct { inner: Lanes; rows: [3]u8x16 }
Global: s32x4;
Bump :: (v: s32x4, by: s32) -> s32x4 #cold { return Simd.Add(v, Simd.Splat(s32x4, by)) }
Fill :: (p: *s32x4, x: s32) #cold { <<p = Simd.Splat(s32x4, x) }
SumRows :: (rows: []s32x4) -> s32x4 #cold {
    total := Simd.Zero(s32x4)
    for row: rows { total = Simd.Add(total, row) }
    return total
}
Make :: (x: s32) -> Lanes #cold {
    l: Lanes
    l.tag = x
    l.v = Simd.Make(s32x4, x, x + 1, x + 2, x + 3)
    l.wide = Simd.Splat(s16x16, cast(s16) x)
    l.f = Simd.Make(f32x4, 1.5, -0.0, 0.25, 3.0)
    return l
}
main :: () {
    rows: [4]s32x4
    for i: 0..3 { rows[i] = Simd.Splat(s32x4, cast(s32) i) }
    rows[2] = Bump(rows[2], 10)
    copy := rows
    copy[0] = Simd.Splat(s32x4, 100)
    Fill(*rows[3], 7)
    Global = SumRows(rows[:])
    b: Box
    b.inner = Make(5)
    b.rows[1] = Simd.Splat(u8x16, 200)
    saved := b
    b.inner.v = Simd.Zero(s32x4)
    v := saved.inner.v
    p := *v
    <<p = Bump(<<p, 1)
    print("% % % % % % % %\n", Simd.Lane(Global, 0), Simd.Lane(copy[0], 1), Simd.Lane(rows[0], 0),
          Simd.Lane(v, 3), Simd.Lane(saved.inner.wide, 15), Simd.Lane(saved.rows[1], 4),
          Simd.Lane(b.inner.v, 0), saved.inner.tag)
    print("% % %\n", Simd.Lane(saved.inner.f, 0), Simd.Lane(saved.inner.f, 1), Simd.Lane(saved.inner.f, 2))
}
ZI
values_expected='20 100 0 9 5 200 0 5
1.5 -0 0.25'
"$ziran" build --target=c --entry values:main --root "$work" -o "$work/values" "$work/values.zi"
printf '#include "values.h"\nint main(void) { values_main(); return 0; }\n' > "$work/values/main.c"
for flags in "-O2" "-O2 -DZIRAN_SIMD_SCALAR" "-O1 -D__TINYC__"; do
    "${CC:-cc}" -std=c11 -Wno-psabi $flags -I"$work/values" "$work/values"/*.c -o "$work/values/app"
    test "$("$work/values/app")" = "$values_expected" ||
        { echo "vector values $flags: wrong result" >&2; exit 1; }
done
if command -v go >/dev/null 2>&1; then
    "$ziran" build --target=go --pkg main --entry values:main --exe --root "$work" \
        -o "$work/values-go" "$work/values.zi"
    test "$(cd "$work/values-go" && GO111MODULE=off go run ./*.go)" = "$values_expected"
fi
"$ziran" bundle --root "$work" --entry values:main -o "$work/values.zx" "$work/values.zi"
test "$(ZIRAN_NATIVE_CACHE="$work/cache" "$ziran" run "$work/values.zx")" = "$values_expected"
if test -x "$work/vm-run"; then
    test "$("$work/vm-run" "$work/values.zx")" = "$values_expected" ||
        { echo "vector values on the portable VM: wrong result" >&2; exit 1; }
fi

# Loads and gathers read a constant table in place, as index reads do,
# instead of copying it into a local array at every call.
cat > "$work/table.zi" <<'ZI'
Simd :: #import "simd"
TABLE :: s32.[10, 11, 12, 13, 14, 15, 16, 17]
main :: () {
    sum := Simd.Zero(s32x4)
    picks := Simd.Make(s32x4, 7, 0, 3, 5)
    for i: 0..cast(s64)3 {
        sum = Simd.Add(sum, Simd.Gather(TABLE, picks))
        sum = Simd.Add(sum, Simd.Load(s32x4, TABLE, i))
    }
    print("% % %\n", Simd.Lane(sum, 0), Simd.Lane(sum, 3), TABLE.count)
}
ZI
"$ziran" build --target=c --entry table:main --root "$work" -o "$work/table" "$work/table.zi"
sed -n '/^table_main(/,/^}/p' "$work/table/table.c" > "$work/table-main.c"
grep -q 'zi_gather_s32x4_s32x4(ZIRAN_TABLE_H_table0, ' "$work/table-main.c"
if grep -q 'memmove\|static const' "$work/table-main.c"; then
    echo "a SIMD load or gather copied a constant table" >&2
    exit 1
fi
printf '#include "table.h"\nint main(void) { table_main(); return 0; }\n' > "$work/table/main.c"
for flags in "-O2" "-O2 -DZIRAN_SIMD_SCALAR" "-O1 -D__TINYC__"; do
    "${CC:-cc}" -std=c11 -Wno-psabi -Werror=discarded-qualifiers $flags -I"$work/table" \
        "$work/table"/*.c -o "$work/table/app"
    test "$("$work/table/app")" = "114 118 8" ||
        { echo "constant table SIMD reads $flags: wrong result" >&2; exit 1; }
done
if command -v go >/dev/null 2>&1; then
    "$ziran" build --target=go --pkg main --entry table:main --exe --root "$work" \
        -o "$work/table-go" "$work/table.zi"
    test "$(cd "$work/table-go" && GO111MODULE=off go run ./*.go)" = "114 118 8"
    grep -q 'zi_gather_s32x4_s32x4(Table_ziran_table0\[:\], ' "$work/table-go/table.go"
fi

# A module that only names vector types includes the small types header;
# only the module that calls vector operations pays for the ops header.
# #cold keeps lib's procedures out of line; inlined copies would bring
# their operations, and the ops header, into the caller.
mkdir "$work/split"
cat > "$work/split/lib.zi" <<'ZI'
Simd :: #import "simd"
Pair :: struct { v: s32x4 }
Make :: (x: s32) -> Pair #cold {
    p: Pair
    p.v = Simd.Add(Simd.Splat(s32x4, x), Simd.Splat(s32x4, 1))
    return p
}
Total :: (v: s32x4) -> s32 #cold { return Simd.Lane(v, 0) + Simd.Lane(v, 3) }
ZI
cat > "$work/split/user.zi" <<'ZI'
Lib :: #import "lib"
main :: () {
    p := Lib.Make(20)
    v: s32x4 = p.v
    print("%\n", Lib.Total(v))
}
ZI
"$ziran" build --target=c --entry user:main --root "$work/split" \
    -o "$work/split/c" "$work/split/user.zi"
grep -q '#include "ziran_simd_types.h"' "$work/split/c/lib.h"
grep -q '#include "ziran_simd.h"' "$work/split/c/lib.c"
if grep -q '#include "ziran_simd.h"' "$work/split/c/lib.h" "$work/split/c/user.h" "$work/split/c/user.c"; then
    echo "a module that only names vector types included the SIMD ops header" >&2
    exit 1
fi
printf '#include "user.h"\nint main(void) { user_main(); return 0; }\n' > "$work/split/c/main.c"
for flags in "-O2" "-O2 -DZIRAN_SIMD_SCALAR" "-O1 -D__TINYC__"; do
    "${CC:-cc}" -std=c11 -Wno-psabi $flags -I"$work/split/c" "$work/split/c"/*.c -o "$work/split/app"
    test "$("$work/split/app")" = 42 ||
        { echo "split SIMD headers $flags: wrong result" >&2; exit 1; }
done

reject() {
    printf 'Simd :: #import "simd"\nmain :: () {\n    a := Simd.Splat(s16x8, 3)\n    %s\n}\n' "$1" > "$work/bad.zi"
    if "$ziran" ir --root "$work" -o "$work/bad-ir" "$work/bad.zi" 2> "$work/bad.err"; then
        echo "accepted: $1" >&2
        exit 1
    fi
    grep -q "$2" "$work/bad.err" || { cat "$work/bad.err" >&2; exit 1; }
}
reject 'b := Simd.Add(a, Simd.Splat(u16x8, 3))' 'Simd.Add takes two vectors of one type'
reject 'b := Simd.Lane(a, 8)' 'Simd.Lane takes a constant lane index below the lane count'
reject 'b := Simd.Splat(u8x16, 300)' 'Simd.Splat takes the vector type and one value of its lane type'
reject 'b := a + a' 'vectors use std/simd operations'
reject 'b := a == a' 'vectors use std/simd operations'
reject 'b := Simd.Frob(a)' 'unresolved function: Simd.Frob'
reject 'b := Simd.Shuffle(a, 0, 1, 2, 3, 4, 5, 6, 8)' 'constant lane indices'
reject 'b := Simd.Narrow(u8x16, a)' 'Simd.Narrow takes'
echo "simd: C, Go, bundle and portable VM lanes agree ($expected)"
