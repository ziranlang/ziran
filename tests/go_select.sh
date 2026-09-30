#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/channels.zi" <<'ZI'
#import "go_types"
selection :: #import "select_go";
context :: #import "context_go";
clock :: #import "time_go";
go_time :: #system_library "go:time";
builtin :: #system_library "go:builtin";
Panic :: (value: Error) #foreign builtin "panic";
StopAtReturn :: (ticker: *clock.Ticker) #go_defer #foreign go_time "(*Ticker).Stop";
Pair :: (left: Any, right: Any) -> selection.SelectResult {
    cases: [2]selection.Case
    cases[0] = selection.Receive(left)
    cases[1] = selection.Receive(right)
    return selection.Select(cases[:])
}
Send :: (channel: Any, value: Any) -> selection.SelectResult {
    cases: [1]selection.Case
    cases[0] = selection.Send(channel, value)
    return selection.Select(cases[:])
}
Default :: (channel: Any) -> selection.SelectResult {
    cases: [2]selection.Case
    cases[0] = selection.Receive(channel)
    cases[1] = selection.Default()
    return selection.Select(cases[:])
}
Wait :: (value: context.Context, ticker: *clock.Ticker) -> bool {
    return Pair(context.Done(value), clock.TickerChannel(ticker)).chosen == cast(isize)1
}
NewTicker :: (duration: s64) -> *clock.Ticker {
    return clock.NewTicker(cast(clock.Duration)duration)
}
StopTicker :: (ticker: *clock.Ticker) {
    clock.StopTicker(ticker)
}
PanicAfterTick :: (duration: s64, error: Error) {
    ticker := NewTicker(duration)
    StopAtReturn(ticker)
    Wait(context.Background(), ticker)
    Panic(error)
}
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/channels.zi"
for input in source saved; do
    root=$work
    file=$root/channels.zi
    if test "$input" = saved; then root=$work/ir; file=$root/channels.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main

import (
    "context"
    "errors"
    "reflect"
    "time"
)

func main() {
    channel := make(chan int, 1)
    channel <- 42
    received := Channels_Pair((<-chan int)(nil), (<-chan int)(channel))
    if received.Chosen != 1 || !received.ReceiveOK || received.Value.Int() != 42 { panic("native channel receipt changed") }
    close(channel)
    received = Channels_Pair(channel, nil)
    if received.Chosen != 0 || received.ReceiveOK || received.Value.Int() != 0 { panic("closed channel receipt changed") }
    defaulted := Channels_Default((<-chan int)(nil))
    if defaulted.Chosen != 1 || defaulted.ReceiveOK || defaulted.Value.IsValid() { panic("default selection changed") }
    destination := make(chan string, 1)
    sent := Channels_Send((chan<- string)(destination), "bytes\x00\xff")
    if sent.Chosen != 0 || sent.ReceiveOK || sent.Value.IsValid() || <-destination != "bytes\x00\xff" { panic("native channel send changed") }
    ctx, cancel := context.WithCancel(context.Background())
    ticker := Channels_NewTicker(int64(time.Hour))
    finished := make(chan bool, 1)
    go func() { finished <- Channels_Wait(ctx, ticker) }()
    cancel()
    select {
    case tick := <-finished:
        if tick { panic("cancellation selected a tick") }
    case <-time.After(time.Second):
        panic("native channel cancellation did not wake")
    }
    Channels_StopTicker(ticker)
    if Channels_Default(TimeGo_TickerChannel(ticker)).Chosen != 1 { panic("stopping a ticker closed its channel") }
    fast := Channels_NewTicker(int64(time.Millisecond))
    if !Channels_Wait(context.Background(), fast) { panic("native ticker did not deliver") }
    Channels_StopTicker(fast)
    for _, duration := range []int64{0, -1} {
        func() {
            defer func() { if recover() == nil { panic("invalid ticker duration accepted") } }()
            Channels_NewTicker(duration)
        }()
    }
    for _, cases := range [][]reflect.SelectCase{
        {SelectGo_Receive(42)},
        {SelectGo_Send((<-chan int)(make(chan int)), 42)},
        {SelectGo_Send(make(chan int, 1), "wrong type")},
        {SelectGo_Default(), SelectGo_Default()},
    } {
        func() {
            defer func() { if recover() == nil { panic("invalid native channel selection accepted") } }()
            SelectGo_Select(cases)
        }()
    }
    sentinel := errors.New("native channel cleanup panic")
    func() {
        defer func() { if recover() != sentinel { panic("native deferred ticker panic changed") } }()
        Channels_PanicAfterTick(int64(time.Millisecond), sentinel)
    }()
}
GO
    GO111MODULE=off go run -race "$out"/*.go
done
cmp "$work/source-go/channels.go" "$work/saved-go/channels.go"
echo 'Go native channel selection, ticker delivery, cancellation, sends and saved IR: passed'
