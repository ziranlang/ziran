#!/bin/sh
set -eu

tools=$(dirname "$1")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Both modules instantiate the same vector and pass it across a checked call.
cat > "$work/keys.zi" <<'ZI'
#import "vec"
MakeKey :: (text: string) -> Vec(u8) {
    result: Vec(u8)
    index: s64 = 0
    while index < text.count {
        if !VecPush(result, text[index]) {
            break
        }
        index += 1
    }
    return result
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "vec"
#import "keys"
#import "hmac_sha256_go"
Digest :: (key: string, message: string) -> DigestResult {
    bytes: Vec(u8) = MakeKey(key)
    return HMACSHA256(VecSlice(bytes, 0, bytes.count), message)
}
ZI
"$tools/zi2zir" --root "$work" --module-path std -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/app.zi
    else
        root=$work/ir
        file=$work/ir/app.zir
    fi
    out=$work/$input
    "$tools/zi2go" --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main
import (
    "bytes"
    "crypto/hmac"
    "crypto/sha256"
    "encoding/hex"
)
func main() {
    // RFC 4231 test case 1, followed by empty, block-boundary, long-key,
    // long-message, and arbitrary binary inputs compared to crypto/hmac.
    known := App_Digest(string(bytes.Repeat([]byte{0x0b}, 20)), "Hi There")
    if known.Error != "" || hex.EncodeToString([]byte(known.Value)) !=
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7" {
        panic("RFC 4231 digest mismatch")
    }
    for _, keySize := range []int{0, 1, 20, 63, 64, 65, 127, 128, 129, 1024} {
        key := make([]byte, keySize)
        for index := range key {
            key[index] = byte(index * 37)
        }
        for _, size := range []int{0, 1, 55, 56, 63, 64, 65, 511, 2048} {
            message := make([]byte, size)
            for index := range message {
                message[index] = byte(index * 19)
            }
            expected := hmac.New(sha256.New, key)
            expected.Write(message)
            got := App_Digest(string(key), string(message))
            if got.Error != "" || !hmac.Equal([]byte(got.Value), expected.Sum(nil)) {
                panic("HMAC digest mismatch")
            }
        }
    }
}
GO
    GO111MODULE=off go run "$out"/*.go
done
echo 'HMAC-SHA256 and shared concrete Go types: passed'
