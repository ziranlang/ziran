#include "zir_parse.h"
#include "zir_check.h"
#include "zir_emit.h"
#include "zir_text.h"
#include "zir_load.h"
#include <assert.h>
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static int descriptors(void)
{
    DIR *directory = opendir("/proc/self/fd");
    if(!directory) return -1;
    int count = 0;
    while(readdir(directory)) count++;
    closedir(directory);
    return count;
}

static void inspect(ZirProgram *program, const char *path)
{
    assert(program && program->module_count == 1);
    assert(!strcmp(SpanPath(program->modules[0].span), path));
    assert(CheckPrograms(&program, 1));
    assert(program->modules[0].functions[0].checked);
}

static void contexts(void)
{
    CompilerContext *first = CompilerContextOpen(), *second = CompilerContextOpen();
    assert(first && second);
    CompilerContext *previous = CompilerContextSelect(first);
    assert(!CompilerContextClose(first));
    GeneratedOutputRecord("first.c");
    const char *name = KeepName("Value");
    assert(name == KeepName("Value"));
    const ZirParameters *parameters = ParametersOf("using item: Pair(s32, string) = Pair(s32, string).{}");
    assert(parameters->count == 1 && parameters->items[0].is_using);
    assert(!strcmp(parameters->items[0].binding_name, "item"));
    assert(!strcmp(parameters->items[0].value_type, "Pair(s32, string)"));
    assert(!strcmp(parameters->items[0].default_value, "Pair(s32, string).{}"));
    ZirProgram *a = parse_source_text("first.zi", "Value :: struct { first: s32 }\nAnswer :: () -> s32 { return 42 }\n");
    assert(a && a->context == first && !a->owns_context);
    CompilerContextSelect(second);
    GeneratedOutputRecord("second.c");
    assert(!strcmp(GeneratedOutputPath(0), "second.c"));
    assert(GeneratedOutputPath(1) == NULL);
    ZirProgram *b = parse_source_text("second.zi", "Value :: struct { second: string }\nAnswer :: () -> s32 { return 42 }\n");
    assert(b && b->context == second);
    CompilerContextSelect(first);
    assert(!strcmp(GeneratedOutputPath(0), "first.c"));
    assert(GeneratedOutputPath(1) == NULL);
    CompilerContextSelect(previous);
    assert(!CompilerContextClose(first));
    assert(!CompilerContextClose(second));
    inspect(a, "first.zi"); inspect(b, "second.zi");
    ZirTypeField field;
    assert(TypeFindField(&a->modules[0].types[0], "first", &field) == 1);
    assert(!strcmp(field.type, "s32"));
    assert(TypeFindField(&b->modules[0].types[0], "first", &field) == 0);
    assert(TypeFindField(&b->modules[0].types[0], "second", &field) == 1);
    assert(!strcmp(field.type, "string"));
    ProgramFree(a);
    assert(CompilerContextClose(first));
    // Closing one context cannot invalidate the other context's spans or IR.
    inspect(b, "second.zi");
    ProgramFree(b);
    assert(CompilerContextClose(second));
}

static void *worker(void *argument)
{
    (void)argument;
    const char *invalid[] = {
        "/* unfinished", "Answer :: () {", "#string END\nunfinished\n",
        "Answer :: () { if true { return }", "Bad :: struct {",
        "Tag :: enum s32 { First; Last }\nAnswer :: () {"
    };
    for(int i = 0; i < 24; i++) {
        assert(!parse_source_text("bad.zi", invalid[i % 6]));
        ZirProgram *program = parse_source_text("good.zi", "Answer :: () -> s32 { return 42 }\n");
        inspect(program, "good.zi");
        ProgramFree(program);
    }
    assert(CompilerContextCurrent() == NULL);
    return NULL;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    contexts();
    int before = descriptors();
    char imports[1024];
    char saved_root[1024], saved_imports[1024];
    assert(snprintf(imports, sizeof(imports), "%s/imports.zi", argv[2]) < (int)sizeof(imports));
    assert(snprintf(saved_root, sizeof(saved_root), "%s/saved-imports", argv[2]) < (int)sizeof(saved_root));
    assert(snprintf(saved_imports, sizeof(saved_imports), "%s/imports.zi", saved_root) < (int)sizeof(saved_imports));
    for(int i = 0; i < 32; i++) {
        assert(!parse_file(argv[1], argv[2]));
        ProgramSet set = {0};
        const char *input = argv[1];
        assert(!ProgramsLoad(&set, argv[2], NULL, 0, &input, 1));
        ProgramsFree(&set);
        input = saved_imports;
        assert(!ProgramsLoad(&set, saved_root, NULL, 0, &input, 1));
        ProgramsFree(&set);
        // The resolver loads and checks a dependency before the parent's
        // later syntax failure. The failed parent must not free that graph.
        input = imports;
        assert(!ProgramsLoad(&set, argv[2], NULL, 0, &input, 1));
        ProgramsFree(&set);
    }
    int after = descriptors();
    if(before >= 0) assert(before == after);
    pthread_t threads[4];
    for(int i = 0; i < 4; i++) assert(!pthread_create(&threads[i], NULL, worker, NULL));
    for(int i = 0; i < 4; i++) assert(!pthread_join(threads[i], NULL));
    // Semantic checking must not mark a valid empty-record body as unchecked
    // merely because the scalar-body emitter cannot represent it.
    ZirProgram *program = parse_source_text("empty.zi", "Empty :: struct {}\nAnswer :: () -> Empty { return .{} }\n");
    inspect(program, "empty.zi");
    assert(!CanEmitBody(&program->modules[0], &program->modules[0].functions[0]));
    assert(program->modules[0].functions[0].checked);
    ProgramFree(program);
    puts("Parser failures recover with independent contexts, closed streams and semantically checked bodies");
    return 0;
}
