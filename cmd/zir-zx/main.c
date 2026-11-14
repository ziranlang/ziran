#include "zx_exec_run.h"
#include "compiler_bundle.h"
#include "zir.h"
#include "zir_link.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_command.h"
#include "zir_host_exec.h"
#include "zir_load.h"
#include "zir_serial.h"
#include "ziran_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "usage: zi2zx file.zi|file.zir|file.zx\n"
            "       zi2zx bundle [--module-path DIR] [--define NAME] [--asset-dir NAME=DIR] [--retain module:function] [--bind module:function=module:function] [--bind-host module] --root DIR --entry module:function -o FILE file.zi|file.zir ...\n"
            "       zi2zx run file.zx\n"
            "       zi2zx run [bundle options] [--root DIR] [--entry module:function] file.zi ...\n");
}

/* `zi2zx bundle` writes revision-28 executable ZX to its -o file.
 * `zi2zx run FILE.zi` writes it to INTO instead, with FILE's folder as the
 * default root and its main as the default entry. */
static int
bundle_to(int argc, char **argv, FILE *into)
{
    CompilerConfigureTarget(ZIR_TARGET_ZX, ZIR_OS_NONE, ZIR_CPU_NONE);
    const char *root = NULL;
    const char *output = NULL;
    const char *entry = NULL;
    int first_file = -1;
    int count, result = 1;
    char entry_module[ZIR_NAME_MAX], entry_function[ZIR_NAME_MAX];
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
    ZirProgram **programs = NULL;
    ZirProgram merged = {0};
    ZxAssets assets = {0};
    Asset *views = NULL;
    unsigned char *bytes = NULL;
    for(int i = 0; i < argc; i++) {
        if(strcmp(argv[i], "--root") == 0 && i + 1 < argc)
            root = argv[++i];
        else if(strcmp(argv[i], "--module-path") == 0 && i + 1 < argc)
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
        else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
        else if(strncmp(argv[i], "--diagnostics=", 14) == 0) {
            if(!SetDiagnosticFormat(argv[i] + 14))
                return 1;
        } else if(argv[i][0] == '-') {
            usage();
            return 1;
        } else {
            first_file = i;
            break;
        }
    }
    char default_root[ZIR_PATH_MAX], default_entry[ZIR_NAME_MAX * 2];
    if(into != NULL && first_file >= 0) {
        const char *source = argv[first_file], *slash = strrchr(source, '/');
        const char *stem = slash ? slash + 1 : source, *dot = strrchr(stem, '.');
        if(root == NULL) {
            int length = slash ? (int)(slash - source) : 1;
            snprintf(default_root, sizeof(default_root), "%.*s",
                     length ? length : 1, slash ? source : ".");
            root = default_root;
        }
        if(entry == NULL) {
            snprintf(default_entry, sizeof(default_entry), "%.*s:main",
                     (int)(dot ? (size_t)(dot - stem) : strlen(stem)), stem);
            entry = default_entry;
        }
    }
    if(root == NULL || (output == NULL) == (into == NULL) || first_file < 0 ||
       !split_entry(entry, entry_module, entry_function)) {
        usage();
        return 1;
    }
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
    /* The assets as the writer takes them, borrowing their bytes. */
    views = calloc(assets.count + 1, sizeof(*views));
    if(views == NULL)
        goto done;
    for(size_t i = 0; i < assets.count; i++)
        views[i] = (Asset){{assets.items[i].name, strlen(assets.items[i].name)},
                           {(const char *)assets.items[i].data, assets.items[i].size}};
    size_t size = 0;
    bytes = ZxExecBuild(&merged, entry_module, entry_function, (Slice){views, (int64_t)assets.count}, &size);
    if(bytes == NULL)
        goto done;
    FILE *target = into != NULL ? into : fopen(output, "wb");
    int written = target != NULL && fwrite(bytes, 1, size, target) == size;
    if(target != NULL && into == NULL && fclose(target) != 0)
        written = 0;
    if(!written) {
        if(into == NULL)
            remove(output);
        Diagnostic(Span(output ? output : "<memory>", 1, 1), "zx.output", "cannot write bundle");
        goto done;
    }
    result = 0;
done:
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
    if(bundle_to(argc, argv, memory) == 0 && fflush(memory) == 0) {
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
run_command(int argc, char **argv)
{
    int dispatch_host = 0;
    for(; argc > 0 && !strncmp(argv[0], "--", 2); argc--, argv++) {
        if(!strcmp(argv[0], "--host-dispatch"))
            dispatch_host = 1;
        else if(strncmp(argv[0], "--diagnostics=", 14))
            break;
        else if(!SetDiagnosticFormat(argv[0] + 14)) {
            usage();
            return 1;
        }
    }
    int source = 0;
    for(int i = 0; i < argc; i++)
        source |= is_source(argv[i]);
    if(!source && argc != 1) {
        usage();
        return 1;
    }
    Bundle *bundle = source ? open_source(argc, argv) : BundleOpen(argv[0]);
    if(bundle == NULL)
        return 1;
    /* Discover the platform provider before running application code. */
    if(dispatch_host && !source && BundleCapabilityCount(bundle) != 0) {
        BundleClose(bundle);
        return 3;
    }
    long long result = 0;
    int has_result = 0;
    int ok = BundleRun(bundle, NULL, 0, &result, &has_result);
    if(ok && has_result) {
        if(BundleEntryReturnType(bundle)[0] == 'u')
            printf("%llu\n", (unsigned long long)result);
        else
            printf("%lld\n", result);
    }
    BundleClose(bundle);
    return ok ? 0 : 1;
}

int
main(int argc, char **argv)
{
    SetDiagnosticFormatFromArguments(argc, argv);
    if(argc > 1 && (is_source(argv[1]) ||
       (strlen(argv[1]) > 3 && strcmp(argv[1] + strlen(argv[1]) - 3, ".zx") == 0)))
        return run_command(argc - 1, argv + 1);
    if(argc > 1 && strcmp(argv[1], "bundle") == 0) {
        CommandLowerPriority();
        return bundle_to(argc - 2, argv + 2, NULL);
    }
    if(argc > 1 && strcmp(argv[1], "run") == 0)
        return run_command(argc - 2, argv + 2);
    usage();
    return 1;
}
