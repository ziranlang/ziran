#ifndef ZIRAN_PARALLEL_H
#define ZIRAN_PARALLEL_H

/* Threaded execution for checked `#parallel for` regions on the native C
 * target. Generated code includes this header only when it emits a
 * parallel worker; such translation units must compile and link with
 * -pthread. Iterations run in contiguous chunks with no shared mutable
 * state: the checker admits only pure or observing callees and
 * iteration-local writes, so any interleaving yields the same result. */

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int ziran_parallel_threads(void)
{
    long requested = 4;
    const char *from_env = getenv("ZIRAN_PAR_THREADS");
    if(from_env != NULL && from_env[0] != '\0') {
        long parsed = strtol(from_env, NULL, 10);
        if(parsed >= 1)
            requested = parsed;
    }
    if(requested > 64)
        requested = 64;
    return (int)requested;
}

typedef struct ziran_parallel_job {
    int64_t low;
    int64_t high;
    void *context;
    void (*worker)(int64_t low, int64_t high, void *context);
} ziran_parallel_job;

static inline void *ziran_parallel_entry(void *opaque)
{
    ziran_parallel_job *job = (ziran_parallel_job *)opaque;
    job->worker(job->low, job->high, job->context);
    return NULL;
}

static inline void ziran_parallel_run(
    void (*worker)(int64_t low, int64_t high, void *context),
    int64_t start, int64_t end, void *context)
{
    int64_t span = end - start + 1;
    int threads = ziran_parallel_threads();
    pthread_t handles[64];
    ziran_parallel_job jobs[64];
    int launched = 0;
    int64_t chunk;
    if(end < start)
        return;
    if(span / 2 < threads)
        threads = (int)(span / 2);
    if(threads <= 1) {
        worker(start, end, context);
        return;
    }
    chunk = span / threads;
    for(int t = 0; t < threads; t++) {
        int64_t low = start + (int64_t)t * chunk;
        int64_t high = t == threads - 1 ? end : low + chunk - 1;
        if(t == threads - 1) {
            worker(low, high, context);
            break;
        }
        jobs[t].low = low;
        jobs[t].high = high;
        jobs[t].context = context;
        jobs[t].worker = worker;
        if(pthread_create(&handles[launched], NULL, ziran_parallel_entry,
                          &jobs[t]) != 0)
            worker(low, high, context);
        else
            launched++;
    }
    for(int t = 0; t < launched; t++)
        pthread_join(handles[t], NULL);
}

#ifdef __cplusplus
}
#endif

#endif
