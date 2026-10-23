#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
for mode in memory temporary; do
    define=
    if test "$mode" = temporary; then define=-DTEST_TEXT_STREAM_TMPFILE=1; fi
    "${CC:-cc}" ${VM_CFLAGS:-} -O2 -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror \
        $define -I"$repo/cmd/zir" "$repo/tests/text_stream_test.c" -o "$work/check"
    (cd "$work" && ./check)
done
