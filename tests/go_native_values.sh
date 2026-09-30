#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/native.zi" <<'ZI'
#import "go_types"
json :: #system_library "go:encoding/json";
builtin :: #system_library "go:builtin";
RawMessage :: #type #foreign json "RawMessage";
Panic :: (value: Error) #foreign builtin "panic";
Wrap :: (value: []u8) -> RawMessage { return cast(RawMessage)value }
Unwrap :: (value: RawMessage) -> []u8 { return cast([]u8)value }
ReturnLocal :: () -> []u8 {
    data: [2]u8 = .[4, 5]
    native := cast(RawMessage)data[:]
    return cast([]u8)native
}
Fail :: (value: Error) { Panic(value) }
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/native.zi"
for input in source saved; do
    root=$work
    file=$root/native.zi
    if test "$input" = saved; then root=$work/ir; file=$root/native.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main
import (
    "errors"
    "runtime"
)
func main() {
    if Native_Wrap(nil) != nil || Native_Unwrap(nil) != nil { panic("nil identity changed") }
    bytes := make([]byte, 2, 8)
    bytes[0], bytes[1] = 1, 2
    named := Native_Wrap(bytes)
    result := Native_Unwrap(named)
    if len(result) != 2 || cap(result) != 8 || &result[0] != &bytes[0] { panic("slice conversion copied storage") }
    result[0] = 9
    if named[0] != 9 || bytes[0] != 9 { panic("shared slice storage lost") }
    local := Native_ReturnLocal()
    runtime.GC()
    if len(local) != 2 || local[0] != 4 || local[1] != 5 { panic("native slice backing storage escaped incorrectly") }
    sentinel := errors.New("panic identity")
    func() {
        defer func() {
            if recover() != sentinel { panic("native panic lost error identity") }
        }()
        Native_Fail(sentinel)
    }()
}
GO
    GO111MODULE=off go run -race "$out"/*.go
    "$ziran" bundle --entry native:Answer --root "$root" --module-path std -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done
cmp "$work/source-go/native.go" "$work/saved-go/native.go"
for declaration in \
    'Bad :: () #foreign builtin "panic";' \
    'Bad :: (value: s32) -> s32 #foreign builtin "panic";' \
    'Bad :: (first: s32, second: s32) #foreign builtin "panic";'; do
    printf 'builtin :: #system_library "go:builtin";\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid panic signature accepted: $declaration" >&2
        exit 1
    fi
done
for body in \
    'Bad :: (value: [2]u8) -> RawMessage { return cast(RawMessage)value }' \
    'Bad :: (value: []u8) -> []s32 { return cast([]s32)value }' \
    'Bad :: (value: RawMessage) -> []Box { return cast([]Box)value }' \
    'Panic :: (value: Box) #foreign builtin "panic";'; do
    printf '#import "vec"\njson :: #system_library "go:encoding/json";\nbuiltin :: #system_library "go:builtin";\nRawMessage :: #type #foreign json "RawMessage";\nBox :: struct { values: Vec(u8) }\n%s\n' "$body" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "unsafe native value accepted: $body" >&2
        exit 1
    fi
done
echo 'Go slice conversions, backing storage, panic identity and saved IR: passed'
