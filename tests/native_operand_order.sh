#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
module=native_operand_order_test
source=$repo/tests/spec/$module.zi
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$source"
for form in source saved; do
    input=$source; root=$repo/tests/spec
    if [ "$form" = saved ]; then input=$work/ir/$module.zir; root=$work/ir; fi
    "$ziran" bundle --root "$root" --entry "$module:Answer" -o "$work/$form.zib" "$input"
    test "$("$ziran" run "$work/$form.zib")" = 42
    for target in c cpp go rust py; do
        output=$work/$form-$target
        if [ "$target" = cpp ]; then
            "$ziran" build --target=cpp --entry "$module:main" --root "$root" -o "$output" "$input"
            "${CXX:-c++}" -std=c++17 -I"$repo/include" "$output/"*.cpp -o "$output/app"
            "$output/app"
        elif [ "$target" = go ]; then
            "$ziran" build --target=go --pkg main --exe --entry "$module:main" --root "$root" -o "$output" "$input"
            GO111MODULE=off go run "$output/"*.go
        else
            "$ziran" build "--target=$target" --exe --entry "$module:main" --root "$root" -o "$output" "$input"
            case "$target" in
                c) "$output/$module" ;;
                py) python3 "$output" ;;
                rust) CARGO_TARGET_DIR=$work/rust-target cargo build --quiet --offline --manifest-path "$output/Cargo.toml"
                      "$work/rust-target/debug/ziran_generated" ;;
            esac
        fi
    done
    "$ziran" build --target=plan9-c --entry "$module:main" --root "$root" -o "$work/$form-plan9" "$input"
done
cmp "$work/source.zib" "$work/saved.zib"
