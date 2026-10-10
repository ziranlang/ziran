#!/bin/sh
set -eu
ulimit -c 0
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/vm-metadata-cache
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cat > "$work/second.zi" <<'ZI'
#scope_export
first_value: s32 = 101;
second_value: s32 = 102;
third_value: s32 = 103;
One :: () -> s32 { return 101 }
Two :: () -> s32 { return 102 }
Three :: () -> s32 { return 103 }
ZI
cat > "$work/first.zi" <<'ZI'
Other :: #import "second";
first_value: s32 = 1;
second_value: s32 = 2;
third_value: s32 = 3;
One :: () -> s32 { return 1 }
Two :: () -> s32 { return 2 }
Three :: () -> s32 { return 3 }
#program_export
Answer :: () -> s32 {
    return One() + Two() + Three() + first_value + second_value + third_value +
        Other.One() + Other.Two() + Other.Three() +
        Other.first_value + Other.second_value + Other.third_value
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/first.zi" "$work/second.zi"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/vm_metadata_cache_test.c" "$repo"/cmd/zir/zir_vm*.c \
    "$library" -Wl,--wrap=calloc -lm -pthread -o "$work/test"
for form in source saved; do
    root=$work
    if test "$form" = saved; then root=$work/ir; fi
    suffix=zi
    if test "$form" = saved; then suffix=zir; fi
    "$ziran" bundle --root "$root" --entry first:Answer -o "$work/$form.zib" \
        "$root/first.$suffix" "$root/second.$suffix"
    test "$("$ziran" run "$work/$form.zib")" = 624
    "$work/test" "$work/$form.zib"
done
cmp "$work/source.zib" "$work/saved.zib"
