#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/wide.zi" <<'EOF'

#program_export
Answer :: () -> s32 {
    high: s64 = cast(s64)2147483647 + cast(s64)100
    if high != cast(s64)2147483747 { return 0 }
    low: s64 = cast(s64)(-2147483648) - cast(s64)100
    if low != cast(s64)(-2147483748) { return 0 }
    if (cast(s64)(-1) >> cast(s64)1) != cast(s64)(-1) { return 0 }
    maximum: s64 = cast(s64)9223372036854775807
    minimum: s64 = maximum + cast(s64)1
    if minimum >= cast(s64)0 { return 0 }
    if minimum / cast(s64)(-1) != minimum { return 0 }
    if (cast(s64)1 << cast(s64)63) >= cast(s64)0 { return 0 }
    return 42
}
EOF
"$ziran" ir --root "$work" -o "$work/ir" "$work/wide.zi"
"$ziran" bundle --root "$work" --entry wide:Answer \
    -o "$work/source.zx" "$work/wide.zi"
"$ziran" bundle --root "$work/ir" --entry wide:Answer \
    -o "$work/saved.zx" "$work/ir/wide.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42
