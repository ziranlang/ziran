#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/compiler-profile-portable.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

check_record() {
    python3 - "$1" <<'PY'
import json
import sys
from pathlib import Path

record, = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert record['schema_version'] == 1 and record['pid'] > 0, record
assert record['profile_elapsed_ms'] > 0 and record['max_rss_kib'] > 0, record
assert record['workspace_allocation_calls'] == 1, record
assert record['workspace_allocation_bytes'] == 4096, record
assert record['counters'] == {'portable': 1}, record
phase, = record['phases']
assert phase['name'] == 'portable' and phase['calls'] == 1, record
assert 0 <= phase['inclusive_ms'] <= record['profile_elapsed_ms'], record
PY
}

"${CC:-cc}" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror \
    -I"$root/include" -I"$root/cmd/zir" -I"$root/bootstrap/compiler_ir" "$root/tests/compiler_profile_portable.c" \
    "$root/cmd/zir/zir_profile.c" -o "$work/profile"
env -u ZIRAN_PROFILE "$work/profile"
ZIRAN_PROFILE="$work/native.jsonl" "$work/profile"
check_record "$work/native.jsonl"

cc=${WIN64_CC:-x86_64-w64-mingw32-gcc}
set --
if test -n "${MCFGTHREADS:-}"; then
    set -- "-L$MCFGTHREADS/lib"
fi
if ! command -v "$cc" >/dev/null 2>&1; then
    echo "compiler_profile_portable: native passed; $cc unavailable"
    exit 0
fi
"$cc" -std=c11 -static -Wall -Wextra -Werror -I"$root/include" -I"$root/cmd/zir" -I"$root/bootstrap/compiler_ir" \
    "$root/tests/compiler_profile_portable.c" "$root/cmd/zir/zir_profile.c" \
    "$@" -o "$work/profile.exe"

echo 'compiler_profile_portable: native profile records and Windows compile/link passed'
