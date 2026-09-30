#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/records.zi" <<'ZI'
Item :: struct { value: s32 }
Pair :: struct($T: Type) { value: T }
ZI
cat > "$work/api.zi" <<'ZI'
using Records :: #import "records";
Wrapper :: struct { item: Item }
ZI
cat > "$work/main.zi" <<'ZI'
#import "api"
#import "map_go"
#import "go_types"
builtin :: #system_library "go:builtin";
Allocate :: (count: isize) -> []Item #foreign builtin "make";
Append :: (items: []Item, item: Item) -> []Item #foreign builtin "append";
AllocateRows :: (count: isize) -> []Map(string, Any) #foreign builtin "make";
AppendRow :: (items: []Map(string, Any), item: Map(string, Any)) -> []Map(string, Any) #foreign builtin "append";
AllocatePairs :: (count: isize) -> []Pair(s32) #foreign builtin "make";
Rows :: () -> []Map(string, Any) {
    result := AllocateRows(0)
    row: Map(string, Any)
    MapSet(row, "value", cast(isize)42)
    return AppendRow(result, row)
}
EmptyRows :: () -> []Map(string, Any) {
    return AllocateRows(0)
}
Pairs :: () -> []Pair(s32) {
    result := AllocatePairs(1)
    result[0].value = 42
    return result
}
Answer :: () -> s32 {
    values := Allocate(0)
    item: Item
    item.value = 42
    values = Append(values, item)
    return values[0].value
}
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/main.zi"
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
        "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$@"
        cat > "$out/entry.go" <<'GO'
package main
func main() {
    if Main_Answer() != 42 { panic("transitive slice element") }
    var rows []map[string]any = Main_Rows()
    if len(rows) != 1 || cap(rows) != 1 || rows[0]["value"] != int(42) {
        panic("generic map slice return")
    }
    rows[0]["value"] = int(7)
    if Main_Rows()[0]["value"] != int(42) { panic("map slice storage shared across calls") }
    empty := Main_EmptyRows()
    if empty == nil || len(empty) != 0 || cap(empty) != 0 { panic("empty map slice became nil") }
    pairs := Main_Pairs()
    if len(pairs) != 1 || pairs[0].Value != int32(42) { panic("imported generic record slice") }
}
GO
        GO111MODULE=off go run -race "$out"/*.go
    done
done
for element in 'Missing' 'Missing(s32)' 'Map(string)' '[][]u8'; do
    printf '#import "map_go"\nbuiltin :: #system_library "go:builtin";\nBad :: (count: isize) -> []%s #foreign builtin "make";\n' "$element" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid foreign slice element accepted: $element" >&2
        exit 1
    fi
done
echo 'Foreign slices resolve transitive imports and generic elements independently of module order: passed'
