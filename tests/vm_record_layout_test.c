#include "zir_vm_internal.h"
#include <assert.h>

static int fail_calloc, calloc_count, fail_malloc;
void *__real_calloc(size_t count, size_t bytes);
void *__real_malloc(size_t bytes);

void *
__wrap_calloc(size_t count, size_t bytes)
{
    if(fail_calloc && ++calloc_count == fail_calloc)
        return NULL;
    return __real_calloc(count, bytes);
}

void *
__wrap_malloc(size_t bytes)
{
    return fail_malloc ? NULL : __real_malloc(bytes);
}

static void
initialize_fields(Vm *vm, Record *record)
{
    for(int i = 0; i < record->field_count; i++) {
        record->fields[i].field = record_layout(vm, record->type)->fields[i];
        record->fields[i].value = int_value(i);
    }
}

static void
stable_borrowed_layout(void)
{
    Vm vm = {0};
    ZirType *node = calloc(1, sizeof(*node));
    ZirType *extras = calloc(160, sizeof(*extras));
    assert(node && extras);
    strcpy(node->name, "Node");
    size_t used = 0;
    for(int i = 0; i < 24; i++)
        used += (size_t)snprintf(node->body + used, sizeof(node->body) - used,
                                 "v%d: s64; ", i);
    snprintf(node->body + used, sizeof(node->body) - used,
             "text: string; next: *Node; links: []*Node; flag: Shade; handler: Handler;");
    Record *root = allocate_record(&vm, NULL, node, 29);
    Record *child = allocate_record(&vm, NULL, node, 29);
    Record *leaf = allocate_record(&vm, NULL, node, 29);
    assert(root && child && leaf && !vm.failed);
    initialize_fields(&vm, root);
    initialize_fields(&vm, child);
    initialize_fields(&vm, leaf);
    const int *slots = root->field_slots;
    size_t capacity = root->field_slot_count;
    assert(slots == child->field_slots && slots == leaf->field_slots);
    assert(slots[capacity] == 5);
    for(int i = 0; i < 5; i++)
        assert(slots[capacity + 1 + (size_t)i] == 24 + i);
    StringLiteral *text = malloc(sizeof(*text) + 4);
    assert(text);
    text->length = 4;
    memcpy(text->data, "stay", 4);
    root->fields[24].value = keep_string(&vm, text, NULL, sizeof(*text) + 4);
    root->fields[25].value = (Value){.kind = VALUE_POINTER, .record = child,
                                   .pointee = &child->fields[0].value};
    child->fields[25].value = (Value){.kind = VALUE_POINTER, .record = root,
                                    .pointee = &root->fields[0].value};
    Array *links = allocate_array_try(&vm, NULL, "*Node", 2, 1);
    assert(links && !vm.failed);
    array_set(links, 0, (Value){.kind = VALUE_POINTER, .record = leaf,
                              .pointee = &leaf->fields[0].value});
    array_set(links, 1, (Value){.kind = VALUE_POINTER, .record = child,
                              .pointee = &child->fields[0].value});
    root->fields[26].value = (Value){.kind = VALUE_SLICE, .array = links, .length = 2};
    root->fields[27].value = (Value){.kind = VALUE_ENUM, .integer = 7};
    root->fields[28].value = (Value){.kind = VALUE_SLOT};
    size_t live = vm.record_bytes + vm.array_bytes;

    /* Several table growths must preserve metadata held by existing records.
     * Types containing only scalars need no collector field visits. */
    for(int i = 0; i < 160; i++) {
        snprintf(extras[i].name, sizeof(extras[i].name), "Extra%d", i);
        strcpy(extras[i].body, "value: s32;");
        Record *extra = allocate_record(&vm, NULL, &extras[i], 1);
        assert(extra && !vm.failed && extra->field_slots[extra->field_slot_count] == 0);
        initialize_fields(&vm, extra);
    }
    assert(vm.layout_slots >= 512 && root->field_slots == slots);
    assert(record_layout(&vm, node)->field_slots == slots);
    assert(record_field(root, "v23") == &root->fields[23].value);
    assert(record_field(root, "text") == &root->fields[24].value);
    assert(record_field(root, "missing") == NULL);
    for(Record *record = vm.records; record; record = record->next)
        retire_value(&vm, (Value){.kind = VALUE_RECORD, .record = record}, 0);
    retire_value(&vm, (Value){.kind = VALUE_ARRAY, .array = links}, 0);
    Value borrowed = {.kind = VALUE_POINTER, .record = root,
                      .pointee = &root->fields[0].value};
    VmRoots roots = {NULL, &borrowed, 1};
    vm.evaluation_roots = &roots;
    for(int i = 0; i < 3; i++) {
        release_retired(&vm);
        assert(vm.record_bytes + vm.array_bytes == live);
        assert(root->pinned == vm.pin_generation && child->pinned == vm.pin_generation);
        assert(leaf->pinned == vm.pin_generation && links->pinned == vm.pin_generation);
        assert(text->pinned == vm.pin_generation && !memcmp(text->data, "stay", 4));
        assert(record_field(root, "v23")->integer == 23);
    }
    vm.evaluation_roots = NULL;
    release_retired(&vm);
    assert(vm.record_bytes == 0 && vm.array_bytes == 0);
    free_strings(&vm);
    free_layouts(&vm);
    free(node);
    free(extras);
}

static void
allocation_failures(void)
{
    ZirType *type = calloc(1, sizeof(*type));
    assert(type);
    strcpy(type->name, "AllocationFailure");
    strcpy(type->body, "existing: s64;");
    (void)KeepText("existing");
    (void)KeepText("s64");
    /* Record storage, table and fields each have independent
     * allocation failures. The shared slots/reference allocation can fail too. */
    for(int failure = 1; failure <= 4; failure++) {
        Vm vm = {0};
        calloc_count = 0;
        fail_calloc = failure <= 3 ? failure : 0;
        fail_malloc = failure == 4;
        Record *record = allocate_record(&vm, NULL, type, 1);
        fail_calloc = fail_malloc = 0;
        assert(record == NULL && vm.failed && vm.records == NULL && vm.record_bytes == 0);
        free_layouts(&vm);
    }
    free(type);
}

static void
integer_pointer_field(void)
{
    Vm vm = {0};
    ZirType *types = calloc(2, sizeof(*types));
    assert(types);
    strcpy(types[0].name, "IntegerPointer");
    strcpy(types[0].body, "reference: integer;");
    strcpy(types[1].name, "Pointee");
    strcpy(types[1].body, "value: s64;");
    Record *root = allocate_record(&vm, NULL, &types[0], 1);
    Record *child = allocate_record(&vm, NULL, &types[1], 1);
    assert(root && child && !vm.failed);
    initialize_fields(&vm, root);
    initialize_fields(&vm, child);
    Value pointer = {.kind = VALUE_POINTER, .record = child,
                     .pointee = &child->fields[0].value};
    /* Existing unsized integer coercion preserves its input representation,
     * including a VM pointer's owner. Its field must therefore retain storage. */
    root->fields[0].value = coerce(&vm, NULL, pointer, "integer");
    assert(!vm.failed && root->fields[0].value.kind == VALUE_POINTER);
    size_t bytes = vm.record_bytes;
    retire_value(&vm, (Value){.kind = VALUE_RECORD, .record = root}, 0);
    retire_value(&vm, (Value){.kind = VALUE_RECORD, .record = child}, 0);
    Value borrowed = {.kind = VALUE_RECORD, .record = root};
    VmRoots roots = {NULL, &borrowed, 1};
    vm.evaluation_roots = &roots;
    release_retired(&vm);
    assert(vm.record_bytes == bytes);
    assert(root->fields[0].value.pointee->integer == 0);
    vm.evaluation_roots = NULL;
    release_retired(&vm);
    assert(vm.record_bytes == 0);
    free_layouts(&vm);
    free(types);
}

int
main(void)
{
    stable_borrowed_layout();
    integer_pointer_field();
    allocation_failures();
    return 0;
}
