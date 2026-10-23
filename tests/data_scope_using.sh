#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/lib/native_answer.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Point :: struct { x: s64; y: s64; }
Entity :: struct { position: Point; }
using shared: Point;
LibValue :: () -> s64 { x = 7; return x }
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib"
using, only("x") point;
point: Point;
using, only("x") point;
using, map("nested_x" = "x") entity.position;
entity: Entity;
using, only("y") prepared: Point = Point.{.x = 20, .y = 22};
Overlay :: union { left: s64; right: s64; }
using, map("union_left" = "left") overlay: Overlay;
#program_export
Answer :: () -> s32 {
    x = prepared.x
    nested_x = y
    union_left = 42
    {
        x: s64 = 9
        if x != 9 { return 0 }
    }
    if point.x != 20 || entity.position.x != 22 { return 0 }
    if overlay.right != 42 { return 0 }
    if LibValue() != 7 { return 0 }
    return cast(s32) (x + nested_x)
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

for input in "$work/app.zi" "$work/ir/app.zir"; do
    case "$input" in
        *.zi) suffix=source; root=$work ;;
        *) suffix=saved; root=$work/ir ;;
    esac
    for target in c cpp go; do
        out="$work/$target-$suffix"
        native_answer_42 "$target" "$root" "$out" "$input" \
            app App_Answer 'data scope using'
    done
done

cat > "$work/file_expr.zi" <<'ZI'
Point :: struct { x: s64; y: s64; }
Entity :: struct { position: Point; }
using, map("left" = "x") seed: Point = Point.{.x = 17, .y = 22};
using, only("y") seed;
using, map("nested" = "x") entity.position;
entity: Entity = Entity.{.position = Point.{.x = 1, .y = 0}};
delta: s64 = 2;
sum: s64 = left + y + nested + delta;
#program_export
Answer :: () -> s64 { return sum }
ZI
"$ziran" ir --root "$work" -o "$work/file-ir" "$work/file_expr.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/file_expr.zi
        root=$work
    else
        module=$work/file-ir/file_expr.zir
        root=$work/file-ir
    fi
    "$ziran" bundle --root "$root" --entry file_expr:Answer \
        -o "$work/file-$input.zib" "$module"
    test "$("$ziran" run "$work/file-$input.zib")" = 42
    for target in c cpp go; do
        out="$work/file-$target-$input"
        native_answer_42 "$target" "$root" "$out" "$module" \
            file_expr FileExpr_Answer 'file using'
    done
done
cmp "$work/file-source.zib" "$work/file-saved.zib"

cat > "$work/file_shadow.zi" <<'ZI'
Point :: struct { x: s64; y: s64; }
using seed: Point = Point.{.x = 18, .y = 22};
x :: 40
sum: s64 = x + seed.y;
#program_export
Answer :: () -> s64 { return sum }
ZI
"$ziran" bundle --root "$work" --entry file_shadow:Answer \
    -o "$work/file-shadow.zib" "$work/file_shadow.zi"
test "$("$ziran" run "$work/file-shadow.zib")" = 62

cat > "$work/file_ambiguous.zi" <<'ZI'
Point :: struct { x: s64; }
using first: Point = Point.{.x = 20};
using second: Point = Point.{.x = 22};
sum: s64 = x;
ZI
if "$ziran" check --root "$work" "$work/file_ambiguous.zi" \
    2> "$work/file_ambiguous.err"; then
    echo 'ambiguous file-scope initializer using was accepted' >&2
    exit 1
fi
grep -Fq 'ambiguous using field: x' "$work/file_ambiguous.err"

cat > "$work/file_mixed_ambiguous.zi" <<'ZI'
Kind :: enum { x :: 1 }
using Kind;
Point :: struct { x: s64; }
using point: Point = Point.{.x = 42};
sum: s64 = x;
ZI
if "$ziran" check --root "$work" "$work/file_mixed_ambiguous.zi" \
    2> "$work/file_mixed_ambiguous.err"; then
    echo 'record and enum using exposed the same initializer name' >&2
    exit 1
fi
grep -Fq 'ambiguous using field: x' "$work/file_mixed_ambiguous.err"

cat > "$work/ambiguous.zi" <<'ZI'
Box :: struct { value: s64; }
using first: Box;
using second: Box;
Bad :: () -> s64 { return value }
ZI
if "$ziran" check --root "$work" "$work/ambiguous.zi" \
    2> "$work/ambiguous.err"; then
    echo 'ambiguous file-scope record using was accepted' >&2
    exit 1
fi
grep -Fq 'ambiguous using field: value' "$work/ambiguous.err"

cat > "$work/scalar.zi" <<'ZI'
using number;
number: s64;
ZI
if "$ziran" check --root "$work" "$work/scalar.zi" \
    2> "$work/scalar.err"; then
    echo 'file-scope scalar using was accepted' >&2
    exit 1
fi
grep -Fq 'using requires a concrete record binding' "$work/scalar.err"

cat > "$work/shadow.zi" <<'ZI'
Point :: struct { x: s64; }
using point: Point;
Bad :: () -> s64 {
    point: Point;
    return x
}
ZI
if "$ziran" check --root "$work" "$work/shadow.zi" \
    2> "$work/shadow.err"; then
    echo 'shadowed file-scope using root still promoted fields' >&2
    exit 1
fi
grep -Fq 'unresolved name: x' "$work/shadow.err"

cat > "$work/private.zi" <<'ZI'
#scope_file
using private: Point;
#scope_module
Inside :: () -> s64 { x = 11; return x }
ZI
cat > "$work/private_app.zi" <<'ZI'
Point :: struct { x: s64; }
#load "private.zi";
Bad :: () -> s64 { return x }
ZI
cat > "$work/private_ok.zi" <<'ZI'
Point :: struct { x: s64; }
#load "private.zi";
Answer :: () -> s64 { return Inside() }
ZI
"$ziran" check --root "$work" "$work/private_ok.zi"
if "$ziran" check --root "$work" "$work/private_app.zi" \
    2> "$work/private.err"; then
    echo 'file-private record using leaked across #load' >&2
    exit 1
fi
grep -Fq 'unresolved name: x' "$work/private.err"

cat > "$work/private_init.zi" <<'ZI'
Point :: struct { x: s64; }
#load "private.zi";
sum: s64 = x;
ZI
if "$ziran" check --root "$work" "$work/private_init.zi" \
    2> "$work/private_init.err"; then
    echo 'file-private using leaked into a global initializer' >&2
    exit 1
fi
grep -Fq 'unresolved name: x' "$work/private_init.err"
