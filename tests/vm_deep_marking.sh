#!/bin/sh
set -eu
ulimit -c 0
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/vm-deep-marking
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source=$repo/tests/spec/vm_deep_marking_test.zi
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$source"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/vm_deep_marking_test.c" "$repo"/cmd/zir/zir_vm*.c \
    "$library" -Wl,--wrap=malloc -Wl,--wrap=realloc -lm -pthread -o "$work/test"
for form in source saved; do
    input=$source
    root=$repo/tests/spec
    if test "$form" = saved; then
        input=$work/ir/vm_deep_marking_test.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry vm_deep_marking_test:Answer \
        -o "$work/$form.zib" "$input"
    "$work/test" "$work/$form.zib"
done
cmp "$work/source.zib" "$work/saved.zib"
echo 'Deep heap-pointer chains survive repeated collections and release their storage'
