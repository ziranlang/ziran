#include "zir_vm_internal.h"
#include <assert.h>

/* Discarded values can still be borrowed by an expression. Reclaim them only
 * after that expression releases its roots, even while other collectors run. */
int
main(void)
{
    Vm vm = {0};
    ZirType cell = {0};
    strcpy(cell.name, "Cell");
    strcpy(cell.body, "value: s32;");
    Record *record = allocate_record(&vm, NULL, &cell, 1);
    Array *array = allocate_array_try(&vm, NULL, "u8", 4, 1);
    assert(record != NULL && array != NULL && !vm.failed);
    record->fields[0].value = int_value(42);
    array_set(array, 0, int_value(97));
    Value borrowed[2] = {
        {.kind = VALUE_RECORD, .record = record},
        {.kind = VALUE_SLICE, .array = array, .length = 4}
    };
    VmRoots roots = {NULL, borrowed, 2};
    vm.evaluation_roots = &roots;
    retire_value(&vm, borrowed[0], 0);
    retire_value(&vm, (Value){.kind = VALUE_ARRAY, .array = array}, 0);
    size_t borrowed_bytes = vm.record_bytes + vm.array_bytes;
    release_retired(&vm);
    assert(vm.record_bytes + vm.array_bytes == borrowed_bytes);
    assert(record->fields[0].value.integer == 42);
    assert(array_get(array, 0).integer == 97);

    /* Newer live allocations must survive reclaiming an older retired one.
     * A sweep can also remove a retired member before the next local release. */
    Record *live[128];
    Array *buffers[128];
    for(int i = 0; i < 128; i++) {
        live[i] = allocate_record(&vm, NULL, &cell, 1);
        buffers[i] = allocate_array_try(&vm, NULL, "u8", 4, 1);
        assert(live[i] != NULL && buffers[i] != NULL);
        live[i]->fields[0].value = int_value(i);
        array_set(buffers[i], 0, int_value(i));
    }
    for(int i = 0; i < 128; i += 2) {
        retire_value(&vm, (Value){.kind = VALUE_RECORD, .record = live[i]}, 0);
        retire_value(&vm, (Value){.kind = VALUE_ARRAY, .array = buffers[i]}, 0);
    }
    release_record(&vm, live[64]);
    release_array(&vm, buffers[64]);
    release_retired(&vm);
    for(int i = 1; i < 128; i += 2) {
        assert(live[i]->fields[0].value.integer == i);
        assert(array_get(buffers[i], 0).integer == i);
    }
    assert(record->fields[0].value.integer == 42);
    assert(array_get(array, 0).integer == 97);
    size_t with_borrowed = vm.record_bytes + vm.array_bytes;
    vm.evaluation_roots = NULL;
    release_retired(&vm);
    assert(vm.record_bytes + vm.array_bytes == with_borrowed - borrowed_bytes);

    /* Releasing all allocations must leave no stale retirement membership. */
    retire_value(&vm, (Value){.kind = VALUE_RECORD, .record = live[127]}, 0);
    retire_value(&vm, (Value){.kind = VALUE_ARRAY, .array = buffers[127]}, 0);
    free_records(&vm);
    free_arrays(&vm);
    release_retired(&vm);
    assert(vm.record_bytes == 0 && vm.array_bytes == 0);
    assert(vm.retired_records == NULL && vm.retired_arrays == NULL);
    record = allocate_record(&vm, NULL, &cell, 1);
    assert(record != NULL && !vm.failed);
    retire_value(&vm, (Value){.kind = VALUE_RECORD, .record = record}, 0);
    release_retired(&vm);
    assert(vm.record_bytes == 0);
    free_layouts(&vm);
    return 0;
}
