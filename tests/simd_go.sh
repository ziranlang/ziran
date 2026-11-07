#!/bin/sh
# The generated std/simd tables match scripts/simd_header.py, and every typed
# Go helper zi2go can write computes the lanes its reference form does.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 "$repo/scripts/simd_header.py" | cmp -s - "$repo/include/ziran_simd_ops.h" ||
    { echo "include/ziran_simd_ops.h differs from scripts/simd_header.py" >&2; exit 1; }
python3 "$repo/scripts/simd_header.py" --types | cmp -s - "$repo/include/ziran_simd_types.h" ||
    { echo "include/ziran_simd_types.h differs from scripts/simd_header.py --types" >&2; exit 1; }
python3 "$repo/scripts/simd_header.py" --go | cmp -s - "$repo/cmd/zir/zir_simd_go_bodies.h" ||
    { echo "cmd/zir/zir_simd_go_bodies.h differs from scripts/simd_header.py --go" >&2; exit 1; }
command -v go >/dev/null 2>&1 || exit 0
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/cmd/zir" -I"$repo/include" \
    "$repo/tests/simd_go_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -o "$work/write"
mkdir "$work/check"
"$work/write" "$work/check"
if command -v gofmt >/dev/null 2>&1; then
    test -z "$(gofmt -l "$work/check/ziran_simd.go")" ||
        { gofmt -d "$work/check/ziran_simd.go" | head -40 >&2; exit 1; }
fi
cp "$repo/tests/simd_go_check.go" "$work/check/main.go"
(cd "$work/check" && GO111MODULE=off GOFLAGS= go run .)
