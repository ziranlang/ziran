#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/hello.zi" <<'ZI'
#program_export
Answer :: () -> s32 { return 42 }
ZI
cat > "$work/main.c" <<'C'
#include "hello.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C

"$ziran" build --target=c --root "$work" -o "$work/c99" "$work/hello.zi"
"${CC:-cc}" -std=c99 -pedantic-errors -Iinclude -I"$work/c99" \
    "$work/c99/hello.c" "$work/main.c" -o "$work/hello"
"$work/hello"
if grep -Fq '_bits(' "$work/c99/hello.c"; then
    echo 'unused numeric support was emitted' >&2
    exit 1
fi

# An empty record value zeroes every field in strict C99, which has no
# empty braces, both returned and initializing a declaration.
cat > "$work/empty.zi" <<'ZI'
Pair :: struct {
    left: s32
    right: *u8
}
Empty :: (flag: bool) -> Pair {
    if flag { return Pair.{} }
    value: Pair = Pair.{}
    value.left = 1
    return value
}
#program_export
main :: () -> s32 {
    if Empty(true).left != 0 || Empty(false).left != 1 { return 1 }
    return 0
}
ZI
"$ziran" build --target=c --root "$work" -o "$work/empty-c" "$work/empty.zi"
"${CC:-cc}" -std=c99 -pedantic-errors -Iinclude -I"$work/empty-c" \
    "$work"/empty-c/*.c -o "$work/empty"
"$work/empty"

if "$ziran" build --target=c99 --root "$work" \
    -o "$work/c99-alias" "$work/hello.zi" 2> "$work/c99-alias.err"; then
    echo 'removed C99 target alias was accepted' >&2
    exit 1
fi
grep -Fq 'build needs --target=c|cpp|go' "$work/c99-alias.err"

# Native C output uses the saved IR and keeps only code reachable from the
# selected entry, including Ziran implementations of foreign host effects.
cat > "$work/app.zi" <<'ZI'
#import "dead"
host :: #system_library "host";
CallHost :: () -> s32 #foreign host;
#program_export
main :: () -> s32 {
    if CallHost() == 42 { return 0 }
    return 1
}
UnusedApp :: () -> s32 { return 7 }
UnusedShape :: struct {
    value: s32
}
ZI
cat > "$work/impl.zi" <<'ZI'
#program_export
CallHost :: () -> s32 { return 42 }
#program_export
UnusedImpl :: () -> s32 { return 10 }
ZI
cat > "$work/dead.zi" <<'ZI'
#program_export
NeverCalled :: () -> s32 { return 11 }
ZI
"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/app.zi" "$work/impl.zi"
"$ziran" inspect "$work/ir/app.zir" > "$work/inspect.txt"
grep -Fq 'function main(' "$work/inspect.txt"
grep -Fq 'name=CallHost' "$work/inspect.txt"
"$ziran" inspect --hex "$work/ir/app.zir" > "$work/hex.txt"
grep -Fq '5a 49 52 00' "$work/hex.txt"
"$ziran" build --target=c --entry app:main \
    --root "$work/ir" -o "$work/linked" \
    "$work/ir/app.zir" "$work/ir/impl.zir"
if grep -Fq 'UnusedApp' "$work/linked/app.c" ||
   grep -Fq 'UnusedImpl' "$work/linked/impl.c" ||
   grep -Fq 'UnusedShape' "$work/linked/app.h" ||
   test -e "$work/linked/dead.c" ||
   test -e "$work/linked/dead.h"; then
    echo 'unreachable native functions survived entry linking' >&2
    exit 1
fi
"${CC:-cc}" -std=c99 -pedantic-errors -Iinclude -I"$work/linked" \
    "$work/linked"/*.c -o "$work/linked-app"
"$work/linked-app"

"$ziran" ir --entry app:main --root "$work" -o "$work/linked-ir" \
    "$work/app.zi" "$work/impl.zi"
test "$(find "$work/linked-ir" -name '*.zir' | wc -l)" -eq 2
"$ziran" inspect "$work/linked-ir/app.zir" > "$work/linked-inspect.txt"
if grep -Eq 'UnusedApp|UnusedShape' "$work/linked-inspect.txt"; then
    echo 'unreachable declarations survived linked IR' >&2
    exit 1
fi
"$ziran" build --target=c --entry app:main \
    --root "$work/linked-ir" -o "$work/from-linked-ir" \
    "$work/linked-ir/app.zir" "$work/linked-ir/impl.zir"
"${CC:-cc}" -std=c99 -pedantic-errors -Iinclude -I"$work/from-linked-ir" \
    "$work/from-linked-ir"/*.c -o "$work/from-linked-ir-app"
"$work/from-linked-ir-app"

cat > "$work/pointer.zi" <<'ZI'
Node :: struct {
    value: s32
}
UnusedShape :: struct {
    value: s32
}
Read :: (node: *Node) -> s32 { return node.value }
#program_export
main :: () -> s32 {
    node: Node
    node.value = 42
    if Read(*node) == 42 { return 0 }
    return 1
}
ZI
"$ziran" ir --entry pointer:main --root "$work" -o "$work/pointer-ir" \
    "$work/pointer.zi"
"$ziran" inspect "$work/pointer-ir/pointer.zir" > "$work/pointer.txt"
grep -Fq 'type Node' "$work/pointer.txt"
if grep -Fq 'UnusedShape' "$work/pointer.txt"; then
    echo 'unused type survived pointer reachability' >&2
    exit 1
fi
"$ziran" build --target=c --entry pointer:main \
    --root "$work/pointer-ir" -o "$work/pointer-c" \
    "$work/pointer-ir/pointer.zir"
"${CC:-cc}" -std=c99 -pedantic-errors -Iinclude -I"$work/pointer-c" \
    "$work/pointer-c"/*.c -o "$work/pointer-app"
"$work/pointer-app"

cat > "$work/constants.zi" <<'ZI'
Used :: 40
Unused :: 99
Shape :: struct {
    value: s32
}
ZI
cat > "$work/constant_app.zi" <<'ZI'
#import "constants"
LocalUnused :: 77
#program_export
main :: () -> s32 {
    shape: Shape
    shape.value = Used + 2
    if shape.value == 42 { return 0 }
    return 1
}
ZI
"$ziran" ir --entry constant_app:main --root "$work" \
    -o "$work/constant-ir" "$work/constant_app.zi"
"$ziran" inspect "$work/constant-ir/constants.zir" > "$work/constants.txt"
grep -Fq 'constant Used' "$work/constants.txt"
if grep -Eq 'Unused|LocalUnused' "$work/constants.txt" ||
   "$ziran" inspect "$work/constant-ir/constant_app.zir" |
       grep -Fq 'constant LocalUnused'; then
    echo 'unused constant survived linked IR' >&2
    exit 1
fi
"$ziran" build --target=c --entry constant_app:main \
    --root "$work/constant-ir" -o "$work/constant-c" \
    "$work/constant-ir/constant_app.zir"
test -f "$work/constant-c/constants.h"
test ! -e "$work/constant-c/constants.c"
"${CC:-cc}" -std=c99 -pedantic-errors -Iinclude -I"$work/constant-c" \
    "$work/constant-c"/*.c -o "$work/constant-app"
"$work/constant-app"
