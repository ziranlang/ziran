#ifndef ZIRAN_RUNTIME_VEC_H
#define ZIRAN_RUNTIME_VEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

static inline size_t
ZirVecIndex(int64_t count, int64_t index)
{
    if(count < 0 || index < 0 || index >= count) {
        fprintf(stderr, "ziran: Vec index out of bounds\n");
        abort();
    }
    return (size_t)index;
}

#define ZIRAN_VEC_INDEX(data, count, index) \
    ((data)[ZirVecIndex((int64_t)(count), (int64_t)(index))])

/* Reserve room for `additional` more elements (at least one). A failed
 * reserve leaves both fields unchanged. */
static inline void *
ZirVecReserve(void *data, int64_t *capacity, int64_t count,
              int64_t additional, size_t element_size)
{
    if(capacity == NULL || element_size == 0 || count < 0 ||
       *capacity < count || additional <= 0)
        return NULL;
    if(count > INT64_MAX - additional)
        return NULL;
    uint64_t needed = (uint64_t)count + (uint64_t)additional;
    uint64_t next = *capacity > 0 ? (uint64_t)*capacity : 8u;
    while(next < needed) {
        if(next > (uint64_t)INT64_MAX / 2u) {
            next = needed;
            break;
        }
        next *= 2u;
    }
    if(next > (uint64_t)INT64_MAX || next > SIZE_MAX / element_size)
        return NULL;
    if((uint64_t)*capacity >= needed)
        return data;
    void *grown = realloc(data, (size_t)next * element_size);
    if(grown == NULL)
        return NULL;
    *capacity = (int64_t)next;
    return grown;
}

/* Reserve one more element. A failed reserve leaves both fields unchanged. */
static inline void *
ZirVecGrow(void *data, int64_t *capacity, int64_t count,
           size_t element_size)
{
    return ZirVecReserve(data, capacity, count, 1, element_size);
}

#endif
