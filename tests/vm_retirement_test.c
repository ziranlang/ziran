#include "zir_vm_internal.h"
#include <assert.h>

static void
borrowed_graph(Vm *vm)
{
    ZirType node = {0};
    strcpy(node.name, "Node");
    strcpy(node.body, "name: string; next: *Node; data: []u8; links: [3]*Node;");
    Record *root = allocate_record(vm, NULL, &node, 4);
    Record *child = allocate_record(vm, NULL, &node, 4);
    Record *discarded = allocate_record(vm, NULL, &node, 4);
    Array *bytes = allocate_array_try(vm, NULL, "u8", 4, 1);
    Array *links = allocate_array_try(vm, NULL, "*Node", 3, 1);
    assert(root != NULL && child != NULL && discarded != NULL);
    assert(bytes != NULL && links != NULL && !vm->failed);
    StringLiteral *text = malloc(sizeof(*text) + 4);
    assert(text != NULL);
    text->length = 4;
    memcpy(text->data, "kept", 4);
    Value name = keep_string(vm, text, NULL, sizeof(*text) + 4);
    root->fields[0].value = name;
    root->fields[1].value = (Value){.kind = VALUE_POINTER, .record = child,
                                  .pointee = &child->fields[0].value};
    root->fields[2].value = (Value){.kind = VALUE_SLICE, .array = bytes, .length = 4};
    root->fields[3].value = (Value){.kind = VALUE_ARRAY, .array = links};
    child->fields[1].value = (Value){.kind = VALUE_POINTER, .record = root,
                                   .pointee = &root->fields[0].value};
    array_set(bytes, 0, int_value(97));
    array_set(links, 0, (Value){.kind = VALUE_POINTER, .record = child,
                              .pointee = &child->fields[0].value});
    array_set(links, 1, (Value){.kind = VALUE_POINTER, .array = bytes,
                              .offset = 0, .indexed = 1});
    /* Popped slots may still have inactive payload bits. They cannot retain
     * a discarded allocation just because a record pointer remains there. */
    array_set(links, 2, (Value){.kind = VALUE_INVALID, .record = discarded});
    Value borrowed = {.kind = VALUE_RECORD, .record = root};
    VmRoots roots = {NULL, &borrowed, 1};
    vm->evaluation_roots = &roots;
    retire_value(vm, borrowed, 0);
    retire_value(vm, (Value){.kind = VALUE_RECORD, .record = child}, 0);
    retire_value(vm, (Value){.kind = VALUE_RECORD, .record = discarded}, 0);
    retire_value(vm, (Value){.kind = VALUE_ARRAY, .array = bytes}, 0);
    release_retired(vm);
    assert(root->pinned == vm->pin_generation);
    assert(child->pinned == vm->pin_generation);
    assert(bytes->pinned == vm->pin_generation && links->pinned == vm->pin_generation);
    assert(text->pinned == vm->pin_generation);
    assert(root->fields[0].value.length == 4);
    assert(memcmp(root->fields[0].value.data, "kept", 4) == 0);
    assert(array_get(bytes, 0).integer == 97);
    assert(vm->record_bytes == 2 * (sizeof(Record) + 4 * sizeof(RecordField)));
    vm->evaluation_roots = NULL;
    release_retired(vm);
    assert(vm->record_bytes == 0 && vm->array_bytes == 0);
    free_strings(vm);
    assert(vm->string_bytes == 0);
}

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
    borrowed_graph(&vm);
    free_layouts(&vm);
    return 0;
}
