#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/main.zi" <<'ZI'
Point :: struct { x: s32; y: s32; }
Answer :: () -> s32 { point: Point; point.x = 42; return point.x; }
ZI
ZIRAN_PROFILE="$work/profile.jsonl" "$ziran" check --diagnostics=json --root "$work" "$work/main.zi" > "$work/out" 2> "$work/err"
test ! -s "$work/err"
ZIRAN_PROFILE="$work/profile.jsonl" "$ziran" build --target=c --root "$work" -o "$work/c" "$work/main.zi"
python3 - "$work/profile.jsonl" <<'PY'
import json
from pathlib import Path
import sys
records = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert len(records) == 2, records
for record in records:
    assert record['schema_version'] == 1 and record['pid'] > 0, record
    assert record['profile_elapsed_ms'] > 0 and record['max_rss_kib'] > 0, record
    assert record['workspace_allocation_calls'] > 0 and record['workspace_allocation_bytes'] > 0, record
    phases = {item['name']: item for item in record['phases']}
    assert {'parse', 'load', 'check'} <= phases.keys(), record
    assert all(item['calls'] > 0 and item['inclusive_ms'] >= 0 for item in phases.values()), record
    assert record['counters']['record_field_parse'] > 0, record
assert 'emit.c' in {item['name'] for item in records[1]['phases']}, records
PY
if ZIRAN_PROFILE="$work/missing/profile.jsonl" "$ziran" check --diagnostics=json --root "$work" "$work/main.zi" 2> "$work/missing.err"; then
    echo 'an unwritable profile unexpectedly passed' >&2
    exit 1
fi
python3 - "$work/missing.err" <<'PY'
import json
from pathlib import Path
import sys
item, = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert item['code'] == 'zir.output', item
PY
