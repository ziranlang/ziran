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
