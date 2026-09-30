#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/scheduled.zi" <<'ZI'
#import "sync_go"
#import "map_go"
atomic :: #import "atomic_go";
http :: #import "http_go";
go_sync :: #system_library "go:sync";
go_format :: #system_library "go:fmt";
builtin :: #system_library "go:builtin";
Store :: struct { mutex: Mutex; count: atomic.Uint64 }
Allocate :: () -> *Store #foreign builtin "new";
AllocateCounts :: () -> Map(string, u64) #foreign builtin "make";
UnlockAtReturn :: (value: *Mutex) #go_defer #foreign go_sync "(*Mutex).Unlock";
WriteAtReturn :: (writer: http.ResponseWriter, value: string) #go_defer #foreign go_format "Fprint";
Write :: (writer: http.ResponseWriter, value: string) #foreign go_format "Fprint";
New :: () -> *Store { return Allocate() }
NewCounts :: () -> Map(string, u64) {
    counts := AllocateCounts()
    MapSet(counts, "total", cast(u64)1)
    return counts
}
Record :: (store: *Store) -> u64 { return atomic.Add(*store.count, cast(u64)1) }
Count :: (store: *Store) -> u64 { return atomic.Load(*store.count) }
LockedWrite :: (store: *Store, writer: http.ResponseWriter) {
    Lock(*store.mutex)
    UnlockAtReturn(*store.mutex)
    http.SetHeader(http.ResponseHeaders(writer), "Content-Type", "text/plain")
    Write(writer, "write")
}
ScheduledWrites :: (writer: http.ResponseWriter) {
    value := "first"
    WriteAtReturn(writer, value)
    value = "second"
    WriteAtReturn(writer, value)
    Write(writer, "now:")
}
BlockSchedule :: (writer: http.ResponseWriter) {
    {
        WriteAtReturn(writer, "scheduled")
    }
    Write(writer, "body:")
}
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/scheduled.zi"
python3 - "$work/ir/scheduled.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
signature = b'UnlockAtReturn :: (value: *Mutex) #go_defer #foreign go_sync "(*Mutex).Unlock";'
assert data.count(signature) == 1
path.write_bytes(data.replace(signature, b'?' * len(signature)))
PY
for input in source saved; do
    root=$work
    file=$root/scheduled.zi
    if test "$input" = saved; then root=$work/ir; file=$root/scheduled.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main
import (
    "net/http"
    "net/http/httptest"
    "sync"
    "time"
)
type panickingWriter struct { header http.Header }
func (writer *panickingWriter) Header() http.Header { return writer.header }
func (writer *panickingWriter) WriteHeader(int) {}
func (writer *panickingWriter) Write([]byte) (int, error) { panic("writer failure") }
func main() {
    counts := Scheduled_NewCounts()
    if counts == nil || counts["total"] != 1 { panic("generic foreign result signature") }
    store := Scheduled_New()
    var workers sync.WaitGroup
    for i := 0; i < 16; i++ {
        workers.Add(1)
        go func() {
            defer workers.Done()
            for j := 0; j < 100; j++ { Scheduled_Record(store) }
        }()
    }
    workers.Wait()
    if Scheduled_Count(store) != 1600 { panic("atomic increments lost") }
    if Scheduled_Record(store) != 1601 { panic("atomic Add result") }
    recorder := httptest.NewRecorder()
    Scheduled_ScheduledWrites(recorder)
    if recorder.Body.String() != "now:secondfirst" { panic("Go argument capture or reverse cleanup order") }
    recorder = httptest.NewRecorder()
    Scheduled_BlockSchedule(recorder)
    if recorder.Body.String() != "body:scheduled" { panic("Go defer ran at block exit") }
    func() {
        defer func() {
            if recover() != "writer failure" { panic("panic identity changed") }
        }()
        Scheduled_LockedWrite(store, &panickingWriter{header: make(http.Header)})
    }()
    done := make(chan struct{})
    go func() {
        defer close(done)
        Scheduled_LockedWrite(store, httptest.NewRecorder())
    }()
    select {
    case <-done:
    case <-time.After(time.Second): panic("mutex remained locked after panic")
    }
}
GO
    GO111MODULE=off go run -race "$out"/*.go
    "$ziran" bundle --entry scheduled:Answer --root "$root" --module-path std -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done
cmp "$work/source-go/scheduled.go" "$work/saved-go/scheduled.go"
for declaration in \
    'Bad :: (value: *Mutex) -> s64 #go_defer #foreign go_sync "(*Mutex).Unlock";' \
    'Bad :: (value: *Mutex) #go_defer #go_defer #foreign go_sync "(*Mutex).Unlock";' \
    'Bad :: (value: *Mutex) #go_defer #go_results #foreign go_sync "(*Mutex).Unlock";' \
    'Bad :: (value: *Mutex) #go_defer #go_field #foreign go_sync "(*Mutex).Unlock";' \
    'Bad :: (value: *Mutex) #go_defer { }' \
    'Bad :: (value: *Mutex) #go_defer #foreign libc "unlock";'; do
    printf 'go_sync :: #system_library "go:sync";\nlibc :: #system_library "libc";\nMutex :: #type #foreign go_sync "Mutex";\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid Go defer declaration accepted: $declaration" >&2
        exit 1
    fi
done
for body in \
    'Main :: () { writer: http.ResponseWriter; unused := WriteAtReturn(writer, "value") }' \
    'Main :: () { writer: http.ResponseWriter; #unused WriteAtReturn(writer, "value") }' \
    'Slot :: #type (writer: http.ResponseWriter, value: string); Main :: () { callback: Slot = WriteAtReturn }'; do
    printf 'http :: #import "http_go";\ngo_format :: #system_library "go:fmt";\nWriteAtReturn :: (writer: http.ResponseWriter, value: string) #go_defer #foreign go_format "Fprint";\n%s\n' "$body" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "Go defer binding accepted outside a standalone call: $body" >&2
        exit 1
    fi
done
cat > "$work/owned.zi" <<'ZI'
#import "vec"
go_format :: #system_library "go:fmt";
Bad :: (value: Vec(u8)) #go_defer #foreign go_format "Print";
ZI
if "$ziran" check --root "$work" --module-path std "$work/owned.zi" > "$work/bad.out" 2>&1; then
    echo 'owned argument accepted by Go defer' >&2
    exit 1
fi
echo 'Go deferred calls, panic cleanup, atomics, HTTP headers and saved IR: passed'
