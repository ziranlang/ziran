/* Browser clients supply ordinary source files and module search roots in
 * Emscripten's filesystem. This is the zi2zib pipeline, without UI policy. */
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

static int
paths(char *text, const char **items, int capacity)
{
    int count = 0;
    char *save = NULL;
    for(char *line = strtok_r(text, "\n", &save); line != NULL;
        line = strtok_r(NULL, "\n", &save)) {
        if(count == capacity) return -1;
        items[count++] = line;
    }
    return count;
}

/* Search roots and inputs are newline-separated absolute paths. Named roots
 * such as "package=/packages/package" use the normal module loader. */
int
BuildBundle(const char *root, const char *search, const char *inputs,
            const char *entry_module, const char *entry_function,
            const char *output)
{
    ProgramSet set = {0};
    ZirProgram merged = {0}, *linked = NULL;
    const char *module_paths[64], *input_paths[64];
    char *search_copy = NULL, *input_copy = NULL;
    FILE *file = NULL;
    int status = 1;
    if(root == NULL || search == NULL || inputs == NULL || output == NULL ||
       entry_module == NULL || entry_function == NULL) return 1;
    remove(output);
    search_copy = strdup(search);
    input_copy = strdup(inputs);
    if(search_copy == NULL || input_copy == NULL) goto done;
    int module_count = paths(search_copy, module_paths, 64);
    int input_count = paths(input_copy, input_paths, 64);
    if(module_count < 0 || input_count <= 0) goto done;
    SetDiagnosticFormat("text");
    if(!ProgramsLoad(&set, root, module_paths, module_count,
                     input_paths, input_count) ||
       !CheckCanonicalPrograms(set.programs, set.count,
                               (const char *const *)set.paths)) goto done;
    for(int p = 0; p < set.count; p++)
        merged.module_count += set.programs[p]->module_count;
    merged.modules = calloc((size_t)merged.module_count, sizeof(*merged.modules));
    if(merged.modules == NULL) goto done;
    int at = 0;
    for(int p = 0; p < set.count; p++)
        for(int m = 0; m < set.programs[p]->module_count; m++)
            merged.modules[at++] = set.programs[p]->modules[m];
    ZirProgram *program = &merged;
    if(!LinkImports(&program, 1)) goto done;
    linked = BundleLink(&merged, entry_module, entry_function);
    if(linked == NULL || !VmVerify(linked, entry_module, entry_function)) goto done;
    file = fopen(output, "wb");
    if(file == NULL || !BundleWrite(file, linked, entry_module, entry_function)) goto done;
    status = 0;
done:
    if(file != NULL && fclose(file) != 0) status = 1;
    if(status != 0) remove(output);
    ProgramFree(linked);
    free(merged.modules);
    ProgramsFree(&set);
    free(search_copy);
    free(input_copy);
    fflush(NULL);
    return status;
}
