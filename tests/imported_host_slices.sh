#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/records.zi" <<'ZI'
Item :: struct { value: s32 }
ZI
cat > "$work/api.zi" <<'ZI'
using Records :: #import "records";
Wrapper :: struct { item: Item }
ZI
cat > "$work/main.zi" <<'ZI'
#import "api"
builtin :: #system_library "go:builtin";
Allocate :: (count: isize) -> []Item #foreign builtin "make";
Append :: (items: []Item, item: Item) -> []Item #foreign builtin "append";
Answer :: () -> s32 {
    values := Allocate(0)
    item: Item
    item.value = 42
    values = Append(values, item)
    return values[0].value
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/main.zi"
for input in source saved; do
    root=$work
    extension=zi
    if test "$input" = saved; then root=$work/ir; extension=zir; fi
    for order in consumer-first owner-first; do
        if test "$order" = consumer-first; then
            set -- "$root/main.$extension" "$root/api.$extension" "$root/records.$extension"
        else
            set -- "$root/records.$extension" "$root/api.$extension" "$root/main.$extension"
        fi
        out=$work/$input-$order
        "$ziran" build --target=go --no-main --pkg main --root "$root" -o "$out" "$@"
        cat > "$out/entry.go" <<'GO'
package main
func main() {
    if Main_Answer() != 42 { panic("transitive slice element") }
}
GO
        GO111MODULE=off go run -race "$out"/*.go
    done
done
echo 'Foreign slice returns resolve transitive imports independently of module order: passed'
