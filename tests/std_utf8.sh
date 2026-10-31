#!/bin/sh
set -eu

ziran=$1
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root tests/spec --module-path std tests/spec/std_utf8.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" \
    tests/spec/std_utf8.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry std_utf8:SelfTest -o "$work/source.zx" \
    tests/spec/std_utf8.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry std_utf8:SelfTest -o "$work/saved.zx" \
    "$work/ir/std_utf8.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export GOCACHE="$work/go-cache"
for form in source saved; do
    if test "$form" = source; then
        input="$repo/tests/spec/std_utf8.zi"
        set -- --root tests/spec --module-path std
    else
        input="$work/ir/std_utf8.zir"
        set -- --root "$work/ir" --module-path "$work/ir"
    fi
    output="$work/$form-c"
    "$ziran" build "$@" --target=c --exe --entry std_utf8:NativeMain -o "$output" "$input"
    "$output/std_utf8"
    output="$work/$form-go"
    "$ziran" build "$@" --target=go --pkg main --entry std_utf8:NativeMain -o "$output" "$input"
    printf 'package main\nfunc main() { if StdUtf8_NativeMain() != 0 { panic("UTF-8 validity") } }\n' > "$output/driver.go"
    GO111MODULE=off go run "$output"/*.go
done
echo 'UTF-8 validity passed exhaustive one/two-byte cases, scalar boundaries and malformed tails in source/saved IR, VM and native/portable targets'
