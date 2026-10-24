#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/arraylib.zi" <<'EOF'

BASE :: 1
CAPACITY :: BASE + 1
THREE :: CAPACITY + 1
ROWS :: 2
NEGATIVE :: -1

Point :: struct {
    x: s32
}
Direction :: enum { North; South; }

Box :: struct {
    values: [CAPACITY]Point
}

Matrix :: struct {
    values: [ROWS][THREE]s32
    names: [ROWS][THREE + 1]u8
}

matrix_state: Matrix;

Triple :: [THREE]s32
AliasState :: [CAPACITY]Point

#program_export
AliasAnswer :: () -> s32 {
    triple: Triple
    triple[0] = 4
    triple[1] = 5
    triple[2] = 6
    state: AliasState
    state[0] = Point.{.x = 7}
    if triple.count != 3 || state.count != 2 { return 0 }
    return triple[0] + triple[1] + triple[2] + state[0].x
}

#program_export
MatrixGlobalAnswer :: () -> s32 {
    matrix_state.values[1][2] = 42
    matrix_state.names[1][2] = cast(u8)65
    if matrix_state.names[1][2] != cast(u8)65 { return 0 }
    return matrix_state.values[1][2]
}

#program_export
MatrixAnswer :: () -> s32 {
    matrix: Matrix
    expanded: [CAPACITY * 2]u8
    expanded[3] = cast(u8)65
    if expanded[3] != cast(u8)65 { return 0 }
    matrix.values[0][0] = 40
    matrix.values[1][2] = 2
    matrix.names[1][2] = cast(u8)120
    copy: Matrix = matrix
    matrix.values[0][0] = 0
    matrix.names[1][2] = cast(u8)121
    if copy.names[1][2] != cast(u8)120 || copy.names[0][0] != 0 {
        return 0
    }
    return copy.values[0][0] + copy.values[1][2]
}

LargeBox :: struct {
    values: [4096]Point
}

#program_export
RoundTripLarge :: (box: LargeBox) -> LargeBox {
    copy: LargeBox = box
    copy.values[0].x = 42
    return copy
}

#program_export
CheckLarge :: (box: LargeBox) -> bool {
    updated: LargeBox = RoundTripLarge(box)
    return box.values[0].x == 41 && updated.values[0].x == 42
}

#program_export
SumBox :: (box: Box) -> s32 {
    return box.values[0].x + box.values[1].x
}

#program_export
SumArray :: (values: [THREE]s32) -> s32 {
    return values[0] + values[1]
}
EOF

cat > "$work/arrayapp.zi" <<'EOF'
#import "arraylib"
Named :: #import "arraylib";

ReadLoop :: () -> s32 {
    values: [4096]s32
    values[0] = 42
    total: s32 = 0
    index: s32 = 0
    while index < 300 {
        total += values[0]
        index += 1
    }
    return total / 300
}

#program_export
Answer :: () -> s32 {
    if CAPACITY != 2 || NEGATIVE != -1 { return 0 }
    inferred := s32.[41, 1]
    if inferred.count != 2 || inferred[0] + inferred[1] != 42 {
        return 0
    }
    if SumArray(s32.[41, 1, 0]) != 42 { return 0 }
    directions := Direction.[.North, .South]
    if directions.count != 2 || directions[0] != .North ||
       directions[1] != .South { return 0 }
    contextual: [2]Direction = .[.South, .North]
    if contextual[0] != .South || contextual[1] != .North { return 0 }
    first: Named.Point
    first.x = 41
    second: Named.Point
    second.x = 1
    points := Named.Point.[first, second]
    if points.count != 2 || points[0].x + points[1].x != 42 {
        return 0
    }
    computed: [(CAPACITY + 1) * 2]s32
    computed[5] = 42
    if computed[5] != 42 { return 0 }
    if ReadLoop() != 42 { return 0 }
    large: LargeBox
    large.values[0].x = 41
    iteration: s32 = 0
    while iteration < 80 {
        if !CheckLarge(large) { return 0 }
        iteration += 1
    }
    moving: LargeBox = large
    iteration = 0
    while iteration < 80 {
        moving = RoundTripLarge(moving)
        iteration += 1
    }
    if large.values[0].x != 41 || moving.values[0].x != 42 {
        return 0
    }
    values: [THREE]s32 = .[41, 1, 0]
    copy: [THREE]s32 = values
    values[0] = 0
    if SumArray(copy) != 42 { return 0 }
    point: Point
    point.x = 41
    box: Box
    box.values[0] = point
    point.x = 1
    box.values[1] = point
    saved: Box = box
    point.x = 0
    box.values[0] = point
    if SumBox(saved) != 42 { return 0 }
    if MatrixGlobalAnswer() != 42 { return 0 }
    if AliasAnswer() != 22 { return 0 }
    return MatrixAnswer()
}

#program_export
OutOfBounds :: () -> s32 {
    values: [1]s32
    index: s32 = 1
    return values[index]
}

#program_export
NestedOutOfBounds :: () -> s32 {
    matrix: Matrix
    column: s32 = 3
    return matrix.values[0][column]
}
EOF

"$ziran" ir --root "$work" -o "$work/ir" "$work/arrayapp.zi"
if ! rg -aFq '.[41, 1, 0]' "$work/ir/arrayapp.zir" ||
   ! rg -aFq 's32.[41, 1]' "$work/ir/arrayapp.zir" ||
   ! rg -aFq 'Named.Point.[first, second]' "$work/ir/arrayapp.zir" ||
   rg -aFq '([THREE]s32){41, 1, 0}' "$work/ir/arrayapp.zir"; then
    echo 'saved array expression did not retain Jai initializer syntax' >&2
    exit 1
fi
for input in source saved; do
    if test "$input" = source; then
        module=$work/arrayapp.zi
        module_dir=$work
    else
        module=$work/ir/arrayapp.zir
        module_dir=$work/ir
    fi
    "$ziran" bundle --root "$module_dir" --entry arrayapp:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42

    for target in c go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main \
                --root "$module_dir" -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
func main() { if Arrayapp_Answer() != 42 { panic("array result") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$module_dir" \
                -o "$output" "$module"
            cat > "$output/main.c" <<'C'
#include "arrayapp.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        fi
    done
done

cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/c_style_array.zi" <<'EOF'
Answer :: () -> s32 {
    values: [2]s32 = ([2]s32){40, 2}
    return values[0] + values[1]
}
EOF
if "$ziran" check --root "$work" "$work/c_style_array.zi" \
    2> "$work/c_style_array.err"; then
    echo 'C-style array initializer unexpectedly passed checking' >&2
    exit 1
fi
grep -Fq 'C-style cast or literal is not valid Jai syntax' \
    "$work/c_style_array.err"

cat > "$work/capacity.zi" <<'EOF'
LENGTH :: 1 + 1
EOF
cat > "$work/capacity_app.zi" <<'EOF'
#import "capacity"
#program_export
Answer :: () -> s32 {
    values: [LENGTH * 2]s32 = .[40, 0, 0, 2]
    return values[0] + values[3]
}
EOF
"$ziran" ir --root "$work" -o "$work/capacity-ir" "$work/capacity_app.zi"
"$ziran" bundle --root "$work" --entry capacity_app:Answer \
    -o "$work/capacity-source.zx" "$work/capacity_app.zi"
"$ziran" bundle --root "$work/capacity-ir" --entry capacity_app:Answer \
    -o "$work/capacity-saved.zx" "$work/capacity-ir/capacity_app.zir"
cmp "$work/capacity-source.zx" "$work/capacity-saved.zx"
test "$("$ziran" run "$work/capacity-source.zx")" = 42

"$ziran" bundle --root "$work" --entry arrayapp:OutOfBounds \
    -o "$work/out-of-bounds.zx" "$work/arrayapp.zi"
if "$ziran" run "$work/out-of-bounds.zx" \
    2> "$work/out-of-bounds.err"; then
    echo 'portable array read escaped its bounds' >&2
    exit 1
fi
grep -Fq 'portable execution failed' "$work/out-of-bounds.err"

"$ziran" bundle --root "$work" --entry arrayapp:NestedOutOfBounds \
    -o "$work/nested-out-of-bounds.zx" "$work/arrayapp.zi"
if "$ziran" run "$work/nested-out-of-bounds.zx" \
    2> "$work/nested-out-of-bounds.err"; then
    echo 'portable nested array read escaped its bounds' >&2
    exit 1
fi
grep -Fq 'portable execution failed' "$work/nested-out-of-bounds.err"

cat > "$work/unresolved_inner.zi" <<'EOF'
Matrix :: struct {
    values: [2][MISSING * 2]s32
}
EOF
if "$ziran" check --root "$work" "$work/unresolved_inner.zi" \
    2> "$work/unresolved-inner.err"; then
    echo 'unresolved inner array capacity unexpectedly passed' >&2
    exit 1
fi
grep -Fq 'array capacity requires a known integer constant' \
    "$work/unresolved-inner.err"

cat > "$work/invalid_capacity.zi" <<'EOF'
Values :: struct {
    items: [2 / 0]s32
}
EOF
if "$ziran" check --root "$work" "$work/invalid_capacity.zi" \
    2> "$work/invalid-capacity.err"; then
    echo 'invalid arithmetic array capacity unexpectedly passed' >&2
    exit 1
fi
grep -Fq 'array capacity is not a valid bounded integer constant' \
    "$work/invalid-capacity.err"

cat > "$work/invalid_shadow.zi" <<'EOF'
CAPACITY :: 2
Answer :: () -> s32 {
    CAPACITY: s32 = 42
    return CAPACITY
}
EOF
if "$ziran" check --root "$work" "$work/invalid_shadow.zi" \
    2> "$work/invalid-shadow.err"; then
    echo 'local binding shadowed a compile-time definition' >&2
    exit 1
fi
grep -Fq 'binding shadows a compile-time definition' \
    "$work/invalid-shadow.err"
