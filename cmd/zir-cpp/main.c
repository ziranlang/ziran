/*
 * zi2cpp - .zi -> C++ compiler. Zir is the shared frontend: every .zi parses
 * into a ZirProgram and lowers to C++ source. All inputs are loaded before
 * cross-module symbol resolution.
 */
#include "zir.h"
#include "zir_parse.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_command.h"
#include "zir_serial.h"
#include "zir_load.h"
#include "zir_bundle.h"
#include "zir_cpp_lower.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "usage: zi2cpp [--no-main] [--prune-stale] [--entry module:function] "
            "[--diagnostics=text|json] [--module-path DIR] [--define NAME] --root DIR -o DIR file.zi|file.zir ...\n");
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *out_dir = NULL;
    int prune_stale = 0;
    const char *entry = NULL;
    char entry_module[ZIR_NAME_MAX], entry_function[ZIR_NAME_MAX];
    int no_main = 0;
    int check_ok;
    ProgramSet set = {0};
    CompilerContext *command_context = NULL, *previous_context = NULL;
    const char *module_paths[64];
    int module_path_count = 0;
    const char *defines[65];
    int define_count = 0;
    ZirProgram **progs;
    ZirCppModuleSyms *syms = NULL;
    int syms_count = 0;
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    int result = 1;
    int file_count;
    int symbol_count = 0;
    int i;
    int first_file = 0;

    SetDiagnosticFormatFromArguments(argc, argv);
    const CommandOption options[] = {
        COMMAND_TEXT("--root", root),
        COMMAND_LIST("--module-path", module_paths, module_path_count, 64, NULL),
        COMMAND_LIST("--define", defines, define_count, 64, valid_define),
        COMMAND_TEXT("-o", out_dir),
        COMMAND_TEXT("--entry", entry),
        COMMAND_FLAG("--prune-stale", prune_stale),
        COMMAND_FLAG("--no-main", no_main),
    };
    first_file = ReadCommandOptions(argc, argv, options,
                                    sizeof(options) / sizeof(*options));
    if(first_file < 0) {
        usage();
        return 1;
    }
    if(root == NULL || out_dir == NULL || first_file == 0 ||
       (entry != NULL && !split_entry(entry, entry_module, entry_function))) {
        usage();
        return 1;
    }
    int posix_threads = 1;
    int threads_defined = 0;
    for(int d = 0; d < define_count; d++) {
        threads_defined |= strcmp(defines[d], "POSIX_THREADS") == 0;
        if(strcmp(defines[d], "PLATFORM_WEB") == 0 ||
           strcmp(defines[d], "PLAN9") == 0)
            posix_threads = 0;
    }
    if(posix_threads && !threads_defined) {
        if(define_count == 64) {
            Diagnostic((ZirSourceSpan){0}, "command.arguments",
                       "no define slot left for native thread capability");
            return 1;
        }
        defines[define_count++] = "POSIX_THREADS";
    }
    command_context = CompilerContextOpen();
    if(command_context == NULL) { DiagnosticOutOfMemory(); goto done; }
    previous_context = CompilerContextSelect(command_context);
    if(!ProgramsLoadWithDefines(&set, root, module_paths, module_path_count,
                                defines, define_count,
                                (const char *const *)(argv + first_file), argc - first_file))
        return 1;
    file_count = set.count;
    progs = set.programs;
    check_ok = CheckCanonicalPrograms(progs, file_count,
                                      (const char *const *)set.paths);
    if(!check_ok)
        goto done;
    if(entry == NULL)
        for(i = 0; i < file_count; i++)
            if(!CheckTargetCapabilities(progs[i], "cpp"))
                goto done;
    if(entry != NULL) {
        if(!CommandMergePrograms(&set, &merged))
            goto done;
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
    syms_count = symbol_count;
    int position = 0;
    if(linked != NULL) {
        for(i = 0; i < linked->module_count; i++) {
            ZirProgram view = {0};
            view.context = linked->context;
            view.modules = &linked->modules[i];
            view.module_count = 1;
            if(!cpp_build_syms(&view, &syms[position++]))
                goto done;
        }
        if(!cpp_lower(linked, root, out_dir, syms, symbol_count))
            goto done;
    } else {
        for(i = 0; i < file_count; i++)
            for(int m = 0; m < progs[i]->module_count; m++) {
                ZirProgram view = {0};
                view.context = progs[i]->context;
                view.modules = &progs[i]->modules[m];
                view.module_count = 1;
                if(!cpp_build_syms(&view, &syms[position++]))
                    goto done;
            }
        for(i = 0; i < file_count; i++)
            if(!cpp_lower(progs[i], root, out_dir, syms, symbol_count))
                goto done;
    }
    result = 0;
done:
    (void)no_main;
    ProgramsFree(&set);
    if(syms != NULL)
        cpp_free_syms(syms, syms_count);
    free(syms);
    ProgramFree(linked);
    free(merged.modules);
    if(result == 0 && prune_stale &&
       GeneratedOutputPrune(out_dir, "/* Generated by zi2cpp from ") != 0) {
        Diagnostic((ZirSourceSpan){0}, "zir.output", "cannot remove stale generated files in %s",
                out_dir);
        result = 1;
    }
    CompilerContextSelect(previous_context);
    if(command_context && !CompilerContextClose(command_context)) {
        Diagnostic((ZirSourceSpan){0}, "compiler.context", "compiler operation retained live programs");
        result = 1;
    }
    return result;
}
