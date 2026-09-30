#include "zir.h"
#include "zir_bundle.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_load.h"
#include "zir_serial.h"
#include "zir_vm.h"

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

/* The website uses the same module loader, checker, linker, and VM as zi2zib.
 * The worker supplies one source file and embedded standard modules; no host
 * capabilities are installed. */
int RunSource(void)
{
    const char *inputs[] = {"/playground.zi"};
    const char *module_paths[] = {"/std"};
    ProgramSet set = {0};
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    const char *entry = NULL;
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
                break;
            }
            if(strcmp(name, "Answer") == 0)
                entry = "Answer";
        }
    }
    if(entry == NULL) {
        Diagnostic(Span("playground.zi", 1, 1), "playground.entry",
                   "add a main :: () or Answer :: () entry procedure");
        goto done;
    }
    report_stage("build");
    linked = BundleLink(&merged, "playground", entry);
    if(linked == NULL || !VmVerify(linked, "playground", entry))
        goto done;
    report_stage("run");
    status = 2;
    /* The page runs in the reader's browser tab: a bound keeps a runaway
     * loop from hanging it. */
    if(!VmRunBounded(linked, "playground", entry, NULL, NULL, 1000000,
                     &value, &has_result))
        goto done;
    if(has_result)
        printf("%lld\n", value);
    status = 0;
done:
    fflush(NULL);
    ProgramFree(linked);
    free(merged.modules);
    ProgramsFree(&set);
    return status;
}
