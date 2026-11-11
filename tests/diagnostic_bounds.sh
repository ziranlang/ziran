#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
work="$repo/build/tests/diagnostic-bounds"
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" "$repo/tests/diagnostic_bounds.c" "$(dirname "$ziran")/../libziran.a" -lm -pthread -ldl -o "$work/bounds"
(cd "$work" && ./bounds) 2> "$work/out.jsonl"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- diagnostic-bounds "$work/out.jsonl"
