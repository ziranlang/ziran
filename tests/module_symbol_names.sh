#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/Foo.zi" <<'ZI'
Get :: () -> s64 { return 10 }
ZI
cat > "$work/foo.zi" <<'ZI'
Get :: () -> s64 { return 11 }
ZI
cat > "$work/foo_bar.zi" <<'ZI'
Get :: () -> s64 { return 12 }
ZI
cat > "$work/FooBar.zi" <<'ZI'
Get :: () -> s64 { return 9 }
ZI
cat > "$work/foo-bar.zi" <<'ZI'
Get :: () -> s64 { return 13 }
#program_export
Answer :: () -> s64 { return Get() + 29 }
ZI
cat > "$work/app.zi" <<'ZI'
A :: #import "Foo";
B :: #import "foo";
C :: #import "foo_bar";
D :: #import "FooBar";
#program_export
Answer :: () -> s64 {
    return A.Get() + B.Get() + C.Get() + D.Get()
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/app.zi
    else
        root=$work/ir
        file=$work/ir/app.zir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zx" "$file"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c cpp go; do
        out="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("module symbol names") } }
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$file"
            cat > "$out/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out"/*.c -o "$out/program"
            "$out/program"
        else
            "$ziran" build --target=cpp --root "$root" -o "$out" "$file"
            cat > "$out/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                "$out"/*.cpp -o "$out/program"
            "$out/program"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"

"$ziran" ir --root "$work" -o "$work/ir" "$work/foo-bar.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/foo-bar.zi
    else
        root=$work/ir
        file=$work/ir/foo-bar.zir
    fi
    "$ziran" bundle --root "$root" --entry foo-bar:Answer \
        -o "$work/hyphen-$input.zx" "$file"
    test "$("$ziran" run "$work/hyphen-$input.zx")" = 42
    for target in c cpp go; do
        out=$work/hyphen-$target-$input
        "$ziran" build --target="$target" --root "$root" -o "$out" "$file"
        case "$target" in
            c)
                cat > "$out/main.c" <<'C'
#include "foo-bar.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
                "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                    "$out"/*.c -o "$out/program"
                "$out/program"
                ;;
            cpp)
                cat > "$out/main.cpp" <<'CPP'
#include "foo-bar.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
                "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                    "$out"/*.cpp -o "$out/program"
                "$out/program"
                ;;
            go)
                cat > "$out/main_test.go" <<'GO'
package ziran
import "testing"
func TestHyphenModule(t *testing.T) {
    if FooBar_Answer() != 42 { t.Fatal("hyphen module") }
}
GO
                GO111MODULE=off go test "$out"/*.go
                ;;
        esac
    done
done
cmp "$work/hyphen-source.zx" "$work/hyphen-saved.zx"
