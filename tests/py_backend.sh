#!/bin/sh
set -eu
ziran=$1
script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
backend=py
backend_extra() {
# A C foreign function takes and returns strings as NUL-terminated C text.
cat > "$work/c_text.zi" <<'ZI'
libc :: #system_library "libc";
Length :: (text: string) -> u64 #foreign libc "strlen";
Environment :: (name: string) -> string #foreign libc "getenv";
Answer :: () -> s32 {
    if Length("hello") != cast(u64)5 { return 1 }
    if Environment("ZIRAN_PY_TEXT") != "cairo" { return 2 }
    if Environment("ZIRAN_PY_UNSET_VARIABLE") != "" { return 3 }
    return 42
}
ZI
"$ziran" build --target=py --entry c_text:Answer --root "$work" --exe \
    -o "$work/c_text" "$work/c_text.zi"
set +e
ZIRAN_PY_TEXT=cairo python3 "$work/c_text"
status=$?
set -e
test "$status" = 42

# LDLIBS names a shared library whose C foreign functions the program calls,
# as it does for a C build: crc32_combine lives in zlib, not the C library.
if python3 -c 'import ctypes.util, sys; sys.exit(0 if ctypes.util.find_library("z") else 1)'; then
    cat > "$work/linked.zi" <<'ZI'
zlib :: #system_library "z";
Combine :: (first: u64, second: u64, length: s64) -> u64 #foreign zlib "crc32_combine";
Answer :: () -> s32 {
    // crc32("a") and crc32("b") combine into crc32("ab").
    if Combine(cast(u64)3904355907, cast(u64)1908338681, cast(s64)1) != cast(u64)2659403885 { return 1 }
    return 42
}
ZI
    LDLIBS='-lm -lz' "$ziran" build --target=py --entry linked:Answer --root "$work" \
        --exe -o "$work/linked" "$work/linked.zi"
    grep -q '_link_library("z")' "$work/linked/__main__.py"
    set +e
    python3 "$work/linked"
    status=$?
    set -e
    test "$status" = 42
fi
}
. "$script_dir/lib/native_backend.sh"
