#!/bin/sh
# std/format builds text on a Vec(u8): Append adds one value, and
# BuilderPrint(*builder, "format", args...) appends a formatted piece, each %
# taking the next argument and %% writing one percent. BuilderPrint imports
# std/format for its file. Source and saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Label :: (done: s32, total: s64) -> string {
    line: Vec(u8);
    BuilderPrint(*line, "% of % (%%) ratio % ok=%", done, total, 0.375, done < total);
    small: u8 = 200;
    big: u64 = 18446744073709551615;
    BuilderPrint(*line, " % % %", small, big, -42);
    return BuilderFinish(line);
}

#program_export
Answer :: () -> s32 {
    text := Label(3, 8);
    if text != "3 of 8 (%) ratio 0.375 ok=true 200 18446744073709551615 -42" {
        print("%\n", text);
        return 1;
    }
    return 42;
}
ZI

reject() {
    name=$1
    message=$2
    cat > "$work/$name.zi"
    if "$ziran" check --root "$work" "$work/$name.zi" 2> "$work/$name.err"; then
        echo "$name: accepted" >&2
        exit 1
    fi
    grep -Fq "$message" "$work/$name.err"
}
reject few 'BuilderPrint format has more % than arguments' <<'ZI'
main :: () { b: Vec(u8); BuilderPrint(*b, "% %", 1); }
ZI
reject many 'BuilderPrint has more arguments than % in its format' <<'ZI'
main :: () { b: Vec(u8); BuilderPrint(*b, "%", 1, 2); }
ZI
reject variable 'BuilderPrint needs a builder and a literal format' <<'ZI'
main :: () { b: Vec(u8); f := "%"; BuilderPrint(*b, f, 1); }
ZI

"$ziran" check --root "$work" "$work/app.zi"
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
