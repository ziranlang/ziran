#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/lib/native_answer.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Record :: struct { value: s32; }
Identity :: (value: $T) -> T { return value }
Agree :: (left: $T, right: T) -> T { return right }
Nested :: (value: $T) -> T { return Identity(Identity(value)) }
Field :: (using item: $T) -> s32 { return value }
LocalField :: (item: $T) -> s32 {
    using local: T = item
    return value
}
PathField :: (item: $T) -> s32 {
    using item.position;
    return value
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib"
Local :: struct { value: s32; }
NestedLocal :: struct { position: Local; }
#program_export
Answer :: () -> s32 {
    x: s32 = 30
    record: Record = Record.{value = 2}
    local: Local = Local.{value = 8}
    fraction: float32 = 1.5
    empty := s32.[]
    returned: [0]s32 = Identity(empty)
    if returned.count != 0 || returned.data != null { return 0 }
    if Identity(fraction) != 1.5 { return 0 }
    if Field(record) != 2 { return 0 }
    if LocalField(local) != 8 { return 0 }
    nested: NestedLocal = NestedLocal.{position = Local.{value = 42}}
    if PathField(nested) != 42 { return 0 }
    return x + Agree(x, record.value) + Identity(record).value + Nested(local).value
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zx" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zx" "$work/ir/app.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42

for input in "$work/app.zi" "$work/ir/app.zir"; do
    case "$input" in
        *.zi) suffix=source; root=$work ;;
        *) suffix=saved; root=$work/ir ;;
    esac
    for target in c go; do
        out="$work/$target-$suffix"
        native_answer_42 "$target" "$root" "$out" "$input" \
            app App_Answer 'polymorphic result'
    done
done

# A saved library still has a template body and can specialize a caller's
# record type. A named import also preserves calls inside the template.
"$ziran" ir --root "$work" -o "$work/library-ir" "$work/lib.zi"
cat > "$work/library-ir/named.zi" <<'ZI'
Lib :: #import "lib";
Local :: struct { value: s32; }
NestedLocal :: struct { position: Local; }
#program_export
Answer :: () -> s32 {
    record: Local = Local.{value = 42}
    nested: NestedLocal = NestedLocal.{position = record}
    if Lib.Field(record) != 42 { return 0 }
    if Lib.LocalField(record) != 42 { return 0 }
    if Lib.PathField(nested) != 42 { return 0 }
    return Lib.Nested(record).value
}
ZI
"$ziran" check --root "$work/library-ir" "$work/library-ir/named.zi"
"$ziran" build --target=c --root "$work/library-ir" \
    -o "$work/mixed" "$work/library-ir/named.zi"
cat > "$work/mixed/main.c" <<'C'
#include "named.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
"${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/mixed" \
    "$work/mixed"/*.c -o "$work/mixed/app"
"$work/mixed/app"

# A generic's copy whose signature spells a type through the template's own
# `using` import is declared behind its caller's _PRIVATE guard when the
# caller's module-scoped import takes that name (Kryon's text Draw called
# from Terminal). The caller's source must still see the declaration.
mkdir -p "$work/scoped"
cat > "$work/scoped/paint_props.zi" <<'ZI'
Props :: struct { size: s32; }
ZI
cat > "$work/scoped/frame.zi" <<'ZI'
Frame :: struct { width: s32; }
ZI
cat > "$work/scoped/paint.zi" <<'ZI'
using Text :: #import "paint_props";
painted: s32;
#scope_package
#if #defined(PAINT_HOSTED) {
    Paint :: (frame: $T, props: Text.Props) { painted = -1 }
} else {
    Measure :: (width: s32, props: Text.Props) { painted += width * props.size }
    Paint :: (frame: $T, props: Text.Props) {
        total: s32
        for step: 0..frame.width - 1 {
            if step % 3 == 0 { total += step } else if step % 3 == 1 { total -= step } else { total ^= step }
        }
        Measure(frame.width + (total & 0), props)
    }
}
ZI
cat > "$work/scoped/caller.zi" <<'ZI'
#scope_module
Painter :: #import "paint"
Frames :: #import "frame"
Text :: #import "std/text_buffer"
#scope_export
Draw :: (frame: Frames.Frame, scale: s32) {
    if scale > 0 {
        props: Painter.Props
        props.size = 6 * scale
        Painter.Paint(frame, props)
    }
}
#program_export
Answer :: () -> s32 {
    Draw(Frames.Frame.{width = 7}, 1)
    return Painter.painted
}
ZI
# Package-private templates and retained helpers must keep valid visibility
# when the checked graph is saved, then consumed without source declarations.
"$ziran" ir --root "$work/scoped" -o "$work/scoped/ir" "$work/scoped/caller.zi"
for source in source saved; do
    root="$work/scoped"
    input="$root/caller.zi"
    if [ "$source" = saved ]; then root="$root/ir"; input="$root/caller.zir"; fi
    out="$work/scoped/c-$source"
    "$ziran" build --target=c --no-main --root "$root" -o "$out" "$input"
    cat > "$out/main.c" <<'C'
#include "caller.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -Werror=implicit-function-declaration -I"$repo/include" -I"$out" \
        "$out"/*.c -o "$out/app"
    "$out/app"
done

cat > "$work/ambiguous.zi" <<'ZI'
Identity :: (value: $T) -> T { return value }
Answer :: () -> s32 {
    value := Identity(1)
    return value
}
ZI
if "$ziran" check --root "$work" "$work/ambiguous.zi" \
    2> "$work/ambiguous.err"; then
    echo 'untyped polymorphic literal was accepted' >&2
    exit 1
fi
rg -q 'polymorphic literal needs a concrete type' "$work/ambiguous.err"

cat > "$work/mismatch.zi" <<'ZI'
Same :: (left: $T, right: T) -> T { return left }
Answer :: () -> s32 {
    left: s32 = 1
    right: float32 = 2.0
    return Same(left, right)
}
ZI
if "$ziran" check --root "$work" "$work/mismatch.zi" \
    2> "$work/mismatch.err"; then
    echo 'mismatched polymorphic arguments were accepted' >&2
    exit 1
fi
rg -q 'argument type mismatch: Same' "$work/mismatch.err"

# Several type parameters are allowed; each $Name binds once
# (tests/generic_parameters.sh runs them).
cat > "$work/multiple.zi" <<'ZI'
Bad :: (left: $T, right: $T) -> T { return left }
ZI
if "$ziran" check --root "$work" "$work/multiple.zi" \
    2> "$work/multiple.err"; then
    echo 'a type parameter bound twice was accepted' >&2
    exit 1
fi
rg -q 'binds its type more than once' "$work/multiple.err"

cat > "$work/scalar_using.zi" <<'ZI'
Field :: (using item: $T) -> s32 { return value }
Answer :: () -> s32 {
    value: s32 = 42
    return Field(value)
}
ZI
if "$ziran" check --root "$work" "$work/scalar_using.zi" \
    2> "$work/scalar_using.err"; then
    echo 'using on a scalar polymorphic instance was accepted' >&2
    exit 1
fi
rg -q 'using requires a concrete record binding' "$work/scalar_using.err"
