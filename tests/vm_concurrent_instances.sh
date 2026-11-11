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
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" \
    -I"$repo/bootstrap/compiler_text" \
    -I"$repo/bootstrap/compiler_type" \
    "$repo/tests/vm_concurrent_instances_test.c" "$repo"/cmd/zir/zir_vm*.c \
    "$repo/cmd/zir/zir.c" "$repo/cmd/zir/zir_text.c" "$repo/cmd/zir/zir_diagnostic.c" \
    "$library" -lm -pthread -o "$work/test"
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
        -o "$work/$form.zx" "$input"
    "$work/test" "$work/$form.zx"
    "$work/test" "$work/$form.zx" --bundle
    rm -f "$work/$form-profile.jsonl"
    ZIRAN_PROFILE="$work/$form-profile.jsonl" "$work/test" "$work/$form.zx"
    "$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- concurrent-profile "$work/$form-profile.jsonl"
done
ZIRAN_DIAGNOSTICS=json "$work/test" "$work/source.zx" 2>"$work/diagnostics.jsonl"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- concurrent-diagnostics "$work/diagnostics.jsonl"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- concurrent-cold "$work/cold-profile.jsonl"
