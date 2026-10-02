#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/shapes.zi" <<'ZI'
#import "go_types"
State :: struct { count: s64 }
Result :: struct { value: []u8; error: Error }
ZI
cat > "$work/slots.zi" <<'ZI'
#import "go_types"
shapes :: #import "shapes";
Factory :: #type () -> shapes.Result;
Full :: #type (state: *shapes.State, values: []shapes.Result) -> shapes.Result;
Slice :: #type (values: []shapes.Result) -> []shapes.Result;
Pointer :: #type (value: *shapes.Result) -> *shapes.Result;
Native :: #type (value: Error) -> Error;
ZI
cat > "$work/implementation.zi" <<'ZI'
#import "go_types"
model :: #import "shapes";
Factory :: () -> model.Result { return model.Result.{} }
Full :: (state: *model.State, values: []model.Result) -> model.Result {
    state.count += 1
    return values[0]
}
Slice :: (values: []model.Result) -> []model.Result { return values }
Pointer :: (value: *model.Result) -> *model.Result { return value }
Native :: (value: Error) -> Error { return value }
ZI
cat > "$work/app.zi" <<'ZI'
#import "go_types"
contract :: #import "slots";
impl :: #import "implementation";
visible :: #import "shapes";
Result :: struct { unrelated: s32 }
Factory :: () -> visible.Result {
    callback: contract.Factory = impl.Factory
    return callback()
}
Full :: (state: *visible.State, values: []visible.Result) -> visible.Result {
    callback: contract.Full = impl.Full
    return callback(state, values)
}
Slice :: (values: []visible.Result) -> []visible.Result {
    callback: contract.Slice = impl.Slice
    return callback(values)
}
Pointer :: (value: *visible.Result) -> *visible.Result {
    callback: contract.Pointer = impl.Pointer
    return callback(value)
}
Native :: (value: Error) -> Error {
    callback: contract.Native = impl.Native
    return callback(value)
}
ZI

"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/app.zi"
python3 - "$work/ir/app.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
declaration = b'callback: contract.Factory = impl.Factory'
assert data.count(declaration) == 1
path.write_bytes(data.replace(declaration, b'?' * len(declaration)))
PY
for form in source saved; do
    input=$work/app.zi
    if test "$form" = saved; then input=$work/ir/app.zir; fi
    out=$work/$form-go
    "$ziran" build --target=go --no-main --pkg main --root "$work" --module-path std -o "$out" "$input"
    # Use the compiler's native name for a deliberately colliding record.
    result_type=$(sed -n 's/^type \([^ ]*Result\) struct.*/\1/p' "$out/shapes.go")
    cat > "$out/main.go" <<'GO'
package main
import "errors"
func main() {
    sentinel := errors.New("native identity")
    bytes := []byte{0, 255, 42}
    values := []RESULT{{Value: bytes, Error: sentinel}}
    state := &State{Count: 41}
    got := App_Full(state, values)
    if state.Count != 42 || got.Error != sentinel || &got.Value[0] != &bytes[0] {
        panic("callback owner or argument identity")
    }
    if got := App_Factory(); got.Value != nil || got.Error != nil {
        panic("factory result")
    }
    if got := App_Slice(values); &got[0] != &values[0] || got[0].Error != sentinel {
        panic("slice result identity")
    }
    if App_Slice(nil) != nil { panic("nil slice") }
    if App_Pointer(&values[0]) != &values[0] || App_Pointer(nil) != nil {
        panic("pointer result identity")
    }
    if App_Native(sentinel) != sentinel || App_Native(nil) != nil {
        panic("native interface identity")
    }
}
GO
    sed -i "s/RESULT/$result_type/g" "$out/main.go"
    GO111MODULE=off GOCACHE=/tmp/ziran-bind-go-cache go run -race "$out"/*.go
done
cmp "$work/source-go/app.go" "$work/saved-go/app.go"

cat > "$work/bad.zi" <<'ZI'
contract :: #import "slots";
visible :: #import "shapes";
Result :: struct { value: s32 }
Wrong :: (state: *visible.State, values: []Result) -> visible.Result {
    return visible.Result.{}
}
Reject :: () {
    callback: contract.Full = Wrong
}
ZI
if "$ziran" ir --root "$work" --module-path std -o "$work/bad-ir" "$work/bad.zi" > "$work/bad.log" 2>&1; then
    echo 'unrelated imported slice elements accepted as callback parameters' >&2
    exit 1
fi
grep -q 'function does not match slot signature' "$work/bad.log"
echo 'Go callback declaration type owners: passed'
