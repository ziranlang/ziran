#!/bin/sh
# Fresh aggregate transfer retains value isolation, active views and borrow guards.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source=$repo/tests/spec/vm_return_storage_test.zi
module=vm_return_storage_test
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$source"
for form in source saved; do
    root=$repo/tests/spec; input=$source
    if test "$form" = saved; then root=$work/ir; input=$root/$module.zir; fi
    "$ziran" bundle --root "$root" --entry "$module:Answer" -o "$work/$form.zx" "$input"
    for run in 1 2; do
        test "$(ZIRAN_VM_PROFILE="$work/$form-$run.jsonl" "$ziran" run "$work/$form.zx")" = 42
        python3 - "$work/$form-$run.jsonl" <<'PY'
import json, sys
sample = json.loads(open(sys.argv[1]).read().splitlines()[-1])
assert sample['complete'] and not sample['failed']
functions = {f['function']: f for f in sample['functions']}
for name, per_call in {'MakeRecord': 0, 'MakeArray': 1, 'MakeArrayLiteral': 1,
                       'RelayRecord': 0, 'RelayArray': 0, 'GuardedRecord': 0,
                       'GuardedArray': 2, 'GuardedPointerArray': 2,
                       'GuardedNested': 4}.items():
    f = functions[name]
    assert f['calls'] > 0 and f['self_allocations'] == per_call * f['calls'], (name, f)
PY
    done
    for target in c cpp; do
        output=$work/$form-$target
        "$ziran" build --target="$target" --root "$root" -o "$output" "$input"
        if test "$target" = c; then
            compiler=${CC:-cc}; extension=c; header=h; standard=c11
        else
            compiler=${CXX:-c++}; extension=cpp; header=hpp; standard=c++17
        fi
        printf '#include "%s.%s"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' \
            "$module" "$header" > "$output/host.$extension"
        "$compiler" -O2 -std="$standard" -I"$output" "$output"/*."$extension" -o "$output/program"
        "$output/program"
    done
done
cmp "$work/source.zx" "$work/saved.zx"
echo 'Fresh record/array returns retain copies for stored values and borrowed storage'
