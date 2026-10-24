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
assert item['targets'] == ['c', 'go', 'py', 'zx']
assert item['tier'] == {'c': 'primary', 'go': 'secondary', 'py': 'secondary', 'zx': 'primary'}
assert item['parallel_execution']['c'] == 'threads'
assert item['text_view_mutable_bytes']['c'] == 'borrowed'
assert item['parallel_execution']['py'] == 'serial'
assert item['text_view_mutable_bytes']['py'] == 'snapshot'
assert item['source_and_saved_ir'] is True
assert item['target_preflight'] is True
assert item['text_view_opaque_pointer_check'] is True
assert item['automatic_vec_drop'] is True
assert item['heap_allocation'] is True
assert item['aggregate_vec_transfer'] is True
assert item['text_view_local_mutation_check'] is True
assert item['gpu_execution'] == 'cpu_fallback'
manifest = json.loads((Path(sys.argv[2]) / 'tests/numeric_conformance.json').read_text())
numeric = item['numeric_conformance']
assert numeric['schema_version'] == 1
assert numeric['stability'] == 'stable-id'
assert numeric['targets'] == ['c', 'go', 'py', 'zx']
assert numeric['ids'] == [case['id'] for case in manifest['cases']]
assert len(numeric['ids']) == len(set(numeric['ids']))
assert item['diagnostics_json'] == 'partial'
PY

for target in c go py zx; do
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
assert item['parallel_execution'] == ('threads' if target == 'c' else 'serial')
assert 'target_contract' not in item
assert item['tier'] == ('primary' if target in ('c', 'zx') else 'secondary')
assert item['text_view_mutable_bytes'] == ('borrowed' if target == 'c' else 'snapshot')
assert item['source_and_saved_ir'] is True
assert item['target_preflight'] is True
assert item['text_view_opaque_pointer_check'] is True
assert item['automatic_vec_drop'] is True
numeric = item['numeric_conformance']
assert item['heap_allocation'] is True
assert numeric['schema_version'] == 1
assert numeric['stability'] == 'stable-id'
assert numeric['targets'] == [target]
assert numeric['ids'] == [case['id'] for case in manifest['cases']]
PY
done

for target in unknown cpp rust plan9-c; do
    if "$ziran" capabilities "--target=$target" > "$work/out" 2> "$work/err"; then
        echo "capabilities accepted the unknown target $target" >&2
        exit 1
    fi
done
if "$ziran" capabilities --target=c --target=go > "$work/out" 2> "$work/err"; then
    echo 'capabilities accepted conflicting targets' >&2
    exit 1
fi
