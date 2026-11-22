#include "zir_sha256.h"

#include <stdlib.h>

/* CPUs with SHA-256 instructions (x86's SHA extensions, ARMv8's SHA2 in 64-
 * and 32-bit mode) hash several times faster than the portable rounds, and a
 * phone hashes every bundle it opens. Each path is compiled for its
 * instructions alone and chosen at run time. */
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#define SHA256_X86 1
#include <cpuid.h>
#include <immintrin.h>
#elif (defined(__aarch64__) || defined(__arm__)) && \
      (defined(__clang__) || defined(__ARM_FEATURE_SHA2) || defined(__ARM_FEATURE_CRYPTO))
#define SHA256_ARM 1
#include <arm_neon.h>
#if defined(__linux__)
#include <sys/auxv.h>
#endif
#endif

static const uint32_t round_constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

#if defined(SHA256_X86)
/* The state is kept as ABEF and CDGH; each sha256rnds2 runs two rounds and
 * sha256msg1/msg2 extend the message four words at a time. */
__attribute__((target("sha,sse4.1")))
static void
compress_x86(uint32_t state[8], const unsigned char *block, size_t blocks)
{
    const __m128i order = _mm_set_epi64x(0x0c0d0e0f08090a0bULL, 0x0405060700010203ULL);
    __m128i cdab = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i *)&state[0]), 0xB1);
    __m128i efgh = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i *)&state[4]), 0x1B);
    __m128i abef = _mm_alignr_epi8(cdab, efgh, 8);
    __m128i cdgh = _mm_blend_epi16(efgh, cdab, 0xF0);
    for(; blocks > 0; blocks--, block += 64) {
        __m128i saved_abef = abef, saved_cdgh = cdgh;
        __m128i w[4];
        for(int i = 0; i < 4; i++)
            w[i] = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)(block + i * 16)), order);
        for(int i = 0; i < 16; i++) {
            __m128i words = _mm_add_epi32(w[i & 3],
                _mm_loadu_si128((const __m128i *)&round_constants[i * 4]));
            cdgh = _mm_sha256rnds2_epu32(cdgh, abef, words);
            abef = _mm_sha256rnds2_epu32(abef, cdgh, _mm_shuffle_epi32(words, 0x0E));
            if(i < 12) {
                __m128i next = _mm_add_epi32(_mm_sha256msg1_epu32(w[i & 3], w[(i + 1) & 3]),
                                             _mm_alignr_epi8(w[(i + 3) & 3], w[(i + 2) & 3], 4));
                w[i & 3] = _mm_sha256msg2_epu32(next, w[(i + 3) & 3]);
            }
        }
        abef = _mm_add_epi32(abef, saved_abef);
        cdgh = _mm_add_epi32(cdgh, saved_cdgh);
    }
    __m128i feba = _mm_shuffle_epi32(abef, 0x1B);
    __m128i dchg = _mm_shuffle_epi32(cdgh, 0xB1);
    _mm_storeu_si128((__m128i *)&state[0], _mm_blend_epi16(feba, dchg, 0xF0));
    _mm_storeu_si128((__m128i *)&state[4], _mm_alignr_epi8(dchg, feba, 8));
}

static int
hardware_sha256(void)
{
    unsigned int a, b, c, d;
    if(!__get_cpuid(1, &a, &b, &c, &d) || !(c & bit_SSE4_1) || !(c & bit_SSSE3))
           return 0;
    return __get_cpuid_count(7, 0, &a, &b, &c, &d) && (b & (1u << 29));
}
#define compress_hardware compress_x86
#elif defined(SHA256_ARM)
#if defined(__clang__) && defined(__arm__) && __ARM_ARCH < 8
/* arm_neon.h declares the SHA-256 intrinsics only when the whole file
 * targets ARMv8; an armv7 build reaches the same instructions through their
 * builtins, in a function compiled for ARMv8 that runs only on such CPUs. */
#define SHA256_ARM_TARGET __attribute__((target("sha2,v8")))
#define vsha256hq_u32(a, b, c) ((uint32x4_t)__builtin_neon_vsha256hq_u32( \
    (int8x16_t)(a), (int8x16_t)(b), (int8x16_t)(c), 50))
#define vsha256h2q_u32(a, b, c) ((uint32x4_t)__builtin_neon_vsha256h2q_u32( \
    (int8x16_t)(a), (int8x16_t)(b), (int8x16_t)(c), 50))
#define vsha256su0q_u32(a, b) ((uint32x4_t)__builtin_neon_vsha256su0q_u32( \
    (int8x16_t)(a), (int8x16_t)(b), 50))
#define vsha256su1q_u32(a, b, c) ((uint32x4_t)__builtin_neon_vsha256su1q_u32( \
    (int8x16_t)(a), (int8x16_t)(b), (int8x16_t)(c), 50))
#elif defined(__clang__)
#define SHA256_ARM_TARGET __attribute__((target("sha2")))
#else
#define SHA256_ARM_TARGET
#endif
/* sha256h/sha256h2 run four rounds on ABCD and EFGH; sha256su0/su1 extend
 * the message four words at a time. */
SHA256_ARM_TARGET
static void
compress_arm(uint32_t state[8], const unsigned char *block, size_t blocks)
{
    uint32x4_t abcd = vld1q_u32(&state[0]);
    uint32x4_t efgh = vld1q_u32(&state[4]);
    for(; blocks > 0; blocks--, block += 64) {
        uint32x4_t saved_abcd = abcd, saved_efgh = efgh;
        uint32x4_t w[4];
        for(int i = 0; i < 4; i++)
            w[i] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(block + i * 16)));
        for(int i = 0; i < 16; i++) {
            uint32x4_t words = vaddq_u32(w[i & 3], vld1q_u32(&round_constants[i * 4]));
            uint32x4_t previous = abcd;
            abcd = vsha256hq_u32(abcd, efgh, words);
            efgh = vsha256h2q_u32(efgh, previous, words);
            if(i < 12)
                w[i & 3] = vsha256su1q_u32(vsha256su0q_u32(w[i & 3], w[(i + 1) & 3]),
                                           w[(i + 2) & 3], w[(i + 3) & 3]);
        }
        abcd = vaddq_u32(abcd, saved_abcd);
        efgh = vaddq_u32(efgh, saved_efgh);
    }
    vst1q_u32(&state[0], abcd);
    vst1q_u32(&state[4], efgh);
}

static int
hardware_sha256(void)
{
#if defined(__APPLE__)
    return 1;
#elif defined(__linux__) && defined(__aarch64__)
    return (getauxval(AT_HWCAP) & (1ul << 6)) != 0; /* HWCAP_SHA2 */
#elif defined(__linux__)
    return (getauxval(AT_HWCAP2) & (1ul << 3)) != 0; /* HWCAP2_SHA2 */
#else
       return 0;
#endif
}
#define compress_hardware compress_arm
#endif


int
S15ShaHardwareAvailable(void)
{
#if defined(SHA256_X86) || defined(SHA256_ARM)
    return hardware_sha256();
#else
    return 0;
#endif
}
void
S15ShaHardwareCompress(uint32_t *state, const unsigned char *block, size_t blocks)
{
#if defined(SHA256_X86) || defined(SHA256_ARM)
    compress_hardware(state, block, blocks);
#else
    (void)state; (void)block; (void)blocks;
    abort();
#endif
}
