#!/bin/sh
# Scalar and other built-in type names cannot be redeclared as records or
# enums; a module that tried would give `s32` two meanings.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

for name in s32 u8 bool string float64 usize int float Type any; do
    printf '%s :: struct { x: u8; }\nmain :: () { }\n' "$name" > "$work/shadow.zi"
    if "$ziran" check --root "$work" "$work/shadow.zi" 2> "$work/shadow.err"; then
        echo "type named $name was accepted" >&2
        exit 1
    fi
    grep -Fq 'built-in type cannot be redeclared' "$work/shadow.err"
done

printf 'Mode :: enum { S32; U8; }\nmain :: () { m := Mode.S32; }\n' > "$work/members.zi"
"$ziran" check --root "$work" "$work/members.zi"
