#include "zir_native_memory.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__linux__)
static int
mapping_permissions(const void *address, char permissions[5])
{
    FILE *maps = fopen("/proc/self/maps", "r");
    if(maps == NULL)
        return 0;
    uintptr_t target = (uintptr_t)address;
    char line[512];
    int found = 0;
    while(fgets(line, sizeof(line), maps) != NULL) {
        unsigned long long begin, end;
        char current[5] = {0};
        if(sscanf(line, "%llx-%llx %4s", &begin, &end, current) == 3 &&
           target >= (uintptr_t)begin && target < (uintptr_t)end) {
            memcpy(permissions, current, 5);
            found = 1;
            break;
        }
    }
    fclose(maps);
    return found;
}

static void
expect_permissions(const ZirNativeMapping *mapping, int writable, int executable)
{
    char permissions[5] = {0};
    assert(mapping_permissions(mapping->address, permissions));
    assert((permissions[1] == 'w') == writable);
    assert((permissions[2] == 'x') == executable);
    assert(!(permissions[1] == 'w' && permissions[2] == 'x'));
}
#endif

int
main(void)
{
    ZirNativeMapping invalid = {0}, code = {0}, constants = {0};
    assert(!ZirNativeReserve(0, &invalid));
    assert(!ZirNativeReserve(SIZE_MAX, &invalid));
    assert(!ZirNativeProtectRX(&invalid));
    assert(!ZirNativeProtectR(&invalid));
    assert(!ZirNativeFlushInstructions(&invalid));

    assert(ZirNativeReserve(17, &code));
    assert(code.address != NULL && code.size >= 17);
    memset(code.address, 0x90, 17);
#if defined(__linux__)
    expect_permissions(&code, 1, 0);
#endif
    assert(ZirNativeFlushInstructions(&code));
    assert(ZirNativeProtectRX(&code));
#if defined(__linux__)
    expect_permissions(&code, 0, 1);
#endif

    assert(ZirNativeReserve(9, &constants));
    memcpy(constants.address, "constant", 9);
#if defined(__linux__)
    expect_permissions(&constants, 1, 0);
#endif
    assert(ZirNativeProtectR(&constants));
#if defined(__linux__)
    expect_permissions(&constants, 0, 0);
#endif

    ZirNativeRelease(&constants);
    ZirNativeRelease(&code);
    assert(constants.address == NULL && constants.size == 0);
    assert(code.address == NULL && code.size == 0);
    ZirNativeRelease(&code);
    puts("native mappings transition RW to RX/R without RWX");
    return 0;
}
