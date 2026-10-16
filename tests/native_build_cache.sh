#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/cmd/zir-c" \
    "$repo/tests/native_cache_options_test.c" "$repo/cmd/zir-c/zir_c_cache.c" \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -pthread -o "$work/options"
"$work/options"
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/cmd/zir-c" \
    "$repo/tests/native_cache_names_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -lm -pthread -o "$work/names"
"$work/names"
python3 "$repo/tests/native_build_cache_test.py" "$ziran" "$work"
python3 "$repo/tests/native_link_flags_test.py" "$ziran" "$work/link-flags"
