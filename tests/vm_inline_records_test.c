#include "zir_bundle.h"
#include "zir_vm_internal.h"
#include <assert.h>

static Value field(Value value, const char *name)
{
    int index = inline_record_field(value.inline_record, name, UINT16_MAX);
    assert(index >= 0);
    return inline_record_get(&value, (unsigned)index);
}

static void store(Value *value, const char *name, Value item)
{
    int index = inline_record_field(value->inline_record, name, UINT16_MAX);
    assert(index >= 0);
    inline_record_set(value, (unsigned)index, item);
}

static ZirType *check_storage(VmInstance *instance)
{
    Vm *vm = &instance->vm;
    size_t before = VmInstanceLiveValueBytes(instance);
    Value points = default_value(vm, instance->module, "[32]Point", 0);
    assert(!vm->failed && points.kind == VALUE_ARRAY);
    assert(points.array->storage == ARRAY_INLINE_RECORD && !points.array->holds_references);
    assert(points.array->inline_record->size == 16 && points.array->data_bytes == 512);
    size_t live = VmInstanceLiveValueBytes(instance) - before;
    assert(live == sizeof(Array) + 512 + 4);
    assert(live < 2 * 512);
    Value point = array_get(points.array, 0);
    assert(point.kind == VALUE_INLINE_RECORD && field(point, "x").integer == 0);
    store(&point, "x", int_value(INT64_MIN));
    store(&point, "y", int_value(INT64_MAX));
    array_set(points.array, 1, point);
    Value copy = array_get(points.array, 1);
    assert(field(copy, "x").integer == INT64_MIN && field(copy, "y").integer == INT64_MAX);
    store(&copy, "y", int_value(7));
    assert(field(array_get(points.array, 1), "y").integer == INT64_MAX);
    array_set(points.array, 1, (Value){0});
    assert(array_get(points.array, 1).kind == VALUE_INVALID);
    array_set(points.array, 1, copy);
    assert(field(array_get(points.array, 1), "y").integer == 7);

    Value reals = default_value(vm, instance->module, "[4]Doubles", 0);
    assert(reals.array->storage == ARRAY_INLINE_RECORD);
    double values[] = {-0.0, INFINITY, -INFINITY, NAN};
    for(size_t i = 0; i < 4; i++) {
        Value pair = array_get(reals.array, i);
        store(&pair, "x", real_value(values[i]));
        store(&pair, "y", real_value(values[3-i]));
        array_set(reals.array, i, pair);
    }
    for(size_t i = 0; i < 4; i++) {
        Value pair = array_get(reals.array, i);
        Value actual = field(pair, "x");
        uint64_t expected_bits, actual_bits;
        memcpy(&expected_bits, &values[i], 8);
        memcpy(&actual_bits, &actual.real, 8);
        assert(actual_bits == expected_bits);
    }

    /* Shapes outlive layout-table growth and never retain Value addresses. */
    const InlineRecord *shape = point.inline_record;
    ZirType *types = calloc(160, sizeof(*types));
    assert(types != NULL);
    for(int i = 0; i < 160; i++) {
        snprintf(types[i].name, sizeof(types[i].name), "Extra%d", i);
        strcpy(types[i].body, "item: u8;");
        assert(inline_record_shape(vm, instance->module, &types[i]) != NULL);
    }
    assert(point.inline_record == shape && field(point, "x").integer == INT64_MIN);
    assert(inline_record_shape(vm, instance->module, shape->type) == shape);
    assert(default_value(vm, instance->module, "Large", 0).kind == VALUE_RECORD);
    assert(default_value(vm, instance->module, "Overlay", 0).kind == VALUE_RECORD);
    assert(default_value(vm, instance->module, "Wrapper", 0).kind == VALUE_RECORD);
    assert(!vm->failed);
    printf("{\"point_payload_bytes\":16,\"array_elements\":32,\"vm_value_bytes\":%zu,\"native_payload_bytes\":512}\n", live);
    return types;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL);
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZxLawTable laws = {0};
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                     entry, sizeof(entry), &laws, NULL);
    fclose(input);
    assert(program != NULL);
    VmInstance *instance = VmInstanceOpen(program, module, entry, NULL, NULL);
    assert(instance != NULL);
    ZirType *extra_types = check_storage(instance);
    instance->vm.collection_threshold = 1;
    for(int run = 0; run < 3; run++) {
        long long answer = 0; int present = 0;
        assert(VmInstanceRun(instance, &answer, &present) && present && answer == 42);
    }
    VmInstanceClose(instance);
    free(extra_types);
    ZxLawTableFree(&laws);
    ProgramFree(program);
    return 0;
}
