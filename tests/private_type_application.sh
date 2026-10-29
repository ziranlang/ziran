#!/bin/sh
# A Vec or Option of a module's private record stays out of the module's
# public C header, so another module can include that header.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/points.zi" <<'ZI'
#import "vec"
#import "option"

Total :: () -> s32 {
    VecClear(points)
    unused VecPush(points, .{20})
    unused VecPush(points, .{22})
    last := VecPop(points)
    return points[0].value + last.value.value
}

#scope_file
Point :: struct { value: s32; }
points: Vec(Point);
ZI
cat > "$work/main.zi" <<'ZI'
#import "points"
#program_export
Answer :: () -> s32 { return Total() }
ZI

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/main.zi"
cat > "$work/c/entry.c" <<'C'
#include "main.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
"${CC:-cc}" -Iinclude -I"$work/c" "$work/c"/*.c -o "$work/c/app"
"$work/c/app"
