#include "zir_bundle.h"
/* Compare the compiled instruction stream with the canonical sequence over
 * the same checked program; inspect plan presence to reject silent fallback. */
#include "../cmd/zir/zir_vm_eval.c"
#include "../cmd/zir/zir_vm_run.c"
#include <assert.h>

static void
same_value(Value left, Value right)
{
    assert(left.kind == right.kind);
    if(left.kind == VALUE_STRING) {
        assert(left.length == right.length);
        assert(left.length == 0 || !memcmp(left.data, right.data, left.length));
    } else if(left.kind == VALUE_REAL) {
        uint64_t a, b;
        memcpy(&a, &left.real, sizeof(a));
        memcpy(&b, &right.real, sizeof(b));
        assert(a == b);
    } else {
        assert(left.unsigned64 == right.unsigned64);
        assert(integer_bits(left) == integer_bits(right));
    }
}

static Flow
execute(VmInstance *instance, const ZirFunction *function, const Value *args,
         int steps, int compiled, Value *result)
{
    Vm *vm = &instance->vm;
    vm->steps = vm->failed = 0;
    vm->max_steps = steps;
    VmSignature *signature = vm_signature(vm, instance->module, function);
    assert(signature->count == 3);
    Local *locals = calloc((size_t)signature->local_bound, sizeof(*locals));
    assert(locals != NULL);
    Frame frame = {.vm = vm, .module = instance->module, .function = function,
        .locals = locals, .local_count = 3, .local_capacity = signature->local_bound};
    for(int i = 0; i < 3; i++) {
        locals[i].name = signature->parameters[i].name;
        locals[i].type = signature->parameters[i].type;
        locals[i].value = coerce(vm, instance->module, args[i], locals[i].type);
    }
    vm->active_frame = &frame;
    Flow flow;
    *result = int_value(0);
    if(compiled) {
        uint64_t allocation = vm->allocation;
        assert(execute_scalar_function(&frame, signature, result, &flow));
        assert(vm->allocation == allocation);
        assert(signature->scalar_plan != NULL);
        if(!strcmp(function->name, "Large")) assert(signature->scalar_plan->count > 255);
    } else flow = execute_sequence(&frame, 0, function->stmt_count, 0, result);
    if(flow == FLOW_ERROR) vm->failed = 1;
    vm->active_frame = NULL;
    free(locals);
    return flow;
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZibLawTable laws = {0};
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL);
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                    entry, sizeof(entry), &laws, NULL);
    fclose(input);
    assert(program != NULL);
    VmInstance *canonical = VmInstanceOpen(program, module, entry, NULL, NULL);
    VmInstance *compiled = VmInstanceOpen(program, module, entry, NULL, NULL);
    assert(canonical != NULL && compiled != NULL);
    const char *names[] = {"Kernel", "Named", "Numeric", "Skipped", "Failure", "RealFailure", "Large",
        "TextScan", "TextFailure", "TextCopy", "TextCompare"};
    unsigned char bytes[] = {0, 'a', 255, 'b', 0xc3, 0xa9, 0, 13, 'z'};
    const int limits[] = {-1, 0, 1, 2, 7, 16, 23, 64};
    for(size_t n = 0; n < sizeof names / sizeof *names; n++) {
        const ZirModule *owner = NULL;
        const ZirFunction *function = find_entry(program, module, names[n], &owner);
        assert(function != NULL && owner == compiled->module);
        for(size_t l = 0; l < sizeof limits / sizeof *limits; l++) {
            /* Long jump coverage needs a wide body, not many iterations.
             * Check every budget over two iterations; other kernels retain
             * the full input range without quadratic redundant work here. */
            if(!strcmp(names[n], "Large") && limits[l] > 2) continue;
            for(int choose = 0; choose < 2; choose++) {
                Value args[] = {int_value((int64_t)l * 17 - 200), int_value(limits[l]), int_value(choose)};
                if(!strncmp(function->name, "Text", 4)) {
                    /* The same data address changes between calls; retain
                     * current bytes, explicit lengths, NULs and UTF-8. */
                    bytes[1] = (unsigned char)(l + choose * 128);
                    args[0] = string_value(bytes, l % 3 ? sizeof(bytes) : 0);
                }
                Value expected, actual;
                Flow a = execute(canonical, function, args, 100000, 0, &expected);
                int count = canonical->vm.steps;
                Flow b = execute(compiled, function, args, 100000, 1, &actual);
                assert(a == b && canonical->vm.failed == compiled->vm.failed);
                assert(count == compiled->vm.steps);
                if(a == FLOW_RETURN) same_value(expected, actual);
                /* Every budget boundary must stop at the same source step.
                 * Also execute with an unlimited budget after a failed run. */
                for(int budget = 1; budget <= count + 1; budget++) {
                    a = execute(canonical, function, args, budget, 0, &expected);
                    b = execute(compiled, function, args, budget, 1, &actual);
                    assert(a == b && canonical->vm.failed == compiled->vm.failed);
                    assert(canonical->vm.steps == compiled->vm.steps);
                    if(a == FLOW_RETURN) same_value(expected, actual);
                }
                a = execute(canonical, function, args, 0, 0, &expected);
                b = execute(compiled, function, args, 0, 1, &actual);
                assert(a == b && canonical->vm.failed == compiled->vm.failed);
                if(a == FLOW_RETURN) same_value(expected, actual);
            }
        }
    }
    VmInstanceClose(canonical);
    VmInstanceLimitSteps(compiled, 100000);
    compiled->vm.collection_threshold = 1;
    for(int run = 0; run < 3; run++) {
        long long result = 0;
        int present = 0;
        assert(VmInstanceRun(compiled, &result, &present) && present && result == 42);
    }
    const ZirModule *owner = NULL;
    const ZirFunction *fallback = find_entry(program, module, "Fallback", &owner);
    VmSignature *signature = vm_signature(&compiled->vm, owner, fallback);
    assert(signature->scalar_checked && signature->scalar_plan == NULL);
    VmInstanceClose(compiled);
    ZibLawTableFree(&laws);
    ProgramFree(program);
    puts("Procedure plans preserve changing inputs, scoped and named control flow, errors and every step limit");
    return 0;
}
