#include "zir_bundle.h"
#include "zir_vm.h"
#include <stdio.h>
#include <string.h>

typedef struct Probe {
    VmInstance *instance;
    size_t peak;
    unsigned samples;
} Probe;

static int host_call(void *context, const char *module, const char *function,
                     const VmHostValue *args, int count, VmHostValue *result)
{
    Probe *probe = context;
    (void)args;
    if(strcmp(module, "vm_nested_collection_test") || strcmp(function, "Measure") || count)
        return 0;
    if(probe->instance != NULL) {
        size_t bytes = VmInstanceLiveValueBytes(probe->instance);
        if(bytes > probe->peak) probe->peak = bytes;
        probe->samples++;
    }
    result->kind = VM_HOST_INTEGER;
    result->integer = 0;
    return 1;
}

int main(int argc, char **argv)
{
    if(argc != 2) return 2;
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZibLawTable laws = {0};
    FILE *input = fopen(argv[1], "rb");
    if(input == NULL) return 2;
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                     entry, sizeof(entry), &laws, NULL);
    fclose(input);
    if(program == NULL) return 2;
    Probe probe = {0};
    probe.instance = VmInstanceOpen(program, module, entry, host_call, &probe);
    int valid = probe.instance != NULL;
    for(int run = 0; run < 3 && valid; run++) {
        long long answer = 0;
        int has_result = 0;
        valid = VmInstanceRun(probe.instance, &answer, &has_result) && has_result && answer == 42;
        if(!valid) fprintf(stderr, "Nested collection result: %lld, present: %d\n", answer, has_result);
    }
    // The existing collection interval is 4 MiB. Repeated nested calls must
    // remain within two intervals, rather than accumulate a call's history.
    int bounded = probe.peak < 8 * 1024 * 1024 && probe.peak > 65536 && probe.samples >= 300;
    printf("{\"entry\":\"%s\",\"valid\":%s,\"samples\":%u,\"peak_live_value_bytes\":%zu,\"bounded\":%s}\n",
           entry, valid ? "true" : "false", probe.samples, probe.peak, bounded ? "true" : "false");
    VmInstanceClose(probe.instance);
    ZibLawTableFree(&laws);
    ProgramFree(program);
    return valid && bounded ? 0 : 1;
}
