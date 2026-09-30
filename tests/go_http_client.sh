#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/client.zi" <<'ZI'
#import "go_types"
http :: #import "http_go";
io :: #import "io_go";
clock :: #import "time_go";
ctx :: #import "context_go";
go_http :: #system_library "go:net/http";
SetAddress :: (request: *http.Request, address: string) #go_field #foreign go_http "(*Request).RemoteAddr";
SetDefault :: (client: *http.Client) #go_field #foreign go_http "DefaultClient";
New :: (timeout: s64) -> *http.Client { return http.NewClient(cast(clock.Duration)timeout) }
Request :: (context: ctx.Context, method: string, target: string, body: []u8) -> http.RequestResult {
    return http.NewRequest(context, method, target, io.FromBytes(body))
}
Limited :: (response: *http.Response, limit: s64) -> io.ReadAllResult {
    body := http.ResponseBody(response)
    result := io.ReadAll(io.LimitReader(cast(io.Reader)body, limit))
    io.Close(body)
    return result
}
Status :: (response: *http.Response) -> isize { return http.StatusCode(response) }
StatusText :: (response: *http.Response) -> string { return http.Status(response) }
Until :: (seconds: s64) -> s64 { return cast(s64)clock.Until(clock.FromUnix(seconds, 0)) }
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/client.zi"
python3 - "$work/ir/client.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
for signature in (
    b'SetAddress :: (request: *http.Request, address: string) #go_field #foreign go_http "(*Request).RemoteAddr";',
    b'SetDefault :: (client: *http.Client) #go_field #foreign go_http "DefaultClient";',
):
    assert data.count(signature) == 1
    data = data.replace(signature, b'?' * len(signature))
path.write_bytes(data)
PY
for input in source saved; do
    root=$work
    file=$root/client.zi
    if test "$input" = saved; then root=$work/ir; file=$root/client.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main
import (
    "bytes"
    "context"
    "errors"
    "io"
    "net/http"
    "reflect"
    "strings"
    "time"
)
type transportFunc func(*http.Request) (*http.Response, error)
func (transport transportFunc) RoundTrip(request *http.Request) (*http.Response, error) { return transport(request) }
type body struct { io.Reader; closed int; failure error }
func (value *body) Close() error { value.closed++; return value.failure }
type failedReader struct { failure error }
func (value failedReader) Read([]byte) (int, error) { return 0, value.failure }
func main() {
    client := Client_New(int64(10 * time.Second))
    if client == nil || client.Timeout != 10*time.Second || HttpGo_ClientTimeout(client) != client.Timeout || client.Transport != nil || client.CheckRedirect != nil { panic("native client zero fields or timeout changed") }
    previous := http.DefaultClient
    Client_SetDefault(client)
    if http.DefaultClient != client { panic("package field setter changed pointer identity") }
    http.DefaultClient = previous
    contextValue, cancel := context.WithCancel(context.Background())
    defer cancel()
    for _, data := range [][]byte{nil, {}, []byte("payload\x00\xff")} {
        request := Client_Request(contextValue, "POST", "http://example.test/a%2Fb?name=value", data)
        expected, err := http.NewRequestWithContext(contextValue, "POST", "http://example.test/a%2Fb?name=value", bytes.NewReader(data))
        if err != nil || request.Error != nil || request.Value.Context() != contextValue || request.Value.ContentLength != expected.ContentLength || (request.Value.GetBody == nil) != (expected.GetBody == nil) { panic("request context, error, body length or replayability changed") }
        if request.Value.URL.String() != expected.URL.String() || request.Value.Method != expected.Method { panic("native URL or method changed") }
        Client_SetAddress(request.Value, "[::1]:123")
        if request.Value.RemoteAddr != "[::1]:123" { panic("pointer field setter failed") }
        HttpGo_SetHeader(HttpGo_Headers(request.Value), "Content-Type", "application/json")
        if request.Value.Header.Get("Content-Type") != "application/json" { panic("request headers changed") }
        replay, err := request.Value.GetBody()
        if err != nil { panic(err) }
        got, err := io.ReadAll(replay)
        replay.Close()
        if err != nil || !bytes.Equal(got, data) { panic("native request body replay changed") }
    }
    for _, target := range []string{"http://%zz", ":bad", "http://example.test/"} {
        got := Client_Request(nil, "POST", target, nil)
        _, err := http.NewRequestWithContext(nil, "POST", target, bytes.NewReader(nil))
        if (got.Error == nil) != (err == nil) || (err != nil && got.Error.Error() != err.Error()) { panic("native request rejection changed") }
    }
    sentinel := errors.New("transport or stream failure")
    request := Client_Request(contextValue, "POST", "http://example.test/", []byte("body")).Value
    client.Transport = transportFunc(func(actual *http.Request) (*http.Response, error) {
        if actual.Method != request.Method || actual.URL.String() != request.URL.String() { panic("client changed request") }
        return nil, sentinel
    })
    if got := HttpGo_Do(client, request); !errors.Is(got.Error, sentinel) || got.Value != nil { panic("transport failure identity changed") }
    client.Transport = transportFunc(func(actual *http.Request) (*http.Response, error) {
        return &http.Response{StatusCode: 202, Status: "202 Accepted", Header: make(http.Header), Body: &body{Reader: strings.NewReader("reply"), failure: sentinel}, Request: actual}, nil
    })
    response := HttpGo_Do(client, request)
    if response.Error != nil || Client_Status(response.Value) != 202 || Client_StatusText(response.Value) != "202 Accepted" { panic("native response fields changed") }
    for _, limit := range []int64{-1, 0, 3, 100} {
        tracked := &body{Reader: strings.NewReader("reply"), failure: sentinel}
        got := Client_Limited(&http.Response{Body: tracked}, limit)
        want, err := io.ReadAll(io.LimitReader(strings.NewReader("reply"), limit))
        if err != nil || got.Error != nil || !reflect.DeepEqual(got.Value, want) || tracked.closed != 1 { panic("bounded response read or close changed") }
    }
    tracked := &body{Reader: failedReader{failure: sentinel}}
    if got := Client_Limited(&http.Response{Body: tracked}, 2048); got.Error != sentinel || tracked.closed != 1 { panic("response read failure or cleanup changed") }
    timed := Client_New(int64(5*time.Millisecond))
    timed.Transport = transportFunc(func(request *http.Request) (*http.Response, error) { <-request.Context().Done(); return nil, request.Context().Err() })
    if got := HttpGo_Do(timed, request); !errors.Is(got.Error, context.DeadlineExceeded) { panic("client deadline changed") }
    if Client_Until(time.Now().Add(time.Second).Unix()) < 0 || Client_Until(1) >= 0 { panic("native deadline arithmetic changed") }
}
GO
    GO111MODULE=off go run -race "$out"/*.go
    "$ziran" bundle --entry client:Answer --root "$root" --module-path std -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done
cmp "$work/source-go/client.go" "$work/saved-go/client.go"
for declaration in \
    'Bad :: (request: http.Request, address: string) #go_field #foreign go_http "Request.RemoteAddr";' \
    'Bad :: (request: *http.Request) #go_field #foreign go_http "(*Request).RemoteAddr";' \
    'Bad :: (request: *http.Request, value: string, extra: string) #go_field #foreign go_http "(*Request).RemoteAddr";' \
    'Bad :: (request: *http.Request, value: Vec(u8)) #go_field #foreign go_http "(*Request).RemoteAddr";' \
    'Bad :: (request: *http.Request, value: string) #go_field #go_defer #foreign go_http "(*Request).RemoteAddr";'; do
    printf '#import "vec"\nhttp :: #import "http_go";\ngo_http :: #system_library "go:net/http";\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid Go field setter accepted: $declaration" >&2
        exit 1
    fi
done
echo 'Native Go field setters, HTTP clients, response reads, deadlines and saved IR: passed'
