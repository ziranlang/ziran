#include "zir_bundle.h"
#include "zir_vm_internal.h"
#include <assert.h>

/* Target representation allocations rather than unrelated compiler scratch. */
static int fail_allocation;
static size_t module_slots, function_slots, operand_bytes;
void *__real_calloc(size_t count, size_t size);
void *__real_malloc(size_t size);
void *__real_realloc(void *pointer, size_t size);
static int refuse(int kind)
{
    if(fail_allocation != kind) return 0;
    fail_allocation = 0;
    return 1;
}
void *__wrap_calloc(size_t count, size_t size)
{
    if((count == module_slots && size == sizeof(size_t) && refuse(1)) ||
       (count == function_slots && size == sizeof(VmExecutableFunction) && refuse(2))) return NULL;
    return __real_calloc(count, size);
}
void *__wrap_malloc(size_t size)
{
    if((size == operand_bytes && refuse(3)) ||
       (size == VM_MAX_PARAMS * sizeof(Parameter) && refuse(4)) ||
       (size == VM_MAX_PARAMS * sizeof(VmParameter) && refuse(5))) return NULL;
    return __real_malloc(size);
}
void *__wrap_realloc(void *pointer, size_t size)
{
    if(pointer == NULL && size == VM_MAX_PARAMS * sizeof(VmParameter) && refuse(5)) return NULL;
    return __real_realloc(pointer, size);
}

static void
set_globals(VmInstance *instance, int base)
{
    for(int i = 0; i < instance->vm.global_count; i++) {
        GlobalSlot *slot = &instance->vm.globals[i];
        int value = base + (!strcmp(slot->module->name, "second") ? 100 : 0);
        if(!strcmp(slot->declaration->name, "first_value")) value += 1;
        else if(!strcmp(slot->declaration->name, "second_value")) value += 2;
        else { assert(!strcmp(slot->declaration->name, "third_value")); value += 3; }
        slot->value = int_value(value);
    }
}

static void
check(VmInstance *instance, int expected)
{
    uint32_t *operands = instance->vm.operands;
    VmExecutableFunction *targets = instance->vm.executable_functions;
    size_t count = instance->vm.direct_call_count;
    long long answer; int present;
    assert(VmInstanceRun(instance, &answer, &present) && present && answer == expected);
    assert(instance->vm.operands == operands && instance->vm.executable_functions == targets &&
           instance->vm.direct_call_count == count);
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZxLawTable laws = {0};
    FILE *input = fopen(argv[1], "rb"); assert(input);
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                    entry, sizeof(entry), &laws, NULL);
    fclose(input); assert(program);
    VmInstance *instances[2];
    for(int i = 0; i < 2; i++) {
        instances[i] = VmInstanceOpen(program, module, entry, NULL, NULL);
        assert(instances[i]);
        /* More than the old hash table's total capacity, present before run. */
        assert(instances[i]->vm.direct_call_count > 4096);
        for(int m = 0; m < program->module_count; m++) {
            const ZirModule *owner = &program->modules[m];
            for(int f = 0; f < owner->function_count; f++) {
                const ZirFunction *function = &owner->functions[f];
                const uint32_t *operands = function_code(&instances[i]->vm, owner, function)->operands;
                assert(operands);
                VmSignature *signature = vm_signature(&instances[i]->vm, owner, function);
                assert(signature && signature->count >= 0);
                if(!strcmp(function->name, "One") && !strcmp(owner->name, "first")) {
                    assert(signature->count == 1 && signature->parameters);
                    assert(!strcmp(signature->parameters[0].name, "value"));
                    assert(!strcmp(signature->parameters[0].type, "s32"));
                }
                for(int e = 0; e < function->expr_count; e++) {
                    const ZirExpr *expr = &function->exprs[e];
                    if(expr->kind == ZIR_EXPR_CALL && !expr->slot_type[0])
                        assert(operands[e] < instances[i]->vm.executable_function_count);
                    if(expr->kind == ZIR_EXPR_IDENT && strstr(expr->name, "_value"))
                        assert(operands[e] < (uint32_t)instances[i]->vm.global_count);
                }
            }
        }
        check(instances[i], 4725);
    }
    assert(instances[0]->vm.operands != instances[1]->vm.operands);
    for(int repeat = 0; repeat < 8; repeat++) {
        for(int i = 0; i < 2; i++) {
            int base = repeat * 10 + i * 1000;
            set_globals(instances[i], base);
            check(instances[i], 4725 + base * 6);
        }
    }
    VmInstanceClose(instances[0]);
    check(instances[1], 11145);
    VmInstanceClose(instances[1]);
    /* This entry point is internal: exercise partial preparation without
     * unrelated verifier allocations masking the failure positions. */
    module_slots = (size_t)program->module_count + 1;
    for(int m = 0; m < program->module_count; m++) {
        function_slots += (size_t)program->modules[m].function_count;
        for(int f = 0; f < program->modules[m].function_count; f++)
            operand_bytes += (size_t)program->modules[m].functions[f].expr_count * sizeof(uint32_t);
    }
    CompilerContext *previous = CompilerContextSelect(program->context);
    for(int failure = 1; failure <= 5; failure++) {
        Vm vm = {.program = program};
        fail_allocation = failure;
        assert(!prepare_executable(&vm));
        assert(fail_allocation == 0);
        free_executable(&vm);
        assert(!vm.operands && !vm.executable_functions && !vm.module_functions);
    }
    CompilerContextSelect(previous);
    VmInstance *recovered = VmInstanceOpen(program, module, entry, NULL, NULL);
    assert(recovered); check(recovered, 4725); VmInstanceClose(recovered);
    ZxLawTableFree(&laws); ProgramFree(program);
    puts("Indexed VM operands cover wide call graphs, module identity, live globals, independent lifetimes and failed preparation");
    return 0;
}
