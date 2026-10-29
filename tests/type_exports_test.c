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
        snprintf(name, sizeof(name), "export%04d", i);
        assert(ProgramAddModule(program, name, name, Span(name, 1, 1)));
    }
    return program;
}

static ZirImport *link_module(ZirModule *from, ZirModule *to)
{
    char alias[32];
    if(from->import_count) snprintf(alias, sizeof(alias), "Other%d", from->import_count);
    else strcpy(alias, "Next");
    ZirImport *import = ModuleAddImport(from, ZIR_IMPORT_MODULE, alias,
        to->name, "", 0, Span(from->name, 1, 1));
    assert(import);
    import->resolved_module = to;
    import->is_public = import->is_using = 1;
    return import;
}

static ZirType *value_type(ZirModule *module)
{
    ZirType *type = ModuleAddType(module, "Value", Span(module->name, 1, 1));
    assert(type);
    type->is_public = 1;
    strcpy(type->body, "number: s64;\n");
    return type;
}

static void answer(ZirModule *module, const char *name, ZirModule *wanted,
                   const ZirType *type)
{
    for(int repeat = 0; repeat < 3; repeat++) {
        const ZirModule *owner = module;
        assert(FindType(module, name, &owner) == type);
        assert(owner == wanted);
    }
}

static void chain_scopes(void)
{
    ZirProgram *program = graph(128);
    ZirModule *modules = program->modules;
    for(int i = 0; i < 127; i++) link_module(&modules[i], &modules[i + 1]);
    ZirType *leaf = value_type(&modules[127]);
    answer(&modules[0], "Value", &modules[127], leaf);
    answer(&modules[0], "Next.Value", &modules[127], leaf);
    modules[0].imports[0].kind = ZIR_IMPORT_OPEN;
    modules[0].imports[0].is_using = 0; SymbolLookupsChanged();
    answer(&modules[0], "Value", &modules[127], leaf);
    modules[0].imports[0].kind = ZIR_IMPORT_MODULE; SymbolLookupsChanged();
    answer(&modules[0], "Value", NULL, NULL);
    answer(&modules[0], "Next.Value", &modules[127], leaf);
    modules[0].imports[0].is_using = 1; SymbolLookupsChanged();
    ZirModule *middle = &modules[64];
    middle->imports[0].is_public = 0; SymbolLookupsChanged();
    answer(&modules[0], "Value", NULL, NULL);
    answer(middle, "Value", &modules[127], leaf);
    middle->imports[0].is_public = 1;
    middle->imports[0].is_file_private = 1;
    strcpy(middle->lookup_path, middle->name); SymbolLookupsChanged();
    answer(&modules[0], "Next.Value", NULL, NULL);
    answer(middle, "Value", &modules[127], leaf);
    strcpy(middle->lookup_path, "other.zi"); SymbolLookupsChanged();
    answer(middle, "Value", NULL, NULL);
    middle->imports[0].is_file_private = 0; middle->lookup_path[0] = 0;
    ZirType *shadow = value_type(middle);
    shadow->is_public = 0; SymbolLookupsChanged();
    answer(&modules[0], "Value", NULL, NULL);
    answer(middle, "Value", middle, shadow);
    shadow->is_public = 1; SymbolLookupsChanged();
    answer(&modules[0], "Value", middle, shadow);
    shadow->is_public = 0; shadow->is_file_private = 1;
    strcpy(middle->lookup_path, middle->name); SymbolLookupsChanged();
    answer(middle, "Value", middle, shadow);
    answer(&modules[0], "Value", &modules[127], leaf);
    leaf->is_public = 0; SymbolLookupsChanged();
    answer(&modules[0], "Value", NULL, NULL);
    leaf->is_public = 1; SymbolLookupsChanged();
    answer(&modules[0], "Missing", NULL, NULL);
    ZirType *added = ModuleAddType(&modules[127], "Missing", Span("leaf.zi", 1, 1));
    assert(added); added->is_public = 1;
    answer(&modules[0], "Missing", &modules[127], added);
    ProgramFree(program);
}

static void cycles_and_ambiguity(void)
{
    ZirProgram *program = graph(5);
    ZirModule *m = program->modules;
    link_module(&m[0], &m[1]); link_module(&m[0], &m[2]);
    link_module(&m[1], &m[2]); link_module(&m[1], &m[3]);
    link_module(&m[3], &m[1]);
    ZirType *left = value_type(&m[2]);
    answer(&m[0], "Value", &m[2], left);
    answer(&m[3], "Value", &m[2], left);
    value_type(&m[3]);
    answer(&m[0], "Value", NULL, NULL);
    /* One ambiguous facade cannot be ignored in favor of a direct import. */
    m[3].types[0].is_public = 0; SymbolLookupsChanged();
    answer(&m[0], "Value", &m[2], &m[2].types[0]);
    link_module(&m[3], &m[4]); value_type(&m[4]);
    /* A local private declaration blocks this route despite its imports. */
    answer(&m[0], "Value", &m[2], &m[2].types[0]);
    strcpy(m[3].types[0].name, "Other"); SymbolLookupsChanged();
    answer(&m[0], "Value", NULL, NULL);
    answer(&m[0], "Next.Value", NULL, NULL);
    ProgramFree(program);
}

static void *deep_cycle(void *unused)
{
    (void)unused;
    ZirProgram *program = graph(4096);
    for(int i = 0; i < 4095; i++) link_module(&program->modules[i], &program->modules[i + 1]);
    link_module(&program->modules[4095], &program->modules[0]);
    ZirType *type = value_type(&program->modules[4095]);
    answer(&program->modules[0], "Value", &program->modules[4095], type);
    answer(&program->modules[0], "Next.Value", &program->modules[4095], type);
    answer(&program->modules[0], "Absent", NULL, NULL);
    ProgramFree(program);
    return NULL;
}

int main(void)
{
    chain_scopes(); cycles_and_ambiguity();
    ZirProgram *intrinsic = graph(2);
    link_module(&intrinsic->modules[0], &intrinsic->modules[1]);
    answer(&intrinsic->modules[0], "Source_Code_Location", &intrinsic->modules[0], BuiltinType("Source_Code_Location"));
    answer(&intrinsic->modules[0], "Next.Source_Code_Location", &intrinsic->modules[1], BuiltinType("Source_Code_Location"));
    ProgramFree(intrinsic);
    pthread_attr_t attributes;
    assert(pthread_attr_init(&attributes) == 0);
    assert(pthread_attr_setstacksize(&attributes, 256 * 1024) == 0);
    pthread_t thread;
    assert(pthread_create(&thread, &attributes, deep_cycle, NULL) == 0);
    assert(pthread_attr_destroy(&attributes) == 0);
    assert(pthread_join(thread, NULL) == 0);
    puts("Type exports preserve public paths, local/file scopes, ambiguity and mutation across deep cyclic graphs");
    return 0;
}
