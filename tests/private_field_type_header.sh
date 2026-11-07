#!/bin/sh
# An exported record may hold #scope_module types in its fields, in a Vec
# or by value. The module's public C header defines those private types
# ahead of the record, so an importer can compile its layout, while Ziran
# code outside the module still cannot name them.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/state.zi" <<'ZI'
#import "vec"
#scope_export
State :: struct { items: Vec(Item); one: Item; }
MakeState :: () -> State {
    s: State;
    s.one = .{20, .{1}}
    unused VecPush(s.items, .{21, .{0}})
    return s
}
Sum :: (s: *State) -> s32 { return s.one.a + s.one.inner.b + s.items[0].a }
#scope_module
Item :: struct { a: s32; inner: Inner; }
Inner :: struct { b: s32; }
ZI
# The importer has its own Item; the two never meet in C.
cat > "$work/main.zi" <<'ZI'
#import "state"
Item :: struct { other: u8; }
Main :: () -> s32 {
    s := MakeState()
    mine: Item
    mine.other = 0
    return Sum(*s) + cast(s32) mine.other
}
ZI

"$ziran" check --root "$work" "$work/main.zi"
"$ziran" bundle --root "$work" --entry main:Main -o "$work/app.zx" "$work/main.zi"
test "$("$ziran" run "$work/app.zx")" = 42

"$ziran" build --target=c --root "$work" --entry main:Main --exe \
    -o "$work/c" "$work/main.zi"
status=0
"$work/c/main" || status=$?
test "$status" = 42

# The private types still stay unnamed in Ziran outside their module.
mkdir "$work/reject"
cp "$work/state.zi" "$work/reject/state.zi"
cat > "$work/reject/main.zi" <<'ZI'
#import "state"
Main :: () -> s32 { item: Inner; return item.b }
ZI
if "$ziran" check --root "$work/reject" "$work/reject/main.zi" 2> "$work/reject.err"; then
    echo "a module-private field type was usable outside its module" >&2
    exit 1
fi
echo 'Exported records hold module-private field types through C and .zx'
