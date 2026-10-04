#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/stored.zi" <<'ZI'
Callback :: #type (value: s32) -> s32;
Mode :: enum u8 { Off; On; }
EnumCallback :: #type (mode: Mode) -> Mode;
HolderBuilder :: #type (value: s32) -> Holder;

Holder :: struct {
    callback: Callback
    callbacks: [2]Callback
    unused_bytes: [256]u8
}
EnumHolder :: struct {
    callback: EnumCallback
}

installed: Callback;

AddOne :: (value: s32) -> s32 { return value + 1 }
AddTwo :: (value: s32) -> s32 { return value + 2 }
Echo :: (mode: Mode) -> Mode { return mode }

NewHolder :: (value: s32) -> Holder {
    result: Holder
    callback: Callback = AddOne
    result.callback = callback
    return result
}

Choose :: () -> Callback {
    result: Callback = AddTwo
    return result
}

#program_export
Answer :: () -> s32 {
    first: Callback = AddOne
    installed = first
    holder: Holder
    selected: Callback = Choose()
    holder.callback = selected
    holder.callbacks[0] = installed
    holder.callbacks[1] = holder.callback
    if holder.callback(40) != 42 || holder.callbacks[0](40) != 41 ||
        installed(40) != 41 { return 0 }
    a: Callback = holder.callbacks[0]
    b: Callback = holder.callbacks[1]
    c: Callback = holder.callback
    enum_callback: EnumCallback = Echo
    enum_holder: EnumHolder
    enum_holder.callback = enum_callback
    from_holder: EnumCallback = enum_holder.callback
    if from_holder(Mode.On) != Mode.On { return 0 }
    builder: HolderBuilder = NewHolder
    built: Holder = builder(0)
    built_callback: Callback = built.callback
    if built_callback(1) != 2 { return 0 }
    return a(38) + b(0) + c(1)
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/stored.zi"
for input in source saved; do
    if test "$input" = source; then
        set -- "$work/stored.zi"
    else
        set -- "$work/ir/stored.zir"
    fi
    "$ziran" bundle --root "$work" --entry stored:Answer -o "$work/$input.zib" "$@"
    test "$("$ziran" run "$work/$input.zib")" = 44
    for target in c cpp go; do
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --entry stored:Answer --root "$work" -o "$work/$target-$input" "$@"
        else
            "$ziran" build --target="$target" --entry stored:Answer --root "$work" -o "$work/$target-$input" "$@"
        fi
    done
    cat > "$work/c-$input/main.c" <<'C'
#include "stored.h"
int main(void) { return Answer() == 44 ? 0 : 1; }
C
    "${CC:-cc}" -std=c99 -pedantic-errors -I"$repo/include" -I"$work/c-$input" \
        "$work/c-$input/stored.c" "$work/c-$input/main.c" -o "$work/c-$input/app"
    if grep -q 'unused_bytes' "$work/c-$input/stored.h"; then
        echo 'unread record field survived entry linking' >&2
        exit 1
    fi
    "$work/c-$input/app"
    cat > "$work/cpp-$input/main.cpp" <<'CPP'
#include "stored.hpp"
int main() { return Answer() == 44 ? 0 : 1; }
CPP
    "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$work/cpp-$input" \
        "$work/cpp-$input/stored.cpp" "$work/cpp-$input/main.cpp" -o "$work/cpp-$input/app"
    "$work/cpp-$input/app"
    cat > "$work/go-$input/main.go" <<'GO'
package main
func main() { if Stored_Answer() != 44 { panic("stored slot result") } }
GO
    GO111MODULE=off go run "$work/go-$input/stored.go" "$work/go-$input/main.go"
done
cmp "$work/source.zib" "$work/saved.zib"
