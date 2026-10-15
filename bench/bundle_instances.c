/* Measure the public, checked bundle lifecycle and independent fresh states. */
#include "ziran_host.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double milliseconds(clockid_t clock)
{
    struct timespec now;
    if(clock_gettime(clock, &now)) abort();
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
    double *samples[6];
    for(int phase = 0; phase < 6; phase++) {
        samples[phase] = calloc((size_t)runs, sizeof(double));
        if(samples[phase] == NULL) return 2;
    }
    double wall = milliseconds(CLOCK_MONOTONIC);
    double cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID);
    Bundle *bundle = BundleOpen(argv[1]);
    double load_cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID) - cpu;
    double load_wall = milliseconds(CLOCK_MONOTONIC) - wall;
    if(bundle == NULL) return 1;
    double first[6];
    for(long run = 0; run <= runs; run++) {
        wall = milliseconds(CLOCK_MONOTONIC);
        cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID);
        BundleInstance *instance = BundleInstantiate(bundle, NULL, 0);
        double open_cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID) - cpu;
        double open_wall = milliseconds(CLOCK_MONOTONIC) - wall;
        if(instance == NULL) return 1;
        BundleInstanceLimitSteps(instance, 1000000);
        long long result = 0;
        int present = 0;
        wall = milliseconds(CLOCK_MONOTONIC);
        cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID);
        if(!BundleInstanceRun(instance, &result, &present) || !present || result != expected) {
            fprintf(stderr, "fresh instance %ld: %lld, expected %lld\n", run, result, expected);
            return 1;
        }
        double run_cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID) - cpu;
        double run_wall = milliseconds(CLOCK_MONOTONIC) - wall;
        wall = milliseconds(CLOCK_MONOTONIC);
        cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID);
        BundleInstanceClose(instance);
        double close_cpu = milliseconds(CLOCK_PROCESS_CPUTIME_ID) - cpu;
        double close_wall = milliseconds(CLOCK_MONOTONIC) - wall;
        double values[] = {open_wall, open_cpu, run_wall, run_cpu, close_wall, close_cpu};
        for(int phase = 0; phase < 6; phase++) {
            if(run == 0) first[phase] = values[phase];
            else samples[phase][run - 1] = values[phase];
        }
    }
    BundleClose(bundle);
    printf("{\"load_wall_ms\":%.9f,\"load_cpu_ms\":%.9f,\"runs\":%ld,\"result\":%lld",
           load_wall, load_cpu, runs, expected);
    const char *names[] = {"instantiate_wall", "instantiate_cpu", "run_wall", "run_cpu",
                           "close_wall", "close_cpu"};
    for(int phase = 0; phase < 6; phase++) {
        qsort(samples[phase], (size_t)runs, sizeof(double), ordered);
        double median = runs % 2 ? samples[phase][runs / 2] :
            (samples[phase][runs / 2 - 1] + samples[phase][runs / 2]) * 0.5;
        printf(",\"first_%s_ms\":%.9f,\"%s_ms\":%.9f", names[phase], first[phase], names[phase], median);
        free(samples[phase]);
    }
    puts("}");
    return 0;
}
