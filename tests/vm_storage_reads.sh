#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/vm-storage-reads
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source=$repo/tests/spec/vm_storage_reads_test.zi
entry=vm_storage_reads_test:main
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    root=$repo/tests/spec
    if test "$form" = saved; then
        input=$work/ir/vm_storage_reads_test.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry "$entry" -o "$work/$form.zx" "$input"
    test "$("$ziran" run "$work/$form.zx")" = 0
    for target in c go; do
        output=$work/$form-$target
        if test "$target" = c; then
            "$ziran" build --target=c --root "$root" --entry "$entry" --exe -o "$output" "$input"
            "$output/vm_storage_reads_test"
        else
            "$ziran" build --target=go --pkg main --root "$root" --entry "$entry" --exe -o "$output" "$input"
            env GO111MODULE=off go run "$output"/*.go
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"
echo 'Stored field, index and global reads preserve numeric widths, unions, mutable views and copy boundaries'
