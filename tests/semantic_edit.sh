#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
work="$repo/build/tests/semantic-edit"
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp" ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/semantic_edit_cases.zi" -- "$ziran" "$work"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" "$repo/tests/semantic_edit_runtime.c" "$repo/cmd/zir/zir_edit.c" "$(dirname "$ziran")/../libziran.a" -Wl,--wrap=renameat -Wl,--wrap=fstat -lm -pthread -ldl -o "$work/runtime"
(cd "$work/source" && ../runtime) 2> "$work/runtime.err"
test -z "$(find "$work/source" -maxdepth 1 -name '.zi-edit-*' -print -quit)"
