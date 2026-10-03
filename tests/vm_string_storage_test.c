#include "zir_bundle.h"
#include "zir_vm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Probe { VmInstance *instance; size_t peak; int calls; } Probe;

static int host_call(void *context, const char *module, const char *function,
                     const VmHostValue *args, int count, VmHostValue *result)
{
    Probe *probe = context;
    assert(!strcmp(module, "string_storage"));
    if(!strcmp(function, "Tail")) {
        assert(count == 1 && args[0].kind == VM_HOST_STRING && args[0].length >= 5);
        result->kind = VM_HOST_STRING;
        result->data = args[0].data + 1;
        result->length = 4;
    } else {
        assert(!strcmp(function, "Measure") && count == 0);
        size_t bytes = VmInstanceLiveValueBytes(probe->instance);
        if(bytes > probe->peak) probe->peak = bytes;
        probe->calls++;
        assert(bytes < 8 * 1024 * 1024);
        result->kind = VM_HOST_INTEGER;
        result->integer = 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZibLawTable laws = {0};
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL);
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                     entry, sizeof(entry), &laws);
    fclose(input);
    assert(program != NULL);
    Probe probe = {0};
    probe.instance = VmInstanceOpen(program, module, entry, host_call, &probe);
    assert(probe.instance != NULL);
    for(int run = 0; run < 3; run++) {
        long long result = 0;
        int has_result = 0;
        assert(VmInstanceRun(probe.instance, &result, &has_result));
        assert(has_result && result == 42);
    }
    assert(probe.calls == 300 && probe.peak > 1024);
    VmInstanceClose(probe.instance);
    ZibLawTableFree(&laws);
    ProgramFree(program);
    return 0;
}
