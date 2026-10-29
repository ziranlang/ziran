#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/methods.zi" <<'ZI'
#import "go_types"
io :: #import "io_go";
go_io :: #system_library "go:io";
go_fmt :: #system_library "go:fmt";
builtin :: #system_library "go:builtin";
CapturedWriter :: struct { destination: io.Writer; calls: isize }
WriteResult :: struct { count: isize; error: Error }
Label :: struct { value: string }
Counter :: struct { value: s32 }
WriteRaw :: (writer: io.Writer, bytes: []u8) -> WriteResult #go_results #foreign go_io "Writer.Write";
Allocate :: () -> *CapturedWriter #foreign builtin "new";
PrintNative :: (value: Any) #foreign go_fmt "Print";
New :: (destination: io.Writer) -> io.Writer {
    writer := Allocate()
    writer.destination = destination
    return cast(io.Writer)writer
}
Write :: (writer: *CapturedWriter, bytes: []u8) -> WriteResult #go_method "Write" #go_results {
    writer.calls += 1
    return WriteRaw(writer.destination, bytes)
}
Describe :: (label: Label) -> string #go_method "String" {
    return label.value
}
Add :: (counter: *Counter, amount: s32) -> s32 #go_method "Add" {
    counter.value += amount
    return counter.value
}
Reset :: (counter: *Counter) #go_method "Reset" {
    counter.value = 0
}
#program_export
Answer :: () -> s32 {
    counter: Counter
    Add(*counter, cast(s32)42)
    return counter.value
}
InterfaceEntry :: () {
    PrintNative(cast(Any)Label.{"42"})
}
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/methods.zi"

# Statement text is diagnostic only; bodies and adapters use checked metadata.
python3 - "$work/ir/methods.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
signature = b'return WriteRaw(writer.destination, bytes)'
assert data.count(signature) == 1
path.write_bytes(data.replace(signature, b'?' * len(signature)))
PY
for form in source saved; do
    input=$work/methods.zi
    if test "$form" = saved; then input=$work/ir/methods.zir; fi
    out=$work/$form-go
    "$ziran" build --target=go --no-main --pkg main --root "$work" --module-path std -o "$out" "$input"
    cat > "$out/main.go" <<'GO'
package main
import (
    "bytes"
    "errors"
    "fmt"
    "io"
)
var _ io.Writer = (*CapturedWriter)(nil)
var _ fmt.Stringer = Label{}
type failingWriter struct { err error; marker any }
func (writer failingWriter) Write([]byte) (int, error) {
    if writer.marker != nil { panic(writer.marker) }
    return 7, writer.err
}
func main() {
    var buffer bytes.Buffer
    writer := Methods_New(&buffer)
    count, err := writer.Write([]byte{0, 255, 42})
    concrete := writer.(*CapturedWriter)
    if count != 3 || err != nil || !bytes.Equal(buffer.Bytes(), []byte{0, 255, 42}) || concrete.Calls != 1 {
        panic("native interface dispatch or pointer receiver identity")
    }
    sentinel := errors.New("sentinel")
    concrete.Destination = failingWriter{err: sentinel}
    count, err = writer.Write(nil)
    if count != 7 || err != sentinel || concrete.Calls != 2 { panic("native multiple results or error identity") }
    marker := &struct{ code int }{42}
    concrete.Destination = failingWriter{marker: marker}
    func() {
        defer func() { if recover() != marker { panic("native panic identity") } }()
        writer.Write(nil)
    }()
    if concrete.Calls != 3 { panic("mutation before panic") }
    if fmt.Sprint(Label{Value: "bytes\x00\xff"}) != "bytes\x00\xff" { panic("value receiver") }
    var counter Counter
    if counter.Add(42) != 42 || counter.Value != 42 { panic("scalar result") }
    counter.Reset()
    if counter.Value != 0 || Methods_Answer() != 42 { panic("void method or ordinary calls") }
}
GO
    GO111MODULE=off GOCACHE=/tmp/ziran-method-go-cache go run -race "$out"/*.go
    "$ziran" build --target=go --exe --pkg main --root "$work" --module-path std --entry methods:InterfaceEntry -o "$work/$form-entry" "$input"
    test "$(GO111MODULE=off GOCACHE=/tmp/ziran-method-go-cache go run -race "$work/$form-entry"/*.go)" = 42
    "$ziran" bundle --root "$work" --module-path std --entry methods:Answer -o "$work/$form.zx" "$input"
    test "$("$ziran" run "$work/$form.zx")" = 42
    "$ziran" build --target=c --exe --root "$work" --module-path std --entry methods:Answer -o "$work/$form-c" "$input"
    result=0
    "$work/$form-c/methods" || result=$?
    test "$result" = 42
done
cmp "$work/source-go/methods.go" "$work/saved-go/methods.go"

# Invalid checked method metadata must fail independently of source text.
python3 - "$work/ir/methods.zir" "$work" <<'PY'
from pathlib import Path
import sys
data = Path(sys.argv[1]).read_bytes()
# Numbers are LEB128 and signed fields zigzag, so 1 is 2 and 2 is 4. A new
# string is twice its length, then its bytes; a later use is an odd index.
def read_number(at):
    value = shift = 0
    while True:
        byte = data[at]
        at += 1
        value |= (byte & 0x7f) << shift
        shift += 7
        if byte < 0x80:
            return value, at
def skip_string(at):
    tag, at = read_number(at)
    return at + (0 if tag & 1 else tag // 2)
args = b'writer:*CapturedWriter, bytes:[]u8'
at = data.index(bytes([2 * 5]) + b'Write' + bytes([2 * len(args)]) + args) + 2 + 5 + len(args)
at = skip_string(at)
_, at = read_number(at)
at = skip_string(at)
assert data[at:at + 2] == bytes(2)
at = skip_string(at + 2)
assert data[at] == 2
Path(sys.argv[2], 'bad-results.zir').write_bytes(data[:at] + bytes([4]) + data[at + 1:])
# Describe's method name is the only use of "String"; a keyword of the same
# length keeps every later string reference in place.
method = bytes([2 * 6]) + b'String'
assert data.count(method) == 1
Path(sys.argv[2], 'bad-name.zir').write_bytes(data.replace(method, bytes([2 * 6]) + b'return'))
PY
for invalid in "$work/bad-results.zir" "$work/bad-name.zir"; do
    if "$ziran" check --root "$work/ir" --module-path std "$invalid" > "$work/bad.out" 2>&1; then
        echo "invalid checked method accepted: $invalid" >&2
        exit 1
    fi
    case "$invalid" in
        *bad-results.zir) rg -q 'invalid checked IR structure' "$work/bad.out" ;;
        *bad-name.zir) rg -q 'non-keyword Go identifier' "$work/bad.out" ;;
    esac
done

for declaration in \
    'Bad :: () #go_method "Reset" { }' \
    'Bad :: (value: s32) #go_method "Reset" { }' \
    'Bad :: (value: **Counter) #go_method "Reset" { }' \
    'Bad :: (value: Counter) #go_method "Value" { }' \
    'Bad :: (value: Counter) #go_method "return" { }' \
    'Bad :: (value: Counter) #go_method "_" { }' \
    'Bad :: (value: Counter) #go_method "bad.name" { }' \
    'Bad :: (value: Counter) #go_method Reset { }' \
    'Bad :: (value: Counter) #go_method "Reset" extra { }' \
    'Bad :: (value: Counter) #go_method "Reset" #go_method "Again" { }' \
    'Bad :: (value: Counter) #go_method "Reset" #go_results { }' \
    'Bad :: (value: Counter) -> s32 #go_method "Reset" #go_results { return 0 }' \
    'Bad :: (value: Counter) -> Empty #go_method "Reset" #go_results { return .{} }' \
    'Bad :: (value: Counter) -> Result #go_method "Reset" #go_results #go_results { return .{} }' \
    'Bad :: (value: Counter) -> Result #go_results extra #go_method "Reset" { return .{} }' \
    'Bad :: (value: $T) #go_method "Reset" { }' \
    'Bad :: (value: Counter) #go_method "Reset" #foreign builtin "call";'; do
    printf 'builtin :: #system_library "go:builtin";\nCounter :: struct { value: s32 }\nEmpty :: struct { }\nResult :: struct { value: s32 }\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid method accepted: $declaration" >&2
        exit 1
    fi
done
cat > "$work/bad.zi" <<'ZI'
Counter :: struct { value: s32 }
Reset :: (value: Counter) #go_method "Reset" { }
Again :: (value: *Counter) #go_method "Reset" { }
ZI
if "$ziran" check --root "$work" "$work/bad.zi" > "$work/bad.out" 2>&1; then
    echo 'duplicate native method accepted' >&2
    exit 1
fi
for declaration in \
    'Counter :: struct {
#go_anonymous
value: s32
}; Reset :: (value: Counter) #go_method "Reset" { }' \
    '#import "io_go"; Reset :: (value: Writer) #go_method "Reset" { }' \
    '#import "vec"; Counter :: struct { values: Vec(u8) }; Reset :: (value: *Counter) #go_method "Reset" { }' \
    'Counter :: struct { value: s32 }; Result :: struct { _: s32 }; Reset :: (value: Counter) -> Result #go_method "Reset" #go_results { return .{} }' \
    '#go_method "Reset"'; do
    printf '%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid receiver or modifier accepted: $declaration" >&2
        exit 1
    fi
done
echo 'Go method exports, native interfaces, ordinary portable calls and saved IR: passed'
