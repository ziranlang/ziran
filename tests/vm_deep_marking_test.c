#include "zir_bundle.h"
#include "zir_vm_internal.h"
#include <assert.h>

static int fail_allocation, failed_allocations;
void *__real_malloc(size_t bytes);
void *__real_realloc(void *pointer, size_t bytes);

void *
__wrap_malloc(size_t bytes)
{
    if(fail_allocation == 1) {
        failed_allocations++;
        return NULL;
    }
    return __real_malloc(bytes);
}

void *
__wrap_realloc(void *pointer, size_t bytes)
{
    if(fail_allocation == 2) {
        failed_allocations++;
        return NULL;
    }
    return __real_realloc(pointer, bytes);
}

static void
borrowed_graph(void)
{
    Vm vm = {0};
    ZirType node = {0};
    strcpy(node.name, "Node");
    strcpy(node.body, "next: *Node; links: []*Node; name: string;");
    Record *root = allocate_record(&vm, NULL, &node, 3);
    Array *branches = allocate_array_try(&vm, NULL, "*Node", 80, 1);
    assert(root != NULL && branches != NULL && !vm.failed);
    root->fields[1].value = (Value){.kind = VALUE_SLICE,
                                   .array = branches, .length = 80};
    for(size_t branch = 0; branch < 80; branch++) {
        Record *previous = root;
        for(int depth = 0; depth < 256; depth++) {
            Record *current = allocate_record(&vm, NULL, &node, 3);
            assert(current != NULL && !vm.failed);
            Value pointer = {.kind = VALUE_POINTER, .record = current,
                             .pointee = &current->fields[0].value};
            if(depth == 0)
                array_set(branches, branch, pointer);
            else
                previous->fields[0].value = pointer;
            previous = current;
        }
        previous->fields[0].value = (Value){.kind = VALUE_POINTER,
            .record = root, .pointee = &root->fields[0].value};
        StringLiteral *text = malloc(sizeof(*text) + 4);
        assert(text != NULL);
        text->length = 4;
        memcpy(text->data, "deep", 4);
        previous->fields[2].value = keep_string(&vm, text, NULL,
                                               sizeof(*text) + 4);
    }
    for(Record *record = vm.records; record != NULL; record = record->next)
        retire_value(&vm, (Value){.kind = VALUE_RECORD, .record = record}, 0);
    retire_value(&vm, (Value){.kind = VALUE_ARRAY, .array = branches}, 0);
    size_t bytes = vm.record_bytes + vm.array_bytes;
    Record *discarded = allocate_record(&vm, NULL, &node, 3);
    assert(discarded != NULL && !vm.failed);
    retire_value(&vm, (Value){.kind = VALUE_RECORD, .record = discarded}, 0);
    size_t before_collection = vm.record_bytes + vm.array_bytes;
    Value borrowed = {.kind = VALUE_RECORD, .record = root};
    VmRoots roots = {NULL, &borrowed, 1};
    vm.evaluation_roots = &roots;
    /* The initial queue allocation and its later growth may fail separately.
     * Both must retain the complete live graph until a successful collection. */
    for(int allocation = 1; allocation <= 2; allocation++) {
        fail_allocation = allocation;
        failed_allocations = 0;
        release_retired(&vm);
        fail_allocation = 0;
        assert(failed_allocations == 1);
        assert(vm.record_bytes + vm.array_bytes == before_collection);
        for(Record *record = vm.records; record != NULL; record = record->next)
            assert(record->pinned == vm.pin_generation);
        for(StringLiteral *text = vm.strings; text != NULL; text = text->next)
            assert(text->pinned == vm.pin_generation);
    }
    release_retired(&vm);
    assert(vm.record_bytes + vm.array_bytes == bytes);
    assert(branches->pinned == vm.pin_generation);
    for(Record *record = vm.records; record != NULL; record = record->next)
        assert(record->pinned == vm.pin_generation);
    for(StringLiteral *text = vm.strings; text != NULL; text = text->next) {
        assert(text->pinned == vm.pin_generation);
        assert(memcmp(text->data, "deep", 4) == 0);
    }
    vm.evaluation_roots = NULL;
    release_retired(&vm);
    assert(vm.record_bytes == 0 && vm.array_bytes == 0);
    free_strings(&vm);
    free_layouts(&vm);
    assert(vm.string_bytes == 0);
}

int
main(int argc, char **argv)
{
    borrowed_graph();
    assert(argc == 2);
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZibLawTable laws = {0};
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL);
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                     entry, sizeof(entry), &laws, NULL);
    fclose(input);
    assert(program != NULL);
    VmInstance *instance = VmInstanceOpen(program, module, entry, NULL, NULL);
    assert(instance != NULL);
    for(int run = 0; run < 3; run++) {
        long long answer = 0;
        int present = 0;
        assert(VmInstanceRun(instance, &answer, &present));
        assert(present && answer == 42);
    }
    VmInstanceClose(instance);
    ZibLawTableFree(&laws);
    ProgramFree(program);
    return 0;
}
