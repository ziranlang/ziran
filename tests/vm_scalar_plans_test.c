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
            for(int mode = 0; mode < 3; mode++) {
                cached.failed = 0;
                Value input = values[value], output;
                memset(&output, 0xa5, sizeof(output));
                Value actual = mode == 0 ?
                    scalar_result(&frame, &input, types[type], scalar_conversion(types[type])) :
                    *scalar_coerce(&frame, &input, types[type], scalar_conversion(types[type]),
                                     mode == 1 ? &output : &input);
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
}

static void check_literal_eviction(void)
{
    /* Two operands that replace the same numeric cache entry must retain
     * their own values until the operation consumes both. */
    enum { SLOTS = 4096, COUNT = SLOTS + 2 };
    ZirExpr *expressions = calloc(COUNT, sizeof(*expressions));
    int *seen = malloc(SLOTS * sizeof(*seen));
    assert(expressions != NULL && seen != NULL);
    for(int i = 0; i < SLOTS; i++) seen[i] = -1;
    int left = -1, right = -1;
    size_t collided = 0;
    for(int i = 0; i < COUNT - 1; i++) {
        uintptr_t hash = (uintptr_t)&expressions[i];
        hash ^= hash >> 17;
        hash ^= hash >> 9;
        size_t slot = (hash >> 4) & (SLOTS - 1);
        if(seen[slot] >= 0) {
            left = seen[slot]; right = i; collided = slot; break;
        }
        seen[slot] = i;
    }
    assert(left >= 0 && right >= 0);
    expressions[left] = (ZirExpr){.kind = ZIR_EXPR_INT, .text = "11", .type = "s64"};
    expressions[right] = (ZirExpr){.kind = ZIR_EXPR_INT, .text = "31", .type = "s64"};
    int root = COUNT - 1;
    expressions[root] = (ZirExpr){.kind = ZIR_EXPR_BINARY, .left = left,
                                 .right = right, .type = "s64"};
    strcpy(expressions[root].op, "+");
    ZirFunction function = {.exprs = expressions, .expr_count = COUNT};
    VmInstance *instance = calloc(1, sizeof(*instance));
    assert(instance != NULL);
    Frame frame = {.vm = &instance->vm, .function = &function};
    const ScalarPlan *plan = scalar_plan(&frame, root);
    assert(plan != NULL && plan->count == 3);
    for(int run = 0; run < 100; run++) {
        Value result = eval(&frame, root, 0);
        assert(!instance->vm.failed && result.kind == VALUE_INT && result.integer == 42);
        assert(instance->vm.constant_sites[collided].expression == &expressions[right]);
    }
    VmInstanceClose(instance);
    free(seen);
    free(expressions);
}

static void check_short_plans(void)
{
    struct {
        const char *op, *left_type, *right_type, *type;
        Value left, right, expected;
        int fails;
    } cases[] = {
        {"+", "u8", "u8", "u8", int_value(255), int_value(2), int_value(1), 0},
        {"-", "s8", "s8", "s8", int_value(-128), int_value(1), int_value(127), 0},
        {"+", "u32", "u32", "u32", int_value(UINT32_MAX), int_value(1), int_value(0), 0},
        {"*", "u32", "u32", "u32", int_value(UINT32_MAX), int_value(UINT32_MAX), int_value(1), 0},
        {"+", "u64", "u64", "u64", uint_value(UINT64_MAX), uint_value(1), uint_value(0), 0},
        {"+", "s64", "s64", "s64", int_value(INT64_MAX), int_value(1), int_value(INT64_MIN), 0},
        {">>", "s8", "s32", "s8", int_value(-128), int_value(1), int_value(-64), 0},
        {"<<", "u16", "u64", "u16", int_value(32769), uint_value(1), int_value(2), 0},
        {">>", "s64", "u64", "s64", int_value(-8), uint_value(2), int_value(-2), 0},
        {">", "u64", "u64", "bool", uint_value(UINT64_C(9007199254740993)),
            uint_value(UINT64_C(9007199254740992)), int_value(1), 0},
        {"/", "s64", "s64", "s64", int_value(INT64_MIN), int_value(-1), int_value(INT64_MIN), 0},
        {"/", "integer", "integer", "integer", int_value(INT64_MIN), int_value(-1), int_value(INT64_MIN), 0},
        {"%", "integer", "integer", "integer", int_value(INT64_MIN), int_value(-1), int_value(0), 0},
        {"+", "float32", "float32", "float32", real_value(16777216.0), real_value(1.0),
            real_value(16777216.0), 0},
        {"/", "s64", "s64", "s64", int_value(9), int_value(0), int_value(0), 1},
        {"%", "s64", "s64", "s64", int_value(9), int_value(0), int_value(0), 1},
        {"<<", "u64", "s32", "u64", uint_value(1), int_value(64), int_value(0), 1},
        {">>", "s8", "s32", "s8", int_value(-128), int_value(8), int_value(0), 1},
        {">>", "s64", "s32", "s64", int_value(-8), int_value(-1), int_value(0), 1}
    };
    for(size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        VmInstance *instance = calloc(1, sizeof(*instance));
        assert(instance != NULL);
        Local locals[] = {{"left", cases[i].left_type, cases[i].left},
                          {"right", cases[i].right_type, cases[i].right}};
        ZirExpr expressions[] = {
            {.kind = ZIR_EXPR_IDENT, .name = locals[0].name, .type = locals[0].type},
            {.kind = ZIR_EXPR_IDENT, .name = locals[1].name, .type = locals[1].type},
            {.kind = ZIR_EXPR_BINARY, .left = 0, .right = 1, .type = cases[i].type}
        };
        strcpy(expressions[2].op, cases[i].op);
        ZirFunction function = {.exprs = expressions, .expr_count = 3};
        Frame frame = {.vm = &instance->vm, .function = &function,
                       .locals = locals, .local_count = 2, .local_capacity = 2};
        const ScalarPlan *plan = scalar_plan(&frame, 2);
        assert(plan != NULL && plan->count == 3 && plan->steps[2].op == SCALAR_BINARY);
        for(int run = 0; run < 2; run++) {
            instance->vm.failed = 0;
            Value actual = eval(&frame, 2, 0);
            assert(instance->vm.failed == cases[i].fails);
            if(cases[i].fails) continue;
            Value expected = cases[i].expected;
            assert(actual.kind == expected.kind);
            if(actual.kind == VALUE_REAL) assert(actual.real == expected.real);
            else assert(actual.unsigned64 == expected.unsigned64 &&
                        integer_bits(actual) == integer_bits(expected));
        }
        instance->vm.failed = 0;
        (void)eval(&frame, 2, VM_MAX_DEPTH - 1);
        assert(instance->vm.failed);
        VmInstanceClose(instance);
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    check_conversions();
    check_short_plans();
    check_literal_eviction();
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
            int plans = 0, branches = 0, indices = 0, conversions = 0, operations = 0, short_plans = 0;
            for(size_t i = 0; i < instance->vm.scalar_site_slots; i++) {
                const ScalarPlan *plan = instance->vm.scalar_sites[i].plan;
                if(plan == NULL) continue;
                plans++;
                short_plans += plan->count == 3 && plan->steps[2].op == SCALAR_BINARY;
                for(int step = 0; step < plan->count; step++) {
                    branches += plan->steps[step].op == SCALAR_FALSE ||
                        plan->steps[step].op == SCALAR_AND || plan->steps[step].op == SCALAR_OR;
                    indices += plan->steps[step].op == SCALAR_INDEX;
                    conversions += plan->steps[step].conversion != ScalarGeneric;
                    operations += plan->steps[step].operation != BinaryInvalid;
                }
            }
            assert(plans >= 4 && branches >= 2 && indices >= 2 && conversions >= 4 && operations >= 4);
            assert(short_plans > 0);
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
