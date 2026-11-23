#include "zx_exec_run.h"
#include "compiler_bundle.h"
#include "compiler_exec_link.h"
#include "zir.h"
#include "zir_link.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_command.h"
#include "zir_host_exec.h"
#include "zir_load.h"
#include "zir_serial.h"
#include "zir_text.h"
#include "ziran_host.h"
#include "ziran_zx_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

static void
usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "usage: zi2zx [--module-path DIR] [--define NAME] [--asset-dir NAME=DIR] [--retain module:function] [--bind module:function=module:function] [--bind-host module] --root DIR --entry module:function [--target zx|wasm] [--opt] [--debug] [--checks] [--emit zx|lir|opt-report] -o FILE file.zi|file.zir ...\n"
            "       zi2zx --library NAME --export module:function [--export module:function ...] [ZX build options] -o FILE file.zi|file.zir ...\n"
            "       ZX module options: [--require-module INTERFACE=NAME] [--import module:function=INTERFACE:EXPORT]\n"
            "       zi2zx [--emit zx|lir|linkage] -o FILE file.zx\n"
            "       zi2zx run [--backend native|engine] [--module-root DIR | --module-image NAME=SHA256=FILE]... file.zx\n"
            "       zi2zx run [--backend native|engine] [--module-root DIR]... [ZX build options] [--root DIR] [--entry module:function] file.zi ...\n"
            "run uses native code on linux-x86_64, linux-aarch64 and linux-armv7 and the portable engine elsewhere; a bundle\n"
            "native code cannot run fails before it starts with a named gap, and nothing falls back to the engine\n");
}

static void
opt_pass(FILE *stream, const char *name, const LirOptPassCounters *counters)
{
    fprintf(stream, "{\"name\":\"%s\",\"runs\":%lld,\"instructions\":%lld,\"values\":%lld,\"blocks\":%lld,\"constants\":%lld}",
        name, (long long)counters->runs, (long long)counters->instructions,
        (long long)counters->values, (long long)counters->blocks,
        (long long)counters->constants);
}

static int
opt_report(FILE *stream, const LirOptResult *result)
{
    const LirOptCounters *counters = &result->counters;
    fprintf(stream, "{\"version\":1,\"work\":%lld,\"scratch_bytes\":%lld,\"input\":{\"functions\":%lld,\"blocks\":%lld,\"values\":%lld,\"instructions\":%lld},",
        (long long)result->work, (long long)result->scratch_bytes,
        (long long)counters->input_functions, (long long)counters->input_blocks,
        (long long)counters->input_values, (long long)counters->input_instructions);
    fprintf(stream, "\"output\":{\"functions\":%lld,\"blocks\":%lld,\"values\":%lld,\"instructions\":%lld},\"passes\":[",
        (long long)counters->output_functions, (long long)counters->output_blocks,
        (long long)counters->output_values, (long long)counters->output_instructions);
    opt_pass(stream, "fold", &counters->fold); fputc(',', stream);
    opt_pass(stream, "copy", &counters->copy); fputc(',', stream);
    opt_pass(stream, "cse", &counters->cse); fputc(',', stream);
    opt_pass(stream, "cfg", &counters->cfg); fputc(',', stream);
    opt_pass(stream, "dead", &counters->dead);
    fputs("]}\n", stream);
    return !ferror(stream);
}

typedef enum { LinkExport, LinkRequire, LinkImport } LinkKind;

typedef struct LinkRecord {
    LinkKind kind;
    const char *name[4];
    char *storage;
    size_t length;
} LinkRecord;

static const char *const link_options[] = {"--export", "--require-module", "--import"};
static const char *const link_separators[] = {":", "=", ":=:"};

static int
link_name(const char *name, size_t length, int logical)
{
    size_t start = 0;
    char separator = logical ? '/' : '.';
    for(size_t i = 0; i <= length; i++) {
        if(i == length || name[i] == separator) {
            size_t segment = i - start;
            if(segment == 0 || (logical &&
               ((segment == 1 && name[start] == '.') ||
                (segment == 2 && name[start] == '.' && name[start + 1] == '.'))))
                return 0;
            start = i + 1;
            continue;
        }
        unsigned char c = (unsigned char)name[i];
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-' || (logical && c == '.')) continue;
        return 0;
    }
    return 1;
}

static int
parse_link_record(LinkRecord *record, LinkKind kind, const char *text)
{
    record->kind = kind;
    record->length = strlen(text);
    char *storage = malloc(record->length + 1);
    if(storage == NULL) return 0;
    memcpy(storage, text, record->length + 1);
    char *field = storage;
    const char *separator = link_separators[kind];
    for(int i = 0; ; i++) {
        char *end = *separator ? strchr(field, *separator) : storage + record->length;
        if(end == NULL || !link_name(field, (size_t)(end - field), kind == LinkRequire && i == 1)) {
            free(storage);
            return 0;
        }
        *end = 0;
        record->name[i] = field;
        if(!*separator) break;
        separator++;
        field = end + 1;
    }
    record->storage = storage;
    return 1;
}

static int
compare_link_records(const void *left, const void *right)
{
    const LinkRecord *a = left, *b = right;
    if(a->kind != b->kind) return (int)a->kind - (int)b->kind;
    if(a->kind == LinkExport) return strcmp(a->name[1], b->name[1]);
    int first = strcmp(a->name[0], b->name[0]);
    if(first != 0 || a->kind == LinkRequire) return first;
    return strcmp(a->name[1], b->name[1]);
}

static ZirModule *
link_source_module(ZirProgram *program, const char *name)
{
    ZirModule *found = NULL;
    for(int i = 0; i < program->module_count; i++) {
        ZirModule *module = &program->modules[i];
        if(strcmp(module->name, name)) continue;
        if(found != NULL) return NULL;
        found = module;
    }
    return found;
}

static ZirFunction *
link_export(ZirModule *module, const char *name)
{
    if(module == NULL) return NULL;
    ZirFunction *found = NULL;
    for(int i = 0; i < module->function_count; i++) {
        ZirFunction *function = &module->functions[i];
        if(strcmp(function->name, name)) continue;
        if(found != NULL || !function->checked || function->is_extern || function->is_template ||
           function->is_global_initializer)
            return NULL;
        found = function;
    }
    return found;
}

static const ZirImport *
link_import(ZirModule *module, const char *name)
{
    if(module == NULL) return NULL;
    const ZirImport *found = NULL;
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *decl = &module->imports[i];
        if(decl->kind != ZIR_IMPORT_EXTERN || strcmp(decl->name, name)) continue;
        if(found != NULL || decl->extern_kind != ZIR_EXTERN_HOST || decl->is_varargs ||
           !strncmp(decl->target, "ziran:", 6) ||
           (decl->extern_symbol[0] && strcmp(decl->extern_symbol, name)))
            return NULL;
        found = decl;
    }
    return found;
}

static int
link_has_requirement(const LinkRecord *records, int count, const char *interface)
{
    LinkRecord key = {.kind = LinkRequire, .name = {interface}};
    return bsearch(&key, records, (size_t)count, sizeof(*records), compare_link_records) != NULL;
}

static int
prepare_module_link(ZirProgram *program, const char *library, LinkRecord *records, int count,
                    ZxModuleBuild *build, char **text)
{
    if(count == 0) return 1;
    for(int i = 0; i < program->module_count; i++) {
        if(!strcmp(program->modules[i].name, "__zx_module_dispatch")) {
            Diagnostic(program->modules[i].span, "zx.module", "reserved module name: __zx_module_dispatch");
            return 0;
        }
    }
    qsort(records, (size_t)count, sizeof(*records), compare_link_records);
    int export_count = 0, import_count = 0;
    size_t capacity = library != NULL ? strlen(library) + sizeof("module\t\n") : 1;
    for(int i = 0; i < count; i++) {
        if(i > 0 && compare_link_records(&records[i - 1], &records[i]) == 0) {
            Diagnostic(Span("<command>", 1, 1), "zx.module", "duplicate %s key: %s%s%s",
                       link_options[records[i].kind], records[i].kind == LinkExport ? "" : records[i].name[0],
                       records[i].kind == LinkImport ? ":" : "",
                       records[i].kind == LinkRequire ? "" : records[i].name[1]);
            return 0;
        }
        if(records[i].kind == LinkExport) export_count++;
        if(records[i].kind == LinkImport) import_count++;
        // Every emitted record adds a kind, tab and line end to its option value.
        size_t overhead = sizeof("require\t\n");
        if(capacity > SIZE_MAX - overhead || records[i].length > SIZE_MAX - capacity - overhead)
            return 0;
        capacity += records[i].length + overhead;
    }
    if(export_count > 0) {
        build->exports = calloc((size_t)export_count, sizeof(*build->exports));
        if(build->exports == NULL) return 0;
    }
    if(import_count > 0) {
        build->imports = calloc((size_t)import_count, sizeof(*build->imports));
        if(build->imports == NULL) return 0;
    }
    *text = malloc(capacity);
    if(*text == NULL) return 0;
    size_t offset = 0;
    if(library != NULL)
        offset = (size_t)snprintf(*text, capacity, "module\t%s\n", library);
    for(int i = 0; i < count; i++) {
        const LinkRecord *record = &records[i];
        char *line = *text + offset;
        size_t remaining = capacity - offset;
        if(record->kind == LinkRequire) {
            offset += (size_t)snprintf(line, remaining, "require\t%s\t%s\n", record->name[0], record->name[1]);
            continue;
        }
        ZirModule *module = link_source_module(program, record->name[0]);
        if(record->kind == LinkExport) {
            ZirFunction *function = link_export(module, record->name[1]);
            if(function == NULL) {
                Diagnostic(Span("<command>", 1, 1), "zx.module-export",
                           "expected one concrete export root: %s:%s", record->name[0], record->name[1]);
                return 0;
            }
            build->exports[build->export_count++] = (AssemblyExportRoot){module, function};
            offset += (size_t)snprintf(line, remaining, "export\t%s\n", record->name[1]);
            continue;
        }
        if(!link_has_requirement(records, count, record->name[2])) {
            Diagnostic(Span("<command>", 1, 1), "zx.module-import",
                       "import interface has no --require-module: %s", record->name[2]);
            return 0;
        }
        const ZirImport *decl = link_import(module, record->name[1]);
        if(decl == NULL) {
            Diagnostic(Span("<command>", 1, 1), "zx.module-import",
                       "expected one unbound, fixed-signature host_api declaration with matching symbol: %s:%s",
                       record->name[0], record->name[1]);
            return 0;
        }
        const ZirParameters *parameters = ParametersOf(decl->args);
        if(parameters == NULL || parameters->count == 0 || parameters->items[0].value_type == NULL ||
           strcmp(parameters->items[0].value_type, "u64")) {
            Diagnostic(decl->span, "zx.module-import", "module import must take a u64 handle as its first parameter");
            return 0;
        }
        build->imports[build->import_count++] = (ZxImportRoot){record->name[0], record->name[1]};
        offset += (size_t)snprintf(line, remaining, "import\t%s\t%s\t%s\t%s\n",
            record->name[0], record->name[1], record->name[2], record->name[3]);
    }
    build->linkage = (String){*text, offset};
    return 1;
}

/* `zi2zx` writes revision-28 executable ZX to its -o file.
 * `zi2zx run FILE.zi` writes it to INTO instead, with FILE's folder as the
 * default root and its main as the default entry. */
static int
build_to(int argc, char **argv, FILE *into)
{
    const char *root = NULL;
    const char *output = NULL;
    const char *entry = NULL;
    const char *library = NULL;
    const char *emit = "zx";
    const char *compile_target = "zx";
    int optimize = 0;
    int debug = 0;
    int checks = 0;
    int first_file = -1;
    int count, result = 1;
    char entry_module[ZIR_NAME_MAX] = {0}, entry_function[ZIR_NAME_MAX] = {0};
    ProgramSet set = {0};
    const char **module_paths = CommandModulePaths(argc);
    if(module_paths == NULL) return 1;
    int module_path_count = 0;
    const char *defines[64];
    int define_count = 0;
    const char *bindings[64];
    int binding_count = 0;
    const char *host_modules[16];
    int host_module_count = 0;
    const char *retained[64];
    int retained_count = 0;
    LinkRecord *records = NULL;
    int record_count = 0;
    int export_count = 0;
    ZxModuleBuild link = {0};
    char *linkage = NULL;
    ZirProgram **programs = NULL;
    ZirProgram merged = {0};
    ZxAssets assets = {0};
    Asset *views = NULL;
    unsigned char *bytes = NULL;
    ZxLowerOwner lower = {0};
    FILE *target = NULL;
    char temporary[ZIR_PATH_MAX] = {0};
    for(int i = 0; i < argc; i++) {
        int kind;
        for(kind = LinkExport; kind <= LinkImport; kind++)
            if(!strcmp(argv[i], link_options[kind])) break;
        if(kind <= LinkImport) {
            if(records == NULL) records = calloc((size_t)argc, sizeof(*records));
            if(records == NULL) goto done;
            if(i + 1 == argc || !parse_link_record(&records[record_count], (LinkKind)kind, argv[i + 1])) {
                Diagnostic(Span("<command>", 1, 1), "zx.module", "invalid %s value: %s",
                           argv[i], i + 1 < argc ? argv[i + 1] : "<missing>");
                goto done;
            }
            record_count++;
            if(kind == LinkExport) export_count++;
            i++;
            continue;
        }
        if(strcmp(argv[i], "--root") == 0 && i + 1 < argc)
            root = argv[++i];
        else if(!strcmp(argv[i], "--library")) {
            if(library != NULL || i + 1 == argc || !link_name(argv[i + 1], strlen(argv[i + 1]), 1)) {
                Diagnostic(Span("<command>", 1, 1), "zx.module", "expected one valid --library name");
                goto done;
            }
            library = argv[++i];
        }
        else if((strcmp(argv[i], "--module-path") == 0 || strcmp(argv[i], "-I") == 0) && i + 1 < argc)
            module_paths[module_path_count++] = argv[++i];
        else if(strcmp(argv[i], "--define") == 0 && i + 1 < argc &&
                define_count < 64 && valid_define(argv[i + 1]))
            defines[define_count++] = argv[++i];
        else if(strcmp(argv[i], "--asset-dir") == 0 && i + 1 < argc) {
            if(!ZxAssetsCollect(&assets, argv[++i])) {
                Diagnostic(Span("<command>", 1, 1), "zx.assets",
                           "cannot bundle assets: %s", argv[i]);
                goto done;
            }
        }
        else if(strcmp(argv[i], "--bind") == 0 && i + 1 < argc && binding_count < 64)
            bindings[binding_count++] = argv[++i];
        else if(strcmp(argv[i], "--bind-host") == 0 && i + 1 < argc && host_module_count < 16)
            host_modules[host_module_count++] = argv[++i];
        else if(strcmp(argv[i], "--retain") == 0 && i + 1 < argc && retained_count < 64)
            retained[retained_count++] = argv[++i];
        else if(strcmp(argv[i], "--entry") == 0 && i + 1 < argc)
            entry = argv[++i];
        else if(strcmp(argv[i], "--emit") == 0 && i + 1 < argc)
            emit = argv[++i];
        else if(strcmp(argv[i], "--target") == 0 && i + 1 < argc)
            compile_target = argv[++i];
        else if(!strncmp(argv[i], "--target=", 9))
            compile_target = argv[i] + 9;
        else if(strcmp(argv[i], "--opt") == 0)
            optimize = 1;
        else if(strcmp(argv[i], "--debug") == 0)
            debug = 1;
        else if(strcmp(argv[i], "--checks") == 0)
            checks = 1;
        else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
        else if(strncmp(argv[i], "--diagnostics=", 14) == 0) {
            if(!SetDiagnosticFormat(argv[i] + 14))
                goto done;
        } else if(argv[i][0] == '-') {
            usage();
            goto done;
        } else {
            first_file = i;
            break;
        }
    }
    char default_root[ZIR_PATH_MAX], default_entry[ZIR_NAME_MAX * 2];
    if((into != NULL || library != NULL) && first_file >= 0) {
        const char *source = argv[first_file], *slash = strrchr(source, '/');
        const char *stem = slash ? slash + 1 : source, *dot = strrchr(stem, '.');
        if(root == NULL) {
            int length = slash ? (int)(slash - source) : 1;
            snprintf(default_root, sizeof(default_root), "%.*s",
                     length ? length : 1, slash ? source : ".");
            root = default_root;
        }
        if(into != NULL && entry == NULL) {
            snprintf(default_entry, sizeof(default_entry), "%.*s:main",
                     (int)(dot ? (size_t)(dot - stem) : strlen(stem)), stem);
            entry = default_entry;
        }
    }
    if((library == NULL && export_count > 0) ||
       (library != NULL && (into != NULL || entry != NULL || retained_count != 0 || export_count == 0))) {
        Diagnostic(Span("<command>", 1, 1), "zx.module",
                   "--library requires --export roots and cannot be combined with run, --entry or --retain");
        goto done;
    }
    if(root == NULL || (output == NULL) == (into == NULL) || first_file < 0 ||
       (strcmp(emit, "zx") && strcmp(emit, "lir") && strcmp(emit, "opt-report")) ||
       (strcmp(compile_target, "zx") && strcmp(compile_target, "wasm")) ||
       (!strcmp(emit, "opt-report") && !optimize) ||
       (into != NULL && (strcmp(emit, "zx") || optimize)) ||
       (library == NULL && !split_entry(entry, entry_module, entry_function))) {
        usage();
        goto done;
    }
    if(!strcmp(compile_target, "wasm"))
        CompilerConfigureTarget(ZIR_TARGET_WASM, ZIR_OS_WEB, ZIR_CPU_WASM32);
    else
        CompilerConfigureTarget(ZIR_TARGET_ZX, ZIR_OS_NONE, ZIR_CPU_NONE);
    if(!ProgramsLoadWithDefines(&set, root, module_paths, module_path_count,
                     defines, define_count,
                     (const char *const *)(argv + first_file), argc - first_file))
        goto done;
    count = set.count;
    programs = set.programs;
    for(int i = 0; i < count; i++)
        merged.module_count += programs[i]->module_count;
    if(merged.module_count == 0 ||
       !CheckCanonicalPrograms(programs, count,
                               (const char *const *)set.paths))
        goto done;
    merged.context = set.context;
    merged.checks = checks;
    if(checks) CommandEnableChecks(&set);
    merged.modules = calloc((size_t)merged.module_count, sizeof(*merged.modules));
    if(merged.modules == NULL)
        goto done;
    int position = 0;
    for(int i = 0; i < count; i++)
        for(int m = 0; m < programs[i]->module_count; m++)
            merged.modules[position++] = programs[i]->modules[m];
    ZirProgram *merged_ptr = &merged;
    if(!LinkImports(&merged_ptr, 1))
        goto done;
    for(int r = 0; r < retained_count; r++) {
        char module[ZIR_NAME_MAX], function[ZIR_NAME_MAX];
        int found = 0;
        if(split_entry(retained[r], module, function))
            for(int m = 0; m < merged.module_count; m++)
                if(strcmp(merged.modules[m].name, module) == 0)
                    for(int f = 0; f < merged.modules[m].function_count; f++)
                        if(strcmp(merged.modules[m].functions[f].name, function) == 0) {
                            merged.modules[m].functions[f].retained_root = 1;
                            found = 1;
                        }
        if(!found) {
            Diagnostic(Span("<command>", 1, 1), "zx.entry",
                       "retained function not found: %s", retained[r]);
            goto done;
        }
    }
    for(int binding = 0; binding < binding_count; binding++)
        if(!BindHostProvider(&merged, bindings[binding]))
            goto done;
    for(int host = 0; host < host_module_count; host++)
        if(!BindHostModule(&merged, host_modules[host]))
            goto done;
    if(!prepare_module_link(&merged, library, records, record_count, &link, &linkage))
        goto done;
    /* The assets as the writer takes them, borrowing their bytes. */
    views = calloc(assets.count + 1, sizeof(*views));
    if(views == NULL)
        goto done;
    for(size_t i = 0; i < assets.count; i++)
        views[i] = (Asset){{assets.items[i].name, strlen(assets.items[i].name)},
                           {(const char *)assets.items[i].data, assets.items[i].size}};
    size_t size = 0;
    int listing = !strcmp(emit, "lir");
    int reporting = !strcmp(emit, "opt-report");
    if(!listing && !reporting) {
        bytes = ZxExecBuild(&merged, entry_module, entry_function, &link,
            (Slice){views, (int64_t)assets.count}, debug, &size);
        if(bytes == NULL) goto done;
    } else if(!ZxLowerBuild(&merged, entry_module, entry_function, &link,
                           optimize, &lower))
        goto done;

    if(into != NULL)
        target = into;
    else if(!strcmp(output, "-"))
        target = stdout;
    else
        target = GeneratedOutputOpen(output, temporary, sizeof(temporary));
    if(target == NULL) {
        Diagnostic(Span(output ? output : "<memory>", 1, 1), "zx.output",
            "cannot create output");
        goto done;
    }
    int written;
    if(reporting)
        written = opt_report(target, ZxLowerOptimization(&lower));
    else if(listing)
        written = AssemblyPrintLir((LirProgram *)ZxLowerView(&lower), target);
    else
        written = fwrite(bytes, 1, size, target) == size;
    if(into == NULL && target != stdout) {
        if(fclose(target) != 0) written = 0;
        target = NULL;
        if(written && GeneratedOutputReplace(temporary, output) == 0)
            temporary[0] = 0;
        else
            written = 0;
    } else if(target == stdout && fflush(stdout) != 0)
        written = 0;
    if(!written) {
        Diagnostic(Span(output ? output : "<memory>", 1, 1), "zx.output",
            "cannot finish output");
        goto done;
    }
    result = 0;
done:
    if(target != NULL && target != into && target != stdout) fclose(target);
    if(temporary[0]) remove(temporary);
    ZxLowerFree(&lower);
    free(linkage);
    free(link.imports);
    free(link.exports);
    for(int i = 0; i < record_count; i++) free(records[i].storage);
    free(records);
    free(bytes);
    free(views);
    ZxAssetsFree(&assets);
    SymbolLookupsChanged();
    free(merged.modules);
    ProgramsFree(&set);
    return result;
}

static int
is_source(const char *path)
{
    size_t length = strlen(path);
    return (length > 3 && strcmp(path + length - 3, ".zi") == 0) ||
           (length > 4 && strcmp(path + length - 4, ".zir") == 0);
}

static int
is_zx(const char *path)
{
    size_t length = strlen(path);
    return length > 3 && !strcmp(path + length - 3, ".zx");
}

static unsigned char *
read_zx(const char *path, size_t *size)
{
    unsigned char *bytes = CommandReadBounded(path, 268435456, size);
    if(bytes == NULL)
        Diagnostic((ZirSourceSpan){0}, "zx.input", "cannot read bounded verified ZX input: %s", path);
    return bytes;
}

static int
inspect_zx(int argc, char **argv)
{
    const char *emit = "zx";
    const char *output = NULL;
    const char *input = NULL;
    for(int index = 0; index < argc; index++) {
        if(!strcmp(argv[index], "--emit") && index + 1 < argc)
            emit = argv[++index];
        else if(!strcmp(argv[index], "-o") && index + 1 < argc)
            output = argv[++index];
        else if(!strncmp(argv[index], "--diagnostics=", 14)) {
            if(!SetDiagnosticFormat(argv[index] + 14)) return 1;
        } else if(argv[index][0] == '-' || input != NULL)
            return usage(), 1;
        else
            input = argv[index];
    }
    if(input == NULL || output == NULL || !is_zx(input) ||
       (strcmp(emit, "zx") && strcmp(emit, "lir") && strcmp(emit, "linkage")))
        return usage(), 1;

    size_t size = 0;
    unsigned char *bytes = read_zx(input, &size);
    if(bytes == NULL) return 1;
    ZxExecOwner owner = {0};
    ZxExecResult read = compiler_exec_link_ZxExecRead(&owner,
        (String){(char *)bytes, size}, compiler_lowered_ir_LirDefaultLimits());
    if(read.problem != ZxExecProblem_None) {
        String problem = compiler_exec_link_ZxExecProblemName(read.problem);
        Diagnostic((ZirSourceSpan){0}, "zx.container",
            "executable ZX rejected: problem=%.*s codec=%d offset=%lld verifier=%d",
            (int)problem.length, problem.data, read.codec,
            (long long)read.offset, read.diagnostic.problem);
        free(bytes);
        return 1;
    }

    char temporary[ZIR_PATH_MAX] = {0};
    FILE *target = !strcmp(output, "-") ? stdout :
        GeneratedOutputOpen(output, temporary, sizeof(temporary));
    int status = 1;
    if(target == NULL) {
        Diagnostic((ZirSourceSpan){0}, "zx.output",
            "cannot create output: %s", output);
        goto done;
    }
    int written;
    if(!strcmp(emit, "lir")) {
        LirProgram program = compiler_exec_link_ZxExecReadProgram(&owner);
        written = AssemblyPrintLir(&program, target);
    } else if(!strcmp(emit, "linkage")) {
        String linkage = compiler_exec_link_ZxExecReadModuleLinkage(&owner);
        written = linkage.length == 0 || fwrite(linkage.data, 1, linkage.length, target) == linkage.length;
    } else {
        written = fwrite(bytes, 1, size, target) == size;
    }
    if(target == stdout) {
        if(fflush(stdout) != 0) written = 0;
    } else {
        if(fclose(target) != 0) written = 0;
        target = NULL;
        if(written && GeneratedOutputReplace(temporary, output) == 0)
            temporary[0] = 0;
        else
            written = 0;
    }
    if(!written)
        Diagnostic((ZirSourceSpan){0}, "zx.output",
            "cannot finish output: %s", output);
    else
        status = 0;
done:
    if(target != NULL && target != stdout) fclose(target);
    if(temporary[0]) remove(temporary);
    compiler_exec_link_ZxExecReadFree(&owner);
    free(bytes);
    return status;
}

/* Source runs as a bundle built here in memory. */
static Bundle *
open_source(int argc, char **argv)
{
    FILE *memory = tmpfile();
    Bundle *bundle = NULL;
    if(memory == NULL) {
        Diagnostic(Span("<command>", 1, 1), "zx.output", "cannot create a temporary bundle");
        return NULL;
    }
    if(build_to(argc, argv, memory) == 0 && fflush(memory) == 0) {
        long size = ftell(memory);
        unsigned char *data = size > 0 ? malloc((size_t)size) : NULL;
        rewind(memory);
        if(data != NULL && fread(data, 1, (size_t)size, memory) == (size_t)size)
            bundle = BundleOpenBytes(data, (size_t)size);
        free(data);
    }
    fclose(memory);
    return bundle;
}

static int
run_limit(const char *text, int64_t *value)
{
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(text, &end, 10);
    if(errno || end == text || *end || parsed > INT64_MAX) return 0;
    *value = (int64_t)parsed;
    return 1;
}

static int
run_debug_image(const char *path, LirExecLimits limits, int heap_report)
{
    size_t size = 0;
    unsigned char *bytes = read_zx(path, &size);
    if(bytes == NULL) return 1;
    ZxExecOwner owner = {0};
    ZxExecResult read = compiler_exec_link_ZxExecRead(&owner,
        (String){(char *)bytes, size}, compiler_lowered_ir_LirDefaultLimits());
    int status = 1;
    if(read.problem != ZxExecProblem_None) {
        String problem = compiler_exec_link_ZxExecProblemName(read.problem);
        Diagnostic((ZirSourceSpan){0}, "zx.container",
            "executable ZX rejected: problem=%.*s codec=%d offset=%lld verifier=%d",
            (int)problem.length, problem.data, read.codec,
            (long long)read.offset, read.diagnostic.problem);
    } else if(compiler_exec_link_ZxExecReadModuleLinkage(&owner).length != 0) {
        Diagnostic((ZirSourceSpan){0}, "zx.module-run",
            "module-linked ZX requires normal execution with --backend engine or --backend native");
    } else {
        LirProgram program = compiler_exec_link_ZxExecReadProgram(&owner);
        status = ZxExecRunProgram(&program, limits, heap_report);
    }
    compiler_exec_link_ZxExecReadFree(&owner);
    free(bytes);
    return status;
}

static int
run_command(int argc, char **argv)
{
    const char **roots = NULL;
    size_t root_count = 0;
    ZxRunModuleImage *images = NULL;
    size_t image_count = 0;
    Bundle *bundle = NULL;
    int status = 1;
    int dispatch_host = 0;
    int debug_engine = 0;
    int heap_report = 0;
    /* The platform decides the default, never the bundle: native code on
     * linux-x86_64, linux-aarch64 and linux-armv7, where a native gap is a
     * named error. */
#if (defined(__x86_64__) || defined(__aarch64__) || defined(__arm__)) && defined(__linux__)
    BundleBackendPolicy backend = BUNDLE_BACKEND_NATIVE;
#else
    BundleBackendPolicy backend = BUNDLE_BACKEND_AUTO;
#endif
    LirExecLimits limits = compiler_exec_LirExecDefaultLimits();
    for(; argc > 0 && !strncmp(argv[0], "--", 2); argc--, argv++) {
        if(!strcmp(argv[0], "--host-dispatch"))
            dispatch_host = 1;
        else if(!strcmp(argv[0], "--backend") && argc > 1) {
            if(!strcmp(argv[1], "native")) backend = BUNDLE_BACKEND_NATIVE;
            else if(!strcmp(argv[1], "engine")) backend = BUNDLE_BACKEND_ENGINE;
            else goto arguments;
            argc--;
            argv++;
        } else if(!strcmp(argv[0], "--module-root")) {
            if(argc < 2 || !argv[1][0] || !strncmp(argv[1], "--", 2)) goto arguments;
            if(roots == NULL) roots = calloc((size_t)argc, sizeof(*roots));
            if(roots == NULL) goto done;
            roots[root_count++] = argv[1];
            argc--;
            argv++;
        } else if(!strcmp(argv[0], "--module-image")) {
            if(argc < 2) goto arguments;
            if(images == NULL) images = calloc((size_t)argc, sizeof(*images));
            if(images == NULL) goto done;
            ZxRunModuleImage *image = &images[image_count];
            if(!ZxRunModuleImageParse(argv[1], image)) goto arguments;
            for(size_t i = 0; i < image_count; i++) {
                if(images[i].name_length == image->name_length &&
                   !memcmp(images[i].name, image->name, image->name_length)) goto arguments;
            }
            image_count++;
            argc--;
            argv++;
        } else if(!strcmp(argv[0], "--heap-report")) {
            heap_report = 1;
            debug_engine = 1;
        } else if((!strcmp(argv[0], "--work") || !strcmp(argv[0], "--fuel") ||
                   !strcmp(argv[0], "--frames") || !strcmp(argv[0], "--addresses")) && argc > 1) {
            int64_t value = 0;
            if(!run_limit(argv[1], &value)) goto arguments;
            if(!strcmp(argv[0], "--work")) limits.work = value;
            else if(!strcmp(argv[0], "--fuel")) limits.fuel = value;
            else if(!strcmp(argv[0], "--addresses")) limits.addresses = value;
            else limits.frames = value;
            debug_engine = 1;
            argc--;
            argv++;
        }
        else if(strncmp(argv[0], "--diagnostics=", 14))
            break;
        else if(!SetDiagnosticFormat(argv[0] + 14)) goto arguments;
    }
    int source = 0;
    for(int i = 0; i < argc; i++)
        source |= is_source(argv[i]);
    if(!source && argc != 1) goto arguments;
    if(image_count != 0 && (source || dispatch_host || debug_engine || root_count != 0)) goto arguments;
    if(debug_engine) {
        if(source || dispatch_host || root_count != 0) goto arguments;
        status = run_debug_image(argv[0], limits, heap_report);
        goto done;
    }
    bundle = source ? open_source(argc, argv) : BundleOpen(argv[0]);
    if(bundle == NULL) goto done;
    /* Discover the platform provider before running application code. */
    if(dispatch_host && !source && ZxModuleProgramHasExternalHosts(bundle)) {
        status = 3;
        goto done;
    }
    long long result = 0;
    int has_result = 0;
    /* Native preflight finishes before any initializer runs. */
    if(backend == BUNDLE_BACKEND_NATIVE) {
        const char *problem = BundleNativeProblem(bundle);
        if(problem[0] != 0) {
            Diagnostic((ZirSourceSpan){0}, "zx.native", "%s", problem);
            goto done;
        }
    }
    BundleInstanceOptions options = {backend, 0};
    int ok = ZxExecRunBundle(bundle, source ? NULL : argv[0], roots, root_count,
                             images, image_count, &options, &result, &has_result);
    if(ok && has_result) {
        if(BundleEntryReturnType(bundle)[0] == 'u')
            printf("%llu\n", (unsigned long long)result);
        else
            printf("%lld\n", result);
    }
    status = ok ? 0 : 1;
done:
    BundleClose(bundle);
    free(roots);
    free(images);
    return status;
arguments:
    usage();
    goto done;
}

int
main(int argc, char **argv)
{
    SetDiagnosticFormatFromArguments(argc, argv);
    if(argc > 1 && strcmp(argv[1], "run") == 0)
        return run_command(argc - 2, argv + 2);
    if(argc <= 1 || !strcmp(argv[1], "bundle")) return usage(), 1;
    CommandLowerPriority();
    // `zi2zx run` returns the program's own status; the other commands fail
    // only with a diagnostic.
    // `zi2zx -o FILE.zx` writes an input; only a trailing .zx that is not
    // -o's value asks to inspect one.
    if(is_zx(argv[argc - 1]) && (argc < 3 || strcmp(argv[argc - 2], "-o")))
        return DiagnosticEnsureFailureReported(inspect_zx(argc - 1, argv + 1), "zi2zx");
    return DiagnosticEnsureFailureReported(build_to(argc - 1, argv + 1, NULL), "zi2zx");
}
