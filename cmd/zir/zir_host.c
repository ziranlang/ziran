#include "ziran_host.h"
#include "zir_bundle.h"
#include "zir_diagnostic.h"
#include "zir_vm.h"
#include "zir_check.h"

#include "zir_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Bundle {
    ZirProgram *program;
    char entry_module[ZIR_NAME_MAX];
    char entry_function[ZIR_NAME_MAX];
    ZibLawTable laws;
    ZibAssets assets;
    int own_module_count;
    int shared_module_count;
    size_t own_asset_count;
    Bundle **libraries;
    size_t library_count;
    size_t references;
};

const ZirProgram *
BundleProgram(const Bundle *bundle)
{
    return bundle != NULL ? bundle->program : NULL;
}

static int
share_library_modules(Bundle *bundle)
{
    for(int m = bundle->own_module_count; m < bundle->program->module_count; m++) {
        ZirModule *module = &bundle->program->modules[m];
        const ZirModule *shared = NULL;
        for(size_t i = 0; i < bundle->library_count && shared == NULL; i++) {
            const ZirProgram *library = bundle->libraries[i]->program;
            for(int n = 0; n < library->module_count; n++) {
                if(strcmp(module->name, library->modules[n].name) == 0) {
                    shared = &library->modules[n];
                    break;
                }
            }
        }
        if(shared == NULL)
            return 0;
        ZirProgram *discard = ProgramNew();
        if(discard == NULL)
            return 0;
        discard->modules = malloc(sizeof(*discard->modules));
        if(discard->modules == NULL) {
            ProgramFree(discard);
            return 0;
        }
        discard->module_count = 1;
        discard->modules[0] = *module;
        ZirImport *imports = module->imports;
        discard->modules[0].imports = NULL;
        discard->modules[0].import_count = 0;
        *module = *shared;
        module->imports = imports;
        ProgramFree(discard);
        bundle->shared_module_count++;
    }
    return LinkImports(&bundle->program, 1);
}

static int
share_library_assets(Bundle *bundle)
{
    for(size_t i = 0; i < bundle->library_count; i++) {
        const ZibAssets *assets = &bundle->libraries[i]->assets;
        for(size_t a = 0; a < assets->count; a++) {
            const ZibAsset *asset = &assets->items[a];
            for(size_t b = 0; b < bundle->assets.count; b++) {
                if(strcmp(asset->name, bundle->assets.items[b].name) == 0)
                    return 0;
            }
            if(bundle->assets.count >= SIZE_MAX / sizeof(*bundle->assets.items))
                return 0;
            ZibAsset *items = realloc(bundle->assets.items,
                (bundle->assets.count + 1) * sizeof(*items));
            if(items == NULL)
                return 0;
            bundle->assets.items = items;
            items[bundle->assets.count++] = *asset;
        }
    }
    return 1;
}

/* Mirrors the linker's rule: externs kept only for laws demand no binding. */
static int
capability_import(const ZirModule *module, const ZirImport *import)
{
    if(import->kind != ZIR_IMPORT_EXTERN ||
       (import->extern_kind == ZIR_EXTERN_HOST &&
        strncmp(import->target, "ziran:", 6) == 0))
        return 0;
    for(int f = 0; f < module->function_count; f++) {
        const ZirFunction *fn = &module->functions[f];
        for(int e = 0; e < fn->expr_count; e++)
            if(fn->exprs[e].kind == ZIR_EXPR_CALL &&
               fn->exprs[e].slot_type[0] == '\0' &&
               strcmp(fn->exprs[e].name, import->name) == 0)
                return 1;
    }
    return 0;
}

static const ZirImport *
capability_at(const Bundle *bundle, size_t index, const ZirModule **owner)
{
    if(bundle == NULL)
        return NULL;
    for(int m = 0; m < bundle->program->module_count; m++) {
        const ZirModule *module = &bundle->program->modules[m];
        for(int i = 0; i < module->import_count; i++) {
            if(!capability_import(module, &module->imports[i]))
                continue;
            if(index-- == 0) {
                if(owner != NULL)
                    *owner = module;
                return &module->imports[i];
            }
        }
    }
    return NULL;
}

static Bundle *
open_bundle_stream(FILE *file, const char *name,
                   Bundle *const *libraries, size_t count)
{
    Bundle *bundle = calloc(1, sizeof(*bundle));
    if(bundle == NULL)
        return NULL;
    bundle->references = 1;
    if(count > 64 || (count != 0 && libraries == NULL)) {
        BundleClose(bundle);
        return NULL;
    }
    const ZirProgram *programs[64];
    if(count != 0) {
        bundle->libraries = calloc(count, sizeof(*bundle->libraries));
        if(bundle->libraries == NULL) {
            BundleClose(bundle);
            return NULL;
        }
    }
    for(size_t i = 0; i < count; i++) {
        if(libraries[i] == NULL || libraries[i]->references == SIZE_MAX) {
            BundleClose(bundle);
            return NULL;
        }
        libraries[i]->references++;
        bundle->libraries[bundle->library_count++] = libraries[i];
        programs[i] = libraries[i]->program;
    }
    bundle->program = BundleReadInContext(file, name, bundle->entry_module,
                                 sizeof(bundle->entry_module),
                                 bundle->entry_function,
                                 sizeof(bundle->entry_function),
                                 &bundle->laws, &bundle->assets, programs,
                                 count, &bundle->own_module_count);
    bundle->own_asset_count = bundle->assets.count;
    if(bundle->program == NULL ||
       !VmVerify(bundle->program, bundle->entry_module,
                 bundle->entry_function)) {
        BundleClose(bundle);
        return NULL;
    }
    if(!share_library_modules(bundle) || !share_library_assets(bundle)) {
        BundleClose(bundle);
        return NULL;
    }
    return bundle;
}

Bundle *
BundleOpen(const char *path)
{
    return BundleOpenWithLibraries(path, NULL, 0);
}

Bundle *
BundleOpenWithLibraries(const char *path, Bundle *const *libraries, size_t count)
{
    if(path == NULL)
        return NULL;
    FILE *file = fopen(path, "rb");
    if(file == NULL) {
        Diagnostic(Span(path, 1, 1), "zib.input", "cannot open bundle");
        return NULL;
    }
    Bundle *bundle = open_bundle_stream(file, path, libraries, count);
    fclose(file);
    return bundle;
}

Bundle *
BundleOpenBytes(const unsigned char *data, size_t size)
{
    return BundleOpenBytesWithLibraries(data, size, NULL, 0);
}

Bundle *
BundleOpenBytesWithLibraries(const unsigned char *data, size_t size,
                            Bundle *const *libraries, size_t count)
{
    if(data == NULL || size == 0)
        return NULL;
    FILE *file = ZirReadMemory(data, size);
    if(file == NULL)
        return NULL;
    Bundle *bundle = open_bundle_stream(file, "<embedded bundle>", libraries, count);
    fclose(file);
    return bundle;
}

void
BundleClose(Bundle *bundle)
{
    if(bundle != NULL) {
        if(--bundle->references != 0)
            return;
        if(bundle->program != NULL) {
            /* Executable graphs in borrowed modules belong to the library.
             * Their import tables alone belong to this lookup context. */
            for(int m = bundle->own_module_count;
                m < bundle->own_module_count + bundle->shared_module_count; m++) {
                ZirImport *imports = bundle->program->modules[m].imports;
                memset(&bundle->program->modules[m], 0, sizeof(ZirModule));
                bundle->program->modules[m].imports = imports;
            }
        }
        ProgramFree(bundle->program);
        ZibLawTableFree(&bundle->laws);
        bundle->assets.count = bundle->own_asset_count;
        ZibAssetsFree(&bundle->assets);
        for(size_t i = 0; i < bundle->library_count; i++)
            BundleClose(bundle->libraries[i]);
        free(bundle->libraries);
        free(bundle);
    }
}

size_t
BundleAssetCount(const Bundle *bundle)
{
    return bundle != NULL ? bundle->assets.count : 0;
}

const char *
BundleAssetName(const Bundle *bundle, size_t index)
{
    return bundle != NULL && index < bundle->assets.count ?
        bundle->assets.items[index].name : NULL;
}

const unsigned char *
BundleAssetData(const Bundle *bundle, size_t index)
{
    return bundle != NULL && index < bundle->assets.count ?
        bundle->assets.items[index].data : NULL;
}

size_t
BundleAssetSize(const Bundle *bundle, size_t index)
{
    return bundle != NULL && index < bundle->assets.count ?
        bundle->assets.items[index].size : 0;
}

size_t
BundleCapabilityCount(const Bundle *bundle)
{
    size_t count = 0;
    if(bundle != NULL)
        for(int m = 0; m < bundle->program->module_count; m++)
            for(int i = 0; i < bundle->program->modules[m].import_count; i++)
                count += capability_import(
                    &bundle->program->modules[m],
                    &bundle->program->modules[m].imports[i]);
    return count;
}

const char *
BundleCapabilityModule(const Bundle *bundle, size_t index)
{
    const ZirModule *owner = NULL;
    return capability_at(bundle, index, &owner) != NULL ? owner->name : NULL;
}

const char *
BundleCapabilityFunction(const Bundle *bundle, size_t index)
{
    const ZirImport *import = capability_at(bundle, index, NULL);
    return import != NULL ? import->name : NULL;
}

typedef struct BindingSet {
    const HostBinding *bindings;
    size_t count;
} BindingSet;

struct BundleInstance {
    BindingSet set;
    HostBinding *owned_bindings;
    VmInstance *vm;
};

static char *
copy_binding_name(const char *name)
{
    size_t length = strlen(name) + 1;
    char *copy = malloc(length);
    if(copy != NULL)
        memcpy(copy, name, length);
    return copy;
}

static const HostBinding *
find_binding(const BindingSet *set, const char *module, const char *function)
{
    for(size_t i = 0; i < set->count; i++)
        if(strcmp(set->bindings[i].module, module) == 0 &&
           strcmp(set->bindings[i].function, function) == 0)
            return &set->bindings[i];
    return NULL;
}

static int
dispatch(void *context, const char *module, const char *function,
         const VmHostValue *args, int arg_count, VmHostValue *result)
{
    const BindingSet *set = context;
    const HostBinding *binding = find_binding(set, module, function);
    return binding != NULL && binding->call(binding->context, module,
                                            function, args, arg_count, result);
}

static int
validate_bindings(const Bundle *bundle, const HostBinding *bindings,
                  size_t binding_count)
{
    BindingSet set = {bindings, binding_count};
    if(bundle == NULL || (binding_count != 0 && bindings == NULL))
        return 0;
    for(size_t i = 0; i < binding_count; i++) {
        if(bindings[i].module == NULL || bindings[i].function == NULL ||
           bindings[i].call == NULL)
            return 0;
        for(size_t j = 0; j < i; j++)
            if(strcmp(bindings[i].module, bindings[j].module) == 0 &&
               strcmp(bindings[i].function, bindings[j].function) == 0) {
                Diagnostic(Span("<host>", 1, 1), "zib.capability",
                           "duplicate host capability binding: %s:%s",
                           bindings[i].module, bindings[i].function);
                return 0;
            }
    }
    for(size_t i = 0; i < BundleCapabilityCount(bundle); i++) {
        const char *module = BundleCapabilityModule(bundle, i);
        const char *function = BundleCapabilityFunction(bundle, i);
        if(find_binding(&set, module, function) == NULL) {
            Diagnostic(Span("<host>", 1, 1), "zib.capability",
                       "missing host capability: %s:%s", module, function);
            return 0;
        }
    }
    return 1;
}

BundleInstance *
BundleInstantiate(const Bundle *bundle, const HostBinding *bindings,
                  size_t binding_count)
{
    if(!validate_bindings(bundle, bindings, binding_count))
        return NULL;
    BundleInstance *instance = calloc(1, sizeof(*instance));
    if(instance == NULL)
        return NULL;
    instance->set.count = binding_count;
    if(binding_count > 0) {
        instance->owned_bindings = calloc(binding_count,
                                          sizeof(*instance->owned_bindings));
        if(instance->owned_bindings == NULL) {
            BundleInstanceClose(instance);
            return NULL;
        }
        for(size_t i = 0; i < binding_count; i++) {
            instance->owned_bindings[i].module =
                copy_binding_name(bindings[i].module);
            instance->owned_bindings[i].function =
                copy_binding_name(bindings[i].function);
            instance->owned_bindings[i].call = bindings[i].call;
            instance->owned_bindings[i].context = bindings[i].context;
            if(instance->owned_bindings[i].module == NULL ||
               instance->owned_bindings[i].function == NULL) {
                BundleInstanceClose(instance);
                return NULL;
            }
        }
    }
    instance->set.bindings = instance->owned_bindings;
    instance->vm = VmInstanceOpen(bundle->program, bundle->entry_module,
                                   bundle->entry_function,
                                   dispatch, &instance->set);
    if(instance->vm == NULL) {
        BundleInstanceClose(instance);
        return NULL;
    }
    return instance;
}

int
BundleInstanceRun(BundleInstance *instance, long long *result,
                  int *has_result)
{
    return instance != NULL && VmInstanceRun(instance->vm, result,
                                              has_result);
}

void
BundleInstanceLimitSteps(BundleInstance *instance, int max_steps)
{
    if(instance != NULL)
        VmInstanceLimitSteps(instance->vm, max_steps);
}

void
BundleInstanceClose(BundleInstance *instance)
{
    if(instance == NULL)
        return;
    VmInstanceClose(instance->vm);
    for(size_t i = 0; i < instance->set.count &&
        instance->owned_bindings != NULL; i++) {
        free((void *)instance->owned_bindings[i].module);
        free((void *)instance->owned_bindings[i].function);
    }
    free(instance->owned_bindings);
    free(instance);
}

int
BundleRun(const Bundle *bundle, const HostBinding *bindings,
          size_t binding_count, long long *result, int *has_result)
{
    if(result == NULL || has_result == NULL)
        return 0;
    BundleInstance *instance = BundleInstantiate(bundle, bindings,
                                                 binding_count);
    if(instance == NULL)
        return 0;
    int ok = BundleInstanceRun(instance, result, has_result);
    BundleInstanceClose(instance);
    return ok;
}

size_t
BundleLawCount(const Bundle *bundle)
{
    if(bundle == NULL) return 0;
    size_t count = (size_t)bundle->laws.law_count;
    for(size_t i = 0; i < bundle->library_count; i++)
        count += BundleLawCount(bundle->libraries[i]);
    return count;
}

static const ZibLawRecord *
law_at(const Bundle *bundle, size_t index)
{
    if(bundle == NULL) return NULL;
    if(index < (size_t)bundle->laws.law_count)
        return &bundle->laws.laws[index];
    index -= (size_t)bundle->laws.law_count;
    for(size_t i = 0; i < bundle->library_count; i++) {
        size_t count = BundleLawCount(bundle->libraries[i]);
        if(index < count) return law_at(bundle->libraries[i], index);
        index -= count;
    }
    return NULL;
}

const char *
BundleLawModule(const Bundle *bundle, size_t index)
{
    const ZibLawRecord *law = law_at(bundle, index);
    return law != NULL ? law->module : NULL;
}

const char *
BundleLawName(const Bundle *bundle, size_t index)
{
    const ZibLawRecord *law = law_at(bundle, index);
    return law != NULL ? law->name : NULL;
}

const char *
BundleLawStatus(const Bundle *bundle, size_t index)
{
    const ZibLawRecord *law = law_at(bundle, index);
    return law != NULL ? law->status : NULL;
}

size_t
BundleLawWaiverCount(const Bundle *bundle)
{
    if(bundle == NULL) return 0;
    size_t count = (size_t)bundle->laws.waiver_count;
    for(size_t i = 0; i < bundle->library_count; i++)
        count += BundleLawWaiverCount(bundle->libraries[i]);
    return count;
}

static const ZibLawWaiverRecord *
waiver_at(const Bundle *bundle, size_t index)
{
    if(bundle == NULL) return NULL;
    if(index < (size_t)bundle->laws.waiver_count)
        return &bundle->laws.waivers[index];
    index -= (size_t)bundle->laws.waiver_count;
    for(size_t i = 0; i < bundle->library_count; i++) {
        size_t count = BundleLawWaiverCount(bundle->libraries[i]);
        if(index < count) return waiver_at(bundle->libraries[i], index);
        index -= count;
    }
    return NULL;
}

const char *
BundleLawWaiverName(const Bundle *bundle, size_t index)
{
    const ZibLawWaiverRecord *waiver = waiver_at(bundle, index);
    return waiver != NULL ? waiver->name : NULL;
}

const char *
BundleLawWaiverReason(const Bundle *bundle, size_t index)
{
    const ZibLawWaiverRecord *waiver = waiver_at(bundle, index);
    return waiver != NULL ? waiver->reason : NULL;
}
