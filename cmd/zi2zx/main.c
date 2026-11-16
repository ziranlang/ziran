#include "zx_exec_run.h"
#include "compiler_bundle.h"
#include "compiler_exec_link.h"
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
#include <errno.h>
#include <stdint.h>

static void
usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "usage: zi2zx [--module-path DIR] [--define NAME] [--asset-dir NAME=DIR] [--retain module:function] [--bind module:function=module:function] [--bind-host module] --root DIR --entry module:function [--target zx|wasm] [--opt] [--debug] [--checks] [--emit zx|lir|opt-report] -o FILE file.zi|file.zir ...\n"
            "       zi2zx [--emit zx|lir] -o FILE file.zx\n"
            "       zi2zx run [--backend native|engine] file.zx\n"
            "       zi2zx run [--backend native|engine] [ZX build options] [--root DIR] [--entry module:function] file.zi ...\n"
            "run uses native code on linux-x86_64, linux-aarch64 and linux-armv7 and the portable engine elsewhere; a bundle\n"
            "native code cannot run fails before it starts with a named gap, and nothing falls back to the engine\n");
}

static void
opt_pass(FILE *stream, const char *name, const LirOptPassCounters *counters)
{
    fprintf(stream, "{\"name\":\"%s\",\"runs\":%lld,\"instructions\":%lld,\"values\":%lld,\"blocks\":%lld,\"constants\":%lld}",
        name, (long long)counters->runs, (long long)counters->instructions,
        (long long)counters->values, (long long)counters->blocks,
        (long long)counters->constants);
}

static int
opt_report(FILE *stream, const LirOptResult *result)
{
    const LirOptCounters *counters = &result->counters;
    fprintf(stream, "{\"version\":1,\"work\":%lld,\"scratch_bytes\":%lld,\"input\":{\"functions\":%lld,\"blocks\":%lld,\"values\":%lld,\"instructions\":%lld},",
        (long long)result->work, (long long)result->scratch_bytes,
        (long long)counters->input_functions, (long long)counters->input_blocks,
        (long long)counters->input_values, (long long)counters->input_instructions);
    fprintf(stream, "\"output\":{\"functions\":%lld,\"blocks\":%lld,\"values\":%lld,\"instructions\":%lld},\"passes\":[",
        (long long)counters->output_functions, (long long)counters->output_blocks,
        (long long)counters->output_values, (long long)counters->output_instructions);
    opt_pass(stream, "fold", &counters->fold); fputc(',', stream);
    opt_pass(stream, "copy", &counters->copy); fputc(',', stream);
    opt_pass(stream, "cse", &counters->cse); fputc(',', stream);
    opt_pass(stream, "cfg", &counters->cfg); fputc(',', stream);
    opt_pass(stream, "dead", &counters->dead);
    fputs("]}\n", stream);
    return !ferror(stream);
}

/* `zi2zx` writes revision-28 executable ZX to its -o file.
 * `zi2zx run FILE.zi` writes it to INTO instead, with FILE's folder as the
 * default root and its main as the default entry. */
static int
build_to(int argc, char **argv, FILE *into)
{
    const char *root = NULL;
    const char *output = NULL;
    const char *entry = NULL;
    const char *emit = "zx";
    const char *compile_target = "zx";
    int optimize = 0;
    int debug = 0;
    int checks = 0;
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
    ZxLowerOwner lower = {0};
    FILE *target = NULL;
    char temporary[ZIR_PATH_MAX] = {0};
    for(int i = 0; i < argc; i++) {
        if(strcmp(argv[i], "--root") == 0 && i + 1 < argc)
            root = argv[++i];
        else if((strcmp(argv[i], "--module-path") == 0 || strcmp(argv[i], "-I") == 0) && i + 1 < argc)
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
        else if(strcmp(argv[i], "--emit") == 0 && i + 1 < argc)
            emit = argv[++i];
        else if(strcmp(argv[i], "--target") == 0 && i + 1 < argc)
            compile_target = argv[++i];
        else if(!strncmp(argv[i], "--target=", 9))
            compile_target = argv[i] + 9;
        else if(strcmp(argv[i], "--opt") == 0)
            optimize = 1;
        else if(strcmp(argv[i], "--debug") == 0)
            debug = 1;
        else if(strcmp(argv[i], "--checks") == 0)
            checks = 1;
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
       (strcmp(emit, "zx") && strcmp(emit, "lir") && strcmp(emit, "opt-report")) ||
       (strcmp(compile_target, "zx") && strcmp(compile_target, "wasm")) ||
       (!strcmp(emit, "opt-report") && !optimize) ||
       (into != NULL && (strcmp(emit, "zx") || optimize)) ||
       !split_entry(entry, entry_module, entry_function)) {
        usage();
        return 1;
    }
    if(!strcmp(compile_target, "wasm"))
        CompilerConfigureTarget(ZIR_TARGET_WASM, ZIR_OS_WEB, ZIR_CPU_WASM32);
    else
        CompilerConfigureTarget(ZIR_TARGET_ZX, ZIR_OS_NONE, ZIR_CPU_NONE);
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
    merged.checks = checks;
    if(checks) CommandEnableChecks(&set);
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
    int listing = !strcmp(emit, "lir");
    int reporting = !strcmp(emit, "opt-report");
    if(!listing && !reporting) {
        bytes = ZxExecBuild(&merged, entry_module, entry_function,
            (Slice){views, (int64_t)assets.count}, debug, &size);
        if(bytes == NULL) goto done;
    } else if(!ZxLowerBuild(&merged, entry_module, entry_function,
                           optimize, &lower))
        goto done;

    if(into != NULL)
        target = into;
    else if(!strcmp(output, "-"))
        target = stdout;
    else
        target = GeneratedOutputOpen(output, temporary, sizeof(temporary));
    if(target == NULL) {
        Diagnostic(Span(output ? output : "<memory>", 1, 1), "zx.output",
            "cannot create output");
        goto done;
    }
    int written;
    if(reporting)
        written = opt_report(target, ZxLowerOptimization(&lower));
    else if(listing)
        written = AssemblyPrintLir((LirProgram *)ZxLowerView(&lower), target);
    else
        written = fwrite(bytes, 1, size, target) == size;
    if(into == NULL && target != stdout) {
        if(fclose(target) != 0) written = 0;
        target = NULL;
        if(written && GeneratedOutputReplace(temporary, output) == 0)
            temporary[0] = 0;
        else
            written = 0;
    } else if(target == stdout && fflush(stdout) != 0)
        written = 0;
    if(!written) {
        Diagnostic(Span(output ? output : "<memory>", 1, 1), "zx.output",
            "cannot finish output");
        goto done;
    }
    result = 0;
done:
    if(target != NULL && target != into && target != stdout) fclose(target);
    if(temporary[0]) remove(temporary);
    ZxLowerFree(&lower);
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

static int
is_zx(const char *path)
{
    size_t length = strlen(path);
    return length > 3 && !strcmp(path + length - 3, ".zx");
}

static unsigned char *
read_zx(const char *path, size_t *size)
{
    FILE *stream = fopen(path, "rb");
    unsigned char *bytes = NULL;
    if(stream == NULL || fseek(stream, 0, SEEK_END) != 0) goto failure;
    long length = ftell(stream);
    if(length < 0 || length > 268435456 || fseek(stream, 0, SEEK_SET) != 0)
        goto failure;
    bytes = malloc((size_t)length + 1);
    if(bytes == NULL) { DiagnosticOutOfMemory(); goto failure; }
    if(fread(bytes, 1, (size_t)length, stream) != (size_t)length ||
       fgetc(stream) != EOF || ferror(stream) || fclose(stream) != 0) {
        stream = NULL;
        goto failure;
    }
    *size = (size_t)length;
    return bytes;
failure:
    if(stream != NULL) fclose(stream);
    free(bytes);
    Diagnostic((ZirSourceSpan){0}, "zx.input",
        "cannot read bounded verified ZX input: %s", path);
    return NULL;
}

static int
inspect_zx(int argc, char **argv)
{
    const char *emit = "zx";
    const char *output = NULL;
    const char *input = NULL;
    for(int index = 0; index < argc; index++) {
        if(!strcmp(argv[index], "--emit") && index + 1 < argc)
            emit = argv[++index];
        else if(!strcmp(argv[index], "-o") && index + 1 < argc)
            output = argv[++index];
        else if(!strncmp(argv[index], "--diagnostics=", 14)) {
            if(!SetDiagnosticFormat(argv[index] + 14)) return 1;
        } else if(argv[index][0] == '-' || input != NULL)
            return usage(), 1;
        else
            input = argv[index];
    }
    if(input == NULL || output == NULL || !is_zx(input) ||
       (strcmp(emit, "zx") && strcmp(emit, "lir")))
        return usage(), 1;

    size_t size = 0;
    unsigned char *bytes = read_zx(input, &size);
    if(bytes == NULL) return 1;
    ZxExecOwner owner = {0};
    ZxExecResult read = compiler_exec_link_ZxExecRead(&owner,
        (String){(char *)bytes, size}, compiler_lowered_ir_LirDefaultLimits());
    if(read.problem != ZxExecProblem_None) {
        String problem = compiler_exec_link_ZxExecProblemName(read.problem);
        Diagnostic((ZirSourceSpan){0}, "zx.container",
            "executable ZX rejected: problem=%.*s codec=%d offset=%lld verifier=%d",
            (int)problem.length, problem.data, read.codec,
            (long long)read.offset, read.diagnostic.problem);
        free(bytes);
        return 1;
    }

    char temporary[ZIR_PATH_MAX] = {0};
    FILE *target = !strcmp(output, "-") ? stdout :
        GeneratedOutputOpen(output, temporary, sizeof(temporary));
    int status = 1;
    if(target == NULL) {
        Diagnostic((ZirSourceSpan){0}, "zx.output",
            "cannot create output: %s", output);
        goto done;
    }
    LirProgram program = compiler_exec_link_ZxExecReadProgram(&owner);
    int written = !strcmp(emit, "lir")
        ? AssemblyPrintLir((LirProgram *)&program, target)
        : fwrite(bytes, 1, size, target) == size;
    if(target == stdout) {
        if(fflush(stdout) != 0) written = 0;
    } else {
        if(fclose(target) != 0) written = 0;
        target = NULL;
        if(written && GeneratedOutputReplace(temporary, output) == 0)
            temporary[0] = 0;
        else
            written = 0;
    }
    if(!written)
        Diagnostic((ZirSourceSpan){0}, "zx.output",
            "cannot finish output: %s", output);
    else
        status = 0;
done:
    if(target != NULL && target != stdout) fclose(target);
    if(temporary[0]) remove(temporary);
    compiler_exec_link_ZxExecReadFree(&owner);
    free(bytes);
    return status;
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
    if(build_to(argc, argv, memory) == 0 && fflush(memory) == 0) {
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
run_limit(const char *text, int64_t *value)
{
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(text, &end, 10);
    if(errno || end == text || *end || parsed > INT64_MAX) return 0;
    *value = (int64_t)parsed;
    return 1;
}

static int
run_debug_image(const char *path, LirExecLimits limits, int heap_report)
{
    size_t size = 0;
    unsigned char *bytes = read_zx(path, &size);
    if(bytes == NULL) return 1;
    ZxExecOwner owner = {0};
    ZxExecResult read = compiler_exec_link_ZxExecRead(&owner,
        (String){(char *)bytes, size}, compiler_lowered_ir_LirDefaultLimits());
    int status = 1;
    if(read.problem != ZxExecProblem_None) {
        String problem = compiler_exec_link_ZxExecProblemName(read.problem);
        Diagnostic((ZirSourceSpan){0}, "zx.container",
            "executable ZX rejected: problem=%.*s codec=%d offset=%lld verifier=%d",
            (int)problem.length, problem.data, read.codec,
            (long long)read.offset, read.diagnostic.problem);
    } else {
        LirProgram program = compiler_exec_link_ZxExecReadProgram(&owner);
        status = ZxExecRunProgram(&program, limits, heap_report);
    }
    compiler_exec_link_ZxExecReadFree(&owner);
    free(bytes);
    return status;
}

static int
run_command(int argc, char **argv)
{
    int dispatch_host = 0;
    int debug_engine = 0;
    int heap_report = 0;
    /* The platform decides the default, never the bundle: native code on
     * linux-x86_64, linux-aarch64 and linux-armv7, where a native gap is a
     * named error. */
#if (defined(__x86_64__) || defined(__aarch64__) || defined(__arm__)) && defined(__linux__)
    BundleBackendPolicy backend = BUNDLE_BACKEND_NATIVE;
#else
    BundleBackendPolicy backend = BUNDLE_BACKEND_AUTO;
#endif
    LirExecLimits limits = compiler_exec_LirExecDefaultLimits();
    for(; argc > 0 && !strncmp(argv[0], "--", 2); argc--, argv++) {
        if(!strcmp(argv[0], "--host-dispatch"))
            dispatch_host = 1;
        else if(!strcmp(argv[0], "--backend") && argc > 1) {
            if(!strcmp(argv[1], "native")) backend = BUNDLE_BACKEND_NATIVE;
            else if(!strcmp(argv[1], "engine")) backend = BUNDLE_BACKEND_ENGINE;
            else { usage(); return 1; }
            argc--;
            argv++;
        }
        else if(!strcmp(argv[0], "--heap-report")) {
            heap_report = 1;
            debug_engine = 1;
        } else if((!strcmp(argv[0], "--work") || !strcmp(argv[0], "--fuel") ||
                   !strcmp(argv[0], "--frames") || !strcmp(argv[0], "--addresses")) && argc > 1) {
            int64_t value = 0;
            if(!run_limit(argv[1], &value)) { usage(); return 1; }
        if(!strcmp(argv[0], "--work")) limits.work = value;
        else if(!strcmp(argv[0], "--fuel")) limits.fuel = value;
        else if(!strcmp(argv[0], "--addresses")) limits.addresses = value;
        else limits.frames = value;
            debug_engine = 1;
            argc--;
            argv++;
        }
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
    if(debug_engine) {
        if(source || dispatch_host) { usage(); return 1; }
        return run_debug_image(argv[0], limits, heap_report);
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
    /* Native preflight finishes before any initializer runs. */
    if(backend == BUNDLE_BACKEND_NATIVE) {
        const char *problem = BundleNativeProblem(bundle);
        if(problem[0] != 0) {
            Diagnostic((ZirSourceSpan){0}, "zx.native", "%s", problem);
            BundleClose(bundle);
            return 1;
        }
    }
    BundleInstanceOptions options = {backend, 0};
    BundleInstance *instance = BundleInstantiateWithOptions(bundle, NULL, 0, &options);
    int ok = instance != NULL && BundleInstanceRun(instance, &result, &has_result);
    BundleInstanceClose(instance);
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
    if(argc > 1 && strcmp(argv[1], "run") == 0)
        return run_command(argc - 2, argv + 2);
    if(argc <= 1 || !strcmp(argv[1], "bundle")) return usage(), 1;
    CommandLowerPriority();
    // `zi2zx run` returns the program's own status; the other commands fail
    // only with a diagnostic.
    if(is_zx(argv[argc - 1]))
        return DiagnosticEnsureFailureReported(inspect_zx(argc - 1, argv + 1), "zi2zx");
    return DiagnosticEnsureFailureReported(build_to(argc - 1, argv + 1, NULL), "zi2zx");
}
