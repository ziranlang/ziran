#ifndef ZIRAN_PARALLEL_H
#define ZIRAN_PARALLEL_H
/* Natively run bundles hand `#parallel for` regions to the engine, which
 * runs their chunks on its worker threads. */
#include <stdint.h>

void ZxNativeParallel(void (*worker)(int64_t low, int64_t high, void *context),
                      int64_t start, int64_t end, void *context);

static inline void ziran_parallel_run(
    void (*worker)(int64_t low, int64_t high, void *context),
    int64_t start, int64_t end, void *context)
{
    ZxNativeParallel(worker, start, end, context);
}

#endif
