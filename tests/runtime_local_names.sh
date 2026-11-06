#!/bin/sh
# Locals and parameters spelled like the C library or Go builtins that
# generated code calls, such as `free := ...` beside a freed Text, are
# renamed in C and Go instead of hiding those calls.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/names.zi" <<'ZI'
#import "vec"
Make :: (a: string, b: string) -> Text {
    out: Vec(u8)
    if !BuilderAppend(out, a) || !BuilderAppend(out, b) { return .{} }
    return BuilderFinish(out)
}
Take :: (s: string) -> s64 { return s.count }
Grow :: (malloc: s64, len: s64) -> s64 {
    v: Vec(s64)
    VecPush(v, malloc)
    VecPush(v, len)
    return v[0] + v[1] + cast(s64) v.count
}

#program_export
Answer :: () -> s32 {
    free := Take(Make("ab", "c")) == 3
    memcpy := Make("x", "y")
    copy := Take(memcpy) + Grow(10, 20)
    append := ""
    strlen: s64 = 0
    for i: 0..2 { strlen += Take(Make(append, "zz")) }
    if !free || copy != 34 || strlen != 6 { return 1 }
    return 42
}
ZI
"$ziran" check --root "$work" --module-path "$repo/std" "$work/names.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/names.zi"
for form in source saved; do
    root=$work; input=$work/names.zi
    if test "$form" = saved; then root=$work/ir; input=$work/ir/names.zir; fi
    output=$work/$form-c
    "$ziran" build --target=c --entry names:Answer --root "$root" \
        --module-path "$repo/std" -o "$output" "$input"
    cat > "$output/main.c" <<'C'
#include "names.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c -o "$output/test"
    "$output/test"
    go_output=$work/$form-go
    "$ziran" build --target=go --pkg main --entry names:Answer --root "$root" \
        --module-path "$repo/std" -o "$go_output" "$input"
    cat > "$go_output/main.go" <<'GO'
package main
func main() { if Names_Answer() != 42 { panic("runtime local names") } }
GO
    GO111MODULE=off go run "$go_output"/*.go
done
echo 'Locals named like C library and Go builtin calls pass C and Go from source and saved IR'
