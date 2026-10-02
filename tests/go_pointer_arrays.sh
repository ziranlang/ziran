#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/test"
work=$(mktemp -d "$repo/build/test/go_pointer_arrays.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/shapes.zi" <<'ZI'
Record :: struct { value: s32 }
ZI
cat > "$work/app.zi" <<'ZI'
model :: #import "shapes";
base64 :: #system_library "go:encoding/base64";
Encoding :: #type #foreign base64 "Encoding";
Record :: struct { unrelated: s64 }
Standard :: () -> *Encoding #go_field #foreign base64 "StdEncoding";
RawStandard :: () -> *Encoding #go_field #foreign base64 "RawStdEncoding";
URL :: () -> *Encoding #go_field #foreign base64 "URLEncoding";

Pointers :: (left: *s32, right: *s32) -> [2]*s32 {
    values: [2]*s32 = .[left, right]
    copied: [2]*s32 = values
    return copied
}
Indirect :: (left: **s32, right: **s32) -> [2]**s32 {
    values: [2]**s32 = .[left, right]
    return values
}
Grid :: (left: *s32, right: *s32) -> [2][2]*s32 {
    values: [2][2]*s32
    values[0][0] = left
    values[0][1] = right
    values[1][0] = right
    values[1][1] = left
    return values
}
Records :: (value: *model.Record) -> [2]*model.Record {
    values: [2]*model.Record = .[value, null]
    return values
}
Encodings :: () -> [3]*Encoding {
    values: [3]*Encoding = .[Standard(), RawStandard(), URL()]
    return values
}
Zero :: () -> [2]*s32 {
    values: [2]*s32
    return values
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
python3 - "$work/ir/app.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
declaration = b'values: [2]*s32 = .[left, right]'
assert data.count(declaration) == 1
path.write_bytes(data.replace(declaration, b'?' * len(declaration)))
PY

for form in source saved; do
    input=$work/app.zi
    if test "$form" = saved; then input=$work/ir/app.zir; fi
    out=$work/$form-go
    "$ziran" build --target=go --no-main --pkg main --root "$work" -o "$out" "$input"
    record_type=$(sed -n 's/^type \([^ ]*Record\) struct.*/\1/p' "$out/shapes.go")
    cat > "$out/main.go" <<'GO'
package main

import "encoding/base64"

func main() {
    left, right := int32(41), int32(42)
    pointers := App_Pointers(&left, &right)
    if pointers != [2]*int32{&left, &right} {
        panic("pointer array identity")
    }
    *pointers[0] = 43
    if left != 43 { panic("pointer array storage") }
    first, second := &left, &right
    if App_Indirect(&first, &second) != [2]**int32{&first, &second} {
        panic("indirect pointer array")
    }
    if App_Grid(&left, &right) != [2][2]*int32{{&left, &right}, {&right, &left}} {
        panic("nested pointer array")
    }
    record := &RECORD{Value: 42}
    records := App_Records(record)
    if records[0] != record || records[1] != nil {
        panic("imported record pointer identity")
    }
    records[0].Value = 43
    if record.Value != 43 { panic("imported pointer storage") }
    if App_Encodings() != [3]*base64.Encoding{
        base64.StdEncoding, base64.RawStdEncoding, base64.URLEncoding,
    } {
        panic("native Go pointer array")
    }
    if App_Zero() != [2]*int32{} { panic("zero pointer array") }
}
GO
    sed -i "s/RECORD/$record_type/g" "$out/main.go"
    gofmt -w "$out"/*.go
    GO111MODULE=off GOCACHE=/tmp/ziran-bind-go-cache go run -race "$out"/*.go
done
cmp "$work/source-go/app.go" "$work/saved-go/app.go"

# The shared array declaration path must retain C and C++ pointer syntax.
cat > "$work/portable.zi" <<'ZI'
#program_export
Answer :: () -> s32 {
    left: s32 = 41
    right: s32 = 1
    values: [2]*s32 = .[*left, *right]
    return values[0][0] + values[1][0]
}
ZI
for target in c cpp; do
    "$ziran" build --target="$target" --no-main \
        --root "$work" -o "$work/$target" "$work/portable.zi"
    case "$target" in
        c) suffix=c; header=h; compiler=${CC:-cc}; standard=c11;;
        cpp) suffix=cpp; header=hpp; compiler=${CXX:-c++}; standard=c++11;;
    esac
    cat > "$work/$target/driver.$suffix" <<EOF
#include "portable.$header"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
    "$compiler" -std="$standard" -Wall -Werror -I"$work/$target" \
        "$work/$target/portable.$suffix" "$work/$target/driver.$suffix" \
        -o "$work/$target/run"
    "$work/$target/run"
done
echo 'Go fixed arrays of pointers: source, saved IR, native identity and C/C++ passed'
