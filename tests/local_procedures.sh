#!/bin/sh
# A procedure may declare procedures inside it, as in Jai: they may recurse,
# nest, and share a name with a local procedure elsewhere, and like Jai's
# they see file scope but not the enclosing procedure's locals. They stay
# out of the module's API. Source and saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Point :: struct { x: s32; }
NamedPoint :: struct { Twice: s32; }
Scale :: 3;

Compute :: () -> s64 {
    Twice :: (x: s32) -> s32 { return x * 2; }
    Fact :: (n: s64) -> s64 {
        if n <= 1 { return 1 }
        return n * Fact(n - 1)
    }
    Shift :: (p: Point) -> Point {
        Bump :: (v: s32) -> s32 { return v + Scale; }
        return Point.{x = Bump(p.x)}
    }
    label := "Twice(1)"
    if label.count != 8 { return -1 }
    return cast(s64) Twice(21) + Fact(5) + cast(s64) Shift(Point.{x = 9}).x
}

Other :: () -> s32 {
    Twice :: (x: s32) -> s32 { return x * 3; }
    return Twice(2)
}

MemberSpacing :: () -> s32 {
    Twice :: () -> s32 { return 35; }
    value := NamedPoint.{Twice = 7}
    return Twice() + value . Twice
}

#program_export
Answer :: () -> s32 {
    // 42 + 120 + 12, then 6.
    if Compute() != 174 || Other() != 6 { return 1 }
    if MemberSpacing() != 42 { return 2 }
    return 42
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" api --root "$work" "$work/app.zi" > "$work/api.txt"
if grep -q zi_local "$work/api.txt"; then cat "$work/api.txt" >&2; exit 1; fi

cat > "$work/outer.zi" <<'ZI'
main :: () {
    base: s32 = 5
    Add :: (x: s32) -> s32 {
        return x + base
    }
    print("%\n", Add(1))
}
ZI
if "$ziran" check --root "$work" "$work/outer.zi" 2> "$work/outer.err"; then
    echo "outer: accepted" >&2
    exit 1
fi
grep -Fq 'outer.zi:4:' "$work/outer.err"
grep -Fq 'unresolved name: base (a local procedure cannot use the locals of the procedure around it)' "$work/outer.err"

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 42
    output="$work/c-$input"
    "$ziran" build --target=c --exe --entry app:Answer --root "$root" -o "$output" "$module"
    status=0
    "$output/app" || status=$?
    test "$status" = 42
    output="$work/cpp-$input"
    "$ziran" build --target=cpp --root "$root" -o "$output" "$module"
    cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    "${CXX:-c++}" -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
    "$output/app"
    output="$work/go-$input"
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("wrong result") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zib" "$work/saved.zib"
