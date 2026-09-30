#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" capabilities --json > "$work/all.json"
python3 - "$work/all.json" "$repo" <<'PY'
import json
from pathlib import Path
import sys

item = json.loads(Path(sys.argv[1]).read_text())
assert item['schema_version'] == 1
assert item['targets'] == ['c', 'cpp', 'go', 'rust', 'py', 'zib', 'plan9-c']
assert item['tier'] == {'c': 'primary', 'cpp': 'secondary', 'go': 'secondary',
                        'rust': 'secondary', 'py': 'secondary', 'zib': 'primary',
                        'plan9-c': 'experimental'}
assert item['parallel_execution']['rust'] == 'serial'
assert item['text_view_mutable_bytes']['rust'] == 'borrowed'
assert item['parallel_execution']['py'] == 'serial'
assert item['text_view_mutable_bytes']['py'] == 'snapshot'
assert item['source_and_saved_ir'] is True
assert item['automatic_vec_drop'] is True
assert item['aggregate_vec_transfer'] is True
assert item['text_view_local_mutation_check'] is True
assert item['gpu_execution'] == 'cpu_fallback'
manifest = json.loads((Path(sys.argv[2]) / 'tests/numeric_conformance.json').read_text())
numeric = item['numeric_conformance']
assert numeric['schema_version'] == 1
assert numeric['stability'] == 'stable-id'
assert numeric['targets'] == ['c', 'cpp', 'go', 'rust', 'py', 'zib']
assert numeric['ids'] == [case['id'] for case in manifest['cases']]
assert len(numeric['ids']) == len(set(numeric['ids']))
assert item['diagnostics_json'] == 'partial'
PY

for target in c cpp go rust py zib plan9-c; do
    "$ziran" capabilities "--target=$target" --json > "$work/$target.json"
    python3 - "$work/$target.json" "$target" \
            "$repo/tests/numeric_conformance.json" <<'PY'
import json
from pathlib import Path
import sys

item = json.loads(Path(sys.argv[1]).read_text())
target = sys.argv[2]
manifest = json.loads(Path(sys.argv[3]).read_text())
assert item['target'] == target
threads = target in ('c', 'cpp')
assert item['parallel_execution'] == ('threads' if threads else 'serial')
if target in ('rust', 'py'):
    assert 'target_contract' not in item
borrowed = target in ('c', 'cpp', 'rust', 'plan9-c')
assert item['tier'] == ('primary' if target in ('c', 'zib') else
                        'experimental' if target == 'plan9-c' else 'secondary')
assert item['text_view_mutable_bytes'] == ('borrowed' if borrowed else 'snapshot')
assert item['source_and_saved_ir'] is True
assert item['automatic_vec_drop'] is True
numeric = item['numeric_conformance']
assert numeric['schema_version'] == 1
assert numeric['stability'] == 'stable-id'
if target == 'plan9-c':
    assert numeric['targets'] == []
    assert numeric['ids'] == []
else:
    assert numeric['targets'] == [target]
    assert numeric['ids'] == [case['id'] for case in manifest['cases']]
if target == 'plan9-c':
    assert item['target_contract'] == 'experimental-post-pass'
    assert item['native_os'] == 'plan9'
    assert item['native_libc'] == 'plan9'
    assert item['architecture'] == '386'
    assert item['dialect'] == 'plan9-c88'
PY
done

if "$ziran" capabilities --target=unknown > "$work/out" 2> "$work/err"; then
    echo 'capabilities accepted an unknown target' >&2
    exit 1
fi
if "$ziran" capabilities --target=c --target=go > "$work/out" 2> "$work/err"; then
    echo 'capabilities accepted conflicting targets' >&2
    exit 1
fi
