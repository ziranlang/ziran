#!/bin/sh
set -eu
ulimit -c 0
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/vm-concurrent-instances
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS ZIRAN_PROFILE ZIRAN_VM_PROFILE
source=$repo/tests/spec/vm_concurrent_instances_test.zi
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$source"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/vm_concurrent_instances_test.c" "$library" -lm -pthread -o "$work/test"
"$work/test" --metadata-only
rm -f "$work/cold-profile.jsonl"
ZIRAN_PROFILE="$work/cold-profile.jsonl" "$work/test" --metadata-only
for form in source saved; do
    input=$source
    root=$repo/tests/spec
    if test "$form" = saved; then
        input=$work/ir/vm_concurrent_instances_test.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry vm_concurrent_instances_test:Next \
        -o "$work/$form.zib" "$input"
    "$work/test" "$work/$form.zib"
    rm -f "$work/$form-profile.jsonl"
    ZIRAN_PROFILE="$work/$form-profile.jsonl" "$work/test" "$work/$form.zib"
    python3 - "$work/$form-profile.jsonl" <<'PY'
import json, sys
records = [json.loads(line) for line in open(sys.argv[1])]
assert len(records) == 1
record = records[0]
assert record['counters']['concurrent_probe'] == 8 * 600
phase = next(value for value in record['phases'] if value['name'] == 'concurrent_probe_phase')
assert phase['calls'] == 8 * 600
assert record['workspace_allocation_calls'] >= 8 * 600
PY
done
python3 - "$work/cold-profile.jsonl" <<'PY'
import json, sys
record, = [json.loads(line) for line in open(sys.argv[1])]
assert record['counters']['concurrent_probe'] == 8 * 600
assert next(value for value in record['phases'] if value['name'] == 'concurrent_probe_phase')['calls'] == 8 * 600
PY
