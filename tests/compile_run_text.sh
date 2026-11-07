#!/bin/sh
# #run procedures build text of any length: Vec(u8) builders filled in for
# and while loops, results past the old 4096-byte cap, pointers to locals and
# records, slices, void procedures and loops over array literals.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/main.zi" <<'ZI'
using Vectors :: #import "std/vec";
#import "std/format";

Digits :: () -> string {
    b: Vec(u8)
    for i: 0..3 { BuilderPrint(*b, "%x", i) }
    n := 0
    while n < 2 {
        BuilderAppend(b, "!")
        n += 1
    }
    return BuilderFinish(b)
}

Line :: (b: *Vec(u8), row: s64) { BuilderPrint(b, "row %\n", row) }

Long :: () -> string {
    b: Vec(u8)
    for row: 0..1000 { Line(*b, row) }
    return BuilderFinish(b)
}

Pair :: struct {
    n: s64
    v: [4]s64
}

Set :: (pair: *Pair, i: s64) {
    pair.v[i] = i * 10
    pair.n += 1
}

Bump :: (x: *s64) { <<x += 1 }

Total :: (xs: []s64) -> s64 {
    total: s64 = 0
    for x: xs { total += x }
    return total
}

Mixed :: () -> s64 {
    pair: Pair
    Set(*pair, 1)
    Set(*pair, 3)
    a: s64 = 4
    Bump(*a)
    xs: [3]s64 = s64.[1, 2, 3]
    listed: s64 = 0
    for x: s64.[5, 6] { listed += x }
    return pair.n * 1000 + pair.v[1] + pair.v[3] + a * 100 + Total(xs[:]) + listed
}

DIGITS :: #run Digits();
LONG :: #run Long();
MIXED :: #run Mixed();
#assert MIXED == 2557

#program_export
Main :: () {
    print("%\n", DIGITS)
    print("%", LONG)
    listed: s64 = 0
    for x: s64.[1, 2, 3] { listed += x }
    print("% %\n", MIXED, listed)
}
ZI
{
    echo '0x1x2x3x!!'
    awk 'BEGIN { for (row = 0; row <= 1000; row++) printf "row %d\n", row }'
    echo '2557 6'
} > "$work/expected"
test "$(wc -c < "$work/expected")" -gt 4096

"$ziran" check --root "$work" "$work/main.zi"
"$ziran" bundle --root "$work" --entry main:Main -o "$work/main.zx" "$work/main.zi"
"$ziran" run "$work/main.zx" > "$work/zx.out"
cmp "$work/expected" "$work/zx.out"

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/main.zi"
printf '#include "main.h"\nint main(void) { Main(); return 0; }\n' > "$work/host.c"
"${CC:-cc}" -std=c11 -Iinclude -I"$work/c" "$work/c/"*.c "$work/host.c" -o "$work/app"
"$work/app" > "$work/c.out"
cmp "$work/expected" "$work/c.out"

echo '#run builds long text with builders, pointers, slices and loops: passed'
