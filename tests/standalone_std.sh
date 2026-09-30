#!/bin/sh
# Without a project, std/NAME and plain NAME imports find the standard
# modules that came with the compiler; no --module-path is needed.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset ZIRAN_STD

cat > "$work/app.zi" <<'ZI'
#import "std/vec";
#import "std/option";
#import "text";

#program_export
Answer :: () -> s32 {
    values: Vec(s32);
    VecPush(values, 20);
    VecPush(values, 22);
    total: s32 = 0;
    i: s64 = 0;
    while i < values.count { total += VecGet(values, i).value; i += 1; }
    if !StartsWithFoldASCII("Ziran", "zi") { return 0; }
    return total;
}
ZI
"$ziran" check --root "$work" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:Answer -o "$work/app.zib" "$work/app.zi"
test "$("$ziran" run "$work/app.zib")" = 42

# ZIRAN_STD names another standard directory; one without the standard
# modules makes the import fail instead of silently using another.
mkdir "$work/empty"
if ZIRAN_STD="$work/empty" "$ziran" check --root "$work" "$work/app.zi" 2> "$work/empty.err"; then
    echo 'an unusable ZIRAN_STD was ignored' >&2
    exit 1
fi
grep -q 'cannot find imported module' "$work/empty.err"

# Passing the Vec's address gets a hint instead of a cascade.
cat > "$work/address.zi" <<'ZI'
#import "std/vec";
main :: () {
    values: Vec(s32);
    VecPush(*values, 1);
}
ZI
if "$ziran" check --root "$work" "$work/address.zi" 2> "$work/address.err"; then
    echo 'VecPush of an address was accepted' >&2
    exit 1
fi
grep -q 'pass the Vec variable itself' "$work/address.err"
if grep -q 'element type mismatch' "$work/address.err"; then
    echo 'VecPush reported a follow-on element error' >&2
    exit 1
fi
