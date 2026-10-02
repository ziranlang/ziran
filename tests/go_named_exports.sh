#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/exports.zi" <<'ZI'
#import "go_types"
builtin :: #system_library "go:builtin";
State :: struct { count: s64 }
Result :: struct { bytes: []u8; error: Error }
Panic :: (value: Error) #foreign builtin "panic";
Value: s32 = 40;
#program_export "Value"
GetValue :: () -> s32 { return Value + 2 }
#program_export "NativeForward"
Forward :: (Exports_Forward: *State, bytes: []u8, error: Error) -> Result {
    Exports_Forward.count += 1
    return Result.{bytes, error}
}
#program_export "NativeVoid"
Raise :: (state: *State) { state.count += 1 }
#program_export "NativePanic"
RaisePanic :: (error: Error) { Panic(error) }
#program_export "NativeEntry"
Answer :: () -> s32 { return GetValue() }
ZI
cat > "$work/caller.zi" <<'ZI'
exports :: #import "exports";
Reader :: #type () -> s32;
Read :: () -> s32 {
    callback: Reader = exports.GetValue
    return callback() + exports.Answer()
}
ZI
cat > "$work/testing_helper_test.zi" <<'ZI'
native :: #system_library "go:testing";
Test :: #type #foreign native "T";
Fuzzer :: #type #foreign native "F";
Benchmark :: #type #foreign native "B";
StringCase :: #type (test: *Test, value: string) -> void;
FatalRaw :: (test: *Test, message: string) #foreign native "(*T).Fatal";
AddRaw :: (test: *Fuzzer, value: string) #foreign native "(*F).Add";
FuzzRaw :: (test: *Fuzzer, callback: StringCase) #foreign native "(*F).Fuzz";
CountRaw :: (test: *Benchmark) -> isize #go_field #foreign native "(*B).N";
BenchFatalRaw :: (test: *Benchmark, message: string) #foreign native "(*B).Fatal";
Fatal :: (test: *Test, message: string) { FatalRaw(test, message) }
Add :: (test: *Fuzzer, value: string) { AddRaw(test, value) }
Fuzz :: (test: *Fuzzer, callback: StringCase) { FuzzRaw(test, callback) }
Count :: (test: *Benchmark) -> isize { return CountRaw(test) }
BenchFatal :: (test: *Benchmark, message: string) { BenchFatalRaw(test, message) }
ZI
cat > "$work/checks_test.zi" <<'ZI'
support :: #import "testing_helper_test";
exports :: #import "exports";
#program_export "TestNamedExportPass"
Pass :: (test: *support.Test) {
    if exports.Answer() != 42 { support.Fatal(test, "wrong native answer") }
}
#program_export "TestNamedExportFails"
Fail :: (test: *support.Test) { support.Fatal(test, "native assertion preserved") }
FuzzCase :: (test: *support.Test, value: string) {
    if exports.Answer() != 42 { support.Fatal(test, "fuzz callback") }
}
#program_export "FuzzNamedExport"
Fuzz :: (test: *support.Fuzzer) {
    support.Add(test, "seed")
    support.Fuzz(test, FuzzCase)
}
#program_export "BenchmarkNamedExport"
Benchmark :: (test: *support.Benchmark) {
    for index: 0..support.Count(test) {
        if exports.Answer() != 42 { support.BenchFatal(test, "benchmark callback") }
    }
}
ZI
mkdir -p "$work/left" "$work/right"
for side in Left Right; do
    filename=collision_test
    if test "$side" = Right; then filename=Collision_test; fi
    directory=$(printf '%s' "$side" | tr '[:upper:]' '[:lower:]')
    cat > "$work/$directory/$filename.zi" <<ZI
support :: #import "testing_helper_test";
#program_export "TestCollision$side"
Pass :: (test: *support.Test) {}
ZI
done

"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/caller.zi" "$work/checks_test.zi" "$work/left/collision_test.zi" "$work/right/Collision_test.zi"
python3 - "$work/ir/exports.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
statement = b'return GetValue()'
assert data.count(statement) == 1
path.write_bytes(data.replace(statement, b'?' * len(statement)))
PY
for form in source saved; do
    inputs="$work/caller.zi $work/checks_test.zi $work/left/collision_test.zi $work/right/Collision_test.zi"
    entry=$work/exports.zi
    if test "$form" = saved; then
        inputs="$work/ir/caller.zir $work/ir/checks_test.zir $work/ir/left/collision_test.zir $work/ir/right/Collision_test.zir"
        entry=$work/ir/exports.zir
    fi
    out=$work/$form-go
    "$ziran" build --target=go --no-main --pkg ziran --root "$work" --module-path std -o "$out" $inputs
    cat > "$out/native_test.go" <<'GO'
package ziran
import (
    "errors"
    "testing"
)
func TestTypedNamedExports(t *testing.T) {
    sentinel := errors.New("native error")
    bytes := []byte{0, 255, 42}
    state := &State{Count: 41}
    result := NativeForward(state, bytes, sentinel)
    if state.Count != 42 || &result.Bytes[0] != &bytes[0] || result.Error != sentinel {
        t.Fatal("native pointer, slice or error identity")
    }
    if result := NativeForward(state, nil, nil); result.Bytes != nil || result.Error != nil {
        t.Fatal("native nil identity")
    }
    NativeVoid(state)
    if state.Count != 44 || Value() != 42 || NativeEntry() != 42 || Caller_Read() != 84 {
        t.Fatal("void, global collision or canonical callback")
    }
    defer func() {
        if recover() != sentinel { t.Fatal("native panic identity") }
    }()
    NativePanic(sentinel)
}
GO
    (cd "$out" && GO111MODULE=off GOCACHE=/tmp/ziran-bind-go-cache go build .)
    (cd "$out" && GO111MODULE=off GOCACHE=/tmp/ziran-bind-go-cache go test -list . .) > "$work/discovered.txt"
    grep -q '^TestCollisionLeft$' "$work/discovered.txt"
    grep -q '^TestCollisionRight$' "$work/discovered.txt"
    (cd "$out" && GO111MODULE=off GOCACHE=/tmp/ziran-bind-go-cache go test -race -run 'Test(NamedExportPass|TypedNamedExports|CollisionLeft|CollisionRight)$|^FuzzNamedExport$' .)
    (cd "$out" && GO111MODULE=off CGO_ENABLED=0 GOCACHE=/tmp/ziran-bind-go-cache go test -run 'Test(NamedExportPass|TypedNamedExports|CollisionLeft|CollisionRight)$|^FuzzNamedExport$' .)
    (cd "$out" && GO111MODULE=off GOCACHE=/tmp/ziran-bind-go-cache go test -run '^$' -bench '^BenchmarkNamedExport$' -benchtime=1x .)
    if (cd "$out" && GO111MODULE=off GOCACHE=/tmp/ziran-bind-go-cache go test -run '^TestNamedExportFails$' .) > "$work/failure.log" 2>&1; then
        cat "$work/failure.log" >&2
        ls "$out" >&2
        (cd "$out" && GO111MODULE=off go test -list . .) >&2
        echo 'compiled Ziran test assertion did not fail' >&2
        exit 1
    fi
    grep -q 'native assertion preserved' "$work/failure.log"
    "$ziran" build --target=go --exe --pkg main --root "$work" --module-path std --entry exports:Answer -o "$work/$form-entry" "$entry"
    GO111MODULE=off GOCACHE=/tmp/ziran-bind-go-cache go build -o "$work/$form-bin" "$work/$form-entry"/*.go
    result=0
    "$work/$form-bin" || result=$?
    test "$result" = 42
done
for file in exports.go caller.go checks_test.go testing_helper_test.go; do
    cmp "$work/source-go/$file" "$work/saved-go/$file"
done

for symbol in '_' init main string len panic unsafe for integerOp; do
    printf '#program_export "%s"\nAnswer :: () -> s32 { return 42 }\n' "$symbol" > "$work/bad.zi"
    if "$ziran" build --target=go --root "$work" -o "$work/rejected" "$work/bad.zi" > "$work/rejected.log" 2>&1; then
        echo "reserved native Go export accepted: $symbol" >&2
        exit 1
    fi
    grep -q 'reserved native Go export name' "$work/rejected.log"
done
for mode in alias procedure type import startup; do
    case "$mode" in
    alias)
        cat > "$work/bad.zi" <<'ZI'
#program_export "Duplicate"
First :: () -> s32 { return 1 }
#program_export "Duplicate"
Second :: () -> s32 { return 2 }
ZI
        ;;
    procedure)
        cat > "$work/bad.zi" <<'ZI'
First :: () -> s32 { return 1 }
#program_export "Bad_First"
Second :: () -> s32 { return 2 }
ZI
        ;;
    type)
        cat > "$work/bad.zi" <<'ZI'
Record :: struct { value: s32 }
#program_export "Record"
Answer :: () -> s32 { return 42 }
ZI
        ;;
    import)
        cat > "$work/bad.zi" <<'ZI'
native :: #system_library "go:strings";
Upper :: (value: string) -> string #foreign native "ToUpper";
#program_export "strings"
Answer :: () -> string { return Upper("answer") }
ZI
        ;;
    startup)
        cat > "$work/bad.zi" <<'ZI'
count: s32;
Read :: () -> s32 { count += 1; return count }
value: s32 = Read();
#program_export "Bad_ziranInit"
Answer :: () -> s32 { return value }
ZI
        ;;
    esac
    if "$ziran" build --target=go --root "$work" -o "$work/rejected" "$work/bad.zi" > "$work/rejected.log" 2>&1; then
        echo "colliding native Go export accepted: $mode" >&2
        exit 1
    fi
    grep -Eq 'duplicate native Go export name|native Go export collides' "$work/rejected.log"
done
echo 'Typed named Go exports and native Go test discovery: passed'
