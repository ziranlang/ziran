#!/bin/sh
# Type and name errors say what was expected and what was found, suggest a
# close visible name, and do not report follow-on errors for an operand
# whose type is already unknown.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

expect() {
    name=$1
    message=$2
    if "$ziran" check --root "$work" "$work/$name.zi" 2> "$work/$name.err"; then
        echo "$name: accepted" >&2
        exit 1
    fi
    if ! grep -Fq -- "$message" "$work/$name.err"; then
        echo "$name: missing \"$message\"" >&2
        cat "$work/$name.err" >&2
        exit 1
    fi
}

cat > "$work/types.zi" <<'ZI'
Add :: (a: s32, b: s32) -> s32 { return a + b; }
Text :: () -> s32 { return "x"; }
main :: () {
    flag: bool = 3.5;
    total: s32 = Add(1, "two");
}
ZI
expect types 'initializer type mismatch: flag (expected bool, found a float literal)'
expect types 'argument type mismatch: Add (expected s32, found string)'
expect types 'return type mismatch: Text (expected s32, found string)'

cat > "$work/counts.zi" <<'ZI'
Pair :: (a: s32, b: s32) -> s32 { return a + b; }
main :: () { print("%\n", Pair(1)); print("%\n", Pair(1, 2, 3)); }
ZI
expect counts 'argument count mismatch: Pair (a required argument is missing)'
expect counts 'argument count mismatch: Pair (takes 2 arguments)'

cat > "$work/names.zi" <<'ZI'
Compute :: (a: s32) -> s32 { return a; }
main :: () {
    count := 3;
    print("%\n", cuont);
    print("%\n", Comptue(count));
}
ZI
expect names 'unresolved name: cuont (did you mean count?)'
expect names 'unresolved function: Comptue (did you mean Compute?)'
if grep -q 'print argument must be' "$work/names.err"; then
    echo 'an unresolved print argument reported a second error' >&2
    exit 1
fi
