#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/results.zi" <<'ZI'
#import "go_types"
#import "random_go"
net :: #system_library "go:net";
strconv :: #system_library "go:strconv";
strings :: #system_library "go:strings";
Reader :: #type #foreign strings "Reader";
HostPort :: struct { host: string; port: string; error: Error }
Integer :: struct { value: s64; error: Error }
Split :: (address: string) -> HostPort #go_results #foreign net "SplitHostPort";
Parse :: (value: string, base: isize, bits: isize) -> Integer #go_results #foreign strconv "ParseInt";
NewReader :: (value: string) -> *Reader #foreign strings;
ReadReader :: (reader: *Reader, output: []u8) -> ReadResult #go_results #foreign strings "(*Reader).Read";
ReadText :: (value: string, output: []u8) -> ReadResult {
    return ReadReader(NewReader(value), output)
}
Host :: (address: string) -> string {
    result := Split(address)
    if result.error != null { return address }
    return result.host
}
Port :: (address: string) -> string {
    result := Split(address)
    return result.port
}
Number :: (value: string) -> Integer {
    return Parse(value, cast(isize)10, cast(isize)64)
}
Random :: (output: []u8) -> bool {
    result := Fill(output)
    return result.error == null && result.count == cast(isize)output.count
}
#program_export
Answer :: () -> s32 {
    if Host("[::1]:443") == "::1" { return 42 }
    return 0
}
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/results.zi"
python3 - "$work/ir/results.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
signature = b'Split :: (address: string) -> HostPort #go_results #foreign net "SplitHostPort";'
assert data.count(signature) == 1
path.write_bytes(data.replace(signature, b'?' * len(signature)))
PY
for input in source saved; do
    root=$work; file=$root/results.zi
    if test "$input" = saved; then root=$work/ir; file=$root/results.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main
import (
    "io"
    "strconv"
)
func main() {
    if Results_Host("[::1]:443") != "::1" || Results_Port("[::1]:443") != "443" || Results_Host("bad-address") != "bad-address" { panic("three results or error handling") }
    for _, value := range []string{"42", "-9223372036854775808", "9223372036854775808", "oops", ""} {
        want, err := strconv.ParseInt(value, 10, 64)
        got := Results_Number(value)
        if got.Value != want || (got.Error == nil) != (err == nil) { panic("integer result/error") }
        if err != nil && got.Error.Error() != err.Error() { panic("error identity behavior") }
    }
    bytes := make([]byte, 32)
    if !Results_Random(bytes) || !Results_Random(nil) { panic("secure random result") }
    text := make([]byte, 2)
    read := Results_ReadText("abc", text)
    eof := Results_ReadText("", text)
    if read.Count != 2 || read.Error != nil || string(text) != "ab" || eof.Count != 0 || eof.Error != io.EOF { panic("method result/error identity") }
}
GO
    GO111MODULE=off go run "$out"/*.go
    "$ziran" build --target=go --pkg main --exe --entry results:Answer --root "$root" --module-path std -o "$work/$input-entry" "$file"
    GO111MODULE=off go build -o "$work/$input-entry-app" "$work/$input-entry"/*.go
    code=0
    "$work/$input-entry-app" || code=$?
    test "$code" = 42
done
cmp "$work/source-go/results.go" "$work/saved-go/results.go"
for declaration in \
    'Bad :: (value: string) -> s64 #go_results #foreign strconv "ParseInt";' \
    'Bad :: () -> Error #go_results #foreign strconv "ParseInt";' \
    'Empty :: struct {}; Bad :: () -> Empty #go_results #foreign strconv "ParseInt";' \
    'Bad :: () -> Integer #go_results #go_results #foreign strconv "ParseInt";' \
    'Bad :: () -> Integer #go_results #foreign libc "read";'; do
    printf '#import "go_types";\nstrconv :: #system_library "go:strconv";\nlibc :: #system_library "libc";\nInteger :: struct { value: s64; error: Error };\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid Go result adaptation accepted: $declaration" >&2
        exit 1
    fi
done
echo 'Go multiple results, error interfaces, randomness and saved IR: passed'
