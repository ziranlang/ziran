#!/bin/sh
set -eu
ulimit -c 0
ziran=${1:?pass the ziran command}
# These check the portable VM itself, which runs where native code cannot.
run_vm="$(dirname "$ziran")/zi2zx-vm"
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_VM_LIB:-"$(dirname "$ziran")/../libziran-vm.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/vm-inline-records
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source=$repo/tests/spec/vm_inline_records_test.zi
"$ziran" ir --root "$repo/tests/spec" --module-path "$repo/std" -o "$work/ir" "$source"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/vm_inline_records_test.c" "$repo/cmd/zir/zir_vm.c" \
    "$repo/cmd/zir/zir_vm_eval.c" "$repo/cmd/zir/zir_vm_run.c" \
    "$repo/cmd/zir/zir_vm_verify.c" "$library" -lm -pthread -o "$work/test"
for form in source saved; do
    input=$source; root=$repo/tests/spec
    if test "$form" = saved; then input=$work/ir/vm_inline_records_test.zir; root=$work/ir; fi
    "$ziran" bundle --root "$root" --module-path "$repo/std" --entry vm_inline_records_test:Answer \
        -o "$work/$form.zx" "$input"
    "$work/test" "$work/$form.zx"
    for entry in Dangling Freed Popped Moved; do
        "$ziran" bundle --root "$root" --module-path "$repo/std" --entry "vm_inline_records_test:$entry" \
            -o "$work/$form-$entry.zx" "$input"
        if "$run_vm" run "$work/$form-$entry.zx" > "$work/$form-$entry.out" 2>&1; then
            echo "read expired record storage: $form $entry" >&2
            exit 1
        fi
        grep -q 'portable execution failed' "$work/$form-$entry.out"
    done
    for target in c go; do
        output=$work/$target-$form
        "$ziran" build --target="$target" --root "$root" --module-path "$repo/std" -o "$output" "$input"
        if test "$target" = go; then
            cat > "$output/inline_test.go" <<'GO'
package ziran
import "testing"
func TestInlineRecords(t *testing.T) {
    if result := VmInlineRecordsTest_Answer(); result != 42 { t.Fatal(result) }
}
GO
            GO111MODULE=off go test "$output"/*.go
        else
            compiler=${CC:-cc}; ext=c; header=h; standard=c11
            printf '#include "vm_inline_records_test.%s"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' "$header" > "$output/host.$ext"
            "$compiler" -std="$standard" -I"$output" -I"$repo/include" "$output"/*."$ext" -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"
