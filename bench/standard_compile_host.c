/* Include native instantiation in build timing, without timing execution. */
#include "ziran_host.h"
#include <stdio.h>
#include <stdlib.h>
static long long inputs[2];
static int input(void *ctx, const char *mod, const char *fn,
                 const HostValue *args, int n, HostValue *out) {
    (void)ctx; (void)mod; (void)fn;
    if (n != 1 || args[0].kind != HOST_INTEGER ||
        args[0].integer < 0 || args[0].integer > 1) return 0;
    out->kind = HOST_INTEGER;
    out->type = "s64";
    out->integer = inputs[args[0].integer];
    return 1;
}
int main(int argc, char **argv) {
    if (argc != 2 && argc != 5) return 2;
    if (argc == 5) {
        inputs[0] = atoll(argv[2]);
        inputs[1] = atoll(argv[3]);
        if (inputs[0] < 0 || inputs[0] > 28 || inputs[1] < 1) return 2;
    }
    Bundle *bundle = BundleOpen(argv[1]);
    if (!bundle) return 3;
    HostBinding binding = {"entry", "Input", input, NULL};
    BundleInstanceOptions options = {0};
    options.backend = BUNDLE_BACKEND_NATIVE;
    BundleInstance *instance = BundleInstantiateWithOptions(bundle, &binding, 1, &options);
    if (!instance) { BundleClose(bundle); return 4; }
    int backend = BundleInstanceBackend(instance);
    int status = 0;
    if (backend != BUNDLE_EXECUTION_NATIVE_X86_64 &&
        backend != BUNDLE_EXECUTION_NATIVE_AARCH64) status = 5;
    if (!status && argc == 5) {
        long long value = 0;
        int present = 0;
        if (!BundleInstanceRun(instance, &value, &present) || !present || value != atoll(argv[4])) status = 6;
        else printf("%lld\n", value);
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return status;
}
