#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/parser-recovery-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
printf 'Answer :: () {\000}\n' > "$work/bad.zi"
cat > "$work/library.zi" <<'ZI'
Value :: 42;
Read :: () -> s32 { return 42 }
ZI
cat > "$work/imports.zi" <<'ZI'
#if Lib.Value == 42 && Lib.Read() == 42 {
Selected :: 42;
}
Lib :: #import "library";
Answer :: () -> s32 {
ZI
"$ziran" ir --root "$work" -o "$work/saved-imports" "$work/library.zi"
cp "$work/imports.zi" "$work/saved-imports/imports.zi"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" \
    "$repo/tests/parser_recovery_test.c" "${ZIRAN_LIB:-$(dirname "$ziran")/../libziran.a}" \
    -lm -pthread -o "$work/test"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/native_inline_cases.zi" -- "$ziran" "$work" recovery-probe "$work/test" "$work/bad.zi"
