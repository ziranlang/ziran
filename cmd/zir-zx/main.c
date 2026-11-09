#include "zir.h"
#include "zir_bundle.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_command.h"
#include "zir_serial.h"
#include "zir_load.h"
#include "zir_vm.h"
#include "ziran_host.h"

#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#endif
#ifdef ZIRAN_NATIVE_ZX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static void
usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "usage: zi2zx bundle [--module-path DIR] [--define NAME] [--asset-dir NAME=DIR] [--library FILE.zx|--include-library FILE.zx] [--retain module:function] [--bind module:function=module:function] [--bind-host module] --root DIR --entry module:function -o FILE file.zi|file.zir ...\n"
            "       zi2zx run [--vm] file.zx\n"
            "       zi2zx run [--vm] [bundle options] [--root DIR] [--entry module:function] file.zi ...\n"
#ifdef ZIRAN_NATIVE_ZX
            "       zi2zx native-c [--embedded] -o DIR file.zx ...\n"
#endif
            );
}

static int
include_library_assets(ZxAssets *assets, const Bundle *library)
{
    for(size_t i = 0; i < BundleAssetCount(library); i++)
        if(!ZxAssetsAdd(assets, BundleAssetName(library, i), BundleAssetData(library, i),
                        BundleAssetSize(library, i)))
            return 0;
    return 1;
}

/* `zi2zx bundle` writes the bundle to its -o file. `zi2zx run FILE.zi`
 * writes it to INTO instead, with FILE's folder as the default root and
 * its main as the default entry, and *LOCAL reports whether all of its code
 * came from source, without libraries from other bundles. */
static int
bundle_to(int argc, char **argv, FILE *into, int *local)
{
    const char *root = NULL;
    const char *output = NULL;
    const char *entry = NULL;
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
    ZirProgram *linked = NULL;
    ZxAssets assets = {0};
    FILE *file = NULL;
    const char *library_paths[64];
    int library_count = 0;
    int include_libraries = 0;
    Bundle *libraries[64] = {0};
    ZirProgram *thin = NULL, *context = NULL;
    const ZirProgram *library_programs[64];
    for(int i = 0; i < argc; i++) {
        if(strcmp(argv[i], "--root") == 0 && i + 1 < argc)
            root = argv[++i];
        else if(strcmp(argv[i], "--module-path") == 0 && i + 1 < argc)
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
        else if((strcmp(argv[i], "--library") == 0 ||
                 strcmp(argv[i], "--include-library") == 0) &&
                i + 1 < argc && library_count < 64) {
            int embedded = strcmp(argv[i], "--include-library") == 0;
            if(library_count != 0 && embedded != include_libraries) {
                Diagnostic(Span("<command>", 1, 1), "zx.library",
                           "do not mix external and included libraries");
                goto done;
            }
            include_libraries = embedded;
            library_paths[library_count++] = argv[++i];
        }
        else if(strcmp(argv[i], "--entry") == 0 && i + 1 < argc)
            entry = argv[++i];
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
       !split_entry(entry, entry_module, entry_function)) {
        usage();
        return 1;
    }
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
    linked = BundleLink(&merged, entry_module, entry_function);
    if(linked == NULL || !CheckTargetCapabilities(linked, "zx") ||
       !VmVerify(linked, entry_module, entry_function))
        goto done;
    const ZirProgram *output_program = linked;
    const ZirProgram *checking_program = linked;
    if(library_count != 0) {
        int total = linked->module_count;
        for(int i = 0; i < library_count; i++) {
            libraries[i] = BundleOpen(library_paths[i]);
            const ZirProgram *library = BundleProgram(libraries[i]);
            if(library == NULL || library->module_count > INT_MAX - total)
                goto done;
            total += library->module_count;
            if(include_libraries && !include_library_assets(&assets, libraries[i])) {
                Diagnostic(Span("<command>", 1, 1), "zx.library",
                           "conflicting or invalid library assets");
                goto done;
            }
        }
        for(int i = 0; i < library_count; i++)
            library_programs[i] = BundleProgram(libraries[i]);
        thin = BundleLibraryDelta(linked, library_programs, (size_t)library_count);
        context = thin ? BundleComposeLibraries(thin, library_programs, (size_t)library_count) : NULL;
        if(context == NULL) {
            Diagnostic(Span("<command>", 1, 1), "zx.library",
                       "conflicting library declarations or providers");
            goto done;
        }
        if(!CheckCanonicalPrograms(&context, 1, NULL) ||
           !CheckTargetCapabilities(context, "zx") ||
           !VmVerify(context, entry_module, entry_function))
            goto done;
        /* Included libraries are a standalone payload: all declarations and
         * their requirements now belong to that output. */
        if(include_libraries)
            for(int m = 0; m < context->module_count; m++)
                context->modules[m].bundle_library = NULL;
        checking_program = context;
        output_program = include_libraries ? context : thin;
    }
    if(into != NULL) {
        if(!BundleWriteInContext(into, output_program, checking_program,
                                 entry_module, entry_function, &assets)) {
            Diagnostic(Span("<command>", 1, 1), "zx.output", "cannot write bundle");
            goto done;
        }
        *local = library_count == 0;
        result = 0;
        goto done;
    }
    file = fopen(output, "wb");
    if(file == NULL) {
        Diagnostic(Span(output, 1, 1), "zx.output",
                      "cannot open bundle output");
        goto done;
    }
    if(!BundleWriteInContext(file, output_program, checking_program,
                             entry_module, entry_function, &assets)) {
        Diagnostic(Span(output, 1, 1), "zx.output",
                      "cannot write bundle");
        goto done;
    }
    result = 0;
done:
    if(file != NULL && fclose(file) != 0)
        result = 1;
    if(result != 0 && file != NULL)
        remove(output);
    ProgramFree(linked);
    ZxAssetsFree(&assets);
    ProgramFree(thin);
    ProgramFree(context);
    SymbolLookupsChanged();
    for(int i = 0; i < library_count; i++)
        BundleClose(libraries[i]);
    free(merged.modules);
    ProgramsFree(&set);
    return result;
}

typedef struct RunJob {
    Bundle *bundle;
    long long result;
    int has_result;
    int ok;
} RunJob;

static void *
run_job(void *argument)
{
    RunJob *job = argument;
    job->ok = BundleRun(job->bundle, NULL, 0, &job->result, &job->has_result);
    return NULL;
}

static int
is_source(const char *path)
{
    size_t length = strlen(path);
    return (length > 3 && strcmp(path + length - 3, ".zi") == 0) ||
           (length > 4 && strcmp(path + length - 4, ".zir") == 0);
}

/* Source runs as a bundle built here in memory. It is the only kind whose
 * #asm variants may run natively; a bundle file never is. */
static Bundle *
open_source(int argc, char **argv)
{
    FILE *memory = tmpfile();
    int local = 0;
    Bundle *bundle = NULL;
    if(memory == NULL) {
        Diagnostic(Span("<command>", 1, 1), "zx.output", "cannot create a temporary bundle");
        return NULL;
    }
    if(bundle_to(argc, argv, memory, &local) == 0 && fflush(memory) == 0) {
        long size = ftell(memory);
        unsigned char *data = size > 0 ? malloc((size_t)size) : NULL;
        rewind(memory);
        if(data != NULL && fread(data, 1, (size_t)size, memory) == (size_t)size)
            bundle = BundleOpenBytes(data, (size_t)size);
        free(data);
    }
    fclose(memory);
    if(bundle != NULL && local)
        BundleTrustNativeAssembly(bundle);
    return bundle;
}

/* The portable runner nests about 4 KB of C stack for each Ziran call,
 * and recursion may go as deep as the stack allows, so the program runs
 * on a thread with room for some 60,000 nested calls. Pages are only used
 * as calls reach them. */
enum { RUN_STACK_BYTES = 256 * 1024 * 1024 };

static int
run_command(int argc, char **argv)
{
    for(; argc > 0 && !strncmp(argv[0], "--", 2); argc--, argv++) {
        if(!strcmp(argv[0], "--vm"))
            BundleUsePortableVm(1);
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
    RunJob job = {0};
    job.bundle = source ? open_source(argc, argv) : BundleOpen(argv[0]);
    if(job.bundle == NULL)
        return 1;
    int ran_on_thread = 0;
#if defined(__unix__) || defined(__APPLE__)
    pthread_attr_t attributes;
    pthread_t thread;
    if(pthread_attr_init(&attributes) == 0) {
        if(pthread_attr_setstacksize(&attributes, RUN_STACK_BYTES) == 0 &&
           pthread_create(&thread, &attributes, run_job, &job) == 0) {
            pthread_join(thread, NULL);
            ran_on_thread = 1;
        }
        pthread_attr_destroy(&attributes);
    }
#endif
    if(!ran_on_thread)
        run_job(&job);
    if(job.ok && job.has_result) {
        if(BundleEntry(job.bundle)->return_type[0] == 'u')
            printf("%llu\n", (unsigned long long)job.result);
        else
            printf("%lld\n", job.result);
    }
    BundleClose(job.bundle);
    return job.ok ? 0 : 1;
}

#ifdef ZIRAN_NATIVE_ZX
/* Programs written so far, so a bundle embedded twice is written once. */
typedef struct LinkedSet {
    const char *output;
    unsigned char (*identities)[32];
    int count;
    int embedded;
    FILE *list;
} LinkedSet;

static int write_linked(LinkedSet *set, Bundle *bundle, const char *name);

/* Bundles embedded in another, such as a collection's games, run as their
 * own instances. Those that open without libraries are written too; the
 * others run on the portable VM, as does any bundle without linked code. */
static int
write_embedded(LinkedSet *set, const Bundle *bundle, const char *name)
{
    for(size_t i = 0; i < BundleAssetCount(bundle); i++) {
        const char *asset = BundleAssetName(bundle, i);
        size_t length = strlen(asset);
        if(length < 4 || strcmp(asset + length - 3, ".zx") != 0)
            continue;
        /* A bundle that needs libraries fails to open alone; that is not an
         * error here, so its diagnostics are not shown. */
        fflush(stderr);
        int saved = dup(2), quiet = open("/dev/null", O_WRONLY);
        if(saved >= 0 && quiet >= 0)
            dup2(quiet, 2);
        Bundle *inner = BundleOpenBytes(BundleAssetData(bundle, i), BundleAssetSize(bundle, i));
        fflush(stderr);
        if(saved >= 0 && quiet >= 0)
            dup2(saved, 2);
        if(saved >= 0)
            close(saved);
        if(quiet >= 0)
            close(quiet);
        char inner_name[ZIR_PATH_MAX];
        snprintf(inner_name, sizeof(inner_name), "%s:%s", name, asset);
        if(inner == NULL) {
            printf("%s: needs libraries, runs on the portable VM\n", inner_name);
            continue;
        }
        int ok = write_linked(set, inner, inner_name);
        BundleClose(inner);
        if(!ok)
            return 0;
    }
    return 1;
}

static int
write_linked(LinkedSet *set, Bundle *bundle, const char *name)
{
    unsigned char identity[32];
    BundleIdentity(bundle, identity);
    for(int i = 0; i < set->count; i++)
        if(memcmp(set->identities[i], identity, sizeof(identity)) == 0)
            return 1;
    char directory[ZIR_PATH_MAX];
    snprintf(directory, sizeof(directory), "%s/%d", set->output, set->count);
    if(mkdir(directory, 0700) != 0) {
        Diagnostic(Span(directory, 1, 1), "zx.output", "cannot create the output directory");
        return 0;
    }
    if(!BundleWriteLinked(bundle, directory, identity))
        return 0;
    unsigned char (*grown)[32] = realloc(set->identities, (size_t)(set->count + 1) * sizeof(*grown));
    if(grown == NULL)
        return 0;
    set->identities = grown;
    memcpy(set->identities[set->count], identity, sizeof(identity));
    fprintf(set->list, "%d ", set->count);
    for(int i = 0; i < 32; i++)
        fprintf(set->list, "%02x", identity[i]);
    fprintf(set->list, " %s\n", name);
    printf("%s: linked code in %s\n", name, directory);
    set->count++;
    return !set->embedded || write_embedded(set, bundle, name);
}

/* zi2zx native-c: each bundle's program as C for a host compiled ahead of
 * time, one directory per program under -o, listed in programs.txt. */
static int
native_c_command(int argc, char **argv)
{
    LinkedSet set = {0};
    int first = -1;
    for(int i = 0; i < argc; i++) {
        if(strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            set.output = argv[++i];
        } else if(strcmp(argv[i], "--embedded") == 0) {
            set.embedded = 1;
        } else if(argv[i][0] == '-') {
            usage();
            return 1;
        } else if(first < 0) {
            first = i;
        }
    }
    if(set.output == NULL || first < 0) {
        usage();
        return 1;
    }
    mkdir(set.output, 0700);
    char list[ZIR_PATH_MAX];
    snprintf(list, sizeof(list), "%s/programs.txt", set.output);
    set.list = fopen(list, "wb");
    if(set.list == NULL) {
        Diagnostic(Span(list, 1, 1), "zx.output", "cannot write the program list");
        return 1;
    }
    int ok = 1;
    for(int i = first; ok && i < argc; i++) {
        if(strcmp(argv[i], "-o") == 0) {
            i++;
            continue;
        }
        if(argv[i][0] == '-')
            continue;
        Bundle *bundle = BundleOpen(argv[i]);
        ok = bundle != NULL && write_linked(&set, bundle, argv[i]);
        BundleClose(bundle);
    }
    if(fclose(set.list) != 0)
        ok = 0;
    free(set.identities);
    return ok ? 0 : 1;
}
#endif

int
main(int argc, char **argv)
{
    SetDiagnosticFormatFromArguments(argc, argv);
    if(argc > 1 && strcmp(argv[1], "bundle") == 0) {
        CommandLowerPriority();
        return bundle_to(argc - 2, argv + 2, NULL, NULL);
    }
    if(argc > 1 && strcmp(argv[1], "run") == 0)
        return run_command(argc - 2, argv + 2);
#ifdef ZIRAN_NATIVE_ZX
    if(argc > 1 && strcmp(argv[1], "native-c") == 0) {
        CommandLowerPriority();
        return native_c_command(argc - 2, argv + 2);
    }
#endif
    usage();
    return 1;
}
