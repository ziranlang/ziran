#!/bin/sh
# Every typed Go helper zi2go can write computes the lanes its reference form does.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
command -v go >/dev/null 2>&1 || exit 0
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" -I"$repo/bootstrap/compiler_services" -I"$repo/bootstrap/compiler_native_names" -I"$repo/include" \
    "$repo/tests/simd_go_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -o "$work/write"
mkdir "$work/check"
"$work/write" "$work/check"
if command -v gofmt >/dev/null 2>&1; then
    test -z "$(gofmt -l "$work/check/ziran_simd.go")" ||
        { gofmt -d "$work/check/ziran_simd.go" | head -40 >&2; exit 1; }
fi
cp "$repo/tests/simd_go_check.go" "$work/check/main.go"
(cd "$work/check" && GO111MODULE=off GOFLAGS= go run .)
