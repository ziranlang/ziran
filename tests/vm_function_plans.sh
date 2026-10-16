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
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$source"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/vm_function_plans_test.c" "$repo/cmd/zir/zir_vm.c" \
    "$repo/cmd/zir/zir_vm_verify.c" "$library" -lm -pthread -o "$work/test"
for form in source saved; do
    input=$source
    root=$work
    if test "$form" = saved; then input=$work/ir/vm_function_plans_test.zir; root=$work/ir; fi
    "$ziran" bundle --root "$root" --module-path "$repo/std" --entry vm_function_plans_test:Answer \
        -o "$work/$form.zib" "$input"
    "$work/test" "$work/$form.zib"
    for target in c cpp go; do
        output=$work/$target-$form
        "$ziran" build --target="$target" --root "$root" --module-path "$repo/std" -o "$output" "$input"
        if test "$target" = go; then
            cat > "$output/plans_test.go" <<'GO'
package ziran
import "testing"
func TestProcedureCalls(t *testing.T) {
    if result := VmFunctionPlansTest_Answer(); result != 42 { t.Fatal(result) }
}
GO
            GO111MODULE=off go test "$output"/*.go
        else
            if test "$target" = c; then compiler=${CC:-cc}; ext=c; header=h; standard=c11
            else compiler=${CXX:-c++}; ext=cpp; header=hpp; standard=c++17; fi
            printf '#include "vm_function_plans_test.%s"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' "$header" > "$output/host.$ext"
            "$compiler" -std="$standard" -I"$output" -I"$repo/include" "$output"/*."$ext" -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
