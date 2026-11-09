#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef CONTROL
#include "ziran_host.h"
static int64_t inputs[3];
static int input(void *ctx, const char *mod, const char *fn,
                 const VmHostValue *args, int n, VmHostValue *out) {
    (void)ctx; (void)mod; (void)fn;
    if(n != 1 || args[0].kind != VM_HOST_INTEGER || args[0].integer < 0 || args[0].integer > 2) return 0;
    out->kind = VM_HOST_INTEGER; out->type = "s64";
    out->integer = inputs[args[0].integer]; return 1;
}
#else
int64_t Bench(int64_t, int64_t, int64_t);
#endif
static double ms(void) {
    struct timespec t; if(clock_gettime(CLOCK_MONOTONIC, &t)) abort();
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
int main(int argc, char **argv) {
    if(argc != 8) return 2;
    // mode bundle seed rounds kind batches expected-checksum
    int64_t seed = atoll(argv[3]), rounds = atoll(argv[4]), kind = atoll(argv[5]);
    int batches = atoi(argv[6]); long long expected = atoll(argv[7]);
    if(seed < 0 || seed > 1000000000 || rounds < 1 || rounds > 100000 || kind < 0 || kind > 1 || batches < 1 || batches > 10000) return 2;
    double start = ms(), loaded = start, opened = start;
#ifndef CONTROL
    Bundle *bundle = BundleOpen(argv[2]); if(!bundle) return 3;
    loaded = ms();
    HostBinding binding = {"zx_compare", "Input", input, NULL};
    inputs[0] = seed; inputs[1] = rounds; inputs[2] = kind;
    BundleInstance *instance = !strcmp(argv[1], "vm") ?
        BundleInstantiatePortable(bundle, &binding, 1) : BundleInstantiate(bundle, &binding, 1);
    if(!instance) return 4; opened = ms();
#endif
    long long checksum = 0; double first = 0, warm_ms = 0, measured = 0; int warmup_count = 0;
    for(int j = -1; j < batches; j++) {
        long long value; double begin = ms();
#ifdef CONTROL
        value = Bench(seed + (j < 0 ? 0 : j), rounds, kind);
#else
        int present = 0; inputs[0] = seed + (j < 0 ? 0 : j);
        if(!BundleInstanceRun(instance, &value, &present) || !present) return 5;
#endif
        if(j < 0) {
            first = ms() - begin; double wb = ms();
            do {
#ifdef CONTROL
                value = Bench(seed + (warmup_count & 1023), rounds, kind);
#else
                inputs[0] = seed + (warmup_count & 1023);
                if(!BundleInstanceRun(instance, &value, &present) || !present) return 5;
#endif
                warmup_count++;
            } while(ms() - wb < 500);
            warm_ms = ms() - wb; measured = ms();
        } else checksum += value;
    }
    double end = ms();
    fprintf(stderr, "benchmark warmup_count=%d warmup_ms=%.6f\n", warmup_count, warm_ms);
    if(checksum != expected) { fprintf(stderr, "checksum %lld != %lld\n", checksum, expected); return 6; }
    printf("{\"load_ms\":%.9f,\"instantiate_ms\":%.9f,\"first_ms\":%.9f,\"warm_ms\":%.9f,\"checksum\":%lld,\"batches\":%d}\n", loaded-start, opened-loaded, first, end-measured, checksum, batches);
#ifndef CONTROL
    BundleInstanceClose(instance); BundleClose(bundle);
#endif
    return 0;
}
