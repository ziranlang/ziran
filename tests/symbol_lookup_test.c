#include "zir.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static void
answer(const ZirModule *module, const char *name, const char *path,
       int expected_status, const ZirModule *expected_owner, int index)
{
    const ZirModule *first_owner = NULL;
    const ZirFunction *first_function = NULL;
    for(int repeat = 0; repeat < 4; repeat++) {
        const ZirModule *owner = module;
        const ZirFunction *function = (const ZirFunction *)module;
        int status = ResolveFunctionAt(module, name, path, &owner, &function);
        assert(status == expected_status);
        if(status >= 0) {
            assert(owner == expected_owner);
            assert(function == (index < 0 ? NULL : &owner->functions[index]));
        }
        if(repeat) {
            assert(owner == first_owner);
            assert(function == first_function);
        }
        first_owner = owner;
        first_function = function;
    }
}

static ZirFunction *
procedure(ZirModule *module, const char *name, const char *path, int public)
{
    ZirFunction *function = ModuleAddFunction(module, name, "", "s64", 0,
                                             Span(path, 1, 1));
    assert(function != NULL);
    function->is_public = public;
    return function;
}

static void
import_module(ZirModule *module, ZirModule *target, const char *alias,
              int using, const char *path)
{
    ZirImport *import = ModuleAddImport(module, alias ? ZIR_IMPORT_MODULE : ZIR_IMPORT_OPEN,
                                        alias ? alias : "", target->name, "", 0,
                                        Span(path, 1, 1));
    assert(import != NULL);
    import->is_using = using;
    import->resolved_module = target;
}

static void
scopes_and_mutation(void)
{
    ZirProgram *program = ProgramNew();
    assert(program != NULL);
    for(int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "module%d", i);
        assert(ProgramAddModule(program, name, name, Span(name, 1, 1)) != NULL);
    }
    ZirModule *root = &program->modules[0];
    ZirModule *left = &program->modules[1], *right = &program->modules[2];
    ZirModule *leaf = &program->modules[3], *other = &program->modules[4];
    answer(root, "Later", NULL, 0, NULL, -1);
    procedure(root, "Later", "one.zi", 0);
    answer(root, "Later", NULL, 1, root, 0);
    /* Array growth invalidates pointers as well as formerly absent names. */
    for(int i = 0; i < 150; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Added%d", i);
        procedure(root, name, "one.zi", 0);
        answer(root, "Later", NULL, 1, root, 0);
    }
    strcpy(root->functions[0].name, "Renamed");
    SymbolLookupsChanged();
    answer(root, "Later", NULL, 0, NULL, -1);
    answer(root, "Renamed", NULL, 1, root, 0);
    ZirType *type = ModuleAddType(root, "Renamed", Span("one.zi", 1, 1));
    assert(type != NULL);
    strcpy(type->body, "value: s64;");
    for(int repeat = 0; repeat < 4; repeat++) {
        const ZirModule *owner = NULL;
        assert(FindType(root, "Renamed", &owner) == &root->types[0]);
        assert(owner == root);
        answer(root, "Renamed", NULL, 1, root, 0);
    }
    procedure(root, "Private", "one.zi", 0)->is_file_private = 1;
    procedure(root, "Private", "two.zi", 0)->is_file_private = 1;
    answer(root, "Private", NULL, 1, root, 151);
    answer(root, "Private", "", 1, root, 151);
    answer(root, "AddedAfterMiss", NULL, 0, NULL, -1);
    procedure(root, "AddedAfterMiss", "one.zi", 0);
    answer(root, "AddedAfterMiss", NULL, 1, root, 153);
    char path[32], name[32];
    for(int repeat = 0; repeat < 4; repeat++) {
        strcpy(name, "Private");
        strcpy(path, "one.zi");
        answer(root, name, path, 1, root, 151);
        strcpy(path, "two.zi");
        answer(root, name, path, 1, root, 152);
        strcpy(path, "absent.zi");
        answer(root, name, path, 0, NULL, -1);
        strcpy(name, "Renamed");
        answer(root, name, path, 1, root, 0);
        strcpy(root->lookup_path, "two.zi");
        const ZirModule *owner = NULL;
        const ZirFunction *function = NULL;
        assert(ResolveFunction(root, "Private", &owner, &function) == 1);
        assert(owner == root && function == &root->functions[152]);
    }
    procedure(leaf, "Shared", "leaf.zi", 1);
    procedure(other, "Shared", "other.zi", 1);
    import_module(root, left, "Left", 1, "one.zi");
    import_module(root, right, "Right", 1, "one.zi");
    import_module(left, leaf, "Leaf", 1, "left.zi");
    import_module(right, leaf, "Leaf", 1, "right.zi");
    import_module(leaf, left, "Cycle", 1, "leaf.zi");
    answer(root, "Shared", NULL, 1, leaf, 0);
    answer(root, "Left.Shared", NULL, 1, leaf, 0);
    answer(root, "Right.Shared", NULL, 1, leaf, 0);
    answer(root, "Missing", NULL, 0, NULL, -1);
    answer(root, "Left.Missing", NULL, 0, NULL, -1);
    answer(root, ".Shared", NULL, 0, NULL, -1);
    answer(root, "Left..Shared", NULL, 0, NULL, -1);
    /* A graph change can turn a unique diamond result into ambiguity. */
    import_module(right, other, "Other", 1, "right.zi");
    answer(root, "Shared", NULL, -1, NULL, -1);
    answer(root, "Right.Shared", NULL, -1, NULL, -1);
    other->functions[0].is_public = 0;
    SymbolLookupsChanged();
    answer(root, "Shared", NULL, 1, leaf, 0);
    root->imports[0].is_file_private = 1;
    SymbolLookupsChanged();
    answer(root, "Left.Shared", "one.zi", 1, leaf, 0);
    answer(root, "Left.Shared", "two.zi", 0, NULL, -1);
    root->imports[0].resolved_module = other;
    SymbolLookupsChanged();
    answer(root, "Left.Shared", "one.zi", 0, NULL, -1);
    other->functions[0].is_public = 1;
    SymbolLookupsChanged();
    answer(root, "Left.Shared", "one.zi", 1, other, 0);
    /* Grow the answer table without changing declarations, retaining both
     * hits and misses across rehashes. Reuse mutable name storage. */
    for(int i = 0; i < 9000; i++) {
        snprintf(name, sizeof(name), "Absent%d", i);
        answer(root, name, NULL, 0, NULL, -1);
    }
    answer(root, "Left.Shared", "one.zi", 1, other, 0);
    answer(root, "Renamed", NULL, 1, root, 0);
    ProgramFree(program);
}

static void *
independent_programs(void *argument)
{
    const char *path = argument;
    for(int iteration = 0; iteration < 20; iteration++) {
        ZirProgram *program = ProgramNew();
        assert(program != NULL);
        ZirModule *module = ProgramAddModule(program, "reused", path, Span(path, 1, 1));
        assert(module != NULL);
        for(int i = 0; i < 5; i++) {
            char name[32];
            snprintf(name, sizeof(name), "Other%d", i);
            procedure(module, name, path, 0);
        }
        answer(module, "Shared", path, 0, NULL, -1);
        procedure(module, "Shared", path, 0);
        answer(module, "Shared", path, 1, module, 5);
        ZirType *type = ModuleAddType(module, "Shared", Span(path, 1, 1));
        assert(type != NULL);
        strcpy(type->body, "value: s64;");
        for(int repeat = 0; repeat < 100; repeat++) {
            const ZirModule *owner = NULL;
            assert(FindType(module, "Shared", &owner) == type);
            assert(owner == module);
            answer(module, "Shared", path, 1, module, 5);
        }
        ProgramFree(program);
    }
    return NULL;
}

int
main(void)
{
    scopes_and_mutation();
    pthread_t threads[4];
    const char *paths[] = {"thread0.zi", "thread1.zi", "thread2.zi", "thread3.zi"};
    for(int i = 0; i < 4; i++)
        assert(pthread_create(&threads[i], NULL, independent_programs, (void *)paths[i]) == 0);
    for(int i = 0; i < 4; i++)
        assert(pthread_join(threads[i], NULL) == 0);
    return 0;
}
