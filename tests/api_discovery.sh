#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/api-discovery-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/library.zi" <<'ZI'
Choice :: struct {
    value: s32;
}

Visible :: (value: s32 = 42) -> Choice {
    result: Choice
    result.value = value
    return result
}

#scope_module
Hidden :: () -> s32 { return 9 }
HIDDEN_CONSTANT :: 3;
hidden_global: s32 = 4;

#scope_export
Again :: () -> s32 { return Hidden() }
PUBLIC_CONSTANT :: 5;
public_global: s32 = 6;
host_api :: #system_library "host_api";
CallHost :: (value: s32) -> s32 #foreign host_api;
Handler :: #type (value: s32) -> bool;
FEATURE :: #defined(HAS_EXTRA)
#if FEATURE {
Extra :: () -> s32 { return 1 }
}
ZI
cat > "$work/main.zi" <<'ZI'
#import "library"

Answer :: () -> s32 {
    value: Choice = Visible(42)
    return value.value + Again() - 9
}
ZI

"$ziran" api --json --root "$work" "$work/main.zi" > "$work/source.json"
"$ziran" api --json --define HAS_EXTRA --root "$work" \
    "$work/main.zi" > "$work/defined.json"
"$ziran" api --root "$work" "$work/main.zi" > "$work/source.txt"
"$ziran" ir --root "$work" -o "$work/ir" "$work/main.zi"
"$ziran" api --json --root "$work/ir" "$work/ir/main.zir" > "$work/saved.json"

"$ziran" run --target=c --root "$repo/tests" "$repo/tests/api_assertions.zi" -- "$ziran" "$work" discovery

if "$ziran" api --root "$work" "$work/missing.zi" > "$work/out" 2> "$work/err"; then
    echo 'api accepted a missing source file' >&2
    exit 1
fi
