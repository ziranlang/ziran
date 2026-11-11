#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
mkdir -p "$root/build/tests/tmp"
work=$(mktemp -d "$root/build/tests/tmp/json-string-scan.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$ziran" run --target=c --root tests --module-path bench tests/benchmark_samples_test.zi
"$ziran" run --target=c --root tests/spec --module-path std tests/spec/site_release_inventory_test.zi -- "$root/scripts/site_pages.py" "$ziran" "$work/inventory"
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" tests/spec/json_string_scan_test.zi
for form in source saved; do
    input=tests/spec/json_string_scan_test.zi
    if test "$form" = saved; then input="$work/ir/json_string_scan_test.zir"; fi
    output="$work/$form-c"
    "$ziran" build --target=c --root tests/spec --module-path std -o "$output" "$input"
    printf '%s\n' '#include "json_string_scan_test.h"' 'int main(void) { return NativeMain(); }' > "$output/entry.c"
    for compiler in cc clang; do
        command -v "$compiler" >/dev/null || continue
        "$compiler" -std=c11 -O2 -DZIRAN_BOUNDS_CHECK -Iinclude -I"$output" "$output"/*.c -lm -o "$output/test"
        "$output/test"
    done
    cc -std=c11 -O2 -DZIRAN_SIMD_SCALAR -DZIRAN_BOUNDS_CHECK -fsanitize=address,undefined -Iinclude -I"$output" "$output"/*.c -lm -o "$output/test"
    "$output/test"
    "$ziran" bundle --root tests/spec --module-path std --entry json_string_scan_test:SelfTest -o "$work/$form.zx" "$input"
    test "$("$ziran" run "$work/$form.zx")" = 42
    output="$work/$form-go"
    "$ziran" build --target=go --pkg main --root tests/spec --module-path std -o "$output" "$input"
    printf '%s\n' 'package main' 'func main() { if JsonStringScanTest_NativeMain() != 0 { panic("JSON SIMD string scan differs") } }' > "$output/main.go"
    GO111MODULE=off GOMAXPROCS=2 GOFLAGS=-p=2 go run "$output"/*.go
    for name in InvalidNegative InvalidEnd InvalidLowEnd InvalidWideEnd; do
        printf '%s\n' '#include "json_string_scan_test.h"' "int main(void) { return $name(); }" > "$work/$form-c/entry.c"
        cc -std=c11 -O2 -Iinclude -I"$work/$form-c" "$work/$form-c"/*.c -lm -o "$work/$form-c/test"
        if "$work/$form-c/test" > "$work/bounds.log" 2>&1; then echo 'C string SIMD bounds not enforced'; exit 1; fi
        grep -q 'SIMD access .*out of bounds' "$work/bounds.log"
        printf '%s\n' 'package main' "func main() { JsonStringScanTest_$name() }" > "$output/main.go"
        if GO111MODULE=off GOMAXPROCS=2 GOFLAGS=-p=2 go run "$output"/*.go > "$work/bounds.log" 2>&1; then echo 'Go string SIMD bounds not enforced'; exit 1; fi
        grep -q 'SIMD access .*out of bounds' "$work/bounds.log"
        "$ziran" bundle --root tests/spec --module-path std --entry "json_string_scan_test:$name" -o "$work/bounds.zx" "$input"
        if "$ziran" run "$work/bounds.zx" > "$work/bounds.log" 2>&1; then echo 'VM string SIMD bounds not enforced'; exit 1; fi
        grep -q 'SIMD access .*out of bounds' "$work/bounds.log"
    done
done
"$ziran" build --target=c --root tests/spec --module-path std --entry json_string_guard_test:main --exe -o "$work/guard" tests/spec/json_string_guard_test.zi
"$work/guard/json_string_guard_test"
# String memory stays immutable, and a byte string cannot supply wider lanes.
for expression in 'Simd.Store(text,0,Simd.Zero(u8x16))' 'unused Simd.Load(s8x16,text,0)' 'unused Simd.Load(u16x8,text,0)'; do
    printf '%s\n' 'Simd :: #import "simd"' "main :: () { text := \"0123456789abcdef\"; $expression }" > "$work/rejected.zi"
    if "$ziran" check "$work/rejected.zi" > "$work/rejected.log" 2>&1; then echo 'invalid string SIMD access accepted'; exit 1; fi
done
cmp "$work/source.zx" "$work/saved.zx"
# More than 32 modules exercises forked emission when the managed CPU/memory
# budget permits it. Each worker must contribute its SIMD helpers to the result.
mkdir "$work/parallel"
printf '%s\n' '#import "std/json_bytes"' > "$work/parallel/entry.zi"
index=0
while test "$index" -lt 40; do
    printf 'M%s :: #import "m%s"\n' "$index" "$index" >> "$work/parallel/entry.zi"
    printf '%s\n' 'Simd :: #import "std/simd"' 'Value :: () -> s32 { value := Simd.Splat(u16x8,7); return cast(s32)Simd.Lane(value,0) }' > "$work/parallel/m$index.zi"
    index=$((index + 1))
done
printf '%s\n' '#program_export' 'Entry :: () -> s32 { if JsonStringRunEnd("01234567890123456789012345678901",0) != 32 || M0.Value() != 7 || M39.Value() != 7 { return 1 }; return 0 }' >> "$work/parallel/entry.zi"
ZIRAN_JOBS=2 "$ziran" build --target=c --root "$work/parallel" -o "$work/parallel-c" "$work/parallel/entry.zi"
printf '%s\n' '#include "entry.h"' 'int main(void) { return Entry(); }' > "$work/parallel-c/main.c"
cc -std=c11 -O2 -Iinclude -I"$work/parallel-c" "$work/parallel-c"/*.c -lm -o "$work/parallel-c/test"
"$work/parallel-c/test"
echo 'JSON SIMD string scanning passed source/saved IR, C/Go/VM, sanitizers and protected-page bounds'
