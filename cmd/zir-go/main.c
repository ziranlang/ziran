/*
 * zi2go - .zi -> native Go compiler. Uses the shared Ziran frontend.
 */
#include "zir.h"
#include "zir_parse.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_serial.h"
#include "zir_load.h"
#include "zir_bundle.h"
#include "zir_emit.h"
#include "zir_go_lower.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
    fprintf(stderr,
            "usage: zi2go [--no-main] [--prune-stale] [--exe] [--minify] [--pkg NAME] [--entry module:function] "
            "[--bind module:function=module:function] [--bind-host module] "
            "[--diagnostics=text|json] [--module-path DIR] --root DIR -o DIR file.zi|file.zir ...\n");
}

static int
split_entry(const char *text, char *module, char *function)
{
    const char *separator = text ? strrchr(text, ':') : NULL;
    size_t length;
    if(separator == NULL || separator == text || separator[1] == 0)
        return 0;
    length = (size_t)(separator - text);
    if(length >= ZIR_NAME_MAX || strlen(separator + 1) >= ZIR_NAME_MAX)
        return 0;
    memcpy(module, text, length);
    module[length] = 0;
    strcpy(function, separator + 1);
    return 1;
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *out_dir = NULL;
    int prune_stale = 0;
    const char *pkg = "ziran";
    const char *entry = NULL;
    char entry_module[ZIR_NAME_MAX], entry_function[ZIR_NAME_MAX];
    int no_main = 0;
    int executable = 0;
    int minify = 0;
    int check_ok;
    ProgramSet set = {0};
    const char *module_paths[64];
    int module_path_count = 0;
    const char *bindings[64];
    int binding_count = 0;
    const char *host_modules[16];
    int host_module_count = 0;
    ZirProgram **progs;
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    int result = 1;
    int file_count;
    int i;
    int first_file = 0;

    for(i = 1; i < argc; i++) {
        if(strncmp(argv[i], "--diagnostics=", 14) == 0) {
            if(!SetDiagnosticFormat(argv[i] + 14)) {
                usage();
                return 1;
            }
        } else if(strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if(strcmp(argv[i], "--module-path") == 0 && i + 1 < argc && module_path_count < 64) {
            module_paths[module_path_count++] = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if(strcmp(argv[i], "--pkg") == 0 && i + 1 < argc) {
            pkg = argv[++i];
        } else if(strcmp(argv[i], "--entry") == 0 && i + 1 < argc) {
            entry = argv[++i];
        } else if(strcmp(argv[i], "--bind") == 0 && i + 1 < argc &&
                  binding_count < 64) {
            bindings[binding_count++] = argv[++i];
        } else if(strcmp(argv[i], "--bind-host") == 0 && i + 1 < argc &&
                  host_module_count < 16) {
            host_modules[host_module_count++] = argv[++i];
        } else if(strcmp(argv[i], "--prune-stale") == 0) {
            prune_stale = 1;
        } else if(strcmp(argv[i], "--no-main") == 0) {
            no_main = 1;
        } else if(strcmp(argv[i], "--exe") == 0) {
            executable = 1;
        } else if(strcmp(argv[i], "--minify") == 0) {
            minify = 1;
        } else if(argv[i][0] == '-') {
            usage();
            return 1;
        } else {
            first_file = i;
            break;
        }
    }
    if(root == NULL || out_dir == NULL || first_file == 0 ||
       (entry != NULL && !split_entry(entry, entry_module, entry_function)) ||
       ((binding_count > 0 || host_module_count > 0) && entry == NULL) ||
       (executable && (entry == NULL || strcmp(pkg, "main") != 0 || no_main))) {
        usage();
        return 1;
    }
    EmitUseMinifiedOutput(minify);

    if(!ProgramsLoad(&set, root, module_paths, module_path_count,
                     (const char *const *)(argv + first_file), argc - first_file))
        return 1;
    file_count = set.count;
    progs = set.programs;
    check_ok = CheckCanonicalPrograms(progs, file_count,
                                      (const char *const *)set.paths);
    if(!check_ok)
        goto done;
    if(entry != NULL) {
        for(i = 0; i < file_count; i++)
            merged.module_count += progs[i]->module_count;
        merged.modules = calloc((size_t)merged.module_count,
                                sizeof(*merged.modules));
        if(merged.modules == NULL)
            goto done;
        int position = 0;
        for(i = 0; i < file_count; i++)
            for(int m = 0; m < progs[i]->module_count; m++)
                merged.modules[position++] = progs[i]->modules[m];
        ZirProgram *merged_ptr = &merged;
        if(!LinkImports(&merged_ptr, 1))
            goto done;
        for(i = 0; i < binding_count; i++)
            if(!BindHostProvider(&merged, bindings[i]))
                goto done;
        for(i = 0; i < host_module_count; i++)
            if(!BindHostModule(&merged, host_modules[i]))
                goto done;
        linked = NativeLink(&merged, entry_module, entry_function);
        if(linked == NULL)
            goto done;
        const ZirProgram *only = linked;
        if(!RejectForeignTypesExcept(only, "go:"))
            goto done;
        result = go_lower(&only, 1, root, out_dir, pkg, no_main) != 0;
        if(result == 0 && executable) {
            const ZirModule *module = NULL;
            const ZirFunction *function = NULL;
            char symbol[2 * ZIR_NAME_MAX];
            char path[1024];
            FILE *file;
            for(int m = 0; m < linked->module_count; m++) {
                const ZirModule *candidate = &linked->modules[m];
                if(strcmp(candidate->name, entry_module) != 0)
                    continue;
                for(int f = 0; f < candidate->function_count; f++)
                    if(strcmp(candidate->functions[f].name,
                              entry_function) == 0) {
                        module = candidate;
                        function = &candidate->functions[f];
                    }
            }
            if(function == NULL) {
                Diagnostic(Span("<command>", 1, 1), "zir_go.exe",
                           "executable entry is missing");
                result = 1;
                goto done;
            }
            if(FunctionArgs(function)[0] != '\0' ||
               (strcmp(function->return_type, "void") != 0 &&
                strcmp(function->return_type, "s32") != 0 &&
                strcmp(function->return_type, "s64") != 0 &&
                strcmp(function->return_type, "integer") != 0 &&
                strcmp(function->return_type, "bool") != 0)) {
                Diagnostic(function->span, "zir_go.exe",
                           "executable entry must take no arguments and return void, bool, or an integer");
                result = 1;
                goto done;
            }
            NativeGoFunctionName(&only, 1, module, function,
                                 symbol, sizeof(symbol));
            snprintf(path, sizeof(path), "%s/ziran_entry.go", out_dir);
            file = fopen(path, "wb");
            if(file == NULL) {
                Diagnostic(Span(path, 1, 1), "zir_go.exe",
                           "cannot write Go executable entry");
                result = 1;
                goto done;
            }
            if(function->return_type[0] == '\0' ||
               strcmp(function->return_type, "void") == 0) {
                fprintf(file, "package main\nfunc main() { %s() }\n", symbol);
            } else if(strcmp(function->return_type, "bool") == 0) {
                fprintf(file, "package main\nimport \"os\"\n"
                              "func main() { if %s() { os.Exit(1) } }\n",
                              symbol);
            } else {
                fprintf(file, "package main\nimport \"os\"\n"
                              "func main() { os.Exit(int(%s())) }\n", symbol);
            }
            if(fclose(file) != 0)
                result = 1;
        }
    } else {
        for(int index = 0; index < file_count; index++)
            if(!RejectForeignTypesExcept(progs[index], "go:"))
                goto done;
        result = go_lower((const ZirProgram *const *)progs, file_count,
                          root, out_dir, pkg, no_main) != 0;
    }
done:
    ProgramsFree(&set);
    ProgramFree(linked);
    free(merged.modules);
    if(result == 0 && prune_stale &&
       GeneratedOutputPrune(out_dir, "// Code generated by zi2go from ") != 0) {
        fprintf(stderr, "zi2go: cannot remove stale generated files in %s\n",
                out_dir);
        result = 1;
    }
    return result;
}
