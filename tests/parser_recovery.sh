#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
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
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/parser_recovery_test.c" "${ZIRAN_LIB:-$(dirname "$ziran")/../libziran.a}" \
    -lm -pthread -o "$work/test"
python3 - "$work/test" "$work/bad.zi" "$work" <<'PY'
import subprocess, sys
result = subprocess.run(sys.argv[1:], capture_output=True, text=True, timeout=30)
if result.returncode:
    sys.stderr.write(result.stdout + result.stderr)
    raise SystemExit(result.returncode)
assert 'recover' in result.stdout, result.stdout
print(result.stdout, end='')
PY
