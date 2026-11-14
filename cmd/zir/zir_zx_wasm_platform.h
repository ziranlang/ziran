#ifndef ZIR_ZX_WASM_PLATFORM_H
#define ZIR_ZX_WASM_PLATFORM_H
#include <stddef.h>
/*
 * zir_zx_wasm_platform - the WebAssembly provider seam of the wasm
 * execution tier. libziran lowers bundles to wasm32 modules in plain C
 * (revision-28 bundles through cmd/zir-wasm/zx_wasm_lower.c, semantic
 * bundles through cmd/zir-wasm/zir_wasm_lower.c); instantiating and
 * calling the module needs a host runtime, so the embedding application
 * installs one provider at startup. Web hosts implement it over the
 * browser's WebAssembly engine through their bridge; until a provider is
 * installed every bundle runs on its interpreter, exactly as before.
 */
typedef struct ZxWasmPlatform {
    void *context;
    /* Compile and instantiate a module image with the tier's imports:
     * (zx, overrun) -> (), the mutable (zx, budget) i64 global, and
     * (zx, host_i64) and (zx, host), which call ZxWasmHostCallI64 and
     * ZxWasmHostCall with `context`. Returns a nonzero handle, or 0 when
     * compilation fails. */
    unsigned long (*instantiate)(void *context, const unsigned char *bytes,
                                 size_t size);
    /* Point the instance's budget global at the run limit. A zero or
     * negative budget makes the next step trip the overrun import. */
    int (*set_budget)(void *context, unsigned long handle, long long budget);
    /* Call zx_entry and return the i64 result. Returns 0 on success, or
     * nonzero when the run trapped or failed; *result is then unused. */
    int (*call)(void *context, unsigned long handle, long long *result);
    /* Copy instance linear-memory bytes in either direction. Both return 0
     * on success, or nonzero when the handle, address, or size is invalid.
     * The address is in the instantiated module's wasm32 memory. */
    int (*read)(void *context, unsigned long handle, size_t address,
                void *destination, size_t size);
    int (*write)(void *context, unsigned long handle, size_t address,
                 const void *source, size_t size);
    /* Reserve zero-filled storage in the instance and return its nonzero
     * wasm32 address, or zero when the instance cannot grow: the module's
     * zx_reserve export when it has one, else past its state arena. */
    size_t (*allocate_state)(void *context, unsigned long handle, size_t size);
    /* Optional: call zx_initialize when the module exports it. Returns 0
     * on success or when there is nothing to run. */
    int (*initialize)(void *context, unsigned long handle);
    /* Optional: the budget left after the last call. */
    int (*budget_left)(void *context, unsigned long handle, long long *budget);
    /* Drop an instance created by instantiate. A NULL entry is allowed. */
    void (*release)(void *context, unsigned long handle);
} ZxWasmPlatform;

/* Installs the provider used by new bundle instances. Passing NULL
 * removes it; existing instances keep running. The structure is copied. */
void ZxWasmSetPlatform(const ZxWasmPlatform *platform);
const ZxWasmPlatform *ZxWasmPlatformActive(void);
#endif
