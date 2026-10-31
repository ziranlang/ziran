#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/outside_loop.zi" <<'ZI'
Answer :: () -> s32 {
    break
    return 42
}
ZI
cat > "$work/orphan_else.zi" <<'ZI'
Answer :: () -> s32 {
    else
        return 42
    return 0
}
ZI
cat > "$work/deferred_return.zi" <<'ZI'
Answer :: () -> s32 {
    defer return 1
    return 42
}
ZI

for name in outside_loop orphan_else deferred_return; do
    if "$ziran" check --diagnostics=json --root "$work" \
        "$work/$name.zi" > "$work/$name.out" 2> "$work/$name.jsonl"; then
        echo "$name unexpectedly passed" >&2
        exit 1
    fi
    python3 - "$work/$name.jsonl" "$work/$name.zi" <<'PY'
import json
from pathlib import Path
import sys

lines = Path(sys.argv[1]).read_text().splitlines()
assert lines, 'missing diagnostics'
for line in lines:
    item = json.loads(line)
    assert item['severity'] == 'error', item
    assert item['code'].startswith('check.'), item
    assert Path(item['path']).name == Path(sys.argv[2]).name, item
    assert item['line'] > 0 and item['column'] > 0, item
    assert item['end_line'] >= item['line'], item
PY
done

sh "$(dirname "$0")/diagnostic_android.sh"
