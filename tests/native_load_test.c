#include "ziran_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void
run(BundleInstance *instance, BundleExecutionBackend backend)
{
    long long result = 0;
    int present = 0;
    assert(instance != NULL);
    assert(BundleInstanceBackend(instance) == backend);
    assert(BundleInstanceRun(instance, &result, &present));
    assert(present && result == 42);
    BundleInstanceClose(instance);
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    assert(strcmp(BundleNativeProblem(bundle), "") == 0);
    assert(BundleInstanceBackend(NULL) == BUNDLE_EXECUTION_NONE);
    assert(BundleInstantiateWithOptions(bundle, NULL, 0, NULL) == NULL);

    BundleInstanceOptions invalid = {BUNDLE_BACKEND_AUTO, -1};
    assert(BundleInstantiateWithOptions(bundle, NULL, 0, &invalid) == NULL);
    invalid.backend = (BundleBackendPolicy)99;
    invalid.max_steps = 0;
    assert(BundleInstantiateWithOptions(bundle, NULL, 0, &invalid) == NULL);

    BundleInstanceOptions native = {BUNDLE_BACKEND_NATIVE, 0};
    run(BundleInstantiateWithOptions(bundle, NULL, 0, &native),
        BUNDLE_EXECUTION_NATIVE_X86_64);
    BundleInstanceOptions engine = {BUNDLE_BACKEND_ENGINE, 0};
    run(BundleInstantiateWithOptions(bundle, NULL, 0, &engine),
        BUNDLE_EXECUTION_ENGINE);
    BundleInstanceOptions automatic = {BUNDLE_BACKEND_AUTO, 0};
    run(BundleInstantiateWithOptions(bundle, NULL, 0, &automatic),
        BUNDLE_EXECUTION_ENGINE);

    BundleInstance *left = BundleInstantiateWithOptions(bundle, NULL, 0, &native);
    BundleInstance *right = BundleInstantiateWithOptions(bundle, NULL, 0, &native);
    assert(left != NULL && right != NULL && left != right);
    long long result = 0;
    int present = 0;
    assert(BundleInstanceRun(left, &result, &present) && present && result == 42);
    assert(BundleInstanceRun(right, &result, &present) && present && result == 42);
    BundleInstanceClose(left);
    BundleInstanceClose(right);
    BundleClose(bundle);
    puts("verified ZX runs through strict native and platform-selected engine backends");
    return 0;
}
