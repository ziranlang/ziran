#!/bin/sh
# `ziran check --lint` warns about casts a program no longer needs: a cast
# to the value's own type, and a cast that only widens a whole typed
# initializer, assignment, argument, or result. Casts that set the width of
# arithmetic, choose an inferred type, pick an overload, or pass a variadic
# argument (whose width the callee's format decides) stay. Warnings
# do not fail the check, and JSON diagnostics mark them as warnings.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
libc :: #system_library "libc";
Format :: (output: *u8, capacity: u64, format: *u8, args: ..any) -> s32 #foreign libc "snprintf";
Take :: (value: s64) -> s64 { return value; }
Pick :: (a: s64) -> s64 { return a; }
Pick :: (a: s32) -> s64 { return cast(s64) a * 2; }
Widen :: (x: s32) -> s64 {
    a: s64 = cast(s64) x
    b := cast(s64) x
    c: s32 = cast(s32) x
    product := cast(s64) x * cast(s64) x
    d: s64 = 0
    d = cast(s64) x
    t := Take(cast(s64) x)
    o := Pick(cast(s64) x)
    buffer: [16]u8
    Format(*buffer[0], 16, null, cast(s64) x)
    byte: s8 = -1
    widened: s32 = cast(u8) byte
    return cast(s64) x
}
main :: () { print("%\n", Widen(3)); }
ZI

"$ziran" check --lint --root "$work" "$work/app.zi" 2> "$work/text.err"
cat > "$work/expected" <<'OUT'
app.zi:7 cast(s64) is not needed: s32 widens to s64 implicitly
app.zi:9 cast(s32) is not needed: the value is already s32
app.zi:12 cast(s64) is not needed: s32 widens to s64 implicitly
app.zi:13 cast(s64) is not needed: s32 widens to s64 implicitly
app.zi:19 cast(s64) is not needed: s32 widens to s64 implicitly
OUT
sed -n 's/^\([^:]*:[0-9]*\):[0-9]*: warning: /\1 /p' "$work/text.err" > "$work/actual"
cmp "$work/expected" "$work/actual" || { cat "$work/text.err" >&2; exit 1; }

"$ziran" check --lint --diagnostics=json --root "$work" "$work/app.zi" 2> "$work/json.err"
test "$(grep -c '"severity":"warning","code":"lint.cast"' "$work/json.err")" = 5

# Without --lint the check stays quiet.
"$ziran" check --root "$work" "$work/app.zi" 2> "$work/quiet.err"
test ! -s "$work/quiet.err"
