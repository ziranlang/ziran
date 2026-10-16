#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 "$repo/tests/native_build_parallel_test.py" "$ziran" "$work"
mkdir "$work/owned"
wrap=
if test "$(uname -s)" = Linux; then wrap=-Wl,--wrap=syscall; fi
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/cmd/zir-c" \
    "$repo/tests/native_build_children_test.c" "$repo/cmd/zir-c/zir_c_build.c" "$repo/cmd/zir-c/zir_c_cache.c" \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -pthread $wrap -o "$work/children"
ZIRAN_PROFILE="$work/children.jsonl" "$work/children" "$work/owned"
python3 - "$work/children.jsonl" <<'PY'
import json,sys
sample,=map(json.loads,open(sys.argv[1]))
assert sample['counters'].get('native_process_event',0)==0
assert sample['counters']['native_process_poll']==2
PY
