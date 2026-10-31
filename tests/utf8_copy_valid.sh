#!/bin/sh
set -eu

ziran=$1
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=${ZIRAN_UTF8_COPY_WORK:-$repo/work/utf8-copy/run}
rm -rf "$work"
mkdir -p "$work"

"$ziran" check --root tests/spec --module-path std \
    tests/spec/utf8_copy_valid.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" \
    tests/spec/utf8_copy_valid.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry utf8_copy_valid:SelfTest -o "$work/source.zx" \
    tests/spec/utf8_copy_valid.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry utf8_copy_valid:SelfTest -o "$work/saved.zx" \
    "$work/ir/utf8_copy_valid.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42

export GOCACHE="$work/go-cache"
for form in source saved; do
    if test "$form" = source; then
        input="$repo/tests/spec/utf8_copy_valid.zi"
        set -- --root tests/spec --module-path std
    else
        input="$work/ir/utf8_copy_valid.zir"
        set -- --root "$work/ir" --module-path "$work/ir"
    fi
    output="$work/$form-c"
    "$ziran" build "$@" --target=c --exe \
        --entry utf8_copy_valid:NativeMain -o "$output" "$input"
    "$output/utf8_copy_valid"
done

echo 'UTF-8 bounded repair-copy passed valid, malformed, boundary and separate-source preservation cases in source/saved VM and C targets'
