#include "zir.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef TYPE_ALLOCATIONS
static int collecting;
static uint64_t allocation_calls, allocation_bytes;
extern void *__real_malloc(size_t size);
extern void *__real_calloc(size_t count, size_t size);
extern void *__real_realloc(void *memory, size_t size);
void *__wrap_malloc(size_t size)
{
    void *result = __real_malloc(size);
    if(collecting) { allocation_calls++; allocation_bytes += size; }
    return result;
}
void *__wrap_calloc(size_t count, size_t size)
{
    void *result = __real_calloc(count, size);
    if(collecting) { allocation_calls++; allocation_bytes += (uint64_t)count * size; }
    return result;
}
void *__wrap_realloc(void *memory, size_t size)
{
    void *result = __real_realloc(memory, size);
    if(collecting) { allocation_calls++; allocation_bytes += size; }
    return result;
}
#else
static const uint64_t allocation_calls = 0, allocation_bytes = 0;
#endif

static uint64_t clock_ns(clockid_t clock)
{
    struct timespec value;
    assert(clock_gettime(clock, &value) == 0);
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + (uint64_t)value.tv_nsec;
}

static void link_module(ZirModule *from, ZirModule *to, int index)
{
    char alias[32];snprintf(alias, sizeof(alias), "Import%d", index);
    ZirImport *import = ModuleAddImport(from, ZIR_IMPORT_MODULE, alias,
        to->name, "", 0, Span(from->name, 1, 1));
    assert(import);import->resolved_module = to;
    import->is_public = import->is_using = 1;
}

int main(int argc, char **argv)
{
    assert(argc == 5);
    int count = atoi(argv[1]), rounds = atoi(argv[3]);
    int local = !strcmp(argv[2], "local"), cold = !strcmp(argv[4], "cold");
    assert(count >= 1 && count <= 4096 && rounds > 0);
    assert((local && count == 1) || !strcmp(argv[2], "wide") || !strcmp(argv[2], "deep") || !strcmp(argv[2], "shared"));
    assert(cold || !strcmp(argv[4], "cached"));
    ZirProgram *program = ProgramNew();assert(program);
    for(int i = 0; i < count; i++) {
        char name[32];snprintf(name, sizeof(name), "node%04d", i);
        assert(ProgramAddModule(program, name, name, Span(name, 1, 1)));
    }
    ZirModule *root = &program->modules[0], *last = &program->modules[count - 1];
    for(int i = 1; i < count; i++) {
        link_module(&program->modules[!strcmp(argv[2], "wide") ? 0 : i - 1], &program->modules[i], i);
        if(!strcmp(argv[2], "shared") && i > 1)
            link_module(&program->modules[i - 2], &program->modules[i], i);
    }
    if(!local) link_module(last, root, 0);
    ZirType *type = ModuleAddType(last, "Value", Span(last->name, 1, 1));
    assert(type);type->is_public = 1;strcpy(type->body, "number: s64;\n");
    if(local) { type = ModuleAddType(root, "Other", Span(root->name, 1, 1));assert(type);type->is_public = 1; }
    type = &last->types[0];
    const ZirType *secondary = local ? &root->types[1] : NULL;
    const char *second_name = local ? "Other" : "Missing";
    const ZirModule *owner = NULL;
    int found = FindType(root, "Value", &owner) == type;
    assert(!found || owner == last);
    assert(FindType(root, second_name, &owner) == secondary && owner == (local ? root : NULL));
    uint64_t wall_start = clock_ns(CLOCK_MONOTONIC), cpu_start = clock_ns(CLOCK_PROCESS_CPUTIME_ID);
#ifdef TYPE_ALLOCATIONS
    collecting = 1;
#endif
    for(int i = 0; i < rounds; i++) {
        if(cold) SymbolLookupsChanged();
        assert((FindType(root, "Value", &owner) == type) == found);
        assert(owner == (found ? last : NULL));
        assert(FindType(root, second_name, &owner) == secondary && owner == (local ? root : NULL));
    }
#ifdef TYPE_ALLOCATIONS
    collecting = 0;
#endif
    uint64_t cpu = clock_ns(CLOCK_PROCESS_CPUTIME_ID) - cpu_start, wall = clock_ns(CLOCK_MONOTONIC) - wall_start;
    printf("{\"modules\":%d,\"rounds\":%d,\"found\":%s,\"secondary_correct\":true,\"wall_ns\":%" PRIu64 ",\"cpu_ns\":%" PRIu64 ",\"allocation_calls\":%" PRIu64 ",\"allocation_bytes\":%" PRIu64 "}\n",
        count, rounds, found ? "true" : "false", wall, cpu, allocation_calls, allocation_bytes);
    ProgramFree(program);return 0;
}
