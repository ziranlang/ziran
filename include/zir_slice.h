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
        int64_t __zi_slice_index = (index); \
        if((uint64_t)__zi_slice_index >= (uint64_t)(source).length) \
            (SliceIndex)((source), __zi_slice_index); \
        __zi_slice_index; })
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
/* Where nothing inlines, the view is taken in place; the call only reports. */
#if defined(__TINYC__) || ((defined(__GNUC__) || defined(__clang__)) && !defined(__OPTIMIZE__))
#define SliceRange(source, low, high, element_size) __extension__ ({ \
        Slice __zi_range = (source); \
        int64_t __zi_range_low = (low), __zi_range_high = (high); \
        if(__zi_range_low < 0 || __zi_range_high < __zi_range_low || __zi_range_high > __zi_range.length) \
            (SliceRange)(__zi_range, __zi_range_low, __zi_range_high, (element_size)); \
        if(__zi_range.data != NULL) \
            __zi_range.data = (char *)__zi_range.data + (size_t)__zi_range_low * (element_size); \
        __zi_range.length = __zi_range_high - __zi_range_low; \
        __zi_range; })
#endif

#endif /* ZIRAN_SLICE_H */
