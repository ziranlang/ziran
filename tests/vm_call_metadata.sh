#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/app.zi" <<'ZI'
Cell :: struct { value: s32; values: [4]s32; label: string }
Read :: (cell: Cell) -> s32 { return cell.value + cell.values[0] }
Write :: (cell: Cell) -> s32 {
    cell.value += 1
    cell.values[0] += 1
    return cell.value + cell.values[0]
}
Callback :: #type (cell: Cell) -> s32;
current: Callback = Read;
stored: Cell;

#program_export
main :: () -> s32 {
    stored.value = 10; stored.values[0] = 20; stored.label = "retained"
    for i: 0..99 {
        // Identical parameter spellings must keep each function's own
        // mutation analysis; mutable globals and procedure bindings stay live.
        if Read(stored) != 30 || Write(stored) != 32 { return 1 }
        if stored.value != 10 || stored.values[0] != 20 { return 2 }
        current = Write
        if current(stored) != 32 { return 3 }
        current = Read
        if current(stored) != 30 { return 4 }
        stored.value += 1
        if current(stored) != 31 { return 5 }
        stored.value -= 1
        {
            stored: Cell
            stored.value = cast(s32)i
            if Read(stored) != i { return 6 }
        }
        if stored.label != "retained" { return 7 }
    }
    return 0
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    module="$work/app.zi"; root=$work
    if test "$form" = saved; then module="$work/ir/app.zir"; root="$work/ir"; fi
    "$ziran" bundle --root "$root" --entry app:main -o "$work/$form.zib" "$module"
    test "$("$ziran" run "$work/$form.zib")" = 0
    "$ziran" build --root "$root" --target=c --exe --entry app:main \
        -o "$work/$form-c" "$module"
    "$work/$form-c/app"
done
