#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/types.zi" <<'ZI'
Add :: (a: s32, b: s32) -> s32 { return a + b; }
Text :: () -> s32 { return "x"; }
main :: () {
    flag: bool = 3.5;
    total: s32 = Add(1, "two");
    total = "wrong";
}
ZI
if "$ziran" check --diagnostics=json --root "$work" "$work/types.zi" 2> "$work/errors.jsonl"; then
    echo 'type mismatches unexpectedly passed' >&2
    exit 1
fi
python3 - "$work/errors.jsonl" <<'PY'
import json
from pathlib import Path
import sys
items = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert items and all(item['schema_version'] == 1 for item in items), items
def error(prefix):
    matches = [item for item in items if item['message'].startswith(prefix)]
    assert len(matches) == 1, (prefix, items)
    return matches[0]
for prefix, expected, actual in [
    ('initializer type mismatch', 'bool', 'real'),
    ('argument type mismatch', 's32', 'string'),
    ('assignment type mismatch', 's32', 'string'),
    ('return type mismatch', 's32', 'string'),
]:
    item = error(prefix)
    assert (item['expected_type'], item['actual_type']) == (expected, actual), item
for prefix, line in [('argument type mismatch', 1), ('return type mismatch', 2)]:
    item = error(prefix)
    related = item['related'][0]
    assert related['line'] == line and Path(related['path']).name == 'types.zi', item
    assert related['message'], item
PY
cat > "$work/names.zi" <<'ZI'
Compute :: (value: s64) -> s64 { return value; }
main :: () {
    count := 3;
    print("cuont: %\n", cuont);
    Comptue(count);
}
ZI
if "$ziran" check --diagnostics=json --root "$work" "$work/names.zi" 2> "$work/names.jsonl"; then
    echo 'unknown names unexpectedly passed' >&2
    exit 1
fi
python3 - "$work/names.jsonl" "$work/names.zi" <<'PY'
import json
from pathlib import Path
import sys
items = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
source = Path(sys.argv[2])
lines = source.read_text().splitlines(keepends=True)
assert {item['suggested_name'] for item in items} == {'count', 'Compute'}, items
for item in items:
    edit, = item['edits']
    line = edit['line'] - 1
    low, high = edit['column'] - 1, edit['end_column'] - 1
    assert edit['line'] == edit['end_line'], edit
    assert lines[line][low:high] == edit['original'], (lines[line], edit)
    lines[line] = lines[line][:low] + edit['replacement'] + lines[line][high:]
source.write_text(''.join(lines))
PY
"$ziran" check --root "$work" "$work/names.zi"
