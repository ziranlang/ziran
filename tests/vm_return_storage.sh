#!/bin/sh
# Fresh aggregate transfer retains value isolation, active views and borrow guards.
set -eu
ziran=${1:?pass the ziran command}
# These check the portable VM itself, which runs where native code cannot.
run_vm="$(dirname "$ziran")/zi2zx"
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
        test "$(ZIRAN_VM_PROFILE="$work/$form-$run.jsonl" "$run_vm" run --vm "$work/$form.zx")" = 42
        "$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- vm-returns "$work/$form-$run.jsonl"
    done
    output=$work/$form-c
    "$ziran" build --target=c --root "$root" -o "$output" "$input"
    compiler=${CC:-cc}; extension=c; header=h; standard=c11
    printf '#include "%s.%s"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' \
        "$module" "$header" > "$output/host.$extension"
    "$compiler" -O2 -std="$standard" -I"$output" "$output"/*."$extension" -o "$output/program"
    "$output/program"
done
cmp "$work/source.zx" "$work/saved.zx"
echo 'Fresh record/array returns retain copies for stored values and borrowed storage'
