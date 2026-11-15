/*
 * zi2c - .zi -> C compiler. Zir is the shared frontend: every .zi parses
 * into a ZirProgram (zir_parse.c) and lowers to C (zir_c_lower.c). All inputs
 * are parsed first so cross-module calls resolve through one symbol table.
 */
#include "zir.h"
#include "zir_parse.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_command.h"
#include "zir_serial.h"
#include "zir_load.h"
#include "zir_link.h"
#include "zir_c_lower.h"
#include "zir_c_build.h"

#include "zir_emit.h"
#include "zir_profile.h"
#include "zir_simd.h"
#include "zir_runtime.h"

#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

static void
usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "usage: zi2c [--no-main] [--prune-stale] "
            "[--entry module:function [--exe [--release]] [--bind MODULE:CAPABILITY=MODULE:EXPORT] [--bind-host MODULE]] "
            "[--diagnostics=text|json] [--module-path DIR] [--define NAME] "
            "[--target-os linux|windows|macos|android|web|none] "
            "[--target-cpu x64|arm64|wasm32|none] --root DIR -o DIR file.zi|file.zir ...\n");
}

static int
lock_output(const char *directory)
{
    int fd = c_build_lock(directory);
    if(fd < 0)
        Diagnostic((ZirSourceSpan){0}, "zir_c.global", "cannot prepare output directory: %s", directory);
    return fd;
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


static int
write_all(int fd, const char *bytes, size_t length)
{
    while(length > 0) {
        ssize_t written = write(fd, bytes, length);
        if(written < 0 && errno == EINTR) continue;
        if(written <= 0) return 0;
        bytes += written;
        length -= (size_t)written;
    }
    return 1;
}

/* Record the newline-separated paths one emission worker wrote. */
static int
read_worker_outputs(int fd)
{
    size_t used = 0, capacity = 0;
    char *text = NULL;
    int ok = 1;
    for(;;) {
        if(used + 4096 > capacity) {
            size_t grown_capacity = capacity ? capacity * 2 : 65536;
            char *grown = realloc(text, grown_capacity);
            if(grown == NULL) { ok = 0; break; }
            text = grown;
            capacity = grown_capacity;
        }
        ssize_t length = read(fd, text + used, capacity - used - 1);
        if(length < 0 && errno == EINTR) continue;
        if(length < 0) { ok = 0; break; }
        if(length == 0) break;
        used += (size_t)length;
    }
    close(fd);
    if(ok && text != NULL) {
        text[used] = 0;
        for(char *line = text, *end; *line; line = end + 1) {
            end = strchr(line, '\n');
            if(end == NULL) { ok = 0; break; }
            *end = 0;
            GeneratedOutputRecord(line);
        }
    }
    free(text);
    return ok;
}

/* Emission only reads the checked programs and writes each module's own
 * files, so forked workers share the programs copy-on-write and lower
 * disjoint sets of them on the CPUs the build may use. Each reports the
 * paths it wrote, which pruning and the native build need. Machine-readable
 * diagnostics stay serial so that every record stays whole. */
// Unlinked programs lower separately, possibly in forked workers. Their
// pruned SIMD header must contain the union of helpers, not the last module's
// subset. Collect only this invocation's recorded outputs after workers finish.
static int
finish_simd_helpers(const char *out_dir)
{
    int simd = 0;
    for(int i = 0; GeneratedOutputPath(i) != NULL; i++) {
        const char *path = GeneratedOutputPath(i);
        const char *name = strrchr(path, '/');
        if(!strcmp(name ? name + 1 : path, "ziran_simd_ops.h")) { simd = 1; break; }
    }
    if(!simd) return 1;
    ZirSimdUses uses = {0};
    int ok = 1;
    for(int i = 0; ok && GeneratedOutputPath(i) != NULL; i++) {
        const char *path = GeneratedOutputPath(i);
        size_t length = strlen(path);
        if(length < 2 || path[length - 2] != '.' ||
           (path[length - 1] != 'c' && path[length - 1] != 'h')) continue;
        const char *name = strrchr(path, '/');
        name = name ? name + 1 : path;
        int runtime = 0;
        for(size_t h = 0; ZirRuntimeHeaders[h].name != NULL; h++)
            if(!strcmp(name, ZirRuntimeHeaders[h].name)) { runtime = 1; break; }
        if(runtime) continue;
        // Scan public headers too: an inline body may be the only user of
        // a helper. Embedded runtime headers are deliberately excluded.
        FILE *file = fopen(path, "rb");
        if(file == NULL) { ok = 0; break; }
        long size = -1;
        if(fseek(file, 0, SEEK_END) == 0) size = ftell(file);
        char *text = size >= 0 && (uintmax_t)size < SIZE_MAX ? malloc((size_t)size + 1) : NULL;
        if(text == NULL || fseek(file, 0, SEEK_SET) != 0 ||
           fread(text, 1, (size_t)size, file) != (size_t)size) ok = 0;
        if(fclose(file) != 0) ok = 0;
        if(ok) {
            text[size] = '\0';
            CollectNativeSimdUses(text, &uses);
        }
        free(text);
    }
    if(ok && uses.names.count > 0) ok = EmitSimdOps(out_dir, &uses);
    SimdUsesFree(&uses);
    if(!ok) Diagnostic((ZirSourceSpan){0}, "zir_c.global", "cannot finish shared SIMD helpers in %s", out_dir);
    return ok;
}

static int
lower_programs(ZirProgram **progs, int count, const char *root, const char *out_dir,
               const ZirCModuleSyms *syms, int symbol_count)
{
    enum { MAX_EMIT_WORKERS = 16, PROGRAMS_PER_WORKER = 16 };
    int workers = c_worker_limit();
    if(workers > count / PROGRAMS_PER_WORKER) workers = count / PROGRAMS_PER_WORKER;
    if(workers > MAX_EMIT_WORKERS) workers = MAX_EMIT_WORKERS;
    if(workers < 2 || DiagnosticJsonEnabled()) {
        for(int i = 0; i < count; i++)
            if(!c_lower(progs[i], root, out_dir, syms, symbol_count, 0)) return 0;
        return finish_simd_helpers(out_dir);
    }
    int recorded = 0;
    while(GeneratedOutputPath(recorded) != NULL) recorded++;
    pid_t pids[MAX_EMIT_WORKERS];
    int reports[MAX_EMIT_WORKERS];
    int started = 0, ok = 1, start_error = 0;
    uint64_t profile_started = ProfileStart();
    fflush(stdout);
    fflush(stderr);
    for(; started < workers; started++) {
        int pipes[2];
        if(pipe(pipes) != 0) { start_error = errno; ok = 0; break; }
        pid_t pid = fork();
        if(pid < 0) { start_error = errno; close(pipes[0]); close(pipes[1]); ok = 0; break; }
        if(pid == 0) {
            close(pipes[0]);
            for(int w = 0; w < started; w++) close(reports[w]);
            int lowered = 1;
            for(int i = started; lowered && i < count; i += workers)
                lowered = c_lower(progs[i], root, out_dir, syms, symbol_count, 0);
            for(int k = recorded; lowered && GeneratedOutputPath(k) != NULL; k++) {
                const char *path = GeneratedOutputPath(k);
                lowered = write_all(pipes[1], path, strlen(path)) &&
                          write_all(pipes[1], "\n", 1);
            }
            fflush(stdout);
            fflush(stderr);
            /* The parent owns every exit handler and cleanup. */
            _exit(lowered && close(pipes[1]) == 0 ? 0 : 1);
        }
        close(pipes[1]);
        pids[started] = pid;
        reports[started] = pipes[0];
    }
    for(int w = 0; w < started; w++) {
        if(!read_worker_outputs(reports[w])) ok = 0;
        int status = 0;
        pid_t waited;
        do { waited = waitpid(pids[w], &status, 0); } while(waited < 0 && errno == EINTR);
        if(waited != pids[w] || !WIFEXITED(status) || WEXITSTATUS(status) != 0) ok = 0;
    }
    ProfileEnd("emit.c.workers", profile_started);
    if(start_error)
        Diagnostic((ZirSourceSpan){0}, "zir_c.global", "cannot start C emission workers: %s",
                   strerror(start_error));
    return ok && finish_simd_helpers(out_dir);
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
    int release = 0;
    int output_lock = -1;
    int check_ok;
    int result = 1;
    ProgramSet set = {0};
    CompilerContext *command_context = NULL, *previous_context = NULL;
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    const char **module_paths = CommandModulePaths(argc);
    if(module_paths == NULL) return 1;
    int module_path_count = 0;
    const char *defines[65];
    int define_count = 0;
    const char *bindings[64];
    int binding_count = 0;
    const char *host_modules[16];
    int host_module_count = 0;
    ZirProgram **progs;
    ZirCModuleSyms *syms = NULL;
    int syms_count = 0;
    int file_count;
    int symbol_count = 0;
    int i;
    int first_file = 0;
    const char *target_os_name = NULL;
    const char *target_cpu_name = NULL;

    SetDiagnosticFormatFromArguments(argc, argv);
    CommandLowerPriority();
    const CommandOption options[] = {
        COMMAND_TEXT("--root", root),
        COMMAND_LIST("--module-path", module_paths, module_path_count, argc, NULL),
        COMMAND_LIST("--define", defines, define_count, 64, valid_define),
        COMMAND_TEXT("--target-os", target_os_name),
        COMMAND_TEXT("--target-cpu", target_cpu_name),
        COMMAND_TEXT("-o", out_dir),
        COMMAND_TEXT("--entry", entry),
        COMMAND_LIST("--bind", bindings, binding_count, 64, NULL),
        COMMAND_LIST("--bind-host", host_modules, host_module_count, 16, NULL),
        COMMAND_FLAG("--prune-stale", prune_stale),
        COMMAND_FLAG("--no-main", no_main),
        COMMAND_FLAG("--exe", exe),
        COMMAND_FLAG("--release", release),
    };
    first_file = ReadCommandOptions(argc, argv, options,
                                    sizeof(options) / sizeof(*options));
    if(first_file < 0) {
        usage();
        return 1;
    }
    if(release && !exe) {
        Diagnostic((ZirSourceSpan){0}, "command.arguments", "--release needs --exe");
        return 1;
    }
    if(root == NULL || out_dir == NULL || first_file == 0 ||
       (entry != NULL && !split_entry(entry, entry_module, entry_function)) ||
       ((binding_count > 0 || host_module_count > 0) && entry == NULL) ||
       (exe && entry == NULL)) {
        if(exe && entry == NULL) {
            Diagnostic((ZirSourceSpan){0}, "command.arguments", "--exe needs --entry module:function");
            return 1;
        }
        usage();
        return 1;
    }
    ZirTargetOS target_os = CompilerHostOS();
    ZirTargetCPU target_cpu = CompilerHostCPU();
    if((target_os_name != NULL &&
        !CompilerParseTargetOS(target_os_name, &target_os)) ||
       (target_cpu_name != NULL &&
        !CompilerParseTargetCPU(target_cpu_name, &target_cpu))) {
        usage();
        return 1;
    }
    if(target_os == ZIR_OS_WEB && target_cpu_name == NULL)
        target_cpu = ZIR_CPU_WASM32;
    CompilerConfigureTarget(ZIR_TARGET_C, target_os, target_cpu);
    /* POSIX_THREADS is a true adapter capability. The selected web target
     * lacks it; PLATFORM_WEB is no longer interpreted as a target proxy. */
    int posix_threads = target_os != ZIR_OS_WEB;
    int threads_defined = 0;
    for(int d = 0; d < define_count; d++)
        threads_defined |= strcmp(defines[d], "POSIX_THREADS") == 0;
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
                                (const char *const *)(argv + first_file),
                                argc - first_file))
        return 1;
    file_count = set.count;
    progs = set.programs;
    check_ok = CheckCanonicalPrograms(progs, file_count,
                                      (const char *const *)set.paths);
    if(!check_ok)
        goto done;
    if(entry == NULL)
        for(i = 0; i < file_count; i++)
            if(!CheckTargetCapabilities(progs[i], "c"))
                goto done;
    if(entry != NULL) {
        if(!CommandMergePrograms(&set, &merged))
            goto done;
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
        if(!CheckTargetCapabilities(linked, "c"))
            goto done;
        output_lock = lock_output(out_dir);
        if(output_lock < 0) goto done;
        syms = calloc((size_t)linked->module_count, sizeof(*syms));
        if(syms == NULL)
            goto done;
        syms_count = linked->module_count;
        for(i = 0; i < linked->module_count; i++) {
            ZirProgram view = {0};
            view.context = linked->context;
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
                view.context = progs[i]->context;
                view.modules = &progs[i]->modules[m];
                view.module_count = 1;
                if(!c_build_syms(&view, &syms[position++]))
                    goto done;
            }
        /* Pass 2: lower with full cross-module resolution. */
        if(!lower_programs(progs, file_count, root, out_dir, syms, symbol_count))
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
        if(!c_compile_executable(out_dir, name != NULL ? name + 1 : entry_module, posix_threads, release)) {
            result = 1;
            goto finish;
        }
    }
finish:
    if(output_lock >= 0) close(output_lock);
    CompilerContextSelect(previous_context);
    if(command_context && !CompilerContextClose(command_context)) {
        Diagnostic((ZirSourceSpan){0}, "compiler.context", "compiler operation retained live programs");
        result = 1;
    }
    return result;
}
