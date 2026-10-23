#include "ziran_host.h"
#include "zir_bundle.h"
#include "zir_vm.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(long long result, int present, uint64_t expected, int has_result)
{
    assert(present == has_result);
    if(present) assert((uint64_t)result == expected);
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    uint64_t expected = strtoull(argv[2], NULL, 10);
    int has_result = atoi(argv[3]);
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL && fseek(input, 0, SEEK_END) == 0);
    long length = ftell(input);
    assert(length > 0 && fseek(input, 0, SEEK_SET) == 0);
    unsigned char *bytes = malloc((size_t)length);
    assert(bytes != NULL && fread(bytes, 1, (size_t)length, input) == (size_t)length);
    Bundle *embedded = BundleOpenBytes(bytes, (size_t)length);
    memset(bytes, 0, (size_t)length);
    free(bytes);
    assert(embedded != NULL);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    for(int i = 0; i < 3; i++) {
        long long result = 0;
        int present = -1;
        assert(BundleRun(i ? bundle : embedded, NULL, 0, &result, &present));
        check(result, present, expected, has_result);
        BundleInstance *instance = BundleInstantiate(bundle, NULL, 0);
        assert(instance != NULL);
        for(int run = 0; run < 2; run++) {
            assert(BundleInstanceRun(instance, &result, &present));
            check(result, present, expected, has_result);
        }
        BundleInstanceClose(instance);
    }
    BundleClose(embedded);
    BundleClose(bundle);
    rewind(input);
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZxLawTable laws = {0};
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module),
                                    entry, sizeof(entry), &laws, NULL);
    fclose(input);
    assert(program != NULL);
    long long result = 0;
    int present = -1;
    assert(VmRun(program, module, entry, &result, &present));
    check(result, present, expected, has_result);
    assert(VmRunWithHost(program, module, entry, NULL, NULL, &result, &present));
    check(result, present, expected, has_result);
    assert(VmRunBounded(program, module, entry, NULL, NULL, 1000, &result, &present));
    check(result, present, expected, has_result);
    ZxLawTableFree(&laws);
    ProgramFree(program);
    return 0;
}
