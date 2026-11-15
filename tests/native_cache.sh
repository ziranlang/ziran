#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/native-cache-host.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
module_dir=$(mktemp -d "$work/module.XXXXXX")
"$ziran" build --target=c --no-main --entry native_cache_module_cases:main --root "$repo/tests" --module-path "$repo/cmd" \
    -o "$module_dir/test" "$repo/tests/native_cache_module_cases.zi" 2>/dev/null || true
printf 'int main(void) { extern int native_cache_module_cases_main(void); return native_cache_module_cases_main(); }\n' > "$work/module_main.c"
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/include" -I"$module_dir/test" \
    "$module_dir/test/native_cache_module_cases.c" \
    "$work/module_main.c" \
    $(find "$module_dir/test" -maxdepth 1 -name '*.c' ! -name 'native_cache_module_cases.c' ! -name 'ziran_exe_main.c') \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -pthread -o "$module_dir/binary"
"$module_dir/binary"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/native_build_cache_cases.zi" -- "$ziran"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/native_link_flags_cases.zi" -- "$ziran"
