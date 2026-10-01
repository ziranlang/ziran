#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "text_buffer"
#program_export
Check :: () -> s32 {
    bytes: [4]u8
    bytes[0] = cast(u8)97
    bytes[1] = cast(u8)98
    bytes[2] = cast(u8)99
    bytes[3] = cast(u8)0
    text := TextView(bytes[0:3])
    whole := TextView(bytes[:])
    terminated := TextUntilNul(bytes[:])
    if text != "abc" || text.count != 3 ||
        whole.count != 4 || whole[3] != cast(u8)0 ||
        terminated != "abc" { return 1 }
    return 0
}
ZI

"$ziran" bundle --root "$work" --module-path "$repo/std" --entry app:Check \
    -o "$work/app.zib" "$work/app.zi"
test "$("$ziran" run "$work/app.zib")" = 0
"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$repo/std" \
    --entry app:Check -o "$work/saved.zib" "$work/ir/app.zir"
test "$("$ziran" run "$work/saved.zib")" = 0

"$ziran" build --target=c --no-main --root "$work" \
    --module-path "$repo/std" \
    -o "$work/c" "$work/app.zi"
cat > "$work/c/main.c" <<'C'
#include "app.h"
int main(void) { return Check(); }
C
"${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/c/app"
"$work/c/app"

"$ziran" build --target=cpp --no-main --root "$work" \
    --module-path "$repo/std" \
    -o "$work/cpp" "$work/app.zi"
cat > "$work/cpp/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Check(); }
CPP
"${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$work/cpp" \
    "$work/cpp"/*.cpp -o "$work/cpp/app"
"$work/cpp/app"

"$ziran" build --target=go --pkg main --no-main --root "$work" \
    --module-path "$repo/std" \
    -o "$work/go" "$work/app.zi"
cat > "$work/go/main.go" <<'GO'
package main
func main() { if App_Check() != 0 { panic("TextView") } }
GO
GO111MODULE=off go run "$work/go"/*.go

cat > "$work/invalid.zi" <<'ZI'
Check :: () -> string {
    numbers: [1]s32
    return TextView(numbers[:])
}
ZI
if "$ziran" ir --root "$work" -o "$work/invalid-ir" \
    "$work/invalid.zi" > "$work/invalid.log" 2>&1; then
    echo 'TextView accepted a non-byte slice' >&2
    exit 1
fi
rg -q 'TextView requires one \[\]u8 argument' "$work/invalid.log"

cat > "$work/escape.zi" <<'ZI'
Bad :: () -> string {
    bytes: [2]u8
    bytes[0] = cast(u8)97
    bytes[1] = cast(u8)98
    return TextView(bytes[:])
}
ZI
if "$ziran" ir --root "$work" -o "$work/escape-ir" \
    "$work/escape.zi" > "$work/escape.log" 2>&1; then
    echo 'TextView let a local buffer escape' >&2
    exit 1
fi
rg -q 'returned text view borrows local or temporary storage' "$work/escape.log"

cat > "$work/global.zi" <<'ZI'
global_bytes: [3]u8 = .[97, 98, 99];
global_text: string;

Save :: () -> string {
    global_text = TextView(global_bytes[:])
    return global_text
}
ZI
"$ziran" ir --root "$work" -o "$work/global-ir" "$work/global.zi"

cat > "$work/global_mutation.zi" <<'ZI'
global_bytes: [3]u8 = .[97, 98, 99];
global_text: string;

Install :: () {
    global_text = TextView(global_bytes[:])
}
Bad :: () {
    global_bytes[0] = 120
}
ZI
if "$ziran" ir --root "$work" -o "$work/global-mutation-ir" \
    "$work/global_mutation.zi" > "$work/global-mutation.log" 2>&1; then
    echo 'TextView allowed cross-function mutation of its global backing' >&2
    exit 1
fi
rg -q 'mutating text backing storage while its view is live' \
    "$work/global-mutation.log"

cat > "$work/global_fields.zi" <<'ZI'
State :: struct { bytes: [3]u8; status: s32; }
state: State;
global_text: string;
global_alias: string;

Install :: () {
    global_text = TextView(state.bytes[:])
    global_alias = global_text
}
Safe :: () {
    state.status = 1
}
ZI
"$ziran" ir --root "$work" -o "$work/global-fields-ir" \
    "$work/global_fields.zi"

cat > "$work/global_field_mutation.zi" <<'ZI'
State :: struct { bytes: [3]u8; status: s32; }
state: State;
global_text: string;

Install :: () {
    global_text = TextView(state.bytes[:])
}
Bad :: () {
    state.bytes[0] = 120
}
ZI
if "$ziran" ir --root "$work" -o "$work/global-field-mutation-ir" \
    "$work/global_field_mutation.zi" > "$work/global-field-mutation.log" 2>&1; then
    echo 'TextView allowed mutation of a borrowed global field' >&2
    exit 1
fi
rg -q 'mutating text backing storage while its view is live' \
    "$work/global-field-mutation.log"

cat > "$work/global_alias_mutation.zi" <<'ZI'
global_bytes: [3]u8 = .[97, 98, 99];
global_text: string;
global_alias: string;

Install :: () {
    global_text = TextView(global_bytes[:])
    global_alias = global_text
}
Bad :: () {
    global_bytes[0] = 120
}
ZI
if "$ziran" ir --root "$work" -o "$work/global-alias-mutation-ir" \
    "$work/global_alias_mutation.zi" > "$work/global-alias-mutation.log" 2>&1; then
    echo 'TextView lost backing through a global alias chain' >&2
    exit 1
fi
rg -q 'mutating text backing storage while its view is live' \
    "$work/global-alias-mutation.log"

printf 'global_first: [3]u8 = .[97, 98, 99];\n' > "$work/many_globals_base.zi"
index=0
while [ "$index" -lt 80 ]; do
    printf 'padding_%s: s32;\n' "$index" >> "$work/many_globals_base.zi"
    index=$((index + 1))
done
cat >> "$work/many_globals_base.zi" <<'ZI'
global_second: [3]u8 = .[100, 101, 102];
global_text: string;
global_alias: string;

Choose :: (first: bool) -> string {
    if first { return TextView(global_first[:]) }
    return TextView(global_second[:])
}
Install :: () {
    global_text = Choose(true)
    global_alias = global_text
}
ZI
cp "$work/many_globals_base.zi" "$work/many_globals_valid.zi"
"$ziran" ir --root "$work" -o "$work/many-globals-valid-ir" \
    "$work/many_globals_valid.zi"
for backing in global_first global_second; do
    cp "$work/many_globals_base.zi" "$work/many_globals_$backing.zi"
    printf 'Bad :: () { %s[0] = 120 }\n' "$backing" \
        >> "$work/many_globals_$backing.zi"
    if "$ziran" ir --root "$work" -o "$work/many-globals-$backing-ir" \
        "$work/many_globals_$backing.zi" \
        > "$work/many-globals-$backing.log" 2>&1; then
        echo "TextView allowed mutation of $backing through a global alias" >&2
        exit 1
    fi
    rg -q 'mutating text backing storage while its view is live' \
        "$work/many-globals-$backing.log"
done

cat > "$work/global_escape.zi" <<'ZI'
global_text: string;

Bad :: () {
    bytes: [2]u8
    global_text = TextView(bytes[:])
}
ZI
if "$ziran" ir --root "$work" -o "$work/global-escape-ir" \
    "$work/global_escape.zi" > "$work/global-escape.log" 2>&1; then
    echo 'TextView let a local buffer escape into a global' >&2
    exit 1
fi
rg -q 'text view assignment may escape its backing storage' "$work/global-escape.log"

# Remembering a local address in an opaque global pointer must not retain a
# pointer to the compiler's per-function binding table. The program clears
# those pointers before ever dereferencing them.
cat > "$work/global_pointer_storage.zi" <<'ZI'
items: [2]*u8;
pointer: *u8;
Remember :: () {
    bytes: [1]u8 = .[97]
    items[1] = *bytes[0]
    pointer = *bytes[0]
}
Reset :: () { items[0] = null; items[1] = null; pointer = null }
#program_export
main :: () -> s32 { Remember(); Reset(); return 0 }
ZI
"$ziran" ir --root "$work" -o "$work/global-pointer-storage-ir" \
    "$work/global_pointer_storage.zi"
"$ziran" build --target=c --root "$work" -o "$work/global-pointer-storage-source" \
    "$work/global_pointer_storage.zi"
"$ziran" build --target=c --root "$work/global-pointer-storage-ir" \
    -o "$work/global-pointer-storage-saved" \
    "$work/global-pointer-storage-ir/global_pointer_storage.zir"

# Changing a pointer field does not redirect the enclosing record pointer.
cat > "$work/pointer_field_mutation.zi" <<'ZI'
State :: struct { bytes: [3]u8; other: *u8; }
state: State;
Bad :: () {
    owner := *state
    local: [1]u8
    owner.other = *local[0]
    text := TextView(owner.bytes[:])
    state.bytes[0] = 120
    print("%", text)
}
ZI
if "$ziran" ir --root "$work" -o "$work/pointer-field-mutation-ir" \
    "$work/pointer_field_mutation.zi" > "$work/pointer-field-mutation.log" 2>&1; then
    echo 'TextView lost its container backing after a pointer field assignment' >&2
    exit 1
fi
rg -q 'mutating text backing storage while its view is live' \
    "$work/pointer-field-mutation.log"
