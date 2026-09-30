#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/semicolon_statements.zi" <<'ZI'
Pair :: struct { left: s32; right: s32; }; First :: 20; Second :: 22
Inline :: () -> s32 { left: s32 = 20; right: s32 = 22; return left + right; }
// A statement after a one-line block's closing brace is the next statement.
Blocks :: () -> s32 {
    x: s32 = 0; i: s32 = 0
    while i < 4 { if i == 1 { x += 100; } else if i == 2 { x += 10; } else { x += 1; } i += 1; }
    if x == 112 { x = 0; } else { x = 1; } x += 5
    for 0..2 { if it == 1 { x += 7 } x += 3 }
    defer { x += 0 } x += 1
    return x
}
#program_export
Answer :: () -> s32 {
    a: s32 = #ifx true then First; else 0; b: s32 = Second
    if a == 20 { b += 0; }; pair: Pair = Pair.{left = a, right = b}
    if Inline() != 42 { return 0 }
    if Blocks() != 22 { return 0 }
    return pair.left + pair.right
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/semicolon_statements.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/semicolon_statements.zi
        root=$work
    else
        module=$work/ir/semicolon_statements.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry semicolon_statements:Answer \
        -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build "--target=$target" --root "$root" \
            -o "$output" "$module"
        case "$target" in
            c)
                printf '#include "semicolon_statements.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.c"
                "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                    "$output"/*.c "$work/main.c" -o "$output/app"
                "$output/app"
                ;;
            cpp)
                printf '#include "semicolon_statements.hpp"\nint main() { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.cpp"
                "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                    "$output"/*.cpp "$work/main.cpp" -o "$output/app"
                "$output/app"
                ;;
            go)
                cat > "$output/semicolon_statements_test.go" <<'GO'
package ziran
import "testing"
func TestSemicolonStatements(t *testing.T) {
    if SemicolonStatements_Answer() != 42 { t.Fatal("semicolon statements") }
}
GO
                GO111MODULE=off go test "$output"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zib" "$work/saved.zib"
