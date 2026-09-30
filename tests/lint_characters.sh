#!/bin/sh
# `ziran check --lint` warns when a byte is compared with a printable
# character's code written as a number, and suggests the #char literal.
# #char comparisons, control characters, and non-byte values stay quiet.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
main :: () {
    text := "a\"b"
    x: u8 = text[0]
    if x == #char "H" { print("h\n") }
    if x >= 65 && x <= 90 { print("upper\n") }
    if x == 34 || x == 92 || 32 == x { print("quote\n") }
    n: s32 = 65
    if n == 65 { print("n\n") }
    if x == 10 { print("newline\n") }
}
ZI

"$ziran" check --lint --root "$work" "$work/app.zi" 2> "$work/text.err"
cat > "$work/expected" <<'OUT'
app.zi:5 compare with #char "A" rather than 65
app.zi:5 compare with #char "Z" rather than 90
app.zi:6 compare with #char "\"" rather than 34
app.zi:6 compare with #char "\\" rather than 92
app.zi:6 compare with #char " " rather than 32
OUT
sed -n 's/^\([^:]*:[0-9]*\):[0-9]*: warning: /\1 /p' "$work/text.err" > "$work/actual"
cmp "$work/expected" "$work/actual" || { cat "$work/text.err" >&2; exit 1; }
"$ziran" explain lint.char > "$work/explain.txt"
grep -Fq 'code: lint.char' "$work/explain.txt"
