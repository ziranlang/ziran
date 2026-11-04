#!/bin/sh
set -eu
ziran=$1
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/unicode.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
export GOCACHE="$work/go-cache"
export TMPDIR="$work" ZIRAN_NATIVE_DIR="$work" ZIRAN_NATIVE_CACHE=0
for fixture in std_unicode unicode_conformance; do
"$ziran" check --root tests/spec --module-path std "tests/spec/$fixture.zi"
"$ziran" ir --root tests/spec --module-path std -o "$work/$fixture/ir" "tests/spec/$fixture.zi"
for form in source saved; do
    if test "$form" = source; then
        input="$repo/tests/spec/$fixture.zi"
        set -- --root tests/spec --module-path std
    else
        input="$work/$fixture/ir/$fixture.zir"
        set -- --root "$work/$fixture/ir" --module-path "$work/$fixture/ir"
    fi
    "$ziran" bundle "$@" --entry "$fixture:NativeMain" -o "$work/$fixture-$form.zx" "$input"
    test "$("$ziran" run "$work/$fixture-$form.zx")" = 0
    test "$("$(dirname -- "$ziran")/zi2zx-vm" run "$work/$fixture-$form.zx")" = 0
    for target in c go; do
        output="$work/$fixture/$form-$target"
        if test "$target" = go; then
            "$ziran" build "$@" --target=go --pkg main --exe --entry "$fixture:NativeMain" -o "$output" "$input"
            GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build "$@" --target=c --exe --entry "$fixture:NativeMain" -o "$output" "$input"
            "$output/$fixture"
        fi
    done
done
cmp "$work/$fixture-source.zx" "$work/$fixture-saved.zx"
done
echo 'Unicode: UCD assertions plus case/context/titlecase, word/space/decimal, splitting, malformed UTF-8 and ownership passed on C/Go/VM from source and saved IR'
