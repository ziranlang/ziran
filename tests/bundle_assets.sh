#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/assets/sub"
printf 'a\000b' > "$work/assets/a.bin"
: > "$work/assets/empty"
printf 'nested' > "$work/assets/sub/z.bin"
cat > "$work/main.zi" <<'ZI'
#program_export
Answer :: () -> s32 { return 42 }
ZI
cat > "$work/read.zi" <<'ZI'
#import "bundle_host"
#import "byte_text_linux"
#program_export
main :: (argc: s32, argv: **u8) -> s32 {
    if argc != 2 { return 1 }
    bundle := OpenBundle(argv[1])
    if bundle == null { return 2 }
    ok := BundleAssets(bundle) == cast(u64)3 &&
        TextFromCString(BundleAssetPath(bundle, cast(u64)0)) == "assets/a.bin" &&
        TextFromCString(BundleAssetPath(bundle, cast(u64)1)) == "assets/empty" &&
        TextFromCString(BundleAssetPath(bundle, cast(u64)2)) == "assets/sub/z.bin" &&
        BundleAssetLength(bundle, cast(u64)0) == cast(u64)3 &&
        BundleAssetLength(bundle, cast(u64)1) == cast(u64)0 &&
        BundleAssetLength(bundle, cast(u64)2) == cast(u64)6 &&
        BundleAssetPath(bundle, cast(u64)3) == null &&
        BundleAssetBytes(bundle, cast(u64)3) == null
    bytes := BundleAssetBytes(bundle, cast(u64)0)
    ok = ok && bytes != null && bytes[0] == cast(u8)97 &&
        bytes[1] == cast(u8)0 && bytes[2] == cast(u8)98
    CloseBundle(bundle)
    if !ok { return 3 }
    return 0
}
ZI
"$ziran" bundle --root "$work" --entry main:Answer --asset-dir "assets=$work/assets" \
    -o "$work/source.zib" "$work/main.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/main.zi"
"$ziran" bundle --root "$work/ir" --entry main:Answer --asset-dir "assets=$work/assets" \
    -o "$work/saved.zib" "$work/ir/main.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
"$ziran" build --target=c --root "$work" -o "$work/c" "$work/read.zi"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -Wno-main -I"$repo/include" -I"$work/c" "$work/c"/*.c \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -lpthread -o "$work/read"
"$work/read" "$work/source.zib"
python3 - "$work" <<'PY'
from pathlib import Path
import struct
import sys
root = Path(sys.argv[1])
data = (root / "source.zib").read_bytes()
at = data.index(b"assets/a.bin")
for name, replacement in [
    ("traversal", b"../bad/a.bin"),
    ("absolute", b"/ssets/a.bin"),
    ("backslash", b"assets\\a.bin"),
    ("nul", b"assets\x00a.bin"),
]:
    assert len(replacement) == len(b"assets/a.bin")
    (root / (name + ".zib")).write_bytes(data[:at] + replacement + data[at + len(replacement):])
(root / "truncated.zib").write_bytes(data[:-1])
count_offset = at - 8
(root / "oversized.zib").write_bytes(data[:count_offset] + struct.pack("<I", 0xffffffff) + data[count_offset + 4:])
at = data.index(b"assets/sub/z.bin")
(root / "unsorted.zib").write_bytes(data[:at] + b"assets/aaa/z.bin" + data[at + 16:])
PY
for case in traversal absolute backslash nul truncated oversized unsorted; do
    if "$work/read" "$work/$case.zib" >/dev/null 2>&1; then
        echo "accepted malformed bundle assets: $case" >&2
        exit 1
    fi
done
ln -s "$work/main.zi" "$work/assets/link"
if "$ziran" bundle --root "$work" --entry main:Answer --asset-dir "assets=$work/assets" \
    -o "$work/symlink.zib" "$work/main.zi" >/dev/null 2>&1; then
    echo 'accepted an asset symlink' >&2
    exit 1
fi
printf '%s\n' 'Zib embedded binary files, empty files, deterministic packaging and malformed paths passed'
