#include "ziran_host.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) && !defined(__EMSCRIPTEN__)
#include <pthread.h>
#define CONCURRENT_BINDINGS 1
#endif

enum { PER_MODULE = 64 };
typedef struct State { size_t calls; } State;
typedef struct Context { unsigned index, group; State *state; } Context;

static int host_call(void *opaque, const char *module, const char *function,
                     const VmHostValue *args, int count, VmHostValue *result)
{
    Context *context = opaque;
    assert(context != NULL); /* Extra bindings must never be dispatched. */
    assert(strcmp(module, context->group ? "app" : "provider") == 0);
    char expected[32];
    snprintf(expected, sizeof(expected), "H%03u", context->index);
    assert(strcmp(function, expected) == 0);
    assert(count == 1 && args[0].kind == VM_HOST_INTEGER);
    context->state->calls++;
    result->kind = VM_HOST_INTEGER;
    result->integer = (int64_t)(((uint64_t)args[0].integer * 48271 + context->index + 1 +
                                context->group * 1000) & INT64_MAX);
    return 1;
}

static char *copy_name(const char *name)
{
    size_t size = strlen(name) + 1;
    char *copy = malloc(size);
    assert(copy != NULL);
    memcpy(copy, name, size);
    return copy;
}

static int only_call(void *opaque, const char *module, const char *function,
                      const VmHostValue *args, int count, VmHostValue *result)
{
    unsigned *calls = opaque;
    assert(strcmp(module, "solo") == 0 && strcmp(function, "Only") == 0);
    assert(count == 1 && args[0].kind == VM_HOST_INTEGER);
    assert(args[0].integer == 7 || args[0].integer == 8);
    (*calls)++;
    result->kind = VM_HOST_INTEGER;
    result->integer = args[0].integer + 1;
    return 1;
}

static void single_binding(const char *path)
{
    Bundle *bundle = BundleOpen(path);
    assert(bundle != NULL && BundleCapabilityCount(bundle) == 1);
    unsigned calls = 0;
    HostBinding binding = {"solo", "Only", only_call, &calls}, invalid = binding;
    invalid.module = "wrong";
    assert(BundleInstantiate(bundle, &invalid, 1) == NULL);
    invalid = binding; invalid.function = "wrong";
    assert(BundleInstantiate(bundle, &invalid, 1) == NULL);
    invalid = binding; invalid.call = NULL;
    assert(BundleInstantiate(bundle, &invalid, 1) == NULL && calls == 0);
    long long result = 0;
    int present = 0;
    assert(BundleRun(bundle, &binding, 1, &result, &present));
    assert(present && result == 9 && calls == 2);
    HostBinding ambient[] = {{"_ambient", "Unused", host_call, NULL}, binding};
    assert(BundleRun(bundle, ambient, 2, &result, &present));
    assert(present && result == 9 && calls == 4);
    BundleInstance *instance = BundleInstantiate(bundle, &binding, 1);
    assert(instance != NULL && calls == 5);
    binding.module = binding.function = "changed after instantiate";
    assert(BundleInstanceRun(instance, &result, &present));
    assert(present && result == 9 && calls == 6);
    BundleInstanceClose(instance);
    BundleClose(bundle);
}

#ifdef CONCURRENT_BINDINGS
typedef struct Worker {
    Bundle *bundle;
    long long expected;
    pthread_barrier_t *ready;
} Worker;

static void *run_worker(void *opaque)
{
    Worker *worker = opaque;
    HostBinding bindings[2 * PER_MODULE];
    Context contexts[2 * PER_MODULE];
    State state = {0};
    for(size_t i = 0; i < 2 * PER_MODULE; i++) {
        const char *module = BundleCapabilityModule(worker->bundle, i);
        const char *function = BundleCapabilityFunction(worker->bundle, i);
        contexts[i] = (Context){(unsigned)strtoul(function + 1, NULL, 10),
                                strcmp(module, "app") == 0, &state};
        bindings[2 * PER_MODULE - i - 1] = (HostBinding){module, function, host_call, &contexts[i]};
    }
    pthread_barrier_wait(worker->ready);
    BundleInstance *instance = BundleInstantiate(worker->bundle, bindings, 2 * PER_MODULE);
    assert(instance != NULL && state.calls == 1);
    for(unsigned run = 1; run <= 5; run++) {
        long long result = 0;
        int present = 0;
        assert(BundleInstanceRun(instance, &result, &present));
        assert(present && result == worker->expected && state.calls == 1 + run * 2 * PER_MODULE);
    }
    BundleInstanceClose(instance);
    return NULL;
}

static void concurrent_bindings(Bundle *bundle, long long expected)
{
    enum { WORKERS = 8 };
    pthread_t threads[WORKERS];
    pthread_barrier_t ready;
    assert(pthread_barrier_init(&ready, NULL, WORKERS) == 0);
    Worker worker = {bundle, expected, &ready};
    for(unsigned i = 0; i < WORKERS; i++)
        assert(pthread_create(&threads[i], NULL, run_worker, &worker) == 0);
    for(unsigned i = 0; i < WORKERS; i++)
        assert(pthread_join(threads[i], NULL) == 0);
    pthread_barrier_destroy(&ready);
}
#endif

int main(int argc, char **argv)
{
    if(argc == 2) { single_binding(argv[1]); return 0; }
    assert(argc >= 3 && argc <= 5);
    long long expected = strtoll(argv[2], NULL, 10);
    Bundle *bundle;
    if(argc >= 4) {
        Bundle *leaf = argc == 5 ? BundleOpen(argv[4]) : NULL;
        if(argc == 5) assert(leaf != NULL);
        Bundle *library = leaf != NULL ? BundleOpenWithLibraries(argv[3], &leaf, 1) : BundleOpen(argv[3]);
        BundleClose(leaf);
        assert(library != NULL && BundleCapabilityCount(library) == PER_MODULE);
        bundle = BundleOpenWithLibraries(argv[1], &library, 1);
        BundleClose(library); /* Borrowed capability names follow retained code. */
    } else bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    size_t count = BundleCapabilityCount(bundle);
    assert(count == 2 * PER_MODULE);
    assert(BundleCapabilityCount(NULL) == 0);
    assert(BundleCapabilityModule(NULL, 0) == NULL);
    assert(BundleCapabilityFunction(NULL, 0) == NULL);
    assert(BundleCapabilityModule(bundle, count) == NULL);
    assert(BundleCapabilityFunction(bundle, SIZE_MAX) == NULL);
    HostBinding *bindings = calloc(count + 3, sizeof(*bindings));
    Context *contexts = calloc(count, sizeof(*contexts));
    assert(bindings != NULL && contexts != NULL);
    unsigned char seen[2][PER_MODULE] = {{0}};
    State state = {0};
    for(size_t i = 0; i < count; i++) {
        const char *module = BundleCapabilityModule(bundle, i);
        const char *function = BundleCapabilityFunction(bundle, i);
        assert(module != NULL && function != NULL && function[0] == 'H');
        unsigned group = strcmp(module, "app") == 0;
        assert(group || strcmp(module, "provider") == 0);
        char *end;
        unsigned index = (unsigned)strtoul(function + 1, &end, 10);
        assert(*end == 0 && index < PER_MODULE && !seen[group][index]++);
        contexts[i] = (Context){index, group, &state};
        /* A permutation mixes modules and function names in caller order. */
        bindings[(i * 37 + 5) % count] = (HostBinding){copy_name(module), copy_name(function),
                                                       host_call, &contexts[i]};
    }
    bindings[count] = (HostBinding){"", "", host_call, NULL};
    bindings[count + 1] = (HostBinding){"_ambient", "Unused", host_call, NULL};
    assert(BundleInstantiate(NULL, bindings, count) == NULL);
    assert(BundleInstantiate(bundle, NULL, count) == NULL);
    assert(BundleInstantiate(bundle, bindings, SIZE_MAX) == NULL);
    assert(BundleInstantiate(bundle, bindings + 1, count - 1) == NULL);
    bindings[count + 2] = bindings[17];
    assert(BundleInstantiate(bundle, bindings, count + 3) == NULL);
    bindings[count + 2] = bindings[count];
    assert(BundleInstantiate(bundle, bindings, count + 3) == NULL);
    HostBinding original = bindings[12];
    bindings[12].module = NULL;
    assert(BundleInstantiate(bundle, bindings, count) == NULL);
    bindings[12] = original; bindings[12].function = NULL;
    assert(BundleInstantiate(bundle, bindings, count) == NULL);
    bindings[12] = original; bindings[12].call = NULL;
    assert(BundleInstantiate(bundle, bindings, count) == NULL);
    bindings[12] = original;
    assert(state.calls == 0); /* Reject before the host-backed global initializer. */
    size_t bytes = (count + 2) * sizeof(*bindings);
    HostBinding *snapshot = malloc(bytes);
    assert(snapshot != NULL);
    memcpy(snapshot, bindings, bytes);
    BundleInstance *instance = BundleInstantiate(bundle, bindings, count + 2);
    assert(instance != NULL && state.calls == 1);
    assert(memcmp(snapshot, bindings, bytes) == 0); /* Sorting is private. */
    free(snapshot);
    for(size_t i = 0; i < count; i++) {
        memset((char *)bindings[i].module, 'x', strlen(bindings[i].module));
        memset((char *)bindings[i].function, 'x', strlen(bindings[i].function));
        free((void *)bindings[i].module);
        free((void *)bindings[i].function);
    }
    memset(bindings, 0, bytes);
    free(bindings); /* Names, entries and array must all have been copied. */
    for(unsigned run = 1; run <= 3; run++) {
        long long result = 0;
        int present = 0;
        assert(BundleInstanceRun(instance, &result, &present));
        assert(present && result == expected && state.calls == 1 + run * count);
    }
    BundleInstanceClose(instance);
    free(contexts);
#ifdef CONCURRENT_BINDINGS
    concurrent_bindings(bundle, expected);
#endif
    BundleClose(bundle);
    puts("Host capability identity, validation, copied storage and library lifetimes passed");
    return 0;
}
