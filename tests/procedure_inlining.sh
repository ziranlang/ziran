#!/bin/sh
# #inline asks generated C to always inline a procedure: GCC and Clang see
# always_inline, and a public one gets its header copy whatever its size.
# #cold keeps a procedure out of line and marks it rarely run, and other
# modules call its one definition instead of a header copy. Results agree
# on C at -O0 and -O2, Go, native and the portable VM from source and saved
# IR, and misuse is rejected.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx-vm"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/bits.zi" <<'ZI'
Reader :: struct { data: []u8; at: s64; window: u64; count: s32; }
// Large enough that the size limit alone would keep it out of headers.
Mix :: (value: s64) -> s64 #inline {
    total := value
    for round: 0..3 {
        total = total * 31 + Step(round)
        if total > 1000000 { total -= 999983 }
        if total < 0 { total = -total }
        total ^= total >> 3
        total += Step(round * 2)
        total = total % 1000003
    }
    return total
}
Step :: (value: s64) -> s64 #inline { return value * 3 + 1 }
// The slow path that refills the window.
Refill :: (reader: *Reader) #cold {
    while reader.count <= 56 && reader.at < reader.data.count {
        reader.window |= cast(u64) reader.data[reader.at] << cast(u64) (56 - reader.count)
        reader.at += 1
        reader.count += 8
    }
}
Read :: (reader: *Reader, bits: s32) -> u64 {
    if reader.count < bits { Refill(reader) }
    value := reader.window >> cast(u64) (64 - bits)
    reader.window <<= cast(u64) bits
    reader.count -= bits
    return value
}
Sum :: (reader: *Reader) -> s64 { return Tally(reader) + Twice(3) }
#scope_file
Tally :: (reader: *Reader) -> s64 #cold { return cast(s64) reader.count }
Twice :: (value: s64) -> s64 #inline { return value * 2 }
ZI
cat > "$work/app.zi" <<'ZI'
Bits :: #import "bits";
#program_export
Answer :: () -> s64 {
    bytes: [4]u8
    bytes[0] = 0xA5; bytes[1] = 0x0F; bytes[2] = 0xF0; bytes[3] = 0x3C
    reader: Bits.Reader
    reader.data = bytes[0:4]
    total: s64 = 0
    for i: 0..7 { total += cast(s64) Bits.Read(*reader, 4) }
    if total != 10 + 5 + 0 + 15 + 15 + 0 + 3 + 12 { return 1 }
    if Bits.Mix(5) != Expected() { return 2 }
    if Bits.Sum(*reader) != 6 { return 3 }
    return 42
}
// Mix(5) step by step, without the procedures under test.
Expected :: () -> s64 {
    total: s64 = 5
    for round: 0..3 {
        total = total * 31 + round * 3 + 1
        if total > 1000000 { total -= 999983 }
        if total < 0 { total = -total }
        total ^= total >> 3
        total += round * 6 + 1
        total = total % 1000003
    }
    return total
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    test "$("$run_vm" run "$work/$input.zx")" = 42

    output=$work/c-$input
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    printf '#include "app.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$output/main.c"
    for level in -O0 -O2; do
        "${CC:-cc}" -std=c99 -pedantic-errors -Wall -Werror $level \
            -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
        "$output/app"
    done
    grep -q '^#define ZIRAN_ALWAYS_INLINE __attribute__((always_inline))' "$output/bits.h"
    grep -q '^#define ZIRAN_COLD __attribute__((noinline, cold))' "$output/bits.h"
    grep -q '^static inline ZIRAN_ALWAYS_INLINE int64_t bits_Mix_ziran_inline(' "$output/bits.h"
    grep -q '^#define bits_Mix(...) bits_Mix_ziran_inline(__VA_ARGS__)' "$output/bits.h"
    grep -q '^ZIRAN_COLD void bits_Refill(' "$output/bits.h"
    if grep -q 'bits_Refill_ziran_inline' "$output/bits.h"; then
        echo 'a #cold procedure was copied into its header' >&2
        exit 1
    fi
    grep -q '^ZIRAN_ALWAYS_INLINE inline int64_t$' "$output/bits.c"
    grep -q '^ZIRAN_COLD void$' "$output/bits.c"
    grep -q '^static inline ZIRAN_ALWAYS_INLINE int64_t bits_Twice' "$output/bits.c"
    grep -q '^static ZIRAN_COLD int64_t bits_Tally' "$output/bits.c"
    # Other modules call the cold refill and inline the rest.
    "${CC:-cc}" -std=c99 -O2 -I"$repo/include" -I"$output" -c "$output/app.c" -o "$work/app.o"
    nm -u "$work/app.o" > "$work/undefined"
    grep -q 'bits_Refill' "$work/undefined"
    if grep -q 'bits_Mix\|bits_Step' "$work/undefined"; then
        echo 'an #inline procedure was called out of line' >&2
        exit 1
    fi

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    grep -B1 '^func Bits_Refill(' "$output"/*.go | grep -q '^.*//go:noinline'
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("procedure inlining") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zx" "$work/saved.zx"

reject() {
    mkdir -p "$work/bad"
    printf '%s\n' "$1" > "$work/bad/bad.zi"
    if "$ziran" check --root "$work/bad" "$work/bad/bad.zi" > "$work/bad/out" 2>&1; then
        echo "accepted: $1" >&2
        exit 1
    fi
    grep -q "$2" "$work/bad/out" || { cat "$work/bad/out" >&2; exit 1; }
}
reject 'Both :: () -> s64 #inline #cold { return 1 }' 'both #inline and #cold'
reject 'Twice :: () -> s64 #inline #inline { return 1 }' 'duplicate #inline or #cold'
reject 'Count :: () -> s64 #cold(1) { return 1 }' 'do not take arguments'
reject 'Down :: (n: s64) -> s64 #inline { if n <= 0 { return 0 }; return Down(n - 1) }' 'calls itself'
reject 'host_api :: #system_library "host_api";
Host :: (n: s32) -> s32 #inline #foreign host_api;' 'need a procedure body'
