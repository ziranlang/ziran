#ifndef ZIR_ZX_WASM_PLATFORM_H
#define ZIR_ZX_WASM_PLATFORM_H
#include <stddef.h>
/*
 * zir_zx_wasm_platform - the WebAssembly provider seam of the wasm
 * execution tier. libziran lowers verified bundles to wasm32 modules in
 * plain C (cmd/zir-wasm/zir_wasm_lower.c); instantiating and calling the
 * module needs a host runtime, so the embedding application installs one
 * provider at startup. Web hosts implement it over the browser's
 * WebAssembly engine through their bridge; until a provider is installed
 * every bundle runs on the portable VM, exactly as before.
 */
typedef struct ZxWasmPlatform {
    void *context;
    /* Compile and instantiate a module image with the tier's imports,
     * (zx, overrun) -> () and the mutable (zx, budget) i64 global.
     * Returns a nonzero handle, or 0 when compilation fails. */
    unsigned long (*instantiate)(void *context, const unsigned char *bytes,
                                 size_t size);
    /* Point the instance's budget global at the run limit. A zero or
     * negative budget makes the next step trip the overrun import. */
    int (*set_budget)(void *context, unsigned long handle, long long budget);
    /* Call zx_entry and return the i64 result. Returns 0 on success, or
     * nonzero when the run trapped or failed; *result is then unused. */
    int (*call)(void *context, unsigned long handle, long long *result);
    /* Drop an instance created by instantiate. A NULL entry is allowed. */
    void (*release)(void *context, unsigned long handle);
} ZxWasmPlatform;

/* Installs the provider used by new bundle instances. Passing NULL
 * removes it; existing instances keep running. The structure is copied. */
void ZxWasmSetPlatform(const ZxWasmPlatform *platform);
const ZxWasmPlatform *ZxWasmPlatformActive(void);
#endif
