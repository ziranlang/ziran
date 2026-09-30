#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/auth.zi" <<'ZI'
keys :: #import "ed25519_go";
text :: #import "text_go";
http :: #import "http_go";
url :: #import "url_go";
clock :: #import "time_go";
Key :: (seed: []u8) -> keys.PrivateKey {
    return keys.NewKeyFromSeed(seed)
}
Sign :: (key: keys.PrivateKey, message: string) -> []u8 {
    return keys.Sign(key, text.ToBytes(message))
}
Verify :: (key: keys.PublicKey, message: string, signature: []u8) -> bool {
    return keys.Verify(key, text.ToBytes(message), signature)
}
Copy :: (value: string) -> []u8 {
    return text.ToBytes(value)
}
Method :: (request: *http.Request) -> string {
    return http.Method(request)
}
URL :: (request: *http.Request) -> *url.URL {
    return http.RequestURL(request)
}
Path :: (request: *http.Request) -> string {
    return url.EscapedPath(http.RequestURL(request))
}
Unix :: (value: clock.Time) -> s64 {
    return clock.Unix(value)
}
#program_export
Answer :: () -> s32 {
    return 42
}
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/auth.zi"
for input in source saved; do
    root=$work
    file=$root/auth.zi
    if test "$input" = saved; then root=$work/ir; file=$root/auth.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main

import (
    "bytes"
    "crypto/ed25519"
    "net/http"
    "net/url"
    "runtime"
    "time"
)

func main() {
    seed := bytes.Repeat([]byte{0x42}, ed25519.SeedSize)
    key := Auth_Key(seed)
    if !bytes.Equal(key, ed25519.NewKeyFromSeed(seed)) {
        panic("native private key type or derivation")
    }
    public := key.Public().(ed25519.PublicKey)
    for _, message := range []string{"", "text", "日本語", "\x00\xff\x80\xc3\xa9"} {
        signature := Auth_Sign(key, message)
        if !bytes.Equal(signature, ed25519.Sign(key, []byte(message))) || !Auth_Verify(public, message, signature) {
            panic("native signature bytes or public key identity")
        }
        if Auth_Verify(public, message+"changed", signature) || Auth_Verify(public, message, signature[:63]) {
            panic("modified message or short signature accepted")
        }
        copied := Auth_Copy(message)
        runtime.GC()
        if copied == nil || string(copied) != message {
            panic("allocated, byte-preserving copy")
        }
        if len(copied) > 0 {
            copied[0] ^= 255
            if string(copied) == message {
                panic("copied data mutation")
            }
        }
    }
    request := &http.Request{Method: "patch", URL: &url.URL{Path: "/a/b", RawPath: "/a%2Fb"}}
    if Auth_Method(request) != "patch" || Auth_URL(request) != request.URL || Auth_Path(request) != "/a%2Fb" {
        panic("native request method, pointer or escaped path")
    }
    request.Method = "POST"
    request.URL.RawPath = "/invalid"
    if Auth_Method(request) != "POST" || Auth_Path(request) != request.URL.EscapedPath() {
        panic("live field read or invalid RawPath fallback")
    }
    request.URL = nil
    if Auth_URL(request) != nil {
        panic("nil URL identity")
    }
    for _, instant := range []time.Time{time.Time{}, time.Unix(-1, 999999999), time.Now()} {
        if Auth_Unix(instant) != instant.Unix() {
            panic("native timestamp conversion")
        }
    }
}
GO
    GO111MODULE=off go run -race "$out"/*.go
    "$ziran" bundle --entry auth:Answer --root "$root" --module-path std -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done
cmp "$work/source-go/auth.go" "$work/saved-go/auth.go"
echo 'Go Ed25519 types, exact signatures, copied bytes, escaped URLs, time and saved IR: passed'
