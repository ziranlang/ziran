#!/bin/sh
# Views stored in a pointer parameter's pointee belong to the caller, so they
# may be returned; views of the pointee's own arrays and of locals may not.
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir "$work/safe" "$work/unsafe"

cat > "$work/safe/safe.zi" <<'ZI'
Library :: struct { error: string; names: [2]string }
BorrowError :: (lib: *Library) -> string {
    if lib.error.count > 0 { return lib.error }
    return ""
}
Forward :: (lib: *Library) -> string { return BorrowError(lib) }
Alias :: (lib: *Library) -> string {
    alias := lib
    return alias.error
}
Choose :: (lib: *Library, first: bool) -> string {
    return ifx first then lib.names[0] else BorrowError(lib)
}
SetText :: (lib: *Library, text: string) { lib.error = text }
Rebind :: (bytes: []u8) -> s32 {
    buffer: [4]u8
    bytes = buffer[0:2]
    return cast(s32)bytes.count
}
Local :: () -> string {
    lib: Library
    lib.error = "static"
    return BorrowError(*lib)
}
main :: () -> s32 {
    lib: Library
    SetText(*lib, "failed")
    lib.names[0] = "first"
    if Forward(*lib) != "failed" { return 1 }
    if Alias(*lib) != "failed" { return 2 }
    if Choose(*lib, true) != "first" { return 3 }
    if Local() != "static" { return 4 }
    bytes: [4]u8
    if Rebind(bytes[:]) != 2 { return 5 }
    return 0
}
ZI

cat > "$work/unsafe/return_array_field.zi" <<'ZI'
Node :: struct { bytes: [2]u8 }
Bytes :: (node: *Node) -> string { return TextView(node.bytes[:]) }
ZI
cat > "$work/unsafe/return_names_slice.zi" <<'ZI'
Library :: struct { names: [2]string }
Names :: (lib: *Library) -> []string { return lib.names[:] }
ZI
cat > "$work/unsafe/return_local_through_call.zi" <<'ZI'
Library :: struct { error: string }
BorrowError :: (lib: *Library) -> string { return lib.error }
Leak :: () -> string {
    buffer: [4]u8
    buffer[0] = cast(u8)97
    lib: Library
    lib.error = TextView(buffer[0:1])
    return BorrowError(*lib)
}
ZI

for fixture in return_array_field return_names_slice \
    return_local_through_call; do
    if "$ziran" check --diagnostics=json --root "$work/unsafe" \
        "$work/unsafe/$fixture.zi" > "$work/stdout" 2> "$work/diagnostics.jsonl"; then
        echo "unsafe $fixture was accepted" >&2
        exit 1
    fi
    python3 - "$work/diagnostics.jsonl" <<'PY'
import json, pathlib, sys
items = [json.loads(line) for line in pathlib.Path(sys.argv[1]).read_text().splitlines()]
assert items and all(item['code'] == 'check.slice_lifetime' for item in items), items
PY
done

"$ziran" ir --root "$work/safe" -o "$work/ir" "$work/safe/safe.zi"
for input in "$work/safe/safe.zi" "$work/ir/safe.zir"; do
    "$ziran" check --root "$work/safe" "$input"
done
"$ziran" build --root "$work/safe" --target=c --entry safe:main --exe \
    -o "$work/out" "$work/safe/safe.zi"
"$work/out/safe"
