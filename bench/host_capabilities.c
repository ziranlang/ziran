/* Public bundle binding/dispatch costs, checked against an external oracle. */
#include "ziran_host.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __GLIBC__
#include <malloc.h>
#endif

static double milliseconds(clockid_t clock)
{
    struct timespec now;
    if(clock_gettime(clock, &now)) abort();
    return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
}

static long long heap_bytes(void)
{
#if defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 33)
    return (long long)mallinfo2().uordblks;
#endif
#endif
    return -1;
}

typedef struct State { size_t calls, count; } State;
typedef struct Context { size_t index; const char *function; State *state; } Context;

static int host_call(void *opaque, const char *module, const char *function,
                     const VmHostValue *args, int count, VmHostValue *result)
{
    Context *context = opaque;
    assert(strcmp(module, "api") == 0 && strcmp(function, context->function) == 0);
    assert(context->state->calls % context->state->count == context->index);
    assert(count == 1 && args[0].kind == VM_HOST_INTEGER);
    context->state->calls++;
    result->kind = VM_HOST_INTEGER;
    result->integer = (int64_t)(((uint64_t)args[0].integer * 48271 + context->index + 1) & INT64_MAX);
    return 1;
}

static int ordered(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv)
{
    if(argc != 5) return 2;
    char *end;
    errno = 0;
    long instances = strtol(argv[2], &end, 10);
    if(errno || *end || instances < 1 || instances > 10000) return 2;
    long loops = strtol(argv[3], &end, 10);
    if(errno || *end || loops < 1 || loops > 10000) return 2;
    long long expected = strtoll(argv[4], &end, 10);
    if(errno || *end) return 2;
    double *samples[7], first[7];
    for(int p = 0; p < 7; p++) {
        samples[p] = calloc((size_t)instances, sizeof(double));
        assert(samples[p] != NULL);
    }
    long long heap_before = heap_bytes();
    double wall = milliseconds(CLOCK_MONOTONIC), cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID);
    Bundle *bundle = BundleOpen(argv[1]);
    double load_cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID) - cpu;
    double load_wall = milliseconds(CLOCK_MONOTONIC) - wall;
    assert(bundle != NULL);
    long long bundle_heap = heap_bytes() - heap_before;
    wall = milliseconds(CLOCK_MONOTONIC); cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID);
    size_t count = BundleCapabilityCount(bundle);
    assert(count > 0 && count <= 1024);
    Context *contexts = calloc(count, sizeof(*contexts));
    HostBinding *bindings = calloc(count, sizeof(*bindings));
    assert(contexts != NULL && bindings != NULL);
    State state = {.count = count};
    for(size_t i = 0; i < count; i++) {
        const char *module = BundleCapabilityModule(bundle, i);
        const char *function = BundleCapabilityFunction(bundle, i);
        assert(module != NULL && function != NULL && function[0] == 'H');
        unsigned index = (unsigned)strtoul(function + 1, &end, 10);
        assert(*end == 0 && index < count);
        contexts[index] = (Context){index, function, &state};
        /* Host order must not determine which implementation is called. */
        bindings[count - i - 1] = (HostBinding){module, function, host_call, &contexts[index]};
    }
    double enumeration_cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID) - cpu;
    double enumeration_wall = milliseconds(CLOCK_MONOTONIC) - wall;
    for(long i = 0; i <= instances; i++) {
        heap_before = heap_bytes();
        wall = milliseconds(CLOCK_MONOTONIC); cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID);
        BundleInstance *instance = BundleInstantiate(bundle, bindings, count);
        double open_cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID) - cpu;
        double open_wall = milliseconds(CLOCK_MONOTONIC) - wall;
        assert(instance != NULL);
        long long instance_heap = heap_bytes() - heap_before;
        BundleInstanceLimitSteps(instance, 1000000);
        long long result = 0;
        int present = 0;
        state.calls = 0;
        wall = milliseconds(CLOCK_MONOTONIC); cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID);
        assert(BundleInstanceRun(instance, &result, &present));
        assert(present && result == expected && state.calls == count * (size_t)loops);
        double run_cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID) - cpu;
        double run_wall = milliseconds(CLOCK_MONOTONIC) - wall;
        wall = milliseconds(CLOCK_MONOTONIC); cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID);
        BundleInstanceClose(instance);
        double close_cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID) - cpu;
        double close_wall = milliseconds(CLOCK_MONOTONIC) - wall;
        double values[] = {open_wall, open_cpu, run_wall, run_cpu, close_wall, close_cpu,
                           heap_before < 0 ? -1 : (double)instance_heap};
        for(int p = 0; p < 7; p++) {
            if(i == 0) first[p] = values[p];
            else samples[p][i - 1] = values[p];
        }
    }
    BundleClose(bundle);
    free(bindings); free(contexts);
    printf("{\"load_wall_ms\":%.9f,\"load_cpu_ms\":%.9f,\"enumeration_wall_ms\":%.9f,"
           "\"enumeration_cpu_ms\":%.9f,\"bundle_heap_bytes\":%lld,\"capabilities\":%zu,"
           "\"instances\":%ld,\"loops\":%ld,\"result\":%lld",
           load_wall, load_cpu, enumeration_wall, enumeration_cpu, heap_before < 0 ? -1 : bundle_heap,
           count, instances, loops, expected);
    const char *names[] = {"instantiate_wall_ms", "instantiate_cpu_ms", "run_wall_ms", "run_cpu_ms",
                           "close_wall_ms", "close_cpu_ms", "instance_heap_bytes"};
    for(int p = 0; p < 7; p++) {
        qsort(samples[p], (size_t)instances, sizeof(double), ordered);
        double median = instances % 2 ? samples[p][instances / 2] :
            (samples[p][instances / 2 - 1] + samples[p][instances / 2]) * 0.5;
        printf(",\"first_%s\":%.9f,\"%s\":%.9f", names[p], first[p], names[p], median);
        free(samples[p]);
    }
    puts("}");
    return 0;
}
