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
    if(strcmp(module, "vm_packed_arrays_test") || strcmp(function, "Measure") || count)
        return 0;
    size_t bytes = VmInstanceLiveValueBytes(probe->instance);
    if(bytes > probe->peak) probe->peak = bytes;
    probe->samples++;
    result->kind = VM_HOST_INTEGER;
    result->integer = (long long)bytes;
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
    long long answer = 0;
    int present = 0;
    if(valid) valid = VmInstanceRun(probe.instance, &answer, &present) && present && answer == 42;
    printf("{\"valid\":%s,\"answer\":%lld,\"samples\":%u,\"peak_live_value_bytes\":%zu}\n",
           valid ? "true" : "false", answer, probe.samples, probe.peak);
    if(!strcmp(entry, "Capacity"))
        valid = valid && probe.samples == 1 && probe.peak > 1048576 && probe.peak < 1572864;
    else
        valid = valid && probe.samples == 3 && probe.peak > 983040 && probe.peak < 2097152;
    VmInstanceClose(probe.instance);
    ZibLawTableFree(&laws);
    ProgramFree(program);
    return valid ? 0 : 1;
}
