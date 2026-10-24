#!/bin/sh
set -eu
ulimit -c 0
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-$repo/build/libziran.a}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work="$build/tests/vm-packed-arrays"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source="$repo/tests/spec/vm_packed_arrays_test.zi"
"$ziran" ir --root "$repo/tests/spec" --module-path "$repo/std" -o "$work/ir" "$source"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/vm_packed_arrays_test.c" "$library" -lm -pthread -o "$work/vm-test"
for form in source saved; do
    module=$source; root="$repo/tests/spec"
    if test "$form" = saved; then module="$work/ir/vm_packed_arrays_test.zir"; root="$work/ir"; fi
    "$ziran" bundle --root "$root" --module-path "$repo/std" --entry vm_packed_arrays_test:Answer \
        -o "$work/$form.zx" "$module"
    "$work/vm-test" "$work/$form.zx"
    "$ziran" bundle --root "$root" --module-path "$repo/std" --entry vm_packed_arrays_test:Capacity \
        -o "$work/$form-Capacity.zx" "$module"
    "$work/vm-test" "$work/$form-Capacity.zx"
    for entry in PopInvalid ClearInvalid GrowthInvalid; do
        "$ziran" bundle --root "$root" --module-path "$repo/std" --entry "vm_packed_arrays_test:$entry" \
            -o "$work/$form-$entry.zx" "$module"
        if "$ziran" run "$work/$form-$entry.zx" > "$work/$form-$entry.out" 2>&1; then
            echo "read invalid vector storage: $form $entry" >&2
            exit 1
        fi
        grep -q 'portable execution failed' "$work/$form-$entry.out"
    done
    for target in c go; do
        output="$work/$target-$form"
        "$ziran" build --target="$target" --root "$root" --module-path "$repo/std" -o "$output" "$module"
        if test "$target" = go; then
            cat > "$output/packed_test.go" <<'GO'
package ziran
import "testing"
type packedHost struct{}
func (packedHost) Measure() int64 { return 0 }
func TestPackedArrays(t *testing.T) {
    SetVmPackedArraysTestHost(packedHost{})
    if result := VmPackedArraysTest_Answer(); result != 42 { t.Fatal(result) }
}
GO
            GO111MODULE=off go test "$output"/*.go
        else
            cat > "$output/main.c" <<'C'
#include "vm_packed_arrays_test.h"
int64_t Measure(void) { return 0; }
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$output" -I"$repo/include" "$output"/*.c -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"
