#include "zir.h"
#include "zir_bundle.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_serial.h"
#include "zir_load.h"
#include "zir_vm.h"
#include "ziran_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#endif

static void
usage(void)
{
    fprintf(stderr,
            "usage: zi2zib bundle [--module-path DIR] [--bind module:function=module:function] --root DIR --entry module:function -o FILE file.zi|file.zir ...\n"
            "       zi2zib run file.zib\n");
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

static int
bundle_command(int argc, char **argv)
{
    const char *root = NULL;
    const char *output = NULL;
    const char *entry = NULL;
    int first_file = 0;
    int count, result = 1;
    char entry_module[ZIR_NAME_MAX], entry_function[ZIR_NAME_MAX];
    ProgramSet set = {0};
    const char *module_paths[64];
    int module_path_count = 0;
    const char *bindings[64];
    int binding_count = 0;
    ZirProgram **programs = NULL;
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    FILE *file = NULL;
    for(int i = 0; i < argc; i++) {
        if(strcmp(argv[i], "--root") == 0 && i + 1 < argc)
            root = argv[++i];
        else if(strcmp(argv[i], "--module-path") == 0 && i + 1 < argc && module_path_count < 64)
            module_paths[module_path_count++] = argv[++i];
        else if(strcmp(argv[i], "--bind") == 0 && i + 1 < argc && binding_count < 64)
            bindings[binding_count++] = argv[++i];
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
    if(root == NULL || output == NULL || first_file == 0 ||
       !split_entry(entry, entry_module, entry_function)) {
        usage();
        return 1;
    }
    if(!ProgramsLoad(&set, root, module_paths, module_path_count,
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
    for(int binding = 0; binding < binding_count; binding++)
        if(!BindHostProvider(&merged, bindings[binding]))
            goto done;
    linked = BundleLink(&merged, entry_module, entry_function);
    if(linked == NULL || !VmVerify(linked, entry_module, entry_function))
        goto done;
    file = fopen(output, "wb");
    if(file == NULL) {
        Diagnostic(Span(output, 1, 1), "zib.output",
                      "cannot open bundle output");
        goto done;
    }
    if(!BundleWrite(file, linked, entry_module, entry_function)) {
        Diagnostic(Span(output, 1, 1), "zib.output",
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

/* The portable runner nests about 4 KB of C stack for each Ziran call,
 * and recursion may go as deep as the stack allows, so the program runs
 * on a thread with room for some 60,000 nested calls. Pages are only used
 * as calls reach them. */
enum { RUN_STACK_BYTES = 256 * 1024 * 1024 };

static int
run_command(int argc, char **argv)
{
    if(argc != 1) {
        usage();
        return 1;
    }
    RunJob job = {0};
    job.bundle = BundleOpen(argv[0]);
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
    if(job.ok && job.has_result)
        printf("%lld\n", job.result);
    BundleClose(job.bundle);
    return job.ok ? 0 : 1;
}

int
main(int argc, char **argv)
{
    if(argc > 1 && strcmp(argv[1], "bundle") == 0)
        return bundle_command(argc - 2, argv + 2);
    if(argc > 1 && strcmp(argv[1], "run") == 0)
        return run_command(argc - 2, argv + 2);
    usage();
    return 1;
}
