#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/bindings.zi" <<'ZI'
#import "sync_go"
#import "text_go"
clock :: #import "time_go";
builtin :: #system_library "go:builtin";
Counter :: struct { mutex: Mutex; value: s64 }
Allocate :: () -> *Counter #foreign builtin "new";
Bytes :: (count: isize, capacity: isize) -> []u8 #foreign builtin "make";
AppendByte :: (values: []u8, value: u8) -> []u8 #foreign builtin "append";
New :: () -> *Counter {
    return Allocate()
}
Increment :: (counter: *Counter) {
    Lock(*counter.mutex)
    defer Unlock(*counter.mutex)
    counter.value += 1
}
Value :: (counter: *Counter) -> s64 {
    Lock(*counter.mutex)
    defer Unlock(*counter.mutex)
    return counter.value
}
MakeBytes :: (count: isize, capacity: isize) -> []u8 {
    return Bytes(count, capacity)
}
Text :: (value: []u8) -> string {
    return FromBytes(value)
}
Append :: (values: []u8, value: u8) -> []u8 {
    return AppendByte(values, value)
}
Shift :: (value: clock.Time, nanoseconds: s64) -> clock.Time {
    return clock.Add(value, cast(clock.Duration)nanoseconds)
}
Later :: (value: clock.Time, other: clock.Time) -> bool {
    return clock.After(value, other)
}
Now :: () -> clock.Time {
    return clock.Now()
}
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/bindings.zi"
for input in source saved; do
    root=$work
    file=$work/bindings.zi
    if test "$input" = saved; then root=$work/ir; file=$root/bindings.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main
import (
    "sync"
    "time"
)
func main() {
    counter := Bindings_New()
    var workers sync.WaitGroup
    for i := 0; i < 16; i++ {
        workers.Add(1)
        go func() {
            defer workers.Done()
            for j := 0; j < 100; j++ { Bindings_Increment(counter) }
        }()
    }
    workers.Wait()
    if counter == nil || Bindings_Value(counter) != 1600 { panic("mutex/heap allocation") }
    bytes := Bindings_MakeBytes(2, 8)
    if len(bytes) != 2 || cap(bytes) != 8 || bytes[0] != 0 || Bindings_MakeBytes(0, 0) == nil { panic("make semantics") }
    data := []byte{0, 255, 195, 169}
    text := Bindings_Text(data)
    data[0] = 99
    if text != "\x00\xffé" || Bindings_Text(nil) != "" { panic("byte-preserving string copy") }
    grown := Bindings_Append(nil, 255)
    if len(grown) != 1 || grown[0] != 255 { panic("nil slice append") }
    grown = Bindings_Append(bytes, 42)
    if len(grown) != 3 || cap(grown) != 8 || grown[2] != 42 || &grown[0] != &bytes[0] { panic("append length, capacity or backing storage") }
    now := Bindings_Now()
    shifted := Bindings_Shift(now, 123)
    if shifted != now.Add(123) || !Bindings_Later(shifted, now) || Bindings_Later(now, now) || Bindings_Later(time.Time{}, now) { panic("native monotonic timestamp") }
}
GO
    GO111MODULE=off go run -race "$out"/*.go
done
cmp "$work/source-go/bindings.go" "$work/saved-go/bindings.go"
for declaration in \
    'Lock :: () #foreign go_sync "(*Mutex).Lock";' \
    'Lock :: (value: Mutex) #foreign go_sync "(*Mutex).Lock";' \
    'Lock :: (value: *s64) #foreign go_sync "(*Mutex).Lock";' \
    'Lock :: (value: *Mutex) #foreign go_sync "(*Mutex).Bad.More";' \
    'Alloc :: () -> s64 #foreign builtin "new";' \
    'Alloc :: (count: s64) -> *Mutex #foreign builtin "new";' \
    'Alloc :: () -> *void #foreign builtin "new";' \
    'Bytes :: (count: string) -> []u8 #foreign builtin "make";' \
    'Bytes :: () -> []u8 #foreign builtin "make";' \
    'Bytes :: (count: s64) -> s64 #foreign builtin "make";' \
    'Append :: (values: []u8, value: s64) -> []u8 #foreign builtin "append";' \
    'Append :: (values: []u8, value: u8) -> []s64 #foreign builtin "append";' \
    'Append :: (values: []u8) -> []u8 #foreign builtin "append";' \
    'Append :: (values: s64, value: s64) -> s64 #foreign builtin "append";'; do
    printf 'go_sync :: #system_library "go:sync";\nbuiltin :: #system_library "go:builtin";\nMutex :: #type #foreign go_sync "Mutex";\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid Go binding accepted: $declaration" >&2
        exit 1
    fi
done
echo 'Go method receivers, mutexes, monotonic time, allocation and saved IR: passed'
