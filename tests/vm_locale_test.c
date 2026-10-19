#include "zir_bundle.h"
#include "zir_vm.h"

#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

static int
host_call(void *context, const char *module, const char *function,
          const VmHostValue *args, int count, VmHostValue *result)
{
    (void)context; (void)module; (void)function; (void)args; (void)count; (void)result;
    return 0;
}

/* Loads and runs the bundle after switching to a decimal-comma locale, as a
 * GTK host does, so verification, literals and float text all see it. */
int
main(int argc, char **argv)
{
    static const char *locales[] = {"de_DE.UTF-8", "de_DE.utf8", "es_PY.UTF-8", "es_PY.utf8",
                                    "es_ES.UTF-8", "es_ES.utf8", "fr_FR.UTF-8", "fr_FR.utf8",
                                    "pt_BR.UTF-8", "pt_BR.utf8", NULL};
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZibLawTable laws = {0};
    long long result = -1;
    int has_result = 0;
    const char *selected = NULL;
    assert(argc == 2);
    for(int i = 0; locales[i] != NULL && selected == NULL; i++)
        if(setlocale(LC_ALL, locales[i]) != NULL &&
           strcmp(localeconv()->decimal_point, ",") == 0)
            selected = locales[i];
    if(selected == NULL) {
        puts("vm_locale: no decimal-comma locale is installed; skipped");
        return 0;
    }
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL);
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                     entry, sizeof(entry), &laws, NULL);
    fclose(input);
    assert(program != NULL);
    VmInstance *instance = VmInstanceOpen(program, module, entry, host_call, NULL);
    assert(instance != NULL);
    assert(VmInstanceRun(instance, &result, &has_result));
    if(!has_result || result != 0) {
        fprintf(stderr, "vm_locale: %s: check %lld failed\n", selected, result);
        return 1;
    }
    VmInstanceClose(instance);
    ZibLawTableFree(&laws);
    ProgramFree(program);
    return 0;
}
