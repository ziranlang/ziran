/* Uses only host API shared by ZX 26 and ZX 27 hosts, so the same source
 * links against an older libziran.a to check what that host can open. */
#include "ziran_host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int host(void *context, const char *module, const char *function,
                const VmHostValue *args, int count, VmHostValue *result)
{
    (void)context; (void)module; (void)function; (void)args; (void)count;
    result->kind = VM_HOST_INTEGER;
    result->integer = 0;
    return 1;
}

static int run(const Bundle *bundle, long long expected)
{
    HostBinding bindings[] = {{"model", "BaseHost", host, NULL},
                              {"model", "AddonHost", host, NULL},
                              {"model", "CommonHost", host, NULL}};
    long long result = 0;
    int present = 0;
    return bundle && BundleRun(bundle, bindings, 3, &result, &present) &&
           present && result == expected;
}

/* base.zx standalone.zx plain-thin.zx partial-thin.zx old|new */
int main(int argc, char **argv)
{
    if(argc != 6) return 2;
    int old = !strcmp(argv[5], "old");
    Bundle *base = BundleOpen(argv[1]);
    Bundle *standalone = BundleOpen(argv[2]);
    Bundle *libraries[] = {base};
    Bundle *plain = base ? BundleOpenWithLibraries(argv[3], libraries, 1) : NULL;
    Bundle *partial = base ? BundleOpenWithLibraries(argv[4], libraries, 1) : NULL;
    int ok = 1;
    if(!run(base, 41)) { fprintf(stderr, "library bundle did not run\n"); ok = 0; }
    if(!run(standalone, 45)) { fprintf(stderr, "standalone bundle did not run\n"); ok = 0; }
    if(!run(plain, 42)) { fprintf(stderr, "plain add-on did not run\n"); ok = 0; }
    if(old && partial) { fprintf(stderr, "old host opened a ZX 27 add-on\n"); ok = 0; }
    if(!old && !run(partial, 45)) { fprintf(stderr, "partial add-on did not run\n"); ok = 0; }
    BundleClose(partial); BundleClose(plain); BundleClose(standalone); BundleClose(base);
    if(ok) printf("%s host: plain bundles and add-ons run; partial add-ons %s\n",
                  old ? "ZX 26" : "current", old ? "refused" : "run");
    return ok ? 0 : 1;
}
