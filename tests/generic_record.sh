#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/std/option.zi" "$work/option.zi"
cp "$repo/std/result.zi" "$work/result.zi"

cat > "$work/app.zi" <<'ZI'
#import "option"
#import "result"

Number :: Option(s32)
Outcome :: Result(s32, string)
Holder :: struct($T: Type) { item: T; }
Nested :: Holder(Number)
EmptyHolder :: Holder([0]s32)
#assert size_of(EmptyHolder) == 0

ReadNumber :: (number: Number) -> s32 {
    if number.has_value { return number.value }
    return 0
}

MakeOutcome :: () -> Outcome {
    return Outcome.{is_ok = true, value = 40, error = ""}
}

#program_export
Answer :: () -> s32 {
    number: Number = Number.{has_value = true, value = 2}
    nested: Nested = Nested.{item = number}
    empty: EmptyHolder
    outcome: Outcome = MakeOutcome()
    if !outcome.is_ok || outcome.error.count != 0 ||
       empty.item.count != 0 || empty.item.data != null { return 0 }
    return ReadNumber(nested.item) + outcome.value
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zx" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zx" "$work/ir/app.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42

for input in "$work/app.zi" "$work/ir/app.zir"; do
    case "$input" in
        *.zi) suffix=source ; root=$work ;;
        *) suffix=saved ; root=$work/ir ;;
    esac
    for target in c cpp go; do
        out="$work/$target-$suffix"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$input"
            cat > "$out/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("generic record result") } }
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" \
                -o "$out" "$input"
            cat > "$out/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -std=c11 -Iinclude -I"$out" "$out"/*.c \
                -o "$out/app"
            "$out/app"
        else
            "$ziran" build --target=cpp --root "$root" \
                -o "$out" "$input"
            cat > "$out/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -std=c++17 -Iinclude -I"$out" "$out"/*.cpp \
                -o "$out/app"
            "$out/app"
        fi
    done
done
