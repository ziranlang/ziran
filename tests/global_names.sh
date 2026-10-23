#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/global_names.zi" <<'ZI'
TextStyleResolver :: struct {
    count: s64
}
class :: struct { x: s64; }
type :: struct { y: s64; }
switch :: enum { On :: 2; }
xor :: struct { z: s64; }
_Atomic :: struct { w: s64; }
text_style_resolver: TextStyleResolver;
range: s64;
ziran_keyword_range_0: s64;
char: s64;
ziran_keyword_char_0: s64;

KeywordResult :: () -> s64 {
    a: class = class.{.x = 11}
    b: type = type.{.y = 22}
    state: switch = switch.On
    c: xor = xor.{.z = 3}
    d: _Atomic = _Atomic.{.w = 4}
    return a.x + b.y + cast(s64)state + c.z + d.w
}

#program_export
Answer :: () -> s64 {
    if KeywordResult() != 42 { return 0 }
    text_style_resolver.count = 38
    range = 1
    ziran_keyword_range_0 = 1
    char = 1
    ziran_keyword_char_0 = 1
    return text_style_resolver.count + range + ziran_keyword_range_0 +
        char + ziran_keyword_char_0
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/global_names.zi"
for input in source saved; do
    if test "$input" = source; then
        module="$work/global_names.zi"
    else
        module="$work/ir/global_names.zir"
    fi
    "$ziran" bundle --root "$work" --entry global_names:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c cpp go; do
        output="$work/$input-$target"
        "$ziran" build "--target=$target" --root "$work" \
            -o "$output" "$module"
        case "$target" in
            c)
                printf '#include "global_names.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' \
                    > "$work/main.c"
                "${CC:-cc}" -std=c11 -Wall -Werror -I"$repo/include" \
                    -I"$output" "$output/global_names.c" "$work/main.c" \
                    -o "$output/app"
                "$output/app"
                ;;
            cpp)
                printf '#include "global_names.hpp"\nint main() { return Answer() == 42 ? 0 : 1; }\n' \
                    > "$work/main.cpp"
                "${CXX:-c++}" -std=c++17 -Wall -Werror -I"$repo/include" \
                    -I"$output" "$output/global_names.cpp" "$work/main.cpp" \
                    -o "$output/app"
                "$output/app"
                ;;
            go)
                printf 'package ziran\nimport "testing"\nfunc TestGlobalNames(t *testing.T) { if GlobalNames_Answer() != 42 { t.Fatal("global names") } }\n' \
                    > "$output/global_names_test.go"
                GO111MODULE=off go test "$output"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zx" "$work/saved.zx"
