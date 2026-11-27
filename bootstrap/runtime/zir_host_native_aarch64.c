/*
 * zir_host_native_aarch64.c - AAPCS64 veneers and MOVZ/MOVK and BL
 * relocations for the native ZX loader (zir_host_native_arch.h).
 */
#include "ziran_bootstrap_runtime.h"

#if defined(__aarch64__) && defined(__linux__) && !defined(ZIRAN_NO_ENGINE)

#include <string.h>
#include <sys/auxv.h>

enum {
    /* sp stays 16-byte aligned: the NativeAbiFrame rounded up. */
    HOST_FRAME = (sizeof(NativeAbiFrame) + 15) / 16 * 16,
    /* q0-q31. */
    RESIZE_FRAME = 32 * 16
};

/* x<rd> = VALUE as MOVZ and three MOVKs. */
static void
move_wide(NativeStub *out, int rd, uint64_t value)
{
    native_stub_u32(out, 0xD2800000u | (uint32_t)(value & 0xffff) << 5 | (uint32_t)rd);
    for(uint32_t half = 1; half < 4; half++)
        native_stub_u32(out, 0xF2800000u | half << 21 |
                        (uint32_t)((value >> (16 * half)) & 0xffff) << 5 | (uint32_t)rd);
}

/* stp x29, x30, [sp, #-16]!; mov x29, sp; sub sp, sp, #FRAME. */
static void
enter(NativeStub *out, uint32_t frame)
{
    native_stub_u32(out, 0xA9BF7BFDu);
    native_stub_u32(out, 0x910003FDu);
    native_stub_u32(out, 0xD10003FFu | frame << 10);
}

/* mov sp, x29; ldp x29, x30, [sp], #16; ret. */
static void
leave(NativeStub *out)
{
    native_stub_u32(out, 0x910003BFu);
    native_stub_u32(out, 0xA8C17BFDu);
    native_stub_u32(out, 0xD65F03C0u);
}

/* stp/ldp x<rt>, x<rt+1>, [sp, #offset]. */
static void
pair_x(NativeStub *out, int load, uint32_t rt, uint32_t offset)
{
    uint32_t base = load ? 0xA9400000u : 0xA9000000u;
    native_stub_u32(out, base | (offset / 8) << 15 | (rt + 1) << 10 | 31u << 5 | rt);
}

/* stp/ldp q<rt>, q<rt+1>, [sp, #offset]. */
static void
pair_q(NativeStub *out, int load, uint32_t rt, uint32_t offset)
{
    uint32_t base = load ? 0xAD400000u : 0xAD000000u;
    native_stub_u32(out, base | (offset / 16) << 15 | (rt + 1) << 10 | 31u << 5 | rt);
}

/* str x<rt>, [sp, #offset] (0xF9000000) or ldr d<rt>, [sp, #offset] (0xFD400000). */
static void
sp_word(NativeStub *out, uint32_t opcode, uint32_t rt, uint32_t offset)
{
    native_stub_u32(out, opcode | (offset / 8) << 10 | 31u << 5 | rt);
}

/* mov x16, TARGET; blr x16 (0xD63F0200) or br x16 (0xD61F0200). */
static void
call_through_x16(NativeStub *out, uint32_t opcode, uintptr_t target)
{
    move_wide(out, 16, (uint64_t)target);
    native_stub_u32(out, opcode);
}

/* Saves x0-x7, q0-q7, x8 and the caller's stack argument base into a
 * NativeAbiFrame on its own stack, calls host(instance, function, &frame)
 * and returns its x0, with d0 from the frame's float result. */
int
ZirNativeHostVeneer(NativeStub *out, const NativeVeneerCalls *calls, int32_t function)
{
    enter(out, HOST_FRAME);
    for(uint32_t reg = 0; reg < NATIVE_ABI_GENERAL; reg += 2)
        pair_x(out, 0, reg, reg * 8);
    for(uint32_t reg = 0; reg < 8; reg += 2)
        pair_q(out, 0, reg, (uint32_t)(offsetof(NativeAbiFrame, floating) + reg * 16));
    /* add x9, x29, #16: sp at the call, where stack arguments begin. */
    native_stub_u32(out, 0x910043A9u);
    sp_word(out, 0xF9000000u, 9, offsetof(NativeAbiFrame, stack));
    sp_word(out, 0xF9000000u, 8, offsetof(NativeAbiFrame, result_address));
    move_wide(out, 0, (uint64_t)(uintptr_t)calls->instance);
    /* movz w1, #low; movk w1, #high, lsl #16; mov x2, sp. */
    uint32_t id = (uint32_t)function;
    native_stub_u32(out, 0x52800001u | (id & 0xffff) << 5);
    native_stub_u32(out, 0x72A00001u | (id >> 16) << 5);
    native_stub_u32(out, 0x910003E2u);
    call_through_x16(out, 0xD63F0200u, calls->host);
    sp_word(out, 0xFD400000u, 0, offsetof(NativeAbiFrame, floating_result));
    leave(out);
    return !out->failed;
}

/* trap(instance, code, return address): the generated trap put the code in
 * w0 and branched here with bl from inside the trapping instruction. */
int
ZirNativeTrapVeneer(NativeStub *out, const NativeVeneerCalls *calls)
{
    native_stub_u32(out, 0xAA1E03E2u);
    native_stub_u32(out, 0x2A0003E1u);
    move_wide(out, 0, (uint64_t)(uintptr_t)calls->instance);
    call_through_x16(out, 0xD61F0200u, calls->trap);
    return !out->failed;
}

/* Resize keeps every vector register: the machine form saves the general
 * registers it needs, while the C allocator may use any. It passes x0-x3 as
 * the form placed them and the instance in x4. */
int
ZirNativeResizeVeneer(NativeStub *out, const NativeVeneerCalls *calls)
{
    enter(out, RESIZE_FRAME);
    for(uint32_t reg = 0; reg < 32; reg += 2) pair_q(out, 0, reg, reg * 16);
    move_wide(out, 4, (uint64_t)(uintptr_t)calls->instance);
    call_through_x16(out, 0xD63F0200u, calls->resize);
    for(uint32_t reg = 0; reg < 32; reg += 2) pair_q(out, 1, reg, reg * 16);
    leave(out);
    return !out->failed;
}

/* A width-2 relocation is one 64-bit MOVZ/MOVK of four and takes the
 * halfword its addend names as 2 * half; a width-4 one is a BL, whose
 * addend is the shared -4 and whose displacement is from the BL itself. */
int
ZirNativePatch(unsigned char *field, uintptr_t target, int32_t width, int64_t addend)
{
    uint32_t word;
    memcpy(&word, field, sizeof(word));
    if(width == 2) {
        int64_t half = addend / 2;
        if(addend < 0 || addend % 2 != 0 || half > 3 ||
           (word & 0x9F800000u) != 0x92800000u || (word >> 29 & 3) < 2 ||
           (word >> 21 & 3) != (uint32_t)half)
            return 0;
        word = (word & ~(0xffffu << 5)) | (uint32_t)((target >> (16 * half)) & 0xffff) << 5;
    } else if(width == 4) {
        uintptr_t source = (uintptr_t)field;
        int64_t delta = target >= source ? (int64_t)(target - source) : -(int64_t)(source - target);
        if(addend != -4 || (word & 0xFC000000u) != 0x94000000u || (delta & 3) != 0 ||
           delta < -(INT64_C(1) << 27) || delta >= (INT64_C(1) << 27))
            return 0;
        word = 0x94000000u | ((uint32_t)(delta >> 2) & 0x03FFFFFFu);
    } else {
        return 0;
    }
    memcpy(field, &word, sizeof(word));
    return 1;
}

uint64_t
ZirNativeCpuFeatures(void)
{
    /* Linux/Android HWCAP bits report that the kernel supports the feature. */
    uint64_t cap = getauxval(AT_HWCAP);
    uint64_t simd = (cap >> 1) & 1u;
    return (((cap >> 3) & 1u) << 12) | /* AES */
           (((cap >> 7) & 1u) << 14) | /* CRC32 */
           (((cap >> 4) & 1u) << 15) | /* PMULL */
           (((cap >> 6) & 1u) << 16) | /* SHA2 */
           (((cap >> 8) & 1u) << 17) | /* ATOMICS */
           (simd << 18) | (simd << 19); /* ASIMD/NEON */
}

#endif
