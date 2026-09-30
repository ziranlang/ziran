#!/bin/sh
# `ziran check --lint` warns when an integer local only ever holds 0 or 1
# (or a bool cast to an integer) and is only compared with 0 or 1: it reads
# better as a bool. Locals whose address is taken, that feed arithmetic,
# that hold other values, parameters, and bools stay quiet.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
set_one :: (p: *s32) { p.* = 1 }

pick :: (limit: s32) -> s32 {
    found: s32 = 0
    for i: 0..limit {
        if i == 3 { found = 1 }
    }
    if found == 0 { return 0 }
    wide: s32 = cast(s32)(limit > 5)
    if wide != 0 { return 2 }
    shared: s32 = 0
    set_one(*shared)
    if shared == 1 { return 3 }
    count: s32 = 0
    if limit > 1 { count = 1 }
    count = count + 1
    if count == 1 { return 4 }
    other: s32 = 0
    if limit > 2 { other = 2 }
    if other == 0 { return 5 }
    done: bool = false
    if limit > 4 { done = true }
    if done { return 6 }
    return limit
}

main :: () {
    print("%\n", pick(4))
}
ZI

"$ziran" check --lint --root "$work" "$work/app.zi" 2> "$work/text.err"
cat > "$work/expected" <<'OUT'
app.zi:4 found holds only 0 or 1 and is only compared with them; declare it bool
app.zi:9 wide holds only 0 or 1 and is only compared with them; declare it bool
OUT
sed -n 's/^\([^:]*:[0-9]*\):[0-9]*: warning: /\1 /p' "$work/text.err" > "$work/actual"
cmp "$work/expected" "$work/actual" || { cat "$work/text.err" >&2; exit 1; }
"$ziran" explain lint.bool > "$work/explain.txt"
grep -Fq 'code: lint.bool' "$work/explain.txt"
