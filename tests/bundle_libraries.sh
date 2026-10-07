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
#program_export
Next :: () -> s32 {
    count += 1
    return count
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
    --asset-dir "shared=$work/shared" -o "$work/common.zib" "$work/common.zi"
"$ziran" bundle --root "$work" --entry app:main --library "$work/common.zib" \
    -o "$work/thin.zib" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:main --include-library "$work/common.zib" \
    -o "$work/standalone.zib" "$work/app.zi"
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
    -o "$work/wrong.zib" "$work/wrong/common.zi"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" "$repo/tests/bundle_libraries_test.c" \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -lpthread -o "$work/test"
"$work/test" "$work/common.zib" "$work/thin.zib" "$work/standalone.zib" "$work/wrong.zib"
