#include "ziran_host.h"
#include "zir_bundle.h"
#include "zir_serial.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int host(void *context, const char *module, const char *function,
                const VmHostValue *args, int count, VmHostValue *result)
{
    assert(strcmp(module, "model") == 0 && count == 0);
    (*(int *)context)++;
    result->kind = VM_HOST_INTEGER;
    result->integer = 0;
    return 1;
}

static const ZirModule *module(const ZirProgram *program, const char *name)
{
    for(int m = 0; m < program->module_count; m++)
        if(!strcmp(program->modules[m].name, name)) return &program->modules[m];
    return NULL;
}

static const ZirFunction *function(const ZirModule *owner, const char *name)
{
    for(int f = 0; f < owner->function_count; f++)
        if(!strcmp(owner->functions[f].name, name)) return &owner->functions[f];
    return NULL;
}

static unsigned char *bytes(const ZirProgram *program, const ZirProgram *context,
                             size_t *size)
{
    FILE *out = tmpfile();
    assert(out && BundleWriteInContext(out, program, context, "addon", "Main", NULL));
    long length = ftell(out);
    assert(length > 0);
    *size = (size_t)length;
    unsigned char *data = malloc(*size);
    rewind(out);
    assert(data && fread(data, 1, *size, out) == *size);
    fclose(out);
    return data;
}

static size_t skip_name(const unsigned char *data, size_t offset)
{
    size_t length = data[offset] | (size_t)data[offset+1] << 8 |
                    (size_t)data[offset+2] << 16 | (size_t)data[offset+3] << 24;
    return offset + 4 + length;
}

int main(int argc, char **argv)
{
    assert(argc == 5);
    Bundle *base = BundleOpen(argv[1]);
    assert(base && BundleCapabilityCount(base) == 2);
    const ZirModule *base_model = module(BundleProgram(base), "model");
    assert(base_model && base_model->type_count == 0 && base_model->global_count == 1);
    Bundle *libraries[] = {base};
    assert(BundleOpen(argv[2]) == NULL);
    Bundle *thin = BundleOpenWithLibraries(argv[2], libraries, 1);
    assert(thin && BundleCapabilityCount(thin) == 3 && BundleLawCount(thin) == 2);
    const ZirProgram *linked = BundleProgram(thin);
    const ZirModule *model = module(linked, "model");
    assert(model && model->type_count == 1 && model->global_count == 2);
    assert(function(model, "LibraryValue")->exprs == function(base_model, "LibraryValue")->exprs);
    for(int m = 0; m < linked->module_count; m++)
        for(int n = 0; n < m; n++) assert(strcmp(linked->modules[m].name, linked->modules[n].name));
    Bundle *duplicate[] = {base, base};
    assert(BundleOpenWithLibraries(argv[2], duplicate, 2) == NULL);

    /* Deep-linked programs must retain the original library text/spans even
     * after every Bundle handle providing their borrowed bodies closes. */
    ZirProgram *survivor = NativeLink(linked, "addon", "Main");
    assert(survivor);
    int retains_library = survivor->context == BundleProgram(base)->context;
    for(int c = 0; c < survivor->retained_context_count; c++)
        retains_library |= survivor->retained_contexts[c] == BundleProgram(base)->context;
    assert(retains_library);
    Bundle *standalone = BundleOpen(argv[3]);
    const ZirProgram *providers[] = {BundleProgram(base)};
    ZirProgram *delta = BundleLibraryDelta(BundleProgram(standalone), providers, 1);
    assert(delta);
    const ZirModule *delta_model = module(delta, "model");
    assert(delta_model && delta_model->function_count == 1 && delta_model->global_count == 1);
    assert(!strcmp(delta_model->functions[0].name, "AddonValue"));
    /* A full overlapping payload is never accepted as a declaration delta. */
    size_t size;
    unsigned char *data = bytes(BundleProgram(standalone), BundleProgram(standalone), &size);
    assert(BundleOpenBytesWithLibraries(data, size, libraries, 1) == NULL);
    free(data);
    data = bytes(delta, linked, &size);
    size_t caps = skip_name(data, skip_name(data, 8));
    assert(data[caps] == 2); /* Own AddonHost/CommonHost; BaseHost belongs to the library. */
    /* Hiding a fragment's host requirements must fail during loading. */
    size_t tail = caps + 4;
    for(int i = 0; i < 4; i++) tail = skip_name(data, tail);
    memset(data + caps, 0, 4);
    memmove(data + caps + 4, data + tail, size - tail);
    assert(BundleOpenBytesWithLibraries(data, size - tail + caps + 4, libraries, 1) == NULL);
    free(data);
    /* An imported capability shared by both fragments cannot change its
     * signature while keeping the same module/function identity. */
    ZirImport *common = NULL;
    for(int i = 0; i < delta_model->import_count; i++)
        if(!strcmp(delta_model->imports[i].name, "CommonHost"))
            common = &delta_model->imports[i];
    assert(common);
    strcpy(common->return_type, "s64");
    data = bytes(delta, linked, &size);
    assert(BundleOpenBytesWithLibraries(data, size, libraries, 1) == NULL);
    free(data);
    strcpy(common->return_type, "s32");
    /* Unknown/old format is rejected before any capability callback. */
    data = bytes(delta, linked, &size);
    data[4] = 26;
    assert(BundleOpenBytesWithLibraries(data, size, libraries, 1) == NULL);
    data[4] = 255;
    assert(BundleOpenBytesWithLibraries(data, size, libraries, 1) == NULL);
    free(data);
    /* Even within a composed module, a private declaration is visible only
     * in its original file. Repoint a checked call to an inaccessible body. */
    ZirFunction *addon = (ZirFunction *)function(delta_model, "AddonValue");
    int changed = 0;
    for(int e = 0; e < addon->expr_count; e++)
        if(addon->exprs[e].kind == ZIR_EXPR_CALL && !strcmp(addon->exprs[e].name, "AddonHost")) {
            addon->exprs[e].name = KeepName("Secret");
            addon->exprs[e].span = Span("intruder.zi", 1, 1);
            changed = 1;
        }
    assert(changed);
    addon->span = Span("intruder.zi", 1, 1);
    data = bytes(delta, linked, &size);
    assert(BundleOpenBytesWithLibraries(data, size, libraries, 1) == NULL);
    free(data);
    ProgramFree(delta);

    int calls = 0;
    HostBinding bindings[] = {{"model", "BaseHost", host, &calls},
                              {"model", "AddonHost", host, &calls},
                              {"model", "CommonHost", host, &calls}};
    assert(BundleInstantiate(thin, bindings, 2) == NULL && calls == 0);
    BundleInstance *first = BundleInstantiate(thin, bindings, 3);
    BundleInstance *second = BundleInstantiate(thin, bindings, 3);
    assert(first && second);
    BundleClose(base); /* composed graph retains the actual provider */
    long long result; int present;
    assert(BundleInstanceRun(first, &result, &present) && present && result == 45);
    assert(BundleInstanceRun(first, &result, &present) && present && result == 47);
    assert(BundleInstanceRun(second, &result, &present) && present && result == 45);
    BundleInstanceClose(first); BundleInstanceClose(second);
    BundleClose(thin);
    data = bytes(survivor, survivor, &size);
    Bundle *survived = BundleOpenBytes(data, size);
    assert(survived && BundleRun(survived, bindings, 3, &result, &present) && result == 45);
    BundleClose(survived); free(data); ProgramFree(survivor);
    assert(BundleRun(standalone, bindings, 3, &result, &present) && result == 45);
    BundleClose(standalone);
    Bundle *included = BundleOpen(argv[4]);
    assert(included && BundleRun(included, bindings, 3, &result, &present) && result == 45);
    BundleClose(included);
    puts("Partial modules: nested records, source/saved IR, shared code, private access, capabilities, globals and format rejection passed");
    return 0;
}
