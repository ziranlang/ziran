/* Process-priority calls for compiler_cli. Compiling is background work:
 * the policy lives in cmd/compiler_cli.zi; this file only reaches the OS. */
#include <errno.h>
#include <stddef.h>

#if (defined(__unix__) || defined(__APPLE__)) && !defined(__EMSCRIPTEN__) && !defined(__wasi__)
#define ZIR_COMMAND_PRIORITY 1
#include <sys/resource.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <unistd.h>
#endif
#endif

int ZiranCommandPriorityAvailable(void)
{
#ifdef ZIR_COMMAND_PRIORITY
    return 1;
#else
    return 0;
#endif
}

/* Returns the niceness; *error is the errno of a failed call, else 0. */
int ZiranCommandPriorityGet(int *error)
{
#ifdef ZIR_COMMAND_PRIORITY
    errno = 0;
    int current = getpriority(PRIO_PROCESS, 0);
    *error = errno;
    return current;
#else
    *error = ENOSYS;
    return 0;
#endif
}

void ZiranCommandPrioritySet(int value)
{
#ifdef ZIR_COMMAND_PRIORITY
    (void)setpriority(PRIO_PROCESS, 0, value);
#else
    (void)value;
#endif
}

void ZiranCommandPriorityIo(int value)
{
#if defined(ZIR_COMMAND_PRIORITY) && defined(__linux__) && defined(SYS_ioprio_set)
    /* Lowest best-effort level for this process; the idle class could
     * stall a build behind a busy disk indefinitely. */
    if(value > 0) (void)syscall(SYS_ioprio_set, 1, 0, (2 << 13) | 7);
#else
    (void)value;
#endif
}

/* Emission workers: pipe, fork, read/write and waitpid with EINTR retried,
 * so the generated driver never reads errno. The zi2c worker path is POSIX;
 * a Windows host has no fork and reports the worker as unavailable. */
#include <string.h>
#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>

int ZiranCommandPipe(int *descriptors, int *error)
{
    if(pipe(descriptors) == 0) return 0;
    if(error) *error = errno;
    return -1;
}

int ZiranCommandFork(int *error)
{
    int process = fork();
    if(process >= 0) return process;
    if(error) *error = errno;
    return -1;
}

/* Bytes read, 0 at end of file, or -1 with *error set. */
long ZiranCommandReadRetry(int descriptor, void *output, size_t count, int *error)
{
    for(;;) {
        ssize_t got = read(descriptor, output, count);
        if(got >= 0) return (long)got;
        if(errno != EINTR) {
            if(error) *error = errno;
            return -1;
        }
    }
}

/* 1 after writing count bytes, 0 on failure. */
int ZiranCommandWriteAll(int descriptor, const void *bytes, size_t count)
{
    const unsigned char *at = bytes;
    while(count > 0) {
        ssize_t wrote = write(descriptor, at, count);
        if(wrote < 0 && errno == EINTR) continue;
        if(wrote <= 0) return 0;
        at += wrote;
        count -= (size_t)wrote;
    }
    return 1;
}

/* The waited process, or -1 after a non-EINTR failure. */
long ZiranCommandWaitRetry(int process, int *status, int *error)
{
    for(;;) {
        pid_t waited = waitpid((pid_t)process, status, 0);
        if(waited >= 0) return (long)waited;
        if(errno != EINTR) {
            if(error) *error = errno;
            return -1;
        }
    }
}

const char *ZiranCommandErrorText(int error)
{
    return strerror(error);
}

int ZiranCommandLastError(void)
{
    return errno;
}

void ZiranCommandClearError(void)
{
    errno = 0;
}
#else
/* A Windows host has no fork/waitpid; zi2c reports the worker unavailable and
 * runs its single-process path. */
int ZiranCommandPipe(int *descriptors, int *error)
{
    (void)descriptors;
    if(error) *error = ENOSYS;
    return -1;
}

int ZiranCommandFork(int *error)
{
    if(error) *error = ENOSYS;
    return -1;
}

long ZiranCommandReadRetry(int descriptor, void *output, size_t count, int *error)
{
    (void)descriptor; (void)output; (void)count;
    if(error) *error = ENOSYS;
    return -1;
}

int ZiranCommandWriteAll(int descriptor, const void *bytes, size_t count)
{
    (void)descriptor; (void)bytes; (void)count;
    return 0;
}

long ZiranCommandWaitRetry(int process, int *status, int *error)
{
    (void)process; (void)status;
    if(error) *error = ENOSYS;
    return -1;
}

const char *ZiranCommandErrorText(int error)
{
    return strerror(error);
}

int ZiranCommandLastError(void)
{
    return errno;
}

void ZiranCommandClearError(void)
{
    errno = 0;
}
#endif
