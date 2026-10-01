#include "ziran_host.h"
#include "zir_bundle.h"
#include "zir_diagnostic.h"
#include "zir_vm.h"

#include "zir_bundle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Bundle {
    ZirProgram *program;
    char entry_module[ZIR_NAME_MAX];
    char entry_function[ZIR_NAME_MAX];
    ZibLawTable laws;
};

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

Bundle *
BundleOpen(const char *path)
{
    FILE *file;
    Bundle *bundle;
    if(path == NULL)
        return NULL;
    file = fopen(path, "rb");
    if(file == NULL) {
        Diagnostic(Span(path, 1, 1), "zib.input", "cannot open bundle");
        return NULL;
    }
    bundle = calloc(1, sizeof(*bundle));
    if(bundle != NULL)
        bundle->program = BundleRead(file, path, bundle->entry_module,
                                     sizeof(bundle->entry_module),
                                     bundle->entry_function,
                                     sizeof(bundle->entry_function),
                                     &bundle->laws);
    fclose(file);
    if(bundle == NULL)
        return NULL;
    if(bundle->program == NULL ||
       !VmVerify(bundle->program, bundle->entry_module,
                 bundle->entry_function)) {
        BundleClose(bundle);
        return NULL;
    }
    return bundle;
}

void
BundleClose(Bundle *bundle)
{
    if(bundle != NULL) {
        ProgramFree(bundle->program);
        ZibLawTableFree(&bundle->laws);
        free(bundle);
    }
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
    return bundle == NULL ? 0 : (size_t)bundle->laws.law_count;
}

const char *
BundleLawModule(const Bundle *bundle, size_t index)
{
    return bundle != NULL && index < (size_t)bundle->laws.law_count ?
           bundle->laws.laws[index].module : NULL;
}

const char *
BundleLawName(const Bundle *bundle, size_t index)
{
    return bundle != NULL && index < (size_t)bundle->laws.law_count ?
           bundle->laws.laws[index].name : NULL;
}

const char *
BundleLawStatus(const Bundle *bundle, size_t index)
{
    return bundle != NULL && index < (size_t)bundle->laws.law_count ?
           bundle->laws.laws[index].status : NULL;
}

size_t
BundleLawWaiverCount(const Bundle *bundle)
{
    return bundle == NULL ? 0 : (size_t)bundle->laws.waiver_count;
}

const char *
BundleLawWaiverName(const Bundle *bundle, size_t index)
{
    return bundle != NULL && index < (size_t)bundle->laws.waiver_count ?
           bundle->laws.waivers[index].name : NULL;
}

const char *
BundleLawWaiverReason(const Bundle *bundle, size_t index)
{
    return bundle != NULL && index < (size_t)bundle->laws.waiver_count ?
           bundle->laws.waivers[index].reason : NULL;
}
