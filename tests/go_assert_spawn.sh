#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/native.zi" <<'ZI'
#import "go_types"
ctx :: #import "context_go";
channels :: #import "channel_go";
selection :: #import "select_go";
clock :: #import "time_go";
sync :: #system_library "go:sync";
io :: #system_library "go:io";
reflect :: #system_library "go:reflect";
builtin :: #system_library "go:builtin";
Reader :: #type #foreign io "Reader";
Group :: #type #foreign sync "WaitGroup";
Event :: struct { name: string; count: s64 }
ReaderResult :: struct { value: Reader; present: bool }
EventResult :: struct { value: Event; present: bool }
SliceResult :: struct { value: []u8; present: bool }
PointerResult :: struct { value: *s32; present: bool }
AsReader :: (value: Any) -> ReaderResult #go_results #foreign builtin "assert";
AsEvent :: (value: Any) -> EventResult #go_results #foreign builtin "assert";
AsSlice :: (value: Any) -> SliceResult #go_results #foreign builtin "assert";
AsPointer :: (value: Any) -> PointerResult #go_results #foreign builtin "assert";
Worker :: #type (group: *Group, channel: channels.Channel, name: string) -> void;
Spawn :: (callback: Worker, group: *Group, channel: channels.Channel, name: string) #foreign builtin "spawn";
Complete :: (group: *Group) #foreign sync "(*WaitGroup).Done";
CompleteAtReturn :: (group: *Group) #go_defer #foreign sync "(*WaitGroup).Done";
Add :: (group: *Group, count: isize) #foreign sync "(*WaitGroup).Add";
Wait :: (group: *Group) #foreign sync "(*WaitGroup).Wait";
Unbox :: (value: selection.Value) -> Any #foreign reflect "Value.Interface";
Cleanup :: #type (callback: ctx.CancelFunc) -> void;
CancelAtReturn :: (cleanup: Cleanup, callback: ctx.CancelFunc) #go_defer #foreign builtin "call";
Panic :: (value: Any) #foreign builtin "panic";
RunWorker :: (group: *Group, channel: channels.Channel, name: string) {
    CompleteAtReturn(group)
    value: Event
    value.name = name
    value.count = 42
    channels.TrySend(channel, value)
}
BlockWorker :: (group: *Group, channel: channels.Channel, name: string) {
    CompleteAtReturn(group)
    cases: [1]selection.Case
    cases[0] = selection.Receive(channels.Interface(channel))
    selection.Select(cases[:])
}
Begin :: (group: *Group, channel: channels.Channel) {
    Add(group, cast(isize)1)
    Spawn(BlockWorker, group, channel, "blocked")
}
NewGate :: () -> channels.Channel {
    value: Event
    return channels.New(value, cast(isize)0)
}
CloseGate :: (channel: channels.Channel) {
    channels.Close(channel)
}
Run :: (group: *Group, name: string) -> EventResult {
    zero: Event
    channel := channels.New(zero, cast(isize)1)
    Add(group, cast(isize)1)
    Spawn(RunWorker, group, channel, name)
    name = "changed"
    Wait(group)
    cases: [1]selection.Case
    cases[0] = selection.Receive(channels.Interface(channel))
    received := selection.Select(cases[:])
    channels.Close(channel)
    return AsEvent(Unbox(received.value))
}
Buffered :: () -> bool {
    channel := channels.New(cast(s64)0, cast(isize)1)
    if !channels.TrySend(channel, cast(s64)1) { return false }
    if channels.TrySend(channel, cast(s64)2) { return false }
    cases: [1]selection.Case
    cases[0] = selection.Receive(channels.Interface(channel))
    received := selection.Select(cases[:])
    if !received.receiveOK { return false }
    channels.Close(channel)
    received = selection.Select(cases[:])
    return !received.receiveOK
}
Cancelled :: (parent: ctx.Context, fail: bool, observed: *ctx.Context) -> ctx.Context {
    child := ctx.WithCancel(parent)
    observed.* = child.value
    CancelAtReturn(ctx.Cancel, child.cancel)
    if fail { Panic("cleanup") }
    return child.value
}
TimedOut :: (parent: ctx.Context) -> ctx.Context {
    child := ctx.WithTimeout(parent, cast(clock.Duration)1000000)
    return child.value
}
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/native.zi"
python3 - "$work/ir/native.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
for signature in [
    b'AsReader :: (value: Any) -> ReaderResult #go_results #foreign builtin "assert";',
    b'Spawn :: (callback: Worker, group: *Group, channel: channels.Channel, name: string) #foreign builtin "spawn";',
]:
    assert data.count(signature) == 1
    data = data.replace(signature, b'?' * len(signature))
path.write_bytes(data)
PY
for input in source saved; do
    root=$work
    file=$root/native.zi
    if test "$input" = saved; then root=$work/ir; file=$root/native.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main
import (
    "bytes"
    "context"
    "io"
    "reflect"
    "sync"
    "time"
)
func main() {
    reader := bytes.NewReader([]byte("hello"))
    if value := Native_AsReader(reader); !value.Present || value.Value != reader { panic("interface assertion lost identity") }
    for _, value := range []any{nil, "reader", 42} {
        if result := Native_AsReader(value); result.Present || result.Value != nil { panic("failed interface assertion") }
    }
    var typedNil *bytes.Reader
    if result := Native_AsReader(typedNil); !result.Present || result.Value == nil { panic("typed nil assertion") }
    event := Event{Name: "native", Count: 7}
    if result := Native_AsEvent(event); !result.Present || result.Value != event { panic("record assertion") }
    if result := Native_AsEvent(&event); result.Present || result.Value != (Event{}) { panic("pointer matched value assertion") }
    data := make([]byte, 3, 8)
    result := Native_AsSlice(data)
    if !result.Present || cap(result.Value) != 8 || &result.Value[0] != &data[0] { panic("slice assertion copied storage") }
    var nilSlice []byte
    if result := Native_AsSlice(nilSlice); !result.Present || result.Value != nil { panic("typed nil slice") }
    pointer := new(int32)
    if result := Native_AsPointer(pointer); !result.Present || result.Value != pointer { panic("pointer assertion") }
    var nilPointer *int32
    if result := Native_AsPointer(nilPointer); !result.Present || result.Value != nil { panic("typed nil pointer") }
    var group sync.WaitGroup
    if result := Native_Run(&group, "captured"); !result.Present || result.Value != (Event{Name: "captured", Count: 42}) { panic("spawn argument capture or typed channel") }
    if !Native_Buffered() { panic("channel capacity or close behavior") }
    gate := Native_NewGate()
    started := make(chan struct{})
    go func() {
        Native_Begin(&group, gate)
        close(started)
    }()
    select {
    case <-started:
    case <-time.After(time.Second):
        Native_CloseGate(gate)
        panic("spawn blocked its caller")
    }
    Native_CloseGate(gate)
    group.Wait()
    var observed context.Context
    cancelled := Native_Cancelled(context.Background(), false, &observed)
    if cancelled.Err() != context.Canceled { panic("cancel cleanup") }
    parent, cancel := context.WithCancel(context.Background())
    cancel()
    if child := Native_Cancelled(parent, false, &observed); child.Err() != context.Canceled { panic("parent cancellation") }
    func() {
        defer func() { if recover() != "cleanup" { panic("cancel panic identity") } }()
        Native_Cancelled(context.Background(), true, &observed)
    }()
    if observed.Err() != context.Canceled { panic("panic left context uncancelled") }
    timeout := Native_TimedOut(context.Background())
    select {
    case <-timeout.Done():
        if timeout.Err() != context.DeadlineExceeded { panic("timeout identity") }
    case <-time.After(time.Second): panic("timeout did not close")
    }
    if _, ok := any(result.Value).([]byte); !ok { panic("native slice type") }
    if reflect.TypeOf(Native_AsReader(reader).Value) != reflect.TypeOf(reader) { panic("dynamic reader type") }
    if _, err := io.ReadAll(Native_AsReader(reader).Value); err != nil { panic(err) }
}
GO
    GO111MODULE=off go run -race "$out"/*.go
    "$ziran" bundle --entry native:Answer --root "$root" --module-path std -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done
cmp "$work/source-go/native.go" "$work/saved-go/native.go"
for declaration in \
    'Bad :: (value: Any) -> Result #foreign builtin "assert";' \
    'Bad :: () -> Result #go_results #foreign builtin "assert";' \
    'Bad :: (value: s32) -> Result #go_results #foreign builtin "assert";' \
    'Bad :: (value: Any, other: Any) -> Result #go_results #foreign builtin "assert";' \
    'Bad :: (value: Any) -> s32 #go_results #foreign builtin "assert";' \
    'Bad :: (callback: Worker, count: s32) -> s32 #foreign builtin "spawn";' \
    'Bad :: (callback: Worker) #foreign builtin "spawn";' \
    'Bad :: (callback: Worker, count: string) #foreign builtin "spawn";' \
    'Bad :: (callback: Worker, count: s32) #go_defer #foreign builtin "spawn";'; do
    printf '#import "go_types"\nbuiltin :: #system_library "go:builtin";\nResult :: struct { value: s32; present: bool }\nWorker :: #type (count: s32) -> void;\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid assertion or spawn accepted: $declaration" >&2
        exit 1
    fi
done
for fields in 'value: s32' 'value: s32; present: s32' 'value: s32; present: bool; extra: bool' 'value: Vec(u8); present: bool'; do
    printf '#import "go_types"\n#import "vec"\nbuiltin :: #system_library "go:builtin";\nResult :: struct { %s }\nBad :: (value: Any) -> Result #go_results #foreign builtin "assert";\n' "$fields" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid assertion record accepted: $fields" >&2
        exit 1
    fi
done
cat > "$work/owned.zi" <<'ZI'
#import "vec"
builtin :: #system_library "go:builtin";
Worker :: #type (value: Vec(u8)) -> void;
Bad :: (callback: Worker, value: Vec(u8)) #foreign builtin "spawn";
ZI
if "$ziran" check --root "$work" --module-path std "$work/owned.zi" > "$work/bad.out" 2>&1; then
    echo 'owned value accepted by spawn' >&2
    exit 1
fi
echo 'Go interface assertions, goroutines, channels, cancellation and saved IR: passed'
