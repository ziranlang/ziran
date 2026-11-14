#include "ziran_host.h"

#include <assert.h>
#include <stdio.h>

static int tick(void *context, const char *module, const char *function,
                const VmHostValue *args, int count, VmHostValue *result)
{
    (void)module; (void)function; (void)args;
    assert(count == 0);
    (*(int *)context)++;
    result->kind = VM_HOST_VOID;
    return 1;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    Bundle *loop = BundleOpen(argv[1]);
    Bundle *finite = BundleOpen(argv[2]);
    assert(loop != NULL && finite != NULL);
    int calls = 0;
    assert(BundleCapabilityCount(finite) == 1);
    HostBinding binding = {BundleCapabilityModule(finite, 0),
                           BundleCapabilityFunction(finite, 0), tick, &calls};
    assert(BundleInstantiateLimited(finite, &binding, 1, 0) == NULL);
    assert(BundleInstantiateLimited(finite, &binding, 1, -1) == NULL);
    /* An infinite global initializer must fail before returning an instance. */
    assert(BundleInstantiateLimited(loop, NULL, 0, 100) == NULL);

    int minimum = 0;
    for(int steps = 1; steps < 1024; steps++) {
        calls = 0;
        BundleInstance *instance = BundleInstantiateLimited(finite, &binding, 1, steps);
        if(instance != NULL) {
            minimum = steps;
            assert(calls == 100);
            BundleInstanceClose(instance);
            break;
        }
    }
    assert(minimum > 0);
    long long result = 0;
    int has_result = 0;
    /* Both initializer and entry call Work. This allowance is enough for
     * either separately, and too small for both together. */
    BundleInstance *instance = BundleInstantiateLimited(finite, &binding, 1, minimum + 16);
    assert(instance != NULL);
    assert(!BundleInstanceRun(instance, &result, &has_result));
    BundleInstanceClose(instance);

    /* An ordinary instance's limit bounds each run on its own. */
    instance = BundleInstantiate(finite, &binding, 1);
    assert(instance != NULL);
    BundleInstanceLimitSteps(instance, minimum + 16);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 14);
    assert(BundleInstanceRun(instance, &result, &has_result));
    BundleInstanceClose(instance);

    instance = BundleInstantiateLimited(finite, &binding, 1, 10000);
    assert(instance != NULL);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 14);
    BundleInstanceClose(instance);

    /* Repeated entry calls consume the same allowance, rather than resetting. */
    instance = BundleInstantiateLimited(finite, &binding, 1, 2 * minimum + 32);
    assert(instance != NULL);
    assert(BundleInstanceRun(instance, &result, &has_result));
    int stopped = 0;
    for(int run = 0; run < 4; run++)
        if(!BundleInstanceRun(instance, &result, &has_result)) {
            stopped = 1;
            break;
        }
    assert(stopped);
    BundleInstanceClose(instance);
    BundleClose(finite);
    BundleClose(loop);
    puts("Bundle startup and cumulative statement budget local tests passed");
    return 0;
}
