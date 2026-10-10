#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/vm-u64-enums
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source=$repo/tests/spec/vm_u64_enums_test.zi
entry=vm_u64_enums_test:main
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    root=$repo/tests/spec
    if test "$form" = saved; then
        input=$work/ir/vm_u64_enums_test.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry "$entry" -o "$work/$form.zib" "$input"
    test "$("$ziran" run "$work/$form.zib")" = 0
    for target in c cpp go; do
        output=$work/$form-$target
        if test "$target" = c; then
            "$ziran" build --target=c --root "$root" --entry "$entry" --exe -o "$output" "$input"
            "$output/vm_u64_enums_test"
        elif test "$target" = cpp; then
            "$ziran" build --target=cpp --root "$root" --entry "$entry" -o "$output" "$input"
            "${CXX:-c++}" -std=c++17 -O1 -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/run"
            "$output/run"
        else
            "$ziran" build --target=go --pkg main --root "$root" --entry "$entry" --exe -o "$output" "$input"
            env GO111MODULE=off go run "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
echo 'Unsigned 64-bit enums retain their values through casts, flags, calls and storage on every target'
