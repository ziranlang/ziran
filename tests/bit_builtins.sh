#!/bin/sh
# clz, ctz, popcount and bswap agree on C, Go and the portable VM, from
# source and from saved IR, with the width as the count for zero.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Swap :: (value: u32) -> u32 { return bswap(value) }
#program_export
Answer :: () -> s64 {
    a: u8 = 0x10
    b: u16 = 0x0100
    c: u32 = 0x00f00000
    d: u64 = 0x0000000100000000
    zero8: u8 = 0
    zero16: u16 = 0
    zero32: u32 = 0
    zero64: u64 = 0
    total: s64 = 0
    if clz(a) != 3 || ctz(a) != 4 || popcount(a) != 1 || bswap(a) != 0x10 { return 1 }
    if clz(b) != 7 || ctz(b) != 8 || popcount(b) != 1 || bswap(b) != 0x0001 { return 2 }
    if clz(c) != 8 || ctz(c) != 20 || popcount(c) != 4 || bswap(c) != 0x0000f000 { return 3 }
    if clz(d) != 31 || ctz(d) != 32 || popcount(d) != 1 || bswap(d) != 0x01000000 { return 4 }
    if clz(zero8) != 8 || ctz(zero8) != 8 || popcount(zero8) != 0 || bswap(zero8) != 0 { return 5 }
    if clz(zero16) != 16 || ctz(zero16) != 16 || popcount(zero16) != 0 || bswap(zero16) != 0 { return 6 }
    if clz(zero32) != 32 || ctz(zero32) != 32 || popcount(zero32) != 0 || bswap(zero32) != 0 { return 7 }
    if clz(zero64) != 64 || ctz(zero64) != 64 || popcount(zero64) != 0 || bswap(zero64) != 0 { return 8 }
    all: u64 = 0xffffffff
    all = (all << 32) | all
    if clz(all) != 0 || ctz(all) != 0 || popcount(all) != 64 || bswap(all) != all { return 9 }
    swapped: u64 = 0x0102030405060708
    if bswap(swapped) != 0x0807060504030201 { return 10 }
    if Swap(0x12345678) != 0x78563412 || bswap(bswap(c)) != c { return 11 }
    // The result keeps the operand's type.
    narrow: u8 = popcount(a) + clz(a)
    if narrow != 4 { return 12 }
    for i: 0..63 {
        bit: u64 = cast(u64) 1 << cast(u64) i
        total += cast(s64) (clz(bit) + ctz(bit) + popcount(bit))
    }
    for i: 0..31 {
        low: u32 = (cast(u32) 1 << cast(u32) i) - 1
        total += cast(s64) popcount(low) - i
    }
    return total - 64 * 64 + 42
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
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42

    output=$work/c-$input
    "$ziran" build --target=c --entry app:Answer --root "$root" \
        -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    # Compiler builtins, then the portable fallback a compiler without them uses.
    "${CC:-cc}" -std=c99 -pedantic-errors -O2 \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    "${CC:-cc}" -std=c99 -pedantic-errors -O2 -D__TINYC__ \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/portable"
    "$output/portable"
    grep -q 'ZiClz(' "$output/app.c"
    grep -q 'ZiBswap64(' "$output/app.c"
    if grep -q 'IntegerOp(.*Zi\(Clz\|Ctz\|Popcount\)' "$output/app.c"; then
        echo 'bit builtin result went through IntegerOp' >&2
        exit 1
    fi

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --entry app:Answer \
        --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("bit builtins") } }
GO
    GO111MODULE=off go run "$output"/*.go
    grep -q 'zibits.LeadingZeros64(' "$output/app.go"
done
cmp "$work/source.zx" "$work/saved.zx"

# Only bswap of a u8 needs no math/bits import in Go.
cat > "$work/byte_swap.zi" <<'ZI'
#program_export
Answer :: () -> s64 { value: u8 = 42; return cast(s64) bswap(value) }
ZI
"$ziran" build --target=go --pkg main --entry byte_swap:Answer \
    --root "$work" -o "$work/byte-go" "$work/byte_swap.zi"
cat > "$work/byte-go/main.go" <<'GO'
package main
func main() { if ByteSwap_Answer() != 42 { panic("byte swap") } }
GO
GO111MODULE=off go run "$work/byte-go"/*.go

# A program's own procedure of the same name wins.
cat > "$work/own.zi" <<'ZI'
popcount :: (value: s64) -> s64 { return value + 1 }
#program_export
Answer :: () -> s64 { return popcount(41) }
ZI
"$ziran" bundle --root "$work" --entry own:Answer -o "$work/own.zx" "$work/own.zi"
test "$("$ziran" run "$work/own.zx")" = 42

reject() {
    name=$1
    body=$2
    printf '#program_export\nAnswer :: () -> u64 {\n    %s\n}\n' "$body" \
        > "$work/$name.zi"
    if "$ziran" check --root "$work" "$work/$name.zi" 2> "$work/$name.err"; then
        echo "accepted $name" >&2
        exit 1
    fi
    grep -q 'takes one u8, u16, u32 or u64 value' "$work/$name.err"
}
reject signed 'value: s32 = 1; return cast(u64) clz(value)'
reject literal 'return cast(u64) ctz(8)'
reject two 'value: u32 = 1; return cast(u64) popcount(value, value)'
reject none 'return cast(u64) bswap()'
reject float 'value: float32 = 1.0; return cast(u64) clz(value)'
