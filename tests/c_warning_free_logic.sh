#!/bin/sh
set -eu

# Generated C mixes && and || without tripping -Wparentheses, so programs
# that build their output with -Wall -Werror compile, and the grouping
# still evaluates as the source wrote it.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Mixed :: (a: bool, b: bool, c: bool) -> s32 {
    r: s32 = 0
    if a || b && c { r += 1 }
    if a && b || c { r += 2 }
    return r
}

#program_export
Answer :: () -> s32 {
    if Mixed(false, true, false) != 0 { return 0 }
    if Mixed(false, true, true) != 3 { return 0 }
    if Mixed(true, false, false) != 1 { return 0 }
    if Mixed(false, false, true) != 2 { return 0 }
    return 1
}
ZI

"$ziran" bundle --root "$work" --entry app:Answer -o "$work/app.zib" \
    "$work/app.zi"
test "$("$ziran" run "$work/app.zib")" = 1

"$ziran" build --target=c --entry app:Answer --root "$work" \
    -o "$work/c" "$work/app.zi"
cat > "$work/c/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 1 ? 0 : 1; }
C
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/include" -I"$work/c" \
    "$work"/c/*.c -o "$work/c/app"
"$work/c/app"
