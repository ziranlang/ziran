/*
 * zi2rust - .zi/.zir -> Rust compiler. Uses the shared Ziran frontend.
 */
#include "zir.h"
#include "zir_parse.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_command.h"
#include "zir_serial.h"
#include "zir_load.h"
#include "zir_bundle.h"
#include "zir_rust_lower.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "usage: zi2rust [--exe] [--no-main] [--entry module:function] "
            "[--bind module:function=module:function] [--bind-host module] "
            "[--diagnostics=text|json] [--module-path DIR] --root DIR -o DIR "
            "file.zi|file.zir ...\n");
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
    int no_main = 0;
    int check_ok;
    ProgramSet set = {0};
    CompilerContext *command_context = NULL, *previous_context = NULL;
    const char *module_paths[64];
    int module_path_count = 0;
    const char *bindings[64];
    int binding_count = 0;
    const char *host_modules[16];
    int host_module_count = 0;
    ZirProgram **programs;
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    int result = 1;
    int file_count;
    int first_file = 0;

    SetDiagnosticFormatFromArguments(argc, argv);
    const CommandOption options[] = {
        COMMAND_TEXT("--root", root),
        COMMAND_LIST("--module-path", module_paths, module_path_count, 64, NULL),
        COMMAND_TEXT("-o", output_directory),
        COMMAND_TEXT("--entry", entry),
        COMMAND_LIST("--bind", bindings, binding_count, 64, NULL),
        COMMAND_LIST("--bind-host", host_modules, host_module_count, 16, NULL),
        COMMAND_FLAG("--exe", executable),
        COMMAND_FLAG("--no-main", no_main),
    };
    first_file = ReadCommandOptions(argc, argv, options,
                                    sizeof(options) / sizeof(*options));
    if(first_file < 0) {
        usage();
        return 1;
    }
    if(root == NULL || output_directory == NULL || first_file == 0 ||
       (entry != NULL && !split_entry(entry, entry_module, entry_function)) ||
       ((binding_count > 0 || host_module_count > 0) && entry == NULL) ||
       (executable && (entry == NULL || no_main))) {
        usage();
        return 1;
    }

    command_context = CompilerContextOpen();
    if(command_context == NULL) { DiagnosticOutOfMemory(); goto done; }
    previous_context = CompilerContextSelect(command_context);
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
        if(!CommandMergePrograms(&set, &merged))
            goto done;
        ZirProgram *merged_pointer = &merged;
        if(!LinkImports(&merged_pointer, 1))
            goto done;
        for(int index = 0; index < binding_count; index++)
            if(!BindHostProvider(&merged, bindings[index]))
                goto done;
        for(int host = 0; host < host_module_count; host++)
            if(!BindHostModule(&merged, host_modules[host]))
                goto done;
        linked = NativeLink(&merged, entry_module, entry_function);
        if(linked == NULL)
            goto done;
        const ZirProgram *only = linked;
        if(!CheckTargetCapabilities(only, "rust"))
            goto done;
        result = rust_lower(&only, 1, output_directory, entry_module,
                            entry_function, executable) != 0;
    } else {
        for(int index = 0; index < file_count; index++)
            if(!CheckTargetCapabilities(programs[index], "rust"))
                goto done;
        result = rust_lower((const ZirProgram *const *)programs, file_count,
                            output_directory, "", "", 0) != 0;
    }
done:
    ProgramsFree(&set);
    ProgramFree(linked);
    free(merged.modules);
    CompilerContextSelect(previous_context);
    if(command_context && !CompilerContextClose(command_context)) {
        Diagnostic((ZirSourceSpan){0}, "compiler.context", "compiler operation retained live programs");
        result = 1;
    }
    return result;
}
