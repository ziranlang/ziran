/* Browser clients supply ordinary source files and module search roots in
 * Emscripten's filesystem. This is the zi2zx pipeline, without UI policy. */
#include "zir.h"
#include "zir_bundle.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_load.h"
#include "zir_serial.h"
#include "zir_vm.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
paths(char *text, const char ***items)
{
    size_t count = 0;
    for(const char *at = text; *at != '\0'; at++)
        if(*at != '\n' && (at == text || at[-1] == '\n')) count++;
    if(count > INT_MAX || count > SIZE_MAX / sizeof(**items)) return -1;
    if(count == 0) return 0;
    *items = malloc(count * sizeof(**items));
    if(*items == NULL) return -1;
    int index = 0;
    char *save = NULL;
    for(char *line = strtok_r(text, "\n", &save); line != NULL;
        line = strtok_r(NULL, "\n", &save))
        (*items)[index++] = line;
    return index;
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
    const char **module_paths = NULL, **input_paths = NULL;
    char *search_copy = NULL, *input_copy = NULL;
    FILE *file = NULL;
    int status = 1;
    if(root == NULL || search == NULL || inputs == NULL || output == NULL ||
       entry_module == NULL || entry_function == NULL) return 1;
    remove(output);
    search_copy = strdup(search);
    input_copy = strdup(inputs);
    if(search_copy == NULL || input_copy == NULL) goto done;
    int module_count = paths(search_copy, &module_paths);
    int input_count = paths(input_copy, &input_paths);
    if(module_count < 0 || input_count <= 0) goto done;
    SetDiagnosticFormat("text");
    if(!ProgramsLoad(&set, root, module_paths, module_count,
                     input_paths, input_count) ||
       !CheckCanonicalPrograms(set.programs, set.count,
                               (const char *const *)set.paths)) goto done;
    for(int p = 0; p < set.count; p++)
        merged.module_count += set.programs[p]->module_count;
    merged.context = set.context;
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
    if(file == NULL || !BundleWrite(file, linked, entry_module, entry_function, NULL)) goto done;
    status = 0;
done:
    if(file != NULL && fclose(file) != 0) status = 1;
    if(status != 0) remove(output);
    ProgramFree(linked);
    free(merged.modules);
    ProgramsFree(&set);
    free(module_paths);
    free(input_paths);
    free(search_copy);
    free(input_copy);
    fflush(NULL);
    return status;
}
