#include "zir_bundle.h"
#include "zir_vm.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct State { unsigned char bytes[3]; int oversized; } State;

static int host_call(void *context, const char *module, const char *function,
                     const VmHostValue *args, int count, VmHostValue *result)
{
    State *state = context;
    assert(strcmp(module, "host_strings") == 0);
    /* All results borrow one scratch buffer, overwritten on every call. */
    memset(state->bytes, 'x', sizeof(state->bytes));
    if(strcmp(function, "Clobber") == 0) {
        assert(count == 0);
        result->kind = VM_HOST_VOID;
        return 1;
    }
    memcpy(state->bytes, "a\0b", 3);
    VmHostValue text = {.kind = VM_HOST_STRING, .type = "string",
        .data = state->bytes, .length = state->oversized ? SIZE_MAX : 3};
    if(strcmp(function, "ReadText") == 0) {
        assert(count == 0);
        *result = text;
    } else if(strcmp(function, "Empty") == 0) {
        assert(count == 0);
        *result = (VmHostValue){.kind = VM_HOST_STRING, .type = "string"};
    } else if(strcmp(function, "Replace") == 0) {
        assert(count == 1 && args[0].kind == VM_HOST_SLICE && args[0].length == 1);
        args[0].elements[0] = text;
        result->kind = VM_HOST_VOID;
    } else {
        static VmHostValue elements[2];
        elements[0] = text;
        elements[1] = (VmHostValue){.kind = VM_HOST_STRING, .type = "string",
            .data = state->bytes + 2, .length = 1};
        if(strcmp(function, "ReadView") == 0) {
            assert(count == 0);
            *result = (VmHostValue){.kind = VM_HOST_SLICE, .type = "[]string",
                .elements = elements, .length = 2};
        } else {
            assert(strcmp(function, "ReadRow") == 0 && count == 0);
            static VmHostField fields[2];
            fields[0] = (VmHostField){"title", text};
            fields[1] = (VmHostField){"labels", {.kind = VM_HOST_ARRAY,
                .type = "[2]string", .elements = elements, .length = 2}};
            *result = (VmHostValue){.kind = VM_HOST_RECORD, .type = "Row",
                .fields = fields, .field_count = 2};
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZxLawTable laws = {0};
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL);
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                     entry, sizeof(entry), &laws, NULL);
    fclose(input);
    assert(program != NULL);
    State state = {0};
    for(int named = 0; named < 2; named++) {
        VmInstance *instance = VmInstanceOpen(program, module, entry, host_call, &state);
        assert(instance != NULL);
        VmInstanceMatchFieldsByName(instance, named);
        size_t previous = 0;
        int collected = 0;
        for(int repeat = 0; repeat < 10000; repeat++) {
            long long result = 0;
            int present = 0;
            assert(VmInstanceRun(instance, &result, &present));
            assert(present && result == 42);
            /* Global roots survive another run; discarded host strings collect. */
            size_t bytes = VmInstanceLiveValueBytes(instance);
            assert(bytes < 8 * 1024 * 1024);
            if(bytes < previous) collected = 1;
            previous = bytes;
        }
        assert(collected);
        VmInstanceClose(instance);
    }
    state.oversized = 1;
    long long result = 0;
    int present = 0;
    assert(!VmRunWithHost(program, module, entry, host_call, &state, &result, &present));
    ZxLawTableFree(&laws);
    ProgramFree(program);
    return 0;
}
