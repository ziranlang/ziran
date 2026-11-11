#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
work="$repo/build/tests/zir-api"
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp" ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS XDG_RUNTIME_DIR SESSION_MANAGER XDG_SESSION_ID XDG_SESSION_TYPE DESKTOP_SESSION SSH_AUTH_SOCK
cat > "$work/library.zi" <<'ZI'
#import "vec"
TEXT :: "é";
Box :: struct { bytes: Vec(u8); }
Take :: (values: Vec(u8), view: []u8, pointer: *s32) -> Vec(u8) {
    return values
}
Pure :: () -> s32 { return 42 }
#law PureEffect effect Pure == pure;
#if #defined(EXTRA) {
Extra :: () -> s32 { return 9 }
}
#scope_module
Hidden :: () -> s32 { return 0 }
#law HiddenEffect effect Hidden == pure;
ZI
cat > "$work/main.zi" <<'ZI'
#import "library"
Answer :: () -> s32 { return Pure() }
ZI
"$ziran" api --json --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/source.json"
"$ziran" api --json --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/repeated.json"
cmp "$work/source.json" "$work/repeated.json"
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/main.zi"
"$ziran" api --json --root "$work/ir" "$work/ir/main.zir" > "$work/saved.json"
"$ziran" api --json --module library --symbol Take --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/scoped.json"
"$ziran" api --json --imports-only --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/imports.json"
"$ziran" api --json --max-symbols 2 --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/limited.json"
"$ziran" api --json --module library --max-bytes 2048 --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/bytes.json"
"$ziran" api --json --define EXTRA --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/conditional.json"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/api_assertions.zi" -- "$ziran" "$work" metadata
"$ziran" api --json --symbol "$(cat "$work/symbol-id")" --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/by-id.json"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/api_assertions.zi" -- "$ziran" "$work" matched
# Positions may change without changing semantic identities.
sed -i '1i// leading comment' "$work/library.zi"
"$ziran" api --json --module library --symbol Take --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/moved.json"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/api_assertions.zi" -- "$ziran" "$work" moved
mkdir -p "$work/many"
: > "$work/many/main.zi"
i=0
while test "$i" -lt 12; do
    printf '#import "mod%s"\n' "$i" >> "$work/many/main.zi"
    printf 'VALUE_%s :: %s;\n' "$i" "$i" > "$work/many/mod$i.zi"
    i=$((i + 1))
done
printf 'Answer :: () -> s32 { return 42 }\n' >> "$work/many/main.zi"
if "$ziran" api --json --max-bytes 1024 --root "$work/many" "$work/many/main.zi" > "$work/framing.out" 2> "$work/framing.err"; then exit 1; fi
test ! -s "$work/framing.out"
rg -q 'API framing exceeds' "$work/framing.err"
for invalid in 0 -1 1x 99999999999999999999999999; do
    if "$ziran" api --json --max-bytes "$invalid" --root "$work" "$work/main.zi" > "$work/invalid.out" 2> "$work/invalid.err"; then exit 1; fi
    test ! -s "$work/invalid.out"
done
cat > "$work/rewritten.zi" <<'ZI'
TEXT :: #string RAW
é
RAW; After :: () -> s32 { return 7 }
Answer :: () -> s32 { return 42 }
ZI
"$ziran" api --json --root "$work" "$work/rewritten.zi" > "$work/rewritten.json"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/api_assertions.zi" -- "$ziran" "$work" rewritten
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" "$repo/tests/zir_api_document.c" "$(dirname "$ziran")/../libziran.a" -lm -pthread -ldl -o "$work/documents"
"$work/documents"
echo 'zir api: scoped checked retrieval, stable identities, captured document hashes/UTF8 spans, saved-IR unknown bytes, ownership/laws, deterministic complete bounded JSON passed'
