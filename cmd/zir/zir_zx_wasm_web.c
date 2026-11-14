/*
 * zir_zx_wasm_web.c - the WebAssembly execution tier's web provider.
 *
 * Compiled only by Emscripten builds of libziran: the provider reaches the
 * browser's WebAssembly engine through EM_JS, installs itself when the
 * module loads (unless ZIRAN_WASM_TIER=0 or a JSPI runtime, which leave
 * bundles to the instruction engine), and memoizes compiled modules per
 * page session
 * keyed by the module image, so reopening a bundle pays instantiation but
 * not compilation again. Bundles outside the Wasm lowering fail with its
 * wasm.gap diagnostic.
 *
 * The JavaScript side only moves bytes: a revision-28 module's (zx, host)
 * import hands the call record's address to ZxWasmHostCall, which reads
 * the host images from the instance's memory, translates them to the
 * page's VmHostValue bindings (ExecHosts) and writes the result back,
 * placing result images with the module's zx_reserve.
 *
 * Under Asyncify a host may suspend the page's stack inside a host call.
 * Modules are then lowered suspendable: when the host call returns while
 * the stack unwinds, the module is told to unwind too and saves its
 * frames; when the stack rewinds into the call of zx_entry or
 * zx_initialize, the module is told to rewind and resumes at the host
 * call, which re-enters the suspended host.
 */
#include "zir_zx_wasm_platform.h"
#include "zir_zx_wasm.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <stddef.h>

/* Returns a nonzero instance handle, or 0 when the module is invalid. */
EM_JS(unsigned long, zx_wasm_web_instantiate,
      (const unsigned char *bytes, size_t size, void *host), {
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
        const entry = {instance: null, budget: budget};
        const unwinding = () => typeof Asyncify !== 'undefined' && Asyncify.State !== undefined &&
                                Asyncify.state === Asyncify.State.Unwinding;
        const instance = new WebAssembly.Instance(compiled, {
            zx: {
                overrun: () => { throw new Error('zx budget spent'); },
                host_i64: (index, value) => {
                    return _zx_wasm_provider_print(host, index >>> 0, value);
                },
                host: (index, record) => {
                    const ok = _zx_wasm_provider_call(host, index >>> 0, record >>> 0);
                    const state = entry.instance.exports.zx_async_state;
                    if(state !== undefined && unwinding()) {
                        state.value = 1;
                        return 0;
                    }
                    return ok;
                },
                budget: budget,
            },
        });
        entry.instance = instance;
        const id = tier.next++;
        tier.instances.set(id, entry);
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
        const state = entry.instance.exports.zx_async_state;
        if(state !== undefined && typeof Asyncify !== 'undefined' && Asyncify.state === Asyncify.State.Rewinding)
            state.value = 2;
        const value = entry.instance.exports.zx_entry();
        /* The page's stack unwinds through this call; it calls again to
         * rewind. */
        if(state !== undefined && state.value === 1)
            return 0;
        setValue(result, value, 'i64');
        return 0;
    } catch(error) {
        /* A trap the module could not see coming leaves its output in its
         * buffer. */
        try {
            if(entry.instance.exports.zx_flush !== undefined)
                entry.instance.exports.zx_flush();
        } catch(ignored) {
        }
        console.error('zx wasm tier: ' + error);
        return 1;
    }
});

EM_JS(int, zx_wasm_web_read, (unsigned long handle, size_t address,
                              void *destination, size_t size), {
    try {
        const entry = globalThis.zxWasmTier.instances.get(handle);
        if(entry === undefined)
            return 1;
        const source = new Uint8Array(entry.instance.exports.memory.buffer,
                                      Number(address), Number(size));
        HEAPU8.set(source, Number(destination));
        return 0;
    } catch(error) {
        return 2;
    }
});

EM_JS(int, zx_wasm_web_write, (unsigned long handle, size_t address,
                               const void *source, size_t size), {
    const entry = globalThis.zxWasmTier.instances.get(handle);
    if(entry === undefined)
        return 1;
    const bytes = new Uint8Array(entry.instance.exports.memory.buffer,
                                 Number(address), Number(size));
    bytes.set(new Uint8Array(HEAPU8.buffer, Number(source), Number(size)));
    return 0;
});

EM_JS(size_t, zx_wasm_web_allocate_state,
      (unsigned long handle, size_t size), {
    const entry = globalThis.zxWasmTier.instances.get(handle);
    if(entry === undefined)
        return 0;
    if(entry.instance.exports.zx_reserve === undefined)
        return 0;
    try {
        return entry.instance.exports.zx_reserve(Number(size)) >>> 0;
    } catch(error) {
        return 0;
    }
});

EM_JS(int, zx_wasm_web_initialize, (unsigned long handle), {
    const entry = globalThis.zxWasmTier.instances.get(handle);
    if(entry === undefined)
        return 1;
    try {
        const state = entry.instance.exports.zx_async_state;
        if(state !== undefined && typeof Asyncify !== 'undefined' && Asyncify.state === Asyncify.State.Rewinding)
            state.value = 2;
        if(entry.instance.exports.zx_initialize !== undefined)
            entry.instance.exports.zx_initialize();
        return 0;
    } catch(error) {
        console.error('zx wasm tier: ' + error);
        return 1;
    }
});

EM_JS(int, zx_wasm_web_budget_left, (unsigned long handle, long long *budget), {
    const entry = globalThis.zxWasmTier.instances.get(handle);
    if(entry === undefined)
        return 1;
    setValue(budget, entry.budget.value, 'i64');
    return 0;
});

EM_JS(void, zx_wasm_web_release, (unsigned long handle), {
    if(globalThis.zxWasmTier !== undefined)
        globalThis.zxWasmTier.instances.delete(handle);
});

EM_JS(int, zx_wasm_web_enabled, (void), {
    /* A JSPI runtime suspends through JavaScript frames, which the module
     * calls go through. */
    if(typeof Asyncify !== 'undefined' && Asyncify.State === undefined)
        return 0;
    if(typeof process !== 'undefined' && process.env !== undefined &&
       process.env.ZIRAN_WASM_TIER === '0')
        return 0;
    /* A page opts out before the runtime loads. */
    if(globalThis.ZIRAN_WASM_TIER === '0')
        return 0;
    return 1;
});

/* Asyncify: hosts may suspend the stack, so modules are lowered to follow. */
EM_JS(int, zx_wasm_web_suspends, (void), {
    return typeof Asyncify !== 'undefined' && Asyncify.State !== undefined ? 1 : 0;
});

EM_JS(void, zx_wasm_web_host_failed, (void), {
    throw new Error('zx platform output failed');
});

EMSCRIPTEN_KEEPALIVE long long
zx_wasm_provider_print(void *host, unsigned service, long long argument)
{
    if(!ZxWasmHostPrint(host, service, argument))
        zx_wasm_web_host_failed();
    return 0;
}

EMSCRIPTEN_KEEPALIVE int
zx_wasm_provider_call(void *host, unsigned index, unsigned record)
{
    return ZxWasmHostCall(host, index, record);
}

static unsigned long
web_instantiate(void *context, const unsigned char *bytes, size_t size)
{
    return zx_wasm_web_instantiate(bytes, size, context);
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

static int
web_read(void *context, unsigned long handle, size_t address,
         void *destination, size_t size)
{
    (void)context;
    return zx_wasm_web_read(handle, address, destination, size);
}

static int
web_write(void *context, unsigned long handle, size_t address,
          const void *source, size_t size)
{
    (void)context;
    return zx_wasm_web_write(handle, address, source, size);
}

static size_t
web_allocate_state(void *context, unsigned long handle, size_t size)
{
    (void)context;
    return zx_wasm_web_allocate_state(handle, size);
}

static int
web_initialize(void *context, unsigned long handle)
{
    (void)context;
    return zx_wasm_web_initialize(handle);
}

static int
web_budget_left(void *context, unsigned long handle, long long *budget)
{
    (void)context;
    return zx_wasm_web_budget_left(handle, budget);
}

static void
web_release(void *context, unsigned long handle)
{
    (void)context;
    zx_wasm_web_release(handle);
}

/* Called from the load-time constructor: a rebuilt web runtime activates
 * the tier without any host change; ZIRAN_WASM_TIER=0 and JSPI runtimes
 * leave bundles to the instruction engine. */
void
ZxWasmInstallBundledPlatform(void)
{
    /* The provider's JavaScript speaks wasm32 addresses; a 64-bit runtime
     * such as the playground's leaves bundles to the instruction engine. */
    if(sizeof(void *) != 4 || !zx_wasm_web_enabled())
        return;
    ZxWasmPlatform platform = {0};
    platform.instantiate = web_instantiate;
    platform.set_budget = web_set_budget;
    platform.call = web_call;
    platform.read = web_read;
    platform.write = web_write;
    platform.allocate_state = web_allocate_state;
    platform.initialize = web_initialize;
    platform.budget_left = web_budget_left;
    platform.release = web_release;
    platform.suspends = zx_wasm_web_suspends();
    ZxWasmSetPlatform(&platform);
}

#else

/* Native and non-Emscripten builds keep the provider seam empty; the
 * instruction engine runs bundles there. */
typedef int zx_wasm_web_unused;

#endif
