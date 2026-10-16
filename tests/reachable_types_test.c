#include "zir.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static ZirProgram *graph(int count)
{
    ZirProgram *program = ProgramNew();
    assert(program);
    for(int i = 0; i < count; i++) {
        char name[32];
        snprintf(name, sizeof(name), "reach%04d", i);
        assert(ProgramAddModule(program, name, name, Span(name, 1, 1)));
    }
    return program;
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

static ZirType *value_type(ZirModule *module, const char *name)
{
    ZirType *type = ModuleAddType(module, name, Span(module->name, 1, 1));
    assert(type);
    type->is_public = 1;
    strcpy(type->body, "value: s64;\n");
    return type;
}

static void answer(ZirModule *root, const char *name, ZirModule *expected,
                   const ZirType *type)
{
    for(int repeat = 0; repeat < 3; repeat++) {
        const ZirModule *owner = root;
        assert(FindType(root, name, &owner) == type);
        assert(owner == expected);
    }
}

static void wide_graph(void)
{
    ZirProgram *program = graph(2049);
    ZirModule *root = &program->modules[0];
    for(int i = 1; i < program->module_count; i++)
        link_module(root, &program->modules[i], i);
    const int indices[] = {1, 510, 511, 512, 2048};
    for(size_t i = 0; i < sizeof(indices) / sizeof(*indices); i++) {
        int index = indices[i];
        ZirModule *module = &program->modules[index];
        ZirType *type = value_type(module, "Value");
        char name[64];
        snprintf(name, sizeof(name), "%s.Value", module->name);
        answer(root, name, module, type);
    }
    answer(root, "Value", NULL, NULL);
    answer(root, "unreachable.Value", NULL, NULL);
    ZirModule *late = &program->modules[2000];
    answer(root, "reach2000.Later", NULL, NULL);
    ZirType *type = value_type(late, "Later");
    answer(root, "reach2000.Later", late, type);
    strcpy(type->name, "Renamed"); SymbolLookupsChanged();
    answer(root, "reach2000.Later", NULL, NULL);
    answer(root, "reach2000.Renamed", late, type);
    type->is_public = 0; SymbolLookupsChanged();
    answer(root, "reach2000.Renamed", NULL, NULL);
    type->is_public = 1; type->is_file_private = 1; SymbolLookupsChanged();
    answer(root, "reach2000.Renamed", NULL, NULL);
    type->is_file_private = 0; SymbolLookupsChanged();
    answer(root, "reach2000.Renamed", late, type);
    root->imports[2047].resolved_module = &program->modules[1]; SymbolLookupsChanged();
    answer(root, "reach2048.Value", NULL, NULL);
    root->imports[2047].resolved_module = &program->modules[2048]; SymbolLookupsChanged();
    answer(root, "reach2048.Value", &program->modules[2048], &program->modules[2048].types[0]);
    ProgramFree(program);
}

static void shared_graph(void)
{
    ZirProgram *program = graph(6);
    ZirModule *modules = program->modules;
    link_module(&modules[0], &modules[1], 1);
    link_module(&modules[0], &modules[2], 2);
    link_module(&modules[1], &modules[3], 3);
    link_module(&modules[2], &modules[3], 3);
    link_module(&modules[3], &modules[0], 0);
    link_module(&modules[3], &modules[4], 4);
    ZirType *type = value_type(&modules[4], "Value");
    value_type(&modules[5], "Value");
    answer(&modules[0], "reach0004.Value", &modules[4], type);
    answer(&modules[0], "reach0005.Value", NULL, NULL);
    answer(&modules[0], "absent.Value", NULL, NULL);
    link_module(&modules[2], &modules[5], 5);
    answer(&modules[0], "reach0005.Value", &modules[5], &modules[5].types[0]);
    ProgramFree(program);
}

static void *deep_graph(void *unused)
{
    (void)unused;
    ZirProgram *program = graph(4096);
    for(int i = 0; i + 1 < program->module_count; i++)
        link_module(&program->modules[i], &program->modules[i + 1], i + 1);
    link_module(&program->modules[4095], &program->modules[0], 0);
    ZirType *type = value_type(&program->modules[4095], "Value");
    answer(&program->modules[0], "reach4095.Value", &program->modules[4095], type);
    answer(&program->modules[0], "absent.Value", NULL, NULL);
    ProgramFree(program);
    return NULL;
}

int main(void)
{
    wide_graph();
    shared_graph();
    pthread_attr_t attributes;
    assert(pthread_attr_init(&attributes) == 0);
    assert(pthread_attr_setstacksize(&attributes, 256 * 1024) == 0);
    pthread_t thread;
    assert(pthread_create(&thread, &attributes, deep_graph, NULL) == 0);
    assert(pthread_attr_destroy(&attributes) == 0);
    assert(pthread_join(thread, NULL) == 0);
    puts("Reachable types preserve visibility and mutation across wide, deep, cyclic and shared import graphs");
    return 0;
}
