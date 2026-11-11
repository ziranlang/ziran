#ifndef ZIRAN_RUNTIME_BOUNDS_H
#define ZIRAN_RUNTIME_BOUNDS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Fixed-array and string indexing is checked in every build. Keep the index
 * wide until it is validated: a 64-bit index must not wrap into a valid
 * offset on a 32-bit target. */
static inline size_t BoundsCheck(size_t length, uint64_t index,
                                 const char *what)
{
    if(index >= length) {
        fprintf(stderr, "ziran: index %llu out of bounds for %s (length %zu)\n",
                (unsigned long long)index, what, length);
        abort();
    }
    return (size_t)index;
}
#define ZIRAN_INDEX(base, length, index) \
    ((base)[BoundsCheck((size_t)(length), (uint64_t)(index), #base)])
/* Compilers that never inline, such as TinyCC and unoptimized GCC, check in
 * place rather than call. */
#if defined(__TINYC__) || ((defined(__GNUC__) || defined(__clang__)) && !defined(__OPTIMIZE__))
#undef ZIRAN_INDEX
#define ZIRAN_INDEX(base, length, index) \
    ((base)[__extension__ ({ uint64_t __zi_index = (uint64_t)(index); \
        if(__zi_index >= (uint64_t)(size_t)(length)) \
            BoundsCheck((size_t)(length), __zi_index, #base); \
        (size_t)__zi_index; })])
#endif

/* A zero-capacity array has no valid index in any build configuration. */
static inline size_t EmptyArrayIndex(uint64_t index, const char *what)
{
    fprintf(stderr, "ziran: index %llu out of bounds for %s (length 0)\n",
            (unsigned long long)index, what);
    abort();
}
#define ZIRAN_EMPTY_INDEX(base, index) \
    ((base)[EmptyArrayIndex((uint64_t)(index), #base)])

#endif /* ZIRAN_RUNTIME_BOUNDS_H */
