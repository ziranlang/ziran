#!/bin/sh
set -eu
ulimit -c 0
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/symbol-lookup
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/symbol_lookup_test.c" \
    "$library" -lm -pthread -o "$work/test"
ZIRAN_VERIFY_SYMBOL_LOOKUPS=1 "$work/test"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/reachable_types_test.c" "$library" -lm -pthread -o "$work/reachable"
ZIRAN_VERIFY_SYMBOL_LOOKUPS=1 "$work/reachable"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/type_exports_test.c" "$library" -lm -pthread -o "$work/exports"
ZIRAN_VERIFY_SYMBOL_LOOKUPS=1 "$work/exports"
echo 'Type and procedure resolution preserve file scopes, imports, ambiguity, mutation and independent concurrent programs'
