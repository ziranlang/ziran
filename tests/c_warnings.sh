#!/bin/sh
# Generated C builds cleanly under -Wall -Wextra with GCC and, when it is
# installed, Clang: an `ifx` comparison has one pair of parentheses, a local
# the program never reads is cast to void, and a procedure whose name ends in
# a helper's name, such as ReadSignedBits, does not pull in that helper.
# A parameter a body never reads is cast to void too, in out-of-line
# definitions and in the inline copies another module's header carries, and a
# C mock of that module compiles once it opts out of the copies.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

cat > "$work/quiet.zi" <<'ZI'
Skip :: (ignored: s32) {}
First :: (value: s32, rest: s64) -> s32 { return value }
Hidden :: (code: s32) -> s32 { return Step() }
#scope_file
Step :: () -> s32 { return 1 }
ZI
cat > "$work/app.zi" <<'ZI'
Quiet :: #import "quiet";
Unread :: (x: s64, label: s64) -> s64 { return x }
ReadSignedBits :: (x: s64) -> s64 { return x + 1 }
Pick :: (a: s64, b: s64) -> s64 {
    ignored := ReadSignedBits(a)
    return ifx a == b then 1 else 2
}
#program_export
Answer :: () -> s64 {
    if Pick(3, 3) != 1 || Pick(3, 4) != 2 { return 1 }
    Quiet.Skip(5)
    if Quiet.First(4, 9) != 4 || Quiet.Hidden(0) != 1 || Unread(2, 3) != 2 { return 2 }
    return 42
}
ZI

output=$work/c
"$ziran" build --target=c --root "$work" -o "$output" "$work/app.zi"
cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
cat > "$work/mock.c" <<'C'
#define ZIRAN_QUIET_H_SOURCE 1
#include "quiet.h"
void quiet_Skip(int32_t ignored) { (void)ignored; }
int32_t quiet_First(int32_t value, int64_t rest) { (void)rest; return value; }
C
compilers=${CC:-cc}
for clang in clang clang-19 clang-18; do
    if command -v "$clang" > /dev/null 2>&1; then compilers="$compilers $clang"; break; fi
done
for compiler in $compilers; do
    for level in -O0 -O2; do
        "$compiler" -std=c11 -Wall -Wextra -Werror $level \
            -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
        "$output/app"
        "$compiler" -std=c11 -Wall -Wextra -Werror $level \
            -I"$repo/include" -I"$output" -c "$work/mock.c" -o "$work/mock.o"
    done
done
grep -q 'quiet_Skip_ziran_inline' "$output/quiet.h"
for unread in ignored rest; do
    if ! grep -q "^    (void)$unread;\$" "$output/quiet.h"; then
        echo "the inline copy does not cast unread parameter $unread to void" >&2
        exit 1
    fi
done
for unread in code label; do
    if ! grep -q "^    (void)$unread;\$" "$output"/*.c; then
        echo "the out-of-line definition does not cast unread parameter $unread to void" >&2
        exit 1
    fi
done
if grep -q '(void)value;\|(void)x;' "$output/quiet.h" "$output"/*.c; then
    echo 'a parameter the body reads was cast to void' >&2
    exit 1
fi
if grep -q 'if ((' "$output/app.c"; then
    echo 'a condition has two pairs of parentheses' >&2
    exit 1
fi
if grep -q 'ZIRAN_HELPER_SignedBits' "$output/app.c"; then
    echo 'ReadSignedBits pulled in the SignedBits helper' >&2
    exit 1
fi
