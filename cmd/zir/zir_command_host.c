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
