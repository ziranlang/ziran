#!/bin/sh
set -eu

ziran=$1
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/utf8.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
export TMPDIR="$work" ZIRAN_NATIVE_DIR="$work" ZIRAN_NATIVE_CACHE=0

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
test "$("$(dirname -- "$ziran")/zi2zx-vm" run "$work/source.zx")" = 42
test "$("$(dirname -- "$ziran")/zi2zx-vm" run "$work/saved.zx")" = 42

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
echo 'UTF-8 validity and maximal-prefix replacement passed scalar boundaries, malformed tails, independent storage and source/saved IR on VM/C/Go'
