/*
 * zi2py - .zi/.zir -> Python compiler. Uses the shared Ziran frontend.
 */
#include "zir.h"
#include "zir_parse.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_serial.h"
#include "zir_load.h"
#include "zir_bundle.h"
#include "zir_py_lower.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    fprintf(stderr,
            "usage: zi2py [--exe] [--prune-stale] [--entry module:function] "
            "[--bind module:function=module:function] "
            "[--diagnostics=text|json] [--module-path DIR] --root DIR -o DIR "
            "file.zi|file.zir ...\n");
}

static int split_entry(const char *text, char *module, char *function)
{
    const char *separator = text ? strrchr(text, ':') : NULL;
    size_t length;
    if(separator == NULL || separator == text || separator[1] == '\0')
        return 0;
    length = (size_t)(separator - text);
    if(length >= ZIR_NAME_MAX || strlen(separator + 1) >= ZIR_NAME_MAX)
        return 0;
    memcpy(module, text, length);
    module[length] = '\0';
    strcpy(function, separator + 1);
    return 1;
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *output_directory = NULL;
    const char *entry = NULL;
    char entry_module[ZIR_NAME_MAX];
    char entry_function[ZIR_NAME_MAX];
    int executable = 0;
    int prune_stale = 0;
    int check_ok;
    ProgramSet set = {0};
    const char *module_paths[64];
    int module_path_count = 0;
    const char *bindings[64];
    int binding_count = 0;
    ZirProgram **programs;
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    int result = 1;
    int file_count;
    int first_file = 0;

    for(int index = 1; index < argc; index++) {
        if(strncmp(argv[index], "--diagnostics=", 14) == 0) {
            if(!SetDiagnosticFormat(argv[index] + 14)) {
                usage();
                return 1;
            }
        } else if(strcmp(argv[index], "--root") == 0 && index + 1 < argc) {
            root = argv[++index];
        } else if(strcmp(argv[index], "--module-path") == 0 &&
                  index + 1 < argc && module_path_count < 64) {
            module_paths[module_path_count++] = argv[++index];
        } else if(strcmp(argv[index], "-o") == 0 && index + 1 < argc) {
            output_directory = argv[++index];
        } else if(strcmp(argv[index], "--entry") == 0 && index + 1 < argc) {
            entry = argv[++index];
        } else if(strcmp(argv[index], "--bind") == 0 && index + 1 < argc &&
                  binding_count < 64) {
            bindings[binding_count++] = argv[++index];
        } else if(strcmp(argv[index], "--exe") == 0) {
            executable = 1;
        } else if(strcmp(argv[index], "--prune-stale") == 0) {
            prune_stale = 1;
        } else if(argv[index][0] == '-') {
            usage();
            return 1;
        } else {
            first_file = index;
            break;
        }
    }
    if(root == NULL || output_directory == NULL || first_file == 0 ||
       (entry != NULL && !split_entry(entry, entry_module, entry_function)) ||
       (binding_count > 0 && entry == NULL) ||
       (executable && entry == NULL)) {
        usage();
        return 1;
    }

    if(!ProgramsLoad(&set, root, module_paths, module_path_count,
                     (const char *const *)(argv + first_file),
                     argc - first_file))
        return 1;
    file_count = set.count;
    programs = set.programs;
    check_ok = CheckCanonicalPrograms(programs, file_count,
                                      (const char *const *)set.paths);
    if(!check_ok)
        goto done;
    if(entry != NULL) {
        for(int index = 0; index < file_count; index++)
            merged.module_count += programs[index]->module_count;
        merged.modules = calloc((size_t)merged.module_count,
                                sizeof(*merged.modules));
        if(merged.modules == NULL)
            goto done;
        int position = 0;
        for(int index = 0; index < file_count; index++)
            for(int module_index = 0;
                module_index < programs[index]->module_count; module_index++)
                merged.modules[position++] =
                    programs[index]->modules[module_index];
        ZirProgram *merged_pointer = &merged;
        if(!LinkImports(&merged_pointer, 1))
            goto done;
        for(int index = 0; index < binding_count; index++)
            if(!BindHostProvider(&merged, bindings[index]))
                goto done;
        linked = NativeLink(&merged, entry_module, entry_function);
        if(linked == NULL)
            goto done;
        const ZirProgram *only = linked;
        if(!RejectForeignGoTypes(only))
            goto done;
        result = py_lower(&only, 1, output_directory, entry_module,
                            entry_function, executable) != 0;
    } else {
        for(int index = 0; index < file_count; index++)
            if(!RejectForeignGoTypes(programs[index]))
                goto done;
        result = py_lower((const ZirProgram *const *)programs, file_count,
                            output_directory, "", "", 0) != 0;
    }
done:
    ProgramsFree(&set);
    ProgramFree(linked);
    free(merged.modules);
    if(result == 0 && prune_stale &&
       GeneratedOutputPrune(output_directory, "# Code generated by zi2py from ") != 0) {
        fprintf(stderr, "zi2py: cannot remove stale generated files in %s\n",
                output_directory);
        result = 1;
    }
    return result;
}
