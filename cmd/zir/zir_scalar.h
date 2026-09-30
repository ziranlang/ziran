#ifndef ZIR_SCALAR_H
#define ZIR_SCALAR_H

#include <stdint.h>
#include <string.h>

/* Bit representations, rather than host signed arithmetic, define integers. */
static inline unsigned ScalarWidth(const char *type)
{
    if(!strcmp(type, "int") || !strcmp(type, "integer")) return 64;
    if(type[0] != 's' && type[0] != 'u') return 0;
    if(!strcmp(type + 1, "8")) return 8;
    if(!strcmp(type + 1, "16")) return 16;
    if(!strcmp(type + 1, "32")) return 32;
    if(!strcmp(type + 1, "64")) return 64;
    return 0;
}

static inline uint64_t ScalarMask(unsigned width)
{
    return width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
}

static inline int64_t ScalarSigned(uint64_t bits, unsigned width)
{
    bits &= ScalarMask(width);
    uint64_t sign = UINT64_C(1) << (width - 1);
    if(!(bits & sign)) return (int64_t)bits;
    return -1 - (int64_t)((~bits) & ScalarMask(width));
}

static inline int64_t ScalarStored(uint64_t bits, const char *type)
{
    unsigned width = ScalarWidth(type);
    if(!width) width = 64;
    bits &= ScalarMask(width);
    return type[0] == 'u' && width < 64 ? (int64_t)bits :
           ScalarSigned(bits, type[0] == 'u' ? 64 : width);
}

static inline int ScalarCompare(uint64_t a, uint64_t b, unsigned width,
                                int is_unsigned)
{
    a &= ScalarMask(width);
    b &= ScalarMask(width);
    if(is_unsigned) return a < b ? -1 : a > b;
    int64_t x = ScalarSigned(a, width), y = ScalarSigned(b, width);
    return x < y ? -1 : x > y;
}

#endif
