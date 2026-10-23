#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Pair :: struct { left: s64; right: s64; }
Color :: enum { Red :: 0; Green :: 1; Blue :: 2; }
using, map("global_green" = "Green") Color;
GlobalGreen :: global_green;
GlobalGreenRun :: #run global_green;
global_green_value: s64 = global_green;
#assert global_green == 1
#if global_green == 1 {
    GlobalSelected :: 1;
} else {
    GlobalSelected :: 0;
}
FileMapped :: () -> s64 {
    return GlobalGreen + GlobalGreenRun + global_green_value + GlobalSelected
}
OnlyLeft :: (pair: Pair) -> s64 {
    using, only("left") pair;
    return left
}
ExceptLeft :: (pair: Pair) -> s64 {
    using, except("left") pair;
    return right
}
Mapped :: (pair: Pair) -> s64 {
    using, map("first" = "left", "second" = "right") pair;
    return first + second
}
EnumOnly :: () -> s64 {
    using, only("Red", "Blue") Color;
    return Red + Blue
}
EnumMapped :: () -> s64 {
    using, map("go" = "Green") Color;
    if go == 1 { return 5 }
    return 0
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib"
#program_export
Answer :: () -> s32 {
    pair := Pair.{.left = 20, .right = 22}
    if OnlyLeft(pair) != 20 { return 0 }
    if ExceptLeft(pair) != 22 { return 0 }
    if Mapped(pair) != 42 { return 0 }
    if EnumOnly() != 2 { return 0 }
    if EnumMapped() != 5 { return 0 }
    if FileMapped() != 4 { return 0 }
    return 1
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 1
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --entry app:Answer \
                --root "$root" -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 1 { panic("using modifiers") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --entry app:Answer --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 1 ? 0 : 1; }
C
            "${CC:-cc}" -std=c99 -pedantic-errors \
                -I"$repo/include" -I"$output" "$output"/*.c \
                -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --entry app:Answer --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 1 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 \
                -I"$repo/include" -I"$output" "$output"/*.cpp \
                -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/hidden.zi" <<'ZI'
Pair :: struct { left: s64; right: s64; }
#program_export
Bad :: () -> s32 {
    pair := Pair.{.left = 1, .right = 2}
    using, only("left") pair;
    return right
}
ZI
if "$ziran" check --root "$work" "$work/hidden.zi" \
    2> "$work/hidden.err"; then
    echo 'an only-hidden field was accepted' >&2
    exit 1
fi
rg -q 'unresolved name: right' "$work/hidden.err"

cat > "$work/excluded.zi" <<'ZI'
Color :: enum { Red :: 0; Green :: 1; }
#program_export
Bad :: () -> s32 {
    using, except("Green") Color;
    return Green
}
ZI
if "$ziran" check --root "$work" "$work/excluded.zi" \
    2> "$work/excluded.err"; then
    echo 'an except-hidden enum member was accepted' >&2
    exit 1
fi
rg -q 'unresolved name: Green' "$work/excluded.err"

cat > "$work/unmapped.zi" <<'ZI'
Color :: enum { Red :: 0; Green :: 1; }
#program_export
Bad :: () -> s32 {
    using, map("go" = "Green") Color;
    return Green
}
ZI
if "$ziran" check --root "$work" "$work/unmapped.zi" \
    2> "$work/unmapped.err"; then
    echo 'an unmapped enum member was accepted' >&2
    exit 1
fi
rg -q 'unresolved name: Green' "$work/unmapped.err"

cat > "$work/file_excluded.zi" <<'ZI'
Color :: enum { Red :: 0; Green :: 1; }
using, only("Red") Color;
hidden: s64 = Green;
ZI
if "$ziran" check --root "$work" "$work/file_excluded.zi" \
    2> "$work/file_excluded.err"; then
    echo 'an only-hidden file-scope enum member was accepted' >&2
    exit 1
fi
rg -q 'enum member needs a type qualifier or using: Green' \
    "$work/file_excluded.err"

cat > "$work/file_ambiguous_map.zi" <<'ZI'
Color :: enum { Red :: 0; Green :: 1; }
using, map("choice" = "Red") Color;
using, map("choice" = "Green") Color;
choice_value: s64 = choice;
ZI
if "$ziran" check --root "$work" "$work/file_ambiguous_map.zi" \
    2> "$work/file_ambiguous_map.err"; then
    echo 'ambiguous file-scope enum map was accepted' >&2
    exit 1
fi
rg -q 'ambiguous using enum member: choice' \
    "$work/file_ambiguous_map.err"

cat > "$work/local_ambiguous_map.zi" <<'ZI'
Color :: enum { Red :: 0; Green :: 1; }
Bad :: () -> s64 {
    using, map("choice" = "Red") Color;
    using, map("choice" = "Green") Color;
    return choice
}
ZI
if "$ziran" check --root "$work" "$work/local_ambiguous_map.zi" \
    2> "$work/local_ambiguous_map.err"; then
    echo 'ambiguous local enum map was accepted' >&2
    exit 1
fi
rg -q 'ambiguous using enum member: choice' \
    "$work/local_ambiguous_map.err"
