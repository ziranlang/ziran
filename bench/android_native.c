/* Internal kernel timing; ADB and process startup are outside the interval. */
#include "kernel.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>

static double milliseconds(void)
{
    struct timespec value;
    if(clock_gettime(CLOCK_MONOTONIC, &value)) abort();
    return value.tv_sec * 1000.0 + value.tv_nsec / 1000000.0;
}

static double cpu(struct timeval value)
{
    return value.tv_sec * 1000.0 + value.tv_usec / 1000.0;
}

int main(int argc, char **argv)
{
    if(argc != 3) return 2;
    char *end;
    errno = 0;
    int64_t rounds = strtoll(argv[1], &end, 10);
    if(errno || *end || rounds < 1) return 2;
    errno = 0;
    uint64_t seed = strtoull(argv[2], &end, 10);
    if(errno || *end) return 2;
    struct rusage before, after;
    if(getrusage(RUSAGE_SELF, &before)) return 2;
    double started = milliseconds();
    uint64_t result = Kernel(rounds, seed);
    double elapsed = milliseconds() - started;
    if(getrusage(RUSAGE_SELF, &after)) return 2;
    printf("{\"result\":%" PRIu64 ",\"elapsed_ms\":%.9f,\"user_ms\":%.9f,"
           "\"system_ms\":%.9f,\"max_rss_kib\":%ld}\n", result, elapsed,
           cpu(after.ru_utime) - cpu(before.ru_utime),
           cpu(after.ru_stime) - cpu(before.ru_stime), after.ru_maxrss);
    return 0;
}
