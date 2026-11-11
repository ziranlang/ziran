#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/native-cache-host.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" -I"$repo/cmd/zir-c" \
    "$repo/tests/native_cache_options_test.c" "$repo/cmd/zir-c/zir_c_cache.c" \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -pthread -o "$work/options"
"$work/options"
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" -I"$repo/cmd/zir-c" \
    "$repo/tests/native_cache_names_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -lm -pthread -o "$work/names"
"$work/names"
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" -I"$repo/cmd/zir-c" \
    "$repo/tests/native_cache_probe_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -lm -pthread -o "$work/probe"
"$work/probe"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/native_build_cache_cases.zi" -- "$ziran"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/native_link_flags_cases.zi" -- "$ziran"
