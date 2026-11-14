#!/bin/sh
# #target procedures: generated C carries one clone per x86-64 tier (SSSE3,
# SSE4.1, AVX2) behind a dispatcher that picks the best one this CPU runs,
# and ZIRAN_SIMD_TIER=1..4 forces a lower one. A clone calls its tier's copy
# of each #inline procedure of the module that uses std/simd. Every forced tier, every
# fixed-ISA build, sanitizers, Go and bundles print the same
# lanes, from source and from saved IR. Checking keeps 256-bit vectors from
# crossing a #target procedure's calls by value, where AVX2 code passes them
# differently.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Simd :: #import "simd"

// A public #target procedure keeps its dispatcher out of header copies.
Spread :: (out: []s32, value: s32) #target {
    limit := Simd.Make(s32x4, 0, 5, -5, 10)
    Simd.Store(out, 0, Simd.Max(Simd.Splat(s32x4, value), limit))
    Simd.Store(out, 4, Simd.Min(Simd.Splat(s32x4, value), limit))
}
ZI
cat > "$work/app.zi" <<'ZI'
Simd :: #import "simd"
Lib :: #import "lib"

Mix :: (hash: u64, v: u8x16) -> u64 {
    bytes: [16]u8
    Simd.Store(bytes[:], 0, v)
    h := hash
    for i: 0..15 { h = (h ^ cast(u64) bytes[i]) * 1099511628211 }
    return h
}

// Each tier's instruction families, with 256-bit vectors kept inside.
Kernel :: (data: []u8, step: s64) -> u64 #target {
    h: u64 = 14695981039346656037
    table := Simd.Load(u8x16, data, 0)
    picks := Simd.Load(u8x16, data, 16)
    h = Mix(h, Simd.ByteShuffle(table, picks))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.MulAddPairsSat(table, Simd.Reinterpret(s8x16, picks))))
    h = Mix(h, Simd.AlignBytes(table, picks, 7))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Abs(Simd.Reinterpret(s8x16, picks))))
    a := Simd.Reinterpret(s32x4, table)
    b := Simd.Reinterpret(s32x4, picks)
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Min(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Mul(a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Narrow(u16x8, a, b)))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.WidenLow(Simd.Reinterpret(s16x8, table))))
    h = Mix(h, Simd.Reinterpret(u8x16, Simd.Equal(Simd.Reinterpret(u64x2, a), Simd.Reinterpret(u64x2, b))))
    total: u64 = 0
    i := 32
    while i + 33 <= data.count {
        wide := Simd.Load(u8x32, data, i)
        next := Simd.Load(u8x32, data, i + 1)
        blend := Simd.AddSat(Simd.Average(wide, next), Simd.Splat(u8x32, 3))
        h = Mix(Mix(h, Simd.Low(blend)), Simd.High(blend))
        words := Simd.Widen(Simd.Low(wide))
        h = Mix(h, Simd.Reinterpret(u8x16, Simd.High(Simd.MulHigh(words, Simd.Widen(Simd.High(next))))))
        lanes := Simd.Reinterpret(s32x8, next)
        h = Mix(h, Simd.Reinterpret(u8x16, Simd.Low(Simd.Max(lanes, Simd.Shuffle(lanes, 1, 0, 3, 2)))))
        total += cast(u64) Simd.ReduceAdd(Simd.SumAbsDiff(wide, next))
        i += cast(s32) step
    }
    return (h ^ total) * 1099511628211
}

// An array result reaches the clones through the hidden result argument.
Row :: (base: s32) -> [4]s32 #target {
    out: [4]s32
    Simd.Store(out[:], 0, Simd.Add(Simd.Splat(s32x4, base), Simd.Make(s32x4, 1, 2, 3, 4)))
    return out
}

// a raised to at least each row of four lanes; the loop keeps it a call.
Raise :: (a: s32x4, rows: []s32) -> s32x4 #inline {
    best := a
    i := 0
    while i + 4 <= rows.count {
        best = Simd.Max(best, Simd.Load(s32x4, rows, i))
        i += 4
    }
    return best
}

// Raise over both tables, with no std/simd of its own.
RaiseBoth :: (a: s32x4, first: []s32, second: []s32) -> s32x4 #inline {
    best := a
    for pass: 0..1 {
        if pass == 0 { best = Raise(best, first) } else { best = Raise(best, second) }
    }
    return best
}

// Each clone calls its tier's copies of RaiseBoth and Raise.
Peak :: (first: []s32, second: []s32) -> s32 #target {
    return Simd.ReduceMax(RaiseBoth(Simd.Splat(s32x4, -1000), first, second))
}

main :: () {
    data: [160]u8
    for i: 0..159 { data[i] = cast(u8) ((i * 37 + 11) & 255) }
    row := Row(40)
    spread: [8]s32
    Lib.Spread(spread[:], 7)
    first: [8]s32
    second: [12]s32
    for i: 0..7 { first[i] = cast(s32) ((i * 73) % 41 - 20) }
    for i: 0..11 { second[i] = cast(s32) ((i * 29) % 53 - 30) }
    print("% % % % % %\n", Kernel(data[:], 9), row[3], spread[0], spread[3], spread[6],
          Peak(first[:], second[:]))
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
clang=
for candidate in clang clang-21 clang-20 clang-19 clang-18; do
    if command -v "$candidate" >/dev/null 2>&1; then clang=$candidate; break; fi
done
x86=
case "$(uname -m)" in x86_64|amd64) x86=1 ;; esac
# The best tier this CPU runs; forcing a tier only ever lowers it.
best=1
if test -n "$x86"; then
    grep -qw ssse3 /proc/cpuinfo 2>/dev/null && best=2
    test "$best" = 2 && grep -qw sse4_1 /proc/cpuinfo 2>/dev/null && best=3
    test "$best" = 3 && grep -qw avx2 /proc/cpuinfo 2>/dev/null && best=4
fi
debugger=
if command -v gdb >/dev/null 2>&1 && test -n "$x86" &&
    gdb -batch -ex run --args /bin/true >/dev/null 2>&1; then
    debugger=gdb
fi
expected=
agree() {
    if test -z "$expected"; then expected=$2
    elif test "$2" != "$expected"; then
        echo "$1: $2, expected $expected" >&2
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
    grep -q 'static ZI_TARGET_AVX2 uint64_t' "$output/app.c"
    grep -q 'zi_avx2_byte_shuffle_u8x16(' "$output/app.c"
    grep -q 'zi_sse41_min_s32x4(' "$output/app.c"
    grep -q 'case 2: app_Row_ziran_ssse3(array_result_0_0, base); return;' "$output/app.c"
    # A clone calls its tier's copy of an #inline procedure that uses
    # std/simd, itself or through another, with that tier's helpers.
    copy() { awk -v name="$1" '$0 ~ "^" name "\\(.*\\)$" {on = 1} on {print} on && /^}/ {exit}' "$output/app.c"; }
    copy app_Peak_ziran_avx2 | grep -q 'app_RaiseBoth_ziran_avx2('
    copy app_RaiseBoth_ziran_sse41 | grep -q 'app_Raise_ziran_sse41('
    copy app_Raise_ziran_sse41 | grep -q 'zi_sse41_max_s32x4('
    copy app_Raise_ziran_avx2 | grep -q 'zi_avx2_max_s32x4('
    # The dispatcher stays out of line, so no header copy skips it.
    ! grep -q 'Spread_ziran_inline' "$output/lib.h"
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
            agree "$compiler $input $flags" "$("$output/app")"
            test "$flags" = "-O2" && test -n "$x86" || continue
            tier=1
            while test "$tier" -le 4; do
                agree "$compiler $input tier $tier" "$(ZIRAN_SIMD_TIER=$tier "$output/app")"
                tier=$((tier + 1))
            done
        done
        # The clone each forced tier runs, where a debugger can watch.
        if test -n "$debugger" && test "$compiler" = "${CC:-cc}"; then
            "$compiler" -std=c11 -g -O1 -Wno-psabi -I"$output" "$output"/*.c -o "$output/app" -lm
            for clone in 2:ssse3 3:sse41 4:avx2; do
                tier=${clone%%:*}
                test "$tier" -le "$best" || continue
                ZIRAN_SIMD_TIER=$tier gdb -batch -ex "break app_Kernel_ziran_${clone#*:}" -ex run \
                    "$output/app" > "$work/gdb.out" 2>&1
                grep -q "Breakpoint 1, app_Kernel_ziran_${clone#*:}" "$work/gdb.out" ||
                    { cat "$work/gdb.out" >&2; exit 1; }
            done
            ZIRAN_SIMD_TIER=1 gdb -batch -ex "break app_Kernel_ziran_ssse3" \
                -ex "break app_Kernel_ziran_sse41" -ex "break app_Kernel_ziran_avx2" -ex run \
                "$output/app" > "$work/gdb.out" 2>&1
            if grep -q "Breakpoint [0-9], app_Kernel_ziran" "$work/gdb.out"; then
                echo "ZIRAN_SIMD_TIER=1 ran a clone" >&2
                exit 1
            fi
        fi
    done
    if test "$input" = source && "${CC:-cc}" -fsanitize=address,undefined -x c -o "$work/probe" - <<'C' 2>/dev/null; then
int main(void) { return 0; }
C
        "${CC:-cc}" -std=c11 -Wno-psabi -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined \
            -I"$output" "$output"/*.c -o "$output/sanitized" -lm
        for tier in 1 4; do
            agree "sanitized tier $tier" "$(ASAN_OPTIONS=detect_leaks=0 ZIRAN_SIMD_TIER=$tier "$output/sanitized")"
        done
    fi
    if command -v go >/dev/null 2>&1; then
        "$ziran" build --target=go --pkg main --entry app:main --exe --root "$root" \
            -o "$work/go-$input" "$module"
        agree "Go $input" "$(cd "$work/go-$input" && GO111MODULE=off go run ./*.go)"
    fi
    "$ziran" bundle --root "$root" --entry app:main -o "$work/$input.zx" "$module"
    agree "zx $input" "$("$ziran" run "$work/$input.zx")"
done
case "$expected" in *" 44 7 10 -5 19") ;; *) echo "unexpected lanes: $expected" >&2; exit 1 ;; esac

reject() {
    printf 'Simd :: #import "simd"\n%s\nmain :: () {}\n' "$1" > "$work/bad.zi"
    if "$ziran" ir --root "$work" -o "$work/bad-ir" "$work/bad.zi" 2> "$work/bad.err"; then
        echo "accepted: $1" >&2
        exit 1
    fi
    grep -q "$2" "$work/bad.err" || { cat "$work/bad.err" >&2; exit 1; }
}
reject 'Wide :: (v: u8x32) -> u64 #target { return 0 }' 'cannot take a 256-bit vector by value'
reject 'Wide :: () -> u8x32 #target { return Simd.Zero(u8x32) }' 'cannot return a 256-bit vector by value'
reject 'Pair :: struct { v: s16x16 }
Wide :: (p: Pair) #target {}' 'cannot take a 256-bit vector by value'
reject 'Take :: (v: u8x32) -> u64 { return 0 }
Wide :: () -> u64 #target { return Take(Simd.Zero(u8x32)) }' 'cannot pass a 256-bit vector by value'
reject 'Twice :: (v: s64) -> s64 #inline #target { return v * 2 }' 'cannot be #inline'
reject 'Twice :: (v: s64) -> s64 #target #target { return v * 2 }' 'duplicate #target'
reject 'Twice :: (v: s64) -> s64 #target avx2 { return v * 2 }' '#target does not take arguments'
reject 'libc :: #system_library "libc";
labs :: (v: s64) -> s64 #target #foreign libc;' '#target needs a procedure body'
echo "simd_target: every tier, Go and bundles agree ($expected)"
