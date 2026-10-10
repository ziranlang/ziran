#include "zir_bundle.h"
#include "zir_vm_internal.h"
#include <assert.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZibLawTable laws = {0};
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL);
    ZirProgram *program = BundleRead(input, argv[1], module, sizeof(module), entry,
                                    sizeof(entry), &laws, NULL);
    fclose(input);
    assert(program != NULL);
    VmInstance *instances[2];
    size_t retained[2] = {0};
    for(int i = 0; i < 2; i++) {
        instances[i] = VmInstanceOpen(program, module, entry, NULL, NULL);
        assert(instances[i] != NULL);
    }
    for(int run = 0; run < 5; run++) {
        for(int i = 0; i < 2; i++) {
            long long answer = -1;
            int present = 0;
            assert(VmInstanceRun(instances[i], &answer, &present) && present && answer == 0);
            if(run == 0) retained[i] = instances[i]->vm.string_bytes;
            else assert(instances[i]->vm.string_bytes == retained[i]);
        }
    }
    assert(retained[0] > 0 && retained[0] == retained[1]);
    VmInstanceClose(instances[0]);
    /* Closing one instance cannot invalidate another instance's literals. */
    long long answer = -1;
    int present = 0;
    assert(VmInstanceRun(instances[1], &answer, &present) && present && answer == 0);
    assert(instances[1]->vm.string_bytes == retained[1]);
    VmInstanceClose(instances[1]);
    ZibLawTableFree(&laws);
    ProgramFree(program);
    puts("Literal collisions, binary/Unicode text and repeated independent-instance lifetimes passed");
    return 0;
}
