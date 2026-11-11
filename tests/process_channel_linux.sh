#!/bin/sh
set -eu
ziran=${1:-ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/build/tests"
work=$(mktemp -d "$root/build/tests/process-channel.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
"$ziran" build --target=c --exe --root "$root/tests" --entry process_fixture:main \
    -o "$work/child" "$root/tests/process_fixture.zi"
export ZIRAN_PROCESS_FIXTURE="$work/child/process_fixture"
cat > "$work/channel_test.zi" <<'ZI'
#import "process_channel_linux"
#import "byte_text_linux"
#import "timer_linux"
#import "environment_linux"
using Vectors :: #import "vec";
libc :: #system_library "libc";
GroupRaw :: (pid: s32) -> s32 #foreign libc "getpgid";
DuplicateRaw :: (fd: s32) -> s32 #foreign libc "dup";
FlagsRaw :: (fd: s32, command: s32, value: s32) -> s32 #foreign libc "fcntl";
CloseRaw :: (fd: s32) -> s32 #foreign libc "close";
DuplicateToRaw :: (fd: s32, destination: s32) -> s32 #foreign libc "dup2";
PrctlRaw :: (option: s32,a: usize,b: usize,c: usize,d: usize) -> s32 #foreign libc "prctl";
WaitIdRaw :: (kind: s32,id: u32,info: *u8,options: s32) -> s32 #foreign libc "waitid";
WaitChildRaw :: (pid: s32,status: *s32,options: s32) -> s32 #foreign libc "waitpid";
ErrnoRaw :: () -> *s32 #foreign libc "__errno_location";

CancelDescendant :: () -> bool {
    // This disposable test process adopts/reaps only its own descendants.
    if PrctlRaw(36,1,0,0,0) != 0 { return false }
    sleeper: [2]string = .["sleep","30"]
    unrelated := StartChannel(sleeper[:],false,true)
    defer { unused CancelChannel(*unrelated) }
    arguments: [3]string = .["sh","-c","sleep 30 & echo $!; exit 0"]
    owner := StartChannel(arguments[:],false,true)
    defer { unused CancelChannel(*owner) }
    if owner.pid <= 0 || unrelated.pid <= 0 { return false }
    bytes: [32]u8
    count := ChannelReceive(owner,bytes[:],1000)
    if count < 2 { return false }
    descendant: s32 = 0
    index: s64 = 0
    while index < count && bytes[index] != 10 {
        if bytes[index] < 48 || bytes[index] > 57 { return false }
        descendant = descendant * 10 + cast(s32)(bytes[index]-48)
        index += 1
    }
    if descendant <= 0 || GroupRaw(descendant) != owner.pid { return false }
    // WNOWAIT proves the direct child exited without consuming its PID.
    information: [16]u64
    deadline := MonotonicMilliseconds() + 1000
    while true {
        if WaitIdRaw(1,cast(u32)owner.pid,cast(*u8)*information[0],4 | 1 | 16777216) != 0 { return false }
        if cast(u32)information[2] == cast(u32)owner.pid { break }
        if MonotonicMilliseconds() >= deadline || !SleepMilliseconds(1) { return false }
    }
    started := MonotonicMilliseconds()
    if !CancelChannel(*owner) || !owner.exited || owner.socket.valid || owner.pid != 0 ||
        !CancelChannel(*owner) || MonotonicMilliseconds()-started > 1000 { return false }
    status: s32
    if WaitChildRaw(descendant,*status,0) != descendant || (status & 127) != 9 { return false }
    return !WaitChannel(*unrelated,0) && CancelChannel(*unrelated)
}


ArgumentBoundary :: (count: s32) -> bool {
    arguments: Vec(string)
    VecPush(arguments, "printf")
    VecPush(arguments, "%s|")
    expected: Vec(u8)
    index: s32 = 2
    while index < count {
        value := ifx index == count - 1 then "last $HOME;`literal`" else "argument with spaces"
        VecPush(arguments, value)
        BuilderPrint(*expected, "%|", value)
        index += 1
    }
    wanted := BuilderFinish(expected)
    channel := StartChannel(VecSlice(arguments, 0, arguments.count), false)
    defer { unused CloseChannel(*channel, 0) }
    if channel.error != 0 || !channel.socket.valid || channel.pid <= 0 { return false }
    output: [1024]u8
    received: s64 = 0
    while received < wanted.count {
        size := ChannelReceive(channel, output[received:wanted.count], 1000)
        if size <= 0 { return false }
        received += size
    }
    return TextFromBytes(output[0:received]) == wanted && WaitChannel(*channel, 1000) && channel.code == 0
}

#program_export
Main :: () -> s32 {
    args: [2]string = .[EnvironmentText("ZIRAN_PROCESS_FIXTURE"), "echo-bytes"]
    channel := StartChannel(args[:], true)
    defer { unused CloseChannel(*channel, 0) }
    if channel.error != 0 || !channel.socket.valid || channel.pid <= 0 { return 1 }
    input: [5]u8 = .[0,255,10,39,92]
    sent := ChannelSend(channel, input[:], 1000)
    if sent != 5 {
        error := <<ErrnoRaw()
        waited := WaitChannel(*channel, 1000)
        print("binary fixture % sent % bytes: errno %, child reaped %, fd valid %\n", args[0], sent, error, waited, channel.socket.valid)
        return 2
    }
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

    noisy: [3]string = .["sh", "-c", "printf out; printf err >&2"]
    // Daemons can launch children with closed standard descriptors. Restore
    // only this test process's descriptors, after moving its channel above 2.
    for variant: cast(s32)0..cast(s32)1 {
      for mask: cast(s32)0..cast(s32)7 {
        saved: [3]s32
        for fd: cast(s32)0..cast(s32)2 { saved[fd] = DuplicateRaw(fd); if saved[fd] < 0 { return 19 } }
        for fd: cast(s32)0..cast(s32)2 { if (mask & (1 << fd)) != 0 { unused CloseRaw(fd) } }
        channel = StartChannel(ifx variant == 0 then literal[:] else noisy[:], variant == 0, variant == 1)
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
        wanted := ifx variant == 0 then literal[2] else "out"
        if size <= 0 || TextFromBytes(exact[:size]) != wanted {
            print("closed standard descriptor mask % lost child output\n", mask)
            unused CloseChannel(*channel, 0); return 22
        }
        if !WaitChannel(*channel, 1000) || channel.code != 0 || !CloseChannel(*channel, 0) { return 23 }
      }
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

    if !ArgumentBoundary(17) || !ArgumentBoundary(32) { return 24 }
    oversized: [33]string
    oversized[0] = "sleep"; oversized[1] = "60"
    channel = StartChannel(oversized[:], false)
    if channel.error != 22 || channel.pid != 0 || channel.socket.valid { return 25 }
    channel = StartChannel(literal[:], true, true)
    if channel.error != 22 || channel.pid != 0 || channel.socket.valid { return 26 }

    // A writer may read its complete bounded request until EOF before replying.
    // Half-close input only: preserve exact binary output and child ownership.
    eof_args: [2]string = .[EnvironmentText("ZIRAN_PROCESS_FIXTURE"), "echo-eof"]
    channel = StartChannel(eof_args[:], false, true)
    if channel.error != 0 || channel.pid <= 0 { return 27 }
    if ChannelSend(channel,input[:],1000) != 5 || !CloseChannelInput(channel) ||
        !CloseChannelInput(channel) { return 28 }
    if ChannelSend(channel,input[:],1000) != -1 || !channel.socket.valid { return 29 }
    length = 0
    while length < 8 {
        count := ChannelReceive(channel,output[length:8],1000)
        if count <= 0 { return 30 }; length += cast(s32)count
    }
    for i: 0..4 { if output[i] != input[i] { return 31 } }
    if TextFromBytes(output[5:8]) != "EOF" || ChannelReceive(channel,exact[:],1000) != 0 ||
        !WaitChannel(*channel,1000) || channel.code != 0 || !CloseChannel(*channel,0) ||
        CloseChannelInput(channel) { return 32 }
    if !CancelDescendant() { return 33 }
    print("process channel: owned descendant cancellation, binary I/O, EOF-framed input, exact 17/32 arguments, 33 rejected before launch, stderr, deadlines and shutdown passed\n")
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
    "$work/$form-c/channel_test" 2>"$work/$form.stderr"
    if test -s "$work/$form.stderr"; then
        printf 'discard_stderr leaked child diagnostics: %s bytes\n' "$(wc -c < "$work/$form.stderr")" >&2
        exit 1
    fi
done
