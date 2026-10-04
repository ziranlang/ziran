#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/app.zi" <<'ZI'
host_api :: #system_library "host_api";
First :: () -> s32 #foreign host_api;
Second :: () -> s32 #foreign host_api;
#program_export
main :: () -> s32 { return First() + Second() }
ZI
cat > "$work/query.zi" <<'ZI'
#import "bundle_host"
#import "byte_text_linux"
#program_export
main :: (argc: s32, argv: **u8) -> s32 {
    if argc != 2 { return 1 }
    if BundleCapabilities(null) != cast(u64)0 { return 2 }
    if BundleCapabilityModuleName(null, cast(u64)0) != null { return 3 }
    bundle := OpenBundle(argv[1])
    if bundle == null { return 4 }
    ok := BundleCapabilities(bundle) == cast(u64)2
    first := TextFromCString(BundleCapabilityFunctionName(bundle, cast(u64)0))
    second := TextFromCString(BundleCapabilityFunctionName(bundle, cast(u64)1))
    ok = ok && ((first == "First" && second == "Second") ||
        (first == "Second" && second == "First"))
    ok = ok && TextFromCString(BundleCapabilityModuleName(bundle, cast(u64)0)) == "app"
    ok = ok && BundleCapabilityModuleName(bundle, cast(u64)2) == null
    ok = ok && BundleCapabilityFunctionName(bundle, cast(u64)2) == null
    CloseBundle(bundle)
    if !ok { return 5 }
    return 0
}
ZI
"$ziran" bundle --root "$work" --entry app:main -o "$work/app.zib" "$work/app.zi"
"$ziran" build --target=c --root "$work" --module-path "$repo/std" -o "$work/c" "$work/query.zi"
"${CC:-cc}" -std=c11 -Wno-main -I"$repo/include" -I"$work/c" "$work/c"/*.c \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -lpthread -o "$work/query"
"$work/query" "$work/app.zib"
printf '%s\n' 'Ziran bundle host capability discovery passed'
