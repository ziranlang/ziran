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
import json, hashlib
from pathlib import Path
import sys
items = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert items and all(item['schema_version'] == 2 for item in items), items
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
    assert item['cause']['kind']=='type_mismatch' and item['cause']['subject']
    assert (item['expected_type'], item['actual_type']) == (expected, actual), item
for prefix, line in [('argument type mismatch', 1), ('return type mismatch', 2),
                     ('assignment type mismatch', 5)]:
    item = error(prefix)
    related = item['related_graph']['nodes'][1]
    assert related['line'] == line and Path(related['path']).name == 'types.zi', item
    assert related['message'], item
PY
cat > "$work/names.zi" <<'ZI'
Compute :: (value: s64) -> s64 { return value; }
main :: () {
    count := 3;
    print("cuont: %\n", cuont); // cuont in comment
    Comptue(count);
}
ZI
if "$ziran" check --diagnostics=json --root "$work" "$work/names.zi" 2> "$work/names.jsonl"; then
    echo 'unknown names unexpectedly passed' >&2
    exit 1
fi
python3 - "$work/names.jsonl" "$work/names.zi" <<'PY'
import json, hashlib
from pathlib import Path
import sys
items = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
source = Path(sys.argv[2])
lines = source.read_text().splitlines(keepends=True)
assert {item['suggested_name'] for item in items} == {'count', 'Compute'}, items
for item in items:
    assert item['cause']['kind'] in ('unresolved_function','unresolved_name')
    alternative, = item['alternatives']
    edit, = alternative['edits']
    line = edit['line'] - 1
    low, high = edit['column'] - 1, edit['end_column'] - 1
    assert edit['line'] == edit['end_line'], edit
    data=source.read_bytes()
    assert item['semantic_id'].startswith('zi:')
    assert item['document']['sha256']==edit['document_sha256']==hashlib.sha256(data).hexdigest()
    assert data[edit['start_byte']:edit['end_byte']].decode()==edit['original']
    assert lines[line][low:high] == edit['original'], (lines[line], edit)
    lines[line] = lines[line][:low] + edit['replacement'] + lines[line][high:]
source.write_text(''.join(lines))
PY
"$ziran" check --root "$work" "$work/names.zi"
