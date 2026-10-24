#!/bin/sh
set -eu

# Regenerating into a reused directory must leave byte-identical outputs
# untouched so build tools recompile only modules whose code changed.
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Twice :: (x: s32) -> s32 {
    return x * 2
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib"

#program_export
Answer :: () -> s32 {
    return Twice(21)
}
ZI

out=$work/c
"$ziran" build --target=c --entry app:Answer --root "$work" \
    -o "$out" "$work/app.zi"
touch -d '2001-01-01 00:00:00' "$out"/*
"$ziran" build --target=c --entry app:Answer --root "$work" \
    -o "$out" "$work/app.zi"
for file in "$out"/*; do
    test "$(date -r "$file" +%Y)" = 2001 || {
        echo "c: unchanged output was rewritten: $file" >&2
        exit 1
    }
done
if ls -A "$out" | grep -q '\.tmp$'; then
    echo "c: temporary output left behind" >&2
    exit 1
fi

sed 's/x \* 2/x + x/' "$work/lib.zi" > "$work/lib.next"
mv "$work/lib.next" "$work/lib.zi"
"$ziran" build --target=c --entry app:Answer --root "$work" \
    -o "$work/c" "$work/app.zi"
test "$(date -r "$work/c/lib.c" +%Y)" != 2001
test "$(date -r "$work/c/app.c" +%Y)" = 2001
