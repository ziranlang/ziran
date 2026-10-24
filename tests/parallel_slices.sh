#!/bin/sh
# A #parallel body writes through slices: each iteration takes its own
# window, S[it * K : it * K + K], or assigns S[it]. Native code checks the
# written slice against the other slices, strings and arrays the body reads
# and runs the region in order when they share storage, so a region that
# reads the buffer it writes still gives the serial result. C builds,
# natively run bundles, the portable VM and Go all agree.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

cat > "$work/slices.zi" <<'ZI'
Flip :: (pixels: []u8, output: []u8, width: s64, height: s64) {
    #parallel for y: 0..height - 1 {
        row := output[y * width : y * width + width]
        x: s64 = 0
        while x < width {
            row[x] = pixels[(height - 1 - y) * width + x] + 1
            x += 1
        }
    }
}
Scale :: (values: []s64, factor: s64) {
    #parallel for 0..values.count - 1 {
        values[it] = values[it] * factor
    }
}
#program_export
Answer :: () -> s64 {
    source: [64]u8
    target: [64]u8
    numbers: [10]s64
    i: s64 = 0
    while i < 64 {
        source[i] = cast(u8)i
        i += 1
    }
    i = 0
    while i < 10 {
        numbers[i] = i + 1
        i += 1
    }
    Flip(source[:], target[:], 8, 8)
    Scale(numbers[:], 3)
    total: s64 = 0
    i = 0
    while i < 64 {
        total += cast(s64)target[i]
        i += 1
    }
    i = 0
    while i < 10 {
        total += numbers[i] * 1000
        i += 1
    }
    Flip(target[:], target[:], 8, 8)
    i = 0
    while i < 64 {
        total += cast(s64)target[i] * (i + 1)
        i += 1
    }
    return total
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/slices.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        module=$work/slices.zi
    else
        root=$work/ir
        module=$work/ir/slices.zir
    fi
    "$ziran" bundle --root "$root" --entry slices:Answer -o "$work/$input.zx" "$module"
    for threads in 1 4; do
        test "$(ZIRAN_NATIVE_CACHE=0 ZIRAN_PAR_THREADS=$threads "$ziran" run "$work/$input.zx")" = 205368
    done
    "$ziran" build --target=c --entry slices:Answer --root "$root" -o "$work/c-$input" "$module"
    cat > "$work/c-$input/main.c" <<'C'
#include "slices.h"
int main(void) { return Answer() == 205368 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -pedantic-errors -pthread -I"$repo/include" -I"$work/c-$input" \
        "$work/c-$input"/*.c -o "$work/c-$input/app"
    for threads in 1 2 8; do
        ZIRAN_PAR_THREADS=$threads "$work/c-$input/app"
    done
    "$ziran" build --target=go --pkg main --entry slices:Answer --root "$root" \
        -o "$work/go-$input" "$module"
    cat > "$work/go-$input/main.go" <<'GO'
package main
func main() { if Slices_Answer() != 205368 { panic("parallel slices") } }
GO
    GO111MODULE=off go run "$work/go-$input"/*.go
done
cmp "$work/source.zx" "$work/saved.zx"
grep -q "ziran_parallel_overlap" "$work/c-source/slices.c"

# Writes into an array the body declares stay inside the iteration.
cat > "$work/local.zi" <<'ZI'
#program_export
Answer :: () -> s64 {
    out: [8]s64
    #parallel for 0..7 {
        scratch: [4]s64
        scratch[0] = it
        scratch[1] = scratch[0] * 2
        out[it] = scratch[1]
    }
    return out[3] + out[7]
}
ZI
"$ziran" bundle --root "$work" --entry local:Answer -o "$work/local.zx" "$work/local.zi"
test "$(ZIRAN_NATIVE_CACHE=0 ZIRAN_PAR_THREADS=4 "$ziran" run "$work/local.zx")" = 20

# A callee may write through a slice it is handed only when the iteration
# owns that storage: an array it declares, or its own window.
cat > "$work/callees.zi" <<'ZI'
Fill :: (view: []s64, value: s64) {
    view[0] = value
}
Relay :: (view: []s64, value: s64) {
    tail := view[1:]
    Fill(tail, value + 1)
}
#program_export
Answer :: () -> s64 {
    out: [8]s64
    pairs: [16]s64
    shared := pairs[:]
    #parallel for 0..7 {
        scratch: [4]s64
        Fill(scratch[:], it)
        Relay(scratch[:], it)
        row := shared[it * 2 : it * 2 + 2]
        Fill(row, scratch[0] + scratch[1] + 1)
        out[it] = scratch[0] + scratch[1] + 1
    }
    total: s64 = 0
    for 0..7 { total += out[it] + pairs[it * 2] }
    return total
}
ZI
"$ziran" bundle --root "$work" --entry callees:Answer -o "$work/callees.zx" "$work/callees.zi"
for threads in 1 4; do
    test "$(ZIRAN_NATIVE_CACHE=0 ZIRAN_PAR_THREADS=$threads "$ziran" run "$work/callees.zx")" = 144
done

reject() {
    name=$1
    pattern=$2
    if "$ziran" check --root "$work" "$work/$name.zi" 2> "$work/$name.err"; then
        echo "$3 was accepted" >&2
        exit 1
    fi
    grep -Fq "$pattern" "$work/$name.err" || { cat "$work/$name.err" >&2; exit 1; }
}
bad() {
    cat > "$work/$1.zi" <<ZI
counter: s64;
Box :: struct {
    items: []u8
}
Peek :: (value: s64) -> s64 { return value + counter }
Bad :: (output: []u8, box: Box, width: s64) {
$2
}
ZI
}
bad other_row '    #parallel for y: 0..3 {
        row := output[y * width : y * width + width]
        row[0] = output[0]
    }'
reject other_row "uses output only through its per-iteration windows" "a read outside the window"
bad wide_window '    #parallel for y: 0..3 {
        row := output[y * width : y * width + width + 1]
        row[0] = 1
    }'
reject wide_window "iteration-local binding: row" "a window wider than its stride"
bad observing '    #parallel for 0..3 {
        output[it] = cast(u8)Peek(it)
    }'
reject observing "need pure callees: Peek is observing" "an observing callee"
bad nested_slice '    #parallel for 0..3 {
        output[it] = box.items[it]
    }'
reject nested_slice "box refers to storage native code cannot check" "a record holding a slice"
cat > "$work/shared_callee.zi" <<'ZI'
Fill :: (view: []s64, value: s64) {
    view[0] = value
}
Relay :: (view: []s64, value: s64) {
    tail := view[1:]
    Fill(tail, value + 1)
}
Direct :: (shared: []s64) {
    #parallel for 0..7 {
        Fill(shared, it)
    }
}
Passed :: (shared: []s64) {
    #parallel for 0..7 {
        Relay(shared, it)
    }
}
ZI
reject shared_callee "passes shared storage to Fill, which writes through it" "a callee writing a shared slice"
sed -i '/^Direct ::/,/^}/d' "$work/shared_callee.zi"
reject shared_callee "passes shared storage to Relay, which writes through it" "a callee passing a shared slice on"
echo "parallel bodies wrote through slice windows and slots on every target"
