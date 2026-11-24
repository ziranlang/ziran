/*
 * zir_profile_host.c - the OS side of compiler profiling: the monotonic
 * clock, peak RSS, the process id, the one-append record write and the exit
 * hook that flushes the report. The report itself and its opt-in state live
 * in cmd/compiler_profile.zi.
 */
#include "ziran_bootstrap_runtime.h"
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define PSAPI_VERSION 2
#include <windows.h>
#include <psapi.h>
#include <io.h>
#include <process.h>
#else
#include <errno.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

/* Exported by cmd/compiler_profile.zi. */
void ProfileReport(void);

uint64_t
ProfileNow(void)
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

int64_t
ProfileMaxRssKib(void)
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS usage = {0};
    usage.cb = sizeof(usage);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &usage, sizeof(usage)))
        return 0;
    return (int64_t)(usage.PeakWorkingSetSize / 1024);
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

int64_t
ProfilePid(void)
{
#ifdef _WIN32
    return (int64_t)_getpid();
#else
    return (int64_t)getpid();
#endif
}

void
ProfileRegister(void)
{
    atexit(ProfileReport);
}

int64_t
ProfileWrite(void *file, const unsigned char *record, size_t size)
{
    int written;
    do {
#ifdef _WIN32
        written = _write(_fileno(file), record, (unsigned int)size);
#else
        written = (int)write(fileno((FILE *)file), record, size);
#endif
    } while (written < 0 && errno == EINTR);
    return written;
}
