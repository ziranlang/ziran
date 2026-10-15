#include "ziran_host.h"
#include "zir_bundle.h"
#include "zir_check.h"
#include "zir_parse.h"
#include "zir_vm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *bundle_bytes(const ZirProgram *program, const char *entry,
                                    size_t *size)
{
    FILE *output = tmpfile();
    assert(output != NULL);
    assert(BundleWrite(output, program, "verified", entry, NULL));
    long length = ftell(output);
    assert(length > 0);
    rewind(output);
    unsigned char *bytes = malloc((size_t)length);
    assert(bytes != NULL && fread(bytes, 1, (size_t)length, output) == (size_t)length);
    fclose(output);
    *size = (size_t)length;
    return bytes;
}

int main(void)
{
    const char *source = "count: s32 = 40;\n"
        "#program_export\nNext :: () -> s32 {\ncount += 1\nreturn count\n}\n"
        "#program_export\nReal :: () -> float64 {\nreturn 1.5\n}\n";
    ZirProgram *program = parse_source_text("verified.zi", source);
    assert(program != NULL && CheckPrograms(&program, 1));
    assert(VmVerify(program, "verified", "Next"));
    VmInstance *raw = VmInstanceOpen(program, "verified", "Next", NULL, NULL);
    assert(raw != NULL);
    VmInstanceClose(raw);
    /* Caller-owned IR may have changed since an earlier verification. */
    ZirFunction *next = &program->modules[0].functions[0];
    next->checked = 0;
    assert(VmInstanceOpen(program, "verified", "Next", NULL, NULL) == NULL);
    next->checked = 1;
    size_t size = 0;
    unsigned char *bytes = bundle_bytes(program, "Next", &size);
    Bundle *bundle = BundleOpenBytes(bytes, size);
    free(bytes);
    assert(bundle != NULL);
    long long result;
    int present;
    BundleInstance *first = BundleInstantiate(bundle, NULL, 0);
    BundleInstance *second = BundleInstantiate(bundle, NULL, 0);
    assert(first != NULL && second != NULL);
    BundleInstanceLimitSteps(first, 1);
    assert(!BundleInstanceRun(first, &result, &present));
    assert(!BundleInstanceRun(first, &result, &present));
    assert(BundleInstanceRun(second, &result, &present) && present && result == 41);
    assert(BundleInstanceRun(second, &result, &present) && present && result == 42);
    BundleInstanceClose(first);
    BundleInstanceClose(second);
    assert(BundleRun(bundle, NULL, 0, &result, &present) && present && result == 41);
    BundleClose(bundle);
    /* This bundle passes canonical semantic loading but has an unsupported
     * portable entry. Opaque bundles must reject it before instantiation. */
    bytes = bundle_bytes(program, "Real", &size);
    FILE *input = tmpfile();
    assert(input != NULL && fwrite(bytes, 1, size, input) == size);
    rewind(input);
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZirProgram *loaded = BundleRead(input, "<real entry>", module, sizeof(module),
                                    entry, sizeof(entry), NULL, NULL);
    fclose(input);
    assert(loaded != NULL && !VmVerify(loaded, module, entry));
    ProgramFree(loaded);
    assert(BundleOpenBytes(bytes, size) == NULL);
    free(bytes);
    ProgramFree(program);
    puts("Bundle verification, changed raw IR and failure/state isolation passed");
    return 0;
}
