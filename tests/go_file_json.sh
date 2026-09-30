#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/native.zi" <<'ZI'
#import "go_types"
json :: #import "json_go";
files :: #import "file_go";
Record :: struct {
    Name: string #go_tag "json:\"name\""
    Values: []string #go_tag "json:\"values\""
    Optional: string #go_tag "json:\"optional,omitempty\""
}
Encode :: (value: Any) -> json.JSONResult {
    return json.Marshal(value)
}
Decode :: (data: []u8, value: Any) -> Error {
    return json.Unmarshal(data, value)
}
Read :: (path: string) -> files.FileReadResult {
    return files.ReadFile(path)
}
Write :: (path: string, data: []u8) -> Error {
    return files.WriteFile(path, data, cast(files.FileMode)384)
}
Mkdir :: (path: string) -> Error {
    return files.MkdirAll(path, cast(files.FileMode)448)
}
Rename :: (oldPath: string, newPath: string) -> Error {
    return files.Rename(oldPath, newPath)
}
Remove :: (path: string) -> Error {
    return files.Remove(path)
}
Dir :: (path: string) -> string {
    return files.Dir(path)
}
NotExist :: () -> Error {
    return files.NotExist()
}
#program_export
Answer :: () -> s32 {
    return 42
}
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
    "bytes"
    "encoding/json"
    "errors"
    "os"
    "path/filepath"
    "reflect"
)

func main() {
    for _, values := range [][]string{nil, {}, {"日本語", "\x00\xff", "<tag>"}} {
        record := Record{Name: "record", Values: values}
        got := Native_Encode(record)
        want, err := json.Marshal(record)
        if got.Error != nil || err != nil || !bytes.Equal(got.Value, want) {
            panic("native JSON tags, nil/empty slices or exact bytes")
        }
        var decoded, baseline Record
        actual := Native_Decode(got.Value, &decoded)
        expected := json.Unmarshal(want, &baseline)
        if actual != nil || expected != nil || !reflect.DeepEqual(decoded, baseline) {
            panic("native JSON pointer boxing or fields")
        }
    }
    null := Native_Encode(nil)
    if null.Error != nil || string(null.Value) != "null" {
        panic("native nil interface serialization")
    }
    bad := Native_Encode(make(chan int))
    var unsupported *json.UnsupportedTypeError
    if !errors.As(bad.Error, &unsupported) || bad.Value != nil {
        panic("native JSON unsupported type error")
    }
    var record Record
    var syntax *json.SyntaxError
    if !errors.As(Native_Decode([]byte("{"), &record), &syntax) {
        panic("native JSON syntax error")
    }
    var invalid *json.InvalidUnmarshalError
    if !errors.As(Native_Decode([]byte("{}"), nil), &invalid) {
        panic("native nil JSON destination error")
    }
    root, err := os.MkdirTemp("", "ziran-native-files-")
    if err != nil { panic(err) }
    defer os.RemoveAll(root)
    path := filepath.Join(root, "private", "nested", "value")
    if Native_Dir(path) != filepath.Dir(path) || Native_NotExist() != os.ErrNotExist {
        panic("native path or filesystem sentinel")
    }
    missing := Native_Read(path)
    if !errors.Is(missing.Error, os.ErrNotExist) || missing.Value != nil {
        panic("native missing-file result or error identity")
    }
    if err := Native_Mkdir(Native_Dir(path)); err != nil { panic(err) }
    payload := []byte{0, 255, 195, 169}
    if err := Native_Write(path, payload); err != nil { panic(err) }
    read := Native_Read(path)
    if read.Error != nil || !bytes.Equal(read.Value, payload) { panic("native file bytes") }
    info, err := os.Stat(path)
    if err != nil || info.Mode().Perm() != 0600 { panic("native file permissions") }
    info, err = os.Stat(Native_Dir(path))
    if err != nil || info.Mode().Perm() != 0700 { panic("native directory permissions") }
    renamed := path + ".renamed"
    if err := Native_Rename(path, renamed); err != nil { panic(err) }
    if !errors.Is(Native_Read(path).Error, os.ErrNotExist) { panic("native rename left original") }
    if err := Native_Remove(renamed); err != nil { panic(err) }
    if !errors.Is(Native_Remove(renamed), os.ErrNotExist) { panic("native remove error identity") }
}
GO
    GO111MODULE=off go run -race "$out"/*.go
    "$ziran" bundle --entry native:Answer --root "$root" --module-path std -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done
cmp "$work/source-go/native.go" "$work/saved-go/native.go"
echo 'Go JSON bytes, tags, pointer destinations, file permissions, native errors and saved IR: passed'
