#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/slots.zi" <<'ZI'
#import "go_types"
State :: struct { count: s64 }
Result :: struct { value: []u8; error: Error }
Bound :: #type (bytes: []u8) -> Result;
Full :: #type (state: *State, prefix: string, bytes: []u8) -> Result;
ZI
cat > "$work/binding.zi" <<'ZI'
#import "go_types"
slots :: #import "slots";
builtin :: #system_library "go:builtin";
Binder :: (callback: slots.Full, state: *slots.State, prefix: string) -> slots.Bound #foreign builtin "bind";
Adder :: #type (count: *s64, amount: s64) -> s64;
Reader :: #type () -> s64;
BindAll :: (callback: Adder, count: *s64, amount: s64) -> Reader #foreign builtin "bind";
Writer :: #type (count: *s64, bytes: []u8) -> void;
Output :: #type (bytes: []u8) -> void;
BindWriter :: (callback: Writer, count: *s64) -> Output #foreign builtin "bind";
TextState :: struct { text: string }
RetainText :: (value: string) -> string #foreign builtin "retain";
RetainBytes :: (value: []u8) -> []u8 #foreign builtin "retain";
RetainResult :: (value: slots.Result) -> slots.Result #foreign builtin "retain";
ReadText :: (state: *TextState) -> string { return RetainText(state.text) }
Capture :: (callback: slots.Full, state: *slots.State, prefix: string) -> slots.Bound {
    bound := Binder(callback, state, prefix)
    prefix = "changed"
    return bound
}
Add :: (count: *s64, amount: s64) -> s64 {
    <<count += amount
    return <<count
}
#program_export
Answer :: () -> s32 { return 42 }
BindEntry :: () -> s32 {
    count: s64
    result := BindAll(Add, *count, cast(s64)42)
    return cast(s32)result()
}
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/binding.zi"
python3 - "$work/ir/binding.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
signature = b'Binder :: (callback: slots.Full, state: *slots.State, prefix: string) -> slots.Bound #foreign builtin "bind";'
assert data.count(signature) == 1
path.write_bytes(data.replace(signature, b'?' * len(signature)))
PY
for form in source saved; do
    input=$work/binding.zi
    if test "$form" = saved; then input=$work/ir/binding.zir; fi
    out=$work/$form-go
    "$ziran" build --target=go --no-main --pkg main --root "$work" --module-path std -o "$out" "$input"
    cat > "$out/main.go" <<'GO'
package main
import (
    "errors"
    "reflect"
    "strings"
    "unsafe"
)
func main() {
    sentinel := errors.New("native error")
    state := &State{}
    source := []byte{0, 255, 42}
    callback := func(captured *State, prefix string, bytes []byte) Result {
        if captured != state || prefix != "first" { panic("capture changed") }
        captured.Count++
        return Result{Value: bytes, Error: sentinel}
    }
    bound := Binding_Capture(callback, state, "first")
    state.Count = 41
    result := bound(source)
    if state.Count != 42 || result.Error != sentinel || &result.Value[0] != &source[0] {
        panic("pointer, slice or error identity")
    }
    result = bound(nil)
    if result.Value != nil || state.Count != 43 { panic("nil slice") }
    if !reflect.DeepEqual(result, Result{Error: sentinel}) { panic("result record") }
    var count int64
    all := Binding_BindAll(Binding_Add, &count, 42)
    if all() != 42 || all() != 84 { panic("all parameters bound") }
    writer := Binding_BindWriter(func(count *int64, bytes []byte) { *count += int64(len(bytes)) }, &count)
    writer(source)
    if count != 87 { panic("void callback") }
    marker := &struct{ code int }{42}
    panicking := Binding_Binder(func(*State, string, []byte) Result { panic(marker) }, state, "first")
    func() {
        defer func() { if recover() != marker { panic("panic identity") } }()
        panicking(source)
    }()
    nilCallback := Binding_Binder(nil, state, "first")
    if nilCallback == nil { panic("binding must not invoke or substitute a nil callback") }
    func() {
        defer func() { if recover() == nil { panic("nil callback must panic at invocation") } }()
        nilCallback(source)
    }()
    if Binding_BindEntry() != 42 { panic("canonical procedure callback") }
    text := strings.Repeat("heap string", 128)
    retained := Binding_ReadText(&TextState{Text: text})
    if retained != text || unsafe.StringData(retained) != unsafe.StringData(text) { panic("native string storage changed") }
    if bytes := Binding_RetainBytes(source); &bytes[0] != &source[0] { panic("native slice storage changed") }
    if Binding_RetainBytes(nil) != nil { panic("native nil slice changed") }
    result = Binding_RetainResult(Result{Value: source, Error: sentinel})
    if &result.Value[0] != &source[0] || result.Error != sentinel { panic("native record storage changed") }
}
GO
    GO111MODULE=off GOCACHE=/tmp/ziran-bind-go-cache go run -race "$out"/*.go
    "$ziran" build --target=go --exe --pkg main --root "$work" --module-path std --entry binding:BindEntry -o "$work/$form-entry" "$input"
    GO111MODULE=off GOCACHE=/tmp/ziran-bind-go-cache go build -o "$work/$form-entry-bin" "$work/$form-entry"/*.go
    result=0
    "$work/$form-entry-bin" || result=$?
    test "$result" = 42
    "$ziran" bundle --root "$work" --module-path std --entry binding:Answer -o "$work/$form.zib" "$input"
    test "$("$ziran" run "$work/$form.zib")" = 42
    "$ziran" build --target=c --exe --root "$work" --module-path std --entry binding:Answer -o "$work/$form-c" "$input"
    result=0
    "$work/$form-c/binding" || result=$?
    test "$result" = 42
    "$ziran" build --target=cpp --root "$work" --module-path std --entry binding:Answer -o "$work/$form-cpp" "$input"
    printf '#include "binding.hpp"\nint main() { return Answer() == 42 ? 0 : 1; }\n' > "$work/$form-cpp/main.cpp"
    ${CXX:-c++} -std=c++17 -Iinclude -I"$work/$form-cpp" "$work/$form-cpp"/*.cpp -o "$work/$form-cpp/app"
    "$work/$form-cpp/app"
    for target in c cpp; do
        if "$ziran" build --target="$target" --root "$work" --module-path std --entry binding:BindEntry -o "$work/rejected" "$input" > "$work/rejected.log" 2>&1; then
            echo "reachable Go bind accepted by $target" >&2
            exit 1
        fi
    done
    if "$ziran" bundle --root "$work" --module-path std --entry binding:BindEntry -o "$work/rejected.zib" "$input" > "$work/rejected.log" 2>&1; then
        echo "reachable Go bind accepted by portable bundle" >&2
        exit 1
    fi
done
cmp "$work/source-go/binding.go" "$work/saved-go/binding.go"
cmp "$work/source-go/slots.go" "$work/saved-go/slots.go"

for mode in wrong_capture wrong_tail wrong_result no_capture no_callback non_procedure owned_capture owned_tail owned_result c_callback retain_type retain_count retain_owned; do
    cat > "$work/bad.zi" <<'ZI'
builtin :: #system_library "go:builtin";
#import "vec"
Callback :: #type (capture: s32, value: s64) -> s64;
Result :: #type (value: s64) -> s64;
ZI
    case "$mode" in
    wrong_capture) echo 'Bind :: (callback: Callback, capture: s64) -> Result #foreign builtin "bind";' ;;
    wrong_tail) echo 'Other :: #type (value: s32) -> s64; Bind :: (callback: Callback, capture: s32) -> Other #foreign builtin "bind";' ;;
    wrong_result) echo 'Other :: #type (value: s64) -> s32; Bind :: (callback: Callback, capture: s32) -> Other #foreign builtin "bind";' ;;
    no_capture) echo 'Bind :: (callback: Callback) -> Callback #foreign builtin "bind";' ;;
    no_callback) echo 'Bind :: (capture: s32) -> Result #foreign builtin "bind";' ;;
    non_procedure) echo 'Bind :: (callback: Callback, capture: s32) -> s64 #foreign builtin "bind";' ;;
    owned_capture) echo 'Other :: #type (capture: Vec(s32), value: s64) -> s64; Bind :: (callback: Other, capture: Vec(s32)) -> Result #foreign builtin "bind";' ;;
    owned_tail) echo 'Other :: #type (capture: s32, value: Vec(s32)) -> s64; Tail :: #type (value: Vec(s32)) -> s64; Bind :: (callback: Other, capture: s32) -> Tail #foreign builtin "bind";' ;;
    owned_result) echo 'Owned :: struct { value: Vec(s32) }; Other :: #type (capture: s32, value: s64) -> Owned; Tail :: #type (value: s64) -> Owned; Bind :: (callback: Other, capture: s32) -> Tail #foreign builtin "bind";' ;;
    c_callback) echo 'Other :: #type (capture: s32, value: s64) -> s64 #c_call; Bind :: (callback: Other, capture: s32) -> Result #foreign builtin "bind";' ;;
    retain_type) echo 'Retain :: (value: []u8) -> []s32 #foreign builtin "retain";' ;;
    retain_count) echo 'Retain :: (first: string, second: string) -> string #foreign builtin "retain";' ;;
    retain_owned) echo 'Retain :: (value: Vec(s32)) -> Vec(s32) #foreign builtin "retain";' ;;
    esac >> "$work/bad.zi"
    sed -i 's/; Bind/;\nBind/;s/; Tail/;\nTail/;s/; Other/;\nOther/' "$work/bad.zi"
    if "$ziran" ir --root "$work" --module-path std -o "$work/bad-ir" "$work/bad.zi" > "$work/bad.log" 2>&1; then
        echo "invalid bind accepted: $mode" >&2
        exit 1
    fi
    if ! grep -q 'Go builtin requires a valid' "$work/bad.log"; then
        cat "$work/bad.log" >&2
        echo "invalid bind failed outside signature checking: $mode" >&2
        exit 1
    fi
done
echo 'Go callback binding: passed'
