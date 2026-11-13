/*
 * zir_zx_wasm_web.c - the WebAssembly execution tier's web provider.
 *
 * Compiled only by Emscripten builds of libziran: the provider reaches the
 * browser's WebAssembly engine through EM_JS, installs itself when the
 * module loads (unless ZIRAN_WASM_TIER=0), and memoizes compiled modules
 * per page session keyed by the module image, so reopening a bundle pays
 * instantiation but not compilation again. Hosts need no code changes:
 * rebuilding the wasm bundle runtime activates the tier, and bundles
 * outside its subset fail with the selected engine's diagnostic.
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
        const instance = new WebAssembly.Instance(compiled, {
            zx: {
                overrun: () => { throw new Error('zx budget spent'); },
                host_i64: (index, value) => {
                    return _zx_wasm_provider_host(host, Number(index), value);
                },
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
    const memory = entry.instance.exports.memory;
    const arena = entry.instance.exports.zx_state_arena;
    const start = (Number(arena.value) + 7) & ~7;
    const end = start + Number(size);
    if(end > 0x7fffffff)
        return 0;
    if(end > memory.buffer.byteLength) {
        const pages = Math.ceil((end - memory.buffer.byteLength) / 65536);
        if(memory.grow(pages) === -1)
            return 0;
    }
    arena.value = end;
    return start;
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

EM_JS(void, zx_wasm_web_host_failed, (void), {
    throw new Error('zx host capability failed');
});

EMSCRIPTEN_KEEPALIVE long long
zx_wasm_provider_host(void *host, int index, long long argument)
{
    long long result = 0;
    if(!ZxWasmHostCallI64(host, (size_t)index, argument, &result)) {
        zx_wasm_web_host_failed();
        return 0;
    }
    return result;
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
    platform.read = web_read;
    platform.write = web_write;
    platform.allocate_state = web_allocate_state;
    platform.release = web_release;
    ZxWasmSetPlatform(&platform);
}

#else

/* Native and non-Emscripten builds keep the provider seam empty; their
 * configured engine runs bundles there. */
typedef int zx_wasm_web_unused;

#endif
