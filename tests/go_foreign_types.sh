#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/json_types.zi" <<'ZI'
#if !#defined(Raw) {
    Missing :: UnknownValue;
}
zero: Raw;
json :: #system_library "go:encoding/json";
Raw :: #type #foreign json "RawMessage";
ZI
cat > "$work/other.zi" <<'ZI'
json_types :: #import "json_types";
json :: #system_library "go:encoding/json";
Raw :: #type #foreign json "RawMessage";
Forward :: (value: json_types.Raw) -> Raw {
    return value
}
ZI
cat > "$work/opaque.zi" <<'ZI'
json_types :: #import "json_types";
other :: #import "other";
time :: #system_library "go:time";
context :: #system_library "go:context";
ed25519 :: #system_library "go:crypto/ed25519";
Time :: #type #foreign time;
Context :: #type #foreign context;
PrivateKey :: #type #foreign ed25519;
FromSeed :: (seed: []u8) -> PrivateKey #foreign ed25519 "NewKeyFromSeed";
Envelope :: struct {
    payload: json_types.Raw #go_tag "json:\"payload\""
    optional: json_types.Raw #go_tag "json:\"optional,omitempty\""
}
SetPayload :: (record: *Envelope, value: json_types.Raw) {
    record.payload = other.Forward(value)
}
GetPayload :: (record: Envelope) -> json_types.Raw {
    return record.payload
}
ZeroRaw :: () -> json_types.Raw {
    value: json_types.Raw
    return value
}
ZeroTime :: () -> Time {
    value: Time
    return value
}
ZeroContext :: () -> Context {
    value: Context
    return value
}
SeedKey :: (seed: []u8) -> PrivateKey {
    return FromSeed(seed)
}
#program_export
Answer :: () -> s32 {
    return 42
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/opaque.zi"
for input in source saved; do
    root=$work
    file=$work/opaque.zi
    if test "$input" = saved; then
        root=$work/ir
        file=$work/ir/opaque.zir
    fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main

import (
    "bytes"
    "crypto/ed25519"
    "encoding/json"
    "reflect"
)

func main() {
    raw := json.RawMessage(`{"x":[1,true,null],"text":"雪"}`)
    record := Envelope{}
    Opaque_SetPayload(&record, raw)
    if !bytes.Equal(Opaque_GetPayload(record), raw) {
        panic("opaque argument/return changed")
    }
    wire, err := json.Marshal(record)
    if err != nil || string(wire) != `{"payload":{"x":[1,true,null],"text":"雪"}}` {
        panic(string(wire))
    }
    var decoded Envelope
    if err := json.Unmarshal(wire, &decoded); err != nil || !bytes.Equal(decoded.Payload, raw) {
        panic("RawMessage JSON behavior changed")
    }
    field, _ := reflect.TypeOf(record).FieldByName("Payload")
    if field.Type != reflect.TypeOf(raw) || Opaque_ZeroRaw() != nil || Opaque_ZeroContext() != nil || !Opaque_ZeroTime().IsZero() {
        panic("foreign identity or zero value changed")
    }
    seed := make([]byte, ed25519.SeedSize)
    for i := range seed { seed[i] = byte(i) }
    if !bytes.Equal(Opaque_SeedKey(seed), ed25519.NewKeyFromSeed(seed)) {
        panic("direct Go foreign type result changed")
    }
}
GO
    GO111MODULE=off go run "$out"/*.go
    for target in c cpp rust py; do
        if "$ziran" build "--target=$target" --root "$root" -o "$work/rejected-$target" "$file" > "$work/rejected.out" 2>&1; then
            echo "foreign Go type accepted by $target" >&2
            exit 1
        fi
        grep -q 'foreign Go types require the Go target' "$work/rejected.out" || { cat "$work/rejected.out"; exit 1; }
    done
    if "$ziran" bundle --root "$root" --entry opaque:GetPayload -o "$work/rejected.zib" "$file" > "$work/rejected.out" 2>&1; then
        echo 'foreign Go type entered a portable ABI' >&2
        exit 1
    fi
    # An unused Go type need not prevent a portable entry or native build.
    "$ziran" bundle --root "$root" --entry opaque:Answer -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
    "$ziran" build --target=c --entry opaque:Answer --root "$root" -o "$work/$input-c" "$file"
done
cmp "$work/source-go/opaque.go" "$work/saved-go/opaque.go"

for library in 'host_api' 'libc' 'encoding/json'; do
    printf 'json :: #system_library "%s";\nRaw :: #type #foreign json "RawMessage";\n' "$library" > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "non-Go foreign type accepted: $library" >&2
        exit 1
    fi
done
for declaration in 'Raw :: #type #foreign absent;' \
    'Raw :: #type #foreign json "Raw.Message";' \
    'Raw :: #type #foreign json { value: u8 };' \
    'Raw :: #type (u8) #foreign json "RawMessage";' \
    'Raw :: #type #foreign json "RawMessage" extra;' \
    'Raw :: #type #foreign json "RawMessage"; Raw :: #type #foreign json "RawMessage";'; do
    printf 'json :: #system_library "go:encoding/json";\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "malformed foreign type accepted: $declaration" >&2
        exit 1
    fi
done
for body in 'return Raw.{}' 'return size_of(Raw)'; do
    printf 'json :: #system_library "go:encoding/json";\nRaw :: #type #foreign json "RawMessage";\nBad :: () -> Raw { %s; }\n' "$body" > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "opaque layout operation accepted: $body" >&2
        exit 1
    fi
done
python3 - "$work/ir/json_types.zir" "$work/tampered.zir" <<'PY'
from pathlib import Path
import sys
original = Path(sys.argv[1]).read_bytes()
old = b'go:encoding/json.RawMessage'
assert old in original
Path(sys.argv[2]).write_bytes(original.replace(old, b'go:encoding/json.RawMessa!e'))
PY
if "$ziran" build --target=go --no-main --root "$work" -o "$work/tampered-go" "$work/tampered.zir" > "$work/tampered.out" 2>&1; then
    echo 'invalid saved foreign type target accepted' >&2
    exit 1
fi
grep -Eq 'invalid|malformed' "$work/tampered.out" || { cat "$work/tampered.out"; exit 1; }
echo 'opaque Go types, JSON behavior, native rejection and saved IR: passed'
