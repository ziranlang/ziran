#!/bin/sh
set -eu
ulimit -c 0
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source=$repo/tests/spec/host_strings.zi
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$source"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/host_strings_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -lm -pthread -o "$work/test"
for form in source saved; do
    module=$source; root=$repo/tests/spec
    if [ "$form" = saved ]; then module=$work/ir/host_strings.zir; root=$work/ir; fi
    "$ziran" bundle --root "$root" --entry host_strings:Answer \
        -o "$work/$form.zx" "$module"
    "$work/test" "$work/$form.zx"
done
cmp "$work/source.zx" "$work/saved.zx"
