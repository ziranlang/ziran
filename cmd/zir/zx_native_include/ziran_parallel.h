#ifndef ZIRAN_PARALLEL_H
#define ZIRAN_PARALLEL_H
/* Natively run bundles hand `#parallel for` regions to the engine, which
 * runs their chunks on its worker threads. */
#include <stdint.h>

void ZxNativeParallel(void (*worker)(int64_t low, int64_t high, void *context),
                      int64_t start, int64_t end, void *context);

/* Whether two byte ranges share storage: a region that writes through a
 * slice runs in order when another slice, string or array it reads does. */
static inline int ziran_parallel_overlap(const void *a, int64_t a_bytes,
                                         const void *b, int64_t b_bytes)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return a_bytes > 0 && b_bytes > 0 && x < y + (uintptr_t)b_bytes &&
           y < x + (uintptr_t)a_bytes;
}

static inline void ziran_parallel_run(
    void (*worker)(int64_t low, int64_t high, void *context),
    int64_t start, int64_t end, void *context)
{
    ZxNativeParallel(worker, start, end, context);
}

#endif
