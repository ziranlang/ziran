#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
work="$repo/build/tests/diagnostic-bounds"
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" "$repo/tests/diagnostic_bounds.c" "$(dirname "$ziran")/../libziran.a" -lm -pthread -ldl -o "$work/bounds"
(cd "$work" && ./bounds) 2> "$work/out.jsonl"
python3 - "$work/out.jsonl" <<'PY'
import json,pathlib,sys
lines=pathlib.Path(sys.argv[1]).read_bytes().splitlines(keepends=True)
assert len(lines)==3 and all(len(line)<=32768 for line in lines)
a,b,stale=map(json.loads,lines)
assert a['schema_version']==2 and a['truncation']=={'message':True,'metadata':True}
assert len(a['message'])==2048 and a['actual_type']=='s32'
assert 'expected_type' not in a and a['related_graph']=={'nodes':[],'edges':[]} and not a['alternatives']
assert stale['suggested_name']=='count' and not stale['alternatives']
assert stale['document'] is not None
assert b['message']=='🙂é' and b['truncation']=={'message':False,'metadata':False}
PY
