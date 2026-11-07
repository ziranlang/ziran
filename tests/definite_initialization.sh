#!/bin/sh
# Ziran fills a local declared with no value with zeros. Generated C leaves
# the fill out only when every element is provably stored before anything
# can read it: SIMD stores of whole rows and element stores in counted loops
# with constant bounds, field by field writes, and passes over a branch that
# names nothing of the local. A fill stays when a store may be missing or
# late: a loop that stops early or skips an element, a break or store under a condition not
# known here, a read or view of the local first, an index not known or one
# that wraps, a labeled break out of the block, fields of mixed size that
# leave padding, and a field read through using. Every program prints the
# same on C, Go, native and the portable VM, from source and saved IR.
set -eu

ziran=${1:?pass the ziran command}
run_vm="$(dirname "$ziran")/zi2zx"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Simd :: #import "std/simd"

Block :: struct { samples: [64]u16; }
Point :: struct { x: s32; y: s32; z: s32; }
Mixed :: struct { tag: u8; value: s32; }

Sum16 :: (values: []u16) -> s64 { t: s64 = 0; for v: values { t += cast(s64) v }; return t }
Sum32 :: (values: []s32) -> s64 { t: s64 = 0; for v: values { t += cast(s64) v }; return t }

// Proven: eight rows of eight lanes fill the block.
Rows :: (source: []u16, base: s32, stride: s32) -> s64 {
    block: Block
    for i: 0..7 { Simd.Store(block.samples[:], i * 8, Simd.Load(u16x8, source, base + i * stride)) }
    return Sum16(block.samples[:])
}
// Proven: nested loops store every element.
Grid :: (source: []u16, base: s32, stride: s32) -> s64 {
    grid: [8][8]u16
    for i: 0..7 { for j: 0..7 { grid[i][j] = source[base + i * stride + j] } }
    total: s64 = 0
    for i: 0..7 { total += Sum16(grid[i][:]) }
    return total
}
// Proven: two loops each store half, after a statement that names nothing
// of the array, and a condition known true.
Halves :: (seed: s32) -> s64 {
    values: [8]s32
    n := seed * 3
    for i: 0..3 { values[i] = n + cast(s32) i }
    for i: 4..7 { if i < 8 { values[i] = n - cast(s32) i } }
    return Sum32(values[:])
}
// Proven: every field is written.
Fields :: (a: s32, b: s32) -> s64 {
    p: Point
    p.z = a * b
    p.x = a
    p.y = b
    return cast(s64) (p.x + p.y * 10 + p.z * 100)
}
// Proven: the branch names nothing of the array and cannot leave the loop.
Skip :: (source: []u16, flag: bool) -> s64 {
    values: [4]s32
    extra: s32 = 0
    for i: 0..3 {
        if flag { extra += 1 }
        values[i] = cast(s32) source[i] + extra
    }
    return Sum32(values[:]) + cast(s64) extra
}

// Kept: element 7 is never stored, and the sum reads its zero.
Partial :: (seed: s32) -> s64 {
    values: [8]s32
    for i: 0..6 { values[i] = seed }
    return Sum32(values[:])
}
// Kept: the loop leaves at a known point before the last stores.
Early :: (seed: s32) -> s64 {
    values: [8]s32
    for i: 0..7 {
        if i == 5 { break }
        values[i] = seed
    }
    return Sum32(values[:])
}
// Kept: a continue passes over element 2.
Gap :: (seed: s32) -> s64 {
    values: [8]s32
    for i: 0..7 {
        if i == 2 { continue }
        values[i] = seed
    }
    return Sum32(values[:])
}
// Kept: whether the loop leaves early is not known here.
Leave :: (seed: s32, stop: s64) -> s64 {
    values: [8]s32
    for i: 0..7 {
        if i == stop { break }
        values[i] = seed
    }
    return Sum32(values[:])
}
// Kept: a store under a condition not known here.
Some :: (seed: s32, flag: bool) -> s64 {
    values: [4]s32
    for i: 0..3 { if flag { values[i] = seed } }
    return Sum32(values[:])
}
// Kept: values[3] is read, as zero, before it is stored.
Running :: (seed: s32) -> s64 {
    values: [4]s32
    values[0] = seed
    for i: 1..3 { values[i] = values[i - 1] + values[3] }
    return Sum32(values[:])
}
// Kept: a view of the array is read before it is whole.
Escape :: (seed: s32) -> s64 {
    values: [4]s32
    first := Sum32(values[:])
    for i: 0..3 { values[i] = seed }
    return first + Sum32(values[:])
}
// Kept: the index is not known here.
Unknown :: (seed: s32, at: s64) -> s64 {
    values: [4]s32
    for i: 0..3 { values[(i + at) % 4] = seed }
    return Sum32(values[:])
}
// Kept: the u8 index wraps, which is not followed, though it covers all.
Wrapped :: (seed: s32) -> s64 {
    values: [256]s32
    for i: 0..255 { values[cast(u8) (i * 257)] = seed }
    return Sum32(values[:])
}
// Kept: a labeled break leaves the block before the array is whole.
Outer :: (seed: s32) -> s64 {
    total: s64 = 0
    for round: 0..1 {
        values: [4]s32
        for i: 0..3 {
            if i == 2 { break round }
            values[i] = seed
        }
        total += Sum32(values[:])
    }
    return total
}
// Kept: a u8 and an s32 field leave padding between them.
Pad :: (a: s32) -> s64 {
    m: Mixed
    m.tag = 7
    m.value = a
    return cast(s64) m.value + cast(s64) m.tag
}
// Kept: z is read through using before it is written.
Promoted :: (seed: s32) -> s64 {
    using p: Point
    x = seed
    y = x + z
    z = 5
    return cast(s64) (x + y + z)
}
// Kept: the first row is never stored.
Shifted :: (source: []u16, base: s32, stride: s32) -> s64 {
    block: Block
    for i: 1..7 { Simd.Store(block.samples[:], i * 8, Simd.Load(u16x8, source, base + i * stride)) }
    return Sum16(block.samples[:])
}

main :: () {
    data: [128]u16
    for i: 0..127 { data[i] = cast(u16) i }
    source := data[:]
    print("% % % % % %\n", Rows(source, 0, 8), Grid(source, 0, 8), Halves(2), Fields(3, 4),
          Skip(source, true), Skip(source, false))
    print("% % % % % % %\n", Partial(5), Early(5), Gap(5), Leave(5, 3), Some(5, false), Some(5, true),
          Running(5))
    print("% % % % % % %\n", Escape(5), Unknown(5, 1), Wrapped(1), Outer(5), Pad(9), Promoted(4),
          Shifted(source, 0, 8))
}
ZI

cat > "$work/expected" <<'OUT'
2016 2016 32 1243 20 6
35 25 35 15 0 20 20
20 20 256 0 16 13 1988
OUT

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    c=$work/c-$input
    "$ziran" build --target=c --exe --entry app:main --root "$root" -o "$c" "$module"
    "$c/app" > "$work/c-$input.out"
    # The C compiler finds nothing read before it is written either.
    cc -std=c11 -O2 -Wall -Wno-unused-label -Werror=uninitialized -Werror=maybe-uninitialized \
        -I"$c" -c -o "$work/check.o" "$c/app.c"
    for name in Rows Grid Halves Fields Skip; do
        if awk "/^app_$name\\(/,/^}/" "$c/app.c" | grep -q '{0}'; then
            echo "$name still fills a local that is wholly stored before any read ($input)" >&2
            exit 1
        fi
    done
    for name in Partial Early Gap Leave Some Running Escape Unknown Wrapped Outer Pad Promoted Shifted; do
        if ! awk "/^app_$name\\(/,/^}/" "$c/app.c" | grep -q '{0}'; then
            echo "$name dropped the zero fill of a local that may be read first ($input)" >&2
            exit 1
        fi
    done
    "$ziran" bundle --root "$root" --entry app:main -o "$work/$input.zx" "$module"
    "$ziran" run "$work/$input.zx" > "$work/zx-$input.out"
    "$run_vm" run --vm "$work/$input.zx" > "$work/vm-$input.out"
done
"$ziran" build --target=go --pkg main --exe --entry app:main --root "$work" -o "$work/go" "$work/app.zi"
GO111MODULE=off go build -o "$work/go/app" "$work/go"/*.go
"$work/go/app" > "$work/go.out"
for target in c-source c-saved go zx-source zx-saved vm-source vm-saved; do
    if ! cmp -s "$work/expected" "$work/$target.out"; then
        echo "zero-filled locals read differently on $target:" >&2
        diff "$work/expected" "$work/$target.out" >&2 || true
        exit 1
    fi
done
echo "definite_initialization: stored-first locals skip the fill; C, Go, native and the VM agree"
