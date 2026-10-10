#include "zir_vm_internal.h"
#include <assert.h>

int
main(void)
{
    VmInstance *instance = calloc(1, sizeof(*instance));
    assert(instance != NULL);
    Vm *vm = &instance->vm;
    ZirType type = {0};
    strcpy(type.name, "Cell");
    strcpy(type.body, "value: s32;");
    Record *cell = allocate_record(vm, NULL, &type, 1);
    Record *dead = allocate_record(vm, NULL, &type, 1);
    assert(cell && dead);
    cell->fields[0].field = record_layout(vm, &type)->fields[0];
    cell->fields[0].value = int_value(42);
    Array *array = allocate_array_try(vm, NULL, "s32", 3, 1);
    assert(array);
    array_set(array, 1, int_value(17));
    Local locals[] = {
        {"number", "u64", uint_value(UINT64_MAX)},
        {"pointer", "*s32", {.kind = VALUE_POINTER, .record = cell,
                            .pointee = &cell->fields[0].value}},
        {"view", "[]s32", {.kind = VALUE_SLICE, .array = array, .length = 3}},
        {"moved", "integer", {.kind = VALUE_INVALID}}
    };
    ZirExpr expressions[] = {
        {.kind = ZIR_EXPR_INT, .text = "18446744073709551615", .type = "u64"},
        {.kind = ZIR_EXPR_FLOAT, .text = "16777217.0", .type = "float32"},
        {.kind = ZIR_EXPR_STRING, .text = "\"owned\"", .type = "string"},
        {.kind = ZIR_EXPR_SIZE_OF, .name = "u64", .type = "integer"},
        {.kind = ZIR_EXPR_COMPILE_TIME, .type = "integer"},
        {.kind = ZIR_EXPR_IDENT, .name = "true", .type = "bool"},
        {.kind = ZIR_EXPR_IDENT, .name = "false", .type = "bool"},
        {.kind = ZIR_EXPR_IDENT, .name = "null", .type = "*s32"},
        {.kind = ZIR_EXPR_IDENT, .name = locals[0].name, .type = locals[0].type},
        {.kind = ZIR_EXPR_IDENT, .name = locals[1].name, .type = locals[1].type},
        {.kind = ZIR_EXPR_IDENT, .name = locals[2].name, .type = locals[2].type},
        {.kind = ZIR_EXPR_IDENT, .name = locals[3].name, .type = locals[3].type}
    };
    ZirStmt statement = {.kind = ZIR_STMT_RETURN, .expr_root = -1};
    ZirFunction function = {0};
    function.exprs = expressions;
    function.expr_count = sizeof expressions / sizeof *expressions;
    function.stmts = &statement;
    function.stmt_count = 1;
    strcpy(function.return_type, "void");
    Frame frame = {.vm = vm, .function = &function, .locals = locals,
                   .local_count = 4, .local_capacity = 4};
    Value protected = {.kind = VALUE_POINTER, .record = cell,
                       .pointee = &cell->fields[0].value};
    VmRoots guard = {NULL, &protected, 1};
    vm->evaluation_roots = &guard;
    vm->active_frame = &frame;
    for(int repetition = 0; repetition < 2; repetition++) {
        for(int i = 0; i < function.expr_count - 1; i++) {
            Value value = eval(&frame, i, 0);
            assert(!vm->failed && vm->evaluation_roots == &guard);
            if(i == 0 || i == 8) assert(value.unsigned64 && value.bits == UINT64_MAX);
            if(i == 1) assert(value.kind == VALUE_REAL && value.real == 16777216.0);
            if(i == 2) assert(value.kind == VALUE_STRING && value.length == 5 &&
                              memcmp(value.data, "owned", 5) == 0);
            if(i == 3) assert(value.integer == 8);
            if(i == 4 || i == 6 || i == 7) assert(integer_bits(value) == 0);
            if(i == 5) assert(value.integer == 1);
            if(i == 9) assert(value.kind == VALUE_POINTER && value.record == cell);
            if(i == 10) assert(value.kind == VALUE_SLICE && value.array == array);
        }
    }
    locals[0].value = uint_value(99);
    assert(eval(&frame, 8, 0).bits == 99);
    assert(vm->evaluation_roots == &guard);
    vm->collection_threshold = 1;
    Value result = run_function(vm, NULL, &function, NULL, 0);
    assert(result.kind == VALUE_VOID);
    assert(!vm->failed && vm->records == cell && cell->next == NULL);
    assert(array_get(array, 1).integer == 17 && cell->fields[0].value.integer == 42);
    assert(vm->evaluation_roots == &guard);
    const int invalid[] = {-1, function.expr_count, 11};
    for(size_t i = 0; i < sizeof invalid / sizeof *invalid; i++) {
        vm->failed = 0;
        (void)eval(&frame, invalid[i], 0);
        assert(vm->failed && vm->evaluation_roots == &guard);
    }
    vm->failed = 0;
    (void)eval(&frame, 0, VM_MAX_DEPTH);
    assert(vm->failed && vm->evaluation_roots == &guard);
    vm->active_frame = NULL;
    vm->evaluation_roots = NULL;
    VmInstanceClose(instance);
    return 0;
}
