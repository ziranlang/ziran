#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/constant_names.zi" <<'ZI'
Char :: struct {
    value: s64
}
char :: 40
ziran_keyword_char_0 :: char - 39
range :: 1
ziran_keyword_range_0 :: range - 1

#program_export
Answer :: () -> s64 {
    bytes: [char]u8
    bytes[0] = cast(u8)0
    return char + ziran_keyword_char_0 + range +
        ziran_keyword_range_0 + cast(s64)bytes[0]
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/constant_names.zi"
for input in source saved; do
    if test "$input" = source; then
        module="$work/constant_names.zi"
    else
        module="$work/ir/constant_names.zir"
    fi
    "$ziran" bundle --root "$work" --entry constant_names:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c cpp go; do
        output="$work/$input-$target"
        "$ziran" build "--target=$target" --root "$work" \
            -o "$output" "$module"
        case "$target" in
            c)
                printf '#include "constant_names.h"\nint main(void) { char byte = 0; return Answer() == 42 && byte == 0 ? 0 : 1; }\n' \
                    > "$work/main.c"
                "${CC:-cc}" -std=c11 -Wall -Werror -I"$repo/include" \
                    -I"$output" "$output/constant_names.c" "$work/main.c" \
                    -o "$output/app"
                "$output/app"
                ;;
            cpp)
                printf '#include "constant_names.hpp"\nint main() { char byte = 0; return Answer() == 42 && byte == 0 ? 0 : 1; }\n' \
                    > "$work/main.cpp"
                "${CXX:-c++}" -std=c++17 -Wall -Werror -I"$repo/include" \
                    -I"$output" "$output/constant_names.cpp" "$work/main.cpp" \
                    -o "$output/app"
                "$output/app"
                ;;
            go)
                printf 'package ziran\nimport "testing"\nfunc TestConstantNames(t *testing.T) { if ConstantNames_Answer() != 42 { t.Fatal("constant names") } }\n' \
                    > "$output/constant_names_test.go"
                GO111MODULE=off go test "$output"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zx" "$work/saved.zx"
