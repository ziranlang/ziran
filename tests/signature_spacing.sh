#!/bin/sh
# `[] s32` and `* Point` in a signature are the types []s32 and *Point for
# every reader, including the portable runner's parameter parser.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/spaced.zi" <<'ZI'
Sum :: (values: [] s32) -> s32 {
    total: s32 = 0
    for values { total += it }
    return total
}
First :: (pair: [2] s32) -> s32 { return pair[1] }

#program_export
Answer :: () -> s32 {
    numbers: [3]s32 = .[10, 20, 12]
    pair: [2]s32 = .[1, 3]
    return Sum(numbers[:]) - First(pair) + 1
}
ZI
"$ziran" bundle --root "$work" --entry spaced:Answer -o "$work/spaced.zib" "$work/spaced.zi"
test "$("$ziran" run "$work/spaced.zib")" = 40
