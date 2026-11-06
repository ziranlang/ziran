#!/bin/sh
# Generated C builds cleanly under -Wall -Wextra with GCC and, when it is
# installed, Clang: an `ifx` comparison has one pair of parentheses, a local
# the program never reads is cast to void, and a procedure whose name ends in
# a helper's name, such as ReadSignedBits, does not pull in that helper.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

cat > "$work/app.zi" <<'ZI'
ReadSignedBits :: (x: s64) -> s64 { return x + 1 }
Pick :: (a: s64, b: s64) -> s64 {
    ignored := ReadSignedBits(a)
    return ifx a == b then 1 else 2
}
#program_export
Answer :: () -> s64 {
    if Pick(3, 3) != 1 || Pick(3, 4) != 2 { return 1 }
    return 42
}
ZI

output=$work/c
"$ziran" build --target=c --root "$work" -o "$output" "$work/app.zi"
cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
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
    done
done
if grep -q 'if ((' "$output/app.c"; then
    echo 'a condition has two pairs of parentheses' >&2
    exit 1
fi
if grep -q 'ZIRAN_HELPER_SignedBits' "$output/app.c"; then
    echo 'ReadSignedBits pulled in the SignedBits helper' >&2
    exit 1
fi
