#!/bin/sh
set -eu

ziran=$1
host_test=$2
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/host-capability-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/host_api.zi" <<'EOF'
host_api :: #system_library "host_api";
AddTenHost :: (value: s32) -> s32 #foreign host_api;
ByteCountHost :: (value: string) -> s32 #foreign host_api;
DoubleHost :: (value: float32) -> float32 #foreign host_api;
EchoHost :: (value: u32) -> u32 #foreign host_api;
UnusedHost :: () -> s32 #foreign host_api;
#program_export
AddTen :: (value: s32) -> s32 {
    return AddTenHost(value)
}
#program_export
ByteCount :: (value: string) -> s32 {
    return ByteCountHost(value)
}
#program_export
Double :: (value: float32) -> float32 {
    return DoubleHost(value)
}
#program_export
Echo :: (value: u32) -> u32 {
    return EchoHost(value)
}
EOF
cat > "$work/handle_host.zi" <<'EOF'
#scope_export
Box :: struct {
    value: *s32
    count: s32
}
host_api :: #system_library "host_api";
MakeHandleHost :: () -> *s32 #foreign host_api;
ReadHandleHost :: (data: *s32) -> s32 #foreign host_api;
BorrowBoxHost :: () -> Box #foreign host_api;
#program_export
MakeHandle :: () -> *s32 {
    return MakeHandleHost()
}
#program_export
ReadHandle :: (data: *s32) -> s32 {
    return ReadHandleHost(data)
}
#program_export
BorrowBox :: () -> Box {
    return BorrowBoxHost()
}
EOF
cat > "$work/application.zi" <<'EOF'
#import "host_api"
#import "handle_host"
#program_export
Answer :: () -> s32 {
    if Echo(cast(u32)4294967295) != cast(u32)4294967295 { return 0 }
    handle: *s32 = MakeHandle()
    if handle == null { return 1 }
    if ReadHandle(handle) != 7 { return 2 }
    if ReadHandle(null) != -1 { return 3 }
    box: Box = BorrowBox()
    if box.count != 1 { return 4 }
    if box.value == null { return 5 }
    if ReadHandle(box.value) != 7 { return 6 }
    return AddTen(28) + ByteCount("hi") + cast(s32)Double(1.0)
}
EOF
"$ziran" ir --root "$work" -o "$work/ir" "$work/application.zi"
"$ziran" bundle --root "$work" --entry application:Answer \
    -o "$work/source.zx" "$work/application.zi"
"$ziran" bundle --root "$work/ir" --entry application:Answer \
    -o "$work/saved.zx" "$work/ir/application.zir"
cmp "$work/source.zx" "$work/saved.zx"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/bundle_assertions.zi" -- "$ziran" "$work" capabilities
"$host_test" "$work/source.zx"
"$host_test" "$work/saved.zx"
if ZIRAN_HOST_PATH="$work/no-providers" "$ziran" run "$work/source.zx" 2> "$work/missing.err"; then
    echo 'bundle unexpectedly ran without its host capabilities' >&2
    exit 1
fi
grep -Fq 'no installed runtime provider supports this bundle' "$work/missing.err"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/bundle_assertions.zi" -- "$ziran" "$work" tamper-capabilities
if "$ziran" run "$work/tampered.zx" 2> "$work/tampered.err"; then
    echo 'tampered capability list unexpectedly ran' >&2
    exit 1
fi
# The changed program fails its identity check.
grep -Fq 'executable ZX rejected' "$work/tampered.err"
# Host strings the program drops give their storage back: a frame loop of
# 100,000 host calls stays small, while strings it keeps, in a global or a
# local across later calls, stay readable.
cat > "$work/labels.zi" <<'EOF'
host_api :: #system_library "host_api";
Label :: (index: s64) -> string #foreign host_api;
kept: string = "";
calls: s64 = 0;
#program_export
Frame :: () -> s64 {
    first := Label(calls)
    calls += 1
    total: s64 = 0
    index: s64 = 0
    while index < 1000 {
        text := Label(calls)
        total += text.count
        if calls == 500 { kept = text }
        calls += 1
        index += 1
    }
    if first.count != 64 || first[0] != #char "l" || first[63] != #char "." { return -1 }
    if kept.count != 64 || kept[6] != #char "5" || kept[9] != #char "." { return -2 }
    return total
}
EOF
"$ziran" bundle --root "$work" --entry labels:Frame -o "$work/labels.zx" "$work/labels.zi"
"$host_test" "$work/labels.zx" strings
# A record that holds itself through a slice, in a global and through a host
# capability that takes and gives back such a tree. A value that holds itself
# stops at the nesting bound instead of running on.
cat > "$work/tree.zi" <<'EOF'
host_api :: #system_library "host_api";
Node :: struct {
    value: s32
    children: []Node
}
Grow :: (tree: Node) -> Node #foreign host_api;
tree: Node;
leaves: [2]Node;
ring: [1]Node;
#program_export
Answer :: () -> s32 {
    leaves[0].value = 2
    leaves[1].value = 3
    tree.value = 1
    tree.children = leaves[:]
    tree = Grow(tree)
    if tree.value != 6 || tree.children.count != 2 { return 1 }
    if tree.children[0].value != 100 || tree.children[1].children.count != 1 { return 2 }
    return tree.children[1].children[0].value
}
#program_export
Cycle :: () -> s32 {
    ring[0].value = 1
    ring[0].children = ring[:]
    return Grow(ring[0]).value
}
EOF
"$ziran" bundle --root "$work" --entry tree:Answer -o "$work/tree.zx" "$work/tree.zi"
"$ziran" bundle --root "$work" --entry tree:Cycle -o "$work/cycle.zx" "$work/tree.zi"
"$host_test" "$work/tree.zx" tree "$work/cycle.zx" 2> "$work/tree.err"
grep -Fq 'problem=host' "$work/tree.err"
cat > "$work/unsupported.zi" <<'EOF'
host_api :: #system_library "host_api";
MakeHandleHost :: () -> *s32 #foreign host_api;
#program_export
Answer :: () -> s32 {
    handle: *s32 = MakeHandleHost()
    return handle.*
}
EOF
"$ziran" bundle --root "$work" --entry unsupported:Answer \
    -o "$work/unsupported.zx" "$work/unsupported.zi"
"$host_test" "$work/unsupported.zx" dereference 2> "$work/unsupported.err"
grep -Fq 'problem=pointer' "$work/unsupported.err"
