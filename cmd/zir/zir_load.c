#include "zir_load.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_parse.h"
#include "zir_packages.h"
#include "zir_serial.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { MAX_MODULES = 1024 };

typedef struct LoadContext {
    ProgramSet *set;
    const char *root;
    const char *const *module_paths;
    /* NAME for a --module-path NAME=DIR entry, which serves only NAME/Module
     * imports; NULL for an ordinary search path. */
    const char *const *module_path_names;
    int module_path_count;
    const char *const *defines;
    int define_count;
    const char *active[32];
    int active_count;
    ZirPackageMap *packages;
} LoadContext;

#ifndef ZIRAN_STD_DIR
#define ZIRAN_STD_DIR ""
#endif
#ifndef ZIRAN_INCLUDE_DIR
#define ZIRAN_INCLUDE_DIR ""
#endif

static int
toolchain_directory_usable(const char *path, const char *probe_file)
{
    char probe[ZIR_PATH_MAX];
    struct stat info;
    return snprintf(probe, sizeof(probe), "%s/%s", path, probe_file) <
               (int)sizeof(probe) &&
           stat(probe, &info) == 0 && S_ISREG(info.st_mode);
}

/* The environment variable if set; else NAME beside the compiler, as
 * `make install-user` and a checkout lay it out (bin/../../NAME); else the
 * source checkout the compiler was built from. Empty when none has
 * PROBE_FILE. */
static void
toolchain_directory(const char *name, const char *probe_file,
                    const char *variable, const char *built,
                    char *directory, size_t size)
{
    directory[0] = '\0';
    const char *configured = getenv(variable);
    if(configured != NULL && configured[0]) {
        if(toolchain_directory_usable(configured, probe_file))
            copy_text(directory, size, configured);
        return;
    }
    char executable[ZIR_PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if(length > 0) {
        executable[length] = '\0';
        for(int up = 0; up < 2; up++) {
            char *slash = strrchr(executable, '/');
            if(slash != NULL) *slash = '\0';
        }
        char candidate[ZIR_PATH_MAX];
        if(snprintf(candidate, sizeof(candidate), "%s/%s", executable, name) <
               (int)sizeof(candidate) &&
           toolchain_directory_usable(candidate, probe_file)) {
            copy_text(directory, size, candidate);
            return;
        }
    }
    if(built[0] && toolchain_directory_usable(built, probe_file))
        copy_text(directory, size, built);
}

const char *
ToolchainStandardDirectory(void)
{
    static int resolved;
    static char directory[ZIR_PATH_MAX];
    if(!resolved)
        toolchain_directory("std", "text.zi", "ZIRAN_STD", ZIRAN_STD_DIR,
                            directory, sizeof(directory));
    resolved = 1;
    return directory[0] ? directory : NULL;
}

const char *
ToolchainIncludeDirectory(void)
{
    static int resolved;
    static char directory[ZIR_PATH_MAX];
    if(!resolved)
        toolchain_directory("include", "zir_string.h", "ZIRAN_INCLUDE",
                            ZIRAN_INCLUDE_DIR, directory, sizeof(directory));
    resolved = 1;
    return directory[0] ? directory : NULL;
}

static int early_resolve_imports(void *context, ZirProgram *program,
                                 ZirModule *module, const char *source_path,
                                 const char *root,
                                 const char *condition);

void
ProgramsFree(ProgramSet *set)
{
    for(int i = 0; i < set->count; i++) {
        ProgramFree(set->programs[i]);
        free(set->paths[i]);
        free(set->roots[i]);
    }
    free(set->programs);
    free(set->paths);
    free(set->roots);
    *set = (ProgramSet){0};
}

static int
module_named(const ProgramSet *set, const char *target)
{
    for(int i = 0; i < set->count; i++)
        for(int m = 0; m < set->programs[i]->module_count; m++)
            if(strcmp(set->programs[i]->modules[m].name, target) == 0)
                return 1;
    return 0;
}

static int
module_loaded(const ProgramSet *set, const char *target)
{
    for(int i = 0; i < set->count; i++)
        for(int m = 0; m < set->programs[i]->module_count; m++) {
            const ZirModule *module = &set->programs[i]->modules[m];
            char stem[ZIR_PATH_MAX];
            size_t length;
            if(strcmp(module->name, target) == 0)
                return 1;
            if(snprintf(stem, sizeof(stem), "%s", module->source_path) >=
               (int)sizeof(stem))
                continue;
            length = strlen(stem);
            if(length > 3 && strcmp(stem + length - 3, ".zi") == 0)
                stem[length - 3] = '\0';
            if(strcmp(stem, target) == 0)
                return 1;
        }
    return 0;
}

static int
add_program_named(LoadContext *context, const char *path, const char *root,
                  const char *identity)
{
    ProgramSet *set = context->set;
    char *canonical = realpath(path, NULL);
    char *lexical = NULL;
    ZirProgram *program;
    if(canonical == NULL) {
        Diagnostic(Span(path, 1, 1), "module.input",
                   "cannot open module input: %s", path);
        return 0;
    }
    for(int i = 0; i < set->count; i++)
        if(strcmp(set->paths[i], canonical) == 0) {
            free(canonical);
            return 1;
        }
    for(int i = 0; i < context->active_count; i++)
        if(strcmp(context->active[i], canonical) == 0) {
            Diagnostic(Span(path, 1, 1), "module.compile_cycle",
                       "cyclic compile-time import: %s", path);
            free(canonical);
            return 0;
        }
    if(context->active_count == 32) {
        Diagnostic(Span(path, 1, 1), "module.compile_depth",
                   "compile-time import chain is too deep");
        free(canonical);
        return 0;
    }
    if(set->count >= MAX_MODULES) {
        Diagnostic(Span(path, 1, 1), "module.limit",
                   "module graph exceeds %d files", MAX_MODULES);
        free(canonical);
        return 0;
    }
    /* Canonicalize the parent directory, but preserve a symlink at the final
     * component. This gives relative and absolute inputs the same module
     * identity while allowing a project to link a shared .zi file by name. */
    const char *slash = strrchr(path, '/');
    size_t parent_length = slash == NULL ? 0 : (size_t)(slash - path);
    char *parent_path = parent_length == 0 ? strdup(slash == path ? "/" : ".") :
                        strndup(path, parent_length);
    char *parent = parent_path == NULL ? NULL : realpath(parent_path, NULL);
    free(parent_path);
    if(parent != NULL) {
        const char *leaf = slash == NULL ? path : slash + 1;
        size_t length = strlen(parent) + strlen(leaf) + 2;
        lexical = malloc(length);
        if(lexical != NULL)
            snprintf(lexical, length, "%s/%s", parent, leaf);
        free(parent);
    }
    context->active[context->active_count++] = canonical;
    size_t lexical_length = lexical == NULL ? 0 : strlen(lexical);
    program = lexical == NULL ? NULL :
        lexical_length > 3 &&
        strcmp(lexical + lexical_length - 3, ".zi") == 0 ?
        parse_file_with_imports_defined(lexical, root, early_resolve_imports,
                                        context, context->defines,
                                        context->define_count) :
        ProgramLoad(lexical, root);
    context->active_count--;
    free(lexical);
    if(program == NULL) {
        free(canonical);
        return 0;
    }
    if(identity != NULL) {
        if(program->module_count != 1 ||
           snprintf(program->modules[0].source_path,
                    sizeof(program->modules[0].source_path), "%s.zi",
                    identity) >=
               (int)sizeof(program->modules[0].source_path)) {
            Diagnostic(Span(path, 1, 1), "module.identity",
                       "directory module has an invalid identity: %s", identity);
            ProgramFree(program);
            free(canonical);
            return 0;
        }
        snprintf(program->modules[0].name,
                 sizeof(program->modules[0].name), "%s", identity);
    }
    if(context->packages != NULL && !PathIsIR(path)) {
        const char *mapped = PackageModuleName(context->packages, canonical);
        const char *owner = PackageOwner(context->packages, canonical);
        if(owner == NULL) {
            Diagnostic(Span(path, 1, 1), "package.scope",
                       "source file is outside the package graph: %s", path);
            ProgramFree(program);
            free(canonical);
            return 0;
        }
        if(strcmp(owner, "root") != 0 && mapped == NULL) {
            Diagnostic(Span(path, 1, 1), "package.module",
                       "source file is not a declared package module: %s", path);
            ProgramFree(program);
            free(canonical);
            return 0;
        }
        if(mapped != NULL) {
            if(program->module_count != 1 ||
               snprintf(program->modules[0].name,
                        sizeof(program->modules[0].name), "%s", mapped) >=
                   (int)sizeof(program->modules[0].name) ||
               snprintf(program->modules[0].source_path,
                        sizeof(program->modules[0].source_path), "%s.zi",
                        mapped) >= (int)sizeof(program->modules[0].source_path)) {
                Diagnostic(Span(path, 1, 1), "package.identity",
                           "invalid mapped module identity");
                ProgramFree(program);
                free(canonical);
                return 0;
            }
        }
    }
    for(int m = 0; m < program->module_count; m++)
        if(module_named(set, program->modules[m].name)) {
            Diagnostic(program->modules[m].span, "module.duplicate",
                       "duplicate module identity: %s",
                       program->modules[m].name);
            ProgramFree(program);
            free(canonical);
            return 0;
        }
    ZirProgram **programs = realloc(set->programs,
                                   (size_t)(set->count + 1) * sizeof(*programs));
    if(programs == NULL) {
        ProgramFree(program);
        free(canonical);
        return 0;
    }
    set->programs = programs;
    char **paths = realloc(set->paths,
                           (size_t)(set->count + 1) * sizeof(*paths));
    if(paths == NULL) {
        ProgramFree(program);
        free(canonical);
        return 0;
    }
    set->paths = paths;
    char **roots = realloc(set->roots,
                           (size_t)(set->count + 1) * sizeof(*roots));
    if(roots == NULL) {
        ProgramFree(program);
        free(canonical);
        return 0;
    }
    set->roots = roots;
    char *saved_root = strdup(root);
    if(saved_root == NULL) {
        ProgramFree(program);
        free(canonical);
        return 0;
    }
    set->programs[set->count] = program;
    set->paths[set->count] = canonical;
    set->roots[set->count] = saved_root;
    set->count++;
    return 1;
}

static int
path_within(const char *path, const char *directory)
{
    size_t length = strlen(directory);
    return strncmp(path, directory, length) == 0 &&
           (path[length] == '/' || directory[length - 1] == '/');
}

/* An input outside the root but inside a module path gets that module
 * path as its root, so passing a module by file names it the same way as
 * importing it: c_string, not its absolute location. */
static const char *
input_root(const LoadContext *context, const char *input,
           const char *canonical_root)
{
    char *canonical = realpath(input, NULL);
    const char *root = canonical_root;
    if(canonical == NULL || path_within(canonical, canonical_root)) {
        free(canonical);
        return root;
    }
    for(int i = 0; i < context->module_path_count; i++)
        if(path_within(canonical, context->module_paths[i])) {
            root = context->module_paths[i];
            break;
        }
    free(canonical);
    return root;
}

static int
add_program(LoadContext *context, const char *path, const char *root)
{
    return add_program_named(context, path, root, NULL);
}

static int
module_target_part(const char *part, size_t length)
{
    if(length == 0 || (!isalpha((unsigned char)part[0]) && part[0] != '_'))
        return 0;
    for(size_t i = 0; i < length; i++)
        if(!isalnum((unsigned char)part[i]) && part[i] != '_')
            return 0;
    return 1;
}

static int
module_target(const char *target)
{
    /* PACKAGE/Module names a dependency's export; the parser has already
     * checked that both parts are identifiers. */
    const char *slash = strchr(target, '/');
    if(slash != NULL)
        return module_target_part(target, (size_t)(slash - target)) &&
               module_target_part(slash + 1, strlen(slash + 1));
    return module_target_part(target, strlen(target));
}

static int
try_module(LoadContext *context, const char *directory, const char *load_root,
           const char *target, int prefer_ir)
{
    char candidate[ZIR_PATH_MAX * 2];
    const char *extensions[2] = {prefer_ir ? ".zir" : ".zi",
                                  prefer_ir ? ".zi" : ".zir"};
    for(int i = 0; i < 2; i++) {
        if(snprintf(candidate, sizeof(candidate), "%s/%s%s", directory,
                    target, extensions[i]) >= (int)sizeof(candidate))
            continue;
        struct stat info;
        if(stat(candidate, &info) != 0 || !S_ISREG(info.st_mode))
            continue;
        return add_program(context, candidate, load_root) ? 1 : -1;
    }
    if(snprintf(candidate, sizeof(candidate), "%s/%s/module.zi",
                directory, target) < (int)sizeof(candidate)) {
        struct stat info;
        if(stat(candidate, &info) == 0 && S_ISREG(info.st_mode)) {
            char module_root[ZIR_PATH_MAX * 2];
            if(snprintf(module_root, sizeof(module_root), "%s/%s",
                        directory, target) >= (int)sizeof(module_root))
                return -1;
            return add_program_named(context, candidate, module_root,
                                     target) ? 1 : -1;
        }
    }
    return 0;
}
/* Buffers load_import keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LoadImportBuffers {
    char directory[ZIR_PATH_MAX * 2];
    char owner_file[ZIR_PATH_MAX * 3];
    char mapped_path[ZIR_PATH_MAX * 3];
    char parent[ZIR_PATH_MAX * 3];
    char module_root[ZIR_PATH_MAX * 3];
    char candidate[ZIR_PATH_MAX * 3];
} LoadImportBuffers;

static int load_import(LoadContext *context, const char *owner_source,
            const char *owner_root, ZirImport *import);

static int
load_import_with_buffers(LoadContext *context, const char *owner_source,
            const char *owner_root, ZirImport *import, LoadImportBuffers *buffers)
{
    ProgramSet *set = context->set;
    const char *target = import->target;
    struct stat owner_info;
    const char *owner_path = owner_source;
    int prefer_ir = PathIsIR(owner_source);
    if(context->packages != NULL && !prefer_ir) {
        if(import->resolved_module != NULL &&
           module_loaded(set, import->target)) return 1;
        const char *owner = PackageOwner(context->packages, owner_source);
        char mapped_name[ZIR_NAME_MAX];
        if(owner == NULL || !PackageResolve(context->packages, owner,
                    target, buffers->mapped_path, sizeof(buffers->mapped_path), mapped_name,
                    sizeof(mapped_name))) {
            const char *choices = owner == NULL ? NULL :
                PackageAmbiguity(context->packages, owner, target);
            if(choices != NULL)
                Diagnostic(import->span, "package.ambiguous",
                           "module %s is exported by more than one dependency; "
                           "import one of %s", target, choices);
            else if(strchr(target, '/') != NULL)
                Diagnostic(import->span, "package.not_found",
                           "module %s is not exported by a direct dependency; "
                           "ziran pkg list shows the importable modules", target);
            else
                Diagnostic(import->span, "package.not_found",
                           "module %s is not in package %s or its direct dependencies (%s)",
                           target, owner == NULL ? "?" : owner, owner_source);
            return 0;
        }
        snprintf(import->target, sizeof(import->target), "%s", mapped_name);
        if(module_loaded(set, mapped_name)) return 1;
        snprintf(buffers->parent, sizeof(buffers->parent), "%s", buffers->mapped_path);
        char *slash = strrchr(buffers->parent, '/');
        if(slash == NULL) return 0;
        *slash = '\0';
        return add_program_named(context, buffers->mapped_path, buffers->parent,
                                 strcmp(slash + 1, "module.zi") == 0 ?
                                 mapped_name : NULL);
    }
    /* Without a project, PACKAGE/Module resolves through a named module
     * path, --module-path PACKAGE=DIR, and std/NAME is the standard module
     * NAME found on the module path. */
    if(!prefer_ir && strchr(target, '/') != NULL) {
        size_t prefix = (size_t)(strchr(target, '/') - target);
        int named = 0;
        for(int i = 0; i < context->module_path_count; i++)
            if(context->module_path_names[i] != NULL &&
               strlen(context->module_path_names[i]) == prefix &&
               strncmp(context->module_path_names[i], target, prefix) == 0)
                named = 1;
        if(!named && strncmp(target, "std/", 4) != 0) {
            Diagnostic(import->span, "package.project",
                       "package import %s needs a project (run with --project "
                       "next to a ziran.toml that depends on it) or "
                       "--module-path %.*s=DIR", target, (int)prefix, target);
            return 0;
        }
        char package[ZIR_NAME_MAX];
        snprintf(package, sizeof(package), "%.*s", (int)prefix, target);
        memmove(import->target, target + prefix + 1,
                strlen(target + prefix + 1) + 1);
        if(named) {
            if(module_loaded(set, target)) return 1;
            for(int i = 0; i < context->module_path_count; i++) {
                if(context->module_path_names[i] == NULL ||
                   strcmp(context->module_path_names[i], package) != 0) continue;
                int result = try_module(context, context->module_paths[i],
                                        context->module_paths[i], target,
                                        prefer_ir);
                if(result != 0)
                    return result > 0;
            }
            Diagnostic(import->span, "module.not_found",
                       "cannot find %s/%s in --module-path %s=DIR", package,
                       target, package);
            return 0;
        }
    }
    if(!prefer_ir && SpanPath(import->span)[0] != '\0') {
        if(SpanPath(import->span)[0] == '/')
            owner_path = SpanPath(import->span);
        else if(stat(SpanPath(import->span), &owner_info) == 0 &&
                S_ISREG(owner_info.st_mode))
            owner_path = SpanPath(import->span);
        else if(snprintf(buffers->owner_file, sizeof(buffers->owner_file), "%s/%s",
                         owner_root, SpanPath(import->span)) <
                (int)sizeof(buffers->owner_file))
            owner_path = buffers->owner_file;
    }
    const char *slash = strrchr(owner_path, '/');
    if(!prefer_ir && strncmp(import->signature, "dir:", 4) == 0) {
        struct stat info;
        size_t directory_length = slash == NULL ? 0 :
                                  (size_t)(slash - owner_path);
        if(slash == NULL ||
           snprintf(buffers->module_root, sizeof(buffers->module_root), "%.*s/%s",
                    (int)directory_length, owner_path,
                    import->signature + 4) >= (int)sizeof(buffers->module_root) ||
           snprintf(buffers->candidate, sizeof(buffers->candidate), "%s/module.zi",
                    buffers->module_root) >= (int)sizeof(buffers->candidate) ||
           stat(buffers->candidate, &info) != 0 || !S_ISREG(info.st_mode)) {
            Diagnostic(import->span, "module.not_found",
                       "cannot find imported directory module: %s",
                       import->signature + 4);
            return 0;
        }
        return add_program_named(context, buffers->candidate, buffers->module_root,
                                 target);
    }
    if(!prefer_ir && strncmp(import->signature, "file:", 5) == 0) {
        struct stat info;
        size_t directory_length = slash == NULL ? 0 :
                                  (size_t)(slash - owner_path);
        if(slash == NULL ||
           snprintf(buffers->candidate, sizeof(buffers->candidate), "%.*s/%s",
                    (int)directory_length, owner_path,
                    import->signature + 5) >= (int)sizeof(buffers->candidate) ||
           stat(buffers->candidate, &info) != 0 || !S_ISREG(info.st_mode)) {
            Diagnostic(import->span, "module.not_found",
                       "cannot find imported file: %s", import->signature + 5);
            return 0;
        }
        char *canonical = realpath(buffers->candidate, NULL);
        if(canonical == NULL) {
            Diagnostic(import->span, "module.not_found",
                       "cannot find imported file: %s", import->signature + 5);
            return 0;
        }
        char *parent = strrchr(canonical, '/');
        if(parent == NULL) {
            free(canonical);
            return 0;
        }
        *parent = '\0';
        int loaded = add_program(context, buffers->candidate, canonical);
        free(canonical);
        return loaded;
    }
    if(module_loaded(set, target))
        return 1;
    if(slash != NULL) {
        size_t length = (size_t)(slash - owner_path);
        if(length < sizeof(buffers->directory)) {
            memcpy(buffers->directory, owner_path, length);
            buffers->directory[length] = '\0';
            int result = try_module(context, buffers->directory, owner_root,
                                    target, prefer_ir);
            if(result != 0)
                return result > 0;
        }
    }
    int result = try_module(context, owner_root, owner_root,
                            target, prefer_ir);
    if(result != 0)
        return result > 0;
    if(strcmp(context->root, owner_root) != 0) {
        result = try_module(context, context->root, context->root,
                            target, prefer_ir);
        if(result != 0)
            return result > 0;
    }
    for(int i = 0; i < context->module_path_count; i++) {
        if(context->module_path_names[i] != NULL) continue;
        result = try_module(context, context->module_paths[i],
                            context->module_paths[i],
                            target, prefer_ir);
        if(result != 0)
            return result > 0;
    }
    /* Without a project, the standard library that came with this compiler
     * is searched last, so `#import "std/vec"` needs no --module-path. */
    const char *standard = ToolchainStandardDirectory();
    if(standard != NULL) {
        result = try_module(context, standard, standard, target, prefer_ir);
        if(result != 0)
            return result > 0;
    }
    Diagnostic(import->span, "module.not_found",
               "cannot find imported module: %s", target);
    return 0;
}

static int
load_import(LoadContext *context, const char *owner_source,
            const char *owner_root, ZirImport *import)
{
    static _Thread_local LoadImportBuffers *spares[16];
    static _Thread_local int spare_count;
    LoadImportBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = load_import_with_buffers(context, owner_source, owner_root, import, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static ZirModule *
early_target(LoadContext *context, ZirProgram *current,
             const ZirImport *import)
{
    ZirModule *found = NULL;
    for(int p = -1; p < context->set->count; p++) {
        ZirProgram *program = p < 0 ? current : context->set->programs[p];
        for(int m = 0; m < program->module_count; m++) {
            ZirModule *candidate = &program->modules[m];
            char stem[ZIR_PATH_MAX];
            size_t length;
            snprintf(stem, sizeof(stem), "%s", candidate->source_path);
            length = strlen(stem);
            if(length > 3 && strcmp(stem + length - 3, ".zi") == 0)
                stem[length - 3] = '\0';
            if(strcmp(import->target, candidate->name) != 0 &&
               strcmp(import->target, stem) != 0)
                continue;
            if(found != NULL && found != candidate) {
                Diagnostic(import->span, "module.ambiguous",
                           "ambiguous Ziran import: %s", import->target);
                return NULL;
            }
            found = candidate;
        }
    }
    return found;
}

static int
condition_needs_open_layout(const ZirModule *module, const char *condition)
{
    for(const char *cursor = condition;
        (cursor = strstr(cursor, "size_of(")) != NULL; cursor += 8) {
        const char *start = cursor + 8;
        const char *end = start;
        int depth = 1;
        while(*end && depth > 0) {
            if(*end == '(') depth++;
            else if(*end == ')') depth--;
            if(depth > 0) end++;
        }
        if(depth != 0 || (size_t)(end - start) >= ZIR_TEXT_MAX)
            continue;
        char type[ZIR_TEXT_MAX];
        memcpy(type, start, (size_t)(end - start));
        type[end - start] = '\0';
        size_t size, alignment;
        if(!TypeLayout(module, type, &size, &alignment)) return 1;
    }
    return 0;
}

static int
condition_needs_import(const char *condition, const ZirImport *import,
                       int need_open_layout)
{
    if(import->kind == ZIR_IMPORT_OPEN &&
       need_open_layout) return 1;
    for(const char *p = condition; *p;) {
        if(*p == '"' || *p == '\'') {
            char quote = *p++;
            while(*p && *p != quote) {
                if(*p == '\\' && p[1]) p++;
                p++;
            }
            if(*p) p++;
            continue;
        }
        if(!isalpha((unsigned char)*p) && *p != '_') {
            p++;
            continue;
        }
        const char *start = p;
        while(isalnum((unsigned char)*p) || *p == '_') p++;
        const char *next = p;
        while(isspace((unsigned char)*next)) next++;
        size_t length = (size_t)(p - start);
        if(import->kind == ZIR_IMPORT_MODULE && *next == '.' &&
           strlen(import->name) == length &&
           strncmp(start, import->name, length) == 0)
            return 1;
        if(import->kind == ZIR_IMPORT_OPEN && *next == '(' &&
           (start == condition || start[-1] != '.') &&
           !(length == 7 && strncmp(start, "defined", 7) == 0) &&
           !(length == 7 && strncmp(start, "size_of", 7) == 0) &&
           !(length == 7 && strncmp(start, "type_of", 7) == 0))
            return 1;
    }
    return 0;
}

static int
early_resolve_imports(void *opaque, ZirProgram *program,
                      ZirModule *module, const char *source_path,
                      const char *root, const char *condition)
{
    LoadContext *context = opaque;
    int load_all = condition == NULL;
    int need_open_layout = !load_all &&
                           condition_needs_open_layout(module, condition);
    for(int i = 0; i < module->import_count; i++) {
        ZirImport *import = &module->imports[i];
        if(import->kind != ZIR_IMPORT_OPEN &&
           import->kind != ZIR_IMPORT_MODULE) continue;
        if(!load_all && !condition_needs_import(condition, import,
                                                need_open_layout)) continue;
        ZirModule *target = context->packages == NULL ?
            early_target(context, program, import) : NULL;
        if(target == NULL) {
            if(!load_import(context, source_path, root, import))
                return 0;
            target = early_target(context, program, import);
        }
        if(target == NULL) {
            Diagnostic(import->span, "module.not_found",
                       "cannot link compile-time import: %s",
                       import->target);
            return 0;
        }
        import->resolved_module = target;
    }
    return 1;
}

static int
promote_input(ProgramSet *set, const char *path, int position)
{
    char *canonical = realpath(path, NULL);
    if(canonical == NULL) return 0;
    for(int i = 0; i < set->count; i++) {
        if(strcmp(set->paths[i], canonical) != 0) continue;
        if(i < position) {
            free(canonical);
            return 1;
        }
        ZirProgram *program = set->programs[i];
        char *saved_path = set->paths[i];
        char *saved_root = set->roots[i];
        memmove(set->programs + position + 1, set->programs + position,
                (size_t)(i - position) * sizeof(*set->programs));
        memmove(set->paths + position + 1, set->paths + position,
                (size_t)(i - position) * sizeof(*set->paths));
        memmove(set->roots + position + 1, set->roots + position,
                (size_t)(i - position) * sizeof(*set->roots));
        set->programs[position] = program;
        set->paths[position] = saved_path;
        set->roots[position] = saved_root;
        free(canonical);
        return 1;
    }
    free(canonical);
    return 0;
}

int
ProgramsLoad(ProgramSet *set, const char *root,
             const char *const *module_paths, int module_path_count,
             const char *const *inputs, int input_count)
{
    return ProgramsLoadWithDefines(set, root, module_paths,
                                   module_path_count, NULL, 0,
                                   inputs, input_count);
}

int
ProgramsLoadWithDefines(ProgramSet *set, const char *root,
                        const char *const *module_paths, int module_path_count,
                        const char *const *defines, int define_count,
                        const char *const *inputs, int input_count)
{
    char *canonical_root = realpath(root, NULL);
    char **canonical_paths = NULL;
    char **path_names = NULL;
    LoadContext context = {.set = set};
    int ok = 0;
    *set = (ProgramSet){0};
    if(canonical_root == NULL || input_count <= 0 || module_path_count < 0 ||
       define_count < 0 || (define_count > 0 && defines == NULL)) {
        Diagnostic(Span(root, 1, 1), "module.root",
                   "module root is unavailable");
        goto done;
    }
    canonical_paths = calloc((size_t)module_path_count, sizeof(*canonical_paths));
    path_names = calloc((size_t)module_path_count, sizeof(*path_names));
    if(module_path_count > 0 && (canonical_paths == NULL || path_names == NULL))
        goto done;
    for(int i = 0; i < module_path_count; i++) {
        /* NAME=DIR names a package's module directory. */
        const char *directory = module_paths[i];
        const char *equals = strchr(directory, '=');
        if(equals != NULL && equals > directory &&
           memchr(directory, '/', (size_t)(equals - directory)) == NULL &&
           module_target_part(directory, (size_t)(equals - directory))) {
            path_names[i] = strndup(directory, (size_t)(equals - directory));
            if(path_names[i] == NULL) goto done;
            directory = equals + 1;
        }
        canonical_paths[i] = realpath(directory, NULL);
        if(canonical_paths[i] == NULL) {
            Diagnostic(Span(module_paths[i], 1, 1), "module.path",
                       "module search path is unavailable");
            goto done;
        }
    }
    context.root = canonical_root;
    context.module_paths = (const char *const *)canonical_paths;
    context.module_path_names = (const char *const *)path_names;
    context.module_path_count = module_path_count;
    context.defines = defines;
    context.define_count = define_count;
    const char *package_path = getenv("ZIRAN_PACKAGE_MAP");
    if(package_path != NULL && package_path[0] != '\0') {
        context.packages = PackageMapLoad(package_path);
        if(context.packages == NULL) goto done;
    }
    for(int i = 0; i < input_count; i++)
        if(!add_program(&context, inputs[i],
                        input_root(&context, inputs[i], canonical_root)) ||
           !promote_input(set, inputs[i], i))
            goto done;
    for(int p = 0; p < set->count; p++)
        for(int m = 0; m < set->programs[p]->module_count; m++) {
            const ZirModule *module = &set->programs[p]->modules[m];
            for(int i = 0; i < module->import_count; i++) {
                ZirImport *import = &module->imports[i];
                if((import->kind != ZIR_IMPORT_OPEN &&
                    import->kind != ZIR_IMPORT_MODULE) ||
                   !module_target(import->target))
                    continue;
                if(!load_import(&context, set->paths[p], set->roots[p],
                                import))
                    goto done;
            }
        }
    ok = 1;
done:
    for(int i = 0; i < module_path_count; i++) {
        if(canonical_paths != NULL)
            free(canonical_paths[i]);
        if(path_names != NULL)
            free(path_names[i]);
    }
    free(canonical_paths);
    free(path_names);
    free(canonical_root);
    PackageMapFree(context.packages);
    if(!ok)
        ProgramsFree(set);
    return ok;
}
