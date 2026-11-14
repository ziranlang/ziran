#include "zir.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_load.h"
#include "zir_serial.h"
#include "zx_exec_run.h"
#include "ziran_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, report_stage, (const char *stage), {
    postMessage({type: 'stage', stage: UTF8ToString(Number(stage))});
});
#else
static void report_stage(const char *stage) { (void)stage; }
#endif

/* The website uses the same module loader, checker, lowering and engine as
 * zi2zx: the source becomes revision-28 ZX, which the embedding API runs.
 * The worker supplies one source file and embedded standard modules; no host
 * capabilities are installed. */
int RunSource(void)
{
    const char *inputs[] = {"/playground.zi"};
    const char *module_paths[] = {"/std"};
    ProgramSet set = {0};
    ZirProgram merged = {0};
    unsigned char *bytes = NULL;
    size_t size = 0;
    Bundle *bundle = NULL;
    BundleInstance *instance = NULL;
    const char *entry = NULL;
    int unsigned_result = 0;
    long long value;
    int has_result, status = 1;

    SetDiagnosticFormat("text");
    report_stage("check");
    if(!ProgramsLoad(&set, "/", module_paths, 1, inputs, 1) ||
       !CheckCanonicalPrograms(set.programs, set.count,
                               (const char *const *)set.paths))
        goto done;
    for(int p = 0; p < set.count; p++)
        merged.module_count += set.programs[p]->module_count;
    merged.context = set.context;
    merged.modules = calloc((size_t)merged.module_count, sizeof(*merged.modules));
    if(merged.modules == NULL)
        goto done;
    int at = 0;
    for(int p = 0; p < set.count; p++)
        for(int m = 0; m < set.programs[p]->module_count; m++)
            merged.modules[at++] = set.programs[p]->modules[m];
    ZirProgram *program = &merged;
    if(!LinkImports(&program, 1))
        goto done;
    for(int m = 0; m < merged.module_count; m++) {
        ZirModule *module = &merged.modules[m];
        if(strcmp(module->name, "playground") != 0)
            continue;
        for(int f = 0; f < module->function_count; f++) {
            const char *name = module->functions[f].name;
            if(strcmp(name, "main") == 0) {
                entry = "main";
                unsigned_result = module->functions[f].return_type[0] == 'u';
                break;
            }
            if(strcmp(name, "Answer") == 0) {
                entry = "Answer";
                unsigned_result = module->functions[f].return_type[0] == 'u';
            }
        }
    }
    if(entry == NULL) {
        Diagnostic(Span("playground.zi", 1, 1), "playground.entry",
                   "add a main :: () or Answer :: () entry procedure");
        goto done;
    }
    report_stage("build");
    bytes = ZxExecBuild(&merged, "playground", entry, (Slice){0}, &size);
    if(bytes == NULL || (bundle = BundleOpenBytes(bytes, size)) == NULL)
        goto done;
    report_stage("run");
    status = 2;
    /* The page runs in the reader's browser tab: a bound keeps a runaway
     * loop from hanging it. */
    instance = BundleInstantiateLimited(bundle, NULL, 0, 1000000);
    if(instance == NULL || !BundleInstanceRun(instance, &value, &has_result))
        goto done;
    if(has_result) {
        if(unsigned_result)
            printf("%llu\n", (unsigned long long)value);
        else
            printf("%lld\n", value);
    }
    status = 0;
done:
    fflush(NULL);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    free(bytes);
    free(merged.modules);
    ProgramsFree(&set);
    return status;
}
