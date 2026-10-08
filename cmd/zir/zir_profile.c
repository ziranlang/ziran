#include "zir_profile.h"
#include "zir_diagnostic.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define PSAPI_VERSION 2
#include <windows.h>
#include <psapi.h>
#include <io.h>
#include <process.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

typedef struct ProfileSample {
    const char *name;
    uint64_t calls, elapsed_ns;
} ProfileSample;
static struct {
    int initialized;
    FILE *output;
    uint64_t started, allocation_calls, allocation_bytes;
    ProfileSample phases[32], counters[32];
    size_t phase_count, counter_count;
} profile;

static uint64_t now(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency, value;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !QueryPerformanceCounter(&value) || value.QuadPart < 0)
        return 0;
    uint64_t ticks = (uint64_t)value.QuadPart;
    uint64_t rate = (uint64_t)frequency.QuadPart;
    return ticks / rate * UINT64_C(1000000000) +
        ticks % rate * UINT64_C(1000000000) / rate;
#else
    struct timespec value;
    if(clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0;
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + (uint64_t)value.tv_nsec;
#endif
}

static long maximum_rss_kib(void)
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS usage = {0};
    usage.cb = sizeof(usage);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &usage, sizeof(usage)))
        return 0;
    return (long)(usage.PeakWorkingSetSize / 1024);
#else
    struct rusage usage = {0};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        return 0;
#ifdef __APPLE__
    return usage.ru_maxrss / 1024;
#else
    return usage.ru_maxrss;
#endif
#endif
}

static long process_id(void)
{
#ifdef _WIN32
    return (long)_getpid();
#else
    return (long)getpid();
#endif
}

static int write_record(FILE *output, const char *record, int size)
{
    int written;
    do {
#ifdef _WIN32
        written = _write(_fileno(output), record, (unsigned int)size);
#else
        written = (int)write(fileno(output), record, (size_t)size);
#endif
    } while (written < 0 && errno == EINTR);
    return written;
}

static void report(void)
{
    if(profile.output == NULL) return;
    char record[8192];
    long rss = maximum_rss_kib();
    uint64_t finished = now();
    uint64_t elapsed = finished >= profile.started ? finished - profile.started : 0;
    int used = snprintf(record, sizeof(record),
        "{\"schema_version\":1,\"pid\":%ld,\"profile_elapsed_ms\":%.6f,"
        "\"max_rss_kib\":%ld,\"workspace_allocation_calls\":%llu,"
        "\"workspace_allocation_bytes\":%llu,\"phases\":[",
        process_id(), elapsed / 1e6, rss,
        (unsigned long long)profile.allocation_calls,
        (unsigned long long)profile.allocation_bytes);
    for(size_t i = 0; i < profile.phase_count && used > 0 && (size_t)used < sizeof(record); i++) {
        const ProfileSample *value = &profile.phases[i];
        used += snprintf(record + used, sizeof(record) - (size_t)used,
            "%s{\"name\":\"%s\",\"calls\":%llu,\"inclusive_ms\":%.6f}",
            i ? "," : "", value->name, (unsigned long long)value->calls,
            value->elapsed_ns / 1e6);
    }
    if(used > 0 && (size_t)used < sizeof(record))
        used += snprintf(record + used, sizeof(record) - (size_t)used, "],\"counters\":{");
    for(size_t i = 0; i < profile.counter_count && used > 0 && (size_t)used < sizeof(record); i++) {
        const ProfileSample *value = &profile.counters[i];
        used += snprintf(record + used, sizeof(record) - (size_t)used,
                         "%s\"%s\":%llu", i ? "," : "", value->name,
                         (unsigned long long)value->calls);
    }
    if(used > 0 && (size_t)used < sizeof(record))
        used += snprintf(record + used, sizeof(record) - (size_t)used, "}}\n");
    if(used > 0 && (size_t)used < sizeof(record)) {
        /* One append keeps concurrent compiler processes' JSON lines intact. */
        int written = write_record(profile.output, record, used);
        if(written != used)
            Diagnostic(Span("", 0, 0), "zir.output", "cannot finish compiler profile");
    }
    fclose(profile.output);
    profile.output = NULL;
}

static int enabled(void)
{
    if(!profile.initialized) {
        profile.initialized = 1;
        const char *path = getenv("ZIRAN_PROFILE");
        if(path != NULL && *path) {
            profile.output = fopen(path, "ab");
            if(profile.output == NULL) {
                Diagnostic(Span(path, 1, 1), "zir.output", "cannot open compiler profile");
                exit(1);
            }
            profile.started = now();
            atexit(report);
        }
    }
    return profile.output != NULL;
}

uint64_t ProfileStart(void) { return enabled() ? now() : 0; }

static ProfileSample *sample(ProfileSample *items, size_t *count, const char *name)
{
    for(size_t i = 0; i < *count; i++)
        if(!strcmp(items[i].name, name)) return &items[i];
    if(*count == 32) return NULL;
    ProfileSample *value = &items[(*count)++];
    value->name = name;
    return value;
}

void ProfileEnd(const char *phase, uint64_t started)
{
    if(started == 0 || !enabled()) return;
    ProfileSample *value = sample(profile.phases, &profile.phase_count, phase);
    if(value != NULL) {
        uint64_t finished = now();
        value->calls++;
        if(finished >= started) value->elapsed_ns += finished - started;
    }
}

void ProfileAllocation(size_t bytes)
{
    if(!enabled()) return;
    profile.allocation_calls++;
    profile.allocation_bytes += bytes;
}

void ProfileCount(const char *counter)
{
    if(!enabled()) return;
    ProfileSample *value = sample(profile.counters, &profile.counter_count, counter);
    if(value != NULL) value->calls++;
}
