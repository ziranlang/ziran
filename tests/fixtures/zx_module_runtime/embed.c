/* A minimal process adapter so the same test application can run in libziran
 * hosted by Node. The lifecycle assertions live in application.zi. */
#include "ziran_zx_module.h"

#include <stdio.h>
#include <string.h>

static int
check_library(ZxModuleLoader *loader, const char *root, const BundleInstanceOptions *options)
{
    ZxModuleLoader *other = ZxModuleLoaderOpen(&root, 1, options);
    uint64_t first = ZxModuleLoad(loader, "demo/counter");
    uint64_t second = ZxModuleLoad(other, "demo/counter");
    const char *name = ZxModuleExportName(loader, first, 0);
    const char *parameter = ZxModuleParameterType(loader, first, 0, 0);
    const char *result_type = ZxModuleResultType(loader, first, 0);
    HostValue amount = {.kind = HOST_INTEGER, .type = "s32", .integer = 4};
    HostValue result;
    int ok = first != 0 && second != 0 && first != second &&
        ZxModuleExportCount(loader, first) == 4 && ZxModuleParameterCount(loader, first, 0) == 1 &&
        name != NULL && strcmp(name, "Add") == 0 && parameter != NULL && strcmp(parameter, "s32") == 0 &&
        result_type != NULL && strcmp(result_type, "s32") == 0 &&
        !ZxModuleCall(other, first, 0, &amount, 1, &result) &&
        ZxModuleCall(loader, first, 0, &amount, 1, &result) && result.integer == 8;
    amount.integer = 0;
    ok = ok && ZxModuleCall(other, second, 0, &amount, 1, &result) && result.integer == 4 &&
        ZxModuleRelease(loader, first) && !ZxModuleCall(loader, first, 0, &amount, 1, &result) &&
        ZxModuleRelease(other, second);
    ZxModuleLoaderClose(other);
    return ok;
}

int main(int argc, char **argv)
{
    if(argc != 4) return 2;
    BundleExecutionBackend expected = BUNDLE_EXECUTION_WASM;
    BundleInstanceOptions options = {0};
    if(strcmp(argv[3], "engine") == 0) {
        options.backend = BUNDLE_BACKEND_ENGINE;
        expected = BUNDLE_EXECUTION_ENGINE;
    } else if(strcmp(argv[3], "native") == 0) {
        options.backend = BUNDLE_BACKEND_NATIVE;
        expected = BUNDLE_EXECUTION_NATIVE_X86_64;
    }
    const char *root = argv[1];
    ZxModuleLoader *loader = ZxModuleLoaderOpen(&root, 1, &options);
    if(!check_library(loader, root, &options)) {
        fprintf(stderr, "ZX C module API: %s\n", ZxModuleLoaderError(loader));
        ZxModuleLoaderClose(loader);
        return 1;
    }
    Bundle *bundle = BundleOpen(argv[2]);
    ZxModuleProgram *program = ZxModuleProgramOpen(loader, bundle, NULL, 0);
    long long result;
    int has_result;
    int ok = ZxModuleProgramBackend(program) == expected &&
             ZxModuleProgramRun(program, &result, &has_result) && has_result;
    if(ok) printf("%lld\n", result);
    else fprintf(stderr, "%s (backend %d)\n", ZxModuleLoaderError(loader), ZxModuleProgramBackend(program));
    ZxModuleProgramClose(program);
    BundleClose(bundle);
    ZxModuleLoaderClose(loader);
    return ok ? 0 : 1;
}
