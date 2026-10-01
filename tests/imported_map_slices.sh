#!/bin/sh
# Slices of the same imported map application retain native type identity;
# distinct value types remain incompatible even through nested slices.
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
cat > "$work/tables.zi" <<'ZI'
#import "std/map_go"
#import "std/go_types"
Tables :: struct { entries: Map(string, []Map(string, Any)); }
ZI
cat > "$work/app.zi" <<'ZI'
#import "std/map_go"
#import "std/go_types"
#import "tables"
builtin :: #system_library "go:builtin";
Allocate :: (count: isize) -> []Map(string, Any) #foreign builtin "make";
Answer :: () -> s32 {
    tables: Tables
    rows := Allocate(cast(isize)1)
    MapSet(rows[0], "value", cast(s64)42)
    MapSet(tables.entries, "rows", rows)
    copied := MapGet(tables.entries, "rows")
    MapSet(copied[0], "later", true)
    if copied.count != 1 || !MapContains(rows[0], "later") { return 1 }
    return 42
}
ZI
"$ziran" ir --root "$work" --module-path "std=$repo/std" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    root=$work
    input=$root/app.zi
    if test "$form" = saved; then root=$work/ir; input=$root/app.zir; fi
    "$ziran" build --target=go --pkg main --root "$root" --module-path "std=$repo/std" -o "$work/$form" "$input"
    printf 'package main\nfunc main() { if App_Answer() != 42 { panic("slice map identity") } }\n' > "$work/$form/main.go"
    GO111MODULE=off go run "$work/$form"/*.go
done
cat > "$work/bad.zi" <<'ZI'
#import "std/map_go"
#import "tables"
Bad :: () {
    tables: Tables
    row: Map(string, s32)
    rows: [1]Map(string, s32)
    rows[0] = row
    MapSet(tables.entries, "rows", rows[:])
}
ZI
if "$ziran" check --root "$work" --module-path "std=$repo/std" "$work/bad.zi" > "$work/bad.out" 2>&1; then
    echo 'distinct map slice element types were accepted' >&2
    exit 1
fi
rg -q 'map value type mismatch' "$work/bad.out"
