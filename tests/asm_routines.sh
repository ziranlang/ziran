#!/bin/sh
# Typed #asm routines: slice operands, the short form, clobber ranges, the
# register checks, per-feature dispatch, ZIRAN_SIMD_TIER and the
# ZIRAN_ASM_CHECK differential mode against each portable body.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

cat > "$work/app.zi" <<'ZI'
// dst[i] = saturate(dst[i] + src[i]) over blocks of eight lanes.
AddSaturate :: (dst: []s16, src: []s16, blocks: s64)
#asm("x86_64", #string END
    xorl %%eax, %%eax
1:  cmpq %[blocks], %%rax
    jge 2f
    movq %%rax, %%rcx
    shlq $4, %%rcx
    movdqu (%[dst],%%rcx), %%xmm0
    movdqu (%[src],%%rcx), %%xmm9
    paddsw %%xmm9, %%xmm0
    movdqu %%xmm0, (%[dst],%%rcx)
    incq %%rax
    jmp 1b
2:
END, "", "r;r;r", "rax;rcx;xmm0;xmm9;cc")
#asm("aarch64", #string END
    cbz %[blocks], 2f
    mov x9, %[dst]
    mov x10, %[src]
    mov x11, %[blocks]
1:  ld1 {v0.8h}, [x9]
    ld1 {v1.8h}, [x10], #16
    sqadd v0.8h, v0.8h, v1.8h
    st1 {v0.8h}, [x9], #16
    subs x11, x11, #1
    b.ne 1b
2:
END, "", "r;r;r", "x9..x11;v0..v1;cc")
#asm("arm+neon", #string END
    mov r2, %[dst]
    mov r3, %[src]
    movs r12, %[blocks]
    beq 2f
1:  vld1.16 {d0-d1}, [r2]
    vld1.16 {d2-d3}, [r3]!
    vqadd.s16 q0, q0, q1
    vst1.16 {d0-d1}, [r2]!
    subs r12, r12, #1
    bne 1b
2:
END, "", "r;r;r", "r2;r3;r12;q0..q1;cc") {
    for i: 0..blocks * 8 - 1 {
        sum := cast(s32)dst[i] + src[i]
        if sum > 32767 { sum = 32767 }
        if sum < -32768 { sum = -32768 }
        dst[i] = cast(s16)sum
    }
}

// Round2(v, 1) per lane: pmulhrsw by 16384 rounds exactly the same way.
Halve :: (values: []s16, blocks: s64) -> s64
#asm("x86_64+ssse3", #string END
    movl $0x40004000, %%eax
    movd %%eax, %%xmm2
    pshufd $0, %%xmm2, %%xmm2
    xorl %%ecx, %%ecx
1:  cmpq %[blocks], %%rcx
    jge 2f
    movq %%rcx, %%rdx
    shlq $4, %%rdx
    movdqu (%[values],%%rdx), %%xmm0
    pmulhrsw %%xmm2, %%xmm0
    movdqu %%xmm0, (%[values],%%rdx)
    incq %%rcx
    jmp 1b
2:  movq %[blocks], %[out]
END, "=r", "r;r", "rax;rcx;rdx;xmm0;xmm2;cc") {
    for i: 0..blocks * 8 - 1 { values[i] = cast(s16)((cast(s32)values[i] + 1) >> 1) }
    return blocks
}

// Deliberately wrong: wraps where the portable body saturates.
AddWrapping :: (dst: []s16, src: []s16) #asm("x86_64", #string END
    movdqu (%[dst]), %%xmm0
    movdqu (%[src]), %%xmm1
    paddw %%xmm1, %%xmm0
    movdqu %%xmm0, (%[dst])
END, "", "r;r", "xmm0..xmm1") {
    for i: 0..7 {
        sum := cast(s32)dst[i] + src[i]
        if sum > 32767 { sum = 32767 }
        if sum < -32768 { sum = -32768 }
        dst[i] = cast(s16)sum
    }
}

// Random lanes, a quarter of them at the edges of s16.
Fill :: (values: []s16, state: *u64) {
    for i: 0..values.count - 1 {
        x := state.*
        x = x ^ (x << 13)
        x = x ^ (x >> 7)
        x = x ^ (x << 17)
        state.* = x
        pick := x & 7
        if pick == 0 { values[i] = 32767 }
        else if pick == 1 { values[i] = -32768 }
        else { values[i] = cast(s16)(cast(s64)((x >> 20) & 65535) - 32768) }
    }
}

#program_export
Run :: (rounds: s64) -> s64 {
    a: [64]s16
    b: [64]s16
    state: u64 = 88172645463325252
    checksum: s64 = 0
    for round: 0..rounds - 1 {
        Fill(a[:], *state)
        Fill(b[:], *state)
        AddSaturate(a[:], b[:], 8)
        if Halve(a[:], 8) != 8 { return -1 }
        for i: 0..63 { checksum = (checksum * 31 + a[i]) & 1152921504606846975 }
    }
    return checksum
}

#program_export
Broken :: () -> s64 {
    a: [8]s16
    b: [8]s16
    a[0] = 32767
    b[0] = 1
    AddWrapping(a[:], b[:])
    return a[0]
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" build --target=c --root "$work" -o "$work/c" "$work/app.zi"
# Ranges expand into the registers they name; slices pass their data.
grep -Fq '"x9", "x10", "x11", "v0", "v1", "cc", "memory"' "$work/c/app.c"
grep -Fq '"xmm0", "xmm1", "memory"' "$work/c/app.c"
grep -Fq '((values).data)' "$work/c/app.c"

cat > "$work/host.c" <<'C'
#include "app.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    if(argc > 1 && !strcmp(argv[1], "broken")) { printf("%lld\n", (long long)Broken()); return 0; }
    printf("%lld\n", (long long)Run(2000));
    return 0;
}
C
compiler=${CC:-cc}
build() {
    "$compiler" -std=c11 -Wall -Werror "$@" -I"$work/c" -I"$repo/include" "$work/c/app.c" "$work/host.c" -o "$work/run"
}
build -O0 -DZIRAN_NO_ASM
expected=$("$work/run")
test "$("$work/run" broken)" = 32767
for optimization in -O0 -O3; do
    build "$optimization"
    test "$("$work/run")" = "$expected"
    test "$(ZIRAN_SIMD_TIER=1 "$work/run")" = "$expected"
    case $(uname -m) in x86_64) test "$("$work/run" broken)" = -32768 ;; esac
    # The check build runs each variant beside its portable body.
    build "$optimization" -DZIRAN_ASM_CHECK
    test "$("$work/run")" = "$expected"
    case $(uname -m) in
    x86_64)
        if "$work/run" broken > "$work/out" 2> "$work/error"; then
            echo 'ZIRAN_ASM_CHECK missed a variant that differs' >&2; exit 1
        fi
        grep -Fq '#asm check failed: AddWrapping variant "x86_64" differs from its portable body in dst' "$work/error"
        ;;
    esac
done

# NDK clang assembles the AArch64 and ARMv7 NEON variants, check builds too.
ndk=${ANDROID_NDK_HOME:-}
if test -n "$ndk"; then
    for triple in aarch64-linux-android24 armv7a-linux-androideabi24 x86_64-linux-android24; do
        for mode in "" -DZIRAN_ASM_CHECK; do
            "$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/$triple-clang" -std=c11 -O3 -Wall -Werror $mode \
                -I"$work/c" -I"$repo/include" -c "$work/c/app.c" -o "$work/$triple.o"
        done
    done
else
    echo "asm-routines: ANDROID_NDK_HOME is unset; skipping NDK assembly"
fi

reject() {
    expected=$1
    cat > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/error" 2>&1; then
        echo "invalid assembly was accepted: $expected" >&2; exit 1
    fi
    grep -Fq "$expected" "$work/error" || { cat "$work/error"; exit 1; }
}
reject '#asm template uses %rbx, which its clobbers and constraints do not declare' <<'ZI'
F :: (v: s64) -> s64 #asm("x86_64", "movq %%rbx, %[out]", "=r", "r") { return v }
ZI
reject '#asm cannot clobber rbp, the stack, frame or platform register' <<'ZI'
F :: (v: s64) -> s64 #asm("x86_64", "movq %[v], %[out]", "=r", "r", "rbp") { return v }
ZI
reject '#asm clobbers rax, which operand out already holds' <<'ZI'
F :: (v: s64) -> s64 #asm("x86_64", "cpuid", "=a", "a", "rax;rbx;rcx;rdx") { return v }
ZI
reject '#asm template names unknown operand %[nope]' <<'ZI'
F :: (v: s64) -> s64 #asm("x86_64", "movq %[nope], %[out]", "=r", "r") { return v }
ZI
reject '#asm template operand %3 exceeds its 2 operands' <<'ZI'
F :: (v: s64) -> s64 #asm("x86_64", "movq %3, %0", "=r", "r") { return v }
ZI
reject "unknown #asm clobber 'bogus' for x86_64" <<'ZI'
F :: (v: s64) -> s64 #asm("x86_64", "", "=r", "r", "bogus") { return v }
ZI
reject "invalid #asm clobber range 'rcx..rdx'" <<'ZI'
F :: (v: s64) -> s64 #asm("x86_64", "", "=r", "r", "rcx..rdx") { return v }
ZI
reject '#asm template uses v4, which its clobbers do not declare' <<'ZI'
F :: (v: *s16) #asm("aarch64", "ld1 {v0.8h-v4.8h}, [%[v]]", "", "r", "v0..v3") { unused v }
ZI
reject '#asm cannot clobber x18, the stack, frame or platform register' <<'ZI'
F :: (v: *s16) #asm("aarch64", "mov x18, #1", "", "r", "x18") { unused v }
ZI
reject '#asm template uses d4, which its clobbers do not declare' <<'ZI'
F :: (v: *s16) #asm("arm+neon", "vld1.16 {d0-d4}, [%[v]]", "", "r", "q0..q1") { unused v }
ZI
reject '#asm cannot clobber r7, the stack, frame or platform register' <<'ZI'
F :: (v: *s16) #asm("arm", "mov r7, #1", "", "r", "r7") { unused v }
ZI
echo "Typed #asm routines matched their portable bodies; the register checks rejected bad variants"
