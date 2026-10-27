/* Lowering verified ZX bundles to C for the native engine.
 *
 * The bundle's program is written as C by the same backend native builds
 * use, with the run guards, host trampolines and the two entry points the
 * engine calls. TinyCC compiles that C in the running process; linked code
 * (`zi2zx native-c`) is compiled ahead of time into a host, such as the
 * browser's, that cannot compile at run time. Compiled code sees only the C
 * headers in zx_native_include and the engine's services. */
#include "zir_zx_native_internal.h"
#include "zir_zx_host.h"
#include "zir_bundle.h"
#include "zir_check.h"
#include "zir_emit.h"
#include "zir_native.h"
#include "../zir-c/zir_c_lower.h"

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Run guards C lowering writes when the engine asks: a step budget spent at
 * each function entry and loop iteration, a stack floor checked at entry,
 * and unreachable statements that report where they are. Every compiled
 * file includes this first; the trampoline file defines the variables,
 * which each run sets for its instance. While a parallel region runs, only
 * its workers run compiled code: the floor is off, and each worker's stack
 * is aligned to its size, so the mask finds how deep it is and where its own
 * step budget lives, so workers never write a shared counter. Module
 * globals are marked ZIR_GLOBAL. */
static const char guard_header[] =
    "#ifndef ZX_GUARD_H\n"
    "#define ZX_GUARD_H\n"
    "extern long long zx_native_budget;\n"
    "extern char *zx_native_floor;\n"
    "extern unsigned long zx_native_stack_mask;\n"
    "extern unsigned long zx_native_stack_margin;\n"
    "extern unsigned long zx_native_budget_slot;\n"
    "void ZxNativeOverrun(void);\n"
    "void ZxNativeStackOverrun(void);\n"
    "void ZxNativeUnreachable(const char *place);\n"
    "#define ZIR_GLOBAL\n"
    "#define ZIR_STEP() do { char zx_step; long long *zx_budget = zx_native_stack_mask ? "
    "(long long *)(((unsigned long)&zx_step & ~zx_native_stack_mask) + zx_native_budget_slot) : "
    "&zx_native_budget; if(--*zx_budget < 0) ZxNativeOverrun(); } while(0)\n"
    "#define ZIR_ENTER() do { char zx_here; if(&zx_here < zx_native_floor || ((unsigned long)&zx_here & zx_native_stack_mask) < zx_native_stack_margin) ZxNativeStackOverrun(); ZIR_STEP(); } while(0)\n"
    "#define ZIR_UNREACHABLE(place) ZxNativeUnreachable(place)\n"
    "#endif\n";

/* Linked code shares the host's C library, except for the functions the
 * engine serves, and runs parallel regions in place, so its guards need no
 * worker stacks. Its globals go to the program's own data section, which
 * the engine copies back before each instance starts. */
static const char linked_guard_header[] =
    "#ifndef ZX_GUARD_H\n"
    "#define ZX_GUARD_H\n"
    "#define stdout ZxNativeStdout\n"
    "#define stderr ZxNativeStderr\n"
    "#define fprintf ZxNativePrint\n"
    "#define abort ZxNativeAbort\n"
    "#define exit ZxNativeExit\n"
    "#define getenv ZxNativeEnvironment\n"
    "extern long long zx_native_budget;\n"
    "extern char *zx_native_floor;\n"
    "void ZxNativeOverrun(void);\n"
    "void ZxNativeStackOverrun(void);\n"
    "void ZxNativeUnreachable(const char *place);\n"
    "#define ZIR_GLOBAL __attribute__((section(\"%s\")))\n"
    "#define ZIR_STEP() do { if(--zx_native_budget < 0) ZxNativeOverrun(); } while(0)\n"
    "#ifdef __EMSCRIPTEN__\n"
    "unsigned long emscripten_stack_get_current(void);\n"
    "#define ZIR_ENTER() do { if((char *)emscripten_stack_get_current() < zx_native_floor) ZxNativeStackOverrun(); ZIR_STEP(); } while(0)\n"
    "#else\n"
    "#define ZIR_ENTER() do { char zx_here; if(&zx_here < zx_native_floor) ZxNativeStackOverrun(); ZIR_STEP(); } while(0)\n"
    "#endif\n"
    "#define ZIR_UNREACHABLE(place) ZxNativeUnreachable(place)\n"
    "#endif\n";

static int
write_text(const char *directory, const char *name, const char *text)
{
    char path[ZIR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE *file = fopen(path, "wb");
    if(file == NULL)
        return 0;
    int ok = fputs(text, file) >= 0;
    return fclose(file) == 0 && ok;
}

static int
remove_entry(const char *path, const struct stat *status, int flag, struct FTW *walk)
{
    (void)status;
    (void)flag;
    (void)walk;
    remove(path);
    return 0;
}

void
ZxNativeRemoveTree(const char *path)
{
    nftw(path, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
}

/* The generated program's two fixed entry points: zx_native_start runs the
 * module initializers in import order and zx_native_run calls the bundle
 * entry, widening its result as the VM does. Cached objects carry them, so
 * a cache hit needs no lowering. */
static int
write_entry_points(FILE *out, const ZirProgram *program, const ZirCModuleSyms *symbols,
                   const ZirModule *owner, const ZirFunction *entry,
                   char *error, size_t size)
{
    const ZirCModuleSyms *owned = &symbols[owner - program->modules];
    const char *function = NULL;
    for(int i = 0; i < owned->fn_count && function == NULL; i++)
        if(strcmp(owned->fns[i].source, entry->name) == 0)
            function = owned->fns[i].c;
    if(function == NULL) {
        snprintf(error, size, "entry %s has no C function", entry->name);
        return 0;
    }
    const char *path = owner->source_path;
    size_t length = strlen(path);
    if(length > 3 && strcmp(path + length - 3, ".zi") == 0)
        length -= 3;
    fprintf(out, "\n#include \"%.*s.h\"\n", (int)length, path);
    char name[ZIR_NAME_MAX * 3];
    for(int i = 0; i < program->module_count; i++)
        if(ModuleNeedsStartup(&program->modules[i])) {
            NativeCModuleInitName(&program->modules[i], name, sizeof(name));
            fprintf(out, "void %s(void);\n", name);
        }
    fputs("\nvoid\nzx_native_start(void)\n{\n", out);
    for(int i = 0; i < program->module_count; i++)
        if(ModuleNeedsStartup(&program->modules[i])) {
            NativeCModuleInitName(&program->modules[i], name, sizeof(name));
            fprintf(out, "    %s();\n", name);
        }
    fputs("}\n\nlong long zx_native_budget = 9223372036854775807LL;\n"
          "char *zx_native_floor;\nunsigned long zx_native_stack_mask;\n"
          "unsigned long zx_native_stack_margin;\nunsigned long zx_native_budget_slot;\n"
          "\nint64_t\nzx_native_run(void)\n{\n", out);
    if(entry->return_type[0] == '\0' || strcmp(entry->return_type, "void") == 0)
        fprintf(out, "    %s();\n    return 0;\n}\n", function);
    else
        fprintf(out, "    return (int64_t)%s();\n}\n", function);
    return !ferror(out);
}

static int
compare_names(const void *left, const void *right)
{
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

/* Whether two modules define one C function name, as two libraries'
 * exported `main` do. Pruning to what the entry reaches resolves it. */
static int
duplicate_functions(const ZirCModuleSyms *symbols, int count)
{
    size_t total = 0;
    for(int i = 0; i < count; i++)
        total += (size_t)symbols[i].fn_count;
    const char **names = malloc((total ? total : 1) * sizeof(*names));
    if(names == NULL)
        return 1;
    size_t used = 0;
    for(int i = 0; i < count; i++)
        for(int f = 0; f < symbols[i].fn_count; f++)
            names[used++] = symbols[i].fns[f].c;
    qsort(names, used, sizeof(*names), compare_names);
    int duplicate = 0;
    for(size_t i = 1; i < used && !duplicate; i++)
        duplicate = strcmp(names[i - 1], names[i]) == 0;
    free(names);
    return duplicate;
}

/* Generated headers, read back for their #include "..." lines. */
typedef struct Header {
    char *path; /* relative to the build directory */
    int *includes;
    int include_count;
    int depth; /* 0 unvisited, -1 visiting */
} Header;

typedef struct HeaderSet {
    Header *items;
    int count;
    int capacity;
    size_t root_length;
    int failed;
} HeaderSet;

static HeaderSet *header_target;

static int
collect_header(const char *path, const struct stat *status, int flag, struct FTW *walk)
{
    (void)status;
    (void)walk;
    HeaderSet *set = header_target;
    size_t length = strlen(path);
    if(flag != FTW_F || length < 3 || strcmp(path + length - 2, ".h") != 0 ||
       length <= set->root_length || strstr(path + set->root_length, "/zx_include/") != NULL)
        return 0;
    if(set->count == set->capacity) {
        int capacity = set->capacity ? set->capacity * 2 : 64;
        Header *grown = realloc(set->items, (size_t)capacity * sizeof(*grown));
        if(grown == NULL) {
            set->failed = 1;
            return 1;
        }
        set->items = grown;
        set->capacity = capacity;
    }
    Header *header = &set->items[set->count];
    memset(header, 0, sizeof(*header));
    header->path = strdup(path + set->root_length + 1);
    if(header->path == NULL) {
        set->failed = 1;
        return 1;
    }
    set->count++;
    return 0;
}

static int
find_header(const HeaderSet *set, const char *path)
{
    for(int i = 0; i < set->count; i++)
        if(strcmp(set->items[i].path, path) == 0)
            return i;
    return -1;
}

/* Quoted includes resolve beside the including header, then at the root. */
static void
read_includes(HeaderSet *set, int index, const char *directory)
{
    Header *header = &set->items[index];
    char path[ZIR_PATH_MAX], line[ZIR_PATH_MAX], candidate[ZIR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", directory, header->path);
    FILE *file = fopen(path, "rb");
    if(file == NULL)
        return;
    const char *slash = strrchr(header->path, '/');
    int folder = slash != NULL ? (int)(slash - header->path) : 0;
    while(fgets(line, sizeof(line), file) != NULL) {
        if(strncmp(line, "#include \"", 10) != 0)
            continue;
        char *close = strchr(line + 10, '"');
        if(close == NULL)
            continue;
        *close = '\0';
        int found = -1;
        if(folder > 0) {
            snprintf(candidate, sizeof(candidate), "%.*s/%s", folder, header->path, line + 10);
            found = find_header(set, candidate);
        }
        if(found < 0)
            found = find_header(set, line + 10);
        if(found < 0)
            continue;
        int *grown = realloc(header->includes, (size_t)(header->include_count + 1) * sizeof(*grown));
        if(grown == NULL) {
            set->failed = 1;
            break;
        }
        header = &set->items[index];
        header->includes = grown;
        header->includes[header->include_count++] = found;
    }
    fclose(file);
}

/* Depth-first, writing each header after the headers it includes: the
 * order in which a C preprocessor completes them. */
static int
include_depth(HeaderSet *set, int index, int *order, int *ordered)
{
    Header *header = &set->items[index];
    if(header->depth != 0)
        return header->depth < 0 ? 0 : header->depth;
    header->depth = -1; /* include guards stop cycles */
    int deepest = 0;
    for(int i = 0; i < set->items[index].include_count; i++) {
        int depth = include_depth(set, set->items[index].includes[i], order, ordered);
        if(depth > deepest)
            deepest = depth;
    }
    order[(*ordered)++] = index;
    set->items[index].depth = deepest + 1;
    return deepest + 1;
}

/* Module headers include the headers of their imports, and TinyCC allows 32
 * nested includes, while import chains, and cycles of re-exports, can be
 * longer. Then every source first includes zx_headers.h: all generated
 * headers' text in completion order, without the includes between them.
 * Their guards are then set, so the sources' own includes add nothing. */
enum { INCLUDE_DEPTH_LIMIT = 24 };

static int
append_header(FILE *out, const HeaderSet *set, const Header *header, const char *directory)
{
    char path[ZIR_PATH_MAX], line[ZIR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", directory, header->path);
    FILE *file = fopen(path, "rb");
    if(file == NULL)
        return 0;
    while(fgets(line, sizeof(line), file) != NULL) {
        int generated = 0;
        if(strncmp(line, "#include \"", 10) == 0) {
            for(int i = 0; i < header->include_count && !generated; i++) {
                const char *name = set->items[header->includes[i]].path;
                const char *base = strrchr(name, '/');
                size_t length = strlen(base != NULL ? base + 1 : name);
                const char *quoted = line + 10;
                const char *close = strchr(quoted, '"');
                generated = close != NULL && (size_t)(close - quoted) >= length &&
                            memcmp(close - length, base != NULL ? base + 1 : name, length) == 0;
            }
        }
        if(!generated)
            fputs(line, out);
    }
    fputc('\n', out);
    int ok = !ferror(file);
    fclose(file);
    return ok;
}

static int
write_header_order(ZxNativeBuild *build, const char *directory)
{
    HeaderSet set = {.root_length = strlen(directory)};
    header_target = &set;
    nftw(directory, collect_header, 16, FTW_PHYS);
    for(int i = 0; i < set.count && !set.failed; i++)
        read_includes(&set, i, directory);
    int *order = calloc((size_t)set.count + 1, sizeof(*order));
    int ordered = 0, deepest = 0;
    for(int i = 0; order != NULL && !set.failed && i < set.count; i++) {
        int depth = include_depth(&set, i, order, &ordered);
        if(depth > deepest)
            deepest = depth;
    }
    int ok = order != NULL && !set.failed;
    build->include_headers = ok && deepest >= INCLUDE_DEPTH_LIMIT;
    if(build->include_headers) {
        char path[ZIR_PATH_MAX];
        snprintf(path, sizeof(path), "%s/zx_headers.h", directory);
        FILE *out = fopen(path, "wb");
        ok = out != NULL;
        for(int i = 0; ok && i < ordered; i++)
            ok = append_header(out, &set, &set.items[order[i]], directory);
        if(out != NULL && fclose(out) != 0)
            ok = 0;
    }
    for(int i = 0; i < set.count; i++) {
        free(set.items[i].path);
        free(set.items[i].includes);
    }
    free(set.items);
    free(order);
    return ok;
}

static int
record_renames(ZxNativeBuild *build, const ZirProgram *program, const ZxCapabilityName *names,
               size_t capability_count)
{
    for(size_t i = 0; i < capability_count; i++) {
        if(!names[i].renamed)
            continue;
        const ZirModule *module = NULL;
        for(int m = 0; m < program->module_count && module == NULL; m++)
            if(strcmp(program->modules[m].name, names[i].module) == 0)
                module = &program->modules[m];
        if(module == NULL)
            return 0;
        char **grown = realloc(build->renames, (size_t)(build->rename_count + 3) * sizeof(*grown));
        if(grown == NULL)
            return 0;
        build->renames = grown;
        char replacement[64];
        snprintf(replacement, sizeof(replacement), "zx_capability_%zu", i);
        size_t length = strlen(module->source_path);
        if(length > 3 && strcmp(module->source_path + length - 3, ".zi") == 0)
            length -= 3;
        build->renames[build->rename_count] = strndup(module->source_path, length);
        build->renames[build->rename_count + 1] = strdup(names[i].symbol);
        build->renames[build->rename_count + 2] = strdup(replacement);
        build->rename_count += 3;
        for(int k = build->rename_count - 3; k < build->rename_count; k++)
            if(build->renames[k] == NULL)
                return 0;
    }
    return 1;
}

/* The data section of a linked program's globals, named by its identity. */
static void
section_name(const unsigned char identity[32], char name[32])
{
    snprintf(name, 32, "zx_");
    for(int i = 0; i < 12; i++)
        snprintf(name + 3 + i * 2, 3, "%02x", identity[i]);
}

/* Registers linked code with the engine when the host starts. */
static int
write_registration(const char *directory, const unsigned char identity[32])
{
    char path[ZIR_PATH_MAX], section[32];
    section_name(identity, section);
    snprintf(path, sizeof(path), "%s/zx_native_program.c", directory);
    FILE *out = fopen(path, "wb");
    if(out == NULL)
        return 0;
    fprintf(out, "/* Registers this bundle's compiled program under its identity. */\n"
                 "#include <stdint.h>\n"
                 "void zx_native_start(void);\n"
                 "int64_t zx_native_run(void);\n"
                 "extern long long zx_native_budget;\n"
                 "extern char *zx_native_floor;\n"
                 "extern unsigned long zx_native_stack_mask;\n"
                 "extern unsigned long zx_native_stack_margin;\n"
                 "extern unsigned long zx_native_budget_slot;\n"
                 "extern char __start_%s[] __attribute__((weak));\n"
                 "extern char __stop_%s[] __attribute__((weak));\n"
                 "void ZxNativeRegister(const unsigned char identity[32], void (*start)(void),\n"
                 "    int64_t (*run)(void), long long *budget, char **floor,\n"
                 "    unsigned long *stack_mask, unsigned long *stack_margin,\n"
                 "    unsigned long *budget_slot, char *data, char *data_end);\n"
                 "\nstatic const unsigned char zx_native_identity[32] = {",
            section, section);
    for(int i = 0; i < 32; i++)
        fprintf(out, "%s0x%02x,", i % 8 == 0 ? "\n    " : " ", identity[i]);
    fputs("\n};\n\n__attribute__((constructor)) static void\nzx_native_register(void)\n{\n"
          "    ZxNativeRegister(zx_native_identity, zx_native_start, zx_native_run,\n"
          "        &zx_native_budget, &zx_native_floor, &zx_native_stack_mask,\n"
          "        &zx_native_stack_margin, &zx_native_budget_slot,\n", out);
    fprintf(out, "        __start_%s, __stop_%s);\n}\n", section, section);
    int ok = !ferror(out);
    return fclose(out) == 0 && ok;
}

/* Returns 1 when written, 0 on failure, and -1 when the program must first
 * be pruned to its entry. */
static int
lower_program(ZirProgram *program, const ZirModule *owner, const ZirFunction *entry,
              const char *const *capability_modules, const char *const *capability_functions,
              size_t capability_count, ZxNativeTarget target, const unsigned char identity[32],
              const char *directory, int pruned, ZxNativeBuild *build)
{
    char include[ZIR_PATH_MAX];
    ZirCModuleSyms *symbols = NULL;
    ZxCapabilityName *names = NULL;
    int ok = 0;
    CompilerContext *previous = CompilerContextSelect(program->context);
    if(!PrepareNativeNames(&program, 1))
        goto done;
    symbols = calloc((size_t)program->module_count + 1, sizeof(*symbols));
    names = calloc(capability_count + 1, sizeof(*names));
    if(symbols == NULL || names == NULL)
        goto done;
    for(int i = 0; i < program->module_count; i++) {
        ZirProgram view = {0};
        view.context = program->context;
        view.modules = &program->modules[i];
        view.module_count = 1;
        if(!c_build_syms(&view, &symbols[i]))
            goto done;
    }
    /* C lowering also writes a C main() for each module's program main. */
    int mains = 0;
    for(int m = 0; m < program->module_count; m++)
        for(int f = 0; f < program->modules[m].function_count; f++)
            mains += !program->modules[m].functions[f].is_template &&
                     NativeProgramMain(&program->modules[m], &program->modules[m].functions[f]);
    if(!pruned && (mains > 1 || duplicate_functions(symbols, program->module_count))) {
        ok = -1;
        goto done;
    }
    EmitRunGuards(1);
    int lowered = c_lower(program, ".", directory, symbols, program->module_count, 1);
    EmitRunGuards(0);
    /* Parallel regions use the engine's ziran_parallel.h, not the runtime's. */
    char runtime_parallel[ZIR_PATH_MAX];
    snprintf(runtime_parallel, sizeof(runtime_parallel), "%s/ziran_parallel.h", directory);
    unlink(runtime_parallel);
    if(!lowered || !write_header_order(build, directory))
        goto done;
    for(size_t i = 0; i < capability_count; i++) {
        names[i].module = capability_modules[i];
        names[i].function = capability_functions[i];
    }
    char path[ZIR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/zx_native_host.c", directory);
    FILE *out = fopen(path, "wb");
    if(out == NULL)
        goto done;
    ok = ZxWriteHostTrampolines(program, names, capability_count, pruned, out,
                                build->error, sizeof(build->error)) &&
         write_entry_points(out, program, symbols, owner, entry,
                            build->error, sizeof(build->error)) &&
         record_renames(build, program, names, capability_count);
    if(fclose(out) != 0)
        ok = 0;
    snprintf(include, sizeof(include), "%s/zx_include", directory);
    if(ok && mkdir(include, 0700) != 0)
        ok = 0;
    for(size_t i = 0; ok && ZxNativeHeaders[i].name != NULL; i++)
        ok = write_text(include, ZxNativeHeaders[i].name, ZxNativeHeaders[i].text);
    if(ok)
        ok = write_text(include, "zx_native_services.h", ZxNativeServicesHeader);
    if(ok && target == ZX_NATIVE_LINKED) {
        char section[32], *text = malloc(sizeof(linked_guard_header) + sizeof(section));
        section_name(identity, section);
        ok = text != NULL;
        if(ok) {
            snprintf(text, sizeof(linked_guard_header) + sizeof(section), linked_guard_header, section);
            ok = write_text(include, "zx_guard.h", text) && write_registration(directory, identity);
        }
        free(text);
    } else if(ok) {
        ok = write_text(include, "zx_guard.h", guard_header);
    }
done:
    CompilerContextSelect(previous);
    if(symbols != NULL)
        c_free_syms(symbols, program->module_count);
    free(symbols);
    free(names);
    return ok;
}

static void
free_program(const ZirProgram *context, ZirProgram *program)
{
    if(program == NULL)
        return;
    CompilerContext *previous = CompilerContextSelect(context->context);
    ProgramFree(program);
    CompilerContextSelect(previous);
}

/* The bundle's program is already pruned to its entry; lowering only
 * records native spellings in it, which the portable VM does not read.
 * Library modules may still export names the entry never reaches, which
 * the entry's linked program leaves out. */
int
ZxNativeLower(const ZirProgram *program, const ZirModule *entry_owner,
              const ZirFunction *entry, const char *const *capability_modules,
              const char *const *capability_functions, size_t capability_count,
              ZxNativeTarget target, const unsigned char identity[32],
              const char *directory, ZxNativeBuild *build)
{
    if(target == ZX_NATIVE_LINKED && identity == NULL)
        return 0;
    ZirProgram *linked = NULL;
    int ok = lower_program((ZirProgram *)program, entry_owner, entry, capability_modules,
                           capability_functions, capability_count, target, identity,
                           directory, 0, build);
    if(ok < 0) {
        ZxNativeRemoveTree(directory);
        mkdir(directory, 0700);
        CompilerContext *previous = CompilerContextSelect(program->context);
        linked = NativeLink(program, entry_owner->name, entry->name);
        CompilerContextSelect(previous);
        const ZirModule *owner = NULL;
        const ZirFunction *function = NULL;
        for(int m = 0; linked != NULL && m < linked->module_count && function == NULL; m++)
            for(int f = 0; strcmp(linked->modules[m].name, entry_owner->name) == 0 &&
                           f < linked->modules[m].function_count; f++)
                if(strcmp(linked->modules[m].functions[f].name, entry->name) == 0) {
                    owner = &linked->modules[m];
                    function = &linked->modules[m].functions[f];
                    break;
                }
        build->pruned = 1;
        ok = function != NULL &&
             lower_program(linked, owner, function, capability_modules, capability_functions,
                           capability_count, target, identity, directory, 1, build) > 0;
        if(function == NULL)
            snprintf(build->error, sizeof(build->error), "cannot link entry %s", entry->name);
    }
    free_program(program, linked);
    return ok > 0;
}

void
ZxNativeBuildFree(ZxNativeBuild *build)
{
    for(int i = 0; i < build->rename_count; i++)
        free(build->renames[i]);
    free(build->renames);
    build->renames = NULL;
    build->rename_count = 0;
}

/* ---- Linked code: what compiling it needs ---- */

static FILE *manifest_out;
static const ZxNativeBuild *manifest_build;
static size_t manifest_root;
static int manifest_first;

static void
json_text(FILE *out, const char *text, size_t length)
{
    fputc('"', out);
    for(size_t i = 0; i < length; i++) {
        if(text[i] == '"' || text[i] == '\\')
            fputc('\\', out);
        fputc(text[i], out);
    }
    fputc('"', out);
}

static int
manifest_source(const char *path, const struct stat *status, int flag, struct FTW *walk)
{
    (void)status;
    (void)walk;
    size_t length = strlen(path);
    if(flag != FTW_F || length < 3 || strcmp(path + length - 2, ".c") != 0 ||
       length <= manifest_root)
        return 0;
    const char *relative = path + manifest_root + 1;
    size_t stem_length = length - manifest_root - 3;
    fputs(manifest_first ? "\n    " : ",\n    ", manifest_out);
    manifest_first = 0;
    fputs("{\"path\": ", manifest_out);
    json_text(manifest_out, relative, strlen(relative));
    fputs(", \"defines\": [", manifest_out);
    int defines = 0;
    for(int i = 0; i < manifest_build->rename_count; i += 3)
        if(strlen(manifest_build->renames[i]) == stem_length &&
           strncmp(manifest_build->renames[i], relative, stem_length) == 0) {
            char define[ZIR_NAME_MAX * 2 + 2];
            snprintf(define, sizeof(define), "%s=%s", manifest_build->renames[i + 1],
                     manifest_build->renames[i + 2]);
            fputs(defines++ ? ", " : "", manifest_out);
            json_text(manifest_out, define, strlen(define));
        }
    fputs("]}", manifest_out);
    return 0;
}

int
ZxNativeWriteLinked(const ZirProgram *program, const ZirModule *entry_owner,
                    const ZirFunction *entry, const unsigned char identity[32],
                    const char *const *capability_modules,
                    const char *const *capability_functions, size_t capability_count,
                    const char *directory, char *error, size_t error_size)
{
    ZxNativeBuild build = {0};
    int ok = ZxNativeLower(program, entry_owner, entry, capability_modules, capability_functions,
                           capability_count, ZX_NATIVE_LINKED, identity, directory, &build);
    char path[ZIR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/zx_native.json", directory);
    FILE *out = ok ? fopen(path, "wb") : NULL;
    if(out != NULL) {
        fputs("{\n  \"identity\": \"", out);
        for(int i = 0; i < 32; i++)
            fprintf(out, "%02x", identity[i]);
        fputs("\",\n  \"entry\": ", out);
        json_text(out, entry->name, strlen(entry->name));
        fputs(",\n  \"system_include\": \"zx_include\",\n  \"include\": [\"zx_guard.h\"", out);
        if(build.include_headers)
            fputs(", \"zx_headers.h\"", out);
        fputs("],\n  \"sources\": [", out);
        manifest_out = out;
        manifest_build = &build;
        manifest_root = strlen(directory);
        manifest_first = 1;
        nftw(directory, manifest_source, 16, FTW_PHYS);
        fputs("\n  ]\n}\n", out);
        ok = !ferror(out);
        if(fclose(out) != 0)
            ok = 0;
    } else if(ok) {
        snprintf(build.error, sizeof(build.error), "cannot write %s", path);
        ok = 0;
    }
    if(!ok)
        snprintf(error, error_size, "%s", build.error[0] ? build.error : "lowering failed");
    ZxNativeBuildFree(&build);
    return ok;
}
