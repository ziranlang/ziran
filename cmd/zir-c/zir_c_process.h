#ifndef ZIR_C_PROCESS_H
#define ZIR_C_PROCESS_H

#include "zir_profile.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if !defined(__ANDROID__) || __ANDROID_API__ >= 28
#include <spawn.h>
extern char **environ;
#endif

/* posix_spawn can launch tools without duplicating the compiler address space.
 * Keep execvp's script handling and the existing path on hosts that cannot
 * spawn. Failed spawn calls have no child to reap. Only our capture pipe is
 * changed; the output lock and the caller's other descriptors retain their
 * existing inheritance rules. */
static pid_t c_start_process(char *const *args, int read_fd, int write_fd, int report_error)
{
    fflush(NULL);
#if !defined(__ANDROID__) || __ANDROID_API__ >= 28
    /* Bionic adds these functions at API 28. Earlier Android hosts retain
     * fork/exec without introducing unavailable dynamic symbol references. */
    posix_spawn_file_actions_t actions;
    int error = posix_spawn_file_actions_init(&actions);
    if(!error) {
        if(read_fd >= 0) error = posix_spawn_file_actions_addclose(&actions, read_fd);
        if(!error && write_fd >= 0) error = posix_spawn_file_actions_adddup2(&actions, write_fd, STDOUT_FILENO);
        if(!error && write_fd >= 0) error = posix_spawn_file_actions_adddup2(&actions, write_fd, STDERR_FILENO);
        if(!error && write_fd > STDERR_FILENO) error = posix_spawn_file_actions_addclose(&actions, write_fd);
        pid_t child = -1;
        if(!error) error = posix_spawnp(&child, args[0], &actions, NULL, args, environ);
        posix_spawn_file_actions_destroy(&actions);
        if(!error) { ProfileCount("native_process_spawn"); return child; }
    }
#endif
    pid_t child = fork();
    if(child == 0) {
        if(read_fd >= 0) close(read_fd);
        if(write_fd >= 0) {
            if(dup2(write_fd, STDOUT_FILENO) < 0 || dup2(write_fd, STDERR_FILENO) < 0) _exit(127);
            if(write_fd > STDERR_FILENO) close(write_fd);
        }
        execvp(args[0], args);
        if(report_error) dprintf(STDERR_FILENO, "cannot run %s: %s\n", args[0], strerror(errno));
        _exit(127);
    }
    if(child > 0) ProfileCount("native_process_fork");
    return child;
}

#endif
