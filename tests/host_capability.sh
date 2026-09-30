#!/bin/sh
set -eu

ziran=$1
host_test=$2
work=$(mktemp -d)
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
    -o "$work/source.zib" "$work/application.zi"
"$ziran" bundle --root "$work/ir" --entry application:Answer \
    -o "$work/saved.zib" "$work/ir/application.zir"
cmp "$work/source.zib" "$work/saved.zib"
python3 - "$work/source.zib" <<'PY'
from pathlib import Path
import struct
import sys
data = Path(sys.argv[1]).read_bytes()
offset = 8
for _ in range(2):
    length, = struct.unpack_from('<I', data, offset)
    offset += 4 + length
count, = struct.unpack_from('<I', data, offset)
assert count == 7, count
assert b'UnusedHost' not in data
PY
"$host_test" "$work/source.zib"
"$host_test" "$work/saved.zib"
if "$ziran" run "$work/source.zib" 2> "$work/missing.err"; then
    echo 'bundle unexpectedly ran without its host capabilities' >&2
    exit 1
fi
grep -Fq 'missing host capability:' "$work/missing.err"
python3 - "$work/source.zib" "$work/tampered.zib" <<'PY'
from pathlib import Path
import sys
data = bytearray(Path(sys.argv[1]).read_bytes())
assert data[:8] == b'ZIB\0\x19\0\0\0'
assert b'AddTenHost' in data
data[data.index(b'AddTenHost')] = ord('X')
Path(sys.argv[2]).write_bytes(data)
PY
if "$ziran" run "$work/tampered.zib" 2> "$work/tampered.err"; then
    echo 'tampered capability list unexpectedly ran' >&2
    exit 1
fi
grep -Fq 'bundle capability list differs from linked IR' "$work/tampered.err"
cat > "$work/unsupported.zi" <<'EOF'
host_api :: #system_library "host_api";
MakeHandleHost :: () -> *s32 #foreign host_api;
#program_export
Answer :: () -> s32 {
    handle: *s32 = MakeHandleHost()
    return handle.*
}
EOF
if "$ziran" bundle --root "$work" --entry unsupported:Answer \
    -o "$work/unsupported.zib" "$work/unsupported.zi" \
    2> "$work/unsupported.err"; then
    echo 'pointer dereference unexpectedly bundled' >&2
    exit 1
fi
grep -Fq 'outside the portable subset' "$work/unsupported.err"
