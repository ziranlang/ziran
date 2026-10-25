#include "zir_memory.h"
#include "ziran_host.h"
#include "zir_bundle.h"
#include "zir_diagnostic.h"
#include "zir_vm.h"
#include "zir_check.h"
#ifdef ZIRAN_NATIVE_ZX
#include "zir_sha256.h"
#include "zir_zx_native.h"
#endif
#include "zir_zx_wasm.h"
#include "zir_zx_wasm_platform.h"

#include "zir_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Capability {
    const char *module;
    const char *function;
} Capability;

struct Bundle {
    ZirProgram *program;
    const ZirModule *entry_owner;
    const ZirFunction *entry;
    ZxLawTable laws;
    ZxAssets assets;
    int own_module_count;
    int shared_module_count;
    size_t own_asset_count;
    Bundle **libraries;
    size_t library_count;
    size_t references;
    unsigned char digest[32]; /* SHA-256 of the bundle's bytes (native engine) */
    size_t capability_count;
    Capability capabilities[];
};

const ZirProgram *
BundleProgram(const Bundle *bundle)
{
    return bundle != NULL ? bundle->program : NULL;
}

const ZirFunction *
BundleEntry(const Bundle *bundle)
{
    return bundle != NULL ? bundle->entry : NULL;
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
        module->context = bundle->program->context;
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
        const ZxAssets *assets = &bundle->libraries[i]->assets;
        for(size_t a = 0; a < assets->count; a++) {
            const ZxAsset *asset = &assets->items[a];
            for(size_t b = 0; b < bundle->assets.count; b++) {
                if(strcmp(asset->name, bundle->assets.items[b].name) == 0)
                    return 0;
            }
            if(bundle->assets.count >= SIZE_MAX / sizeof(*bundle->assets.items))
                return 0;
            ZxAsset *items = realloc(bundle->assets.items,
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

/* The final graph is immutable. Retain only borrowed names in its enumeration
 * order, rather than rescanning every function for each count/index query. */
static int
cache_capabilities(Bundle **slot, size_t count)
{
    Bundle *bundle = *slot;
    if(count == 0)
        return 1;
    if(count > (SIZE_MAX - sizeof(*bundle)) / sizeof(Capability))
        return 0;
    if(count != 0) {
        Bundle *grown = realloc(bundle, sizeof(*bundle) + count * sizeof(Capability));
        if(grown == NULL)
            return 0;
        *slot = bundle = grown;
    }
    bundle->capability_count = count;
    size_t next = 0;
    for(int m = 0; m < bundle->program->module_count; m++) {
        const ZirModule *module = &bundle->program->modules[m];
        for(int i = 0; i < module->import_count; i++)
            if(capability_import(module, &module->imports[i])) {
                if(next == count)
                    return 0;
                bundle->capabilities[next++] = (Capability){module->name, module->imports[i].name};
            }
    }
    return next == count;
}

static Bundle *
open_bundle_stream(FILE *file, const char *name, const unsigned char digest[32],
                   Bundle *const *libraries, size_t count)
{
    char entry_module[ZIR_NAME_MAX], entry_function[ZIR_NAME_MAX];
    Bundle *bundle = calloc(1, sizeof(*bundle));
    if(bundle == NULL)
        return NULL;
    bundle->references = 1;
    if(digest != NULL)
        memcpy(bundle->digest, digest, sizeof(bundle->digest));
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
    size_t capability_count = 0;
    bundle->program = BundleReadInContext(file, name, entry_module,
                                 sizeof(entry_module), entry_function,
                                 sizeof(entry_function),
                                 &bundle->laws, &bundle->assets, programs,
                                 count, &bundle->own_module_count, &capability_count);
    bundle->own_asset_count = bundle->assets.count;
    if(bundle->program == NULL || !share_library_modules(bundle) ||
       !share_library_assets(bundle) ||
       !VmVerify(bundle->program, entry_module, entry_function)) {
        BundleClose(bundle);
        return NULL;
    }
    for(int m = 0; m < bundle->program->module_count; m++) {
        const ZirModule *module = &bundle->program->modules[m];
        if(strcmp(module->name, entry_module) != 0)
            continue;
        for(int f = 0; f < module->function_count; f++)
            if(strcmp(module->functions[f].name, entry_function) == 0) {
                bundle->entry_owner = module;
                bundle->entry = &module->functions[f];
            }
    }
    /* Library graphs are disjoint (the reader rejects duplicate modules),
     * so their already-validated requirements add to the bundle's own list. */
    for(size_t i = 0; i < bundle->library_count; i++) {
        size_t inherited = bundle->libraries[i]->capability_count;
        if(inherited > SIZE_MAX - capability_count) {
            BundleClose(bundle);
            return NULL;
        }
        capability_count += inherited;
    }
    if(bundle->entry == NULL || !cache_capabilities(&bundle, capability_count)) {
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
        Diagnostic(Span(path, 1, 1), "zx.input", "cannot open bundle");
        return NULL;
    }
    unsigned char *digest = NULL;
#ifdef ZIRAN_NATIVE_ZX
    /* Native code caches are keyed by the bundle's exact bytes. */
    unsigned char bytes_digest[32];
    unsigned char buffer[4096];
    size_t read;
    Sha256 hash;
    Sha256Begin(&hash);
    while((read = fread(buffer, 1, sizeof(buffer), file)) > 0)
        Sha256Add(&hash, buffer, read);
    Sha256End(&hash, bytes_digest);
    if(!ferror(file) && fseek(file, 0, SEEK_SET) == 0)
        digest = bytes_digest;
#endif
    Bundle *bundle = open_bundle_stream(file, path, digest, libraries, count);
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
    unsigned char *digest = NULL;
#ifdef ZIRAN_NATIVE_ZX
    unsigned char bytes_digest[32];
    Sha256 hash;
    Sha256Begin(&hash);
    Sha256Add(&hash, data, size);
    Sha256End(&hash, bytes_digest);
    digest = bytes_digest;
#endif
    Bundle *bundle = open_bundle_stream(file, "<embedded bundle>", digest, libraries, count);
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
        ZxLawTableFree(&bundle->laws);
        bundle->assets.count = bundle->own_asset_count;
        ZxAssetsFree(&bundle->assets);
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
    return bundle != NULL ? bundle->capability_count : 0;
}

const char *
BundleCapabilityModule(const Bundle *bundle, size_t index)
{
    return bundle != NULL && index < bundle->capability_count ?
        bundle->capabilities[index].module : NULL;
}

const char *
BundleCapabilityFunction(const Bundle *bundle, size_t index)
{
    return bundle != NULL && index < bundle->capability_count ?
        bundle->capabilities[index].function : NULL;
}

typedef struct BindingSet {
    const HostBinding *bindings;
    size_t count;
} BindingSet;

struct BundleInstance {
    BindingSet set;
    HostBinding *owned_bindings;
#ifdef ZIRAN_NATIVE_ZX
    ZxNative *native;
    const char **capability_names;
#else
    VmInstance *vm;
    /* Present when a wasm provider is installed and the bundle lowered;
     * otherwise the instance runs on the portable VM above. */
    ZxWasm *wasm;
#endif
};

static int
compare_bindings(const void *left, const void *right)
{
    const HostBinding *a = left, *b = right;
    int module = strcmp(a->module, b->module);
    return module != 0 ? module : strcmp(a->function, b->function);
}

static const HostBinding *
find_binding(const BindingSet *set, const char *module, const char *function)
{
    HostBinding key = {.module = module, .function = function};
    size_t low = 0, high = set->count;
    while(low < high) {
        size_t middle = low + (high - low) / 2;
        int order = compare_bindings(&key, &set->bindings[middle]);
        if(order == 0)
            return &set->bindings[middle];
        if(order < 0) high = middle;
        else low = middle + 1;
    }
    return NULL;
}

static int
dispatch(void *context, const char *module, const char *function,
         const VmHostValue *args, int arg_count, VmHostValue *result)
{
    const BindingSet *set = context;
    const HostBinding *binding = NULL;
    if(set->count == 1) {
        /* Keep the common single-capability call as direct name checks, so it
         * does not pay for the general search's call and loop bookkeeping. */
        const HostBinding *only = set->bindings;
        if(strcmp(only->module, module) == 0 && strcmp(only->function, function) == 0)
            binding = only;
    } else binding = find_binding(set, module, function);
    return binding != NULL && binding->call(binding->context, module,
                                            function, args, arg_count, result);
}

static int
validate_bindings(const Bundle *bundle, const BindingSet *set)
{
    for(size_t i = 1; i < set->count; i++) {
        if(compare_bindings(&set->bindings[i - 1], &set->bindings[i]) == 0) {
            Diagnostic(Span("<host>", 1, 1), "zx.capability",
                       "duplicate host capability binding: %s:%s",
                       set->bindings[i].module, set->bindings[i].function);
            return 0;
        }
    }
    for(size_t i = 0; i < BundleCapabilityCount(bundle); i++) {
        const char *module = BundleCapabilityModule(bundle, i);
        const char *function = BundleCapabilityFunction(bundle, i);
        if(find_binding(set, module, function) == NULL) {
            Diagnostic(Span("<host>", 1, 1), "zx.capability",
                       "missing host capability: %s:%s", module, function);
            return 0;
        }
    }
    return 1;
}

#ifdef ZIRAN_NATIVE_ZX
/* A bundle and the libraries it links name one native program. */
static void
add_identity(Sha256 *hash, const Bundle *bundle)
{
    Sha256Add(hash, bundle->digest, sizeof(bundle->digest));
    for(size_t i = 0; i < bundle->library_count; i++)
        add_identity(hash, bundle->libraries[i]);
}
#endif

BundleInstance *
BundleInstantiate(const Bundle *bundle, const HostBinding *bindings,
                  size_t binding_count)
{
    if(bundle == NULL || (binding_count != 0 && bindings == NULL) ||
       binding_count > SIZE_MAX / sizeof(HostBinding))
        return NULL;
    size_t bytes = binding_count * sizeof(HostBinding);
    for(size_t i = 0; i < binding_count; i++) {
        if(bindings[i].module == NULL || bindings[i].function == NULL || bindings[i].call == NULL)
            return NULL;
        size_t length = strlen(bindings[i].module);
        if(length >= SIZE_MAX - bytes) return NULL;
        bytes += length + 1;
        length = strlen(bindings[i].function);
        if(length >= SIZE_MAX - bytes) return NULL;
        bytes += length + 1;
    }
    BundleInstance *instance = calloc(1, sizeof(*instance));
    if(instance == NULL)
        return NULL;
    instance->set.count = binding_count;
    if(binding_count > 0) {
        instance->owned_bindings = malloc(bytes);
        if(instance->owned_bindings == NULL) {
            BundleInstanceClose(instance);
            return NULL;
        }
        char *name = (char *)(instance->owned_bindings + binding_count);
        for(size_t i = 0; i < binding_count; i++) {
            instance->owned_bindings[i] = bindings[i];
            size_t length = strlen(bindings[i].module) + 1;
            memcpy(name, bindings[i].module, length);
            instance->owned_bindings[i].module = name;
            name += length;
            length = strlen(bindings[i].function) + 1;
            memcpy(name, bindings[i].function, length);
            instance->owned_bindings[i].function = name;
            name += length;
        }
        if(binding_count > 1)
            qsort(instance->owned_bindings, binding_count, sizeof(HostBinding), compare_bindings);
    }
    instance->set.bindings = instance->owned_bindings;
    if(!validate_bindings(bundle, &instance->set)) {
        BundleInstanceClose(instance);
        return NULL;
    }
#ifdef ZIRAN_NATIVE_ZX
    /* Native platforms run bundles only as native code; a bundle that cannot
     * be compiled is an error, never a slower fallback. */
    unsigned char identity[32];
    Sha256 hash;
    Sha256Begin(&hash);
    add_identity(&hash, bundle);
    Sha256End(&hash, identity);
    size_t count = bundle->capability_count;
    instance->capability_names = calloc(count * 2 + 1, sizeof(*instance->capability_names));
    if(instance->capability_names == NULL) {
        BundleInstanceClose(instance);
        return NULL;
    }
    for(size_t i = 0; i < count; i++) {
        instance->capability_names[i] = bundle->capabilities[i].module;
        instance->capability_names[count + i] = bundle->capabilities[i].function;
    }
    instance->native = ZxNativeOpen(bundle->program, bundle->entry_owner, bundle->entry,
                                    identity, instance->capability_names,
                                    instance->capability_names + count, count,
                                    dispatch, &instance->set);
    if(instance->native == NULL) {
#else
    /* With a wasm provider installed, the wasm tier takes the bundles it
     * can lower and the portable VM takes the rest; without one, every
     * bundle runs on the VM exactly as before. */
    if(ZxWasmPlatformActive() != NULL) {
        instance->wasm = ZxWasmOpen(bundle->program, bundle->entry_owner,
                                    bundle->entry, NULL, NULL,
                                    bundle->capability_count);
        if(instance->wasm != NULL)
            return instance;
    }
    instance->vm = VmInstanceOpenVerified(bundle->program, bundle->entry_owner,
                                           bundle->entry, dispatch, &instance->set);
    if(instance->vm == NULL) {
#endif
        BundleInstanceClose(instance);
        return NULL;
    }
    return instance;
}

int
BundleInstanceRun(BundleInstance *instance, long long *result,
                  int *has_result)
{
#ifdef ZIRAN_NATIVE_ZX
    return instance != NULL && ZxNativeRun(instance->native, result, has_result);
#else
    if(instance == NULL)
        return 0;
    if(instance->wasm != NULL)
        return ZxWasmRun(instance->wasm, result, has_result);
    return VmInstanceRun(instance->vm, result, has_result);
#endif
}

void
BundleInstanceLimitSteps(BundleInstance *instance, int max_steps)
{
#ifdef ZIRAN_NATIVE_ZX
    if(instance != NULL)
        ZxNativeLimitSteps(instance->native, max_steps);
#else
    if(instance == NULL)
        return;
    if(instance->wasm != NULL) {
        ZxWasmLimitSteps(instance->wasm, max_steps);
        return;
    }
    VmInstanceLimitSteps(instance->vm, max_steps);
#endif
}

void
BundleInstanceMatchFieldsByName(BundleInstance *instance, int enabled)
{
#ifdef ZIRAN_NATIVE_ZX
    if(instance != NULL)
        ZxNativeMatchFieldsByName(instance->native, enabled);
#else
    if(instance == NULL)
        return;
    if(instance->wasm != NULL) {
        ZxWasmMatchFieldsByName(instance->wasm, enabled);
        return;
    }
    VmInstanceMatchFieldsByName(instance->vm, enabled);
#endif
}

void
BundleInstanceClose(BundleInstance *instance)
{
    if(instance == NULL)
        return;
#ifdef ZIRAN_NATIVE_ZX
    ZxNativeClose(instance->native);
    free(instance->capability_names);
#else
    ZxWasmClose(instance->wasm);
    VmInstanceClose(instance->vm);
#endif
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

static const ZxLawRecord *
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
    const ZxLawRecord *law = law_at(bundle, index);
    return law != NULL ? law->module : NULL;
}

const char *
BundleLawName(const Bundle *bundle, size_t index)
{
    const ZxLawRecord *law = law_at(bundle, index);
    return law != NULL ? law->name : NULL;
}

const char *
BundleLawStatus(const Bundle *bundle, size_t index)
{
    const ZxLawRecord *law = law_at(bundle, index);
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

static const ZxLawWaiverRecord *
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
    const ZxLawWaiverRecord *waiver = waiver_at(bundle, index);
    return waiver != NULL ? waiver->name : NULL;
}

const char *
BundleLawWaiverReason(const Bundle *bundle, size_t index)
{
    const ZxLawWaiverRecord *waiver = waiver_at(bundle, index);
    return waiver != NULL ? waiver->reason : NULL;
}
