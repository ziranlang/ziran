#!/bin/sh
set -eu

ziran=$1
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root tests/spec --module-path std \
    tests/spec/json_string_value_test.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" \
    tests/spec/json_string_value_test.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry json_string_value_test:SelfTest -o "$work/source.zib" \
    tests/spec/json_string_value_test.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry json_string_value_test:SelfTest -o "$work/saved.zib" \
    "$work/ir/json_string_value_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42

# The offset buffer and empty-slice count path must also work in native output.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
for form in source saved; do
    input=tests/spec/json_string_value_test.zi
    if test "$form" = saved; then input="$work/ir/json_string_value_test.zir"; fi
    output="$work/$form-c"
    "$ziran" build --target=c --exe --entry json_string_value_test:NativeMain \
        --root tests/spec --module-path std -o "$output" "$input"
    "$output/json_string_value_test"
    output="$work/$form-cpp"
    "$ziran" build --target=cpp --no-main --root tests/spec --module-path std -o "$output" "$input"
    cat > "$output/main.cpp" <<'CPP'
#include "json_string_value_test.hpp"
int main() { return NativeMain(); }
CPP
    "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
    "$output/app"
    output="$work/$form-go"
    "$ziran" build --target=go --pkg main --root tests/spec --module-path std -o "$output" "$input"
    cat > "$output/main.go" <<'GO'
package main
func main() { if JsonStringValueTest_NativeMain() != 0 { panic("JSON owned string decoding differs") } }
GO
    GO111MODULE=off go run "$output"/*.go
    output="$work/$form-py"
    "$ziran" build --target=py --exe --entry json_string_value_test:NativeMain \
        --root tests/spec --module-path std -o "$output" "$input"
    python3 "$output"
    if command -v cargo >/dev/null 2>&1; then
        output="$work/$form-rust"
        "$ziran" build --target=rust --exe --entry json_string_value_test:NativeMain \
            --root tests/spec --module-path std -o "$output" "$input"
        CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet --offline \
            --manifest-path "$output/Cargo.toml"
        "$work/rust-target/debug/ziran_generated"
    fi
done
echo 'JSON owned string decoding passed source, saved IR, portable VM and C/C++/Go/Python execution; Rust checked when cargo is installed'
