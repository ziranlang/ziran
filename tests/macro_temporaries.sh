#!/bin/sh
set -eu

# Where a compiler never inlines (TinyCC, unoptimized GCC or Clang), checked
# division, shifts and index reads are statement-expression macros. Their
# temporaries are spelled __zi_*, which C reserves, and a program's values
# spelled that way are renamed, so no argument names a temporary.
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/names.zi" <<'EOF'
Pick :: (values: []s64, zi__slice_index: s64, __zi_index: s64) -> s64 {
    return values[zi__slice_index] * 100 + values[__zi_index]
}
Take :: (table: [4]s64, zi__index: s64, __zi_slice_index: s64) -> s64 {
    return table[zi__index] * 100 + table[__zi_slice_index]
}
Mix :: (zi__a: s32, zi__b: s32, __zi_a: s32, __zi_b: s32, zi__s: s32, __zi_s: s32) {
    print("% % % % % %\n", zi__a / zi__b, zi__a % zi__b, __zi_a / __zi_b, __zi_a % zi__b,
          zi__a << __zi_s, __zi_a >> zi__s)
}
main :: () {
    Mix(-17, 5, 40, -6, 1, 2)
    table: [4]s64
    for i: 0..3 { table[i] = 10 + i }
    print("% %\n", Take(table, 2, 1), Pick(table[:], 3, 0))
}
EOF
expected='-3 -2 -6 0 -68 20
1211 1310'

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/names.zi"
printf '#include "names.h"\nint main(void) { names_main(); return 0; }\n' > "$work/c/run.c"
for flags in -O0 -O2; do
    "${CC:-cc}" -std=gnu99 $flags -I"$work/c" "$work/c"/*.c -o "$work/c/program"
    out=$("$work/c/program" 2>&1) || out="exit $?: $out"
    test "$out" = "$expected" || { echo "macro temporaries: C $flags gave '$out'" >&2; exit 1; }
done
"$ziran" bundle --root "$work" --entry names:main -o "$work/names.zx" "$work/names.zi"
test "$(ZIRAN_NATIVE_CACHE="$work/cache" "$ziran" run "$work/names.zx")" = "$expected" ||
    { echo "macro temporaries: ziran run gave a different result" >&2; exit 1; }
echo "macro temporaries: a program's own names never meet them"
