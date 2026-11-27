/*
 * zir_host_native_x86_64.c - SysV x86-64 veneers and rel32 relocations for
 * the native ZX loader (zir_host_native_arch.h).
 */
#include "ziran_bootstrap_runtime.h"

#if defined(__x86_64__) && defined(__linux__) && !defined(ZIRAN_NO_ENGINE)

#include <cpuid.h>
#include <string.h>

enum {
    /* rsp is 8 below a 16-byte boundary at entry; this frame realigns it
     * for the call and holds the NativeAbiFrame. */
    HOST_FRAME = 200,
    /* The caller's first stack argument, above the return address. */
    HOST_STACK_ARGUMENTS = HOST_FRAME + 8,
    RESIZE_FRAME = 264
};

_Static_assert(sizeof(NativeAbiFrame) <= HOST_FRAME, "host veneer frame");

/* mov [rsp + offset], general register. */
static void
store_general(NativeStub *out, int reg, uint32_t offset)
{
    native_stub_byte(out, reg >= 8 ? 0x4c : 0x48);
    native_stub_byte(out, 0x89);
    native_stub_byte(out, 0x84 | ((reg & 7) << 3));
    native_stub_byte(out, 0x24);
    native_stub_u32(out, offset);
}

/* movdqu [rsp + offset], xmm (0x7f) or movdqu xmm, [rsp + offset] (0x6f). */
static void
move_vector(NativeStub *out, unsigned opcode, int reg, uint32_t offset)
{
    native_stub_byte(out, 0xf3);
    if(reg >= 8) native_stub_byte(out, 0x44);
    native_stub_byte(out, 0x0f);
    native_stub_byte(out, opcode);
    native_stub_byte(out, 0x84 | ((reg & 7) << 3));
    native_stub_byte(out, 0x24);
    native_stub_u32(out, offset);
}

/* sub rsp, frame (0xec) or add rsp, frame (0xc4). */
static void
adjust_stack(NativeStub *out, unsigned opcode, uint32_t frame)
{
    native_stub_byte(out, 0x48); native_stub_byte(out, 0x81); native_stub_byte(out, opcode);
    native_stub_u32(out, frame);
}

/* mov REX.W register, imm64; 0xb8 is rax and 0xbf rdi; R8 sets REX.B. */
static void
move_wide(NativeStub *out, unsigned rex, unsigned opcode, uint64_t value)
{
    native_stub_byte(out, rex); native_stub_byte(out, opcode);
    native_stub_u64(out, value);
}

/* One veneer per relocation keeps target code ignorant of C callback
 * addresses. All typed conversion and copy-back remains in the host call. */
int
ZirNativeHostVeneer(NativeStub *out, const NativeVeneerCalls *calls, int32_t function)
{
    adjust_stack(out, 0xec, HOST_FRAME);
    store_general(out, 0, offsetof(NativeAbiFrame, result_address));
    static const int general[NATIVE_ABI_GENERAL] = {7, 6, 2, 1, 8, 9};
    for(int at = 0; at < NATIVE_ABI_GENERAL; at++)
        store_general(out, general[at], (uint32_t)(at * 8));
    for(int at = 0; at < 8; at++)
        move_vector(out, 0x7f, at, (uint32_t)(offsetof(NativeAbiFrame, floating) + at * 16));
    /* lea rax, [rsp + arguments]; save it as the frame's stack base. */
    native_stub_byte(out, 0x48); native_stub_byte(out, 0x8d); native_stub_byte(out, 0x84);
    native_stub_byte(out, 0x24); native_stub_u32(out, HOST_STACK_ARGUMENTS);
    store_general(out, 0, offsetof(NativeAbiFrame, stack));
    /* host(instance, function, rsp). */
    move_wide(out, 0x48, 0xbf, (uint64_t)(uintptr_t)calls->instance);
    native_stub_byte(out, 0xbe); native_stub_u32(out, (uint32_t)function);
    native_stub_byte(out, 0x48); native_stub_byte(out, 0x8d);
    native_stub_byte(out, 0x14); native_stub_byte(out, 0x24);
    move_wide(out, 0x48, 0xb8, calls->host);
    native_stub_byte(out, 0xff); native_stub_byte(out, 0xd0);
    /* movq xmm0, [rsp + floating_result]: float results as their bits. */
    native_stub_byte(out, 0xf3); native_stub_byte(out, 0x0f); native_stub_byte(out, 0x7e);
    native_stub_byte(out, 0x84); native_stub_byte(out, 0x24);
    native_stub_u32(out, offsetof(NativeAbiFrame, floating_result));
    adjust_stack(out, 0xc4, HOST_FRAME);
    native_stub_byte(out, 0xc3);
    return !out->failed;
}

/* trap(instance, code, return address): the generated trap put the code in
 * rdi and called here from inside the trapping instruction. */
int
ZirNativeTrapVeneer(NativeStub *out, const NativeVeneerCalls *calls)
{
    native_stub_byte(out, 0x48); native_stub_byte(out, 0x8b);
    native_stub_byte(out, 0x14); native_stub_byte(out, 0x24);
    native_stub_byte(out, 0x48); native_stub_byte(out, 0x89); native_stub_byte(out, 0xfe);
    move_wide(out, 0x48, 0xbf, (uint64_t)(uintptr_t)calls->instance);
    move_wide(out, 0x48, 0xb8, calls->trap);
    native_stub_byte(out, 0xff); native_stub_byte(out, 0xe0);
    return !out->failed;
}

/* Resize keeps every vector register: the machine form clobbers only general
 * caller-saved registers, while the C allocator may use any. It passes
 * rdi, rsi, rdx and rcx as the form placed them and the instance in r8. */
int
ZirNativeResizeVeneer(NativeStub *out, const NativeVeneerCalls *calls)
{
    adjust_stack(out, 0xec, RESIZE_FRAME);
    for(int reg = 0; reg < 16; reg++) move_vector(out, 0x7f, reg, (uint32_t)(reg * 16));
    move_wide(out, 0x49, 0xb8, (uint64_t)(uintptr_t)calls->instance);
    move_wide(out, 0x48, 0xb8, calls->resize);
    native_stub_byte(out, 0xff); native_stub_byte(out, 0xd0);
    for(int reg = 0; reg < 16; reg++) move_vector(out, 0x6f, reg, (uint32_t)(reg * 16));
    adjust_stack(out, 0xc4, RESIZE_FRAME);
    native_stub_byte(out, 0xc3);
    return !out->failed;
}

/* Every x86-64 relocation is a rel32 measured from the field plus ADDEND. */
int
ZirNativePatch(unsigned char *field, uintptr_t target, int32_t width, int64_t addend)
{
    uintptr_t source = (uintptr_t)field;
    if(width != 4 || target > (uintptr_t)INT64_MAX || source > (uintptr_t)INT64_MAX ||
       addend < INT32_MIN || addend > INT32_MAX)
        return 0;
    int64_t displacement = target >= source ? (int64_t)(target - source) : -(int64_t)(source - target);
    if(displacement > INT32_MAX - addend || displacement < INT32_MIN - addend)
        return 0;
    int32_t narrowed = (int32_t)(displacement + addend);
    memcpy(field, &narrowed, sizeof(narrowed));
    return 1;
}

uint64_t
ZirNativeCpuFeatures(void)
{
    unsigned a, b, c, d;
    if(!__get_cpuid(1, &a, &b, &c, &d)) return 0;
    uint64_t mask = ((uint64_t)((c >> 23) & 1u) << 0) | /* POPCNT */
                    ((uint64_t)((d >> 26) & 1u) << 1) | /* SSE2 */
                    ((uint64_t)((c >> 9) & 1u) << 2) | /* SSSE3 */
                    ((uint64_t)((c >> 19) & 1u) << 3) | /* SSE4.1 */
                    ((uint64_t)((c >> 20) & 1u) << 4) | /* SSE4.2 */
                    ((uint64_t)((c >> 25) & 1u) << 12) | /* AES */
                    ((uint64_t)((c >> 1) & 1u) << 13); /* PCLMUL */
    unsigned state = 0;
    if((c & (7u << 26)) == (7u << 26)) {
        /* CPUID reports XSAVE, OSXSAVE and AVX before XGETBV is legal. */
        __asm__ volatile ("xgetbv" : "=a"(state), "=d"(d) : "c"(0));
    }
    int avx = (state & 0x06u) == 0x06u;
    if(avx) mask |= (UINT64_C(1) << 5) | ((uint64_t)((c >> 12) & 1u) << 11);
    if(__get_cpuid_count(7, 0, &a, &b, &c, &d)) {
        if(avx && (b & (1u << 5))) mask |= UINT64_C(1) << 6; /* AVX2 */
        if((state & 0xe6u) == 0xe6u && (b & (1u << 16)))
            mask |= UINT64_C(1) << 7; /* AVX512F: opmasks and full ZMM state */
        mask |= ((uint64_t)((b >> 3) & 1u) << 8) | /* BMI */
                ((uint64_t)((b >> 8) & 1u) << 9); /* BMI2 */
    }
    if(__get_cpuid(0x80000001u, &a, &b, &c, &d) && (c & (1u << 5)))
        mask |= UINT64_C(1) << 10; /* LZCNT */
    return mask;
}

#endif
