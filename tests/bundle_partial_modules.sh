#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

# The library retains model.LibraryValue but prunes model.Payload. The add-on
# needs that nested record plus a new function and global in the same module.
cat > "$work/model.zi" <<'ZI'
Payload :: struct { value: s32 }
#law LibraryLaw custom 1 == 1;
Unused :: struct { value: s64 }
shared: s32 = 40;
added: s32 = 2;
host_api :: #system_library "host_api";
BaseHost :: () -> s32 #foreign host_api;
AddonHost :: () -> s32 #foreign host_api;
CommonHost :: () -> s32 #foreign host_api;
#scope_file
Secret :: () -> s32 { return 0 }
#scope_export
LibraryValue :: () -> s32 {
    shared += 1
    return shared + BaseHost() + CommonHost() + Secret()
}
AddonValue :: (value: Payload) -> s32 {
    added += 1
    return value.value + added + AddonHost() + CommonHost()
}
ZI
cat > "$work/base.zi" <<'ZI'
#import "model"
Main :: () -> s32 { return LibraryValue() }
ZI
cat > "$work/addon.zi" <<'ZI'
#import "model"
Holder :: struct { state: Payload }
#law AddonLaw custom 2 == 2;
#program_export
Main :: () -> s32 {
    value: Holder
    value.state.value = 1
    return LibraryValue() + AddonValue(value.state)
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/addon.zi" "$work/base.zi"
for input in source saved; do
    root=$work
    ext=zi
    if [ "$input" = saved ]; then root=$work/ir; ext=zir; fi
    "$ziran" bundle --root "$root" --entry base:Main -o "$work/base-$input.zx" "$root/base.$ext"
    "$ziran" bundle --root "$root" --entry addon:Main -o "$work/standalone-$input.zx" "$root/addon.$ext"
    "$ziran" bundle --root "$root" --entry addon:Main --library "$work/base-$input.zx" \
        -o "$work/thin-$input.zx" "$root/addon.$ext"
    "$ziran" bundle --root "$root" --entry addon:Main --include-library "$work/base-$input.zx" \
        -o "$work/included-$input.zx" "$root/addon.$ext"
done
for name in base standalone thin included; do cmp "$work/$name-source.zx" "$work/$name-saved.zx"; done

# Run native C from source and saved IR with explicit host implementations.
for input in source saved; do
    root=$work
    ext=zi
    if [ "$input" = saved ]; then root=$work/ir; ext=zir; fi
    "$ziran" build --target=c --root "$root" -o "$work/c-$input" "$root/addon.$ext"
    cat > "$work/c-$input/entry.c" <<'C'
#include "addon.h"
int BaseHost(void) { return 0; }
int AddonHost(void) { return 0; }
int CommonHost(void) { return 0; }
int main(void) { return Main() == 45 && Main() == 47 ? 0 : 1; }
C
    "${CC:-cc}" ${VM_CFLAGS:-} -I"$repo/include" -I"$work/c-$input" "$work/c-$input"/*.c -o "$work/native"
    "$work/native"
done
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/bundle_partial_modules_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -pthread -o "$work/test"
"$work/test" "$work/base-source.zx" "$work/thin-source.zx" "$work/standalone-source.zx" "$work/included-source.zx"
# The same composition executes on the independent portable VM library.
if [ -f "${ZIRAN_VM_LIB:-$repo/build/libziran-vm.a}" ]; then
    "${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" -I"$repo/cmd/zir" \
        "$repo/tests/bundle_partial_modules_test.c" "${ZIRAN_VM_LIB:-$repo/build/libziran-vm.a}" -lm -pthread -o "$work/test"
    "$work/test" "$work/base-source.zx" "$work/thin-source.zx" "$work/standalone-source.zx" "$work/included-source.zx"
fi
