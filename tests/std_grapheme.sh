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
        entry=SelfTest; expected=42
        if test "$fixture" = grapheme_conformance; then entry=NativeMain; expected=0; fi
        "$ziran" bundle "$@" --entry "$fixture:$entry" -o "$work/$fixture-$form.zx" "$input"
        test "$("$ziran" run "$work/$fixture-$form.zx")" = "$expected"
        for target in c go py; do
            output="$work/$fixture/$form-$target"
            if test "$target" = go; then
                "$ziran" build "$@" --target=go --pkg main --exe --entry "$fixture:NativeMain" -o "$output" "$input"
            else
                "$ziran" build "$@" --target="$target" --exe --entry "$fixture:NativeMain" -o "$output" "$input"
            fi
            case "$target" in
                c) "$output/$fixture"
                       symbols=GraphemeNext
                       # The conformance fixture only calls Next; its Count
                       # export is removed with other unreachable code.
                       if test "$fixture" = std_grapheme; then
                           symbols="$symbols GraphemeCount"
                       fi
                       for symbol in $symbols; do
                           if ! nm "$output/$fixture" | grep -Eq " T $symbol$"; then
                               echo "missing named $target export: $symbol" >&2
                               exit 1
                           fi
                       done ;;
                go) GO111MODULE=off go run "$output"/*.go ;;
                py) python3 "$output" ;;
            esac
        done
    done
    cmp "$work/$fixture-source.zx" "$work/$fixture-saved.zx"
done
echo 'Grapheme segmentation: all 766 Unicode 17 conformance cases and VM editing cases passed from source and saved IR on VM/C/Go/Python'
