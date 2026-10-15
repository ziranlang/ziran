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
#include "zir_c_build.h"
#include "zir_c_plan9.h"

#include "zir_emit.h"

#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

static void
usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "usage: zi2c [--no-main] [--prune-stale] [--plan9|--target=plan9-c] "
            "[--entry module:function [--exe]] [--include-dir DIR] "
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
lock_output(const char *directory)
{
    int fd = c_build_lock(directory);
    if(fd < 0)
        Diagnostic((ZirSourceSpan){0}, "zir_c.global", "cannot prepare output directory: %s", directory);
    return fd;
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

/* The C main that runs the entry procedure, unless the entry is itself an
 * exported main. The entry takes nothing or (argc, argv) and returns
 * nothing or an integer. */
static int
write_entry_main(const ZirModule *module, const ZirFunction *fn,
                 const char *out_dir, const char *path)
{
    if(!CheckNativeEntry(fn, "c")) return 0;
    if(NativeProgramMain(module, fn))
        return 1;
    char stem[ZIR_PATH_MAX];
    copy_text(stem, sizeof(stem), module->source_path);
    size_t length = strlen(stem);
    if(length > 3 && !strcmp(stem + length - 3, ".zi"))
        stem[length - 3] = '\0';
    FILE *out = fopen(path, "w");
    if(out == NULL) {
        Diagnostic(fn->span, "zir.output", "cannot write %s: %s", path, strerror(errno));
        return 0;
    }
    fprintf(out, "/* zi2c --exe: runs %s:%s as the program. */\n", module->name, fn->name);
    fprintf(out, "#include \"%s.h\"\n", stem);
    EmitNativeMain(out, module, fn, ZIR_C);
    (void)out_dir;
    if(fclose(out) != 0) {
        Diagnostic(fn->span, "zir.output", "cannot finish %s: %s", path, strerror(errno));
        return 0;
    }
    return 1;
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
    int exe = 0;
    int output_lock = -1;
    int check_ok;
    int plan9 = 0;
    int unresolved = 0;
    int result = 1;
    ProgramSet set = {0};
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    const char *module_paths[64];
    int module_path_count = 0;
    const char *defines[65];
    int define_count = 0;
    ZirProgram **progs;
    ZirCModuleSyms *syms = NULL;
    int syms_count = 0;
    int file_count;
    int symbol_count = 0;
    int i;
    int first_file = 0;

    SetDiagnosticFormatFromArguments(argc, argv);
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
        } else if(strcmp(argv[i], "--prune-stale") == 0) {
            prune_stale = 1;
        } else if(strcmp(argv[i], "--no-main") == 0) {
            no_main = 1;
        } else if(strcmp(argv[i], "--exe") == 0) {
            exe = 1;
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
       (entry != NULL && !split_entry(entry, entry_module, entry_function)) ||
       (exe && (entry == NULL || plan9))) {
        if(exe && entry == NULL) {
            Diagnostic((ZirSourceSpan){0}, "command.arguments", "--exe needs --entry module:function");
            return 1;
        }
        usage();
        return 1;
    }
    /* Source can tell Plan 9 apart, as std/file does: #if #defined(PLAN9). */
    if(plan9 && define_count < 64) {
        int present = 0;
        for(int i = 0; i < define_count; i++)
            present |= strcmp(defines[i], "PLAN9") == 0;
        if(!present)
            defines[define_count++] = "PLAN9";
    }
    /* Native pthread adapters are available by default, including Windows
     * builds with winpthreads. Web and Plan 9 retain portable adapters. */
    int posix_threads = !plan9;
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
    if(entry == NULL)
        for(i = 0; i < file_count; i++)
            if(!CheckTargetCapabilities(progs[i], plan9 ? "plan9-c" : "c"))
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
        if(!CheckTargetCapabilities(linked, plan9 ? "plan9-c" : "c"))
            goto done;
        output_lock = lock_output(out_dir);
        if(output_lock < 0) goto done;
        syms = calloc((size_t)linked->module_count, sizeof(*syms));
        if(syms == NULL)
            goto done;
        syms_count = linked->module_count;
        for(i = 0; i < linked->module_count; i++) {
            ZirProgram view = {0};
            view.modules = &linked->modules[i];
            view.module_count = 1;
            if(!c_build_syms(&view, &syms[i]))
                goto done;
        }
        if(!c_lower(linked, root, out_dir, syms, linked->module_count, 1))
            goto done;
        char wrapper[ZIR_PATH_MAX];
        snprintf(wrapper, sizeof(wrapper), "%s/ziran_exe_main.c", out_dir);
        remove(wrapper);
        if(exe) {
            const ZirModule *entry_owner = NULL;
            const ZirFunction *entry_fn = NULL;
            for(i = 0; i < linked->module_count && entry_fn == NULL; i++) {
                if(strcmp(linked->modules[i].name, entry_module)) continue;
                for(int f = 0; f < linked->modules[i].function_count; f++)
                    if(!strcmp(linked->modules[i].functions[f].name, entry_function)) {
                        entry_owner = &linked->modules[i];
                        entry_fn = &linked->modules[i].functions[f];
                        break;
                    }
            }
            if(entry_fn == NULL) {
                DiagnosticTarget((ZirSourceSpan){0}, "zir_c.entry", "c", "entry.signature",
                                 "--exe cannot find %s", entry);
                goto done;
            }
            if(!write_entry_main(entry_owner, entry_fn, out_dir, wrapper))
                goto done;
        }
    } else {
        output_lock = lock_output(out_dir);
        if(output_lock < 0) goto done;
        for(i = 0; i < file_count; i++)
            symbol_count += progs[i]->module_count;
        syms = calloc((size_t)symbol_count, sizeof(*syms));
        if(syms == NULL)
            goto done;
        syms_count = symbol_count;
        /* Pass 1: build the cross-module symbol table. */
        int position = 0;
        for(i = 0; i < file_count; i++)
            for(int m = 0; m < progs[i]->module_count; m++) {
                ZirProgram view = {0};
                view.modules = &progs[i]->modules[m];
                view.module_count = 1;
                if(!c_build_syms(&view, &syms[position++]))
                    goto done;
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
    if(syms != NULL)
        c_free_syms(syms, syms_count);
    free(syms);
    ProgramFree(linked);
    free(merged.modules);
    if(result != 0) goto finish;
    if(prune_stale &&
       GeneratedOutputPrune(out_dir, "/* Generated by zi2c from ") != 0) {
        Diagnostic((ZirSourceSpan){0}, "zir.output", "cannot remove stale generated files in %s",
                out_dir);
        result = 1;
        goto finish;
    }
    if(exe) {
        const char *name = strrchr(entry_module, '/');
        if(!c_compile_executable(out_dir, name != NULL ? name + 1 : entry_module, posix_threads)) {
            result = 1;
            goto finish;
        }
    }
    unresolved = c_plan9_unresolved();
    if(plan9 && unresolved > 0) {
        /* Unresolved declarations are usually inside platform guards the
         * native build compiles out; the in-guest compile is the final
         * arbiter, so warn rather than fail. */
        Warning((ZirSourceSpan){0}, "zir_c.plan9",
                "--plan9 left %d __auto_type declarations unresolved "
                "(guarded code compiles out; the rest must be resolvable)",
                unresolved);
    }
finish:
    if(output_lock >= 0) close(output_lock);
    return result;
}
