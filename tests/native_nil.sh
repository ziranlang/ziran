#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/pointers.zi" <<'EOF'
#import "helper"

Cell :: struct {
    value: s32
}

#program_export
Present :: (value: *s32) -> bool {
    return value != null
}

#program_export
Choose :: (use_value: bool, value: *s32) -> *s32 {
    return ifx use_value then value else null
}

#program_export
Answer :: () -> s32 {
    pointer: *s32 = null
    value: s32 = 40
    pointer = *value
    pointer.* = pointer.* + 1
    <<pointer = <<pointer + 1
    if false then <<pointer = 0
    if value != 42 { return 0 }
    if <<pointer != 42 { return 0 }
    if <<*value != 42 { return 0 }
    pointer_pointer: **s32 = *pointer
    if pointer_pointer.*.* != 42 { return 0 }
    if << <<pointer_pointer != 42 { return 0 }
    if Choose(true, pointer).* != 42 { return 0 }
    if <<Choose(true, pointer) != 42 { return 0 }
    if (1 << 1) != 2 { return 0 }
    pointer = null
    cell: Cell
    cell.value = 41
    cell_pointer: *Cell = *cell
    cell_pointer.*.value = 42
    if cell.value != 42 { return 0 }
    if cell_pointer.value != 42 { return 0 }
    if cell_pointer.*.value != 42 { return 0 }
    if Present(pointer) { return 0 }
    if Present(null) { return 0 }
    if RejectNull(null) { return 0 }
    if Choose(false, pointer) != null { return 0 }
    pointer = null
    return 42
}
EOF

cat > "$work/helper.zi" <<'EOF'
#program_export
RejectNull :: (value: *s32) -> bool {
    return value != null
}
EOF

"$ziran" ir --root "$work" -o "$work/ir" "$work/pointers.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/pointers.zi
    else
        module=$work/ir/pointers.zir
    fi
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
func main() { if Pointers_Answer() != 42 { panic("nil pointer") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$work" \
                -o "$output" "$module"
            cat > "$output/main.c" <<'C'
#include "pointers.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --root "$work" \
                -o "$output" "$module"
            cat > "$output/main.cpp" <<'CPP'
#include "pointers.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done

cat > "$work/invalid.zi" <<'EOF'
Wrong :: () -> s32 {
    value: s32 = null
    return value
}
EOF
if "$ziran" check --root "$work" "$work/invalid.zi" \
    2> "$work/invalid.err"; then
    echo 'integer accepted null initializer' >&2
    exit 1
fi
grep -Fq 'initializer type mismatch' "$work/invalid.err"

cat > "$work/invalid.zi" <<'EOF'
Wrong :: () -> s32 {
    value := null
    return 0
}
EOF
if "$ziran" check --root "$work" "$work/invalid.zi" \
    2> "$work/invalid.err"; then
    echo 'untyped null binding was accepted' >&2
    exit 1
fi
grep -Fq 'null requires an explicit pointer type' "$work/invalid.err"

cat > "$work/invalid.zi" <<'EOF'
Wrong :: () -> *const u8 {
    return null
}
EOF
if "$ziran" check --root "$work" "$work/invalid.zi" \
    2> "$work/invalid.err"; then
    echo 'C-style const pointer type was accepted' >&2
    exit 1
fi
grep -Fq 'const qualifier is not Jai syntax' "$work/invalid.err"

cat > "$work/invalid.zi" <<'EOF'
Wrong :: () -> *s32 {
    return nil
}
EOF
if "$ziran" check --root "$work" "$work/invalid.zi" \
    2> "$work/invalid.err"; then
    echo 'non-Jai nil literal was accepted' >&2
    exit 1
fi
grep -Fq 'nil is not Jai syntax; use null' "$work/invalid.err"

cat > "$work/invalid.zi" <<'EOF'
Wrong :: () -> s32 {
    value: s32 = 42
    pointer: *s32 = &value
    return pointer.*
}
EOF
if "$ziran" check --root "$work" "$work/invalid.zi" \
    2> "$work/invalid.err"; then
    echo 'C-style address-of was accepted' >&2
    exit 1
fi
grep -Fq 'C-style address-of is not valid Jai syntax' "$work/invalid.err"

cat > "$work/invalid.zi" <<'EOF'
Wrong :: () -> s32 {
    value: s32 = 42
    return value.*
}
EOF
if "$ziran" check --root "$work" "$work/invalid.zi" \
    2> "$work/invalid.err"; then
    echo 'dereference of a scalar was accepted' >&2
    exit 1
fi
grep -Fq 'dereference requires a pointer' "$work/invalid.err"

cat > "$work/invalid.zi" <<'EOF'
Wrong :: () -> s32 {
    value: s32 = 42
    return <<value
}
EOF
if "$ziran" check --root "$work" "$work/invalid.zi" \
    2> "$work/invalid.err"; then
    echo 'prefix dereference of a scalar was accepted' >&2
    exit 1
fi
grep -Fq 'dereference requires a pointer' "$work/invalid.err"

for increment in 'value++' '++value' 'value--' '--value'; do
    cat > "$work/invalid.zi" <<EOF
Wrong :: () -> s32 {
    value: s32 = 42
    $increment
    return value
}
EOF
    if "$ziran" check --root "$work" "$work/invalid.zi" \
        2> "$work/invalid.err"; then
        echo "non-Jai increment/decrement was accepted: $increment" >&2
        exit 1
    fi
    grep -Fq 'Jai has no increment or decrement operators' "$work/invalid.err"
done

cat > "$work/invalid.zi" <<'EOF'
Wrong :: () -> s32 {
    pointer: *s32 = *42
    return 0
}
EOF
if "$ziran" check --root "$work" "$work/invalid.zi" \
    2> "$work/invalid.err"; then
    echo 'address of a literal was accepted' >&2
    exit 1
fi
grep -Fq 'address-of requires an assignable expression' "$work/invalid.err"

# The portable runner reads and writes through pointers at its own storage.
"$ziran" bundle --root "$work" --entry pointers:Answer \
    -o "$work/pointers.zib" "$work/pointers.zi"
test "$("$ziran" run "$work/pointers.zib")" = 42
