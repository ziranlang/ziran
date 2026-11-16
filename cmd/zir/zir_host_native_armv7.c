/*
 * zir_host_native_armv7.c - A32 veneers, the runtime helpers and MOVW/MOVT
 * and BL relocations for the native ZX loader (zir_host_native_arch.h).
 *
 * Generated code is A32 and reaches C only through these veneers, which
 * call with BLX, so Thumb callees and the softfp ABI never meet generated
 * code: every C entry point here takes and returns integers only.
 */
#include "zir_host_native_arch.h"

#if defined(__arm__) && defined(__linux__) && !defined(ZIRAN_NO_ENGINE)

#include <string.h>
#include <sys/auxv.h>

#include "compiler_machine_image.h"

enum {
    /* sp stays 8-byte aligned; 16 keeps the frame tidy. */
    HOST_FRAME = (sizeof(NativeAbiFrame) + 15) / 16 * 16,
    /* What a saving call pushes: r0-r3, r12, lr, then d0-d7. */
    SAVED_BYTES = 6 * 4 + 8 * 8,
    REG_SP = 13,
    REG_IP = 12
};
_Static_assert(HOST_FRAME < 256, "the host frame is one A32 immediate");

/* movw/movt r<rd>, VALUE. */
static void
move_word(NativeStub *out, uint32_t rd, uint32_t value)
{
    uint32_t low = value & 0xffff, high = value >> 16;
    native_stub_u32(out, 0xE3000000u | (low >> 12) << 16 | rd << 12 | (low & 0xfff));
    native_stub_u32(out, 0xE3400000u | (high >> 12) << 16 | rd << 12 | (high & 0xfff));
}

/* r12 = TARGET; blx r12 (0xE12FFF3C) or bx r12 (0xE12FFF1C). */
static void
call_through_ip(NativeStub *out, uint32_t opcode, uintptr_t target)
{
    move_word(out, REG_IP, (uint32_t)target);
    native_stub_u32(out, opcode);
}

/* vstr/vldr d<rd>, [sp, #offset]. */
static void
sp_double(NativeStub *out, int load, uint32_t rd, uint32_t offset)
{
    native_stub_u32(out, (load ? 0xED9D0B00u : 0xED8D0B00u) | rd << 12 | offset / 4);
}

/* str r<rt>, [sp, #offset]. */
static void
sp_word(NativeStub *out, uint32_t rt, uint32_t offset)
{
    native_stub_u32(out, 0xE58D0000u | rt << 12 | offset);
}

/* Saves r0-r1:r2-r3, d0-d5, the caller's stack argument base and r10 (the
 * record-result address) into a NativeAbiFrame, calls
 * host(instance, function, &frame) and returns its r0:r1, with d0 from the
 * frame's float result. */
int
ZirNativeHostVeneer(NativeStub *out, const NativeVeneerCalls *calls, int32_t function)
{
    /* push {r11, lr}; mov r11, sp; sub sp, sp, #HOST_FRAME; stm sp, {r0-r3}. */
    native_stub_u32(out, 0xE92D4800u);
    native_stub_u32(out, 0xE1A0B00Du);
    native_stub_u32(out, 0xE24DD000u | HOST_FRAME);
    native_stub_u32(out, 0xE88D000Fu);
    for(uint32_t reg = 0; reg < NATIVE_ABI_FLOAT; reg++)
        sp_double(out, 0, reg, (uint32_t)(offsetof(NativeAbiFrame, floating) + reg * 16));
    /* add r12, r11, #8: sp at the call, where stack arguments begin. */
    native_stub_u32(out, 0xE28BC008u);
    sp_word(out, REG_IP, offsetof(NativeAbiFrame, stack));
    sp_word(out, 10, offsetof(NativeAbiFrame, result_address));
    move_word(out, 0, (uint32_t)(uintptr_t)calls->instance);
    move_word(out, 1, (uint32_t)function);
    native_stub_u32(out, 0xE1A0200Du);
    call_through_ip(out, 0xE12FFF3Cu, calls->host);
    sp_double(out, 1, 0, offsetof(NativeAbiFrame, floating_result));
    /* mov sp, r11; pop {r11, pc}. */
    native_stub_u32(out, 0xE1A0D00Bu);
    native_stub_u32(out, 0xE8BD8800u);
    return !out->failed;
}

/* trap(instance, code, return address): the generated trap put the code in
 * r0 and branched here with BL from inside the trapping instruction. The
 * trap never returns, so sp is simply aligned for C. */
int
ZirNativeTrapVeneer(NativeStub *out, const NativeVeneerCalls *calls)
{
    /* mov r2, lr; mov r1, r0; bic sp, sp, #7. */
    native_stub_u32(out, 0xE1A0200Eu);
    native_stub_u32(out, 0xE1A01000u);
    native_stub_u32(out, 0xE3CDD007u);
    move_word(out, 0, (uint32_t)(uintptr_t)calls->instance);
    call_through_ip(out, 0xE12FFF1Cu, calls->trap);
    return !out->failed;
}

/* A call that keeps every register but lr, as an instruction would: it
 * saves the caller-saved r0-r3, r12 and d0-d7 (generated code uses d0-d15,
 * and C keeps d8-d15), then calls FUNCTION(area, first, second), where area
 * is the caller's sp. */
static int
saving_call(NativeStub *out, uintptr_t function, uint32_t first, uint32_t second)
{
    /* push {r0-r3, r12, lr}; vpush {d0-d7}; add r0, sp, #SAVED_BYTES. */
    native_stub_u32(out, 0xE92D500Fu);
    native_stub_u32(out, 0xED2D0B10u);
    native_stub_u32(out, 0xE28D0000u | SAVED_BYTES);
    move_word(out, 1, first);
    move_word(out, 2, second);
    call_through_ip(out, 0xE12FFF3Cu, function);
    /* vpop {d0-d7}; pop {r0-r3, r12, pc}. */
    native_stub_u32(out, 0xECBD0B10u);
    native_stub_u32(out, 0xE8BD900Fu);
    return !out->failed;
}

typedef uint64_t (*NativeResizeCall)(uint64_t data, uint64_t old_count, uint64_t new_count,
                                     uint64_t element, void *instance);

/* The resize form leaves the old block's address, both counts and the
 * element size as four 64-bit words at AREA and reads the new block's
 * address back from its first word. */
static void
armv7_resize(unsigned char *area, void *instance, uintptr_t resize)
{
    uint64_t words[4];
    memcpy(words, area, sizeof(words));
    NativeResizeCall call;
    memcpy(&call, &resize, sizeof(call));
    uint64_t block = call(words[0], words[1], words[2], words[3], instance);
    memcpy(area, &block, sizeof(block));
}

int
ZirNativeResizeVeneer(NativeStub *out, const NativeVeneerCalls *calls)
{
    return saving_call(out, (uintptr_t)(void *)&armv7_resize,
                       (uint32_t)(uintptr_t)calls->instance, (uint32_t)calls->resize);
}

/* The operations A32 has no instruction for. Operands are 64-bit words at
 * AREA (an f32 in the first four bytes); the result replaces the first.
 * The generated code has already trapped a zero divisor and floats outside
 * the target's range; a divisor of -1 wraps, as SDIV does. */
static void
armv7_helper(unsigned char *area, uint32_t helper)
{
    uint64_t a, b, out = 0;
    double d;
    float f;
    memcpy(&a, area, sizeof(a));
    memcpy(&b, area + 8, sizeof(b));
    memcpy(&d, area, sizeof(d));
    memcpy(&f, area, sizeof(f));
    switch((MachineHelper)helper) {
    case MachineHelper_DivideSigned:
        out = (int64_t)b == -1 ? 0 - a : (uint64_t)((int64_t)a / (int64_t)b);
        break;
    case MachineHelper_DivideUnsigned: out = a / b; break;
    case MachineHelper_RemainderSigned:
        out = (int64_t)b == -1 ? 0 : (uint64_t)((int64_t)a % (int64_t)b);
        break;
    case MachineHelper_RemainderUnsigned: out = a % b; break;
    case MachineHelper_SignedToF64: d = (double)(int64_t)a; memcpy(&out, &d, sizeof(d)); break;
    case MachineHelper_UnsignedToF64: d = (double)a; memcpy(&out, &d, sizeof(d)); break;
    case MachineHelper_SignedToF32: f = (float)(int64_t)a; memcpy(&out, &f, sizeof(f)); break;
    case MachineHelper_UnsignedToF32: f = (float)a; memcpy(&out, &f, sizeof(f)); break;
    case MachineHelper_F64ToSigned: out = (uint64_t)(int64_t)d; break;
    case MachineHelper_F64ToUnsigned: out = (uint64_t)d; break;
    case MachineHelper_F32ToSigned: out = (uint64_t)(int64_t)f; break;
    case MachineHelper_F32ToUnsigned: out = (uint64_t)f; break;
    }
    memcpy(area, &out, sizeof(out));
}

int
ZirNativeHelperVeneer(NativeStub *out, const NativeVeneerCalls *calls, int32_t helper)
{
    (void)calls;
    if(helper < 0 || helper >= MachineHelperCount) return 0;
    return saving_call(out, (uintptr_t)(void *)&armv7_helper, (uint32_t)helper, 0);
}

/* A width-2 relocation is MOVW (addend 0, the low half) or MOVT (addend 2,
 * the high half); a width-4 one is a BL, whose displacement is from the
 * BL plus 8 (the addend -8). */
int
ZirNativePatch(unsigned char *field, uintptr_t target, int32_t width, int64_t addend)
{
    uint32_t word;
    memcpy(&word, field, sizeof(word));
    if(width == 2) {
        uint32_t form = addend == 0 ? 0x03000000u : 0x03400000u;
        if((addend != 0 && addend != 2) || (word & 0x0FF00000u) != form) return 0;
        uint32_t half = (uint32_t)((uint64_t)target >> (8 * addend)) & 0xffff;
        word = (word & 0xFFF0F000u) | (half >> 12) << 16 | (half & 0xfff);
    } else if(width == 4) {
        int64_t delta = (int64_t)target - (int64_t)(uintptr_t)field + addend;
        if(addend != -8 || (word & 0x0F000000u) != 0x0B000000u || (delta & 3) != 0 ||
           delta < -(INT64_C(1) << 25) || delta >= (INT64_C(1) << 25))
            return 0;
        word = (word & 0xFF000000u) | ((uint32_t)(delta >> 2) & 0x00FFFFFFu);
    } else {
        return 0;
    }
    memcpy(field, &word, sizeof(word));
    return 1;
}

uint64_t
ZirNativeCpuKey(void)
{
    return (uint64_t)getauxval(AT_HWCAP) ^ ((uint64_t)getauxval(AT_HWCAP2) << 32);
}

#endif
