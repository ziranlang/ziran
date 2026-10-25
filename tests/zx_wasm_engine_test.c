/* The wasm execution tier's seam: with no provider installed every bundle
 * runs on the portable VM; with a provider, a scalar bundle's lowered
 * module reaches the provider and its result comes back through
 * BundleInstanceRun, while an out-of-subset bundle falls back to the VM.
 * The provider here is a recording mock, so the test runs anywhere
 * libziran builds; real providers execute the module under WebAssembly. */
#include "ziran_host.h"
#include "zir.h"
#include "zir_bundle.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_load.h"
#include "zir_serial.h"
#include "zir_vm.h"
#include "zir_zx_wasm_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned long mock_handle;
static int instantiate_calls;
static int release_calls;
static long long set_budget_value = -1;
static int set_budget_calls;
static int call_calls;

static unsigned long
mock_instantiate(void *context, const unsigned char *bytes, size_t size)
{
    (void)context;
    instantiate_calls++;
    if(bytes == NULL || size < 8 || memcmp(bytes, "\0asm", 4) != 0 ||
       memcmp(bytes + 4, "\1\0\0\0", 4) != 0) {
        fprintf(stderr, "provider received %zu bytes without wasm magic\n", size);
        exit(1);
    }
    mock_handle = 0x1000 + (unsigned long)size;
    return mock_handle;
}

static int
mock_set_budget(void *context, unsigned long handle, long long budget)
{
    (void)context;
    set_budget_calls++;
    if(handle != mock_handle)
        exit(1);
    set_budget_value = budget;
    return 0;
}

static int
mock_call(void *context, unsigned long handle, long long *result)
{
    (void)context;
    call_calls++;
    if(handle != mock_handle)
        exit(1);
    *result = 4242;
    return 0;
}

static void
mock_release(void *context, unsigned long handle)
{
    (void)context;
    release_calls++;
    if(handle != mock_handle)
        exit(1);
}

/* Bundles one source through the library's own pipeline, exactly as the
 * zi2zx tool does, and opens the bytes in memory. */
static Bundle *
bundle_of(const char *name, const char *text)
{
    char directory[] = "/tmp/zx-wasm-engine.XXXXXX";
    if(mkdtemp(directory) == NULL)
        exit(2);
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE *source = fopen(path, "w");
    if(source == NULL)
        exit(2);
    fputs(text, source);
    fclose(source);
    char stem[256];
    snprintf(stem, sizeof(stem), "%s", name);
    stem[strlen(stem) - 3] = '\0';

    CompilerContext *context = CompilerContextOpen();
    if(context == NULL)
        exit(2);
    CompilerContext *previous = CompilerContextSelect(context);
    ProgramSet set = {0};
    const char *files[] = {path};
    Bundle *bundle = NULL;
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    if(ProgramsLoadWithDefines(&set, directory, NULL, 0, NULL, 0, files, 1) &&
       CheckCanonicalPrograms(set.programs, set.count,
                              (const char *const *)set.paths)) {
        merged.context = set.context;
        merged.module_count = 0;
        for(int i = 0; i < set.count; i++)
            merged.module_count += set.programs[i]->module_count;
        merged.modules = calloc((size_t)merged.module_count, sizeof(*merged.modules));
        if(merged.modules != NULL) {
            int position = 0;
            for(int i = 0; i < set.count; i++)
                for(int m = 0; m < set.programs[i]->module_count; m++)
                    merged.modules[position++] = set.programs[i]->modules[m];
            ZirProgram *merged_pointer = &merged;
            char entry_module[ZIR_NAME_MAX], entry_function[ZIR_NAME_MAX];
            snprintf(entry_module, sizeof(entry_module), "%s", stem);
            snprintf(entry_function, sizeof(entry_function), "Answer");
            if(LinkImports(&merged_pointer, 1)) {
                linked = BundleLink(&merged, entry_module, entry_function);
                if(linked != NULL && CheckTargetCapabilities(linked, "zx") &&
                   VmVerify(linked, entry_module, entry_function)) {
                    char *bytes = NULL;
                    size_t size = 0;
                    FILE *image = open_memstream(&bytes, &size);
                    ZxAssets assets = {0};
                    if(image != NULL &&
                       BundleWriteInContext(image, linked, linked, entry_module,
                                            entry_function, &assets)) {
                        fclose(image);
                        bundle = BundleOpenBytes((const unsigned char *)bytes, size);
                        free(bytes);
                    } else {
                        if(image != NULL)
                            fclose(image);
                        free(bytes);
                    }
                }
            }
        }
    }
    ProgramsFree(&set);
    ProgramFree(linked);
    free(merged.modules);
    CompilerContextSelect(previous);
    CompilerContextClose(context);
    remove(path);
    rmdir(directory);
    return bundle;
}

static const char scalar_source[] =
    "#program_export\n"
    "Answer :: () -> s64 { return 21 * 2 }\n";

/* Strings lower to the tier now; enums stay outside it, so this one
 * exercises the VM fallback. */
static const char enum_source[] =
    "Direction :: enum {\n"
    "    Up\n"
    "    Down\n"
    "}\n"
    "#program_export\n"
    "Answer :: () -> s64 {\n"
    "    way: Direction = Direction.Down\n"
    "    return ifx way == Direction.Up then 1 else 2\n"
    "}\n";

int
main(void)
{
    /* Without a provider, a scalar bundle runs on the portable VM. */
    Bundle *scalar = bundle_of("scalar.zi", scalar_source);
    if(scalar == NULL)
        return 2;
    BundleInstance *instance = BundleInstantiate(scalar, NULL, 0);
    if(instance == NULL)
        return 2;
    long long result = 0;
    int has_result = 0;
    if(!BundleInstanceRun(instance, &result, &has_result) || !has_result ||
       result != 42) {
        fprintf(stderr, "VM run gave %lld\n", result);
        return 1;
    }
    BundleInstanceClose(instance);
    if(instantiate_calls != 0) {
        fprintf(stderr, "provider reached without installation\n");
        return 1;
    }

    /* With the mock provider, the scalar bundle lowers and its result
     * comes from the provider's call. */
    ZxWasmPlatform platform = {0};
    platform.instantiate = mock_instantiate;
    platform.set_budget = mock_set_budget;
    platform.call = mock_call;
    platform.release = mock_release;
    ZxWasmSetPlatform(&platform);
    instance = BundleInstantiate(scalar, NULL, 0);
    if(instance == NULL || instantiate_calls != 1) {
        fprintf(stderr, "scalar instantiate: instance=%p calls=%d\n",
                (void *)instance, instantiate_calls);
        return 1;
    }
    BundleInstanceLimitSteps(instance, 500);
    if(!BundleInstanceRun(instance, &result, &has_result) || !has_result ||
       result != 4242 || call_calls != 1 || set_budget_calls != 1 ||
       set_budget_value != 500) {
        fprintf(stderr, "provider run gave %lld (budget %lld)\n",
                result, set_budget_value);
        return 1;
    }
    BundleInstanceClose(instance);
    if(release_calls != 1)
        return 1;

    /* An enum bundle is outside the tier: it falls back to the VM and
     * still answers, with the provider left untouched for it. */
    Bundle *enums = bundle_of("enums.zi", enum_source);
    if(enums == NULL)
        return 2;
    instance = BundleInstantiate(enums, NULL, 0);
    if(instance == NULL || instantiate_calls != 1)
        return 1;
    if(!BundleInstanceRun(instance, &result, &has_result) || !has_result ||
       result != 2 || call_calls != 1) {
        fprintf(stderr, "enum fallback gave %lld\n", result);
        return 1;
    }
    BundleInstanceClose(instance);

    /* Removing the provider restores pure VM behavior. */
    ZxWasmSetPlatform(NULL);
    instance = BundleInstantiate(scalar, NULL, 0);
    if(instance == NULL || !BundleInstanceRun(instance, &result, &has_result) ||
       !has_result || result != 42 || instantiate_calls != 1)
        return 1;
    BundleInstanceClose(instance);
    BundleClose(enums);
    BundleClose(scalar);
    fprintf(stdout, "zx-wasm-engine-test: seam, provider handoff and fallback verified\n");
    return 0;
}
