/*
 * ziran_simd.h - Ziran's fixed-width SIMD vectors in generated C.
 *
 * Every operation has one exact definition: zi_ref below computes it lane by
 * lane on bit patterns, and the portable VM runs that same code. GCC and
 * Clang builds use native vectors with SSE2..AVX2, NEON or WebAssembly
 * SIMD128 instructions that give identical bits; other compilers, such as
 * TinyCC, and ZIRAN_SIMD_SCALAR builds keep lane arrays and run plain C over
 * each type's own lanes, TinyCC on x86-64 SSE2 assembly for the common
 * integer helpers.
 *
 * Lanes are numbered from the lowest address. Viewing a vector as bytes
 * (Reinterpret, byte shifts) uses little-endian lanes on every host.
 * Operations that move data between lanes work on each 128-bit block of a
 * 256-bit vector on its own, as AVX2 does; the others use every lane.
 *
 * Including this file again with ZIRAN_SIMD_TIERS defined also defines the
 * x86-64 tiers' operations, named zi_ssse3_..., zi_sse41_... and zi_avx2_...,
 * for #target procedure clones, and zi_simd_tier(), which picks one of them
 * for this CPU once (ZIRAN_SIMD_TIER=1..4 in the environment lowers it).
 */
#ifndef ZIRAN_SIMD_COMMON_H
#define ZIRAN_SIMD_COMMON_H

#include <stdint.h>
#include <string.h>

/* ZI_VEC and the vector types. */
#include "ziran_simd_types.h"

#if ZI_VEC && (defined(__x86_64__) || (defined(__i386__) && defined(__SSE2__)))
#define ZI_X86 1
#include <immintrin.h>
#else
#define ZI_X86 0
#endif
#if ZI_VEC && !ZI_X86 && defined(__ARM_NEON) && \
    (defined(__aarch64__) || (defined(__ARM_FP) && !defined(__SOFTFP__)))
#define ZI_NEON 1
#if defined(__aarch64__)
#define ZI_A64 1
#else
#define ZI_A64 0
#endif
#include <arm_neon.h>
#else
#define ZI_NEON 0
#define ZI_A64 0
#endif
#if ZI_VEC && !ZI_X86 && !ZI_NEON && defined(__wasm_simd128__)
#define ZI_WASM 1
#include <wasm_simd128.h>
#else
#define ZI_WASM 0
#endif

#if ZI_VEC && defined(__GNUC__)
/* 256-bit vectors cross non-AVX procedure boundaries in memory; checking
 * keeps #target procedures, whose AVX2 clones would pass them in YMM
 * registers, from passing them by value at all. GCC's one note per file
 * about its 4.6 ABI change ignores this pragma; `ziran build` passes
 * -Wno-psabi for it. */
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wpsabi"
#else
#pragma GCC diagnostic ignored "-Wpsabi"
#endif
#endif

/* ---- exact lane semantics ------------------------------------------ */

enum {
    ZI_K_U8, ZI_K_S8, ZI_K_U16, ZI_K_S16, ZI_K_U32, ZI_K_S32,
    ZI_K_U64, ZI_K_S64, ZI_K_F32, ZI_K_F64
};

/* Operation numbers shared with the compiler (cmd/zir/zir_simd.c). */
enum {
    ZI_OP_ADD = 1, ZI_OP_SUB, ZI_OP_MUL, ZI_OP_DIV, ZI_OP_ADD_SAT, ZI_OP_SUB_SAT,
    ZI_OP_MIN, ZI_OP_MAX, ZI_OP_AVG, ZI_OP_MUL_HIGH, ZI_OP_MUL_HIGH_ROUND,
    ZI_OP_AND, ZI_OP_OR, ZI_OP_XOR, ZI_OP_AND_NOT, ZI_OP_NOT, ZI_OP_ABS,
    ZI_OP_NEG, ZI_OP_EQ, ZI_OP_LT, ZI_OP_GT, ZI_OP_SELECT, ZI_OP_SHL,
    ZI_OP_SHR, ZI_OP_SHR_ROUND, ZI_OP_SHL_LANES, ZI_OP_SHR_LANES,
    ZI_OP_SHL_BYTES, ZI_OP_SHR_BYTES, ZI_OP_ALIGN_BYTES, ZI_OP_MAKE,
    ZI_OP_SPLAT, ZI_OP_ZERO, ZI_OP_LANE, ZI_OP_WITH_LANE, ZI_OP_LOAD,
    ZI_OP_LOAD_LOW, ZI_OP_STORE, ZI_OP_STORE_LOW, ZI_OP_GATHER,
    ZI_OP_WIDEN_LOW, ZI_OP_WIDEN_HIGH, ZI_OP_WIDEN, ZI_OP_NARROW,
    ZI_OP_MUL_WIDE_LOW, ZI_OP_MUL_WIDE_HIGH, ZI_OP_MADD, ZI_OP_MADD_SAT,
    ZI_OP_ADD_PAIRS, ZI_OP_SAD, ZI_OP_REDUCE_ADD, ZI_OP_REDUCE_MIN,
    ZI_OP_REDUCE_MAX, ZI_OP_REDUCE_MAX_INDEX, ZI_OP_ALL_ZERO,
    ZI_OP_BYTE_SHUFFLE, ZI_OP_SHUFFLE, ZI_OP_REINTERPRET, ZI_OP_CONVERT,
    ZI_OP_LOW, ZI_OP_HIGH, ZI_OP_COMBINE, ZI_OP_INTERLEAVE_LOW,
    ZI_OP_INTERLEAVE_HIGH, ZI_OP_SIGN, ZI_OP_ABS_DIFF, ZI_OP_COUNT
};

static inline int zi_kbits(int kind) { return 8 << (kind == ZI_K_F32 ? 2 : kind == ZI_K_F64 ? 3 : kind / 2); }
static inline int zi_ksigned(int kind) { return kind <= ZI_K_S64 && (kind & 1); }
static inline int zi_kfloat(int kind) { return kind >= ZI_K_F32; }
static inline uint64_t zi_kmask(int kind)
{
    int bits = zi_kbits(kind);
    return bits == 64 ? ~(uint64_t)0 : (((uint64_t)1 << bits) - 1);
}
static inline int64_t zi_sx(uint64_t bits, int kind)
{
    int width = zi_kbits(kind);
    if(width == 64) return (int64_t)bits;
    uint64_t sign = (uint64_t)1 << (width - 1);
    bits &= zi_kmask(kind);
    return (bits & sign) ? (int64_t)(bits | ~zi_kmask(kind)) : (int64_t)bits;
}
static inline double zi_fget(uint64_t bits, int kind)
{
    if(kind == ZI_K_F32) { uint32_t w = (uint32_t)bits; float f; memcpy(&f, &w, 4); return f; }
    { double d; memcpy(&d, &bits, 8); return d; }
}
static inline uint64_t zi_fput(double value, int kind)
{
    if(kind == ZI_K_F32) { float f = (float)value; uint32_t w; memcpy(&w, &f, 4); return w; }
    { uint64_t w; memcpy(&w, &value, 8); return w; }
}
/* Unsigned comparison of lanes as numbers of `kind`; -1, 0 or 1. */
static inline int zi_kcmp(uint64_t a, uint64_t b, int kind)
{
    if(zi_kfloat(kind)) {
        double x = zi_fget(a, kind), y = zi_fget(b, kind);
        return x < y ? -1 : x > y ? 1 : 0;
    }
    if(zi_ksigned(kind)) {
        int64_t x = zi_sx(a, kind), y = zi_sx(b, kind);
        return x < y ? -1 : x > y;
    }
    return a < b ? -1 : a > b;
}
/* A shift count is the operand's value read as unsigned: a negative count
 * is larger than every lane width. */
static inline uint64_t zi_shl1(uint64_t v, uint64_t n, int kind)
{
    return n >= (uint64_t)zi_kbits(kind) ? 0 : (v << n) & zi_kmask(kind);
}
static inline uint64_t zi_shr1(uint64_t v, uint64_t n, int kind)
{
    int width = zi_kbits(kind);
    v &= zi_kmask(kind);
    if(zi_ksigned(kind)) {
        int64_t s = zi_sx(v, kind);
        if(n >= (uint64_t)width) return s < 0 ? zi_kmask(kind) : 0;
        return (uint64_t)(s >> n) & zi_kmask(kind);
    }
    return n >= (uint64_t)width ? 0 : v >> n;
}
static inline uint64_t zi_clamp(int64_t value, int kind)
{
    if(zi_ksigned(kind)) {
        int64_t high = (int64_t)(zi_kmask(kind) >> 1), low = -high - 1;
        return (uint64_t)(value < low ? low : value > high ? high : value) & zi_kmask(kind);
    }
    return value < 0 ? 0 : (uint64_t)value > zi_kmask(kind) ? zi_kmask(kind) : (uint64_t)value;
}
/* Clamp an unsigned 64-bit source lane. */
static inline uint64_t zi_clampu(uint64_t value, int kind)
{
    uint64_t high = zi_ksigned(kind) ? zi_kmask(kind) >> 1 : zi_kmask(kind);
    return value > high ? high : value;
}
static inline int64_t zi_value(uint64_t bits, int kind)
{
    return zi_ksigned(kind) ? zi_sx(bits, kind) : (int64_t)(bits & zi_kmask(kind));
}
/* Lanes as little-endian bytes; at most 32 bytes. */
static inline void zi_bytes(const uint64_t *lanes, int kind, int count, unsigned char *out)
{
    int size = zi_kbits(kind) / 8;
    for(int i = 0; i < count; i++)
        for(int j = 0; j < size; j++)
            out[i * size + j] = (unsigned char)(lanes[i] >> (8 * j));
}
static inline void zi_unbytes(const unsigned char *bytes, int kind, int count, uint64_t *out)
{
    int size = zi_kbits(kind) / 8;
    for(int i = 0; i < count; i++) {
        uint64_t value = 0;
        for(int j = size - 1; j >= 0; j--) value = (value << 8) | bytes[i * size + j];
        out[i] = value;
    }
}
static inline uint64_t zi_float_to_int(uint64_t bits, int from, int to)
{
    double value = zi_fget(bits, from);
    if(value != value) return 0;
    if(zi_ksigned(to)) {
        double limit = to == ZI_K_S64 ? 9223372036854775808.0 : 2147483648.0;
        if(value >= limit) return zi_kmask(to) >> 1;
        if(value <= -limit) return ((zi_kmask(to) >> 1) + 1) & zi_kmask(to);
        return (uint64_t)(int64_t)value & zi_kmask(to);
    }
    double limit = to == ZI_K_U64 ? 18446744073709551616.0 : 4294967296.0;
    if(value <= -1.0) return 0;
    if(value >= limit) return zi_kmask(to);
    return value <= 0.0 ? 0 : (uint64_t)value;
}
static inline uint64_t zi_int_to_float(uint64_t bits, int from, int to)
{
    if(zi_ksigned(from)) {
        int64_t value = zi_sx(bits, from);
        return to == ZI_K_F32 ? zi_fput((float)value, to) : zi_fput((double)value, to);
    }
    uint64_t value = bits & zi_kmask(from);
    return to == ZI_K_F32 ? zi_fput((float)value, to) : zi_fput((double)value, to);
}

/*
 * Compute one operation. r/rn: result lane kind and count; a/an: the first
 * vector operand's. Vector operands and the result hold lane bit patterns
 * zero-extended to 64 bits. k holds the constant operands: a lane index,
 * byte count or shuffle indices; k[0] is also a scalar shift count. A scalar
 * lane operand (Splat, WithLane) is b[0]; Make reads its lanes from a.
 * Scalar results are out[0]. Only ARMv7 NEON's float arithmetic calls it
 * from a helper; elsewhere it is the oracle the tests check helpers against.
 */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static void
zi_ref(int op, int r, int rn, int ak, int an, const uint64_t *a,
       const uint64_t *b, const uint64_t *c, const int64_t *k, uint64_t *out)
{
    uint64_t mask = zi_kmask(ak);
    int bytes = an * zi_kbits(ak) / 8;
    int blocks = bytes > 16 ? 2 : 1;
    int per = an / blocks; /* input lanes per 128-bit block */
    int rper = rn / blocks;
    int i, j, block;
    switch(op) {
    case ZI_OP_ADD: case ZI_OP_SUB: case ZI_OP_MUL: case ZI_OP_DIV:
        for(i = 0; i < an; i++) {
            if(zi_kfloat(ak)) {
                double x = zi_fget(a[i], ak), y = zi_fget(b[i], ak), z;
                if(ak == ZI_K_F32) {
                    float fx = (float)x, fy = (float)y;
                    z = op == ZI_OP_ADD ? (float)(fx + fy) : op == ZI_OP_SUB ? (float)(fx - fy) :
                        op == ZI_OP_MUL ? (float)(fx * fy) : (float)(fx / fy);
                } else
                    z = op == ZI_OP_ADD ? x + y : op == ZI_OP_SUB ? x - y :
                        op == ZI_OP_MUL ? x * y : x / y;
                out[i] = zi_fput(z, ak);
            } else
                out[i] = (op == ZI_OP_ADD ? a[i] + b[i] : op == ZI_OP_SUB ? a[i] - b[i] :
                          a[i] * b[i]) & mask;
        }
        return;
    case ZI_OP_ADD_SAT: case ZI_OP_SUB_SAT:
        for(i = 0; i < an; i++) {
            int64_t x = zi_value(a[i], ak), y = zi_value(b[i], ak);
            out[i] = zi_clamp(op == ZI_OP_ADD_SAT ? x + y : x - y, ak);
        }
        return;
    case ZI_OP_MIN: case ZI_OP_MAX:
        /* Min is a < b ? a : b and Max a > b ? a : b, so floats take b
         * when either is NaN or both are zeros. */
        for(i = 0; i < an; i++)
            out[i] = zi_kcmp(a[i], b[i], ak) == (op == ZI_OP_MIN ? -1 : 1) ? a[i] : b[i];
        return;
    case ZI_OP_AVG:
        for(i = 0; i < an; i++) out[i] = ((a[i] & mask) + (b[i] & mask) + 1) >> 1;
        return;
    case ZI_OP_MUL_HIGH:
        for(i = 0; i < an; i++)
            out[i] = (uint64_t)((zi_value(a[i], ak) * zi_value(b[i], ak)) >> 16) & mask;
        return;
    case ZI_OP_MUL_HIGH_ROUND:
        for(i = 0; i < an; i++)
            out[i] = (uint64_t)((zi_sx(a[i], ak) * zi_sx(b[i], ak) + 0x4000) >> 15) & mask;
        return;
    case ZI_OP_AND: for(i = 0; i < an; i++) out[i] = a[i] & b[i]; return;
    case ZI_OP_OR: for(i = 0; i < an; i++) out[i] = a[i] | b[i]; return;
    case ZI_OP_XOR: for(i = 0; i < an; i++) out[i] = a[i] ^ b[i]; return;
    case ZI_OP_AND_NOT: for(i = 0; i < an; i++) out[i] = a[i] & ~b[i] & mask; return;
    case ZI_OP_NOT: for(i = 0; i < an; i++) out[i] = ~a[i] & mask; return;
    case ZI_OP_ABS:
        for(i = 0; i < an; i++) {
            if(zi_kfloat(ak)) out[i] = a[i] & (mask >> 1);
            else { int64_t x = zi_sx(a[i], ak); out[i] = (x < 0 ? 0 - (uint64_t)x : (uint64_t)x) & mask; }
        }
        return;
    case ZI_OP_NEG:
        for(i = 0; i < an; i++)
            out[i] = zi_kfloat(ak) ? a[i] ^ ((mask >> 1) + 1) : (0 - a[i]) & mask;
        return;
    case ZI_OP_SIGN: /* a negated where b < 0, zero where b == 0 */
        for(i = 0; i < an; i++) {
            int64_t s = zi_sx(b[i], ak);
            out[i] = s < 0 ? (0 - a[i]) & mask : s == 0 ? 0 : a[i] & mask;
        }
        return;
    case ZI_OP_ABS_DIFF: /* |a - b| in the unsigned lanes of the same width */
        for(i = 0; i < an; i++)
            out[i] = (zi_kcmp(a[i], b[i], ak) < 0 ? b[i] - a[i] : a[i] - b[i]) & mask;
        return;
    case ZI_OP_EQ: case ZI_OP_LT: case ZI_OP_GT:
        for(i = 0; i < an; i++) {
            int yes;
            if(zi_kfloat(ak)) {
                double x = zi_fget(a[i], ak), y = zi_fget(b[i], ak);
                yes = op == ZI_OP_EQ ? x == y : op == ZI_OP_LT ? x < y : x > y;
            } else {
                int order = zi_kcmp(a[i], b[i], ak);
                yes = op == ZI_OP_EQ ? (a[i] & mask) == (b[i] & mask) : order == (op == ZI_OP_LT ? -1 : 1);
            }
            out[i] = yes ? zi_kmask(r) : 0;
        }
        return;
    case ZI_OP_SELECT: /* a is the mask */
        for(i = 0; i < rn; i++) out[i] = ((a[i] & b[i]) | (~a[i] & c[i])) & zi_kmask(r);
        return;
    case ZI_OP_SHL: for(i = 0; i < an; i++) out[i] = zi_shl1(a[i], (uint64_t)k[0], ak); return;
    case ZI_OP_SHR: for(i = 0; i < an; i++) out[i] = zi_shr1(a[i], (uint64_t)k[0], ak); return;
    case ZI_OP_SHR_ROUND:
        /* (v + 2^(n-1)) >> n without an intermediate overflow. */
        for(i = 0; i < an; i++) {
            uint64_t n = (uint64_t)k[0];
            out[i] = n == 0 ? a[i] & mask :
                (zi_shr1(a[i], n, ak) + (zi_shr1(a[i], n - 1, ak) & 1)) & mask;
        }
        return;
    case ZI_OP_SHL_LANES: for(i = 0; i < an; i++) out[i] = zi_shl1(a[i], b[i] & mask, ak); return;
    case ZI_OP_SHR_LANES: for(i = 0; i < an; i++) out[i] = zi_shr1(a[i], b[i] & mask, ak); return;
    case ZI_OP_SHL_BYTES: case ZI_OP_SHR_BYTES: case ZI_OP_ALIGN_BYTES: {
        /* AlignBytes(high, low, n) shifts the 32 bytes low:high right by
         * n, as x86 alignr does. */
        unsigned char x[32], y[32], z[32];
        zi_bytes(op == ZI_OP_ALIGN_BYTES ? b : a, ak, an, x);
        if(op == ZI_OP_ALIGN_BYTES) zi_bytes(a, ak, an, y);
        /* One flat loop: NDK Clang 19 for ARMv7 drops the second block of
         * the nested form when op and an are constants. */
        for(j = 0; j < bytes; j++) {
            int base = j & ~15;
            int64_t from = (j & 15) + (op == ZI_OP_SHL_BYTES ? -k[0] : k[0]);
            unsigned char byte = 0;
            if(from >= 0 && from < 16) byte = x[base + from];
            else if(op == ZI_OP_ALIGN_BYTES && from >= 16 && from < 32)
                byte = y[base + from - 16];
            z[j] = byte;
        }
        zi_unbytes(z, ak, an, out);
        return;
    }
    case ZI_OP_MAKE: for(i = 0; i < rn; i++) out[i] = a[i] & zi_kmask(r); return;
    case ZI_OP_SPLAT: for(i = 0; i < rn; i++) out[i] = b[0] & zi_kmask(r); return;
    case ZI_OP_ZERO: for(i = 0; i < rn; i++) out[i] = 0; return;
    case ZI_OP_LANE: out[0] = a[k[0]] & mask; return;
    case ZI_OP_WITH_LANE:
        for(i = 0; i < an; i++) out[i] = i == k[0] ? b[0] & mask : a[i];
        return;
    case ZI_OP_WIDEN_LOW: case ZI_OP_WIDEN_HIGH:
        for(block = 0; block < blocks; block++)
            for(i = 0; i < rper; i++)
                out[block * rper + i] = (uint64_t)zi_value(
                    a[block * per + i + (op == ZI_OP_WIDEN_HIGH ? per / 2 : 0)], ak) & zi_kmask(r);
        return;
    case ZI_OP_WIDEN:
        for(i = 0; i < rn; i++) out[i] = (uint64_t)zi_value(a[i], ak) & zi_kmask(r);
        return;
    case ZI_OP_NARROW:
        for(block = 0; block < blocks; block++)
            for(i = 0; i < per; i++) {
                const uint64_t *source[2] = {a, b};
                for(j = 0; j < 2; j++) {
                    uint64_t lane = source[j][block * per + i];
                    out[block * rper + j * per + i] = zi_ksigned(ak) || zi_kbits(ak) < 64 ?
                        zi_clamp(zi_value(lane, ak), r) : zi_clampu(lane, r);
                }
            }
        return;
    case ZI_OP_MUL_WIDE_LOW: case ZI_OP_MUL_WIDE_HIGH:
        for(block = 0; block < blocks; block++)
            for(i = 0; i < rper; i++) {
                int at = block * per + i + (op == ZI_OP_MUL_WIDE_HIGH ? per / 2 : 0);
                if(zi_ksigned(ak))
                    out[block * rper + i] = (uint64_t)(zi_sx(a[at], ak) * zi_sx(b[at], ak)) & zi_kmask(r);
                else
                    out[block * rper + i] = ((a[at] & mask) * (b[at] & mask)) & zi_kmask(r);
            }
        return;
    case ZI_OP_MADD:
        for(i = 0; i < rn; i++)
            out[i] = (uint64_t)(zi_sx(a[2 * i], ak) * zi_sx(b[2 * i], ak) +
                                zi_sx(a[2 * i + 1], ak) * zi_sx(b[2 * i + 1], ak)) & zi_kmask(r);
        return;
    case ZI_OP_MADD_SAT: /* unsigned a, signed b */
        for(i = 0; i < rn; i++)
            out[i] = zi_clamp((int64_t)(a[2 * i] & 255) * zi_sx(b[2 * i], ZI_K_S8) +
                              (int64_t)(a[2 * i + 1] & 255) * zi_sx(b[2 * i + 1], ZI_K_S8), r);
        return;
    case ZI_OP_ADD_PAIRS:
        for(i = 0; i < rn; i++)
            out[i] = (uint64_t)(zi_value(a[2 * i], ak) + zi_value(a[2 * i + 1], ak)) & zi_kmask(r);
        return;
    case ZI_OP_SAD:
        for(i = 0; i < rn; i++) {
            uint64_t sum = 0;
            for(j = 0; j < 8; j++) {
                uint64_t x = a[8 * i + j] & 255, y = b[8 * i + j] & 255;
                sum += x > y ? x - y : y - x;
            }
            out[i] = sum;
        }
        return;
    case ZI_OP_REDUCE_ADD: {
        uint64_t sum = 0;
        for(i = 0; i < an; i++) sum += a[i];
        out[0] = sum & mask;
        return;
    }
    case ZI_OP_REDUCE_MIN: case ZI_OP_REDUCE_MAX: case ZI_OP_REDUCE_MAX_INDEX: {
        int best = 0;
        for(i = 1; i < an; i++)
            if(zi_kcmp(a[i], a[best], ak) == (op == ZI_OP_REDUCE_MIN ? -1 : 1)) best = i;
        out[0] = op == ZI_OP_REDUCE_MAX_INDEX ? (uint64_t)best : a[best] & mask;
        return;
    }
    case ZI_OP_ALL_ZERO: {
        uint64_t any = 0;
        for(i = 0; i < an; i++) any |= a[i] & mask;
        out[0] = any == 0;
        return;
    }
    case ZI_OP_BYTE_SHUFFLE:
        for(block = 0; block < blocks; block++)
            for(i = 0; i < 16; i++) {
                uint64_t index = b[block * 16 + i] & 255;
                out[block * 16 + i] = index < 16 ? a[block * 16 + index] & 255 : 0;
            }
        return;
    case ZI_OP_SHUFFLE:
        for(block = 0; block < blocks; block++)
            for(i = 0; i < per; i++) out[block * per + i] = a[block * per + k[i]];
        return;
    case ZI_OP_REINTERPRET: {
        unsigned char x[32];
        zi_bytes(a, ak, an, x);
        zi_unbytes(x, r, rn, out);
        return;
    }
    case ZI_OP_CONVERT:
        for(i = 0; i < an; i++)
            out[i] = zi_kfloat(ak) ? zi_float_to_int(a[i], ak, r) : zi_int_to_float(a[i], ak, r);
        return;
    case ZI_OP_LOW: case ZI_OP_HIGH:
        for(i = 0; i < rn; i++) out[i] = a[i + (op == ZI_OP_HIGH ? rn : 0)];
        return;
    case ZI_OP_COMBINE:
        for(i = 0; i < an; i++) { out[i] = a[i]; out[an + i] = b[i]; }
        return;
    case ZI_OP_INTERLEAVE_LOW: case ZI_OP_INTERLEAVE_HIGH:
        for(block = 0; block < blocks; block++)
            for(i = 0; i < per / 2; i++) {
                int at = block * per + i + (op == ZI_OP_INTERLEAVE_HIGH ? per / 2 : 0);
                out[block * per + 2 * i] = a[at];
                out[block * per + 2 * i + 1] = b[at];
            }
        return;
    default:
        return;
    }
}

/* ---- typed operations ----------------------------------------------- */
#ifndef ZIRAN_SIMD_REF_ONLY
#if ZI_VEC
#define ZI_INLINE static inline __attribute__((always_inline))
#else
#define ZI_INLINE static inline
#endif
#if ZI_X86 && defined(__AVX2__)
#define ZI_BASE_TIER 4
#elif ZI_X86 && defined(__SSE4_1__)
#define ZI_BASE_TIER 3
#elif ZI_X86 && defined(__SSSE3__)
#define ZI_BASE_TIER 2
#elif ZI_X86
#define ZI_BASE_TIER 1
#else
#define ZI_BASE_TIER 0
#endif
#define ZI_TIER ZI_BASE_TIER
#define ZI_FN ZI_INLINE
#define ZI_N(name) zi_##name
#define ZI_NS 1 /* which prefix ZI_N adds: zi_, zi_ssse3_, zi_sse41_, zi_avx2_ */
#include "ziran_simd_ops.h"
#undef ZI_TIER
#undef ZI_FN
#undef ZI_N
#undef ZI_NS

#include <stdio.h>
#include <stdlib.h>

/* The address of `count` lanes from `index` in memory of `length` lanes; a
 * run outside it stops the program, as an index out of bounds does. */
static inline const void *
zi_simd_at(const void *data, int64_t length, int64_t index, int count, int lane)
{
    if(index < 0 || index > length - count) {
        fprintf(stderr, "ziran: SIMD access of %d lanes at %lld out of bounds (length %lld)\n",
                count, (long long)index, (long long)length);
        abort();
    }
    return (const char *)data + index * lane;
}

/* Every lane of a 32-bit index vector must name an entry of the table. */
static inline void
zi_simd_gather_check(const void *indices, int lanes, int is_signed, int64_t length)
{
    for(int i = 0; i < lanes; i++) {
        uint32_t bits;
        memcpy(&bits, (const char *)indices + 4 * i, 4);
        int64_t index = is_signed ? (int64_t)(int32_t)bits : (int64_t)bits;
        if(index < 0 || index >= length) {
            fprintf(stderr, "ziran: gather index %lld out of bounds (length %lld)\n",
                    (long long)index, (long long)length);
            abort();
        }
    }
}

/* Compilers that never inline, such as TinyCC and unoptimized GCC or Clang,
 * check in place and call only to report. Neither keeps type-based aliasing,
 * so index lanes are read as they lie. */
#if defined(__TINYC__) || ((defined(__GNUC__) || defined(__clang__)) && !defined(__OPTIMIZE__))
#define zi_simd_at(data, length, index, count, lane) __extension__ ({ \
        const char *__zi_at_data = (const char *)(data); \
        int64_t __zi_at_length = (length), __zi_at = (index); \
        if(__zi_at < 0 || __zi_at > __zi_at_length - (count)) \
            (zi_simd_at)(__zi_at_data, __zi_at_length, __zi_at, (count), (lane)); \
        (const void *)(__zi_at_data + __zi_at * (lane)); })
#define zi_simd_gather_check(indices, lanes, is_signed, length) __extension__ ({ \
        const uint32_t *__zi_gather = (const uint32_t *)(const void *)(indices); \
        uint64_t __zi_gather_length = (uint64_t)(int64_t)(length); \
        for(int __zi_gather_lane = 0; __zi_gather_lane < (lanes); __zi_gather_lane++) { \
            uint32_t __zi_gather_bits = __zi_gather[__zi_gather_lane]; \
            if(((is_signed) ? (uint64_t)(int64_t)(int32_t)__zi_gather_bits : (uint64_t)__zi_gather_bits) >= \
               __zi_gather_length) \
                (zi_simd_gather_check)(__zi_gather, (lanes), (is_signed), (int64_t)__zi_gather_length); \
        } })
#endif
#endif /* ZIRAN_SIMD_REF_ONLY */

#endif /* ZIRAN_SIMD_COMMON_H */

/* x86-64 tiers for #target procedures, outside the guard so a program
 * that defines ZIRAN_SIMD_TIERS after an earlier include still gets them. */
#if !defined(ZIRAN_SIMD_REF_ONLY) && ZI_X86 && ZI_VEC && defined(ZIRAN_SIMD_TIERS) && \
    !defined(ZIRAN_SIMD_TIERS_DONE)
#define ZIRAN_SIMD_TIERS_DONE 1
#define ZI_TIER 2
#define ZI_FN ZI_INLINE __attribute__((target("ssse3")))
#define ZI_N(name) zi_ssse3_##name
#define ZI_NS 2
#include "ziran_simd_ops.h"
#undef ZI_TIER
#undef ZI_FN
#undef ZI_N
#undef ZI_NS
#define ZI_TIER 3
#define ZI_FN ZI_INLINE __attribute__((target("sse4.1")))
#define ZI_N(name) zi_sse41_##name
#define ZI_NS 3
#include "ziran_simd_ops.h"
#undef ZI_TIER
#undef ZI_FN
#undef ZI_N
#undef ZI_NS
#define ZI_TIER 4
#define ZI_FN ZI_INLINE __attribute__((target("avx2")))
#define ZI_N(name) zi_avx2_##name
#define ZI_NS 4
#include "ziran_simd_ops.h"
#undef ZI_TIER
#undef ZI_FN
#undef ZI_N
#undef ZI_NS
#if ZI_BASE_TIER < 4
/* #target procedures dispatch between clones when the build's own tier is
 * below AVX2; generated C tests this. */
#define ZIRAN_SIMD_DISPATCH 1
#define ZI_TARGET_SSSE3 __attribute__((target("ssse3")))
#define ZI_TARGET_SSE41 __attribute__((target("sse4.1")))
#define ZI_TARGET_AVX2 __attribute__((target("avx2")))
#include <cpuid.h>

/* The best tier this CPU and OS run, at most ZIRAN_SIMD_TIER when that is set
 * to 1 (SSE2) .. 4 (AVX2); 0 when the build's own tier is as good. */
static int
zi_simd_tier_detect(void)
{
    unsigned a, b, c, d;
    int tier = 1;
    if(__get_cpuid(1, &a, &b, &c, &d)) {
        if(c & bit_SSSE3) tier = 2;
        if(tier == 2 && (c & bit_SSE4_1)) tier = 3;
        /* AVX2 also needs the OS to save the YMM registers. */
        if(tier == 3 && (c & bit_OSXSAVE) && (c & bit_AVX)) {
            unsigned low, high;
            __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
            (void)high;
            if((low & 6) == 6 && __get_cpuid_max(0, NULL) >= 7) {
                __cpuid_count(7, 0, a, b, c, d);
                if(b & bit_AVX2) tier = 4;
            }
        }
    }
    /* #asm feature probes read it the same way (cmd/zir/zir_asm.c). */
    const char *forced = getenv("ZIRAN_SIMD_TIER");
    if(forced != NULL && forced[0] >= '1' && forced[0] <= '4' && forced[1] == '\0' &&
       forced[0] - '0' < tier)
        tier = forced[0] - '0';
    return tier > ZI_BASE_TIER ? tier : 0;
}

static inline int
zi_simd_tier(void)
{
    static int cached = -1;
    int tier = __atomic_load_n(&cached, __ATOMIC_RELAXED);
    if(tier < 0) {
        tier = zi_simd_tier_detect();
        __atomic_store_n(&cached, tier, __ATOMIC_RELAXED);
    }
    return tier;
}
#endif
#endif
