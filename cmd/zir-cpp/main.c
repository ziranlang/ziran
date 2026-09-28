/*
 * zi2cpp - .zi -> C++ compiler. Zir is the shared frontend: every .zi parses
 * into a ZirProgram and lowers to C++ source. All inputs are loaded before
 * cross-module symbol resolution.
 */
#include "zir.h"
#include "zir_parse.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_serial.h"
#include "zir_load.h"
#include "zir_bundle.h"
#include "zir_cpp_lower.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
    fprintf(stderr,
            "usage: zi2cpp [--no-main] [--entry module:function] "
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
    const char *entry = NULL;
    char entry_module[ZIR_NAME_MAX], entry_function[ZIR_NAME_MAX];
    int no_main = 0;
    int check_ok;
    ProgramSet set = {0};
    const char *module_paths[64];
    int module_path_count = 0;
    ZirProgram **progs;
    ZirCppModuleSyms *syms = NULL;
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    int result = 1;
    int file_count;
    int symbol_count = 0;
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
        } else if(strcmp(argv[i], "--entry") == 0 && i + 1 < argc) {
            entry = argv[++i];
        } else if(strcmp(argv[i], "--no-main") == 0) {
            no_main = 1;
        } else if(argv[i][0] == '-') {
            usage();
            return 1;
        } else {
            first_file = i;
            break;
        }
    }
    if(root == NULL || out_dir == NULL || first_file == 0 ||
       (entry != NULL && !split_entry(entry, entry_module, entry_function))) {
        usage();
        return 1;
    }
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
        linked = NativeLink(&merged, entry_module, entry_function);
        if(linked == NULL)
            goto done;
        symbol_count = linked->module_count;
    } else {
        for(i = 0; i < file_count; i++)
            symbol_count += progs[i]->module_count;
    }
    syms = calloc((size_t)symbol_count, sizeof(*syms));
    if(syms == NULL)
        goto done;
    int position = 0;
    if(linked != NULL) {
        for(i = 0; i < linked->module_count; i++) {
            ZirProgram view = {0};
            view.modules = &linked->modules[i];
            view.module_count = 1;
            cpp_build_syms(&view, &syms[position++]);
        }
        if(!cpp_lower(linked, root, out_dir, syms, symbol_count))
            goto done;
    } else {
        for(i = 0; i < file_count; i++)
            for(int m = 0; m < progs[i]->module_count; m++) {
                ZirProgram view = {0};
                view.modules = &progs[i]->modules[m];
                view.module_count = 1;
                cpp_build_syms(&view, &syms[position++]);
            }
        for(i = 0; i < file_count; i++)
            if(!cpp_lower(progs[i], root, out_dir, syms, symbol_count))
                goto done;
    }
    result = 0;
done:
    (void)no_main;
    ProgramsFree(&set);
    free(syms);
    ProgramFree(linked);
    free(merged.modules);
    return result;
}
