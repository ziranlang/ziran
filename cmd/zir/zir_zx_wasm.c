/*
 * zir_zx_wasm.c - bundles executed as WebAssembly through the installed
 * provider. Mirrors the native engine's five-function contract so
 * zir_host.c picks between tiers at one seam.
 */
#include "zir_zx_wasm.h"
#include "zir_diagnostic.h"
#include "../zir-wasm/zir_wasm_lower.h"
#include "zir_zx_wasm_platform.h"

#include <stdlib.h>
#include <string.h>

struct ZxWasm {
    ZxWasmPlatform platform;
    unsigned long handle;
    long long budget;
    int failed;
};

static ZxWasmPlatform active_platform;
static int platform_installed;

void
ZxWasmSetPlatform(const ZxWasmPlatform *platform)
{
    if(platform == NULL) {
        platform_installed = 0;
        memset(&active_platform, 0, sizeof(active_platform));
        return;
    }
    active_platform = *platform;
    platform_installed = 1;
}

const ZxWasmPlatform *
ZxWasmPlatformActive(void)
{
    return platform_installed ? &active_platform : NULL;
}

ZxWasm *
ZxWasmOpen(const ZirProgram *program, const ZirModule *entry_owner,
           const ZirFunction *entry,
           const char *const *capability_modules,
           const char *const *capability_functions,
           size_t capability_count)
{
    (void)capability_modules;
    (void)capability_functions;
    if(!platform_installed)
        return NULL;
    if(capability_count != 0) {
        Diagnostic(entry->span, "zx.wasm",
                   "bundle needs %zu host bindings, which the wasm execution tier does not take yet",
                   capability_count);
        return NULL;
    }
    size_t size = 0;
    unsigned char *image = zir_wasm_lower(program, entry_owner->name,
                                          entry->name, &size);
    if(image == NULL)
        return NULL;
    ZxWasm *wasm = malloc(sizeof(*wasm));
    if(wasm == NULL) {
        DiagnosticOutOfMemory();
        free(image);
        return NULL;
    }
    wasm->platform = active_platform;
    wasm->budget = 0x7fffffffffffffffLL;
    wasm->failed = 0;
    wasm->handle = active_platform.instantiate(active_platform.context,
                                               image, size);
    free(image);
    if(wasm->handle == 0) {
        Diagnostic(entry->span, "zx.wasm",
                   "the wasm provider rejected the lowered module");
        free(wasm);
        return NULL;
    }
    return wasm;
}

int
ZxWasmRun(ZxWasm *wasm, long long *result, int *has_result)
{
    if(wasm == NULL || wasm->failed)
        return 0;
    long long value = 0;
    if(wasm->platform.set_budget(wasm->platform.context, wasm->handle,
                                 wasm->budget) != 0 ||
       wasm->platform.call(wasm->platform.context, wasm->handle,
                           &value) != 0) {
        Diagnostic((ZirSourceSpan){0}, "zx.wasm",
                   "the wasm instance trapped or failed");
        wasm->failed = 1;
        return 0;
    }
    if(result != NULL)
        *result = value;
    if(has_result != NULL)
        *has_result = 1;
    return 1;
}

void
ZxWasmLimitSteps(ZxWasm *wasm, int max_steps)
{
    if(wasm == NULL)
        return;
    /* The lowered module counts function entries and loop iterations
     * against this global, as the native tier does; zero is unbounded. */
    wasm->budget = max_steps > 0 ? max_steps : 0x7fffffffffffffffLL;
}

void
ZxWasmMatchFieldsByName(ZxWasm *wasm, int enabled)
{
    /* Record field matching needs the host-value bridge, which the scalar
     * tier does not cross; the flag is accepted and ignored. */
    (void)wasm;
    (void)enabled;
}

void
ZxWasmClose(ZxWasm *wasm)
{
    if(wasm == NULL)
        return;
    if(wasm->platform.release != NULL)
        wasm->platform.release(wasm->platform.context, wasm->handle);
    free(wasm);
}
