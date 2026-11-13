#include "zir_memory.h"
#include "ziran_host.h"
#include "zir_bundle.h"
#include "compiler_bundle.h"
#include "zir_diagnostic.h"
#include "zir_vm.h"
#include "zir_check.h"
#include "zir_sha256.h"
#include "zir_vm_internal.h"
/* Bundles run as compiled code: compiled at run time with TinyCC on native
 * platforms, or linked into the host ahead of time in the browser. */
#if defined(ZIRAN_NATIVE_ZX) || defined(ZIRAN_LINKED_ZX)
#define ZIRAN_COMPILED_ZX 1
#include "zir_sha256.h"
#include "zir_zx_native.h"
#include <pthread.h>
#endif
#include "zir_zx_wasm.h"
#include "zir_zx_wasm_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Bundle {
    ZirProgram *program;
    const ZirModule *entry_owner;
    const ZirFunction *entry;
    ZxLawTable laws;
    ZxAssets assets;
    int own_module_count;
    size_t own_asset_count;
    Bundle **libraries;
    size_t library_count;
    size_t references;
    unsigned char digest[32]; /* SHA-256 of the bytes before the assets (native engine) */
    /* 1 while a bundle opened on its cached native code is unchecked, -1
     * when its later check failed. */
    int unchecked;
    /* Set by BundleTrustNativeAssembly: procedures' #asm variants compile. */
    int native_assembly;
    /* An unchecked bundle owns its capability names: checking it later may
     * move the imports that instances' names would otherwise borrow. */
    char *capability_text;
    size_t capability_count;
    Capability capabilities[];
};

static void bundle_identity(const Bundle *bundle, unsigned char identity[32]);

#ifdef ZIRAN_COMPILED_ZX
static int check_unchecked(Bundle *bundle);
#endif

/* Set by `zi2zx run --vm`: instances run on the portable VM even where
 * compiled code is available, so the VM itself can be tested anywhere. */
static int portable_vm;

void
BundleUsePortableVm(int enabled)
{
    portable_vm = enabled != 0;
}

void
BundleTrustNativeAssembly(Bundle *bundle)
{
    /* A library is a bundle of its own, so its variants never run. */
    if(bundle != NULL && bundle->library_count == 0)
        bundle->native_assembly = 1;
}

const ZirProgram *
BundleProgram(const Bundle *bundle)
{
#ifdef ZIRAN_COMPILED_ZX
    if(bundle != NULL && !check_unchecked((Bundle *)bundle))
        return NULL;
#endif
    return bundle != NULL ? bundle->program : NULL;
}

const ZirFunction *
BundleEntry(const Bundle *bundle)
{
    return bundle != NULL ? bundle->entry : NULL;
}

const char *
BundleEntryModuleName(const Bundle *bundle)
{
    return bundle != NULL && bundle->entry_owner != NULL ?
        bundle->entry_owner->name : NULL;
}

const char *
BundleEntryFunctionName(const Bundle *bundle)
{
    return bundle != NULL && bundle->entry != NULL ?
        bundle->entry->name : NULL;
}

static int
share_library_functions(Bundle *bundle)
{
    for(int m = 0; m < bundle->program->module_count; m++) {
        ZirModule *module = &bundle->program->modules[m];
        const ZirModule *shared = module->bundle_library;
        if(shared == NULL) continue;
        if(module->function_count != module->bundle_own_functions + shared->function_count)
            return 0;
        for(int f = 0; f < shared->function_count; f++) {
            ZirFunction *function = &module->functions[module->bundle_own_functions + f];
            free(function->stmts);
            free(function->exprs);
            *function = shared->functions[f];
        }
        module->bundle_shared_functions = 1;
    }
    return LinkImports(&bundle->program, 1);
}

/* Library assets follow the bundle's own, borrowing the library's storage. */
static int
share_library_assets(Bundle *bundle)
{
    ZxAssets *own = &bundle->assets;
    for(size_t i = 0; i < bundle->library_count; i++)
        for(size_t a = 0; a < bundle->libraries[i]->assets.count; a++) {
            const ZxAsset *asset = &bundle->libraries[i]->assets.items[a];
            for(size_t b = 0; b < own->count; b++)
                if(strcmp(asset->name, own->items[b].name) == 0) return 0;
            ZxAsset *items = own->count < SIZE_MAX / sizeof(*items) - 1 ?
                realloc(own->items, (own->count + 1) * sizeof(*items)) : NULL;
            if(items == NULL) return 0;
            own->items = items;
            items[own->count++] = *asset;
        }
    return 1;
}

/* The final graph is immutable. Retain only borrowed names in its enumeration
 * order, rather than rescanning every function for each count/index query.
 * A capability keeps its module/function identity even when both an owned
 * declaration and library code call it. */
static int
cache_capabilities(Bundle **slot)
{
    Bundle *bundle = *slot;
    size_t count = 0, text = 0;
    Capability *list = BundleLinkedCapabilities(bundle->program,
        bundle->program->module_count, 0, &count);
    Bundle *grown = list != NULL && count <= (SIZE_MAX - sizeof(*bundle)) / sizeof(*list) ?
        realloc(bundle, sizeof(*bundle) + count * sizeof(*list)) : NULL;
    if(grown == NULL) {
        free(list);
        return 0;
    }
    *slot = bundle = grown;
    bundle->capability_count = count;
    memcpy(bundle->capabilities, list, count * sizeof(*list));
    free(list);
    /* Checking an unchecked bundle later may move the imports. */
    if(bundle->unchecked == 1) {
        for(size_t i = 0; i < count; i++)
            text += (size_t)(bundle->capabilities[i].module.length +
                             bundle->capabilities[i].function.length) + 2;
        char *copy = bundle->capability_text = malloc(text ? text : 1);
        if(copy == NULL)
            return 0;
        for(Capability *row = bundle->capabilities; row < bundle->capabilities + count; row++) {
            row->module = BundleKeepText(&copy, row->module);
            row->function = BundleKeepText(&copy, row->function);
        }
    }
    return 1;
}

static int
find_bundle_entry(Bundle *bundle, const char *entry_module, const char *entry_function)
{
    const ZirModule *owner = NULL;
    const ZirFunction *entry = NULL;
    for(int m = 0; m < bundle->program->module_count; m++) {
        const ZirModule *module = &bundle->program->modules[m];
        if(strcmp(module->name, entry_module) != 0)
            continue;
        for(int f = 0; f < module->function_count; f++)
            if(strcmp(module->functions[f].name, entry_function) == 0) {
                owner = module;
                entry = &module->functions[f];
            }
    }
    bundle->entry_owner = owner;
    bundle->entry = entry;
    return entry != NULL;
}

/* A bundle and the libraries it links name one native program. */
static void
add_identity(Sha256 *hash, const Bundle *bundle)
{
    Sha256Add(hash, bundle->digest, sizeof(bundle->digest));
    for(size_t i = 0; i < bundle->library_count; i++)
        add_identity(hash, bundle->libraries[i]);
}

static void
bundle_identity(const Bundle *bundle, unsigned char identity[32])
{
    Sha256 hash;
    Sha256Begin(&hash);
    add_identity(&hash, bundle);
    Sha256End(&hash, identity);
}

void
BundleCodeIdentity(const Bundle *bundle, unsigned char identity[32])
{
    if(identity == NULL)
        return;
    if(bundle == NULL) {
        memset(identity, 0, 32);
        return;
    }
    bundle_identity(bundle, identity);
}

#ifdef ZIRAN_NATIVE_ZX
/* Code compiled with a trusted bundle's #asm variants is cached apart from
 * the same bytes opened untrusted, which never link it. */
static void
native_identity(const Bundle *bundle, unsigned char identity[32])
{
    if(!bundle->native_assembly)
        return;
    static const char marker[] = "native #asm";
    Sha256 hash;
    Sha256Begin(&hash);
    Sha256Add(&hash, identity, 32);
    Sha256Add(&hash, marker, sizeof(marker) - 1);
    Sha256End(&hash, identity);
}
#endif

#ifdef ZIRAN_COMPILED_ZX
static pthread_mutex_t check_lock = PTHREAD_MUTEX_INITIALIZER;

/* Checks a bundle opened unchecked the first time something needs its
 * checked program: compiling it, or reading it with BundleProgram. Checking
 * may move functions, so the entry is found again, and the capabilities it
 * finds must be the ones the bundle already lists. */
static int
check_unchecked(Bundle *bundle)
{
    pthread_mutex_lock(&check_lock);
    if(bundle->unchecked == 1) {
        char module[ZIR_NAME_MAX], function[ZIR_NAME_MAX];
        snprintf(module, sizeof(module), "%s", bundle->entry_owner->name);
        snprintf(function, sizeof(function), "%s", bundle->entry->name);
        int ok = BundleCheckTrusted(&bundle->program, bundle->own_module_count, &bundle->laws) &&
                 VmVerify(bundle->program, module, function) &&
                 find_bundle_entry(bundle, module, function);
        size_t count = 0;
        Capability *list = ok ? BundleLinkedCapabilities(bundle->program,
            bundle->program->module_count, 0, &count) : NULL;
        Header found = {.capabilities = {bundle->capabilities, (int64_t)bundle->capability_count}};
        ok = list != NULL && compiler_bundle_Differs(
            (Header){.capabilities = {list, (int64_t)count}}, found) == Problem_None;
        free(list);
        if(!ok)
            Diagnostic(Span("<bundle>", 1, 1), "zx.invalid",
                       "bundle opened on cached native code failed checking");
        bundle->unchecked = ok ? 0 : -1;
    }
    int checked = bundle->unchecked == 0;
    pthread_mutex_unlock(&check_lock);
    return checked;
}

/* Copies the entry of a bundle still unchecked while no check can move it;
 * 0 once the bundle is checked. */
static int
unchecked_entry(const Bundle *bundle, ZirFunction *entry)
{
    pthread_mutex_lock(&check_lock);
    int unchecked = bundle->unchecked == 1;
    if(unchecked)
        *entry = *bundle->entry;
    pthread_mutex_unlock(&check_lock);
    return unchecked;
}
#endif

#ifdef ZIRAN_NATIVE_ZX
void
BundleIdentity(const Bundle *bundle, unsigned char identity[32])
{
    bundle_identity(bundle, identity);
}

/* The bundle's program as C for a host that links it ahead of time, which
 * then runs it for these exact bytes (zi2zx native-c). */
int
BundleWriteLinked(const Bundle *bundle, const char *directory, unsigned char identity[32])
{
    if(bundle == NULL || !check_unchecked((Bundle *)bundle))
        return 0;
    bundle_identity(bundle, identity);
    char error[2048];
    int ok = ZxNativeWriteLinked(bundle->program, bundle->entry_owner, bundle->entry, identity,
                                 bundle->capabilities, bundle->capability_count, directory,
                                 error, sizeof(error));
    if(!ok)
        Diagnostic(bundle->entry->span, "zx.native", "cannot write linked code: %s", error);
    return ok;
}
#endif

static Bundle *
open_bundle(const unsigned char *bytes, size_t size, const char *name,
            Bundle *const *libraries, size_t count)
{
    char entry_module[ZIR_NAME_MAX], entry_function[ZIR_NAME_MAX];
    const ZirProgram *programs[64];
    Bundle *bundle = calloc(1, sizeof(*bundle));
    if(bundle == NULL)
        return NULL;
    bundle->references = 1;
    if(count > 64 || (count != 0 && libraries == NULL) ||
       (count != 0 && (bundle->libraries = calloc(count, sizeof(*bundle->libraries))) == NULL))
        goto failed;
    for(size_t i = 0; i < count; i++) {
        if(libraries[i] == NULL || libraries[i]->references == SIZE_MAX
#ifdef ZIRAN_COMPILED_ZX
           || !check_unchecked(libraries[i])
#endif
           )
            goto failed;
        libraries[i]->references++;
        bundle->libraries[bundle->library_count++] = libraries[i];
        programs[i] = libraries[i]->program;
    }
    int trusted = 0;
    /* Native code caches are keyed by the bytes that code depends on:
     * everything before the assets. Compiled code cached or linked for
     * these exact bytes means this compiler checked them before, so they
     * open without checking until something needs the checked program. A
     * read that trust does not settle reads again. */
    size_t end = BundleProgramEnd(bytes, size);
    if(end > 0) {
        Sha256 hash;
        Sha256Begin(&hash);
        Sha256Add(&hash, bytes, end);
        Sha256End(&hash, bundle->digest);
    }
#ifdef ZIRAN_COMPILED_ZX
    if(end > 0 && count == 0 && !portable_vm) {
        unsigned char identity[32];
        bundle_identity(bundle, identity);
#ifdef ZIRAN_NATIVE_ZX
        trusted = ZxNativeCached(identity);
#else
        trusted = ZxNativeLinked(identity);
#endif
    }
#endif
    int attempts = trusted ? 2 : 1;
    for(int attempt = 0; attempt < attempts && bundle->program == NULL; attempt++) {
        ZxLawTableFree(&bundle->laws);
        bundle->program = BundleReadInContext(bytes, size, name, entry_module,
                                     sizeof(entry_module), entry_function,
                                     sizeof(entry_function),
                                     &bundle->laws, &bundle->assets, programs,
                                     count, &bundle->own_module_count, &trusted);
    }
    bundle->own_asset_count = bundle->assets.count;
    bundle->unchecked = trusted;
#ifdef ZIRAN_COMPILED_ZX
    const char *timing = getenv("ZIRAN_NATIVE_TIMING");
    if(trusted && timing != NULL && strcmp(timing, "1") == 0)
#ifdef ZIRAN_NATIVE_ZX
        fprintf(stderr, "ziran: native %s: opened on cached code, unchecked\n", name);
#else
        fprintf(stderr, "ziran: native %s: opened on linked code, unchecked\n", name);
#endif
#endif
    if(bundle->program == NULL || !share_library_functions(bundle) ||
       !share_library_assets(bundle) ||
       (!trusted && !VmVerify(bundle->program, entry_module, entry_function)) ||
       !find_bundle_entry(bundle, entry_module, entry_function) || !cache_capabilities(&bundle))
        goto failed;
    return bundle;
failed:
    BundleClose(bundle);
    return NULL;
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
    size_t size = 0;
    unsigned char *bytes = file != NULL ? BundleReadStream(file, &size) : NULL;
    if(file != NULL)
        fclose(file);
    if(bytes == NULL) {
        Diagnostic(Span(path, 1, 1), "zx.input", "cannot open bundle");
        return NULL;
    }
    Bundle *bundle = open_bundle(bytes, size, path, libraries, count);
    free(bytes);
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
    return open_bundle(data, size, "<embedded bundle>", libraries, count);
}

void
BundleClose(Bundle *bundle)
{
    if(bundle != NULL) {
        if(--bundle->references != 0)
            return;
        ProgramFree(bundle->program);
        ZxLawTableFree(&bundle->laws);
        bundle->assets.count = bundle->own_asset_count;
        ZxAssetsFree(&bundle->assets);
        free(bundle->capability_text);
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

size_t
BundleOwnAssetCount(const Bundle *bundle)
{
    return bundle != NULL ? bundle->own_asset_count : 0;
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
        bundle->capabilities[index].module.data : NULL;
}

const char *
BundleCapabilityFunction(const Bundle *bundle, size_t index)
{
    return bundle != NULL && index < bundle->capability_count ?
        bundle->capabilities[index].function.data : NULL;
}

static const ZirImport *
capability_declaration(const Bundle *bundle, size_t index)
{
    if(bundle == NULL || index >= bundle->capability_count)
        return NULL;
    const char *module_name = bundle->capabilities[index].module.data;
    const char *function_name = bundle->capabilities[index].function.data;
    for(int m = 0; m < bundle->program->module_count; m++) {
        const ZirModule *module = &bundle->program->modules[m];
        if(strcmp(module->name, module_name) != 0)
            continue;
        for(size_t i = 0; i < (size_t)module->import_count; i++)
            if(strcmp(module->imports[i].name, function_name) == 0)
                return &module->imports[i];
    }
    return NULL;
}

int
BundleCapabilityAbiAt(const Bundle *bundle, size_t index,
                      BundleCapabilityAbi *abi)
{
    const ZirImport *declaration = bundle != NULL && abi != NULL ?
        capability_declaration(bundle, index) : NULL;
    if(declaration == NULL)
        return 0;
    *abi = (BundleCapabilityAbi){
        .module = BundleCapabilityModule(bundle, index),
        .function = BundleCapabilityFunction(bundle, index),
        .signature = declaration->signature,
        .arguments = declaration->args,
        .return_type = declaration->return_type,
        .variable_arguments = declaration->is_varargs != 0,
        .reads_arguments = declaration->reads_arguments != 0
    };
    return 1;
}

static int
same_optional_text(const char *left, const char *right)
{
    left = left != NULL ? left : "";
    right = right != NULL ? right : "";
    return strcmp(left, right) == 0;
}

static int
same_capability_abi(const Bundle *bundle, size_t index,
                    const Bundle *candidate)
{
    BundleCapabilityAbi left, right;
    if(!BundleCapabilityAbiAt(bundle, index, &left))
        return 0;
    for(size_t i = 0; i < candidate->capability_count; i++) {
        if(!BundleCapabilityAbiAt(candidate, i, &right))
            return 0;
        if(strcmp(left.module, right.module) == 0 &&
           strcmp(left.function, right.function) == 0 &&
           same_optional_text(left.signature, right.signature) &&
           same_optional_text(left.arguments, right.arguments) &&
           same_optional_text(left.return_type, right.return_type) &&
           left.variable_arguments == right.variable_arguments &&
           left.reads_arguments == right.reads_arguments)
            return 1;
    }
    return 0;
}

int
BundleAbiCompatible(const Bundle *left, const Bundle *right)
{
    if(left == NULL || right == NULL ||
       !same_optional_text(BundleEntryModuleName(left), BundleEntryModuleName(right)) ||
       !same_optional_text(BundleEntryFunctionName(left), BundleEntryFunctionName(right)) ||
       !same_optional_text(left->entry->args_text, right->entry->args_text) ||
       !same_optional_text(left->entry->return_type, right->entry->return_type))
        return 0;
    for(size_t i = 0; i < left->capability_count; i++)
        if(!same_capability_abi(left, i, right))
            return 0;
    for(size_t i = 0; i < right->capability_count; i++)
        if(!same_capability_abi(right, i, left))
            return 0;
    return 1;
}

typedef struct BindingSet {
    const HostBinding *bindings;
    size_t count;
} BindingSet;

struct BundleInstance {
    BindingSet set;
    HostBinding *owned_bindings;
#ifdef ZIRAN_COMPILED_ZX
    ZxNative *native;
#endif
    VmInstance *vm;
    /* Present when the host selected its installed wasm provider. Provider
     * rejection is an instantiation error; it never selects the VM. */
    ZxWasm *wasm;
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

static BundleInstance *
instantiate_bundle(const Bundle *bundle, const HostBinding *bindings,
                   size_t binding_count, int max_steps, int portable_only)
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
    if(max_steps > 0) {
#ifdef ZIRAN_COMPILED_ZX
        if(!check_unchecked((Bundle *)bundle)) {
            BundleInstanceClose(instance);
            return NULL;
        }
#endif
        instance->vm = VmInstanceOpenVerifiedLimited(bundle->program, bundle->entry_owner,
            bundle->entry, dispatch, &instance->set, max_steps);
        if(instance->vm == NULL) {
            BundleInstanceClose(instance);
            return NULL;
        }
        return instance;
    }
    if(portable_only) {
#ifdef ZIRAN_COMPILED_ZX
        if(!check_unchecked((Bundle *)bundle)) {
            BundleInstanceClose(instance);
            return NULL;
        }
#endif
        instance->vm = VmInstanceOpenVerified(bundle->program, bundle->entry_owner,
                                              bundle->entry, dispatch, &instance->set);
        if(instance->vm == NULL) {
            BundleInstanceClose(instance);
            return NULL;
        }
        return instance;
    }
#ifdef ZIRAN_COMPILED_ZX
    unsigned char identity[32];
    bundle_identity(bundle, identity);
    ZirFunction entry;
    int unchecked = unchecked_entry(bundle, &entry);
#endif
#ifdef ZIRAN_NATIVE_ZX
    /* Native platforms run bundles only as native code; a bundle that cannot
     * be compiled is an error, never a slower fallback. An unchecked bundle
     * runs its cached code, or is checked and compiled when that code is
     * gone. Only an explicit portable VM request runs the VM here. */
    if(!portable_vm) {
        int missing = 1;
        native_identity(bundle, identity);
        if(unchecked)
            instance->native = ZxNativeOpenCached(&entry, identity, bundle->capabilities,
                                                  bundle->capability_count, dispatch,
                                                  &instance->set, &missing);
        if(instance->native == NULL && missing && check_unchecked((Bundle *)bundle))
            instance->native = ZxNativeOpen(bundle->program, bundle->entry_owner, bundle->entry,
                                            identity, bundle->capabilities,
                                            bundle->capability_count, dispatch, &instance->set,
                                            bundle->native_assembly);
        if(instance->native == NULL) {
            BundleInstanceClose(instance);
            return NULL;
        }
        return instance;
    }
#elif defined(ZIRAN_LINKED_ZX)
    /* A bundle whose code was linked into this host runs as that code. While
     * another instance holds it, or for any other bundle, the portable VM
     * runs the checked program. */
    int busy = 0;
    if(!portable_vm && ZxNativeLinked(identity)) {
        instance->native = ZxNativeOpenLinked(unchecked ? &entry : bundle->entry, identity,
                                              bundle->capabilities, bundle->capability_count,
                                              dispatch, &instance->set, &busy);
        if(instance->native != NULL)
            return instance;
        if(!busy || !check_unchecked((Bundle *)bundle)) {
            BundleInstanceClose(instance);
            return NULL;
        }
    }
#endif
#ifdef ZIRAN_COMPILED_ZX
    /* The VM runs the checked program. */
    if(!check_unchecked((Bundle *)bundle)) {
        BundleInstanceClose(instance);
        return NULL;
    }
#endif
    /* A host that installs the wasm provider selects that engine. Lowering
     * or provider rejection is an error and must not start another engine.
     * Hosts configured without the provider use their VM mode below. */
    if(ZxWasmPlatformActive() != NULL) {
        instance->wasm = ZxWasmOpen(bundle->program, bundle->entry_owner,
                                    bundle->entry, bundle->capabilities,
                                    bundle->capability_count, dispatch,
                                    &instance->set);
        if(instance->wasm == NULL) {
            BundleInstanceClose(instance);
            return NULL;
        }
        return instance;
    }
    instance->vm = VmInstanceOpenVerified(bundle->program, bundle->entry_owner,
                                           bundle->entry, dispatch, &instance->set);
    if(instance->vm == NULL) {
        BundleInstanceClose(instance);
        return NULL;
    }
    return instance;
}

BundleInstance *
BundleInstantiate(const Bundle *bundle, const HostBinding *bindings, size_t binding_count)
{
    return instantiate_bundle(bundle, bindings, binding_count, 0, 0);
}

BundleInstance *
BundleInstantiateLimited(const Bundle *bundle, const HostBinding *bindings,
                          size_t binding_count, int max_steps)
{
    return max_steps > 0 ? instantiate_bundle(bundle, bindings, binding_count, max_steps, 0) : NULL;
}

BundleInstance *
BundleInstantiatePortable(const Bundle *bundle, const HostBinding *bindings,
                          size_t binding_count)
{
    return instantiate_bundle(bundle, bindings, binding_count, 0, 1);
}

int
BundleInstanceRun(BundleInstance *instance, long long *result,
                  int *has_result)
{
    if(instance == NULL)
        return 0;
#ifdef ZIRAN_COMPILED_ZX
    if(instance->native != NULL)
        return ZxNativeRun(instance->native, result, has_result);
#endif
    if(instance->wasm != NULL)
        return ZxWasmRun(instance->wasm, result, has_result);
    return VmInstanceRun(instance->vm, result, has_result);
}

void
BundleInstanceLimitSteps(BundleInstance *instance, int max_steps)
{
    if(instance == NULL)
        return;
#ifdef ZIRAN_COMPILED_ZX
    if(instance->native != NULL) {
        ZxNativeLimitSteps(instance->native, max_steps);
        return;
    }
#endif
    if(instance->wasm != NULL) {
        ZxWasmLimitSteps(instance->wasm, max_steps);
        return;
    }
    VmInstanceLimitSteps(instance->vm, max_steps);
}

void
BundleInstanceMatchFieldsByName(BundleInstance *instance, int enabled)
{
    if(instance == NULL)
        return;
#ifdef ZIRAN_COMPILED_ZX
    if(instance->native != NULL) {
        ZxNativeMatchFieldsByName(instance->native, enabled);
        return;
    }
#endif
    if(instance->wasm != NULL) {
        ZxWasmMatchFieldsByName(instance->wasm, enabled);
        return;
    }
    VmInstanceMatchFieldsByName(instance->vm, enabled);
}

void
BundleInstanceClose(BundleInstance *instance)
{
    if(instance == NULL)
        return;
#ifdef ZIRAN_COMPILED_ZX
    ZxNativeClose(instance->native);
#endif
    ZxWasmClose(instance->wasm);
    VmInstanceClose(instance->vm);
    free(instance->owned_bindings);
    free(instance);
}

struct BundleState {
    BundleStateValue *values;
    size_t count;
    size_t bytes;
};

enum {
    BUNDLE_STATE_MAX_BYTES = 16 * 1024 * 1024
};

typedef struct StateBudget {
    size_t used;
    int failed;
} StateBudget;

static void
state_budget_add(StateBudget *budget, size_t bytes)
{
    if(budget->used > BUNDLE_STATE_MAX_BYTES ||
       bytes > (size_t)BUNDLE_STATE_MAX_BYTES - budget->used)
        budget->failed = 1;
    else
        budget->used += bytes;
}

static void
release_borrowed_host_value(VmHostValue *value, int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return;
    if(value->kind == VM_HOST_ARRAY || value->kind == VM_HOST_SLICE) {
        for(size_t i = 0; i < value->length && value->elements != NULL; i++)
            release_borrowed_host_value(&value->elements[i], depth + 1);
        free(value->elements);
        value->elements = NULL;
        return;
    }
    if(value->kind != VM_HOST_RECORD || value->fields == NULL)
        return;
    VmHostField *fields = (VmHostField *)value->fields;
    for(size_t i = 0; i < value->field_count; i++)
        release_borrowed_host_value(&fields[i].value, depth + 1);
    free(fields);
    value->fields = NULL;
}

static void
release_owned_state_value(VmHostValue *value, int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return;
    if(value->kind == VM_HOST_ARRAY) {
        for(size_t i = 0; i < value->length && value->elements != NULL; i++)
            release_owned_state_value(&value->elements[i], depth + 1);
        free(value->elements);
        value->elements = NULL;
    } else if(value->kind == VM_HOST_RECORD && value->fields != NULL) {
        VmHostField *fields = (VmHostField *)value->fields;
        for(size_t i = 0; i < value->field_count; i++)
            release_owned_state_value(&fields[i].value, depth + 1);
        for(size_t i = 0; i < value->field_count; i++)
            free((void *)fields[i].name);
        free(fields);
        value->fields = NULL;
    } else if(value->kind == VM_HOST_STRING)
        free((void *)value->data);
    value->data = NULL;
    free((void *)value->type);
    value->type = NULL;
}

static int
state_type_portable(const char *type)
{
    return type != NULL && type[0] != '*' && !SliceElementType(type, NULL, 0);
}

static const char *
clone_state_name(const char *name, StateBudget *budget)
{
    if(name == NULL)
        return NULL;
    size_t length = strlen(name);
    if(length == SIZE_MAX)
        return NULL;
    if(budget != NULL) {
        state_budget_add(budget, length + 1);
        if(budget->failed)
            return NULL;
    }
    char *copied = malloc(length + 1);
    if(copied == NULL)
        return NULL;
    memcpy(copied, name, length + 1);
    return copied;
}

static int
state_value_portable(const VmHostValue *value, int depth)
{
    if(depth >= VM_MAX_DEPTH || !state_type_portable(value->type) ||
       value->kind == VM_HOST_POINTER || value->kind == VM_HOST_SLICE ||
       (value->kind == VM_HOST_STRING && value->data == NULL &&
        value->length != 0))
        return 0;
    if(value->kind == VM_HOST_ARRAY) {
        if(value->length != 0 && value->elements == NULL)
            return 0;
        for(size_t i = 0; i < value->length; i++)
            if(!state_value_portable(&value->elements[i], depth + 1))
                return 0;
        return 1;
    }
    if(value->kind == VM_HOST_RECORD) {
        if(value->field_count != 0 && value->fields == NULL)
            return 0;
        for(size_t i = 0; i < value->field_count; i++)
            if(!state_value_portable(&value->fields[i].value, depth + 1))
                return 0;
    }
    return 1;
}

static int
clone_state_value(const VmHostValue *input, VmHostValue *output,
                  int depth, StateBudget *budget)
{
    if(depth >= VM_MAX_DEPTH || !state_type_portable(input->type) ||
       input->kind == VM_HOST_POINTER || input->kind == VM_HOST_SLICE ||
       (input->kind == VM_HOST_STRING && input->data == NULL &&
        input->length != 0))
        return 0;
    *output = *input;
    output->type = clone_state_name(input->type, budget);
    output->data = NULL;
    output->elements = NULL;
    output->fields = NULL;
    if(output->type == NULL)
        return 0;
    if(input->kind == VM_HOST_STRING) {
        if(input->length == 0) {
            output->data = NULL;
            return 1;
        }
        if(input->length == SIZE_MAX)
            goto fail;
        state_budget_add(budget, input->length + 1);
        if(budget->failed)
            goto fail;
        unsigned char *data = malloc(input->length + 1);
        if(data == NULL)
            goto fail;
        memcpy(data, input->data, input->length);
        data[input->length] = 0;
        output->data = data;
        return 1;
    }
    if(input->kind == VM_HOST_ARRAY) {
        if(input->length == 0) {
            output->elements = NULL;
            return 1;
        }
        if(input->elements == NULL ||
           input->length > SIZE_MAX / sizeof(*output->elements))
            goto fail;
        state_budget_add(budget, input->length * sizeof(*output->elements));
        if(budget->failed)
            goto fail;
        output->elements = calloc(input->length, sizeof(*output->elements));
        if(output->elements == NULL)
            goto fail;
        for(size_t i = 0; i < input->length; i++) {
            if(!clone_state_value(&input->elements[i], &output->elements[i],
                                  depth + 1, budget)) {
                goto fail;
            }
        }
        return 1;
    }
    if(input->kind != VM_HOST_RECORD)
        return 1;
    if(input->field_count == 0) {
        output->fields = NULL;
        return 1;
    }
    if(input->fields == NULL || input->field_count > SIZE_MAX / sizeof(VmHostField))
        goto fail;
    state_budget_add(budget, input->field_count * sizeof(VmHostField));
    if(budget->failed)
        goto fail;
    output->fields = calloc(input->field_count, sizeof(VmHostField));
    if(output->fields == NULL)
        goto fail;
    for(size_t i = 0; i < input->field_count; i++) {
        const char *name = clone_state_name(input->fields[i].name, budget);
        if(name == NULL) {
            goto fail;
        }
        ((VmHostField *)output->fields)[i].name = name;
        if(!clone_state_value(&input->fields[i].value,
                              (VmHostValue *)&output->fields[i].value,
                              depth + 1, budget)) {
            goto fail;
        }
    }
    return 1;
fail:
    release_owned_state_value(output, depth);
    return 0;
}

static void
state_close(BundleState *state)
{
    if(state != NULL) {
        for(size_t i = 0; i < state->count; i++) {
            release_owned_state_value(&state->values[i].value, 0);
            free((void *)state->values[i].module);
            free((void *)state->values[i].name);
            free((void *)state->values[i].type);
        }
        free(state->values);
        free(state);
    }
}

static int
clone_state_metadata(BundleStateValue *value, StateBudget *budget)
{
    const char *module = clone_state_name(value->module, budget);
    const char *name = clone_state_name(value->name, budget);
    const char *type = clone_state_name(value->type, budget);
    if(module == NULL || name == NULL || type == NULL) {
        free((void *)module);
        free((void *)name);
        free((void *)type);
        release_owned_state_value(&value->value, 0);
        return 0;
    }
    value->module = module;
    value->name = name;
    value->type = type;
    return 1;
}

static uint16_t
little_u16(const unsigned char *bytes)
{
    return (uint16_t)bytes[0] | (uint16_t)bytes[1] << 8;
}

static uint32_t
little_u32(const unsigned char *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
           (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static uint64_t
little_u64(const unsigned char *bytes)
{
    return (uint64_t)little_u32(bytes) |
           (uint64_t)little_u32(bytes + 4) << 32;
}

static void
put_little_u32(unsigned char *bytes, uint32_t value)
{
    for(size_t i = 0; i < 4; i++)
        bytes[i] = (unsigned char)(value >> (8 * i));
}

static void
put_little_u64(unsigned char *bytes, uint64_t value)
{
    for(size_t i = 0; i < 8; i++)
        bytes[i] = (unsigned char)(value >> (8 * i));
}

static const ZirModule *
program_module(const ZirProgram *program, const char *name)
{
    if(program == NULL || name == NULL)
        return NULL;
    for(int i = 0; i < program->module_count; i++)
        if(strcmp(program->modules[i].name, name) == 0)
            return &program->modules[i];
    return NULL;
}

static const ZirGlobal *
module_global(const ZirModule *module, const char *name)
{
    if(module == NULL || name == NULL)
        return NULL;
    for(int i = 0; i < module->global_count; i++)
        if(strcmp(module->globals[i].name, name) == 0)
            return &module->globals[i];
    return NULL;
}

static void
release_raw_state_value(VmHostValue *value, int depth)
{
    if(value == NULL || depth >= VM_MAX_DEPTH)
        return;
    if(value->kind == VM_HOST_ARRAY) {
        for(size_t i = 0; i < value->length && value->elements != NULL; i++)
            release_raw_state_value(&value->elements[i], depth + 1);
        free(value->elements);
    } else if(value->kind == VM_HOST_RECORD && value->fields != NULL) {
        VmHostField *fields = (VmHostField *)value->fields;
        for(size_t i = 0; i < value->field_count; i++)
            release_raw_state_value(&fields[i].value, depth + 1);
        for(size_t i = 0; i < value->field_count; i++)
            free((void *)fields[i].name);
        free((void *)value->fields);
    } else if(value->kind == VM_HOST_STRING) {
        free((void *)value->data);
    }
    value->elements = NULL;
    value->fields = NULL;
    value->data = NULL;
    free((void *)value->type);
    value->type = NULL;
}

static int
wasm_read_scalar(const ZxWasm *wasm, size_t address, const char *type,
                 VmHostValue *value)
{
    const char *scalar = ScalarType(type);
    if(!strcmp(scalar, "string")) {
        unsigned char bytes[8];
        if(!ZxWasmReadState(wasm, address, bytes, sizeof(bytes)))
            return 0;
        uint64_t handle = little_u64(bytes);
        uint32_t data_address = (uint32_t)handle;
        uint32_t length = (uint32_t)(handle >> 32);
        value->kind = VM_HOST_STRING;
        value->length = length;
        if(length == 0)
            return 1;
        if(length > BUNDLE_STATE_MAX_BYTES ||
           data_address > SIZE_MAX - length)
            return 0;
        unsigned char *data = malloc(length);
        if(data == NULL ||
           !ZxWasmReadState(wasm, data_address, data, length)) {
            free(data);
            return 0;
        }
        value->data = data;
        return 1;
    }
    if(!strcmp(scalar, "float64") || !strcmp(scalar, "float32")) {
        unsigned char bytes[8] = {0};
        if(!ZxWasmReadState(wasm, address, bytes,
                            !strcmp(scalar, "float32") ? 4 : 8))
            return 0;
        value->kind = VM_HOST_REAL;
        if(!strcmp(scalar, "float32")) {
            uint32_t bits = little_u32(bytes);
            float number;
            memcpy(&number, &bits, sizeof(number));
            value->real = number;
        } else {
            uint64_t bits = little_u64(bytes);
            memcpy(&value->real, &bits, sizeof(value->real));
        }
        return 1;
    }
    unsigned char bytes[8] = {0};
    size_t width = 1;
    if(!strcmp(scalar, "s64") || !strcmp(scalar, "u64"))
        width = 8;
    else if(!strcmp(scalar, "s32") || !strcmp(scalar, "u32"))
        width = 4;
    else if(!strcmp(scalar, "s16") || !strcmp(scalar, "u16"))
        width = 2;
    if(!ZxWasmReadState(wasm, address, bytes, width))
        return 0;
    uint64_t number = width == 8 ? little_u64(bytes) : little_u32(bytes);
    if(type[0] == 'u') {
        value->kind = VM_HOST_UNSIGNED;
        value->bits = number;
        value->integer = (int64_t)number;
    } else {
        value->kind = VM_HOST_INTEGER;
        value->integer = (int64_t)number;
        value->bits = number;
    }
    return 1;
}

static int
wasm_export_state_value(const ZxWasm *wasm, const ZirModule *owner,
                        const char *type, size_t address,
                        VmHostValue *value, int depth)
{
    size_t size, alignment;
    if(depth >= VM_MAX_DEPTH || !TypeLayout(owner, type, &size, &alignment))
        return 0;
    value->type = clone_state_name(type, NULL);
    if(value->type == NULL)
        return 0;
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        size_t element_size, element_alignment;
        if(capacity < 0 || capacity > BUNDLE_STATE_MAX_BYTES ||
           !TypeLayout(owner, element, &element_size, &element_alignment))
            return 0;
        value->kind = VM_HOST_ARRAY;
        value->length = (size_t)capacity;
        if(capacity == 0)
            return 1;
        value->elements = calloc((size_t)capacity, sizeof(*value->elements));
        if(value->elements == NULL)
            return 0;
        for(int i = 0; i < capacity; i++)
            if(!wasm_export_state_value(wasm, owner, element,
                                        address + (size_t)i * element_size,
                                        &value->elements[i], depth + 1))
                return 0;
        return 1;
    }
    const ZirModule *record_owner = NULL;
    const ZirType *record = FindType(owner, type, &record_owner);
    if(record != NULL && !record->is_enum && !record->is_procedure_type &&
       !record->is_extern && !record->is_map && !record->is_record_template &&
       !record->is_type_instance && !record->is_union) {
        record_owner = record_owner ? record_owner : owner;
        value->kind = VM_HOST_RECORD;
        size_t cursor = 0;
        ZirTypeField field;
        int count = 0;
        while(TypeNextField(record, &cursor, &field) == 1) {
            if(++count > VM_MAX_FIELDS)
                return 0;
        }
        value->field_count = (size_t)count;
        if(count == 0)
            return 1;
        VmHostField *fields = calloc((size_t)count, sizeof(*fields));
        value->fields = fields;
        if(fields == NULL)
            return 0;
        cursor = 0;
        int index = 0;
        size_t field_offset = 0;
        while(TypeNextField(record, &cursor, &field) == 1) {
            size_t field_size, field_alignment;
            if(!TypeLayout(record_owner, field.type, &field_size,
                           &field_alignment))
                return 0;
            size_t pad = field_offset % field_alignment;
            if(pad != 0)
                field_offset += field_alignment - pad;
            fields[index].name = clone_state_name(field.name, NULL);
            if(fields[index].name == NULL)
                return 0;
            if(!wasm_export_state_value(wasm, record_owner, field.type,
                                        address + field_offset,
                                        &fields[index].value, depth + 1))
                return 0;
            field_offset += field_size;
            index++;
        }
        return 1;
    }
    return wasm_read_scalar(wasm, address, type, value);
}

static int
wasm_write_state_value(ZxWasm *wasm, const ZirModule *owner,
                       const char *type, size_t address,
                       const VmHostValue *value, int depth)
{
    size_t size, alignment;
    if(depth >= VM_MAX_DEPTH || value == NULL || value->type == NULL ||
       strcmp(value->type, type) != 0 ||
       !TypeLayout(owner, type, &size, &alignment))
        return 0;
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        size_t element_size, element_alignment;
        if(capacity < 0 || value->kind != VM_HOST_ARRAY ||
            value->length != (size_t)capacity ||
            (capacity != 0 && value->elements == NULL) ||
            !TypeLayout(owner, element, &element_size, &element_alignment))
            return 0;
        for(int i = 0; i < capacity; i++)
            if(!wasm_write_state_value(wasm, owner, element,
                                       address + (size_t)i * element_size,
                                       &value->elements[i], depth + 1))
                return 0;
        return 1;
    }
    const ZirModule *record_owner = NULL;
    const ZirType *record = FindType(owner, type, &record_owner);
    if(record != NULL && !record->is_enum && !record->is_procedure_type &&
       !record->is_extern && !record->is_map && !record->is_record_template &&
       !record->is_type_instance && !record->is_union) {
        record_owner = record_owner ? record_owner : owner;
        if(value->kind != VM_HOST_RECORD ||
           (value->field_count != 0 && value->fields == NULL))
            return 0;
        size_t cursor = 0, field_offset = 0;
        ZirTypeField field;
        while(TypeNextField(record, &cursor, &field) == 1) {
            size_t field_size, field_alignment;
            if(!TypeLayout(record_owner, field.type, &field_size,
                           &field_alignment))
                return 0;
            size_t pad = field_offset % field_alignment;
            if(pad != 0)
                field_offset += field_alignment - pad;
            const VmHostValue *found = NULL;
            for(size_t i = 0; i < value->field_count; i++)
                if(value->fields[i].name != NULL &&
                   strcmp(value->fields[i].name, field.name) == 0) {
                    found = &value->fields[i].value;
                    break;
                }
            if(found == NULL ||
               !wasm_write_state_value(wasm, record_owner, field.type,
                                       address + field_offset, found,
                                       depth + 1))
                return 0;
            field_offset += field_size;
        }
        return 1;
    }
    const char *scalar = ScalarType(type);
    if(!strcmp(scalar, "string")) {
        if(value->kind != VM_HOST_STRING ||
           (value->data == NULL && value->length != 0) ||
           value->length > UINT32_MAX)
            return 0;
        size_t data = ZxWasmAllocateState(wasm, value->length);
        if(data == 0 || data > UINT32_MAX)
            return 0;
        if(value->length != 0 &&
           !ZxWasmWriteState(wasm, data, value->data, value->length))
            return 0;
        unsigned char bytes[8];
        put_little_u64(bytes, ((uint64_t)value->length << 32) | (uint32_t)data);
        return ZxWasmWriteState(wasm, address, bytes, sizeof(bytes));
    }
    unsigned char bytes[8] = {0};
    size_t width = 1;
    if(!strcmp(scalar, "float64"))
        width = 8;
    else if(!strcmp(scalar, "float32") || !strcmp(scalar, "s32") ||
            !strcmp(scalar, "u32"))
        width = 4;
    else if(!strcmp(scalar, "s16") || !strcmp(scalar, "u16"))
        width = 2;
    else if(!strcmp(scalar, "s64") || !strcmp(scalar, "u64"))
        width = 8;
    if(!strcmp(scalar, "float32")) {
        float number = (float)value->real;
        uint32_t bits;
        memcpy(&bits, &number, sizeof(bits));
        put_little_u32(bytes, bits);
    } else if(!strcmp(scalar, "float64")) {
        uint64_t bits;
        memcpy(&bits, &value->real, sizeof(bits));
        put_little_u64(bytes, bits);
    } else {
        if(value->kind != VM_HOST_INTEGER && value->kind != VM_HOST_UNSIGNED)
            return 0;
        uint64_t number = value->kind == VM_HOST_UNSIGNED ?
            value->bits : (uint64_t)value->integer;
        put_little_u64(bytes, number);
    }
    return ZxWasmWriteState(wasm, address, bytes, width);
}

int
BundleInstanceExportState(BundleInstance *instance, BundleState **state)
{
    if(state == NULL)
        return 0;
    *state = NULL;
    if(instance != NULL && instance->wasm != NULL) {
        const ZirProgram *program = ZxWasmProgram(instance->wasm);
        size_t count = ZxWasmStateCount(instance->wasm);
        BundleState *exported = calloc(1, sizeof(*exported));
        if(exported == NULL)
            return 0;
        exported->values = calloc(count ? count : 1,
                                  sizeof(*exported->values));
        if(exported->values == NULL) {
            free(exported);
            return 0;
        }
        StateBudget budget = {0};
        for(size_t i = 0; i < count; i++) {
            const char *module_name, *global_name, *type_name;
            size_t address, size;
            if(!ZxWasmStateAt(instance->wasm, i, &module_name, &global_name,
                              &type_name, &address, &size))
                goto fail;
            const ZirModule *owner = program_module(program, module_name);
            const ZirGlobal *declaration = module_global(owner, global_name);
            if(owner == NULL || declaration == NULL || size == 0 ||
               !state_type_portable(type_name)) {
                Diagnostic(Span("<host>", 1, 1), "zx.state",
                           "component state %s:%s is not owned portable state",
                           module_name, global_name);
                goto fail;
            }
            BundleStateValue value = {
                .module = module_name,
                .name = global_name,
                .type = type_name
            };
            VmHostValue borrowed = {0};
            int raw = wasm_export_state_value(instance->wasm, owner,
                                              type_name, address, &borrowed, 0);
            int portable = raw && state_value_portable(&borrowed, 0);
            int cloned = portable &&
                clone_state_value(&borrowed, &value.value, 0, &budget);
            if(!cloned) {
                Diagnostic(declaration->span, "zx.state",
                           "component state %s:%s exceeds portable state limits",
                           module_name, global_name);
                release_raw_state_value(&borrowed, 0);
                goto fail;
            }
            release_raw_state_value(&borrowed, 0);
            if(!clone_state_metadata(&value, &budget))
                goto fail;
            exported->values[exported->count++] = value;
        }
        exported->bytes = budget.used;
        *state = exported;
        return 1;
fail:
        state_close(exported);
        return 0;
    }
    if(instance == NULL || instance->vm == NULL)
        return 0;
    Vm *vm = &instance->vm->vm;
    BundleState *exported = calloc(1, sizeof(*exported));
    if(exported == NULL)
        return 0;
    exported->values = calloc(vm->global_count ? (size_t)vm->global_count : 1,
                              sizeof(*exported->values));
    if(exported->values == NULL) {
        free(exported);
        return 0;
    }
    StateBudget budget = {0};
    for(int i = 0; i < vm->global_count; i++) {
        GlobalSlot *slot = &vm->globals[i];
        VmHostValue borrowed = {0};
        if(!VmValueHostArgument(slot->module, slot->type, slot->value,
                                &borrowed, 0) ||
           !state_type_portable(slot->type) ||
           borrowed.kind == VM_HOST_POINTER ||
           borrowed.kind == VM_HOST_SLICE) {
            Diagnostic(slot->declaration->span, "zx.state",
                       "component state %s:%s is not owned portable state",
                       slot->module->name, slot->declaration->name);
            release_borrowed_host_value(&borrowed, 0);
            state_close(exported);
            return 0;
        }
        BundleStateValue value = {
            .module = slot->module->name,
            .name = slot->declaration->name,
            .type = slot->type,
            .value = {0}
        };
        if(!clone_state_value(&borrowed, &value.value, 0, &budget)) {
            Diagnostic(slot->declaration->span, "zx.state",
                       "component state %s:%s exceeds portable state limits",
                       slot->module->name, slot->declaration->name);
            release_borrowed_host_value(&borrowed, 0);
            state_close(exported);
            return 0;
        }
        release_borrowed_host_value(&borrowed, 0);
        if(!clone_state_metadata(&value, &budget)) {
            state_close(exported);
            return 0;
        }
        exported->values[exported->count++] = value;
    }
    exported->bytes = budget.used;
    *state = exported;
    return 1;
}

int
BundleInstanceImportState(BundleInstance *instance, const BundleState *state)
{
    if(instance != NULL && instance->wasm != NULL && state != NULL) {
        const ZirProgram *program = ZxWasmProgram(instance->wasm);
        size_t count = ZxWasmStateCount(instance->wasm);
        for(size_t i = 0; i < count; i++) {
            const char *module_name, *global_name, *type_name;
            size_t address, size;
            const BundleStateValue *found = NULL;
            if(!ZxWasmStateAt(instance->wasm, i, &module_name, &global_name,
                              &type_name, &address, &size))
                return 0;
            for(size_t value = 0; value < state->count; value++)
                if(strcmp(state->values[value].module, module_name) == 0 &&
                   strcmp(state->values[value].name, global_name) == 0) {
                    found = &state->values[value];
                    break;
                }
            const ZirModule *owner = program_module(program, module_name);
            const ZirGlobal *declaration = module_global(owner, global_name);
            if(found == NULL || declaration == NULL ||
               strcmp(found->type, type_name) != 0 || size == 0 ||
               !state_type_portable(type_name) ||
               !state_value_portable(&found->value, 0) ||
               !wasm_write_state_value(instance->wasm, owner, type_name,
                                       address, &found->value, 0)) {
                Diagnostic(declaration ? declaration->span : Span("<host>", 1, 1),
                           "zx.state",
                           "component state %s:%s is incompatible with replacement",
                           module_name, global_name);
                return 0;
            }
        }
        return 1;
    }
    if(instance == NULL || instance->vm == NULL || state == NULL)
        return 0;
    Vm *vm = &instance->vm->vm;
    Value *imported = state->count <= SIZE_MAX / sizeof(*imported) ?
        calloc(state->count ? state->count : 1, sizeof(*imported)) : NULL;
    size_t *positions = state->count <= SIZE_MAX / sizeof(*positions) ?
        calloc(state->count ? state->count : 1, sizeof(*positions)) : NULL;
    if(imported == NULL || positions == NULL) {
        free(imported);
        free(positions);
        return 0;
    }
    for(size_t i = 0; i < state->count; i++) {
        const BundleStateValue *value = &state->values[i];
        GlobalSlot *slot = NULL;
        for(int g = 0; g < vm->global_count; g++) {
            GlobalSlot *candidate = &vm->globals[g];
            if(strcmp(candidate->module->name, value->module) == 0 &&
               strcmp(candidate->declaration->name, value->name) == 0) {
                slot = candidate;
                break;
            }
        }
        if(slot == NULL || strcmp(slot->type, value->type) != 0 ||
           !state_type_portable(slot->type) ||
           !state_value_portable(&value->value, 0)) {
            Diagnostic(Span("<host>", 1, 1), "zx.state",
                       slot == NULL ?
                       "component state %s:%s (%s/%d) is absent from the replacement" :
                       "component state %s:%s (%s/%d) is incompatible with replacement %s/%d",
                       value->module, value->name, value->type,
                       (int)value->value.kind,
                       slot != NULL ? slot->type : "",
                       slot != NULL ? (int)slot->value.kind : 0);
            free(imported);
            free(positions);
            return 0;
        }
        positions[i] = (size_t)(slot - vm->globals);
        imported[i] = VmValueHostReturn(vm, slot->module, slot->type,
                                         &value->value, 0);
        if(vm->failed) {
            Diagnostic(Span("<host>", 1, 1), "zx.state",
                       "component state %s:%s is incompatible with the replacement",
                       value->module, value->name);
            vm->failed = 0;
            free(imported);
            free(positions);
            return 0;
        }
    }
    for(size_t i = 0; i < state->count; i++)
        vm->globals[positions[i]].value = imported[i];
    free(imported);
    free(positions);
    return 1;
}

size_t
BundleStateCount(const BundleState *state)
{
    return state != NULL ? state->count : 0;
}

size_t
BundleStateValueBytes(const BundleState *state)
{
    return state != NULL ? state->bytes : 0;
}

int
BundleStateValueAt(const BundleState *state, size_t index,
                   BundleStateValue *value)
{
    if(state == NULL || index >= state->count || value == NULL)
        return 0;
    *value = state->values[index];
    return 1;
}

BundleStateValue *
BundleStateValueRef(BundleState *state, size_t index)
{
    return state != NULL && index < state->count ? &state->values[index] : NULL;
}

int
BundleStateFind(const BundleState *state, const char *module,
                const char *name, size_t *index)
{
    if(state == NULL || module == NULL || name == NULL || index == NULL)
        return 0;
    for(size_t i = 0; i < state->count; i++)
        if(strcmp(state->values[i].module, module) == 0 &&
           strcmp(state->values[i].name, name) == 0) {
            *index = i;
            return 1;
        }
    return 0;
}

int
BundleStateSetInteger(BundleState *state, size_t index, int64_t value)
{
    if(state == NULL || index >= state->count ||
       state->values[index].value.kind != VM_HOST_INTEGER)
        return 0;
    state->values[index].value.integer = value;
    state->values[index].value.bits = (uint64_t)value;
    return 1;
}

int
BundleStateSetUnsigned(BundleState *state, size_t index, uint64_t value)
{
    if(state == NULL || index >= state->count ||
       state->values[index].value.kind != VM_HOST_UNSIGNED)
        return 0;
    state->values[index].value.bits = value;
    state->values[index].value.integer = (int64_t)value;
    return 1;
}

int
BundleStateSetReal(BundleState *state, size_t index, double value)
{
    if(state == NULL || index >= state->count ||
       state->values[index].value.kind != VM_HOST_REAL)
        return 0;
    state->values[index].value.real = value;
    return 1;
}

int
BundleStateSetString(BundleState *state, size_t index,
                     const unsigned char *data, size_t length)
{
    if(state == NULL || index >= state->count ||
       state->values[index].value.kind != VM_HOST_STRING ||
       (data == NULL && length != 0) || length == SIZE_MAX)
        return 0;
    unsigned char *copied = NULL;
    if(length != 0) {
        copied = malloc(length + 1);
        if(copied == NULL)
            return 0;
        memcpy(copied, data, length);
        copied[length] = 0;
    }
    free((void *)state->values[index].value.data);
    state->values[index].value.data = copied;
    state->values[index].value.length = length;
    return 1;
}

void
BundleStateClose(BundleState *state)
{
    state_close(state);
}

struct BundleComponent {
    Bundle *bundle;
    BundleInstance *instance;
    int max_steps;
    uint64_t generation;
    int closed;
    size_t references;
};

struct BundleComponentHandle {
    BundleComponent *component;
    uint64_t generation;
};

static void
component_release(BundleComponent *component)
{
    if(component != NULL && --component->references == 0)
        free(component);
}

static size_t
component_live_value_bytes(const BundleComponent *component)
{
    if(component == NULL || component->closed || component->instance == NULL)
        return 0;
    Vm *vm = &component->instance->vm->vm;
    if(vm->global_count < 0 ||
       (size_t)vm->global_count > SIZE_MAX / sizeof(Value))
        return 0;
    size_t bytes = (size_t)vm->global_count * sizeof(Value);
    size_t allocated = vm->record_bytes;
    if(allocated > SIZE_MAX - vm->array_bytes)
        return SIZE_MAX;
    allocated += vm->array_bytes;
    if(allocated > SIZE_MAX - vm->string_bytes)
        return SIZE_MAX;
    allocated += vm->string_bytes;
    return allocated <= SIZE_MAX - bytes ? bytes + allocated : SIZE_MAX;
}

BundleComponent *
BundleComponentOpen(const Bundle *bundle, const HostBinding *bindings,
                    size_t binding_count, int max_steps)
{
    if(bundle == NULL || max_steps <= 0)
        return NULL;
    Bundle *retained = (Bundle *)bundle;
    if(retained->references == SIZE_MAX)
        return NULL;
    retained->references++;
    BundleComponent *component = calloc(1, sizeof(*component));
    if(component == NULL) {
        BundleClose(retained);
        return NULL;
    }
    component->bundle = retained;
    component->references = 1;
    component->max_steps = max_steps;
    component->instance = instantiate_bundle(retained, bindings, binding_count,
                                              max_steps, 1);
    if(component->instance == NULL) {
        BundleClose(retained);
        free(component);
        return NULL;
    }
    return component;
}

BundleComponentHandle *
BundleComponentAcquire(BundleComponent *component)
{
    if(component == NULL || component->closed ||
       component->references == SIZE_MAX)
        return NULL;
    component->references++;
    BundleComponentHandle *handle = malloc(sizeof(*handle));
    if(handle == NULL) {
        component_release(component);
        return NULL;
    }
    *handle = (BundleComponentHandle){component, component->generation};
    return handle;
}

int
BundleComponentHandleValid(const BundleComponentHandle *handle)
{
    return handle != NULL && handle->component != NULL &&
           !handle->component->closed &&
           handle->generation == handle->component->generation;
}

int
BundleComponentRun(BundleComponentHandle *handle, long long *result,
                   int *has_result)
{
    return BundleComponentHandleValid(handle) &&
           BundleInstanceRun(handle->component->instance, result, has_result);
}

int
BundleComponentReplace(BundleComponent *component, const Bundle *replacement,
                       BundleStateMigrate migrate, BundleComponentVerify verify,
                       void *context, BundleComponentHandle **new_handle)
{
    if(component == NULL || component->closed || replacement == NULL ||
       !BundleAbiCompatible(component->bundle, replacement))
        return 0;
    if(new_handle != NULL)
        *new_handle = NULL;
    Bundle *retained = (Bundle *)replacement;
    if(retained->references == SIZE_MAX)
        return 0;
    BundleComponentHandle *handle = NULL;
    if(new_handle != NULL) {
        handle = malloc(sizeof(*handle));
        if(handle == NULL)
            return 0;
    }
    retained->references++;
    BundleState *old_state = NULL, *new_state = NULL;
    BundleInstance *candidate = instantiate_bundle(
        retained, component->instance->set.bindings,
        component->instance->set.count, component->max_steps, 1);
    int ok = candidate != NULL &&
             BundleInstanceExportState(component->instance, &old_state) &&
             BundleInstanceExportState(candidate, &new_state) &&
             (migrate == NULL || migrate(context, old_state, new_state)) &&
             BundleInstanceImportState(candidate, new_state) &&
             (verify == NULL || verify(context, candidate));
    if(ok) {
        BundleInstanceClose(component->instance);
        BundleClose(component->bundle);
        component->bundle = retained;
        component->instance = candidate;
        component->generation++;
        if(new_handle != NULL) {
            component->references++;
            *handle = (BundleComponentHandle){component, component->generation};
            *new_handle = handle;
            handle = NULL;
        }
    } else {
        BundleInstanceClose(candidate);
        BundleClose(retained);
    }
    BundleStateClose(old_state);
    BundleStateClose(new_state);
    free(handle);
    return ok;
}

size_t
BundleComponentLiveValueBytes(const BundleComponent *component)
{
    return component_live_value_bytes(component);
}

void
BundleComponentClose(BundleComponent *component)
{
    if(component == NULL || component->closed)
        return;
    component->closed = 1;
    BundleInstanceClose(component->instance);
    component->instance = NULL;
    BundleClose(component->bundle);
    component->bundle = NULL;
    component_release(component);
}

void
BundleComponentHandleClose(BundleComponentHandle *handle)
{
    if(handle != NULL) {
        component_release(handle->component);
        free(handle);
    }
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

static const Law *
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
    const Law *law = law_at(bundle, index);
    return law != NULL ? law->module.data : NULL;
}

const char *
BundleLawName(const Bundle *bundle, size_t index)
{
    const Law *law = law_at(bundle, index);
    return law != NULL ? law->name.data : NULL;
}

const char *
BundleLawStatus(const Bundle *bundle, size_t index)
{
    const Law *law = law_at(bundle, index);
    return law != NULL ? law->status.data : NULL;
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

static const Waiver *
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
    const Waiver *waiver = waiver_at(bundle, index);
    return waiver != NULL ? waiver->name.data : NULL;
}

const char *
BundleLawWaiverReason(const Bundle *bundle, size_t index)
{
    const Waiver *waiver = waiver_at(bundle, index);
    return waiver != NULL ? waiver->reason.data : NULL;
}
