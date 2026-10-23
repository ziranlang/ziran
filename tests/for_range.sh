#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/ranges.zi" <<'ZI'
starts: s32;
ends: s32;
Start :: () -> int {
    starts += 1
    return 1
}
End :: () -> int {
    ends += 1
    return 3
}
#program_export
Answer :: () -> s32 {
    starts = 0
    ends = 0
    total: int = 0
    for i: Start()..End() { total += i }
    if starts != 1 || ends != 1 { return 0 }
    for 1..3 { total += it + it_index }
    for < i: 1..3 { total += i * it_index }
    for i: 1..3 {
        if i == 2 { continue }
        total += 10
    }
    for x: 0..2 {
        for y: 0..2 {
            if y == 1 { continue }
            total += x + y
        }
    }
    for i: 0..2 {
        defer { total += 10 }
        if i == 0 { continue }
        if i == 1 { break }
        total += 100
    }
    for i: 9223372036854775807..9223372036854775807 {
        total += 1
    }
    for 3..1 { total += 100 }
    for < 3..1 { total += 100 }
    if total != 72 { return 0 }
    return 42
}
#program_export
SingleBody :: () -> s32 {
    total: s32 = 0
    if true
        total += 1
    total += 1
    if false
        total = 100
    else
        total += 2
    if true
        if false
            total = 100
        else
            total += 4
    total += 1
    while total < 11
        total += 1
    for i: 0..2
        if i == 1
            total += 10
        else
            total += 1
    if false
        total = 100
    else if total == 23
        total += 1
    else
        total = 100
    if true
    {
        total += 1
    }
    if total == 25
        return 42
    return 0
}
#program_export
WideAnswer :: () -> int {
    return 42
}
#program_export
NamedControl :: () -> s32 {
    total: s32 = 0
    for outer: 1..3 {
        for inner: 1..3 {
            if inner == 2 { break outer }
            total += cast(s32) outer
        }
        total += 100
    }
    x: s32 = 0
    while running := x < 3 {
        x += 1
        if x == 1 { continue running }
        if x == 2 { break running }
    }
    for outer: 0..2 {
        defer { total += 10 }
        for inner: 0..2 {
            defer { total += 1 }
            if inner == 0 { continue outer }
        }
    }
    if total != 34 || x != 2 { return 0 }
    return 42
}
ZI

"$ziran" check --root "$work" "$work/ranges.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/ranges.zi"
for input in source saved; do
    if test "$input" = source; then
        file=$work/ranges.zi
        root=$work
    else
        file=$work/ir/ranges.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry ranges:Answer \
        -o "$work/$input.zx" "$file"
    test "$("$ziran" run "$work/$input.zx")" = 42
    "$ziran" bundle --root "$root" --entry ranges:WideAnswer \
        -o "$work/$input-wide.zx" "$file"
    test "$("$ziran" run "$work/$input-wide.zx")" = 42
    "$ziran" bundle --root "$root" --entry ranges:SingleBody \
        -o "$work/$input-single.zx" "$file"
    test "$("$ziran" run "$work/$input-single.zx")" = 42
    "$ziran" bundle --root "$root" --entry ranges:NamedControl \
        -o "$work/$input-named.zx" "$file"
    test "$("$ziran" run "$work/$input-named.zx")" = 42
    for target in c cpp go; do
        out=$work/$input-$target
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
func main() {
    if Ranges_Answer() != 42 { panic("Jai range loop") }
    if Ranges_SingleBody() != 42 { panic("Jai single-statement bodies") }
    if Ranges_NamedControl() != 42 { panic("Jai named loop control") }
}
GO
            GO111MODULE=off go run "$out/ranges.go" "$out/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$file"
            cat > "$out/main.c" <<'C'
#include "ranges.h"
int main(void) { return Answer() == 42 && SingleBody() == 42 && NamedControl() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out/ranges.c" "$out/main.c" -o "$out/app"
            "$out/app"
        else
            "$ziran" build --target=cpp --root "$root" -o "$out" "$file"
            cat > "$out/main.cpp" <<'CPP'
#include "ranges.hpp"
int main() { return Answer() == 42 && SingleBody() == 42 && NamedControl() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                "$out/ranges.cpp" "$out/main.cpp" -o "$out/app"
            "$out/app"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"
cmp "$work/source-single.zx" "$work/saved-single.zx"
cmp "$work/source-named.zx" "$work/saved-named.zx"

cat > "$work/collision.zi" <<'ZI'
range_first_0 :: 7
#program_export
Answer :: () -> s32 {
    for 1..1 { }
    return 42
}
ZI
"$ziran" check --root "$work" "$work/collision.zi"
"$ziran" bundle --root "$work" --entry collision:Answer \
    -o "$work/collision.zx" "$work/collision.zi"
test "$("$ziran" run "$work/collision.zx")" = 42

cat > "$work/c_style.zi" <<'ZI'
Answer :: () -> s32 {
    for i = 0; i < 3; i += 1 { return i }
    return 0
}
ZI
cat > "$work/missing_end.zi" <<'ZI'
Answer :: () -> s32 {
    for i: 1.. {
        return i
    }
    return 0
}
ZI
cat > "$work/index_binder.zi" <<'ZI'
Answer :: () -> s32 {
    for it_index: 1..3 { return cast(s32) it_index }
    return 0
}
ZI
for invalid in c_style missing_end index_binder; do
    if "$ziran" check --root "$work" "$work/$invalid.zi" \
       2> "$work/$invalid.err"; then
        echo "accepted invalid for range: $invalid" >&2
        exit 1
    fi
done
grep -Fq 'C-style for headers are not Jai syntax' "$work/c_style.err"
grep -Fq 'for currently supports Jai integer ranges' "$work/missing_end.err"
grep -Fq 'for range binder cannot be it_index' "$work/index_binder.err"

cat > "$work/missing_body.zi" <<'ZI'
Answer :: () -> s32 {
    if true
}
ZI
if "$ziran" check --root "$work" "$work/missing_body.zi" \
    2> "$work/missing_body.err"; then
    echo 'accepted control header without a body' >&2
    exit 1
fi
grep -Fq 'control header requires a body' "$work/missing_body.err"

cat > "$work/orphan_else.zi" <<'ZI'
Answer :: () -> s32 {
    else
        return 42
    return 0
}
ZI
if "$ziran" check --root "$work" "$work/orphan_else.zi" \
    2> "$work/orphan_else.err"; then
    echo 'accepted else without a matching if' >&2
    exit 1
fi
grep -Fq 'else without matching if' "$work/orphan_else.err"

cat > "$work/unknown_loop.zi" <<'ZI'
Answer :: () -> s32 {
    for outer: 0..2 {
        for inner: 0..2 { continue missing }
    }
    return 42
}
ZI
if "$ziran" check --root "$work" "$work/unknown_loop.zi" \
    2> "$work/unknown_loop.err"; then
    echo 'accepted a named continue without an enclosing target' >&2
    exit 1
fi
grep -Fq 'named loop target is not an enclosing loop' "$work/unknown_loop.err"

cat > "$work/closed_loop.zi" <<'ZI'
Answer :: () -> s32 {
    for outer: 0..2 { break outer }
    break outer
    return 42
}
ZI
if "$ziran" check --root "$work" "$work/closed_loop.zi" \
    2> "$work/closed_loop.err"; then
    echo 'accepted a named break outside its target loop' >&2
    exit 1
fi
grep -Fq 'named loop target is not an enclosing loop' "$work/closed_loop.err"
