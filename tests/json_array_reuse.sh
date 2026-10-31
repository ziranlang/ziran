#!/bin/sh
set -eu

ziran=$1
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root tests/spec --module-path std \
    tests/spec/json_array_reuse_test.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" \
    tests/spec/json_array_reuse_test.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry json_array_reuse_test:SelfTest -o "$work/source.zx" \
    tests/spec/json_array_reuse_test.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry json_array_reuse_test:SelfTest -o "$work/saved.zx" \
    "$work/ir/json_array_reuse_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42

# The offset buffer and empty-slice count path must also work in native output.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
for form in source saved; do
    input=tests/spec/json_array_reuse_test.zi
    if test "$form" = saved; then input="$work/ir/json_array_reuse_test.zir"; fi
    output="$work/$form-c"
    "$ziran" build --target=c --exe --entry json_array_reuse_test:NativeMain \
        --root tests/spec --module-path std -o "$output" "$input"
    "$output/json_array_reuse_test"
    output="$work/$form-go"
    "$ziran" build --target=go --pkg main --root tests/spec --module-path std -o "$output" "$input"
    cat > "$output/main.go" <<'GO'
package main
func main() { if JsonArrayReuseTest_NativeMain() != 0 { panic("JSON array reuse differs") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
echo 'JSON array reuse passed source, saved IR, portable VM and C/Go execution'
