#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/fields.zi" <<'ZI'
http :: #import "http_go";
network :: #import "net_go";
clock :: #import "time_go";
go_http :: #system_library "go:net/http";
go_url :: #system_library "go:net/url";
builtin :: #system_library "go:builtin";
URL :: #type #foreign go_url "URL";
URLRaw :: (request: *http.Request) -> *URL #go_field #foreign go_http "(*Request).URL";
PathRaw :: (value: *URL) -> string #go_field #foreign go_url "(*URL).Path";
Allocate :: () -> *http.Request #foreign builtin "new";
TextCountRaw :: (value: string) -> isize #foreign builtin "len";
ByteCountRaw :: (value: []u8) -> isize #foreign builtin "len";
TextCount :: (value: string) -> isize { return TextCountRaw(value) }
ByteCount :: (value: []u8) -> isize { return ByteCountRaw(value) }
Remote :: (request: *http.Request) -> string { return http.RemoteAddress(request) }
Header :: (request: *http.Request, name: string) -> string { return http.HeaderValue(http.Headers(request), name) }
Path :: (request: *http.Request) -> string { return PathRaw(URLRaw(request)) }
NewRequest :: () -> *http.Request { return Allocate() }
CanonicalIP :: (value: string) -> string {
    parsed := network.ParseIP(value)
    if !network.ValidIP(parsed) { return "" }
    return network.FormatIP(parsed)
}
Loopback :: (value: string) -> bool { return network.IsLoopback(network.ParseIP(value)) }
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/fields.zi"
python3 - "$work/ir/fields.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
signature = b'PathRaw :: (value: *URL) -> string #go_field #foreign go_url "(*URL).Path";'
assert data.count(signature) == 1
path.write_bytes(data.replace(signature, b'?' * len(signature)))
PY
for input in source saved; do
    root=$work
    file=$root/fields.zi
    if test "$input" = saved; then root=$work/ir; file=$root/fields.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main
import (
    "net"
    "net/http"
    "net/url"
)
func main() {
    request := &http.Request{RemoteAddr: "[::1]:443", Header: http.Header{"X-Test": {"first", "second"}}, URL: &url.URL{Path: "/health"}}
    if Fields_Remote(request) != request.RemoteAddr || Fields_Header(request, "x-test") != request.Header.Get("x-test") || Fields_Path(request) != "/health" { panic("native request field/header access") }
    if Fields_NewRequest() == nil || Fields_Remote(Fields_NewRequest()) != "" || Fields_Header(Fields_NewRequest(), "missing") != "" { panic("foreign allocation or zero-value headers") }
    request.RemoteAddr = "new:123"
    request.URL.Path = "/updated"
    if Fields_Remote(request) != request.RemoteAddr || Fields_Path(request) != request.URL.Path { panic("field access took a stale copy") }
    for _, value := range []string{"127.0.0.1", "127.9.8.7", "::1", "2001:DB8::1", "::ffff:192.0.2.1", "192.0.2.1", "127.0.0.01", "::1%lo", "", "invalid"} {
        parsed := net.ParseIP(value)
        want := ""
        if parsed != nil { want = parsed.String() }
        if Fields_CanonicalIP(value) != want || Fields_Loopback(value) != parsed.IsLoopback() { panic("IP parsing, nil, canonicalization or loopback changed") }
    }
    if Fields_TextCount("é\x00") != 3 || Fields_ByteCount(nil) != 0 || Fields_ByteCount([]byte{1, 2}) != 2 { panic("native len signature") }
}
GO
    GO111MODULE=off go run "$out"/*.go
    for target in c cpp zib; do
        if test "$target" = zib; then
            "$ziran" bundle --entry fields:Answer --root "$root" --module-path std -o "$work/$input-$target.zib" "$file"
        else
            "$ziran" build --target="$target" --entry fields:Answer --root "$root" --module-path std -o "$work/$input-$target" "$file"
        fi
    done
    if "$ziran" build --target=c --no-main --root "$root" --module-path std -o "$work/rejected" "$file" > "$work/rejected.out" 2>&1; then
        echo 'reachable foreign Go fields were accepted in C' >&2
        exit 1
    fi
done
cmp "$work/source-go/fields.go" "$work/saved-go/fields.go"
for declaration in \
    'Bad :: () -> string #go_field #foreign go_http "(*Request).RemoteAddr";' \
    'Bad :: (request: Request) -> string #go_field #foreign go_http "(*Request).RemoteAddr";' \
    'Bad :: (request: *Request, extra: string) -> string #go_field #foreign go_http "(*Request).RemoteAddr";' \
    'Bad :: (request: *Request) #go_field #foreign go_http "(*Request).RemoteAddr";' \
    'Bad :: (request: *Request) -> string #go_field #foreign go_http "Read";' \
    'Bad :: (request: *Request) -> string #go_field #go_results #foreign go_http "(*Request).RemoteAddr";' \
    'Bad :: (request: *Request) -> string #go_field #go_field #foreign go_http "(*Request).RemoteAddr";' \
    'Bad :: (request: *Request) -> string #go_field { return "" }' \
    'Bad :: (value: s64) -> isize #foreign builtin "len";' \
    'Bad :: (value: string) -> s64 #foreign builtin "len";'; do
    printf 'go_http :: #system_library "go:net/http";\nbuiltin :: #system_library "go:builtin";\nRequest :: #type #foreign go_http "Request";\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid Go field or len binding accepted: $declaration" >&2
        exit 1
    fi
done
echo 'Go foreign fields, HTTP headers, IP primitives, len and saved IR: passed'
