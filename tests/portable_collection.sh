#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "vec"

Item :: struct {
    number: s32
    label: string
}

items: Vec(Item);

PushOne :: (number: s32) -> bool {
    item: Item
    item.number = number
    item.label = "retained"
    return VecPush(items, item)
}

#program_export
Answer :: () -> s32 {
    marker: Item
    marker.number = 7
    index: s32 = 0
    while index < 12000 {
        if !PushOne(index) { return -1 }
        index += 1
    }
    if marker.number != 7 || items.count != 12000 ||
       items[0].number != 0 || items[5999].number != 5999 ||
       items[11999].number != 11999 || items[11999].label != "retained" {
        return -2
    }
    VecFree(items)
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/std" \
    --entry app:Answer -o "$work/source.zx" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$repo/std" \
    --entry app:Answer -o "$work/saved.zx" "$work/ir/app.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42
