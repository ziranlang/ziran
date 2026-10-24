#!/bin/sh
set -eu

ziran=$1
tool_dir=$(dirname "$ziran")
project_dir=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/slots.zi" <<'EOF'
Record :: struct {
    value: s32
}

Reader :: #type (record: *Record, index: s32) -> s32;
Factory :: #type () -> *Record;
Sink :: #type (record: *Record) -> ();

#program_export
Answer :: () -> s32 {
    return 42
}
EOF

"$ziran" check --root "$work" "$work/slots.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/slots.zi"
"$ziran" bundle --root "$work" --entry slots:Answer \
    -o "$work/source.zx" "$work/slots.zi"
"$ziran" bundle --root "$work/ir" --entry slots:Answer \
    -o "$work/saved.zx" "$work/ir/slots.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42

"$tool_dir/zi2c" --no-main --root "$work" -o "$work/c" "$work/slots.zi"
cc -std=c11 -fsyntax-only -I"$project_dir/include" -I"$work/c" \
    "$work/c/slots.c"

cat > "$work/native_slots.zi" <<'EOF'
Record :: struct {
    value: s32
}

Reader :: #type (record: *Record, index: s32) -> s32;
Factory :: #type () -> *Record;
Sink :: #type (record: *Record) -> ();

storage: Record;
ByteWriter :: #type (output: []u8, value: u8) -> ();

WriteByte :: (output: []u8, value: u8) { output[0] = value }

Read :: (record: *Record, index: s32) -> s32 {
    return record.value + index
}

RecordPointer :: () -> *Record {
    return *storage
}

Raise :: (record: *Record) {
    record.value += 1
}

#program_export
NativeAnswer :: () -> s32 {
    bytes: [1]u8
    writer: ByteWriter = WriteByte
    writer(bytes[:], cast(u8)40)
    if bytes[0] != cast(u8)40 { return 0 }
    storage.value = 39
    reader: Reader = Read
    factory: Factory = RecordPointer
    sink: Sink = Raise
    record := factory()
    sink(record)
    return reader(record, 2)
}
EOF

"$ziran" ir --root "$work" -o "$work/native-ir" "$work/native_slots.zi"
"$ziran" bundle --root "$work" --entry native_slots:NativeAnswer \
    -o "$work/native-source.zx" "$work/native_slots.zi"
"$ziran" bundle --root "$work/native-ir" --entry native_slots:NativeAnswer \
    -o "$work/native-saved.zx" "$work/native-ir/native_slots.zir"
cmp "$work/native-source.zx" "$work/native-saved.zx"
test "$("$ziran" run "$work/native-source.zx")" = 42
test "$("$ziran" run "$work/native-saved.zx")" = 42

"$tool_dir/zi2c" --no-main --root "$work" -o "$work/native-c" \
    "$work/native_slots.zi"
"${CC:-cc}" -std=c11 -fsyntax-only -I"$project_dir/include" \
    -I"$work/native-c" "$work/native-c/native_slots.c"
cat > "$work/native-c/main.c" <<'EOF'
#include "native_slots.h"
int main(void) { return NativeAnswer() == 42 ? 0 : 1; }
EOF
"${CC:-cc}" -std=c11 -I"$project_dir/include" -I"$work/native-c" \
    "$work/native-c/native_slots.c" "$work/native-c/main.c" \
    -o "$work/native-c/app"
"$work/native-c/app"
