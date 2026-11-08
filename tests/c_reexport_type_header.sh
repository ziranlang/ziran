#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/c-reexport-type-header.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/rows.zi" <<'ZI'
Row :: struct { value: s32 }
ZI
cat > "$work/items.zi" <<'ZI'
Item :: struct { value: s32 }
ZI
cat > "$work/surface.zi" <<'ZI'
using Rows :: #import "rows";
using Items :: #import "items";
using Storage :: #import "std/vec";
ZI
cat > "$work/api.zi" <<'ZI'
using Surface :: #import "surface";
ZI
cat > "$work/state.zi" <<'ZI'
#scope_module
using API :: #import "api";
#scope_export
State :: struct { rows: Vec(API.Row); items: Vec(Item); }
#program_export
Answer :: () -> s32 {
    state: State
    unused VecPush(state.rows, .{20})
    unused VecPush(state.items, .{22})
    answer := state.rows[0].value + state.items[0].value
    VecFree(state.rows)
    VecFree(state.items)
    return answer
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/state.zi"
for input in source saved; do
    root=$work
    module=$work/state.zi
    if test "$input" = saved; then root=$work/ir; module=$root/state.zir; fi
    out=$work/$input-c
    "$ziran" build --target=c --root "$root" -o "$out" "$module"
    printf '#include "state.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$out/main.c"
    "${CC:-cc}" -std=c11 -Werror -I"$repo/include" -I"$out" -fsyntax-only "$out/main.c"
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" "$out"/*.c -o "$out/app"
    "$out/app"
done
echo 'reexported qualified/unqualified Vec field headers: source/saved C passed'
