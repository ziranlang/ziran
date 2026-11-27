/*
 * zir_host_native_arch.h - the machine-code half of native ZX loading.
 *
 * zir_host_native.c is target-neutral: it compiles and caches images, maps
 * and relocates them, and bridges host calls through NativeAbiFrame. One
 * file per architecture (zir_host_native_x86_64.c, zir_host_native_aarch64.c,
 * zir_host_native_armv7.c) writes the veneers generated code calls into C,
 * patches relocated fields and names the CPU for the code cache.
 */
#ifndef ZIR_HOST_NATIVE_ARCH_H
#define ZIR_HOST_NATIVE_ARCH_H

#include <stddef.h>
#include <stdint.h>

/* Argument registers of generated code: SysV x86-64 passes six integers,
 * AAPCS64 eight, and ARMv7 two 64-bit pairs (r0:r1, r2:r3); floats go in
 * eight registers, or d0-d5 on ARMv7. */
#if defined(__aarch64__)
#define NATIVE_ABI_GENERAL 8
#define NATIVE_ABI_FLOAT 8
#elif defined(__arm__)
#define NATIVE_ABI_GENERAL 2
#define NATIVE_ABI_FLOAT 6
#else
#define NATIVE_ABI_GENERAL 6
#define NATIVE_ABI_FLOAT 8
#endif

/* What a host veneer saves at a call, the input of the typed bridge. */
typedef struct NativeAbiFrame {
    uint64_t general[NATIVE_ABI_GENERAL];
    unsigned char floating[8][16];
    /* The caller's first stack argument. */
    const unsigned char *stack;
    /* A float result's bits, which the veneer returns in the float register. */
    uint64_t floating_result;
    /* rax on x86-64, x8 on AArch64 and r10 on ARMv7 at the call: the
     * caller's destination for a record or array result. */
    unsigned char *result_address;
} NativeAbiFrame;

/* The C entry points the veneers call, and the instance they pass. */
typedef struct NativeVeneerCalls {
    void *instance;
    /* uint64_t host(instance, int32_t function, NativeAbiFrame *frame) */
    uintptr_t host;
    /* _Noreturn void trap(instance, uint32_t code, uintptr_t return_address) */
    uintptr_t trap;
    /* uint64_t resize(data, old_count, new_count, element, instance) */
    uintptr_t resize;
} NativeVeneerCalls;

typedef struct NativeStub {
    unsigned char *bytes;
    size_t at, capacity;
    int failed;
} NativeStub;

static inline void
native_stub_byte(NativeStub *out, unsigned value)
{
    if(out->at >= out->capacity) {
        out->failed = 1;
        return;
    }
    out->bytes[out->at++] = (unsigned char)value;
}

static inline void
native_stub_u32(NativeStub *out, uint32_t value)
{
    for(int at = 0; at < 4; at++) native_stub_byte(out, value >> (8 * at));
}

static inline void
native_stub_u64(NativeStub *out, uint64_t value)
{
    for(int at = 0; at < 8; at++) native_stub_byte(out, value >> (8 * at));
}

/* Each writes one veneer at OUT and returns 0 when it does not fit. The host
 * veneer saves the argument registers into a NativeAbiFrame and calls
 * host(instance, FUNCTION, &frame); the trap veneer passes the code the
 * generated trap set and its return address to trap; the resize veneer
 * keeps every vector register across resize(..., instance). */
int ZirNativeHostVeneer(NativeStub *out, const NativeVeneerCalls *calls, int32_t function);
int ZirNativeTrapVeneer(NativeStub *out, const NativeVeneerCalls *calls);
int ZirNativeResizeVeneer(NativeStub *out, const NativeVeneerCalls *calls);

/* ARMv7 only, whose images call runtime helpers (MachineHelper) for 64-bit
 * divides and int64/float conversions: the veneer keeps every register but
 * lr and passes the 16 bytes at the caller's sp to helper HELPER. */
#if defined(__arm__)
int ZirNativeHelperVeneer(NativeStub *out, const NativeVeneerCalls *calls, int32_t helper);
#endif

/* Stores TARGET in the relocated field at FIELD as its WIDTH and ADDEND
 * describe it; 0 when the field is not the expected form or TARGET is out
 * of its reach. */
int ZirNativePatch(unsigned char *field, uintptr_t target, int32_t width, int64_t addend);

/* Usable TargetFeature bits, including the OS-enabled register state. */
uint64_t ZirNativeCpuFeatures(void);

#endif
