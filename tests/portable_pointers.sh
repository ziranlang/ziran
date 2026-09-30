#!/bin/sh
# The portable runner supports pointers to its own storage, as native targets
# do: *place takes the address of a local, global, record field, or array
# element; <<p reads and writes through it; p.field reaches a pointed-to
# record; pointers compare by target and with null. A pointer at a local
# fails cleanly once its call has returned instead of reading freed storage.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Point :: struct { x: s32; y: s32; }
Holder :: struct { target: *Point; count: s32; }

total: s32 = 0;

Bump :: (p: *Point) { p.x += 1; (<<p).y += 2; }
Set :: (value: *s32, to: s32) { <<value = to; }
Pick :: (a: *Point, b: *Point, first: bool) -> *Point {
    if first { return a; }
    return b;
}

#program_export
Answer :: () -> s32 {
    p: Point;
    Bump(*p);
    Bump(*p);
    if p.x != 2 || p.y != 4 { return 1; }
    Set(*p.y, 9);
    if p.y != 9 { return 2; }
    points: [3]Point;
    Bump(*points[1]);
    if points[1].x != 1 || points[0].x != 0 { return 3; }
    Set(*points[2].x, 7);
    if points[2].x != 7 { return 4; }
    Set(*total, 5);
    if total != 5 { return 5; }
    q := Pick(*p, *points[1], false);
    q.x = 40;
    if points[1].x != 40 || p.x != 2 { return 6; }
    holder: Holder;
    if holder.target != null { return 7; }
    holder.target = *p;
    holder.target.y += 1;
    if p.y != 10 || holder.target == null { return 8; }
    if Pick(*p, *p, true) != *p || *p == *points[0] { return 9; }
    copy := <<holder.target;
    copy.x = 100;
    if p.x != 2 { return 10; }
    return 42;
}
ZI

cat > "$work/dangling.zi" <<'ZI'
Escape :: () -> *s32 {
    local: s32 = 3;
    return *local;
}
#program_export
Answer :: () -> s32 {
    p := Escape();
    return <<p;
}
ZI
"$ziran" bundle --root "$work" --entry dangling:Answer -o "$work/dangling.zib" "$work/dangling.zi"
if "$ziran" run "$work/dangling.zib" > "$work/dangling.out" 2>&1; then
    echo 'a pointer at a returned local was read' >&2
    exit 1
fi

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 42
    output="$work/c-$input"
    "$ziran" build --target=c --exe --entry app:Answer --root "$root" -o "$output" "$module"
    status=0
    "$output/app" || status=$?
    test "$status" = 42
    output="$work/cpp-$input"
    "$ziran" build --target=cpp --root "$root" -o "$output" "$module"
    cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    "${CXX:-c++}" -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
    "$output/app"
    output="$work/go-$input"
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("wrong result") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zib" "$work/saved.zib"
