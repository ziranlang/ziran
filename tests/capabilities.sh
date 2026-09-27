#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" capabilities --json > "$work/all.json"
python3 - "$work/all.json" <<'PY'
import json
from pathlib import Path
import sys

item = json.loads(Path(sys.argv[1]).read_text())
assert item['schema_version'] == 1
assert item['targets'] == ['c', 'cpp', 'go', 'zib', 'plan9-c']
assert item['source_and_saved_ir'] is True
assert item['automatic_vec_drop'] is True
assert item['aggregate_vec_transfer'] is True
assert item['text_view_local_mutation_check'] is True
assert item['gpu_execution'] == 'cpu_fallback'
assert item['diagnostics_json'] == 'partial'
PY

for target in c cpp go zib plan9-c; do
    "$ziran" capabilities "--target=$target" --json > "$work/$target.json"
    python3 - "$work/$target.json" "$target" <<'PY'
import json
from pathlib import Path
import sys

item = json.loads(Path(sys.argv[1]).read_text())
target = sys.argv[2]
assert item['target'] == target
threads = target in ('c', 'cpp')
assert item['parallel_execution'] == ('threads' if threads else 'serial')
borrowed = target in ('c', 'cpp', 'plan9-c')
assert item['text_view_mutable_bytes'] == ('borrowed' if borrowed else 'snapshot')
assert item['source_and_saved_ir'] is True
assert item['automatic_vec_drop'] is True
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
