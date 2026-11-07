#!/bin/sh
# VecSlice views a global Vec, or a Vec field of a global record, under the
# same borrow as a local one: while the view lives, the Vec cannot grow, be
# freed or reassigned, directly or through any procedure that does so, and
# the view cannot outlive its scope.
set -eu
. "$(dirname -- "$0")/lib/expect_rejection.sh"
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/global_view.zi" <<'ZI'
#import "vec"
Store :: struct { items: Vec(s64); }
values: Vec(s64);
store: Store;

Sum :: (view: []s64) -> s64 {
    total: s64 = 0
    for view { total += it }
    return total
}

Read :: () -> s64 { return values[0] }

#program_export
Main :: () {
    for 1..4 { unused VecPush(values, it * 10) }
    for 1..3 { unused VecPush(store.items, it) }
    {
        view: []s64 = VecSlice(values, 1, 3)
        part: []s64 = store.items[1:]
        values[0] = 5
        print("% % % % %\n", view.count, Sum(view), Sum(part), Read(), view[0])
    }
    // The views closed with their block, so the Vecs can grow again.
    unused VecPush(values, 50)
    unused VecPush(store.items, 4)
    print("% %\n", Sum(VecSlice(values, 0, values.count)), Sum(store.items[0:]))
    VecFree(values)
    VecFree(store.items)
}
ZI
printf '2 50 5 5 20\n145 10\n' > "$work/expected"
"$ziran" check --root "$work" --module-path "$repo/std" "$work/global_view.zi"
"$ziran" bundle --root "$work" --module-path "$repo/std" \
    --entry global_view:Main -o "$work/global_view.zx" "$work/global_view.zi"
"$ziran" run "$work/global_view.zx" > "$work/zx.out"
cmp "$work/expected" "$work/zx.out"
"$ziran" build --target=c --root "$work" --module-path "$repo/std" \
    --entry global_view:Main -o "$work/c" "$work/global_view.zi"
printf '#include "global_view.h"\nint main(void) { Main(); return 0; }\n' > "$work/host.c"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$repo/include" -I"$work/c" \
    "$work/c"/*.c "$work/host.c" -o "$work/app"
"$work/app" > "$work/c.out"
cmp "$work/expected" "$work/c.out"

# Each body below writes the viewed global while VIEW is live.
reject() {
    name=$1
    message=$2
    body=$3
    cat > "$work/$name.zi" <<ZI
#import "vec"
Store :: struct { items: Vec(s64); }
values: Vec(s64);
store: Store;
Grow :: () { unused VecPush(values, 9) }
GrowLater :: () { Grow() }
Replace :: () {
    fresh: Vec(s64)
    values = fresh
}
#program_export
Bad :: () -> s64 {
    unused VecPush(values, 1)
    unused VecPush(store.items, 1)
    view: []s64 = VecSlice(values, 0, 1)
    part: []s64 = store.items[0:1]
$body
    return view[0] + part[0]
}
ZI
    expect_rejection "$message" "$work/$name.err" \
        "$ziran" check --root "$work" --module-path "$repo/std" "$work/$name.zi"
    if ! rg -q 'live borrowed view' "$work/$name.err"; then
        cat "$work/$name.err" >&2
        exit 1
    fi
}
reject push 'pushing to a viewed global Vec was accepted' '    unused VecPush(values, 2)'
reject field 'pushing to a viewed global record Vec was accepted' '    unused VecPush(store.items, 2)'
reject free 'freeing a viewed global Vec was accepted' '    VecFree(values)'
reject call 'a call that grows a viewed global Vec was accepted' '    Grow()'
reject transitive 'a transitive call that grows a viewed global Vec was accepted' '    GrowLater()'
reject assign 'replacing a viewed global Vec in a callee was accepted' '    Replace()'

# A view of a global cannot leave its scope through a return or a global.
cat > "$work/escape.zi" <<'ZI'
#import "vec"
values: Vec(s64);
Peek :: () -> []s64 { return VecSlice(values, 0, 1) }
#program_export
Bad :: () -> s64 {
    unused VecPush(values, 1)
    view := Peek()
    unused VecPush(values, 2)
    return view[0]
}
ZI
expect_rejection 'a view of a global Vec escaped and outlived a push' \
    "$work/escape.err" \
    "$ziran" check --root "$work" --module-path "$repo/std" "$work/escape.zi"
cat > "$work/kept.zi" <<'ZI'
#import "vec"
values: Vec(s64);
kept: []s64;
#program_export
Bad :: () -> s64 {
    unused VecPush(values, 1)
    kept = VecSlice(values, 0, 1)
    unused VecPush(values, 2)
    return kept[0]
}
ZI
expect_rejection 'a view of a global Vec stored in a global outlived a push' \
    "$work/kept.err" \
    "$ziran" check --root "$work" --module-path "$repo/std" "$work/kept.zi"
cat > "$work/outer.zi" <<'ZI'
#import "vec"
values: Vec(s64);
#program_export
Bad :: () -> s64 {
    unused VecPush(values, 1)
    view: []s64
    {
        view = VecSlice(values, 0, 1)
    }
    unused VecPush(values, 2)
    return view[0]
}
ZI
expect_rejection 'a view of a global Vec kept in an outer binding outlived a push' \
    "$work/outer.err" \
    "$ziran" check --root "$work" --module-path "$repo/std" "$work/outer.zi"
for name in escape kept outer; do
    if ! rg -q 'views a global Vec|escape its backing storage' "$work/$name.err"; then
        cat "$work/$name.err" >&2
        exit 1
    fi
done
echo 'VecSlice of global Vec places: passed'
