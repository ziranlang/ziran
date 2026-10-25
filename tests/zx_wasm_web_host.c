/* End-to-end host for the wasm execution tier in its production shape:
 * libziran compiled to WebAssembly, bundles opened from the real file
 * system, and the platform provider implemented in JavaScript over the
 * browser's WebAssembly engine. Runs under node through tests/zx_wasm_web.sh. */
#include "ziran_host.h"
#include "zir_zx_wasm_platform.h"

#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

EM_JS(unsigned long, zx_wasm_js_instantiate, (int ptr, int size), {
    const bytes = new Uint8Array(HEAPU8.buffer, ptr, size).slice();
    try {
        const budget = new WebAssembly.Global({value: 'i64', mutable: true}, 0n);
        const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), {
            zx: {
                overrun: () => { throw new Error('zx budget spent'); },
                budget: budget,
            },
        });
        if(globalThis.zxWasmInstances === undefined)
            globalThis.zxWasmInstances = new Map();
        const id = globalThis.zxWasmInstances.size + 1;
        globalThis.zxWasmInstances.set(id, {instance: instance, budget: budget});
        return id;
    } catch(error) {
        console.error('instantiate failed: ' + error);
        return 0;
    }
});

EM_JS(int, zx_wasm_js_lookup, (unsigned long handle), {
    if(globalThis.zxWasmInstances === undefined ||
       !globalThis.zxWasmInstances.has(handle))
        return 0;
    globalThis.zxWasmInstances.delete(handle);
    return 1;
});

EM_JS(int, zx_wasm_js_set_budget, (unsigned long handle, long long budget), {
    if(globalThis.zxWasmInstances === undefined)
        return 1;
    const entry = globalThis.zxWasmInstances.get(handle);
    if(entry === undefined)
        return 1;
    entry.budget.value = BigInt.asIntN(64, BigInt(budget));
    return 0;
});

EM_JS(int, zx_wasm_js_call, (unsigned long handle, long long *result), {
    if(globalThis.zxWasmInstances === undefined)
        return 1;
    const entry = globalThis.zxWasmInstances.get(handle);
    if(entry === undefined)
        return 1;
    try {
        const value = entry.instance.exports.zx_entry();
        setValue(result, value, 'i64');
        return 0;
    } catch(error) {
        console.error('run trapped: ' + error);
        return 1;
    }
});

static unsigned long
provider_instantiate(void *context, const unsigned char *bytes, size_t size)
{
    (void)context;
    return zx_wasm_js_instantiate((int)bytes, (int)size);
}

static int
provider_set_budget(void *context, unsigned long handle, long long budget)
{
    (void)context;
    return zx_wasm_js_set_budget(handle, budget);
}

static int
provider_call(void *context, unsigned long handle, long long *result)
{
    (void)context;
    return zx_wasm_js_call(handle, result);
}

static void
provider_release(void *context, unsigned long handle)
{
    (void)context;
    zx_wasm_js_lookup(handle);
}

int
main(int argc, char **argv)
{
    if(argc != 4) {
        fprintf(stderr, "usage: zx_wasm_web_host manual|auto bundle.zx expected-result\n");
        return 2;
    }
    int manual = strcmp(argv[1], "manual") == 0;
    if(!manual && strcmp(argv[1], "auto") != 0) {
        fprintf(stderr, "usage: zx_wasm_web_host manual|auto bundle.zx expected-result\n");
        return 2;
    }
    char *end;
    long long expected = strtoll(argv[3], &end, 10);
    if(*end) {
        fprintf(stderr, "usage: zx_wasm_web_host manual|auto bundle.zx expected-result\n");
        return 2;
    }
    if(manual) {
        ZxWasmPlatform platform = {0};
        platform.instantiate = provider_instantiate;
        platform.set_budget = provider_set_budget;
        platform.call = provider_call;
        platform.release = provider_release;
        ZxWasmSetPlatform(&platform);
    } else if(ZxWasmPlatformActive() == NULL) {
        fprintf(stderr, "auto mode found no installed provider\n");
        return 1;
    }

    Bundle *bundle = BundleOpen(argv[2]);
    if(bundle == NULL) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    BundleInstance *instance = BundleInstantiate(bundle, NULL, 0);
    if(instance == NULL) {
        fprintf(stderr, "cannot instantiate\n");
        return 1;
    }
    long long result = 0;
    int has_result = 0;
    if(!BundleInstanceRun(instance, &result, &has_result) || !has_result) {
        fprintf(stderr, "run failed\n");
        return 1;
    }
    printf("%lld\n", result);
    if(result != expected) {
        fprintf(stderr, "tier result %lld != %lld\n", result, expected);
        return 1;
    }
    /* A budget of one step must trip the tier's overrun import. */
    BundleInstanceLimitSteps(instance, 1);
    long long spent = 0;
    if(BundleInstanceRun(instance, &spent, &has_result)) {
        fprintf(stderr, "a one-step budget did not stop the run\n");
        return 1;
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
