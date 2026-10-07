#!/bin/sh
set -eu
ziran=${1:-ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cat > "$work/channel_test.zi" <<'ZI'
#import "process_channel_linux"
#import "byte_text_linux"
#import "timer_linux"
libc :: #system_library "libc";
GroupRaw :: (pid: s32) -> s32 #foreign libc "getpgid";
DuplicateRaw :: (fd: s32) -> s32 #foreign libc "dup";
FlagsRaw :: (fd: s32, command: s32, value: s32) -> s32 #foreign libc "fcntl";
CloseRaw :: (fd: s32) -> s32 #foreign libc "close";
DuplicateToRaw :: (fd: s32, destination: s32) -> s32 #foreign libc "dup2";

#program_export
Main :: () -> s32 {
    args: [3]string = .["python3", "-c", "import os; b=os.read(0,4096); os.write(1,b); os.write(2,b'err'); raise SystemExit(7)"]
    channel := StartChannel(args[:], true)
    defer { unused CloseChannel(*channel, 0) }
    if channel.error != 0 || !channel.socket.valid || channel.pid <= 0 { return 1 }
    input: [5]u8 = .[0,255,10,39,92]
    if ChannelSend(channel, input[:], 1000) != 5 { return 2 }
    output: [8]u8; length: s32
    while length < 8 {
        size := ChannelReceive(channel, output[length:8], 1000)
        if size <= 0 { return 3 }; length += cast(s32)size
    }
    for i: 0..4 { if output[i] != input[i] { return 4 } }
    if TextFromBytes(output[5:8]) != "err" || !WaitChannel(*channel, 1000) || channel.code != 7 { return 5 }
    if ChannelSend(channel, input[:], 1000) != -1 { return 6 }
    if !CloseChannel(*channel, 1000) || channel.socket.valid || !channel.exited { return 7 }
    if !CloseChannel(*channel, 0) { return 8 }

    literal: [3]string = .["printf", "%s", "$HOME;`printf BAD` ' "]
    channel = StartChannel(literal[:], false)
    exact: [64]u8
    size := ChannelReceive(channel, exact[:], 1000)
    if size <= 0 || TextFromBytes(exact[:size]) != literal[2] { return 9 }
    if !WaitChannel(*channel, 1000) || channel.code != 0 || !CloseChannel(*channel, 0) { return 10 }

    // Daemons can launch children with closed standard descriptors. Restore
    // only this test process's descriptors, after moving its channel above 2.
    for mask: cast(s32)0..cast(s32)7 {
        saved: [3]s32
        for fd: cast(s32)0..cast(s32)2 { saved[fd] = DuplicateRaw(fd); if saved[fd] < 0 { return 19 } }
        for fd: cast(s32)0..cast(s32)2 { if (mask & (1 << fd)) != 0 { unused CloseRaw(fd) } }
        channel = StartChannel(literal[:], true)
        if channel.socket.valid && channel.socket.fd <= 2 {
            moved := FlagsRaw(channel.socket.fd, 1030, 3)
            unused CloseRaw(channel.socket.fd)
            channel.socket.fd = moved; channel.socket.valid = moved >= 0
        }
        for fd: cast(s32)0..cast(s32)2 {
            if DuplicateToRaw(saved[fd], fd) < 0 { return 20 }
            unused CloseRaw(saved[fd])
        }
        if channel.error != 0 || !channel.socket.valid { return 21 }
        size = ChannelReceive(channel, exact[:], 1000)
        if size <= 0 || TextFromBytes(exact[:size]) != literal[2] {
            print("closed standard descriptor mask % lost child output\n", mask)
            unused CloseChannel(*channel, 0); return 22
        }
        if !WaitChannel(*channel, 1000) || channel.code != 0 || !CloseChannel(*channel, 0) { return 23 }
    }

    sleeper: [2]string = .["sleep", "60"]
    channel = StartChannel(sleeper[:], true)
    started := MonotonicMilliseconds()
    if ChannelReceive(channel, exact[:], 20) != -1 { return 11 }
    if MonotonicMilliseconds() - started > 500 || WaitChannel(*channel, 0) { return 12 }
    if GroupRaw(channel.pid) != channel.pid || GroupRaw(channel.pid) == GroupRaw(0) { return 17 }
    sibling := StartChannel(sleeper[:], true)
    defer { unused CloseChannel(*sibling, 0) }
    if !CloseChannel(*channel, 0) || channel.code != 137 { return 13 }
    if !CloseChannel(*channel, 0) { return 14 }
    if WaitChannel(*sibling, 0) || !CloseChannel(*sibling, 0) { return 18 }

    absent: [1]string = .["ziran-channel-no-such-executable"]
    channel = StartChannel(absent[:], true)
    if !WaitChannel(*channel, 1000) || channel.code != 127 || !CloseChannel(*channel, 0) { return 15 }
    empty: [0]string
    channel = StartChannel(empty[:], true)
    if channel.error == 0 || channel.pid != 0 || channel.socket.valid { return 16 }

    print("process channel: binary I/O, exact arguments, stderr, deadlines and shutdown passed\n")
    return 0
}
ZI
"$ziran" ir --root "$work" --module-path "$root/std" -o "$work/ir" "$work/channel_test.zi"
for form in source saved; do
    input="$work/channel_test.zi"
    modules="$root/std"
    source_root="$work"
    if test "$form" = saved; then
        input="$work/ir/channel_test.zir"
        modules="$work/ir"
        source_root="$work/ir"
    fi
    "$ziran" build --target=c --root "$source_root" --module-path "$modules" \
        --entry channel_test:Main --exe -o "$work/$form-c" "$input"
    "$work/$form-c/channel_test"
    "$ziran" build --target=cpp --no-main --root "$source_root" --module-path "$modules" \
        -o "$work/$form-cpp" "$input"
    printf '#include "channel_test.hpp"\nint main() { return Main(); }\n' > "$work/$form-cpp/main.cpp"
    "${CXX:-c++}" -std=c++17 -O2 -I"$root/include" -I"$work/$form-cpp" \
        "$work/$form-cpp"/*.cpp -lm -o "$work/$form-cpp/channel_test"
    "$work/$form-cpp/channel_test"
done
