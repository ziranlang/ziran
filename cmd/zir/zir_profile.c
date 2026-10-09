#include "zir_profile.h"
#include "zir_diagnostic.h"
#include <errno.h>
#include <stdatomic.h>
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
    FILE *output;
    uint64_t started, allocation_calls, allocation_bytes;
    ProfileSample phases[32], counters[32];
    size_t phase_count, counter_count;
} profile;
static atomic_flag profile_lock = ATOMIC_FLAG_INIT;
/* 0: not initialized, 1: enabled, 2: disabled. Disabled profiling stays cheap. */
static atomic_int profile_state;

static void lock_profile(void)
{
    while(atomic_flag_test_and_set_explicit(&profile_lock, memory_order_acquire)) {}
}

static void unlock_profile(void)
{
    atomic_flag_clear_explicit(&profile_lock, memory_order_release);
}

static int disabled(void)
{
    return atomic_load_explicit(&profile_state, memory_order_acquire) == 2;
}

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
    lock_profile();
    if(profile.output == NULL) { unlock_profile(); return; }
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
    int failed = 0;
    if(used > 0 && (size_t)used < sizeof(record)) {
        /* One append keeps concurrent compiler processes' JSON lines intact. */
        int written = write_record(profile.output, record, used);
        failed = written != used;
    }
    fclose(profile.output);
    profile.output = NULL;
    atomic_store_explicit(&profile_state, 2, memory_order_release);
    unlock_profile();
    if(failed)
        Diagnostic(Span("", 0, 0), "zir.output", "cannot finish compiler profile");
}

/* Called only with profile_lock held. */
static int enabled(void)
{
    if(atomic_load_explicit(&profile_state, memory_order_relaxed) == 0) {
        const char *path = getenv("ZIRAN_PROFILE");
        if(path != NULL && *path) {
            profile.output = fopen(path, "ab");
            if(profile.output == NULL) {
                /* Reporting the failure can allocate a diagnostic buffer.
                 * Disable first so that allocation cannot retry initialization. */
                atomic_store_explicit(&profile_state, 2, memory_order_release);
                unlock_profile();
                Diagnostic(Span(path, 1, 1), "zir.output", "cannot open compiler profile");
                exit(1);
            }
            profile.started = now();
            atexit(report);
        }
        atomic_store_explicit(&profile_state, profile.output != NULL ? 1 : 2, memory_order_release);
    }
    return profile.output != NULL;
}

uint64_t ProfileStart(void)
{
    if(disabled()) return 0;
    lock_profile();
    uint64_t started = enabled() ? now() : 0;
    unlock_profile();
    return started;
}

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
    if(started == 0 || disabled()) return;
    lock_profile();
    ProfileSample *value = enabled() ? sample(profile.phases, &profile.phase_count, phase) : NULL;
    if(value != NULL) {
        uint64_t finished = now();
        value->calls++;
        if(finished >= started) value->elapsed_ns += finished - started;
    }
    unlock_profile();
}

void ProfileAllocation(size_t bytes)
{
    if(disabled()) return;
    lock_profile();
    if(enabled()) {
        profile.allocation_calls++;
        profile.allocation_bytes += bytes;
    }
    unlock_profile();
}

void ProfileCount(const char *counter)
{
    if(disabled()) return;
    lock_profile();
    ProfileSample *value = enabled() ? sample(profile.counters, &profile.counter_count, counter) : NULL;
    if(value != NULL) value->calls++;
    unlock_profile();
}
