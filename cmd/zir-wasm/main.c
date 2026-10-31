/*
 * zi2wasm - .zi -> WebAssembly compiler. Zir is the shared frontend: every
 * input parses into a ZirProgram, checks, links, and lowers to one wasm32
 * module through zir_wasm_lower.c. The target is the portable execution
 * subset, so a program that bundles as .zx also lowers here unless it uses
 * constructs outside the current wasm subset.
 */
#include "zir.h"
#include "zir_parse.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_command.h"
#include "zir_serial.h"
#include "zir_load.h"
#include "zir_bundle.h"
#include "zir_wasm_lower.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "usage: zi2wasm [--diagnostics=text|json] [--module-path DIR] [--define NAME] "
            "--entry module:function --root DIR -o FILE.wasm file.zi ...\n");
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *output = NULL;
    const char *entry = NULL;
    char entry_module[ZIR_NAME_MAX], entry_function[ZIR_NAME_MAX];
    ProgramSet set = {0};
    CompilerContext *command_context = NULL, *previous_context = NULL;
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    const char *module_paths[64];
    int module_path_count = 0;
    const char *defines[65];
    int define_count = 0;
    ZirProgram **progs;
    int check_ok;
    int result = 1;
    int first_file = 0;
    int i;

    SetDiagnosticFormatFromArguments(argc, argv);
    CommandLowerPriority();
    const CommandOption options[] = {
        COMMAND_TEXT("--root", root),
        COMMAND_LIST("--module-path", module_paths, module_path_count, 64, NULL),
        COMMAND_LIST("--define", defines, define_count, 64, valid_define),
        COMMAND_TEXT("-o", output),
        COMMAND_TEXT("--entry", entry),
    };
    first_file = ReadCommandOptions(argc, argv, options,
                                    sizeof(options) / sizeof(*options));
    if(first_file < 0) {
        usage();
        return 1;
    }
    if(root == NULL || output == NULL || first_file == 0 || entry == NULL ||
       !split_entry(entry, entry_module, entry_function)) {
        usage();
        return 1;
    }
    command_context = CompilerContextOpen();
    if(command_context == NULL) {
        DiagnosticOutOfMemory();
        return 1;
    }
    previous_context = CompilerContextSelect(command_context);
    if(!ProgramsLoadWithDefines(&set, root, module_paths, module_path_count,
                                defines, define_count,
                                (const char *const *)(argv + first_file),
                                argc - first_file))
        goto done;
    progs = set.programs;
    check_ok = CheckCanonicalPrograms(progs, set.count,
                                      (const char *const *)set.paths);
    if(!check_ok)
        goto done;
    if(!CommandMergePrograms(&set, &merged))
        goto done;
    {
        ZirProgram *merged_ptr = &merged;
        if(!LinkImports(&merged_ptr, 1))
            goto done;
    }
    linked = NativeLink(&merged, entry_module, entry_function);
    if(linked == NULL)
        goto done;
    if(!CheckTargetCapabilities(linked, "zx"))
        goto done;
    size_t size = 0;
    unsigned char *image = zir_wasm_lower(linked, entry_module, entry_function,
                                          &size);
    if(image == NULL)
        goto done;
    FILE *out = fopen(output, "wb");
    if(out == NULL) {
        Diagnostic((ZirSourceSpan){0}, "zir.output", "cannot write %s: %s",
                   output, strerror(errno));
        free(image);
        goto done;
    }
    size_t written = fwrite(image, 1, size, out);
    int closed = fclose(out) == 0;
    free(image);
    if(written != size || !closed) {
        Diagnostic((ZirSourceSpan){0}, "zir.output", "cannot finish %s: %s",
                   output, strerror(errno));
        goto done;
    }
    result = 0;
done:
    ProgramsFree(&set);
    ProgramFree(linked);
    free(merged.modules);
    CompilerContextSelect(previous_context);
    if(command_context && !CompilerContextClose(command_context)) {
        Diagnostic((ZirSourceSpan){0}, "compiler.context",
                   "compiler operation retained live programs");
        result = 1;
    }
    return result;
}
