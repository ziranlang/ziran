#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root tests/spec --module-path std \
    tests/spec/json_scan_test.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" \
    tests/spec/json_scan_test.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry json_scan_test:SelfTest -o "$work/source.zib" \
    tests/spec/json_scan_test.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry json_scan_test:SelfTest -o "$work/saved.zib" \
    "$work/ir/json_scan_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42

# The offset buffer and empty-slice count path must also work in native output.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
for form in source saved; do
    input=tests/spec/json_scan_test.zi
    if test "$form" = saved; then input="$work/ir/json_scan_test.zir"; fi
    output="$work/$form-c"
    "$ziran" build --target=c --exe --entry json_scan_test:NativeMain \
        --root tests/spec --module-path std -o "$output" "$input"
    "$output/json_scan_test"
    output="$work/$form-cpp"
    "$ziran" build --target=cpp --no-main --root tests/spec --module-path std -o "$output" "$input"
    cat > "$output/main.cpp" <<'CPP'
#include "json_scan_test.hpp"
int main() { return NativeMain(); }
CPP
    "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
    "$output/app"
    output="$work/$form-go"
    "$ziran" build --target=go --pkg main --root tests/spec --module-path std -o "$output" "$input"
    cat > "$output/main.go" <<'GO'
package main
func main() { if JsonScanTest_NativeMain() != 0 { panic("JSON array positions differ") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
echo 'JSON scanning passed source, saved IR, portable VM and C/C++/Go execution'
