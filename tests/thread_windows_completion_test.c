#include <windows.h>
#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "thread_linux_test.h"

static HANDLE entered, released, polled, poll_ready, poll_start;
static DWORD cleanup_key;
static Thread worker;
static int value, cleanup_case;
static bool poll_result;

static void Hold(void *argument)
{
    (void)argument;
    if (!SetEvent(entered) || WaitForSingleObject(released, 5000) != WAIT_OBJECT_0)
        abort();
}

static void CALLBACK Cleanup(void *argument)
{
    Hold(argument);
}

static void Work(void *argument)
{
    *(int *)argument = 42;
    if (cleanup_case && !FlsSetValue(cleanup_key, argument)) abort();
}

static unsigned __stdcall Poll(void *argument)
{
    (void)argument;
    if (!SetEvent(poll_ready) || WaitForSingleObject(poll_start, 5000) != WAIT_OBJECT_0)
        abort();
    poll_result = ThreadFinished(&worker);
    if (!SetEvent(polled)) abort();
    return 0;
}

static int HeldCompletion(int kind)
{
    if (!ResetEvent(entered) || !ResetEvent(released) || !ResetEvent(polled) ||
        !ResetEvent(poll_ready) || !ResetEvent(poll_start)) return 71;
    cleanup_case = kind;
    value = 0;
    poll_result = true;
    // Creating a thread during held DLL/TLS exit can wait for the loader.
    // Start the observer first, so the watchdog measures only Finished.
    HANDLE owner = (HANDLE)_beginthreadex(NULL, 0, Poll, NULL, 0, NULL);
    if (!owner || WaitForSingleObject(poll_ready, 5000) != WAIT_OBJECT_0) return 74;
    ThreadNotification notification = {kind ? NULL : Hold, NULL};
    if (!ThreadStart(&worker, Work, &value, notification)) return 72;
    if (WaitForSingleObject(entered, 5000) != WAIT_OBJECT_0) return 73;
    if (!SetEvent(poll_start)) return 75;
    bool blocked = WaitForSingleObject(polled, 2000) != WAIT_OBJECT_0;
    if (!SetEvent(released)) return 75;
    if (WaitForSingleObject(owner, 5000) != WAIT_OBJECT_0 || !CloseHandle(owner)) return 76;
    ThreadWait(&worker);
    if (blocked || poll_result) {
        fprintf(stderr, "ThreadFinished with held %s: blocked=%d, completed=%d\n",
                kind ? "TLS cleanup" : "notification", blocked, poll_result);
        return 77;
    }
    if (ThreadRunning(&worker) || worker.handle || ThreadFinished(&worker) || value != 42)
        return 78;
    return 0;
}

int main(void)
{
    if (SelfTest() != 42) return 70;
    entered = CreateEventW(NULL, TRUE, FALSE, NULL);
    released = CreateEventW(NULL, TRUE, FALSE, NULL);
    polled = CreateEventW(NULL, TRUE, FALSE, NULL);
    poll_ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    poll_start = CreateEventW(NULL, TRUE, FALSE, NULL);
    cleanup_key = FlsAlloc(Cleanup);
    if (!entered || !released || !polled || !poll_ready || !poll_start ||
        cleanup_key == FLS_OUT_OF_INDEXES) return 79;
    DWORD before, after;
    if (!GetProcessHandleCount(GetCurrentProcess(), &before)) return 80;
    for (int iteration = 0; iteration < 16; ++iteration) {
        for (int kind = 0; kind < 2; ++kind) {
            int result = HeldCompletion(kind);
            if (result) return result;
        }
    }
    if (!GetProcessHandleCount(GetCurrentProcess(), &after) || after != before) return 81;
    if (!FlsFree(cleanup_key) || !CloseHandle(entered) || !CloseHandle(released) ||
        !CloseHandle(polled) || !CloseHandle(poll_ready) || !CloseHandle(poll_start))
        return 82;
    puts("PASS: Win64 completion polling stays nonblocking through notifications and TLS cleanup; results, reuse and handle balance pass.");
    return 0;
}
