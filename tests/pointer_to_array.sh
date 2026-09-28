#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Native backends have no declarator for a pointer to a fixed array. A
# parameter of that type is rejected before code generation instead of
# being written as an array of pointers.
cat > "$work/rows.zi" <<'ZI'
RowSum :: (rows: *[4]s32, row: s32) -> s32 {
    return rows[row][0]
}
ZI
if "$ziran" check --root "$work" "$work/rows.zi" 2> "$work/rows.err"; then
    echo 'pointer-to-array parameter was accepted' >&2
    exit 1
fi
grep -Fq 'pointers to arrays are not supported; pass the element pointer: rows' "$work/rows.err"
echo 'pointer-to-array parameters: rejected with a diagnostic'
