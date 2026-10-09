#include "zir_bundle.h"
#include "zir_vm.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Probe {
    VmInstance *instance;
    int calls;
} Probe;

static int
host_call(void *context, const char *module, const char *function,
          const VmHostValue *args, int count, VmHostValue *result)
{
    Probe *probe = context;
    (void)args;
    if(strcmp(module, "scope") || strcmp(function, "ProbeHost") || count)
        return 0;
    probe->calls++;
    result->kind = VM_HOST_INTEGER;
    result->integer = (int64_t)VmInstanceLiveValueBytes(probe->instance);
    return 1;
}

int
main(int argc, char **argv)
{
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZibLawTable laws = {0};
    long long result = -1;
    int has_result = 0;
    Probe probe = {0};
    assert(argc == 2);
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL);
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                     entry, sizeof(entry), &laws, NULL);
    fclose(input);
    assert(program != NULL);
    probe.instance = VmInstanceOpen(program, module, entry,
                                    host_call, &probe);
    assert(probe.instance != NULL);
    /* A retained instance repeatedly revisits the same ownership shapes.
     * Every run must release its vectors, including after cached decisions. */
    for(int i = 0; i < 20; i++) {
        probe.calls = 0;
        assert(VmInstanceRun(probe.instance, &result, &has_result));
        assert(has_result && result == 0 && probe.calls == 6);
    }
    VmInstanceClose(probe.instance);
    ZibLawTableFree(&laws);
    ProgramFree(program);
    return 0;
}
