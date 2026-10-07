#!/bin/sh
set -eu
ziran=$1
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
for fixture in std_grapheme grapheme_conformance; do
    "$ziran" ir --root tests/spec --module-path std -o "$work/$fixture/ir" "tests/spec/$fixture.zi"
    for form in source saved; do
        if test "$form" = source; then
            input="$repo/tests/spec/$fixture.zi"
            set -- --root tests/spec --module-path std
        else
            input="$work/$fixture/ir/$fixture.zir"
            set -- --root "$work/$fixture/ir" --module-path "$work/$fixture/ir"
        fi
        if test "$fixture" = std_grapheme; then
            "$ziran" bundle "$@" --entry std_grapheme:SelfTest -o "$work/$form.zib" "$input"
            test "$("$ziran" run "$work/$form.zib")" = 42
        fi
        for target in c cpp go py rust; do
            output="$work/$fixture/$form-$target"
            if test "$target" = cpp; then
                "$ziran" build "$@" --target=cpp --no-main --entry "$fixture:NativeMain" -o "$output" "$input"
                printf '#include "%s.hpp"\nint main() { return %s(); }\n' "$fixture" NativeMain > "$output/driver.cpp"
                "${CXX:-c++}" -std=c++17 -I"$repo/include" "$output"/*.cpp -o "$output/$fixture"
            elif test "$target" = go; then
                "$ziran" build "$@" --target=go --pkg main --exe --entry "$fixture:NativeMain" -o "$output" "$input"
            else
                "$ziran" build "$@" --target="$target" --exe --entry "$fixture:NativeMain" -o "$output" "$input"
            fi
            case "$target" in
                c|cpp) "$output/$fixture" ;;
                go) GO111MODULE=off go run "$output"/*.go ;;
                py) python3 "$output" ;;
                rust) CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet --offline --manifest-path "$output/Cargo.toml"
                      "$work/rust-target/debug/ziran_generated" ;;
            esac
        done
    done
done
cmp "$work/source.zib" "$work/saved.zib"
echo 'Grapheme segmentation: all 766 Unicode 17 conformance cases passed on C/C++/Go/Python/Rust from source and saved IR; VM editing cases passed'
