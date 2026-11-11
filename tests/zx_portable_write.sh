#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/zx-portable-write-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cat > "$work/program.zi" <<'ZI'
Bump :: (value: s64) -> s64 #asm("x86_64", "# NATIVE_FN_CANARY\nadd $1, %[out]", "=r", "0", "cc", "") { return value + 1 }
#law Pure effect Bump == pure;
#law Increase theorem x: s64 => Bump(x) == x + 1;
#proof Increase { unfold Bump; ring; }
state: s64;
#program_export
Answer :: () -> s64 {
    text := "literal #asm survives"
    if text.count != 21 { return 1 }
    state += 1
    result: s64 = 40
    #asm("x86_64", "# NATIVE_BLOCK_CANARY\nadd $1, %[result]", "=r:result", "0:old=result", "cc", "") { result += 1 }
    return Bump(result) + state - 1
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/program.zi" > "$work/laws.json"
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" -I"$repo/include" \
    "$repo/tests/zx_portable_write_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -o "$work/write"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/portable_writer_cases.zi" -- "$ziran" "$work"
