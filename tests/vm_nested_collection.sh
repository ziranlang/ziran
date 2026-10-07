#!/bin/sh
set -eu
ulimit -c 0
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-$repo/build/libziran.a}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work="$build/tests/vm-frame-memory"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source="$repo/tests/spec/vm_nested_collection_test.zi"
"$ziran" ir --root "$repo/tests/spec" --module-path "$repo/std" -o "$work/ir" "$source"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/vm_nested_collection_test.c" "$library" \
    -lm -pthread -o "$work/vm-test"
for form in source saved; do
    module=$source; root="$repo/tests/spec"
    if test "$form" = saved; then module="$work/ir/vm_nested_collection_test.zir"; root="$work/ir"; fi
    for entry in main Memory; do
        "$ziran" bundle --root "$root" --module-path "$repo/std" --entry "vm_nested_collection_test:$entry" \
            -o "$work/$form-$entry.zib" "$module"
        "$work/vm-test" "$work/$form-$entry.zib"
    done
done
