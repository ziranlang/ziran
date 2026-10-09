#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef __cplusplus
#include "thread_linux_test.hpp"
#else
#include "thread_linux_test.h"
#endif

static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static pthread_key_t key;
static Thread worker;
static int entered, released, join_while_held, poll_returned, value, tls_case;
static bool poll_result;
static int mutex_while_held;
#ifdef __cplusplus
static thread_local int polling;
#else
static _Thread_local int polling;
#endif

#ifdef __cplusplus
extern "C" {
#endif
int __real_pthread_join(pthread_t, void **);
int __real_pthread_mutex_lock(pthread_mutex_t *);
int __wrap_pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (polling && tls_case == 2 && mutex == (pthread_mutex_t *)worker.lock) {
        __real_pthread_mutex_lock(&gate);
        mutex_while_held = 1;
        pthread_cond_broadcast(&changed);
        pthread_mutex_unlock(&gate);
    }
    return __real_pthread_mutex_lock(mutex);
}
int __wrap_pthread_join(pthread_t handle, void **result) {
    pthread_mutex_lock(&gate);
    if ((size_t)handle == worker.handle && entered && !released) {
        join_while_held = 1;
        pthread_cond_broadcast(&changed);
    }
    pthread_mutex_unlock(&gate);
    return __real_pthread_join(handle, result);
}
#ifdef __cplusplus
}
#endif

static void Hold(void *argument) {
    (void)argument;
    pthread_mutex_lock(&gate);
    entered = 1;
    pthread_cond_broadcast(&changed);
    while (!released) pthread_cond_wait(&changed, &gate);
    pthread_mutex_unlock(&gate);
}
static void Work(void *argument) {
    *(int *)argument = 42;
    if (tls_case == 1 && pthread_setspecific(key, argument)) abort();
    if (tls_case == 2) Hold(argument);
}
static void *PollFinished(void *argument) {
    (void)argument;
    polling = 1;
    bool result = ThreadFinished(&worker);
    pthread_mutex_lock(&gate);
    poll_result = result;
    poll_returned = 1;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&gate);
    return NULL;
}
static void WaitFor(int *first, int *second) {
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 5;
    while (!*first && (!second || !*second)) {
        if (pthread_cond_timedwait(&changed, &gate, &deadline)) {
            fputs("Thread completion watchdog: test event was not reached\n", stderr);
            exit(70);
        }
    }
}
static int HeldCompletion(int kind) {
    entered = released = join_while_held = mutex_while_held = poll_returned = value = 0;
    poll_result = false;
    tls_case = kind;
    ThreadNotification notification = {kind ? NULL : Hold, NULL};
    if (!ThreadStart(&worker, Work, &value, notification)) return 71;
    pthread_mutex_lock(&gate);
    WaitFor(&entered, NULL);
    pthread_mutex_unlock(&gate);
    if (kind == 2) pthread_mutex_lock((pthread_mutex_t *)worker.lock);
    pthread_t owner;
    if (pthread_create(&owner, NULL, PollFinished, NULL)) return 72;
    pthread_mutex_lock(&gate);
    WaitFor(kind == 2 ? &mutex_while_held : &join_while_held, &poll_returned);
    int violated = join_while_held || mutex_while_held || (poll_returned && poll_result);
    released = 1;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&gate);
    if (kind == 2) pthread_mutex_unlock((pthread_mutex_t *)worker.lock);
    pthread_join(owner, NULL);
    ThreadWait(&worker);
    if (ThreadRunning(&worker) || value != 42) return 73;
    if (violated) {
        fprintf(stderr, "ThreadFinished blocked with held %s\n", kind == 2 ? "completion mutex" : kind == 1 ? "TLS destructor" : "notification");
        return 20;
    }
    return 0;
}
int main(int argc, char **argv) {
    int selected = -1;
    if (argc == 2) {
        char *end;
        long kind = strtol(argv[1], &end, 10);
        if (*end || kind < 0 || kind > 2) return 76;
        selected = (int)kind;
    } else if (argc != 1) return 76;
    if (SelfTest() != 42) return 74;
    if (pthread_key_create(&key, Hold)) return 75;
    for (int iteration = 0; iteration < 8; ++iteration) {
        for (int kind = 0; kind < 3; ++kind) {
            if (selected >= 0 && kind != selected) continue;
            int result = HeldCompletion(kind);
            if (result) return result;
        }
    }
    pthread_key_delete(key);
    puts("PASS: notification/TLS/mutex completion polling is nonblocking; joining, publication and repeated reuse pass.");
    return 0;
}
