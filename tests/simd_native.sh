#!/bin/sh
# Native vectors (u8xN .. f64xN): one 128-bit block in base code, Go, the
# VM and non-AVX2 clones, and two blocks in AVX2 clones and -mavx2 builds.
# A loop stepping by Simd.Lanes(T) prints the same hash on every width,
# compiler, forced tier, Go and bundles, from source and
# saved IR, while Simd.Blocks(u8xN) reports the width each build picked.
# Checking keeps native vectors plain values that never cross code built
# for another width.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# One body, built once as a plain procedure and once as a #target one.
cat > "$work/body.zi" <<'ZI'
    step := Simd.Lanes(u8xN)
    if step != 16 * Simd.Blocks(u8xN) || Simd.Lanes(s16xN) * 2 != step || Simd.Lanes(f64xN) * 8 != step { return 0 }
    quarter := Simd.Lanes(s32xN)
    order := Simd.Make(u8xN, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
    sum: u64 = 0
    sum32: u32 = 0
    best: u16 = 0
    best_at: s64 = -1
    i: s64 = 0
    while i + step <= 224 {
        a := Simd.Load(u8xN, data, i)
        b := Simd.Load(u8xN, data, i + 3)
        mixed := Simd.Xor(Simd.ByteShuffle(a, Simd.And(b, Simd.Splat(u8xN, 31))), Simd.ByteShuffle(b, order))
        mixed = Simd.AddSat(Simd.AlignBytes(mixed, a, 5), Simd.ShiftBytesLeft(b, 2))
        mixed = Simd.Xor(mixed, Simd.InterleaveHigh(a, b))
        mixed = Simd.Select(Simd.Greater(a, b), Simd.InterleaveLow(b, a), mixed)
        Simd.Store(bytes, i, mixed)
        low := Simd.Sub(Simd.Mul(Simd.WidenLow(a), Simd.Splat(u16xN, 3)),
            Simd.MulHigh(Simd.WidenLow(b), Simd.Splat(u16xN, 40000)))
        high := Simd.Sub(Simd.Mul(Simd.WidenHigh(a), Simd.Splat(u16xN, 3)),
            Simd.MulHigh(Simd.WidenHigh(b), Simd.Splat(u16xN, 40000)))
        low = Simd.ShiftRightRound(low, 1)
        high = Simd.ShiftRightRound(high, 1)
        packed := Simd.Narrow(u8xN, Simd.Reinterpret(s16xN, low), Simd.Reinterpret(s16xN, high))
        Simd.Store(packed_out, i, Simd.Average(packed, a))
        signed_low := Simd.Reinterpret(s16xN, low)
        signed_high := Simd.Reinterpret(s16xN, high)
        b_low := Simd.Reinterpret(s16xN, Simd.WidenLow(b))
        b_high := Simd.Reinterpret(s16xN, Simd.WidenHigh(b))
        Simd.Store(words, i, Simd.MulWideLow(signed_low, b_low))
        Simd.Store(words, i + quarter, Simd.MulWideHigh(signed_low, b_low))
        Simd.Store(words, i + 2 * quarter, Simd.MulWideLow(signed_high, b_high))
        Simd.Store(words, i + 3 * quarter, Simd.MulWideHigh(signed_high, b_high))
        Simd.Store(pairs, i / 2, Simd.MulAddPairs(signed_low, b_low))
        Simd.Store(pairs, i / 2 + quarter, Simd.MulAddPairs(signed_high, b_high))
        sum += Simd.ReduceAdd(Simd.SumAbsDiff(a, b))
        sum32 += Simd.ReduceAdd(Simd.Reinterpret(u32xN, Simd.MulAddPairsSat(a, Simd.Reinterpret(s8xN, b))))
        top := Simd.ReduceMax(low)
        if best_at < 0 || top > best {
            best = top
            best_at = i + Simd.ReduceMaxIndex(low)
        }
        top = Simd.ReduceMax(high)
        if top > best {
            best = top
            best_at = i + Simd.Lanes(u16xN) + Simd.ReduceMaxIndex(high)
        }
        picks := Simd.And(Simd.Reinterpret(s32xN, a), Simd.Splat(s32xN, 63))
        got := Simd.Shuffle(Simd.Gather(table, picks), 1, 0, 3, 2)
        half := Simd.Convert(s32xN, Simd.Mul(Simd.Convert(f32xN, got), Simd.Splat(f32xN, 0.5)))
        Simd.Store(gathered, i / 4, Simd.Add(got, Simd.ShiftRight(half, 3)))
        i += step
    }
    j: s64 = 0
    while j + quarter / 2 <= 224 {
        v := Simd.LoadLow(s32xN, words, j)
        Simd.StoreLow(halves, j, Simd.Add(v, Simd.Splat(s32xN, 7)))
        j += quarter / 2
    }
    return ((sum * 1099511628211 + cast(u64) sum32) * 1099511628211 + cast(u64) best) * 1099511628211 +
        cast(u64) best_at
ZI
{
    cat <<'ZI'
Simd :: #import "simd"

Mix :: (hash: u64, value: u64) -> u64 { return (hash ^ value) * 1099511628211 }

Plain :: (data: []u8, table: []s32, bytes: []u8, packed_out: []u8, words: []s32, pairs: []s32,
    gathered: []s32, halves: []s32) -> u64 {
ZI
    cat "$work/body.zi"
    cat <<'ZI'
}

Cloned :: (data: []u8, table: []s32, bytes: []u8, packed_out: []u8, words: []s32, pairs: []s32,
    gathered: []s32, halves: []s32) -> u64 #target {
ZI
    cat "$work/body.zi"
    cat <<'ZI'
}

TargetBlocks :: () -> s64 #target { return Simd.Blocks(u8xN) }

Run :: (data: []u8, table: []s32, cloned: bool) -> u64 {
    bytes: [256]u8
    packed: [256]u8
    words: [256]s32
    pairs: [128]s32
    gathered: [64]s32
    halves: [256]s32
    h: u64 = 14695981039346656037
    if cloned {
        h = Mix(h, Cloned(data, table, bytes[:], packed[:], words[:], pairs[:], gathered[:], halves[:]))
    } else {
        h = Mix(h, Plain(data, table, bytes[:], packed[:], words[:], pairs[:], gathered[:], halves[:]))
    }
    for i: 0..255 { h = Mix(Mix(h, cast(u64) bytes[i]), cast(u64) packed[i]) }
    for i: 0..255 { h = Mix(Mix(h, cast(u64) cast(u32) words[i]), cast(u64) cast(u32) halves[i]) }
    for i: 0..127 { h = Mix(h, cast(u64) cast(u32) pairs[i]) }
    for i: 0..63 { h = Mix(h, cast(u64) cast(u32) gathered[i]) }
    return h
}

main :: () {
    data: [256]u8
    for i: 0..255 { data[i] = cast(u8) ((i * 73 + 19) & 255) }
    table: [64]s32
    for i: 0..63 { table[i] = cast(s32) (i * 1009 - 30000) }
    print("% %\n", Run(data[:], table[:], false), Run(data[:], table[:], true))
    print("blocks % %\n", Simd.Blocks(u8xN), TargetBlocks())
}
ZI
} > "$work/app.zi"

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
clang=
for candidate in clang clang-21 clang-20 clang-19 clang-18; do
    if command -v "$candidate" >/dev/null 2>&1; then clang=$candidate; break; fi
done
x86=
case "$(uname -m)" in x86_64|amd64) x86=1 ;; esac
best=1
if test -n "$x86"; then
    grep -qw ssse3 /proc/cpuinfo 2>/dev/null && best=2
    test "$best" = 2 && grep -qw sse4_1 /proc/cpuinfo 2>/dev/null && best=3
    test "$best" = 3 && grep -qw avx2 /proc/cpuinfo 2>/dev/null && best=4
fi
blocks_at() { if test "$1" -ge 4; then echo 2; else echo 1; fi; }

# The first line must match everywhere; the second names the base and
# #target widths the build should have picked, or is not checked when empty.
expected=
agree() {
    hash=$(printf '%s\n' "$2" | sed -n 1p)
    widths=$(printf '%s\n' "$2" | sed -n 2p)
    case "$hash" in *" "*) ;; *) echo "$1: no hash in: $2" >&2; exit 1 ;; esac
    test "${hash% *}" = "${hash#* }" || { echo "$1: plain and #target differ: $hash" >&2; exit 1; }
    if test -z "$expected"; then expected=$hash
    elif test "$hash" != "$expected"; then
        echo "$1: $hash, expected $expected" >&2
        exit 1
    fi
    if test -n "$3" && test "$widths" != "blocks $3"; then
        echo "$1: $widths, expected blocks $3" >&2
        exit 1
    fi
}
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
    # The AVX2 clone spells 256-bit types; base code keeps the native names.
    grep -q 'zi_avx2_sub_u16x16(' "$output/app.c"
    grep -q 'zi_avx2_native_widen_low_u8x32(' "$output/app.c"
    grep -q 'zi_sse41_sub_u16x8(' "$output/app.c"
    grep -q 'zi_u8xN ' "$output/app.c"
    compilers=${CC:-cc}
    test -n "$clang" && compilers="$compilers $clang"
    for compiler in $compilers; do
        set -- "-O2" "-O2 -DZIRAN_SIMD_SCALAR" "-O1 -D__TINYC__"
        if test -n "$x86"; then
            set -- "$@" "-O2 -mssse3" "-O2 -msse4.1"
            test "$best" = 4 && set -- "$@" "-O2 -mavx2"
        fi
        for flags in "$@"; do
            # shellcheck disable=SC2086
            "$compiler" -std=c11 -Wall -Wno-unused-function -Wno-psabi $flags -I"$output" \
                "$output"/*.c -o "$output/app" -lm
            widths=
            case "$flags" in
                "-O2") widths="1 $(blocks_at "$best")" ;;
                "-O2 -mavx2") widths="2 2" ;;
                *SCALAR*|*TINYC*) widths="1 1" ;;
            esac
            agree "$compiler $input $flags" "$("$output/app")" "$widths"
            test "$flags" = "-O2" && test -n "$x86" || continue
            tier=1
            while test "$tier" -le 4; do
                agree "$compiler $input tier $tier" "$(ZIRAN_SIMD_TIER=$tier "$output/app")" \
                    "1 $(blocks_at "$(( tier < best ? tier : best ))")"
                tier=$((tier + 1))
            done
        done
    done
    if test "$input" = source && "${CC:-cc}" -fsanitize=address,undefined -x c -o "$work/probe" - <<'C' 2>/dev/null; then
int main(void) { return 0; }
C
        "${CC:-cc}" -std=c11 -Wno-psabi -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined \
            -I"$output" "$output"/*.c -o "$output/sanitized" -lm
        for tier in 1 4; do
            agree "sanitized tier $tier" "$(ASAN_OPTIONS=detect_leaks=0 ZIRAN_SIMD_TIER=$tier "$output/sanitized")" ""
        done
        if test "$best" = 4; then
            "${CC:-cc}" -std=c11 -Wno-psabi -O1 -mavx2 -fsanitize=address,undefined -fno-sanitize-recover=undefined \
                -I"$output" "$output"/*.c -o "$output/sanitized" -lm
            agree "sanitized -mavx2" "$(ASAN_OPTIONS=detect_leaks=0 "$output/sanitized")" "2 2"
        fi
    fi
    if command -v go >/dev/null 2>&1; then
        "$ziran" build --target=go --pkg main --entry app:main --exe --root "$root" \
            -o "$work/go-$input" "$module"
        agree "Go $input" "$(cd "$work/go-$input" && GO111MODULE=off go run ./*.go)" "1 1"
    fi
    "$ziran" bundle --root "$root" --entry app:main -o "$work/$input.zx" "$module"
    agree "zx $input" "$("$ziran" run "$work/$input.zx")" ""
done

reject() {
    printf 'Simd :: #import "simd"\n%s\nmain :: () {}\n' "$1" > "$work/bad.zi"
    if "$ziran" ir --root "$work" -o "$work/bad-ir" "$work/bad.zi" 2> "$work/bad.err"; then
        echo "accepted: $1" >&2
        exit 1
    fi
    grep -q "$2" "$work/bad.err" || { cat "$work/bad.err" >&2; exit 1; }
}
reject 'Pair :: struct { v: s16xN }' 'a global or record field cannot hold one'
reject 'kept: u8xN;' 'a global or record field cannot hold one'
reject 'Rows :: () { rows: [2]s16xN }' 'cannot be part of an array'
reject 'Point :: (p: *u32xN) {}' 'cannot be part of an array'
reject 'Mixed :: () -> s16x8 { return Simd.Add(Simd.Zero(s16xN), Simd.Zero(s16x8)) }' 'only with other native vectors'
reject 'First :: () -> s16 { return Simd.Lane(Simd.Zero(s16xN), 0) }' 'does not take native vectors'
reject 'Half :: () -> s16x8 { return Simd.Low(Simd.Zero(s16x16)) }
Join :: () { v := Simd.Combine(Simd.Zero(s16xN), Simd.Zero(s16xN)) }' 'does not take native vectors'
reject 'Wide :: (v: s16xN) -> s64 #target { return 0 }' 'cannot take a native vector'
reject 'Wide :: () -> s16xN #target { return Simd.Zero(s16xN) }' 'cannot return a native vector'
reject 'Take :: (v: s16xN) -> s64 { return 0 }
Wide :: () -> s64 #target { return Take(Simd.Zero(s16xN)) }' 'cannot pass a native vector'
reject 'libc :: #system_library "libc";
Outside :: (v: s16xN) #foreign libc;' 'a #foreign procedure cannot take or return a native vector'
reject 'Rows :: (rows: []s16xN) {}' 'cannot be part of an array, record, pointer, slice'
reject 'Size :: () -> s64 { return size_of(u8xN) }' 'requires a known sized type: u8xN'
reject 'Count :: () -> s64 { return Simd.Lanes(s16xN, 2) }' 'takes one vector type'
echo "simd_native: every width, tier, Go and bundles agree ($expected)"
