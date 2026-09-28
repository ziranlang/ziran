#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

command -v cargo >/dev/null 2>&1 || {
    echo 'cargo is required to test named imports through Rust' >&2
    exit 1
}

cat > "$work/app.zi" <<'ZI'
Text :: #import "text";
#program_export
Answer :: () -> s32 {
    if Text.LowerASCII(cast(u8)65) != cast(u8)97 { return 1 }
    if !Text.StartsWithFoldASCII("abc", "AB") { return 2 }
    return 0
}
ZI

"$ziran" check --root "$work" --module-path "$repo/std" "$work/app.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/std" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0

for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/app.zi
    else
        root=$work/ir
        file=$work/ir/app.zir
    fi
    for target in c cpp go rust; do
        out=$work/$target-$input
        if test "$target" = rust; then
            "$ziran" build --target=rust --root "$root" \
                --module-path "$repo/std" --entry app:Answer --exe \
                -o "$out" "$file"
            cargo build --quiet --manifest-path "$out/Cargo.toml"
            "$out/target/debug/ziran_generated"
        elif test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                --module-path "$repo/std" -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
func main() { if App_Answer() != 0 { panic("named imports") } }
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" \
                --module-path "$repo/std" -o "$out" "$file"
            cat > "$out/test.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 0 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out"/*.c -o "$out/app"
            "$out/app"
        else
            "$ziran" build --target=cpp --root "$root" \
                --module-path "$repo/std" -o "$out" "$file"
            cat > "$out/test.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 0 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                "$out"/*.cpp -o "$out/app"
            "$out/app"
        fi
    done
done

cmp "$work/c-source/app.c" "$work/c-saved/app.c"
cmp "$work/c-source/text.c" "$work/c-saved/text.c"
cmp "$work/cpp-source/app.cpp" "$work/cpp-saved/app.cpp"
cmp "$work/cpp-source/text.cpp" "$work/cpp-saved/text.cpp"
cmp "$work/go-source/app.go" "$work/go-saved/app.go"
cmp "$work/go-source/text.go" "$work/go-saved/text.go"
cmp "$work/rust-source/src/main.rs" "$work/rust-saved/src/main.rs"
