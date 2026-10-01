#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/records.zi" <<'ZI'
Entry :: struct { value: s32 }
Pair :: struct($T: Type) { value: T }
Mutate :: (entries: []Entry) -> []Entry {
    if entries.count > 0 { entries[0].value += 1 }
    return entries
}
ArrayValue :: (entries: [2]Entry) -> s32 {
    entries[0].value += entries[1].value
    return entries[0].value
}
PairValue :: (entries: []Pair(s32)) -> s32 {
    entries[0].value += 1
    return entries[0].value
}
ZI
cat > "$work/relay.zi" <<'ZI'
Rows :: #import "records";
Pass :: (entries: []Rows.Entry) -> []Rows.Entry { return Rows.Mutate(entries) }
ZI
cat > "$work/main.zi" <<'ZI'
Data :: #import "records";
Relay :: #import "relay";
#program_export
Answer :: () -> s32 {
    entries: [2]Data.Entry
    entries[0].value = 40
    entries[1].value = 2
    if Data.ArrayValue(entries) != 42 || entries[0].value != 40 { return 1 }
    borrowed := Relay.Pass(entries[:])
    if entries[0].value != 41 || borrowed[0].value != 41 { return 2 }
    borrowed[0].value += 1
    if entries[0].value != 42 { return 3 }
    empty := Relay.Pass(entries[0:0])
    if empty.count != 0 { return 4 }
    pairs: [1]Data.Pair(s32)
    pairs[0].value = 41
    if Data.PairValue(pairs[:]) != 42 || pairs[0].value != 42 { return 5 }
    return 42
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/main.zi"
for input in "$work/main.zi" "$work/ir/main.zir"; do
    "$ziran" bundle --root "$work" --entry main:Answer -o "$work/check.zib" "$input"
    test "$("$ziran" run "$work/check.zib")" = 42
done

cat > "$work/other.zi" <<'ZI'
Entry :: struct { value: s32 }
ZI
cat > "$work/bad.zi" <<'ZI'
Rows :: #import "records";
Other :: #import "other";
Answer :: () -> s32 {
    entries: [2]Other.Entry
    Rows.Mutate(entries[:])
    return 0
}
ZI
if "$ziran" check --root "$work" "$work/bad.zi" > "$work/bad.out" 2>&1; then
    echo 'distinct same-named record slices were accepted' >&2
    exit 1
fi
