/*
 * zir_zx_wasm_web.c - the WebAssembly execution tier's web provider.
 *
 * Compiled only by Emscripten builds of libziran: the provider reaches the
 * browser's WebAssembly engine through EM_JS, installs itself when the
 * module loads (unless ZIRAN_WASM_TIER=0), and memoizes compiled modules
 * per page session keyed by the module image, so reopening a bundle pays
 * instantiation but not compilation again. Hosts need no code changes:
 * rebuilding the wasm bundle runtime activates the tier, and bundles
 * outside its subset keep running on the portable VM.
 */
#include "zir_zx_wasm_platform.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <stddef.h>

/* Returns a nonzero instance handle, or 0 when the module is invalid. */
EM_JS(unsigned long, zx_wasm_web_instantiate, (const unsigned char *bytes, size_t size), {
    if(globalThis.zxWasmTier === undefined)
        globalThis.zxWasmTier = {modules: new Map(), instances: new Map(), next: 1};
    const tier = globalThis.zxWasmTier;
    const view = new Uint8Array(HEAPU8.buffer, bytes, size);
    /* The session key is the exact image text, so an identical lowered
     * program reuses its compiled module and a different one never can. */
    let key = '';
    for(let at = 0; at < view.length; at += 4096)
        key += String.fromCharCode.apply(null, view.subarray(at, Math.min(at + 4096, view.length)));
    let compiled;
    try {
        compiled = tier.modules.get(key);
        if(compiled === undefined) {
            compiled = new WebAssembly.Module(view.slice());
            tier.modules.set(key, compiled);
        }
        const budget = new WebAssembly.Global({value: 'i64', mutable: true}, 0n);
        const instance = new WebAssembly.Instance(compiled, {
            zx: {
                overrun: () => { throw new Error('zx budget spent'); },
                budget: budget,
            },
        });
        const id = tier.next++;
        tier.instances.set(id, {instance: instance, budget: budget});
        return id;
    } catch(error) {
        console.error('zx wasm tier: ' + error);
        return 0;
    }
});

EM_JS(int, zx_wasm_web_set_budget, (unsigned long handle, long long budget), {
    const entry = globalThis.zxWasmTier.instances.get(handle);
    if(entry === undefined)
        return 1;
    entry.budget.value = BigInt.asIntN(64, BigInt(budget));
    return 0;
});

EM_JS(int, zx_wasm_web_call, (unsigned long handle, long long *result), {
    const entry = globalThis.zxWasmTier.instances.get(handle);
    if(entry === undefined)
        return 1;
    try {
        setValue(result, entry.instance.exports.zx_entry(), 'i64');
        return 0;
    } catch(error) {
        console.error('zx wasm tier: ' + error);
        return 1;
    }
});

EM_JS(void, zx_wasm_web_release, (unsigned long handle), {
    if(globalThis.zxWasmTier !== undefined)
        globalThis.zxWasmTier.instances.delete(handle);
});

EM_JS(int, zx_wasm_web_enabled, (void), {
    if(typeof process !== 'undefined' && process.env !== undefined &&
       process.env.ZIRAN_WASM_TIER === '0')
        return 0;
    return 1;
});

static unsigned long
web_instantiate(void *context, const unsigned char *bytes, size_t size)
{
    (void)context;
    return zx_wasm_web_instantiate(bytes, size);
}

static int
web_set_budget(void *context, unsigned long handle, long long budget)
{
    (void)context;
    return zx_wasm_web_set_budget(handle, budget);
}

static int
web_call(void *context, unsigned long handle, long long *result)
{
    (void)context;
    return zx_wasm_web_call(handle, result);
}

static void
web_release(void *context, unsigned long handle)
{
    (void)context;
    zx_wasm_web_release(handle);
}

/* Called from the engine's load-time constructor: a rebuilt web runtime
 * activates the tier without any host change; hosts that prefer the
 * plain VM set ZIRAN_WASM_TIER=0. */
void
ZxWasmInstallBundledPlatform(void)
{
    if(!zx_wasm_web_enabled())
        return;
    ZxWasmPlatform platform = {0};
    platform.instantiate = web_instantiate;
    platform.set_budget = web_set_budget;
    platform.call = web_call;
    platform.release = web_release;
    ZxWasmSetPlatform(&platform);
}

#else

/* Native and non-Emscripten builds keep the provider seam empty; the
 * TinyCC tier or the portable VM runs bundles there. */
typedef int zx_wasm_web_unused;

#endif
