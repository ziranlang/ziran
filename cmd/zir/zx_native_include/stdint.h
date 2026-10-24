#ifndef ZX_STDINT_H
#define ZX_STDINT_H
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef __INT64_TYPE__ int64_t;
typedef unsigned __INT64_TYPE__ uint64_t;
typedef __PTRDIFF_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef int64_t intmax_t;
typedef uint64_t uintmax_t;
#define INT8_MIN (-128)
#define INT8_MAX 127
#define UINT8_MAX 255
#define INT16_MIN (-32767 - 1)
#define INT16_MAX 32767
#define UINT16_MAX 65535
#define INT32_MIN (-2147483647 - 1)
#define INT32_MAX 2147483647
#define UINT32_MAX 4294967295U
#define INT64_C(c) ((int64_t)c##LL)
#define UINT64_C(c) ((uint64_t)c##ULL)
#define INT64_MAX INT64_C(9223372036854775807)
#define INT64_MIN (-INT64_MAX - 1)
#define UINT64_MAX UINT64_C(18446744073709551615)
#if __SIZEOF_POINTER__ == 8
#define INTPTR_MIN INT64_MIN
#define INTPTR_MAX INT64_MAX
#define UINTPTR_MAX UINT64_MAX
#else
#define INTPTR_MIN INT32_MIN
#define INTPTR_MAX INT32_MAX
#define UINTPTR_MAX UINT32_MAX
#endif
#define PTRDIFF_MIN INTPTR_MIN
#define PTRDIFF_MAX INTPTR_MAX
#define SIZE_MAX UINTPTR_MAX
#endif
