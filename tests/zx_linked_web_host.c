/* Host for code linked into a WebAssembly host ahead of time: the bundle
 * whose program `zi2zx native-c` wrote runs as that code, a second instance
 * open at the same time runs on the portable VM, every instance starts from
 * fresh globals, and another bundle runs on the VM. Run under node by
 * tests/zx_linked_web.sh, which counts the linked opens on stderr. */
#include "ziran_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
host_call(void *context, const char *module, const char *function,
          const VmHostValue *args, int arg_count, VmHostValue *result)
{
    (void)context;
    if(strcmp(module, "linked_api") != 0 || strcmp(function, "AddHost") != 0 ||
       arg_count != 1 || args[0].kind != VM_HOST_INTEGER)
        return 0;
    result->kind = VM_HOST_INTEGER;
    result->type = "s64";
    result->integer = args[0].integer + 1000;
    return 1;
}

static const HostBinding bindings[] = {
    {.module = "linked_api", .function = "AddHost", .call = host_call},
};

static long long
run(BundleInstance *instance)
{
    long long result = 0;
    int has_result = 0;
    if(instance == NULL || !BundleInstanceRun(instance, &result, &has_result) || !has_result)
        return -1;
    return result;
}

int
main(int argc, char **argv)
{
    if(argc != 3)
        return 2;
    setenv("ZIRAN_NATIVE_TIMING", "1", 1);
    Bundle *linked = BundleOpen(argv[1]);
    Bundle *plain = BundleOpen(argv[2]);
    if(linked == NULL || plain == NULL)
        return 3;
    BundleInstance *first = BundleInstantiate(linked, bindings, 1);
    long long a = run(first), b = run(first);
    /* Open beside the first: the VM runs it, from fresh globals. */
    BundleInstance *beside = BundleInstantiate(linked, bindings, 1);
    long long c = run(beside);
    BundleInstanceClose(beside);
    BundleInstanceClose(first);
    /* Reopened alone: linked code again, its globals as they started. */
    BundleInstance *again = BundleInstantiate(linked, bindings, 1);
    long long d = run(again);
    BundleInstanceClose(again);
    long long e = 0;
    int has_result = 0;
    if(!BundleRun(plain, NULL, 0, &e, &has_result))
        e = -1;
    printf("%lld %lld %lld %lld %lld\n", a, b, c, d, e);
    BundleClose(linked);
    BundleClose(plain);
    return 0;
}
