#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/cmd/zir" -I"$repo/include" \
    "$repo/tests/body_eligibility_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -o "$work/check"
python3 - "$work/check" <<'PY'
import subprocess,sys
subprocess.run([sys.argv[1]],check=True,timeout=10)
PY
