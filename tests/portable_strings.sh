#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/strings.zi" <<'EOF'

Greeting :: "hello \"Ziran\""
GreetingAlias :: Greeting

Pair :: struct {
    first: string
    second: string
}

Same :: (left: string, right: string) -> bool {
    return left == right
}

#program_export
Answer :: () -> s32 {
    if Greeting != "hello \"Ziran\"" || GreetingAlias != Greeting { return 0 }
    if Greeting.count != 13 { return 0 }
    empty: string
    if empty.count != 0 || empty != "" { return 0 }
    text: string = "a\u00e9\x00z"
    if text.count != 5 { return 0 }
    if text[0] != cast(u8)97 || text[1] != cast(u8)0xc3 ||
       text[2] != cast(u8)0xa9 || text[3] != cast(u8)0 ||
       text[4] != cast(u8)122 { return 0 }
    pair: Pair
    pair.first = text
    pair.second = "a\u00e9\x00z"
    if !Same(pair.first, pair.second) { return 0 }
    if pair.first == "a\u00e9z" { return 0 }
    if "\U0001f642".count != 4 { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/strings.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/strings.zi"
"$ziran" bundle --root "$work" --entry strings:Answer \
    -o "$work/source.zx" "$work/strings.zi"
"$ziran" bundle --root "$work" --entry strings:Answer \
    -o "$work/ir.zx" "$work/ir/strings.zir"
cmp "$work/source.zx" "$work/ir.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/ir.zx")" = 42

for input in source ir; do
    if test "$input" = source; then
        filename="$work/strings.zi"
    else
        filename="$work/ir/strings.zir"
    fi
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$output" "$filename"
            cat > "$output/main.go" <<'GO'
package main
func main() { if Strings_Answer() != 42 { panic("wrong string result") } }
GO
            GO111MODULE=off go run "$output/strings.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$work" \
                -o "$output" "$filename"
            cat > "$output/main.c" <<'C'
#include "strings.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -Iinclude -I"$output" "$output/strings.c" \
                "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --root "$work" \
                -o "$output" "$filename"
            cat > "$output/main.cpp" <<'CPP'
#include "strings.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -Iinclude -I"$output" "$output/strings.cpp" \
                "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done

cat > "$work/bad_index.zi" <<'EOF'
#program_export
Answer :: () -> s32 {
    text: string = "a"
    return cast(s32)text[1]
}
EOF
"$ziran" bundle --root "$work" --entry bad_index:Answer \
    -o "$work/bad_index.zx" "$work/bad_index.zi"
if "$ziran" run "$work/bad_index.zx" 2> "$work/bad_index.err"; then
    echo 'out-of-range string index unexpectedly ran' >&2
    exit 1
fi
grep -Fq 'portable execution failed' "$work/bad_index.err"
