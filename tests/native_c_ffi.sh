#!/bin/sh
# Real C library calls, pointer outputs, aliases and default symbol names.
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/foreign.c" <<'C'
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
int32_t ffi_open(void **handle) { *handle = calloc(32, 1); return *handle ? 0 : 1; }
uint8_t *ffi_bytes(void *handle) { return handle; }
void ffi_fill(uint8_t *bytes, size_t *length) { bytes[0] = 17; bytes[1] = 25; *length = 2; }
int32_t ffi_alias(uint8_t *a, uint8_t *b) { a[1] = 42; return b[0]; }
C
${CC:-cc} -shared -fPIC "$work/foreign.c" -o "$work/libforeign.so"
cat > "$work/ffi_test.zi" <<'ZI'
library :: #system_library "foreign";
c :: #system_library "c";
Open :: (handle: **void) -> s32 #foreign library "ffi_open";
Bytes :: (handle: *void) -> *u8 #foreign library "ffi_bytes";
Fill :: (bytes: *u8, length: *usize) #foreign library "ffi_fill";
Alias :: (a: *u8, b: *u8) -> s32 #foreign library "ffi_alias";
malloc :: (length: usize) -> *void #foreign c;
free :: (memory: *void) #foreign c;

#program_export
main :: () -> s32 {
    handle: *void = null
    if Open(*handle) != 0 || handle == null { return 1 }
    bytes := Bytes(handle)
    if bytes != Bytes(handle) { return 6 }
    count: usize = 0
    Fill(bytes, *count)
    if count != 2 || bytes[0] + bytes[1] != 42 { return 2 }
    free(handle)
    memory := malloc(cast(usize)32)
    if memory == null { return 3 }
    typed := cast(*u8)memory
    typed[0] = cast(u8)19
    if typed[0] != 19 { return 4 }
    free(memory)
    values: [4]u8
    if Alias(*values[0], *values[1]) != 42 || values[1] != 42 { return 5 }
    return 0
}
ZI
export LDFLAGS="-L$work" LDLIBS=-lforeign LD_LIBRARY_PATH="$work${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$ziran" ir --root "$work" -o "$work/ir" "$work/ffi_test.zi"
for input in source saved; do
    root=$work
    file=$work/ffi_test.zi
    if test "$input" = saved; then root=$work/ir; file=$root/ffi_test.zir; fi
    "$ziran" build --target=go --pkg main --exe --entry ffi_test:main --root "$root" -o "$work/go-$input" "$file"
    (cd "$work/go-$input" && GO111MODULE=off CGO_ENABLED=1 go build -o program . && ./program)
    "$ziran" build --target=rust --exe --entry ffi_test:main --root "$root" -o "$work/rust-$input" "$file"
    cargo build --quiet --offline --manifest-path "$work/rust-$input/Cargo.toml"
    "$work/rust-$input/target/debug/ziran_generated"
    "$ziran" build --target=py --exe --entry ffi_test:main --root "$root" -o "$work/py-$input" "$file"
    python3 "$work/py-$input"
done
echo 'native C ABI: Go, Rust and Python source and saved IR passed'
