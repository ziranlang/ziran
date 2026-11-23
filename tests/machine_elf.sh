#!/bin/sh
# check-cost: 4
set -eu
ziran=${1:?pass the ziran command}
zi2zx=${2:-$(dirname -- "$ziran")/zi2zx}
zx2native=${3:-$(dirname -- "$ziran")/zx2native}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/machine-elf-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0
# No registered host library defines a capability unless a case registers one.
mkdir "$work/no-hosts"
export ZIRAN_HOST_PATH="$work/no-hosts"

"$ziran" run --target=c --root "$repo/tests" --module-path "$repo/cmd" \
    "$repo/tests/machine_elf_cases.zi"
"$ziran" run --target=c --root "$repo/tests" --module-path "$repo/cmd" \
    "$repo/tests/native_services_cases.zi"

cat > "$work/app.zi" <<'ZI'
state: s64;

Factorial :: (value: s64) -> s64 {
    if value <= 1 { return 1 }
    return value * Factorial(value - 1)
}

Step :: (value: s64) -> s64 {
    if value < 5 { return value + 1 }
    return value - 1
}

#program_export
Answer :: () -> s32 {
    state = 0
    current: s64 = 0
    while current < 6 {
        state += Step(current)
        current += 1
    }
    if state != 19 { return 1 }
    if Factorial(5) != 120 { return 2 }
    return 42
}
ZI

"$zi2zx" --root "$work" --entry app:Answer -o "$work/app.zx" "$work/app.zi"

run_status() {
    executable=$1
    expected=$2
    directory=${3:-.}
    set +e
    (cd "$directory" && "$executable")
    actual=$?
    set -e
    if test "$actual" -ne "$expected"; then
        echo "machine ELF exited $actual, expected $expected" >&2
        exit 1
    fi
}

compile_native() {
    output=$1
    root=$2
    entry=$3
    input=$4
    wire=$output.zx
    "$zi2zx" --root "$root" --entry "$entry" -o "$wire" "$input"
    "$zx2native" -o "$output" "$wire"
}

# This is the exact VS Code Native inspection pipeline: checked IR, verified
# ZX, then optimized map/ELF from the same image identity.
"$ziran" ir --root "$repo/examples" --entry hello:main \
    -o "$work/hello-checked" "$repo/examples/hello.zi"
"$zi2zx" --root "$work/hello-checked" --entry hello:main \
    -o "$work/hello.zx" "$work/hello-checked/hello.zir"
"$zx2native" --emit map -o "$work/hello.map" "$work/hello.zx"
"$zx2native" -o "$work/hello.elf" "$work/hello.zx"
# The map's per-instruction ranges are the compiler's own count of what main
# emits: its constant zero element index must fold into the base, leaving
# exactly four instructions (address, descriptor, count, tail call).
main_instructions=$(awk '{
    while (match($0, /\{"begin":[0-9]+,"end":[0-9]+[^{}]*"function":"main"\}/)) {
        range = substr($0, RSTART, RLENGTH)
        after = RSTART + RLENGTH
        begin = 0; end = 0
        if (match(range, /"begin":[0-9]+/)) begin = substr(range, RSTART + 8, RLENGTH - 8) + 0
        if (match(range, /"end":[0-9]+/)) end = substr(range, RSTART + 6, RLENGTH - 6) + 0
        if (begin < end) count++
        $0 = substr($0, after)
    }
}
END { print count + 0 }' "$work/hello.map")
if test "$main_instructions" -ne 4; then
    echo "native hello main kept its constant zero element index: $main_instructions instructions" >&2
    exit 1
fi
readelf -hW "$work/hello.elf" > "$work/hello.header"
grep -Fq 'Number of section headers:         0' "$work/hello.header"
# Hello carries only what it reaches: one direct write, no output buffer,
# terminal probe, formatter, fuel or stack check, and no page padding.
hello_size=$(wc -c < "$work/hello.elf")
if test "$hello_size" -gt 320 || grep -Fq truefalse "$work/hello.elf"; then
    echo "native hello grew to $hello_size bytes or carries unused runtime" >&2
    exit 1
fi
set +e
"$work/hello.elf" > "$work/hello.out"
hello_status=$?
set -e
test "$hello_status" -eq 0
printf 'Hello, World!\n' | cmp - "$work/hello.out"
file "$work/hello.elf" > "$work/hello.file"
grep -Fq 'statically linked' "$work/hello.file"
readelf -lW "$work/hello.elf" > "$work/hello.program-headers"
if grep -Eq '(^|[[:space:]])(INTERP|DYNAMIC)([[:space:]]|$)' "$work/hello.program-headers"; then
    echo "native hello unexpectedly depends on an interpreter or dynamic loader" >&2
    exit 1
fi

# Both ARM rows write a static ELF of their own: EM_AARCH64 (183) as ELF64 and
# EM_ARM (40) as ELF32 EABI5. The program carries no platform service, so
# nothing links against a foreign object.
"$zx2native" --target aarch64 --platform linux -o "$work/aarch64-linux.elf" "$work/app.zx"
"$zx2native" --target armv7 --platform android -o "$work/armv7-android.elf" "$work/app.zx"
python3 - "$work/aarch64-linux.elf" "$work/armv7-android.elf" <<'PYCHECK'
import struct, sys
a64 = open(sys.argv[1], 'rb').read()
assert a64[:4] == b'\x7fELF' and a64[4] == 2 and a64[5] == 1, 'AArch64 image is not ELF64 little-endian'
assert struct.unpack_from('<H', a64, 18)[0] == 183, 'AArch64 image has the wrong machine'
assert struct.unpack_from('<H', a64, 16)[0] == 2, 'AArch64 image is not ET_EXEC'
v7 = open(sys.argv[2], 'rb').read()
assert v7[:4] == b'\x7fELF' and v7[4] == 1 and v7[5] == 1, 'ARMv7 image is not ELF32 little-endian'
assert struct.unpack_from('<H', v7, 18)[0] == 40, 'ARMv7 image has the wrong machine'
assert struct.unpack_from('<H', v7, 16)[0] == 2, 'ARMv7 image is not ET_EXEC'
PYCHECK

# A facility the ARM rows do not carry is a named gap, and the output stays
# untouched.
cat > "$work/format.zi" <<'ZI'
main :: () -> s32 {
    print("n=%\n", 42)
    return 0
}
ZI
"$zi2zx" --root "$work" --module-path "$repo/std" --entry format:main -o "$work/format.zx" "$work/format.zi"
row=armv7-format-gap
printf 'preserve-output\n' > "$work/$row.elf"
if ZIRAN_DIAGNOSTICS=json "$zx2native" --target armv7 --platform android \
    -o "$work/$row.elf" "$work/format.zx" > "$work/$row.log" 2>&1; then
    echo "ARM ELF accepted a formatter the row does not carry" >&2
    exit 1
fi
grep -Fq 'assembly.gap.machine-platform-service-unavailable' "$work/$row.log"
printf 'preserve-output\n' | cmp - "$work/$row.elf"

compile_native "$work/source-a" "$work" app:Answer "$work/app.zi"
compile_native "$work/source-b" "$work" app:Answer "$work/app.zi"
cmp "$work/source-a" "$work/source-b"
run_status "$work/source-a" 42

"$ziran" ir --root "$work" -o "$work/checked" "$work/app.zi"
compile_native "$work/checked-elf" "$work/checked" app:Answer "$work/checked/app.zir"
run_status "$work/checked-elf" 42

"$zi2zx" --root "$work" --entry app:Answer -o "$work/app.zx" "$work/app.zi"
"$zx2native" -o "$work/zx-elf" "$work/app.zx"
run_status "$work/zx-elf" 42
cmp "$work/source-a" "$work/checked-elf"
cmp "$work/source-a" "$work/zx-elf"

# A loop carries a fuel check. Exhaustion must take the in-image hard trap and
# return the fixed status rather than invoking or switching to another engine.
cat > "$work/fuel.zi" <<'ZI'
#program_export
Spin :: () -> s32 {
    value: s64 = 0
    while value >= 0 { value += 1 }
    return 0
}
ZI
# The budget is unbounded unless the executable is built with one.
"$zi2zx" --root "$work" --entry fuel:Spin -o "$work/fuel.zx" "$work/fuel.zi"
"$zx2native" --fuel 1000000 -o "$work/fuel-elf" "$work/fuel.zx"
run_status "$work/fuel-elf" 125
if "$zx2native" --emit map --fuel 1 -o "$work/fuel.map" "$work/fuel.zx" 2> /dev/null; then
    echo "zx2native accepted a fuel budget for a map" >&2
    exit 1
fi

# More iterations than any fixed budget would allow run to completion.
cat > "$work/long.zi" <<'ZI'
#program_export
Answer :: () -> s32 {
    value: s64 = 0
    while value < 20000000 { value += 1 }
    return cast(s32)(value / 1000000)
}
ZI
compile_native "$work/long-elf" "$work" long:Answer "$work/long.zi"
run_status "$work/long-elf" 20

# Files and clock platform services come from code the image carries itself:
# a program that uses every service links no host library, stays a static
# executable with no interpreter or dynamic section, and answers 42 from a
# directory it makes, fills, lists, renames and empties under its working
# directory, a name outside ASCII and a 16 KiB read included, leaving nothing
# behind.
cat > "$work/files.zi" <<'ZI'
Files :: #import "std/guest_files";
Clock :: #import "std/guest_clock";

Big :: () -> string {
    out: Vec(u8)
    for block: 0..255 { BuilderAppend(out, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789++") }
    return BuilderFinish(out)
}

// Whether LISTING, one name per line, carries NAME.
Has :: (listing: string, name: string) -> bool {
    start: s64 = 0
    while start < listing.count {
        end := start
        while end < listing.count && listing[end] != cast(u8)10 { end += 1 }
        if listing[start:end] == name { return true }
        start = end + 1
    }
    return false
}

#program_export
main :: () -> s32 {
    base := "files-case"
    if !Files.DirectoryMake(base) { return 1 }
    if Files.FileStatus(base) != -2 { return 2 }
    if !Files.DirectoryMake(base) { return 3 }
    if !Files.FileWrite("files-case/a.txt", "alpha") { return 4 }
    if Files.FileStatus("files-case/a.txt") != 5 { return 5 }
    big := Big()
    if big.count != 16384 { return 100 }
    if !Files.FileWrite("files-case/big.bin", big) { return 6 }
    if Files.FileStatus("files-case/big.bin") != 16384 { return 7 }
    data, ok := Files.FileRead("files-case/big.bin")
    if !ok || data != big { return 8 }
    if Files.FileStatus("files-case/missing") != -1 { return 9 }
    if !Files.FileWrite("files-case/Paraguái.txt", "í") { return 10 }
    listing := Files.DirectoryList(base)
    if !Has(listing, "a.txt") || !Has(listing, "big.bin") || !Has(listing, "Paraguái.txt") ||
        listing.count != "a.txt\nbig.bin\nParaguái.txt".count { return 11 }
    if !Files.FileRename("files-case/a.txt", "files-case/b.txt") { return 12 }
    if Files.FileStatus("files-case/a.txt") != -1 || Files.FileStatus("files-case/b.txt") != 5 { return 13 }
    if !Files.FileRemove("files-case/b.txt") { return 14 }
    if Files.FileStatus("files-case/b.txt") != -1 { return 15 }
    if Files.FileRemove("files-case/b.txt") { return 16 }
    if Files.FileRename("files-case/missing", "files-case/x") { return 17 }
    if Files.FileWrite("files-case/sub/deep.txt", "x") { return 18 }
    if Files.FileStatus("files-case") != -2 { return 19 }
    again, ok2 := Files.FileRead("files-case/big.bin")
    if !ok2 || again != big { return 20 }
    if Files.FileStatus("") != -1 { return 21 }
    if !Files.FileWrite("files-case/empty", "") { return 22 }
    empty, ok3 := Files.FileRead("files-case/empty")
    if !ok3 || empty != "" { return 23 }
    start := Clock.MonotonicNanoseconds()
    if start < 0 { return 24 }
    wall := Clock.WallMilliseconds()
    if Clock.MonotonicNanoseconds() < start { return 25 }
    if wall < 1700000000000 || wall > 4102444800000 { return 26 }
    if !Files.FileRemove("files-case/empty") || !Files.FileRemove("files-case/big.bin") ||
        !Files.FileRemove("files-case/Paraguái.txt") { return 27 }
    if Files.DirectoryList("files-case") != "" { return 28 }
    if Files.DirectoryList("files-case/missing") != "" { return 29 }
    whole, ok4 := Files.FileRead("files-case")
    if ok4 || whole != "" { return 30 }
    if Clock.MonotonicNanoseconds() < start { return 31 }
    return 42
}
ZI
"$zi2zx" --root "$work" --module-path "$repo/std" --entry files:main -o "$work/files.zx" "$work/files.zi"
"$zx2native" -o "$work/files-elf" "$work/files.zx"
file "$work/files-elf" > "$work/files.file"
grep -Fq 'statically linked' "$work/files.file"
readelf -lW "$work/files-elf" > "$work/files.program-headers"
if grep -Eq '(^|[[:space:]])(INTERP|DYNAMIC)([[:space:]]|$)' "$work/files.program-headers"; then
    echo "native files program unexpectedly depends on an interpreter or dynamic loader" >&2
    exit 1
fi
mkdir "$work/files-run"
run_status "$work/files-elf" 42 "$work/files-run"
if test -n "$(ls -A "$work/files-run/files-case")"; then
    echo "native files program left files behind" >&2
    exit 1
fi

# A record built from a field of another record aliases that field through a
# chain of folded values: no use may name an eliminated virtual.
cat > "$work/alias.zi" <<'ZI'
#import "std/math"
Triple :: struct { x: float64; y: float64; z: float64 }
State :: struct { height: float64; saturation: float64 }
SampleAt :: (hour: float64) -> State {
    out: State
    out.height = hour
    sun: Triple = .{hour, out.height, hour}
    length := Sqrt(sun.x * sun.x + sun.y * sun.y + sun.z * sun.z)
    return out
}
#program_export
Answer :: () -> s32 {
    v := SampleAt(3.0)
    if v.height != 3.0 || v.saturation != 0.0 { return 1 }
    return 0
}
ZI
compile_native "$work/alias-elf" "$work" alias:Answer "$work/alias.zi"
run_status "$work/alias-elf" 0

# Members built where their parent keeps them: nested literals, one replaced
# through a constant and a dynamic index, a call result as a member, a record
# rebuilt from members of itself, and a loop-carried record. A member that
# reads its parent while it is built must keep its own storage.
cat > "$work/nested.zi" <<'ZI'
Layer :: struct { blur: float32; offset_y: float32; spread: float32; opacity: float32 }
Tokens :: struct { rest: Layer; raised: [2]Layer; floating: [2]Layer }
Pair :: struct { a: s32; b: s32 }
Box :: struct { first: Pair; second: Pair; third: Pair }

MakePair :: (x: s32, y: s32) -> Pair { return Pair.{x, y} }

BuildTokens :: (dark: bool, slot: s32) -> Tokens {
    t: Tokens
    if dark {
        t.rest = Layer.{1.0, 2.0, 0.0, 0.5}
        t.raised[0] = Layer.{8.0, 22.0, 0.0, 0.25}
        t.floating[1] = Layer.{1.0, 3.0, -4.0, 0.125}
    } else {
        t.rest = Layer.{6.0, 18.0, 0.0, 0.0625}
        t.raised[1] = Layer.{1.0, 3.0, 0.0, 0.5}
        t.floating[0] = Layer.{12.0, 32.0, -4.0, 0.75}
    }
    t.floating[slot] = Layer.{9.0, 9.0, 9.0, 9.0}
    return t
}

Same :: (a: Layer, blur: float32, offset_y: float32, spread: float32, opacity: float32) -> bool {
    return a.blur == blur && a.offset_y == offset_y && a.spread == spread && a.opacity == opacity
}

Is :: (p: Pair, a: s32, b: s32) -> bool { return p.a == a && p.b == b }

Take :: (p: Pair) -> s32 { return p.a * 10 + p.b }

Rotate :: (steps: s32) -> Box {
    box := Box.{Pair.{1, 2}, Pair.{3, 4}, Pair.{5, 6}}
    for step: 0..steps - 1 {
        box = Box.{box.second, box.third, Pair.{box.first.a + 10, box.first.b + 10}}
    }
    return box
}

#program_export
Answer :: () -> s32 {
    dark := BuildTokens(true, 0)
    if !Same(dark.rest, 1.0, 2.0, 0.0, 0.5) { return 1 }
    if !Same(dark.raised[0], 8.0, 22.0, 0.0, 0.25) { return 2 }
    if !Same(dark.raised[1], 0.0, 0.0, 0.0, 0.0) { return 3 }
    if !Same(dark.floating[0], 9.0, 9.0, 9.0, 9.0) { return 4 }
    if !Same(dark.floating[1], 1.0, 3.0, -4.0, 0.125) { return 5 }
    light := BuildTokens(false, 1)
    if !Same(light.rest, 6.0, 18.0, 0.0, 0.0625) { return 6 }
    if !Same(light.raised[0], 0.0, 0.0, 0.0, 0.0) { return 7 }
    if !Same(light.raised[1], 1.0, 3.0, 0.0, 0.5) { return 8 }
    if !Same(light.floating[0], 12.0, 32.0, -4.0, 0.75) { return 9 }
    if !Same(light.floating[1], 9.0, 9.0, 9.0, 9.0) { return 10 }
    box := Box.{Pair.{1, 2}, MakePair(3, 4), Pair.{5, 6}}
    if !Is(box.first, 1, 2) || !Is(box.second, 3, 4) || !Is(box.third, 5, 6) { return 11 }
    rotated := Rotate(3)
    if !Is(rotated.first, 11, 12) || !Is(rotated.second, 13, 14) || !Is(rotated.third, 15, 16) { return 12 }
    swapped := Box.{box.third, box.first, box.second}
    if !Is(swapped.first, 5, 6) || !Is(swapped.second, 1, 2) || !Is(swapped.third, 3, 4) { return 13 }
    box = Box.{box.third, box.first, box.second}
    if !Is(box.first, 5, 6) || !Is(box.second, 1, 2) || !Is(box.third, 3, 4) { return 14 }
    box.second = Pair.{box.first.a + 100, box.third.b}
    if !Is(box.first, 5, 6) || !Is(box.second, 105, 4) || !Is(box.third, 3, 4) { return 15 }
    shared := Pair.{7, 8}
    both := Box.{shared, shared, Pair.{9, 10}}
    if !Is(both.first, 7, 8) || !Is(both.second, 7, 8) || !Is(both.third, 9, 10) { return 16 }
    both.first = Pair.{1, 1}
    if !Is(shared, 7, 8) || !Is(both.second, 7, 8) { return 17 }
    escaped := Pair.{20, 21}
    both.third = escaped
    if Take(escaped) != 221 || !Is(both.third, 20, 21) { return 18 }
    both.third.a = 99
    if !Is(escaped, 20, 21) || !Is(both.third, 99, 21) { return 19 }
    pairs: [3]Pair
    for index: 0..2 { pairs[index] = Pair.{cast(s32)index, cast(s32)index * 2} }
    if !Is(pairs[0], 0, 0) || !Is(pairs[1], 1, 2) || !Is(pairs[2], 2, 4) { return 20 }
    source := Pair.{30, 31}
    box.first = source
    source.a = 99
    if !Is(box.first, 30, 31) { return 21 }
    return 0
}
ZI
compile_native "$work/nested-elf" "$work" nested:Answer "$work/nested.zi"
run_status "$work/nested-elf" 0

# A record a function returns is built where the caller keeps its result when
# every path returns the same one: through a loop, with an unaligned member,
# as a call result inside another record, across recursion and with many
# values live. Two records on two paths, a parameter and a parameter with a
# member changed are copied instead. Every value is distinct so a stale or
# shared copy shows.
cat > "$work/result.zi" <<'ZI'
Pair :: struct { a: s32; b: s32 }
Trio :: struct { a: s64; b: s64; c: s64 }
Wide :: struct { head: s64; tail: [64]s32; last: s64 }
Mixed :: struct { x: float64; flag: u8; y: float64 }
Frame :: struct { before: u8; inner: Mixed; after: u8 }
Odd :: struct { a: u8; b: [3]u16; c: float64; d: u8 }
Holder :: struct { tag: u8; odd: Odd; end: u8 }
Box :: struct { first: Trio; second: Trio; third: Trio }

MakeWide :: (seed: s32) -> Wide {
    w: Wide
    w.head = cast(s64)seed
    for index: 0..63 { w.tail[index] = seed + cast(s32)index * 3 }
    w.last = cast(s64)seed * 2
    return w
}

MakeMixed :: (v: float64) -> Mixed {
    m: Mixed
    m.x = v
    m.flag = 7
    m.y = v * 2.0
    return m
}

MakeFrame :: (v: float64) -> Frame {
    return Frame.{1, MakeMixed(v), 3}
}

MakeOdd :: (v: u16) -> Odd {
    o: Odd
    o.a = 1
    for index: 0..2 { o.b[index] = v + cast(u16)index }
    o.c = cast(float64)v * 0.5
    o.d = 4
    return o
}

MakeHolder :: (v: u16) -> Holder { return Holder.{9, MakeOdd(v), 11} }

// Eight bytes: too small for the result register to pay, copied as before.
Small :: (a: s32) -> Pair { return Pair.{a, a + 1} }

Choose :: (flag: bool) -> Trio {
    first := Trio.{1, 2, 3}
    second := Trio.{4, 5, 6}
    if flag { return first }
    return second
}

Echo :: (p: Trio) -> Trio { return p }

Bump :: (p: Trio) -> Trio {
    p.a += 1
    return p
}

Swap :: (p: Trio) -> Trio { return Trio.{p.c, p.b, p.a} }

Chain :: (n: s64) -> Trio {
    r := Trio.{0, 0, 0}
    if n > 0 {
        r = Chain(n - 1)
        r.a += n
        r.b += 2
        r.c += r.a * 10
    }
    return r
}

Take :: (p: Trio) -> s64 { return p.a * 100 + p.b * 10 + p.c }

Mix :: (n: s64) -> Trio {
    r := Trio.{n, n + 1, n + 2}
    taken := Take(r)
    r.b = taken
    return r
}

Pressure :: (seed: s64) -> Trio {
    a := seed + 1; b := seed + 2; c := seed + 3; d := seed + 4; e := seed + 5
    f := seed + 6; g := seed + 7; h := seed + 8; i := seed + 9; j := seed + 10
    k := seed + 11; l := seed + 12; m := seed + 13; n := seed + 14
    middle := Take(Trio.{a, b, c})
    return Trio.{a + b + c + d + e + f + g, h + i + j + k + l + m + n, middle}
}

#program_export
Answer :: () -> s32 {
    w := MakeWide(5)
    if w.head != 5 || w.last != 10 { return 1 }
    for index: 0..63 { if w.tail[index] != 5 + cast(s32)index * 3 { return 2 } }
    other := MakeWide(1000)
    if w.tail[63] != 5 + 63 * 3 || other.tail[0] != 1000 || other.last != 2000 { return 3 }
    m := MakeMixed(1.5)
    if m.x != 1.5 || m.flag != 7 || m.y != 3.0 { return 4 }
    f := MakeFrame(2.5)
    if f.before != 1 || f.inner.x != 2.5 || f.inner.flag != 7 || f.inner.y != 5.0 || f.after != 3 { return 5 }
    h := MakeHolder(100)
    if h.tag != 9 || h.odd.a != 1 || h.odd.b[0] != 100 || h.odd.b[1] != 101 || h.odd.b[2] != 102 ||
        h.odd.c != 50.0 || h.odd.d != 4 || h.end != 11 { return 15 }
    small := Small(40)
    if small.a != 40 || small.b != 41 { return 16 }
    c1 := Choose(true)
    c2 := Choose(false)
    if c1.a != 1 || c1.b != 2 || c1.c != 3 || c2.a != 4 || c2.b != 5 || c2.c != 6 { return 6 }
    e := Echo(Trio.{8, 9, 10})
    if e.a != 8 || e.b != 9 || e.c != 10 { return 7 }
    x := Trio.{20, 21, 22}
    x = Bump(x)
    if x.a != 21 || x.b != 21 || x.c != 22 { return 8 }
    x = Swap(x)
    x = Swap(Trio.{x.a + 1, x.b + 2, x.c + 3})
    if x.a != 24 || x.b != 23 || x.c != 23 { return 9 }
    chain := Chain(4)
    if chain.a != 10 || chain.b != 8 || chain.c != 200 { return 10 }
    mixed := Mix(6)
    if mixed.a != 6 || mixed.b != 6 * 100 + 7 * 10 + 8 || mixed.c != 8 { return 11 }
    pressure := Pressure(10)
    if pressure.a != 10 * 7 + 28 || pressure.b != 10 * 7 + 77 || pressure.c != 11 * 100 + 12 * 10 + 13 { return 12 }
    box := Box.{Chain(1), Chain(2), Bump(Trio.{50, 51, 52})}
    if box.first.a != 1 || box.first.b != 2 || box.first.c != 10 || box.second.a != 3 || box.second.b != 4 ||
        box.third.a != 51 || box.third.b != 51 || box.third.c != 52 { return 13 }
    box.second = Chain(3)
    if box.first.a != 1 || box.second.a != 6 || box.second.b != 6 || box.third.a != 51 { return 14 }
    return 0
}
ZI
compile_native "$work/result-elf" "$work" result:Answer "$work/result.zi"
run_status "$work/result-elf" 0

# A debug attachment gives map ranges source lines without changing a byte
# of the executable.
"$zi2zx" --debug --root "$work" --entry app:Answer -o "$work/app-debug.zx" "$work/app.zi"
"$zx2native" -o "$work/debug-elf" "$work/app-debug.zx"
"$zx2native" -o "$work/plain-elf" "$work/app.zx"
cmp "$work/plain-elf" "$work/debug-elf"
"$zx2native" --emit map -o "$work/debug.map" "$work/app-debug.zx"
grep -Fq '"file":"app.zi","line":' "$work/debug.map"
"$zx2native" --emit map -o "$work/plain.map" "$work/app.zx"
if grep -Fq '"file":"app.zi","line":' "$work/plain.map"; then
    echo "a map without a debug attachment named source lines" >&2
    exit 1
fi

reject_gap() {
    expected=$1
    source=$2
    printf 'preserve-output\n' > "$work/preserved"
    cp "$work/preserved" "$work/output"
    "$zi2zx" --root "$work" --entry "$source:Answer" -o "$work/$source.zx" "$work/$source.zi"
    if ZIRAN_DIAGNOSTICS=json "$zx2native" \
        -o "$work/output" "$work/$source.zx" > "$work/$source.log" 2>&1; then
        echo "machine ELF accepted unsupported $source input" >&2
        exit 1
    fi
    if ! grep -Fq "$expected" "$work/$source.log"; then
        echo "machine ELF reported the wrong gap for $source; expected $expected:" >&2
        cat "$work/$source.log" >&2
        exit 1
    fi
    cmp "$work/preserved" "$work/output"
}

cat > "$work/string_gap.zi" <<'ZI'
#program_export
Answer :: () -> s32 { message := "machine gap"; return cast(s32)message.count }
ZI
compile_native "$work/string" "$work" string_gap:Answer "$work/string_gap.zi"
run_status "$work/string" 11

cat > "$work/global_init_gap.zi" <<'ZI'
state: s64 = 7;
#program_export
Answer :: () -> s32 { return cast(s32)state }
ZI
compile_native "$work/global-init" "$work" global_init_gap:Answer "$work/global_init_gap.zi"
run_status "$work/global-init" 7

cat > "$work/slice_gap.zi" <<'ZI'
First :: (values: []s64) -> s64 { return values[0] }
#program_export
Answer :: () -> s32 { values: [1]s64 = .[42]; return cast(s32)First(values[:]) }
ZI
compile_native "$work/slice" "$work" slice_gap:Answer "$work/slice_gap.zi"
run_status "$work/slice" 42

cat > "$work/host_gap.zi" <<'ZI'
host_api :: #system_library "host_api";
HostValue :: () -> s32 #foreign host_api;
#program_export
Answer :: () -> s32 { return HostValue() }
ZI
reject_gap 'assembly.gap.machine-host' host_gap

cat > "$work/print_gap.zi" <<'ZI'
#program_export
Answer :: () -> s32 { print("Hello, World!\n"); return 0 }
ZI
compile_native "$work/print" "$work" print_gap:Answer "$work/print_gap.zi"
"$work/print" > "$work/print.out"
printf 'Hello, World!\n' | cmp - "$work/print.out"

cat > "$work/float_print_gap.zi" <<'ZI'
#program_export
Answer :: () -> s32 { print("%\n", 0.1); return 0 }
ZI
compile_native "$work/float-print" "$work" float_print_gap:Answer "$work/float_print_gap.zi"
"$work/float-print" > "$work/float-print.out"
printf '0.1\n' | cmp - "$work/float-print.out"

# Growth moves a vector through recycled heap blocks. Built with --checks, an
# element pointer taken before the growth traps even after another vector
# reuses its block.
cat > "$work/stale.zi" <<'ZI'
#import "vec"
#program_export
Answer :: () -> s32 {
    values: Vec(s64)
    VecPush(values, 1)
    first := *values[0]
    for i: 0..99 { VecPush(values, cast(s64)i) }
    other: Vec(s64)
    VecPush(other, 5)
    return cast(s32)<<first
}
ZI
"$zi2zx" --checks --root "$work" --module-path "$repo/std" --entry stale:Answer -o "$work/stale.zx" "$work/stale.zi"
"$zx2native" -o "$work/stale-elf" "$work/stale.zx"
run_status "$work/stale-elf" 125

# The native path also rejects an arbitrary host capability with its
# stable platform-service diagnostic and leaves an existing output untouched.
cat > "$work/native_host_gap.zi" <<'ZI'
host_api :: #system_library "host_api";
Input :: (key: s64) -> s64 #foreign host_api;
#program_export
Answer :: () -> s32 { return cast(s32)Input(1) }
ZI
"$ziran" ir --root "$work" -o "$work/native-host-ir" "$work/native_host_gap.zi"
"$zi2zx" --root "$work/native-host-ir" --entry native_host_gap:Answer \
    -o "$work/native-host.zx" "$work/native-host-ir/native_host_gap.zir"
printf 'preserve-output\n' > "$work/native-host.elf"
if ZIRAN_DIAGNOSTICS=json "$zx2native" \
    -o "$work/native-host.elf" "$work/native-host.zx" > "$work/native-host.log" 2>&1; then
    echo "native ELF accepted an unavailable platform service" >&2
    exit 1
fi
grep -Fq 'assembly.gap.machine-host' "$work/native-host.log"
printf 'preserve-output\n' | cmp - "$work/native-host.elf"

# A host capability links by name from a registered host library, called with
# the C ABI the C target declares for it, so a host built by the C target
# serves the executable: strings, integers of every width, bools and floats
# cross in parameter order and come back widened as the program keeps them.
cat > "$work/host_provider.zi" <<'ZI'
#program_export
Measure :: (label: string, scale: s32, flag: bool, low: u8) -> s64 {
    return label.count * scale + ifx flag then cast(s64)low else 0
}
#program_export
Blend :: (left: float64, count: s8, right: float64, last: string) -> float64 {
    return left * right + cast(float64)count + cast(float64)last.count
}
#program_export
Narrow :: (value: s64) -> s16 { return cast(s16)value }
#program_export
Even :: (value: u32) -> bool { return value % 2 == 0 }
#program_export
Low :: (value: s64) -> u32 { return cast(u32)value }
ZI
"$ziran" build --target=c --root "$work" -o "$work/host-provider" "$work/host_provider.zi"
cc -std=c99 -O2 -fPIC -shared -I"$repo/include" "$work"/host-provider/*.c -o "$work/libhost-provider.so"
mkdir "$work/hosts"
printf '%s\n' "$work/libhost-provider.so" > "$work/hosts/test.library"
cat > "$work/host_link.zi" <<'ZI'
host_api :: #system_library "host_api";
Measure :: (label: string, scale: s32, flag: bool, low: u8) -> s64 #reads #foreign host_api;
Blend :: (left: float64, count: s8, right: float64, last: string) -> float64 #reads #foreign host_api;
Narrow :: (value: s64) -> s16 #foreign host_api;
Even :: (value: u32) -> bool #foreign host_api;
Low :: (value: s64) -> u32 #foreign host_api;
#program_export
Answer :: () -> s32 {
    if Measure("Window", 3, true, 200) != 218 || Measure("", 9, false, 7) != 0 { return 1 }
    if Blend(1.5, -2, 2.5, "four") != 5.75 { return 2 }
    if Narrow(65533) != -3 { return 3 }
    if !Even(4) || Even(5) { return 4 }
    if Low(0x1ffffffff) != 0xffffffff { return 5 }
    return 42
}
ZI
"$zi2zx" --root "$work" --entry host_link:Answer -o "$work/host-link.zx" "$work/host_link.zi"
ZIRAN_HOST_PATH="$work/hosts" "$zx2native" -o "$work/host-link" "$work/host-link.zx"
readelf -lW "$work/host-link" > "$work/host-link.program-headers"
grep -Fq 'Requesting program interpreter: /lib64/ld-linux-x86-64.so.2' "$work/host-link.program-headers"
readelf -dW "$work/host-link" > "$work/host-link.dynamic"
grep -Fq "Shared library: [$work/libhost-provider.so]" "$work/host-link.dynamic"
run_status "$work/host-link" 42
# The map describes the same linked file.
ZIRAN_HOST_PATH="$work/hosts" "$zx2native" --emit map -o "$work/host-link.map" "$work/host-link.zx"
grep -Fq '"scope":"complete-elf"' "$work/host-link.map"

# A capability no registered library defines is a gap naming it, and a
# signature the C ABI veneer cannot carry is another.
printf 'preserve-output\n' > "$work/host-unlinked"
if "$zx2native" -o "$work/host-unlinked" "$work/host-link.zx" > "$work/host-unlinked.log" 2>&1; then
    echo "native ELF linked a host capability no registered library defines" >&2
    exit 1
fi
grep -Fq 'assembly.gap.machine-host: machine ELF does not implement host capability Measure: no registered host library defines it' \
    "$work/host-unlinked.log"
printf 'preserve-output\n' | cmp - "$work/host-unlinked"
cat > "$work/host_record.zi" <<'ZI'
host_api :: #system_library "host_api";
Even :: (value: u32) -> string #foreign host_api;
#program_export
Answer :: () -> s32 { return cast(s32)Even(2).count }
ZI
"$zi2zx" --root "$work" --entry host_record:Answer -o "$work/host-record.zx" "$work/host_record.zi"
if ZIRAN_HOST_PATH="$work/hosts" "$zx2native" -o "$work/host-record" "$work/host-record.zx" \
    > "$work/host-record.log" 2>&1; then
    echo "native ELF linked a host capability returning a record" >&2
    exit 1
fi
grep -Fq "assembly.gap.machine-host-abi: machine ELF cannot pass host capability Even's parameters or result" \
    "$work/host-record.log"
test ! -e "$work/host-record"

# After all fixtures are prepared, poison every external compiler, assembler
# and alternate language engine name. The sole native command must still
# produce an executable and the poison log must remain empty.
mkdir "$work/no-tools"
: > "$work/invoked"
for tool in cc c++ gcc g++ clang clang++ as ld go; do
    cat > "$work/no-tools/$tool" <<SCRIPT
#!/bin/sh
printf '%s\n' '$tool' >> '$work/invoked'
exit 97
SCRIPT
    chmod 755 "$work/no-tools/$tool"
done
PATH="$work/no-tools:$PATH" "$zx2native" -o "$work/no-tools-elf" "$work/app.zx"
test ! -s "$work/invoked"
run_status "$work/no-tools-elf" 42
