/* An Asyncify runtime whose host suspends inside a bundle call: the bundle
 * runs on the Wasm tier as a suspendable module, which unwinds with the
 * page's stack and resumes at the host call. */
#include "ziran_host.h"

#include <emscripten.h>
#include <stdio.h>

static int suspended;

static int adjust(void *context, const char *module, const char *function,
                  const VmHostValue *arguments, int count, VmHostValue *result)
{
    (void)context; (void)module; (void)function;
    if(count != 1 || arguments[0].kind != VM_HOST_INTEGER)
        return 0;
    emscripten_sleep(1);
    suspended++;
    result->kind = VM_HOST_INTEGER;
    result->type = "s64";
    result->integer = arguments[0].integer + 1;
    result->bits = (uint64_t)result->integer;
    return 1;
}

static long long run(BundleInstance *instance)
{
    long long result = -1;
    int present = 0;
    return instance != NULL && BundleInstanceRun(instance, &result, &present) && present ? result : -1;
}

/* Prints each run's result, the suspensions and whether the Wasm tier ran:
 * the same with ZIRAN_WASM_TIER=0, which runs the engine, but for the
 * tier. The second bundle suspends in an initializer and in a helper
 * called in a loop. */
int main(int argc, char **argv)
{
    HostBinding bindings[] = {{.module = "component", .function = "Adjust", .call = adjust}};
    Bundle *bundle = argc == 3 ? BundleOpen(argv[1]) : NULL;
    Bundle *deep = argc == 3 ? BundleOpen(argv[2]) : NULL;
    BundleInstance *instance = bundle == NULL ? NULL : BundleInstantiate(bundle, bindings, 1);
    BundleInstance *nested = deep == NULL ? NULL : BundleInstantiate(deep, bindings, 1);
    long long first = run(instance), second = run(instance);
    long long third = run(nested), fourth = run(nested);
    printf("results %lld %lld %lld %lld suspended %d tier %d\n", first, second, third, fourth, suspended,
           EM_ASM_INT({ return globalThis.zxWasmTier !== undefined && globalThis.zxWasmTier.instances.size === 2; }));
    BundleInstanceClose(nested);
    BundleInstanceClose(instance);
    BundleClose(deep);
    BundleClose(bundle);
    return 0;
}
