#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/vm-literal-cache
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
# Exercise more literal sites than the bounded cache can retain at once.
# Every expected result is also built dynamically, including NUL and UTF-8.
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" vm-literals
source=$work/vm_literal_cache_test.zi
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$source"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" \
    "$repo/tests/vm_literal_cache_test.c" "$repo"/cmd/zir/zir_vm*.c \
    "$library" -lm -pthread -o "$work/test"
for form in source saved; do
    input=$source
    root=$work
    if test "$form" = saved; then
        input=$work/ir/vm_literal_cache_test.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --module-path "$repo/std" --entry vm_literal_cache_test:main \
        -o "$work/$form.zx" "$input"
    "$work/test" "$work/$form.zx"
done
