#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/test"
work=$(mktemp -d "$repo/build/test/private-import.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
cat > "$work/value.zi" <<'ZI'
Cell :: struct { value: s32; }
Read :: (cell: Cell) -> s32 { return cell.value }
ZI
cat > "$work/main.zi" <<'ZI'
#scope_file
Value :: #import "value";
#scope_export
#program_export
Answer :: () -> s32 {
    cell: Value.Cell
    cell.value = 42
    return Value.Read(cell)
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/main.zi"
for input in source saved; do
    source_root=$work
    source=$work/main.zi
    if test "$input" = saved; then source_root=$work/ir; source=$work/ir/main.zir; fi
    for target in c plan9-c; do
        out=$work/$input-$target
        "$ziran" build --target="$target" --root "$source_root" -o "$out" "$source"
        rg -Fq '#include "value.h"' "$out/main.c"
        if rg -Fq '#include "value.h"' "$out/main.h"; then
            echo 'implementation-only import leaked into the public header' >&2
            exit 1
        fi
        if test "$target" = c; then
            printf '#include "main.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$out/driver.c"
            "${CC:-cc}" -std=c11 -Werror -I"$out" "$out"/*.c -o "$out/run"
            "$out/run"
        fi
    done
done
echo 'private module imports: C and Plan 9 C source/saved IR passed'
