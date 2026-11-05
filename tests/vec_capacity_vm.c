#include "zir_bundle.h"
#include "zir_vm.h"
#include "zir_vm_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static size_t fail_size;
static int failures;
void *__real_calloc(size_t, size_t);
void *__wrap_calloc(size_t count, size_t size) {
    if(fail_size && count == 1 && size == fail_size) {
        fail_size = 0; failures++; return NULL;
    }
    return __real_calloc(count, size);
}
static int host(void *context, const char *module, const char *function,
                const VmHostValue *args, int count, VmHostValue *result) {
    (void)context;
    if(strcmp(module, "failure") || strcmp(function, "ArmAllocator") || count != 1 ||
       args[0].kind != VM_HOST_INTEGER || args[0].integer < 0 || args[0].integer > 3)
        return 0;
    /* Fail only the storage allocation under test, not evaluator scratch. */
    switch(args[0].integer) {
    case 0: fail_size = sizeof(Array) + 128 + 16; break;
    case 1: fail_size = sizeof(Array) + 128 * sizeof(Value); break;
    case 2: fail_size = sizeof(Array) + 126 * sizeof(Value); break;
    case 3: fail_size = sizeof(Record) + 2 * sizeof(RecordField); break;
    }
    result->kind = VM_HOST_VOID;
    return 1;
}

int main(int argc, char **argv)
{
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZxLawTable laws = {0};
    assert(argc == 2 || argc == 3);
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL);
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                     entry, sizeof(entry), &laws, NULL);
    fclose(input);
    assert(program != NULL);
    VmInstance *instance = VmInstanceOpen(program, module, entry, host, NULL);
    assert(instance != NULL);
    if(argc == 3) {
        long long result; int present;
        int ran = VmInstanceRun(instance, &result, &present);
        VmInstanceClose(instance);
        ZxLawTableFree(&laws);
        ProgramFree(program);
        return ran ? 1 : 0;
    }
    for(int run = 0; run < 3; run++) {
        long long result = -1;
        int present = 0;
        failures = 0;
        assert(VmInstanceRun(instance, &result, &present));
        if(!present || result != 42) {
            fprintf(stderr, "VM %s result=%lld present=%d\n", entry, result, present);
            return 1;
        }
        if(!strcmp(entry, "Check") || !strcmp(entry, "BoundedCheck")) assert(failures == 7 && fail_size == 0);
        if(!strcmp(entry, "Budget"))
            assert(VmInstanceLiveValueBytes(instance) == 0);
    }
    VmInstanceClose(instance);
    ZxLawTableFree(&laws);
    ProgramFree(program);
    return 0;
}
