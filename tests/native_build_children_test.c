#include "zir_c_build.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <errno.h>
#include <sys/syscall.h>
/* The regular scheduler checks use spawn. This executable also checks the
 * unchanged fork/exec path on a host whose spawn operation is unavailable. */
int __wrap_posix_spawnp(pid_t *pid, const char *file,
                       const posix_spawn_file_actions_t *actions,
                       const posix_spawnattr_t *attributes,
                       char *const argv[], char *const envp[])
{
    (void)pid; (void)file; (void)actions; (void)attributes; (void)argv; (void)envp;
    return ENOSYS;
}
#ifdef SYS_pidfd_open
/* GNU ld wraps only this test executable's syscall references. Model an
 * older kernel while exercising the same scheduler and child ownership. */
long __wrap_syscall(long number, ...)
{
    assert(number == SYS_pidfd_open);
    errno = ENOSYS;
    return -1;
}
#endif
#endif

int main(int argc, char **argv)
{
    assert(argc == 2);
    char path[4096];
    assert(snprintf(path, sizeof(path), "%s/main.c", argv[1]) < (int)sizeof(path));
    FILE *source = fopen(path, "w");
    assert(source && fputs("int main(void) { return 42; }\n", source) >= 0 && fclose(source) == 0);
    assert(setenv("CFLAGS", "-O0", 1) == 0);
    int ready[2];
    assert(pipe(ready) == 0);
    pid_t unrelated = fork();
    assert(unrelated >= 0);
    if(unrelated == 0) {
        close(ready[0]);
        /* Closing the write end at exit lets the parent start compilation
         * with another child's exit waiting to be collected. */
        _exit(73);
    }
    close(ready[1]);
    char byte;
    assert(read(ready[0], &byte, 1) == 0);
    close(ready[0]);
    assert(c_compile_executable(argv[1], "app", 0, 0));
    int status;
    assert(waitpid(unrelated, &status, 0) == unrelated);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 73);
    pid_t app = fork();
    assert(app >= 0);
    if(app == 0) {
        assert(snprintf(path, sizeof(path), "%s/app", argv[1]) < (int)sizeof(path));
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    assert(waitpid(app, &status, 0) == app);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 42);
    puts("Native compiler waits preserve unrelated children");
}
