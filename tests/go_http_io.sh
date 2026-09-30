#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/callback.zi" <<'ZI'
#import "go_types"
ctx :: #import "context_go";
Forward :: #type (context: ctx.Context, error: Error) -> Error;
ZI
cat > "$work/streams.zi" <<'ZI'
#import "go_types"
http :: #import "http_go";
io :: #import "io_go";
json :: #import "json_go";
url :: #import "url_go";
ctx :: #import "context_go";
callback :: #import "callback";
go_io :: #system_library "go:io";
builtin :: #system_library "go:builtin";
CleanupState :: struct { called: isize; completed: bool }
Cleanup :: #type (state: *CleanupState, marker: isize) -> void;
CallAtReturn :: (callback: Cleanup, state: *CleanupState, marker: isize) #go_defer #foreign builtin "call";
Forward :: (callback: callback.Forward, context: ctx.Context, error: Error) -> Error #foreign builtin "call";
CloseAtReturn :: (reader: io.ReadCloser) #go_defer #foreign go_io "ReadCloser.Close";
Read :: (writer: http.ResponseWriter, request: *http.Request, limit: s64) -> io.ReadAllResult {
    body := http.Body(request)
    CloseAtReturn(body)
    return io.ReadAll(cast(io.Reader)http.MaxBytesReader(writer, body, limit))
}
Write :: (writer: http.ResponseWriter, status: isize, value: Any) -> Error {
    http.SetHeader(http.ResponseHeaders(writer), "Content-Type", "application/json")
    http.WriteHeader(writer, status)
    return json.Encode(json.NewEncoder(cast(io.Writer)writer), value)
}
Valid :: (body: []u8) -> bool { return json.Valid(body) }
ReadJSON :: (reader: io.Reader, value: Any) -> Error {
    return json.Decode(json.NewDecoder(reader), value)
}
NextJSON :: (decoder: *json.Decoder, value: Any) -> Error {
    return json.Decode(decoder, value)
}
Decoder :: (reader: io.Reader) -> *json.Decoder {
    return json.NewDecoder(reader)
}
Context :: (request: *http.Request) -> ctx.Context { return http.Context(request) }
Query :: (request: *http.Request, name: string) -> string {
    return url.Value(url.Query(http.RequestURL(request)), name)
}
CleanupNow :: (state: *CleanupState, marker: isize) {
    if !state.completed {
        state.called = state.called * cast(isize)10 + marker
    }
}
Scheduled :: (state: *CleanupState, writer: http.ResponseWriter, completed: bool) {
    marker: isize = 2
    CallAtReturn(CleanupNow, state, marker)
    marker = 3
    CallAtReturn(CleanupNow, state, marker)
    marker = 9
    Write(writer, cast(isize)200, "body")
    state.completed = completed
}
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/streams.zi"
python3 - "$work/ir/streams.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
signature = b'CallAtReturn :: (callback: Cleanup, state: *CleanupState, marker: isize) #go_defer #foreign builtin "call";'
assert data.count(signature) == 1
path.write_bytes(data.replace(signature, b'?' * len(signature)))
PY
for input in source saved; do
    root=$work
    file=$root/streams.zi
    if test "$input" = saved; then root=$work/ir; file=$root/streams.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main
import (
    "bytes"
    "context"
    "encoding/json"
    "errors"
    "io"
    "net/http"
    "net/http/httptest"
    "reflect"
    "strings"
)
type trackedBody struct { io.Reader; closed int }
func (body *trackedBody) Close() error { body.closed++; return errors.New("close ignored") }
type failingReader struct { failure error }
func (reader failingReader) Read([]byte) (int, error) { return 0, reader.failure }
type failingWriter struct { header http.Header; failure error; panicValue any }
func (writer *failingWriter) Header() http.Header { return writer.header }
func (writer *failingWriter) WriteHeader(int) {}
func (writer *failingWriter) Write([]byte) (int, error) {
    if writer.panicValue != nil { panic(writer.panicValue) }
    return 0, writer.failure
}
func main() {
    for _, limit := range []int64{-1, 0, 3, 4, 8} {
        body := &trackedBody{Reader: strings.NewReader("abcd")}
        request := httptest.NewRequest("POST", "/", body)
        got := Streams_Read(httptest.NewRecorder(), request, limit)
        want, err := io.ReadAll(http.MaxBytesReader(httptest.NewRecorder(), io.NopCloser(strings.NewReader("abcd")), limit))
        if !reflect.DeepEqual(got.Value, want) || (got.Error == nil) != (err == nil) || (err != nil && got.Error.Error() != err.Error()) { panic("bounded read changed") }
        if body.closed != 1 { panic("request body not closed exactly once") }
    }
    sentinel := errors.New("stream failure")
    body := &trackedBody{Reader: failingReader{failure: sentinel}}
    if got := Streams_Read(httptest.NewRecorder(), httptest.NewRequest("POST", "/", body), 100); got.Error != sentinel || body.closed != 1 { panic("read error identity changed") }
    type payload struct { Name string `json:"name"`; Values []int `json:"values"` }
    value := payload{Name: "<x>&\u2028", Values: []int{}}
    recorder := httptest.NewRecorder()
    if Streams_Write(recorder, 201, value) != nil { panic("JSON write failed") }
    var expected bytes.Buffer
    if json.NewEncoder(&expected).Encode(value) != nil { panic("baseline encode") }
    if recorder.Code != 201 || recorder.Header().Get("Content-Type") != "application/json" || recorder.Body.String() != expected.String() { panic("JSON response bytes or status changed") }
    writer := &failingWriter{header: make(http.Header), failure: sentinel}
    if Streams_Write(writer, 200, value) != sentinel { panic("write error identity changed") }
    for _, data := range []string{"null", "{}", "", "{}[]", "\xff"} {
        if Streams_Valid([]byte(data)) != json.Valid([]byte(data)) { panic("JSON validity changed") }
    }
    for _, data := range []string{"null", "{}", "", "{}[]", "{", "[]", "{\"name\":\"first\"}{\"name\":\"second\"}"} {
        var got, want payload
        failure := Streams_ReadJSON(strings.NewReader(data), &got)
        expectedFailure := json.NewDecoder(strings.NewReader(data)).Decode(&want)
        if !reflect.DeepEqual(got, want) || (failure == nil) != (expectedFailure == nil) || (expectedFailure != nil && failure.Error() != expectedFailure.Error()) { panic("JSON stream decoding or errors changed") }
        if errors.Is(failure, io.EOF) != errors.Is(expectedFailure, io.EOF) { panic("JSON EOF identity changed") }
    }
    var decoded payload
    if Streams_ReadJSON(failingReader{failure: sentinel}, &decoded) != sentinel { panic("JSON read error identity changed") }
    stream := Streams_Decoder(strings.NewReader("{\"name\":\"first\"} {\"name\":\"second\"}"))
    for _, name := range []string{"first", "second"} {
        if Streams_NextJSON(stream, &decoded) != nil || decoded.Name != name { panic("JSON decoder streaming state changed") }
    }
    if Streams_NextJSON(stream, &decoded) != io.EOF { panic("JSON decoder EOF identity changed") }
    ctx, cancel := context.WithCancel(context.Background())
    cancel()
    request := httptest.NewRequest("GET", "/?name=first&name=second&plus=a+b&encoded=%2B", nil).WithContext(ctx)
    if Streams_Context(request) != ctx { panic("native context identity changed") }
    for _, name := range []string{"name", "plus", "encoded", "missing"} {
        if Streams_Query(request, name) != request.URL.Query().Get(name) { panic("query selection changed") }
    }
    if Streams_Forward(func(actual context.Context, failure error) error {
        if actual != ctx { panic("callback context changed") }
        return failure
    }, ctx, sentinel) != sentinel { panic("callback error identity changed") }
    state := &CleanupState{}
    Streams_Scheduled(state, httptest.NewRecorder(), false)
    if state.Called != 32 { panic("callback arguments or reverse order changed") }
    state = &CleanupState{}
    Streams_Scheduled(state, httptest.NewRecorder(), true)
    if state.Called != 0 { panic("completed cleanup ran") }
    state = &CleanupState{}
    func() {
        defer func() { if recover() != sentinel { panic("callback panic identity changed") } }()
        Streams_Scheduled(state, &failingWriter{header: make(http.Header), panicValue: sentinel}, true)
    }()
    if state.Called != 32 { panic("callback cleanup lost during panic") }
}
GO
    GO111MODULE=off go run -race "$out"/*.go
    "$ziran" bundle --entry streams:Answer --root "$root" --module-path std -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done
cmp "$work/source-go/streams.go" "$work/saved-go/streams.go"
for declaration in \
    'Bad :: () #foreign builtin "call";' \
    'Bad :: (callback: s32) #foreign builtin "call";' \
    'Bad :: (callback: Cleanup) #foreign builtin "call";' \
    'Bad :: (callback: Cleanup, state: *CleanupState, marker: s32) #foreign builtin "call";' \
    'Bad :: (callback: Cleanup, state: *CleanupState, marker: isize) -> s32 #foreign builtin "call";' \
    'Bad :: (callback: ResultCallback) -> s32 #go_defer #foreign builtin "call";' \
    'Bad :: (callback: OwnedCallback, values: Vec(u8)) #foreign builtin "call";' \
    'Bad :: (callback: OwnedResult) -> Vec(u8) #foreign builtin "call";'; do
    printf '#import "vec"\nbuiltin :: #system_library "go:builtin";\nCleanupState :: struct { called: isize }\nCleanup :: #type (state: *CleanupState, marker: isize) -> void;\nResultCallback :: #type () -> s32;\nOwnedCallback :: #type (values: Vec(u8)) -> void;\nOwnedResult :: #type () -> Vec(u8);\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid native callback signature accepted: $declaration" >&2
        exit 1
    fi
done
echo 'Go HTTP bodies, JSON responses, stream errors, deferred callbacks and saved IR: passed'
