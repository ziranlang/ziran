#include "zir_bundle.h"
#include "zir_text.h"
#include "zir_vm.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WORKERS = 8, INSERTIONS = 600, RUNS = 60 };
static pthread_barrier_t ready;
static const ZirProgram *program;
static const ZirModule empty_module = {0};
static char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];

typedef struct Worker {
    unsigned index;
    const char *shared;
    const ZirParameters *parameters;
} Worker;

static void *run_worker(void *context)
{
    Worker *worker = context;
    pthread_barrier_wait(&ready);
    for(unsigned index = 0; index < INSERTIONS; index++) {
        uint64_t started = ProfileStart();
        ProfileCount("concurrent_probe");
        ProfileAllocation(7);
        char text[128], parameters[128], name[64];
        snprintf(text, sizeof(text), "concurrent text %u %u", worker->index, index);
        const char *kept = KeepText(text);
        assert(strcmp(kept, text) == 0);
        assert(KeepText(text) == kept);
        worker->shared = KeepText("shared concurrent identity");
        worker->parameters = ParametersOf("value: s32, label: string");
        assert(worker->parameters->count == 2);
        assert(strcmp(worker->parameters->items[1].type, "string") == 0);
        snprintf(name, sizeof(name), "field_%u_%u", worker->index, index);
        const ZirModule *owner = &empty_module;
        assert(FindType(&empty_module, name, &owner) == NULL && owner == NULL);
        snprintf(parameters, sizeof(parameters), "%s: s32, label: string", name);
        const ZirParameters *parsed = ParametersOf(parameters);
        assert(parsed->count == 2 && strcmp(parsed->items[0].name, name) == 0);
        ZirType record = {0};
        snprintf(record.body, sizeof(record.body), "%s: s32; label: string; values: [3]u8", name);
        size_t cursor = 0;
        ZirTypeField field;
        assert(TypeNextField(&record, &cursor, &field) == 1);
        assert(strcmp(field.name, name) == 0 && strcmp(field.type, "s32") == 0);
        // Other workers grow the id table while this cursor retains its snapshot.
        assert(TypeNextField(&record, &cursor, &field) == 1);
        assert(strcmp(field.name, "label") == 0 && strcmp(field.type, "string") == 0);
        assert(TypeNextField(&record, &cursor, &field) == 1);
        assert(strcmp(field.name, "values") == 0 && strcmp(field.type, "[3]u8") == 0);
        assert(TypeNextField(&record, &cursor, &field) == 0);
        assert(strcmp(kept, text) == 0 && strcmp(parsed->items[0].name, name) == 0);
        ProfileEnd("concurrent_probe_phase", started);
    }
    pthread_barrier_wait(&ready);
    if(program == NULL) return NULL;
    VmInstance *instance = VmInstanceOpen(program, module, entry, NULL, NULL);
    assert(instance != NULL);
    for(unsigned index = 0; index < RUNS; index++) {
        long long answer = 0;
        int present = 0;
        assert(VmInstanceRun(instance, &answer, &present));
        assert(present && answer == (long long)index + 42);
    }
    VmInstanceClose(instance);
    return NULL;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    ZibLawTable laws = {0};
    ZirProgram *loaded = NULL;
    if(strcmp(argv[1], "--metadata-only") != 0) {
        FILE *input = fopen(argv[1], "rb");
        assert(input != NULL);
        loaded = BundleRead(input, argv[1], module, sizeof(module), entry,
                            sizeof(entry), &laws, NULL);
        fclose(input);
        assert(loaded != NULL);
    }
    program = loaded;
    pthread_t threads[WORKERS];
    Worker workers[WORKERS] = {0};
    assert(pthread_barrier_init(&ready, NULL, WORKERS) == 0);
    for(unsigned index = 0; index < WORKERS; index++) {
        workers[index].index = index;
        assert(pthread_create(&threads[index], NULL, run_worker, &workers[index]) == 0);
    }
    for(unsigned index = 0; index < WORKERS; index++) {
        assert(pthread_join(threads[index], NULL) == 0);
        assert(workers[index].shared == workers[0].shared);
        assert(workers[index].parameters == workers[0].parameters);
    }
    pthread_barrier_destroy(&ready);
    ZibLawTableFree(&laws);
    if(loaded != NULL) ProgramFree(loaded);
    puts("Concurrent immutable metadata growth and independent portable VM state passed");
    return 0;
}
