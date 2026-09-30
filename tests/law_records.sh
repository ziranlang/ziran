#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY
ziran=${1:?pass ziran}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Laws over functions that build records field by field: a local without an
# initializer holds its type's zero, `name.field = value` and `name.a.b += n`
# change one field, and records compare by value whatever order or subset of
# fields a literal names.
cat > "$work/records.zi" <<'ZI'
Point :: struct {
    x: s32
    y: s32
}

Box :: struct {
    origin: Point
    size: s32
    open: bool
    name: string
}

Build :: (x: s32, open: bool) -> Box {
    box: Box
    box.origin.x = x
    box.size += 2
    box.size *= 3
    box.open = open
    return box
}

Origin :: () -> Point {
    point: Point
    return point
}

Counter :: () -> s32 {
    count: s32
    count += 3
    return count
}

#law FieldsByPath forall x: -3..3, open: 0..1 => Build(x, open != 0) == Box.{.origin = Point.{.x = x}, .size = 6, .open = open != 0};
#law FieldRead forall x: -3..3 => Build(x, false).origin.x == x && Build(x, false).origin.y == 0;
#law OrderDoesNotMatter custom Point.{.y = 1, .x = 2} == Point.{2, 1};
#law ZeroRecord custom Origin() == Point.{0, 0};
#law ZeroScalar custom Counter() == 3;

#program_export
Answer :: () -> s32 {
    box := Build(4, true)
    if box.origin.x != 4 || box.origin.y != 0 || box.size != 6 || !box.open { return 1 }
    if Origin().x != 0 || Counter() != 3 { return 2 }
    return 42
}
ZI
cat > "$work/false.zi" <<'ZI'
Point :: struct {
    x: s32
    y: s32
}

Moved :: (x: s32) -> Point {
    point: Point
    point.x = x
    return point
}

#law WrongField forall x: 0..2 => Moved(x) == Point.{.y = x};

#program_export
Answer :: () -> s32 { return 0 }
ZI

"$ziran" check --root "$work" "$work/records.zi" > "$work/laws.json"
test "$(grep -c '"status":"proved"' "$work/laws.json")" = 5
if "$ziran" check --root "$work" "$work/false.zi" > "$work/false.json" 2> "$work/false.err"; then
    echo 'a false record law was accepted' >&2; exit 1
fi
grep -q '"status":"disproved"' "$work/false.json"
"$ziran" ir --root "$work" -o "$work/ir" "$work/records.zi"
"$ziran" check --root "$work/ir" "$work/ir/records.zir" > "$work/saved.json"
cmp "$work/laws.json" "$work/saved.json"
"$ziran" bundle --root "$work" --entry records:Answer -o "$work/records.zib" "$work/records.zi"
test "$("$ziran" run "$work/records.zib")" = 42
"$ziran" build --target=c --no-main --root "$work" -o "$work/c" "$work/records.zi"
cat > "$work/c/main.c" <<'C'
#include "records.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
${CC:-cc} -I"$repo/include" -I"$work/c" "$work/c/records.c" "$work/c/main.c" -lm -o "$work/c/app"
"$work/c/app"
