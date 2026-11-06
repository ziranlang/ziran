#ifndef ZIRAN_SLICE_H
#define ZIRAN_SLICE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* Compiler-owned descriptor. Element types and storage lifetimes are checked
 * before emission; generated accesses cast data to the declared element type. */
typedef struct Slice {
    void *data;
    int64_t length;
} Slice;

static inline int64_t
SliceIndex(Slice source, int64_t index)
{
    if(index < 0 || index >= source.length) {
        fprintf(stderr, "ziran: slice index out of bounds\n");
        abort();
    }
    return index;
}
#if defined(__TINYC__) || ((defined(__GNUC__) || defined(__clang__)) && !defined(__OPTIMIZE__))
#define SliceIndex(source, index) __extension__ ({ \
        int64_t zi__slice_index = (index); \
        if((uint64_t)zi__slice_index >= (uint64_t)(source).length) \
            (SliceIndex)((source), zi__slice_index); \
        zi__slice_index; })
#endif

static inline Slice
SliceRange(Slice source, int64_t low, int64_t high, size_t element_size)
{
    if(low < 0 || high < low || high > source.length) {
        fprintf(stderr, "ziran: slice range out of bounds\n");
        abort();
    }
    Slice result;
    result.data = source.data != NULL ? (char *)source.data + (size_t)low * element_size : NULL;
    result.length = high - low;
    return result;
}

#endif /* ZIRAN_SLICE_H */
