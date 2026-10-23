#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/shared"
printf 'one shared asset' > "$work/shared/message.txt"
cat > "$work/common.zi" <<'ZI'
count: s32 = 40;
Copy :: (value: s32) -> s32 #asm("x86_64", "# LIBRARY_NATIVE_FN\nmov %[value], %[out]", "=r", "r", "", "") { return value }
#program_export
Next :: () -> s32 {
    value := count
    #asm("x86_64", "# LIBRARY_NATIVE_BLOCK\naddl $1, %[value]", "=r:value", "0:old=value", "cc", "") { value += 1 }
    count = value
    return Copy(count)
}
#program_export
main :: () -> s32 {
    return Next()
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "common"
#program_export
main :: () -> s32 {
    return Next()
}
ZI
"$ziran" bundle --root "$work" --entry common:main \
    --asset-dir "shared=$work/shared" -o "$work/common.zx" "$work/common.zi"
"$ziran" bundle --root "$work" --entry app:main --library "$work/common.zx" \
    -o "$work/thin.zx" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:main --include-library "$work/common.zx" \
    -o "$work/standalone.zx" "$work/app.zi"
python3 - "$work" <<'PY'
from pathlib import Path
import sys
for name in ('common', 'thin', 'standalone'):
    data=(Path(sys.argv[1])/(name+'.zx')).read_bytes()
    assert b'LIBRARY_NATIVE_FN' not in data and b'LIBRARY_NATIVE_BLOCK' not in data
PY
mkdir -p "$work/wrong"
cat > "$work/wrong/common.zi" <<'ZI'
#program_export
Next :: () -> string {
    return "wrong return type"
}
#program_export
main :: () -> s32 {
    unused Next()
    return 0
}
ZI
"$ziran" bundle --root "$work/wrong" --entry common:main \
    -o "$work/wrong.zx" "$work/wrong/common.zi"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" "$repo/tests/bundle_libraries_test.c" \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -lpthread -o "$work/test"
"$work/test" "$work/common.zx" "$work/thin.zx" "$work/standalone.zx" "$work/wrong.zx"
