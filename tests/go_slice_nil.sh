#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/native.zi" <<'ZI'
#import "go_types"
text :: #import "text_go";
builtin :: #system_library "go:builtin";
Record :: struct { name: string; count: s64 }
BytesNil :: (value: []u8) -> bool #foreign builtin "is_nil";
IntegersNil :: (value: []isize) -> bool #foreign builtin "is_nil";
RecordsNil :: (value: []Record) -> bool #foreign builtin "is_nil";
Bytes :: (value: []u8) -> bool { return BytesNil(value) }
StandardBytes :: (value: []u8) -> bool { return text.IsNil(value) }
Integers :: (value: []isize) -> bool { return IntegersNil(value) }
Records :: (value: []Record) -> bool { return RecordsNil(value) }
LocalEmpty :: () -> bool {
    value: [1]u8
    return BytesNil(value[0:0])
}
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/native.zi"
python3 - "$work/ir/native.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
signature = b'BytesNil :: (value: []u8) -> bool #foreign builtin "is_nil";'
assert data.count(signature) == 1
path.write_bytes(data.replace(signature, b'?' * len(signature)))
PY
for input in source saved; do
    root=$work
    file=$root/native.zi
    if test "$input" = saved; then root=$work/ir; file=$root/native.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main
func main() {
    for _, value := range [][]byte{nil, {}, make([]byte, 0, 8), {1, 2, 3}} {
        if Native_Bytes(value) != (value == nil) || Native_StandardBytes(value) != (value == nil) {
            panic("byte slice nilness changed")
        }
        if len(value) > 0 && (Native_Bytes(value[:0]) || Native_Bytes(value[len(value):])) {
            panic("empty subslice was confused with nil")
        }
    }
    for _, value := range [][]int{nil, {}, make([]int, 0, 4), {1, 2}} {
        if Native_Integers(value) != (value == nil) { panic("integer slice nilness changed") }
    }
    for _, value := range [][]Record{nil, {}, make([]Record, 0, 4), {{Name: "record", Count: 42}}} {
        if Native_Records(value) != (value == nil) { panic("record slice nilness changed") }
    }
    if Native_LocalEmpty() { panic("local empty view was confused with nil") }
}
GO
    GO111MODULE=off go run -race "$out"/*.go
    "$ziran" bundle --entry native:Answer --root "$root" --module-path std -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done
cmp "$work/source-go/native.go" "$work/saved-go/native.go"
for declaration in \
    'Bad :: () -> bool #foreign builtin "is_nil";' \
    'Bad :: (value: []u8, second: []u8) -> bool #foreign builtin "is_nil";' \
    'Bad :: (value: []u8) -> isize #foreign builtin "is_nil";' \
    'Bad :: (value: []u8) #foreign builtin "is_nil";' \
    'Bad :: (value: [2]u8) -> bool #foreign builtin "is_nil";' \
    'Bad :: (value: string) -> bool #foreign builtin "is_nil";' \
    'Bad :: (value: Any) -> bool #foreign builtin "is_nil";' \
    'Bad :: (value: *u8) -> bool #foreign builtin "is_nil";' \
    'Bad :: (value: Vec(u8)) -> bool #foreign builtin "is_nil";' \
    'Bad :: (value: []Box) -> bool #foreign builtin "is_nil";' \
    'Bad :: (value: []u8) -> Result #go_results #foreign builtin "is_nil";' \
    'Bad :: (value: []u8) -> bool #go_defer #foreign builtin "is_nil";'; do
    printf '#import "go_types"\n#import "vec"\nbuiltin :: #system_library "go:builtin";\nBox :: struct { values: Vec(u8) }\nResult :: struct { value: bool }\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid Go nil check accepted: $declaration" >&2
        exit 1
    fi
done
echo 'Go slice nilness, allocated empty storage, typed signatures and saved IR: passed'
