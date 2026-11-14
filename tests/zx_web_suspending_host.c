/* An Asyncify runtime whose host suspends inside a bundle call: the Wasm
 * tier stays out, since a module instance cannot be suspended, and the
 * instruction engine runs the bundle with the suspending host. */
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

static int run(BundleInstance *instance, long long expected)
{
    long long result = -1;
    int present = 0;
    return BundleInstanceRun(instance, &result, &present) && present && result == expected;
}

int main(int argc, char **argv)
{
    HostBinding bindings[] = {{.module = "component", .function = "Adjust", .call = adjust}};
    Bundle *bundle = argc == 2 ? BundleOpen(argv[1]) : NULL;
    BundleInstance *instance = bundle == NULL ? NULL : BundleInstantiate(bundle, bindings, 1);
    int ok = instance != NULL && run(instance, 89) && run(instance, 99) && suspended == 2 &&
             EM_ASM_INT({ return globalThis.zxWasmTier === undefined; });
    printf("%s suspending-host-engine\n", ok ? "PASS" : "FAIL");
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return !ok;
}
