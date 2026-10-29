#!/bin/sh
# std/image's AlphaCompositeAt blends a picture drawn between pixels as a
# canvas's drawImage does, and Silhouette fills a shape with one colour.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/composite_at.zi" <<'ZI'
#import "image"

Is :: (image: *Image, x: s32, y: s32, r: u8, g: u8, b: u8, a: u8) -> bool {
    at := (cast(s64)y * image.width + x) * 4
    return image.pixels[at] == r && image.pixels[at + 1] == g && image.pixels[at + 2] == b && image.pixels[at + 3] == a
}

#program_export
Answer :: () -> s32 {
    dot := NewImage(1, 1, 255, 0, 0, 255)
    whole := NewImage(4, 4)
    AlphaCompositeAt(*whole, *dot, 1.0, 1.0)
    if !Is(*whole, 1, 1, 255, 0, 0, 255) || !Is(*whole, 2, 1, 0, 0, 0, 0) { return 1 }
    half := NewImage(4, 4)
    AlphaCompositeAt(*half, *dot, 1.5, 1.0)
    if !Is(*half, 1, 1, 255, 0, 0, 128) || !Is(*half, 2, 1, 255, 0, 0, 128) || !Is(*half, 3, 1, 0, 0, 0, 0) { return 2 }
    blue := NewImage(4, 4, 0, 0, 255, 255)
    AlphaCompositeAt(*blue, *dot, 1.5, 1.0)
    if !Is(*blue, 2, 1, 128, 0, 128, 255) || !Is(*blue, 0, 0, 0, 0, 255, 255) { return 3 }
    shape := NewImage(1, 1, 10, 20, 30, 200)
    Silhouette(*shape, 1, 2, 3, 128)
    if !Is(*shape, 0, 0, 1, 2, 3, 100) { return 4 }
    return 0
}
ZI

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/composite_at.zi"
cat > "$work/c/entry.c" <<'C'
#include <stdio.h>
#include "composite_at.h"
int main(void) { int result = Answer(); if (result) printf("case %d\n", result); return result; }
C
"${CC:-cc}" -Iinclude -I"$work/c" "$work/c"/*.c -lm -o "$work/c/app"
"$work/c/app"
"$ziran" bundle --root "$work" --entry composite_at:Answer -o "$work/composite_at.zx" "$work/composite_at.zi"
test "$("$ziran" run "$work/composite_at.zx")" = 0
