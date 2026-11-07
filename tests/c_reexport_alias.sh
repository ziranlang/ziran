#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/c-reexport-alias.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

# A qualified call follows two reexports even when its name also names an
# import alias in the caller and a type in another reexported module.
cat > "$work/text_widget.zi" <<'ZI'
TextProps :: struct { value: s32 }
Text :: (frame: s32, props: TextProps) -> s32 { return frame + props.value }
ZI
cat > "$work/core.zi" <<'ZI'
using Storage :: #import "std/vec";
using TextWidget :: #import "text_widget";
ZI
cat > "$work/widgets.zi" <<'ZI'
using Core :: #import "core";
ZI
cat > "$work/text_buffer.zi" <<'ZI'
TextUntilNul :: (value: s32) -> s32 { return value }
ZI
cat > "$work/direct.zi" <<'ZI'
using Storage :: #import "std/vec";
Text :: (value: s32) -> s32 { return value }
ZI
cat > "$work/app.zi" <<'ZI'
using UI :: #import "widgets";
Text :: #import "text_buffer";
Own :: #import "direct";
#program_export
Answer :: () -> s32 {
    props: TextProps
    props.value = 22
    return UI.Text(20, props) + Own.Text(0)
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    module=$work/app.zi
    root=$work
    if test "$input" = saved; then
        module=$work/ir/app.zir
        root=$work/ir
    fi
    output=$work/$input-c
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
        "$output"/*.c -o "$output/app"
    "$output/app"
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run --vm "$work/$input.zx")" = 42
done
cmp "$work/source.zx" "$work/saved.zx"
echo 'qualified direct and reexported calls with colliding aliases: source/saved C and VM passed'
