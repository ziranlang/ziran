/* Separate checked bundle loading, instance preparation, and execution. */
#include "zir_bundle.h"
#include "zir_vm.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double milliseconds(void)
{
    struct timespec now;
    if(clock_gettime(CLOCK_MONOTONIC, &now)) abort();
    return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
}

static int ordered(const void *left, const void *right)
{
    double a = *(const double *)left, b = *(const double *)right;
    return (a > b) - (a < b);
}

int main(int argc, char **argv)
{
    if(argc != 4) return 2;
    char *end;
    errno = 0;
    long runs = strtol(argv[2], &end, 10);
    if(errno || *end || runs < 1 || runs > 10000) return 2;
    errno = 0;
    long long expected = strtoll(argv[3], &end, 10);
    if(errno || *end) return 2;
    double *samples = calloc((size_t)runs, sizeof(*samples));
    if(samples == NULL) return 2;
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZibLawTable laws = {0};
    double started = milliseconds();
    FILE *input = fopen(argv[1], "rb");
    if(input == NULL) return 2;
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                    entry, sizeof(entry), &laws, NULL);
    fclose(input);
    double loaded = milliseconds();
    if(program == NULL) return 1;
    VmInstance *instance = VmInstanceOpen(program, module, entry, NULL, NULL);
    double prepared = milliseconds();
    if(instance == NULL) return 1;
    /* Prevent a broken fixture from hanging the benchmark. */
    VmInstanceLimitSteps(instance, 1000000);
    double first_run = 0;
    for(long i = 0; i <= runs; i++) {
        long long result = 0;
        int present = 0;
        double begin = milliseconds();
        if(!VmInstanceRun(instance, &result, &present) || !present || result != expected) {
            fprintf(stderr, "unexpected VM result at run %ld: %lld (expected %lld)\n",
                    i, result, expected);
            return 1;
        }
        double elapsed = milliseconds() - begin;
        if(i == 0) first_run = elapsed;
        else samples[i - 1] = elapsed;
    }
    size_t live_bytes = VmInstanceLiveValueBytes(instance);
    double closing = milliseconds();
    VmInstanceClose(instance);
    ZibLawTableFree(&laws);
    ProgramFree(program);
    double closed = milliseconds();
    qsort(samples, (size_t)runs, sizeof(*samples), ordered);
    double median = runs % 2 ? samples[runs / 2] :
        (samples[runs / 2 - 1] + samples[runs / 2]) * 0.5;
    printf("{\"load_ms\":%.9f,\"open_ms\":%.9f,\"first_run_ms\":%.9f,"
           "\"steady_run_ms\":%.9f,\"steady_min_ms\":%.9f,\"steady_max_ms\":%.9f,"
           "\"close_ms\":%.9f,\"live_value_bytes\":%zu,\"runs\":%ld,\"result\":%lld}\n",
           loaded - started, prepared - loaded, first_run, median,
           samples[0], samples[runs - 1], closed - closing, live_bytes, runs, expected);
    free(samples);
    return 0;
}
