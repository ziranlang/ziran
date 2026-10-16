#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/vm-function-plans
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source=$work/vm_function_plans_test.zi
cp "$repo/tests/spec/vm_function_plans_test.zi" "$source"
# Force control-flow targets beyond a byte without an enormous source fixture.
{
    echo 'Large :: (seed: s64, limit: s32, choose: bool) -> s64 {'
    echo 'total := seed; i: s32; while i < limit {'
    count=0
    while test "$count" -lt 40; do
        echo 'if choose { total += seed + 3 } else { total ^= seed - 7 }'
        count=$((count + 1))
    done
    echo 'i += 1'
    echo '}'
    echo 'return total'
    echo '}'
} >> "$source"
"$ziran" ir --root "$work" -o "$work/ir" "$source"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/vm_function_plans_test.c" "$repo/cmd/zir/zir_vm.c" \
    "$repo/cmd/zir/zir_vm_verify.c" "$library" -lm -pthread -o "$work/test"
for form in source saved; do
    input=$source
    root=$work
    if test "$form" = saved; then input=$work/ir/vm_function_plans_test.zir; root=$work/ir; fi
    "$ziran" bundle --root "$root" --entry vm_function_plans_test:Answer \
        -o "$work/$form.zib" "$input"
    "$work/test" "$work/$form.zib"
done
cmp "$work/source.zib" "$work/saved.zib"
