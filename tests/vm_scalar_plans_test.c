#include "zir_bundle.h"
/* Inspect the test instance's private plans, so a passing interpreter fallback
 * cannot stand in for coverage of the optimized evaluator. */
#include "../cmd/zir/zir_vm_eval.c"
#include <assert.h>

static void check_conversions(void)
{
    const char *types[] = {"bool", "integer", "s8", "u8", "s16", "u16",
        "s32", "u32", "s64", "u64", "float32", "float64", "real", "null",
        "*u8", "s8Extra", "u64Extra"};
    Value values[] = {int_value(0), int_value(-1), int_value(128), int_value(-129),
        int_value(INT64_MIN), int_value(INT64_MAX), uint_value(UINT64_MAX),
        uint_value(UINT64_C(0x8000000000000000)), real_value(-0.0), real_value(1.5),
        real_value(16777217.0), real_value(-129.0), real_value(256.0),
        real_value(INFINITY), real_value(NAN),
        string_value((const unsigned char *)"bad", 3)};
    for(size_t type = 0; type < sizeof types / sizeof *types; type++) {
        for(size_t value = 0; value < sizeof values / sizeof *values; value++) {
            Vm canonical = {0}, cached = {0};
            Frame frame = {.vm = &cached};
            Value expected = coerce_expression(&canonical, NULL, values[value], types[type]);
            Value actual = scalar_result(&frame, values[value], types[type], scalar_conversion(types[type]));
            assert(canonical.failed == cached.failed);
            if(canonical.failed) continue;
            assert(expected.kind == actual.kind);
            if(expected.kind == VALUE_REAL) {
                uint64_t a, b;
                memcpy(&a, &expected.real, sizeof(a));
                memcpy(&b, &actual.real, sizeof(b));
                assert(a == b);
            } else if(expected.kind == VALUE_INT) {
                assert(expected.unsigned64 == actual.unsigned64);
                assert(integer_bits(expected) == integer_bits(actual));
            }
        }
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    check_conversions();
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZibLawTable laws = {0};
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL);
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module), entry,
                                    sizeof(entry), &laws, NULL);
    fclose(input);
    assert(program != NULL);
    VmInstance *first = VmInstanceOpen(program, module, entry, NULL, NULL);
    VmInstance *second = VmInstanceOpen(program, module, entry, NULL, NULL);
    assert(first != NULL && second != NULL);
    for(int run = 0; run < 8; run++) {
        for(int which = 0; which < 2; which++) {
            VmInstance *instance = which ? second : first;
            long long answer = -1;
            int present = 0;
            assert(VmInstanceRun(instance, &answer, &present) && present && answer == 42);
            assert(instance->vm.scalar_sites != NULL);
            int plans = 0, branches = 0, indices = 0, conversions = 0, operations = 0;
            for(size_t i = 0; i < instance->vm.scalar_site_slots; i++) {
                const ScalarPlan *plan = instance->vm.scalar_sites[i].plan;
                if(plan == NULL) continue;
                plans++;
                for(int step = 0; step < plan->count; step++) {
                    branches += plan->steps[step].op == SCALAR_FALSE ||
                        plan->steps[step].op == SCALAR_AND || plan->steps[step].op == SCALAR_OR;
                    indices += plan->steps[step].op == SCALAR_INDEX;
                    conversions += plan->steps[step].conversion != ScalarGeneric;
                    operations += plan->steps[step].operation != BinaryInvalid;
                }
            }
            assert(plans >= 4 && branches >= 2 && indices >= 2 && conversions >= 4 && operations >= 4);
        }
    }
    VmInstanceClose(first);
    long long answer = -1;
    int present = 0;
    assert(VmInstanceRun(second, &answer, &present) && present && answer == 42);
    VmInstanceClose(second);
    ZibLawTableFree(&laws);
    ProgramFree(program);
    puts("Scalar plans preserve branches, numeric coercion, current values and independent lifetimes");
    return 0;
}
