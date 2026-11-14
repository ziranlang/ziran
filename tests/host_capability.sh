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
