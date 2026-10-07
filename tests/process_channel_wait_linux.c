#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int mode, sleep_calls, poll_calls, open_calls, interruptions, packet = -1;

int64_t channel_wait_now(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value)) abort();
    return (int64_t)value.tv_sec * 1000000000 + value.tv_nsec;
}

int channel_wait_spawn(int delay_us) {
    int descriptors[2];
    if (pipe(descriptors)) return -1;
    pid_t pid = fork();
    if (pid < 0) { close(descriptors[0]); close(descriptors[1]); return -1; }
    if (!pid) {
        close(descriptors[0]);
        if (setpgid(0, 0)) _exit(127);
        struct timespec delay = { .tv_nsec = delay_us * 1000L };
        while (nanosleep(&delay, &delay) && errno == EINTR) {}
        int64_t stamp = channel_wait_now();
        if (write(descriptors[1], &stamp, sizeof stamp) != sizeof stamp) _exit(127);
        close(descriptors[1]);
        _exit(7);
    }
    close(descriptors[1]);
    packet = descriptors[0];
    return pid;
}

void channel_wait_release(void) { if (packet >= 0) close(packet); packet = -1; }
int64_t channel_wait_exit_time(void) {
    int64_t stamp;
    if (read(packet, &stamp, sizeof stamp) != sizeof stamp) abort();
    channel_wait_release();
    return stamp;
}
void channel_wait_configure(int value) {
    mode = value; sleep_calls = poll_calls = open_calls = interruptions = 0;
}
int channel_wait_sleeps(void) { return sleep_calls; }
int channel_wait_polls(void) { return poll_calls; }
int channel_wait_opens(void) { return open_calls; }
int channel_wait_interrupts(void) { return interruptions; }
int channel_wait_descriptors(void) {
    DIR *directory = opendir("/proc/self/fd");
    if (!directory) abort();
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(directory))) if (entry->d_name[0] != '.') count++;
    closedir(directory);
    return count;
}

long __real_syscall(long, ...);
long __wrap_syscall(long number, long pid, unsigned long flags) {
    if (number != SYS_pidfd_open || flags) abort();
    open_calls++;
    if (mode == 1 || mode == 2) { errno = mode == 1 ? ENOSYS : EPERM; return -1; }
    return __real_syscall(number, pid, flags);
}
int __real_poll(struct pollfd *, nfds_t, int);
int __wrap_poll(struct pollfd *fds, nfds_t count, int timeout) {
    poll_calls++;
    if (mode == 3) {
        struct timespec delay = { .tv_nsec = 2000000 };
        nanosleep(&delay, NULL);
        interruptions++; errno = EINTR; return -1;
    }
    return __real_poll(fds, count, timeout);
}
int __real_clock_nanosleep(clockid_t, int, const struct timespec *, struct timespec *);
int __wrap_clock_nanosleep(clockid_t clock, int flags, const struct timespec *request,
                           struct timespec *remaining) {
    sleep_calls++;
    return __real_clock_nanosleep(clock, flags, request, remaining);
}
