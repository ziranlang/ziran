set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/resize.zi" <<'ZI'
#import "vec"

Item :: struct { payload: [16384]s32; count: s32; }

#program_export
Answer :: () -> s32 {
    items: Vec(Item)
    if !VecResize(items, 2) { return 0 }
    items[0].count = 1
    return items[0].count + items[1].payload[0]
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/resize.zi"
for input in source saved; do
    module=$work/resize.zi
    root=$work
    if test "$input" = saved; then
        module=$work/ir/resize.zir
        root=$work/ir
    fi
    output=$work/$input-c
    "$ziran" build --target=c --entry resize:Answer --root "$root" \
        --module-path "$repo/std" -o "$output" "$module"
    printf '#include "resize.h"\nint main(void) { return Answer() == 1 ? 0 : 1; }\n' \
        > "$output/main.c"
    "${CC:-cc}" -std=c11 -O2 -fno-inline \
        -Werror=frame-larger-than=8192 -I"$repo/include" -I"$output" \
        "$output"/*.c -o "$output/app"
    "$output/app"
done

echo 'large Vec resize: zero elements in place without a large stack frame'
