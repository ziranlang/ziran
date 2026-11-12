/* Runs a revision-28 executable .zx through the verified-LIR portable engine
 * with the same arguments, Input host ABI and JSON row as zx_compare_host.c. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "compiler_exec_link.h"
#include "compiler_exec.h"
#include "compiler_lowered_ir.h"

static int64_t inputs[3];

static bool input(void *closure, void *context, int32_t function, Slice arguments, uint64_t *result) {
    (void)closure; (void)context; (void)function;
    if(arguments.length != 1) return false;
    uint64_t key = ((const uint64_t *)arguments.data)[0];
    if(key > 2) return false;
    *result = (uint64_t)inputs[key];
    return true;
}

static double ms(void) {
    struct timespec t; if(clock_gettime(CLOCK_MONOTONIC, &t)) abort();
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static unsigned char *read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb"); if(!file) return NULL;
    unsigned char *bytes = NULL; size_t used = 0, capacity = 0; size_t got;
    do {
        if(used == capacity) {
            capacity = capacity ? capacity * 2 : 65536;
            unsigned char *grown = realloc(bytes, capacity);
            if(!grown) { free(bytes); fclose(file); return NULL; }
            bytes = grown;
        }
        got = fread(bytes + used, 1, capacity - used, file); used += got;
    } while(got > 0);
    int failed = ferror(file); fclose(file);
    if(failed) { free(bytes); return NULL; }
    *size = used; return bytes;
}

static int run(LirExecInstance *instance, long long *value) {
    LirExecResult result = compiler_exec_LirExecRun(instance);
    if(result.problem != LirExecProblem_None) {
        String name = compiler_exec_LirExecProblemName(result.problem);
        fprintf(stderr, "lowered run failed: %.*s\n", (int)name.length, name.data);
        return 0;
    }
    *value = (long long)result.value.bits; return 1;
}

int main(int argc, char **argv) {
    if(argc != 8 || strcmp(argv[1], "lowered")) return 2;
    // lowered bundle seed rounds kind batches expected-checksum
    int64_t seed = atoll(argv[3]), rounds = atoll(argv[4]), kind = atoll(argv[5]);
    int batches = atoi(argv[6]); long long expected = atoll(argv[7]);
    if(seed < 0 || seed > 1000000000 || rounds < 1 || rounds > 100000 || kind < 0 || kind > 1 || batches < 1 || batches > 10000) return 2;
    double start = ms();
    size_t size = 0; unsigned char *bytes = read_file(argv[2], &size); if(!bytes) return 3;
    ZxExecOwner owner = {0};
    ZxExecResult read = compiler_exec_link_ZxExecRead(&owner, (String){(char *)bytes, size}, compiler_lowered_ir_LirDefaultLimits());
    free(bytes);
    if(read.problem != ZxExecProblem_None) return 3;
    LirProgram program = compiler_exec_link_ZxExecReadProgram(&owner);
    const LirFunction *functions = program.functions.data;
    int32_t host = -1;
    for(int64_t index = 0; index < program.functions.length; index++)
        if(functions[index].external && functions[index].name.length == 5 && !memcmp(functions[index].name.data, "Input", 5))
            host = (int32_t)index;
    if(host < 0) return 3;
    double loaded = ms();
    LirExecPlan plan = {0}; LirExecInstance instance = {0};
    LirExecLimits limits = compiler_exec_LirExecDefaultLimits();
    // Like the old VM and TinyCC runs being compared, a run has no fuel budget.
    limits.work = INT64_MAX; limits.fuel = INT64_MAX;
    if(compiler_exec_LirExecPlanBuild(program, compiler_lowered_ir_LirDefaultLimits(), &plan).problem != LirExecProblem_None ||
       compiler_exec_LirExecInstanceOpen(&plan, limits, 1, &instance).problem != LirExecProblem_None ||
       compiler_exec_LirExecBindHost(&instance, host, (LirExecHostCall){NULL, input}, NULL) != LirExecProblem_None) return 4;
    inputs[0] = seed; inputs[1] = rounds; inputs[2] = kind;
    double opened = ms();
    long long checksum = 0; double first = 0, warm_ms = 0, measured = 0; int warmup_count = 0;
    for(int j = -1; j < batches; j++) {
        long long value; double begin = ms();
        inputs[0] = seed + (j < 0 ? 0 : j);
        if(!run(&instance, &value)) return 5;
        if(j < 0) {
            first = ms() - begin; double wb = ms();
            do {
                inputs[0] = seed + (warmup_count & 1023);
                if(!run(&instance, &value)) return 5;
                warmup_count++;
            } while(ms() - wb < 500);
            warm_ms = ms() - wb; measured = ms();
        } else checksum += value;
    }
    double end = ms();
    fprintf(stderr, "benchmark warmup_count=%d warmup_ms=%.6f\n", warmup_count, warm_ms);
    fprintf(stderr, "ziran: lowered portable engine executed revision-28 instructions\n");
    if(checksum != expected) { fprintf(stderr, "checksum %lld != %lld\n", checksum, expected); return 6; }
    printf("{\"load_ms\":%.9f,\"instantiate_ms\":%.9f,\"first_ms\":%.9f,\"warm_ms\":%.9f,\"checksum\":%lld,\"batches\":%d}\n", loaded-start, opened-loaded, first, end-measured, checksum, batches);
    compiler_exec_LirExecInstanceClose(&instance);
    compiler_exec_LirExecPlanFree(&plan);
    compiler_exec_link_ZxExecReadFree(&owner);
    return 0;
}
