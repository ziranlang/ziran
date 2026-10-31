#!/bin/sh
set -eu

# #parallel_gpu regions give the CPU's results on any device, and return to the
# CPU when they cannot finish. Machines without a device run the CPU paths only.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY ZIRAN_GPU
export ZIRAN_GPU_MIN=1

cat > "$work/kernels.zi" <<'ZI'
Hash :: (seed: u32) -> u32 {
    value: u32 = seed * 2654435761 + 7
    rounds: s32 = 0
    while rounds < 8 {
        value = value ^ (value >> 13)
        value = value * 16777619 + (value << 3)
        value = value | 1
        if (value & 2) == 0 { value += 5 } else { value -= 3 }
        rounds += 1
    }
    return value % 1000003
}
Signed :: (a: s32, b: s32) -> s32 {
    value: s32 = a * b - (a >> 2) + ~b
    value = ifx value < -1000 then value + 7 else value ^ 0x55
    return -value
}
Settle :: (limit: s32) -> s32 {
    value: s32 = 0
    while true {
        value += 1
        if value < limit { continue }
        break
    }
    return value
}
Scale :: (x: s32, k: float32) -> float32 {
    return cast(float32)x * k / 3.0 + 0.5
}

Hashes :: (out: []u32, first: s64) {
    #parallel_gpu for first..out.count-1 {
        out[it] = Hash(cast(u32)it)
    }
}
Signeds :: (out: []s32, input: []s32, offset: s32) {
    #parallel_gpu for 0..out.count-1 {
        out[it] = Signed(input[it], offset + cast(s32)it)
    }
}
Bytes :: (out: []u8, input: []u8, flags: []bool, add: u8, on: bool) {
    #parallel_gpu for 0..out.count-1 {
        wide: s32 = cast(s32)input[it] * 3 + cast(s32)add
        out[it] = cast(u8)wide
        flags[it] = on && input[it] > 100 || !on
    }
}
Sparse :: (out: []s32) {
    #parallel_gpu for 0..out.count-1 {
        if (cast(s32)it & 3) == 0 {
            out[it] += 11
        } else if (cast(s32)it & 3) == 1 {
            out[it] = Settle(5)
        }
    }
}
Floats :: (out: []float32, input: []s32, gain: float32) {
    #parallel_gpu for 0..out.count-1 {
        out[it] = Scale(input[it], gain)
    }
}
OnArray :: () -> s64 {
    local: [5000]s32
    #parallel_gpu for 0..4999 {
        local[it] = cast(s32)it * 2
    }
    total: s64 = 0
    for 0..4999 { total += cast(s64)local[it] }
    return total
}
// Stays on the CPU: a 64-bit local.
Wide :: (out: []s32) {
    #parallel_gpu for 0..out.count-1 {
        big: s64 = it * 3000000000
        out[it] = cast(s32)(big / 3000000000)
    }
}
// Stays on the CPU when it runs: signed division rounds by driver.
Divide :: (out: []s32, by: s32) {
    #parallel_gpu for 0..out.count-1 {
        out[it] = (cast(s32)it - 100) / by
    }
}
// More loop rounds than a shader's budget, so it returns to the CPU.
Long :: (out: []u32) {
    #parallel_gpu for 0..3 {
        count: u32 = 0
        while count < 3000000 { count += 1 }
        out[it] = count
    }
}
OutOfRange :: (out: []s32, input: []s32) {
    #parallel_gpu for 0..out.count-1 {
        at: s32 = cast(s32)it + 1
        out[it] = input[at]
    }
}

main :: (argc: s32, argv: **u8) -> s32 {
    hashes: [5000]u32
    signeds: [5000]s32
    input: [5000]s32
    bytes: [5000]u8
    bytes_in: [5000]u8
    flags: [5000]bool
    sparse: [5000]s32
    floats: [5000]float32
    for 0..4999 {
        input[it] = cast(s32)it * 37 - 90000
        bytes_in[it] = cast(u8)(it % 251)
        sparse[it] = cast(s32)it
    }
    Hashes(hashes[:], 7)
    Signeds(signeds[:], input[:], 19)
    Bytes(bytes[:], bytes_in[:], flags[:], 9, true)
    Sparse(sparse[:])
    Floats(floats[:], input[:], 0.25)
    Wide(signeds[:100])
    Divide(signeds[100:200], 3)
    long: [4]u32
    Long(long[:])
    sum: u64 = 0
    for 0..4999 {
        sum = sum * 31 + cast(u64)hashes[it]
        sum = sum * 31 + cast(u64)cast(u32)signeds[it]
        sum = sum * 31 + cast(u64)bytes[it]
        sum = sum * 31 + (ifx flags[it] then 1 else 0)
        sum = sum * 31 + cast(u64)cast(u32)sparse[it]
    }
    print("exact % % %\n", sum, OnArray(), long[3])
    near: s32 = 0
    for 0..4999 {
        expected: float32 = cast(float32)input[it] * 0.25 / 3.0 + 0.5
        difference: float32 = floats[it] - expected
        if difference < 0.0 { difference = -difference }
        if difference <= 0.01 { near += 1 }
    }
    print("near %\n", near)
    if argc > 1 { OutOfRange(signeds[:], input[:4000]) }
    print("finished\n")
    return 0
}
ZI

"$ziran" build --target=c --exe --entry kernels:main --root "$work" \
    --module-path "$repo/std" -o "$work/out" "$work/kernels.zi"
c=$work/out/kernels.c
rg -q 'include "ziran_gpu.h"' "$c"
rg -q 'stays on CPU threads: a local has type s64' "$c"
rg -q 'stays on CPU threads: operator / on this type' "$c"

ZIRAN_GPU=off "$work/out/kernels" > "$work/cpu.txt"
rg -q '^near 5000$' "$work/cpu.txt"
rg -q ' 3000000$' "$work/cpu.txt"

# Every device gives the CPU's integers, and floats within rounding.
ran=0
for device in auto 0 1 2 3 4 5; do
    if test "$device" = auto; then
        ZIRAN_GPU_LOG=1 "$work/out/kernels" > "$work/device.txt" 2> "$work/device.log"
    else
        ZIRAN_GPU=$device ZIRAN_GPU_LOG=1 "$work/out/kernels" > "$work/device.txt" 2> "$work/device.log"
    fi
    cmp "$work/cpu.txt" "$work/device.txt"
    if rg -q 'ran on the device' "$work/device.log"; then
        ran=1
        rg -q 'returned to the CPU' "$work/device.log"
    fi
done
test "$ran" = 1 || echo "gpu_offload: no device ran a region; checked the CPU path only"

# Too few iterations start no driver.
ZIRAN_GPU_MIN=1000000 ZIRAN_GPU_LOG=1 "$work/out/kernels" > "$work/small.txt" 2> "$work/small.log"
cmp "$work/cpu.txt" "$work/small.txt"
test ! -s "$work/small.log"

# A failed bounds check is reported by the CPU run, whichever device ran first.
ZIRAN_GPU=off "$work/out/kernels" fail > "$work/fail-cpu.txt" 2> "$work/fail-cpu.err" && status=0 || status=$?
test "$status" -ne 0
for device in auto 0 1 2; do
    if test "$device" = auto; then
        "$work/out/kernels" fail > "$work/fail.txt" 2> "$work/fail.err" && other=0 || other=$?
    else
        ZIRAN_GPU=$device "$work/out/kernels" fail > "$work/fail.txt" 2> "$work/fail.err" && other=0 || other=$?
    fi
    test "$other" = "$status"
    rg -q "slice index out of bounds" "$work/fail.err"
done

# Saved IR lowers to the same shaders.
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/kernels.zi"
"$ziran" build --target=c --entry kernels:main --root "$work/ir" \
    --module-path "$repo/std" -o "$work/saved" "$work/ir/kernels.zir"
"$ziran" build --target=c --entry kernels:main --root "$work" \
    --module-path "$repo/std" -o "$work/source" "$work/kernels.zi"
rg 'zir_fault|stays on CPU' "$work/source/kernels.c" > "$work/source.shaders"
rg 'zir_fault|stays on CPU' "$work/saved/kernels.c" > "$work/saved.shaders"
cmp "$work/source.shaders" "$work/saved.shaders"

# Generated C stays strict C11.
"$ziran" build --target=c --entry kernels:main --root "$work" \
    --module-path "$repo/std" -o "$work/strict" "$work/kernels.zi"
"${CC:-cc}" -std=c11 -pedantic-errors -pthread -I"$repo/include" -I"$work/strict" \
    -c "$work/strict/kernels.c" -o "$work/strict/kernels.o"
