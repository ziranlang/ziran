#include "ziran_host.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

static uint64_t ticks(void)
{
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return (uint64_t)now.tv_sec * 1000000000 + (uint64_t)now.tv_nsec;
}

static long long heap_bytes(void)
{
#if defined(__GLIBC__)
    struct mallinfo2 info = mallinfo2();
    return (long long)info.uordblks + (long long)info.hblkhd;
#else
    return -1;
#endif
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    int functions = atoi(argv[2]), runs = atoi(argv[3]);
    assert(functions > 0 && runs > 0);
    FILE *input = fopen(argv[1], "rb");
    assert(input && !fseek(input, 0, SEEK_END));
    long length = ftell(input);
    assert(length > 0 && !fseek(input, 0, SEEK_SET));
    unsigned char *data = malloc((size_t)length);
    assert(data && fread(data, 1, (size_t)length, input) == (size_t)length && !fclose(input));
    long long before = heap_bytes();
    uint64_t start = ticks();
    Bundle *bundle = BundleOpenBytes(data, (size_t)length);
    uint64_t opened = ticks();
    assert(bundle && BundleCapabilityCount(bundle) == 0);
    long long live = heap_bytes();
    BundleInstance *instance = BundleInstantiate(bundle, NULL, 0);
    uint64_t instantiated = ticks();
    assert(instance);
    for(int i = 0; i < runs; i++) {
        long long result = 0; int has_result = 0;
        assert(BundleInstanceRun(instance, &result, &has_result));
        assert(has_result && result == 7 + functions + i + 1);
    }
    uint64_t executed = ticks();
    BundleInstanceClose(instance); BundleClose(bundle);
    uint64_t closed = ticks();
    long long retained = heap_bytes();
    printf("{\"bytes\":%ld,\"open_ms\":%.6f,\"instantiate_ms\":%.6f,\"run_ms\":%.6f,\"close_ms\":%.6f,\"live_heap_delta_bytes\":%lld,\"retained_heap_delta_bytes\":%lld,\"heap_supported\":%s}\n",
           length, (opened-start)/1e6, (instantiated-opened)/1e6, (executed-instantiated)/1e6,
           (closed-executed)/1e6, live-before, retained-before, before >= 0 ? "true" : "false");
    free(data);
    return 0;
}
