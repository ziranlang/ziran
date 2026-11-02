#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-$repo/build/bin/ziran}
mkdir -p "$repo/work"
work=$(mktemp -d "$repo/work/qualified-slot-record-types.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/first" "$work/second"

cat > "$work/first/model.zi" <<'ZI'
Record :: struct { value: s32 }
ZI
cat > "$work/second/other_model.zi" <<'ZI'
Record :: struct { value: s32 }
ZI
cat > "$work/first/api.zi" <<'ZI'
#import, file "model.zi";
Transform :: #type (record: Record) -> Record;
Read :: #type (record: *Record) -> s32;
Pass :: (record: Record) -> Record { return record }
ReadValue :: (record: *Record) -> s32 { return record.value }
Apply :: (transform: Transform, read: Read) -> s32 {
    record := transform(Record.{value = 42})
    return read(*record)
}
ZI
cat > "$work/main.zi" <<'ZI'
Api :: #import, file "first/api.zi";
Other :: #import, file "second/other_model.zi";
// Same spelling and layout, but a distinct nominal record.
Record :: struct { value: s32 }
#program_export
Answer :: () -> s32 {
    local: Record = Record.{value = 1}
    other: Other.Record = Other.Record.{value = 1}
    return Api.Apply(Api.Pass, Api.ReadValue) + local.value - other.value
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/main.zi"
for input in source saved; do
    root=$work
    file=$work/main.zi
    if test "$input" = saved; then root=$work/ir; file=$root/main.zir; fi
    "$ziran" build --target=c --no-main --root "$root" -o "$work/c" "$file"
    cat > "$work/c/entry.c" <<'C'
#include "main.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -Werror=incompatible-pointer-types \
        -I"$repo/include" -I"$work/c" "$work/c"/*.c -o "$work/program"
    "$work/program"
done

sed 's/Api.Pass/Wrong/' "$work/main.zi" > "$work/wrong.zi"
cat >> "$work/wrong.zi" <<'ZI'
Wrong :: (record: Record) -> Record { return record }
ZI
if "$ziran" check --root "$work" "$work/wrong.zi" > "$work/wrong.log" 2>&1; then
    echo 'A callback using a distinct same-shaped record was accepted' >&2
    exit 1
fi
grep -Fq 'function does not match slot signature' "$work/wrong.log"

echo 'Qualified callback records retain their declaring scope in native C'
