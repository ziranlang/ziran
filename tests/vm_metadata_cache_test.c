#include "zir_bundle.h"
#include "zir_vm_internal.h"
#include <assert.h>

enum { BUCKETS = 2048, SITES = BUCKETS * 2 + 1 };
static int refuse_cache_storage;

void *__real_calloc(size_t count, size_t size);
void *
__wrap_calloc(size_t count, size_t size)
{
    if(refuse_cache_storage && count == BUCKETS * 2 &&
       (size == sizeof(VmCallSite) || size == sizeof(VmGlobalSite)))
        return NULL;
    return __real_calloc(count, size);
}

/* Select real address collisions without assuming allocator alignment. Three
 * distinct sites also exercise eviction beyond the bounded cache's two ways. */
static ZirExpr *
sites(ZirExprKind kind, int selected[3])
{
    ZirExpr *expressions = calloc(SITES, sizeof(*expressions));
    int *counts = calloc(BUCKETS, sizeof(*counts));
    int *choices = calloc(BUCKETS * 3, sizeof(*choices));
    assert(expressions && counts && choices);
    int found = 0;
    for(int i = 0; i < SITES; i++) {
        size_t slot = vm_expression_slot(&expressions[i], BUCKETS);
        int count = counts[slot]++;
        if(count < 3)
            choices[slot * 3 + count] = i;
        if(count == 2) {
            memcpy(selected, &choices[slot * 3], 3 * sizeof(*selected));
            found = 1;
            break;
        }
    }
    assert(found);
    for(int i = 0; i < 3; i++) {
        expressions[selected[i]] = (ZirExpr){
            .kind = kind, .type = KeepName("s32"), .text = "",
            .slot_type = "", .argument_name = "", .argument_index = -1,
            .left = -1, .right = -1, .third = -1,
            .first_child = -1, .next_sibling = -1
        };
    }
    free(counts);
    free(choices);
    return expressions;
}

static const ZirModule *
module_named(const ZirProgram *program, const char *name)
{
    for(int i = 0; i < program->module_count; i++)
        if(!strcmp(program->modules[i].name, name))
            return &program->modules[i];
    assert(0);
    return NULL;
}

static void
set_global(VmInstance *instance, const ZirModule *module,
           const char *name, int value)
{
    for(int i = 0; i < instance->vm.global_count; i++) {
        GlobalSlot *slot = &instance->vm.globals[i];
        if(slot->module == module && !strcmp(slot->declaration->name, name)) {
            slot->value.integer = value;
            slot->value.bits = (uint64_t)value;
            return;
        }
    }
    assert(0);
}

static void
check(VmInstance *instance, const ZirModule *module, ZirExpr *expressions,
      const int selected[3], int base)
{
    ZirFunction context = module->functions[0];
    context.exprs = expressions;
    context.expr_count = SITES;
    Frame frame = {.vm = &instance->vm, .module = module, .function = &context};
    for(int round = 0; round < 5; round++) {
        /* Repeated two-site access, then a third colliding site. */
        const int order[] = {0, 1, 0, 1, 2, 0, 2, 1};
        for(size_t step = 0; step < sizeof(order) / sizeof(*order); step++) {
            int at = order[step];
            Value value = eval(&frame, selected[at], 0);
            assert(!instance->vm.failed && value.kind == VALUE_INT);
            assert(value.integer == base + at + 1);
        }
    }
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZibLawTable laws = {0};
    FILE *input = fopen(argv[1], "rb");
    assert(input);
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                    entry, sizeof(entry), &laws, NULL);
    fclose(input);
    assert(program);
    const ZirModule *first = module_named(program, "first");
    const ZirModule *second = module_named(program, "second");
    int calls[3], globals[3];
    ZirExpr *call_sites = sites(ZIR_EXPR_CALL, calls);
    ZirExpr *global_sites = sites(ZIR_EXPR_IDENT, globals);
    const char *functions[] = {"One", "Two", "Three"};
    const char *names[] = {"first_value", "second_value", "third_value"};
    for(int i = 0; i < 3; i++) {
        call_sites[calls[i]].name = KeepName(functions[i]);
        global_sites[globals[i]].name = KeepName(names[i]);
    }
    VmInstance *instances[2];
    for(int i = 0; i < 2; i++) {
        instances[i] = VmInstanceOpen(program, module, entry, NULL, NULL);
        assert(instances[i]);
    }
    for(int repeat = 0; repeat < 8; repeat++) {
        for(int i = 0; i < 2; i++) {
            check(instances[i], first, call_sites, calls, 0);
            check(instances[i], second, call_sites, calls, 100);
            for(int field = 0; field < 3; field++) {
                set_global(instances[i], first, names[field], repeat * 10 + i * 1000 + field + 1);
                set_global(instances[i], second, names[field], repeat * 10 + i * 1000 + field + 101);
            }
            check(instances[i], first, global_sites, globals, repeat * 10 + i * 1000);
            check(instances[i], second, global_sites, globals, repeat * 10 + i * 1000 + 100);
        }
    }
    VmInstanceClose(instances[0]);
    check(instances[1], first, call_sites, calls, 0);
    check(instances[1], second, global_sites, globals, 1170);
    VmInstanceClose(instances[1]);
    VmInstance *uncached = VmInstanceOpen(program, module, entry, NULL, NULL);
    assert(uncached);
    refuse_cache_storage = 1;
    check(uncached, first, call_sites, calls, 0);
    check(uncached, second, call_sites, calls, 100);
    check(uncached, first, global_sites, globals, 0);
    check(uncached, second, global_sites, globals, 100);
    refuse_cache_storage = 0;
    VmInstanceClose(uncached);
    free(call_sites);
    free(global_sites);
    ZibLawTableFree(&laws);
    ProgramFree(program);
    puts("Colliding call/global sites retain module identity, mutable values, independent lifetimes and allocation-failure behavior");
    return 0;
}
