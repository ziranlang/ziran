#!/bin/sh
# std/image crops, flips and turns pictures as Pillow's crop and transpose
# do, and finds the box of their visible pixels.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/transpose.zi" <<'ZI'
#import "image"

Red :: (image: *Image, x: s32, y: s32) -> u8 { return image.pixels[(cast(s64)y * image.width + x) * 4] }

#program_export
Answer :: () -> s32 {
    // A 3x2 picture whose red channel numbers its pixels 1..6.
    picture := NewImage(3, 2)
    for i: 0..5 { picture.pixels[i * 4] = cast(u8)(i + 1); picture.pixels[i * 4 + 3] = 255 }
    right := RotateRight(*picture)
    if right.width != 2 || right.height != 3 || Red(*right, 0, 0) != 4 || Red(*right, 1, 0) != 1 || Red(*right, 0, 2) != 6 { return 1 }
    left := RotateLeft(*picture)
    if Red(*left, 0, 0) != 3 || Red(*left, 1, 2) != 4 { return 2 }
    half := RotateHalf(*picture)
    if Red(*half, 0, 0) != 6 || Red(*half, 2, 1) != 1 { return 3 }
    flipped := FlipLeftRight(*picture)
    if Red(*flipped, 0, 0) != 3 || Red(*flipped, 2, 1) != 4 { return 4 }
    part := CropImage(*picture, 1, 1, 4, 2)
    if part.width != 3 || part.height != 1 || Red(*part, 0, 0) != 5 || Red(*part, 1, 0) != 6 || part.pixels[2 * 4 + 3] != 0 { return 5 }
    dot := NewImage(5, 4)
    dot.pixels[(2 * 5 + 1) * 4 + 3] = 9
    dot.pixels[(3 * 5 + 3) * 4 + 3] = 1
    x0, y0, x1, y1 := AlphaBounds(*dot)
    if x0 != 1 || y0 != 2 || x1 != 4 || y1 != 4 { return 6 }
    empty := NewImage(2, 2)
    e0, e1, e2, e3 := AlphaBounds(*empty)
    if e0 != 0 || e1 != 0 || e2 != 0 || e3 != 0 { return 7 }
    return 0
}
ZI

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/transpose.zi"
cat > "$work/c/entry.c" <<'C'
#include <stdio.h>
#include "transpose.h"
int main(void) { int result = Answer(); if (result) printf("case %d\n", result); return result; }
C
"${CC:-cc}" -Iinclude -I"$work/c" "$work/c"/*.c -lm -o "$work/c/app"
"$work/c/app"
"$ziran" bundle --root "$work" --entry transpose:Answer -o "$work/transpose.zx" "$work/transpose.zi"
test "$("$ziran" run "$work/transpose.zx")" = 0
echo "std/image: crop, flip, quarter and half turns, and visible bounds"
