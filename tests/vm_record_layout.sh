#!/bin/sh
set -eu
ulimit -c 0
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/vm-record-layout
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/vm_record_layout_test.c" "$repo"/cmd/zir/zir_vm*.c \
    "$library" -Wl,--wrap=malloc -Wl,--wrap=calloc -lm -pthread -o "$work/test"
"$work/test"
echo 'Shared reference indices preserve field lookup, borrowed cycles/text/arrays and allocation failure cleanup across table growth'
