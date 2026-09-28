/*
 * zi2c - .zi -> C compiler. Zir is the shared frontend: every .zi parses
 * into a ZirProgram (zir_parse.c) and lowers to C (zir_c_lower.c). All inputs
 * are parsed first so cross-module calls resolve through one symbol table.
 */
#include "zir.h"
#include "zir_parse.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_serial.h"
#include "zir_load.h"
#include "zir_bundle.h"
#include "zir_c_lower.h"
#include "zir_c_plan9.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
    fprintf(stderr,
            "usage: zi2c [--no-main] [--plan9|--target=plan9-c] "
            "[--entry module:function] [--include-dir DIR] "
            "[--diagnostics=text|json] [--module-path DIR] [--define NAME] --root DIR -o DIR file.zi|file.zir ...\n");
}

static int
valid_define(const char *name)
{
    if(name == NULL || (!isalpha((unsigned char)*name) && *name != '_') ||
       strlen(name) >= ZIR_NAME_MAX)
        return 0;
    for(const unsigned char *p = (const unsigned char *)name + 1; *p; p++)
        if(!isalnum(*p) && *p != '_')
            return 0;
    return 1;
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
    int plan9 = 0;
    int unresolved = 0;
    int result = 1;
    ProgramSet set = {0};
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    const char *module_paths[64];
    int module_path_count = 0;
    const char *defines[64];
    int define_count = 0;
    ZirProgram **progs;
    ZirCModuleSyms *syms = NULL;
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
        } else if(strcmp(argv[i], "--define") == 0 && i + 1 < argc &&
                  define_count < 64 && valid_define(argv[i + 1])) {
            defines[define_count++] = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if(strcmp(argv[i], "--entry") == 0 && i + 1 < argc) {
            entry = argv[++i];
        } else if(strcmp(argv[i], "--no-main") == 0) {
            no_main = 1;
        } else if(strcmp(argv[i], "--plan9") == 0) {
            plan9 = 1;
        } else if(strcmp(argv[i], "--target=plan9-c") == 0) {
            plan9 = 1;
        } else if(strcmp(argv[i], "--include-dir") == 0 && i + 1 < argc) {
            c_plan9_add_include_dir(argv[++i]);
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
    if(!ProgramsLoadWithDefines(&set, root, module_paths, module_path_count,
                                defines, define_count,
                                (const char *const *)(argv + first_file),
                                argc - first_file))
        return 1;
    file_count = set.count;
    progs = set.programs;
    check_ok = CheckCanonicalPrograms(progs, file_count,
                                      (const char *const *)set.paths);
    if(!check_ok)
        goto done;
    c_plan9_set_enabled(plan9);
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
        syms = calloc((size_t)linked->module_count, sizeof(*syms));
        if(syms == NULL)
            goto done;
        for(i = 0; i < linked->module_count; i++) {
            ZirProgram view = {0};
            view.modules = &linked->modules[i];
            view.module_count = 1;
            c_build_syms(&view, &syms[i]);
        }
        if(!c_lower(linked, root, out_dir, syms, linked->module_count, 1))
            goto done;
    } else {
        for(i = 0; i < file_count; i++)
            symbol_count += progs[i]->module_count;
        syms = calloc((size_t)symbol_count, sizeof(*syms));
        if(syms == NULL)
            goto done;
        /* Pass 1: build the cross-module symbol table. */
        int position = 0;
        for(i = 0; i < file_count; i++)
            for(int m = 0; m < progs[i]->module_count; m++) {
                ZirProgram view = {0};
                view.modules = &progs[i]->modules[m];
                view.module_count = 1;
                c_build_syms(&view, &syms[position++]);
            }
        /* Pass 2: lower with full cross-module resolution. */
        for(i = 0; i < file_count; i++)
            if(!c_lower(progs[i], root, out_dir, syms, symbol_count, 0))
                goto done;
    }
    result = 0;
done:
    (void)no_main;
    ProgramsFree(&set);
    free(syms);
    ProgramFree(linked);
    free(merged.modules);
    if(result != 0)
        return result;
    unresolved = c_plan9_unresolved();
    if(plan9 && unresolved > 0) {
        /* Unresolved declarations are usually inside platform guards the
         * native build compiles out; the in-guest compile is the final
         * arbiter, so warn rather than fail. */
        fprintf(stderr,
                "zi2c: --plan9 left %d __auto_type declarations unresolved "
                "(guarded code compiles out; the rest must be resolvable)\n",
                unresolved);
    }
    return 0;
}
