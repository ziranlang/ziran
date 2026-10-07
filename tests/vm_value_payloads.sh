#!/bin/sh
set -eu
ulimit -c 0
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-$repo/build/libziran.a}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work="$build/tests/vm-value-payloads"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source="$repo/tests/spec/vm_value_payloads_test.zi"
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$source"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" \
    "$repo/tests/vm_value_payloads_test.c" "$library" -lm -pthread -o "$work/vm-test"
for form in source saved; do
    module=$source; root="$repo/tests/spec"
    if test "$form" = saved; then module="$work/ir/vm_value_payloads_test.zir"; root="$work/ir"; fi
    "$ziran" bundle --root "$root" --entry vm_value_payloads_test:Answer \
        -o "$work/$form.zib" "$module"
    "$work/vm-test" "$work/$form.zib"
    for target in c cpp go; do
        output="$work/$target-$form"
        "$ziran" build --target="$target" --root "$root" -o "$output" "$module"
        if test "$target" = go; then
            cat > "$output/payload_test.go" <<'GO'
package ziran
import "testing"
type payloadHost struct{}
var payloadHandle byte
func (payloadHost) Handle() *byte { return &payloadHandle }
func (payloadHost) RoundTrip(number int64, wide uint64, real float64, text string, tone Tone, handle *byte) Payload {
    return Payload{number, wide, real, text, tone, handle}
}
func (payloadHost) Inspect(values []Payload) int32 {
    values[0].Wide = 9; values[0].Real = 3.5; values[0].Text = "changed"
    return 42
}
func TestPayloads(t *testing.T) {
    SetVmValuePayloadsTestHost(payloadHost{})
    if VmValuePayloadsTest_Answer() != 42 { t.Fatal("host payload round trip") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        else
            cat > "$output/main.$target" <<'C'
#ifdef __cplusplus
#include "vm_value_payloads_test.hpp"
extern "C" {
#else
#include "vm_value_payloads_test.h"
#endif
static int handle;
void *Handle(void) { return &handle; }
Payload RoundTrip(int64_t number, uint64_t wide, double real, String text, Tone tone, void *pointer) {
    Payload value = {number, wide, real, text, tone, pointer};
    return value;
}
int32_t Inspect(Slice values) {
    Payload *items = (Payload *)values.data;
    items[0].wide = 9; items[0].real = 3.5; items[0].text = StringView("changed", 7);
    return 42;
}
#ifdef __cplusplus
}
#endif
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            if test "$target" = c; then
                "${CC:-cc}" -std=c11 -I"$output" -I"$repo/include" "$output"/*.c -o "$output/app"
            else
                "${CXX:-c++}" -std=c++17 -I"$output" -I"$repo/include" "$output"/*.cpp -o "$output/app"
            fi
            "$output/app"
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
