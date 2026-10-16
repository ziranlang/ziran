#include "zir.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t clock_ns(clockid_t clock)
{
    struct timespec value;
    assert(clock_gettime(clock, &value) == 0);
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + (uint64_t)value.tv_nsec;
}

static void link_module(ZirModule *from, ZirModule *to, int index)
{
    char alias[32];
    snprintf(alias, sizeof(alias), "Import%d", index);
    ZirImport *import = ModuleAddImport(from, ZIR_IMPORT_MODULE, alias,
        to->name, "", 0, Span(from->name, 1, 1));
    assert(import);
    import->resolved_module = to;
}

int main(int argc, char **argv)
{
    assert(argc == 5);
    int count = atoi(argv[1]), rounds = atoi(argv[3]);
    int cold = !strcmp(argv[4], "cold");
    assert(count >= 2 && count <= 8192 && rounds > 0);
    assert(cold || !strcmp(argv[4], "cached"));
    assert(!strcmp(argv[2], "wide") || !strcmp(argv[2], "deep") || !strcmp(argv[2], "shared"));
    ZirProgram *program = ProgramNew();
    for(int i = 0; i < count; i++) {
        char name[32];
        snprintf(name, sizeof(name), "node%04d", i);
        assert(ProgramAddModule(program, name, name, Span(name, 1, 1)));
    }
    ZirModule *root = &program->modules[0], *last = &program->modules[count - 1];
    for(int i = 1; i < count; i++) {
        int parent = !strcmp(argv[2], "wide") ? 0 : i - 1;
        link_module(&program->modules[parent], &program->modules[i], i);
        if(!strcmp(argv[2], "shared") && i > 1)
            link_module(&program->modules[i - 2], &program->modules[i], i);
    }
    link_module(last, root, 0);
    ZirType *type = ModuleAddType(last, "Value", Span(last->name, 1, 1));
    assert(type); type->is_public = 1; strcpy(type->body, "value: s64;\n");
    char name[64];
    snprintf(name, sizeof(name), "%s.Value", last->name);
    const ZirModule *owner = NULL;
    int found = FindType(root, name, &owner) == type;
    assert(!found || owner == last);
    assert(FindType(root, "missing.Value", &owner) == NULL && owner == NULL);
    uint64_t wall_start = clock_ns(CLOCK_MONOTONIC), cpu_start = clock_ns(CLOCK_PROCESS_CPUTIME_ID);
    for(int i = 0; i < rounds; i++) {
        if(cold) SymbolLookupsChanged();
        const ZirType *value = FindType(root, name, &owner);
        assert((value == type) == found && owner == (found ? last : NULL));
        assert(FindType(root, "missing.Value", &owner) == NULL && owner == NULL);
    }
    uint64_t cpu = clock_ns(CLOCK_PROCESS_CPUTIME_ID) - cpu_start;
    uint64_t wall = clock_ns(CLOCK_MONOTONIC) - wall_start;
    printf("{\"modules\":%d,\"rounds\":%d,\"found\":%s,\"missing_correct\":true,\"wall_ns\":%" PRIu64 ",\"cpu_ns\":%" PRIu64 "}\n",
           count, rounds, found ? "true" : "false", wall, cpu);
    ProgramFree(program);
    return 0;
}
