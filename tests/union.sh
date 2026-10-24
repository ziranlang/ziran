#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/overlay.zi" <<'ZI'
Cell :: union {
    narrow: u8
    wide: u64
}

Generic :: union($T: Type) {
    value: T
    bytes: [8]u8
}
#assert size_of(Generic(s32)) == 8

#program_export
Answer :: () -> s32 {
    if size_of(Cell) != 8 { return 0 }
    if size_of(Generic(s32)) != 8 { return 0 }
    cell: Cell
    cell.narrow = cast(u8)1
    cell.wide = cast(u64)40
    other: Generic(s32)
    other.value = 2
    return cast(s32)cell.wide + other.value
}
ZI

cat > "$work/scalar.zi" <<'ZI'
Narrow :: union { value: u8 }
Half :: union { value: u16 }
Word :: union { value: u32 }
Scalar :: union {
    narrow: u8
    wide: u64
    real: float64
}
#assert size_of(Narrow) == 1
#assert size_of(Half) == 2
#assert size_of(Word) == 4
#assert size_of(Scalar) == 8

#program_export
Overlap :: () -> s32 {
    narrow_cell: Narrow
    narrow_cell.value = cast(u8)1
    half_cell: Half
    half_cell.value = cast(u16)2
    word_cell: Word
    word_cell.value = cast(u32)3
    if narrow_cell.value != 1 || half_cell.value != 2 ||
       word_cell.value != 3 { return 5 }
    cell: Scalar
    cell.narrow = cast(u8)255
    if cell.wide != 255 { return 1 }
    other: Scalar
    other.wide = cast(u64)513
    if other.narrow != 1 { return 2 }
    cell.wide = cast(u64)10
    cell.wide += cast(u64)32
    if cell.narrow != 42 { return 3 }
    copied: Scalar = cell
    if copied.wide != 42 { return 5 }
    copied.narrow = cast(u8)1
    if cell.wide != 42 || copied.narrow != 1 { return 6 }
    stored: Scalar
    stored.real = 2.0
    if stored.real != 2.0 { return 4 }
    return 42
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/overlay.zi"
"$ziran" ir --root "$work" -o "$work/scalar-ir" "$work/scalar.zi"
mkdir -p "$work/ir"
cp "$work/scalar-ir/scalar.zir" "$work/ir/scalar.zir" 2>/dev/null || true
for input in source saved; do
    if test "$input" = source; then
        module=$work/overlay.zi
        root=$work
    else
        module=$work/ir/overlay.zir
        root=$work/ir
    fi
    out="$work/c-$input"
    "$ziran" build --target=c --root "$root" \
        -o "$out" "$module"
    cat > "$out/main.c" <<'C'
#include <stddef.h>
#include "overlay.h"
_Static_assert(sizeof(Cell) == 8, "union layout");
_Static_assert(offsetof(Cell, narrow) == 0, "union first field");
_Static_assert(offsetof(Cell, wide) == 0, "union second field");
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
        "$out"/*.c -o "$out/app"
    "$out/app"
    if test "$input" = source; then
        scalar=$work/scalar.zi
        scalar_root=$work
    else
        scalar=$work/ir/scalar.zir
        scalar_root=$work/ir
    fi
    "$ziran" build --target=go --pkg main --entry scalar:Overlap \
        --root "$scalar_root" -o "$work/go-$input" "$scalar"
    cat > "$work/go-$input/main.go" <<'GO'
package main
import "unsafe"
func main() {
    if unsafe.Sizeof(Narrow{}) != 1 || unsafe.Alignof(Narrow{}) != 1 ||
       unsafe.Sizeof(Half{}) != 2 || unsafe.Alignof(Half{}) != 2 ||
       unsafe.Sizeof(Word{}) != 4 || unsafe.Alignof(Word{}) != 4 ||
       unsafe.Sizeof(Scalar{}) != 8 || unsafe.Alignof(Scalar{}) != 8 {
        panic("union layout go")
    }
    if Scalar_Overlap() != 42 { panic("union overlap go") }
}
GO
    GO111MODULE=off go run "$work/go-$input"/*.go
    out="$work/scalar-c-$input"
    "$ziran" build --target=c --entry scalar:Overlap \
        --root "$scalar_root" -o "$out" "$scalar"
    cat > "$out/main.c" <<'C'
#include "scalar.h"
_Static_assert(sizeof(Narrow) == 1 && _Alignof(Narrow) == 1, "narrow union");
_Static_assert(sizeof(Half) == 2 && _Alignof(Half) == 2, "half union");
_Static_assert(sizeof(Word) == 4 && _Alignof(Word) == 4, "word union");
_Static_assert(sizeof(Scalar) == 8 && _Alignof(Scalar) == 8, "wide union");
int main(void) { return Overlap() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
        "$out"/*.c -o "$out/app"
    "$out/app"
    "$ziran" bundle --root "$scalar_root" --entry scalar:Overlap \
        -o "$work/overlap-$input.zx" "$scalar"
    test "$("$ziran" run "$work/overlap-$input.zx")" = 42
done
cmp "$work/overlap-source.zx" "$work/overlap-saved.zx"

cat > "$work/badunion.zi" <<'ZI'
BadUnion :: union {
    text: string
    value: s32
}
#program_export
Bad :: () -> s32 {
    u: BadUnion
    return 0
}
ZI
if "$ziran" bundle --root "$work" --entry badunion:Bad \
    -o "$work/badunion.zx" "$work/badunion.zi" \
    2> "$work/badunion.err"; then
    echo 'portable bundle accepted a non-scalar union field' >&2
    exit 1
fi
grep -Fq 'portable unions support scalar fields only' \
    "$work/badunion.err"

# Native C keeps unions with pointer fields, also when an entry selects the
# functions that are linked.
cat > "$work/pointerunion.zi" <<'ZI'
PointerUnion :: union {
    reference: *void
    value: s64
}
#program_export
main :: () -> s32 {
    u: PointerUnion
    u.value = 42
    return cast(s32)(u.value - 42)
}
ZI
mkdir -p "$work/pointerunion-c"
"$ziran" build --target=c --root "$work" --entry pointerunion:main \
    -o "$work/pointerunion-c" "$work/pointerunion.zi"
"${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/pointerunion-c" \
    "$work"/pointerunion-c/*.c -o "$work/pointerunion"
"$work/pointerunion"
