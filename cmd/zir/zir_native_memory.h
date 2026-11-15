/* zir_native_memory.h - W^X mappings for verified native ZX images. */
#ifndef ZIR_NATIVE_MEMORY_H
#define ZIR_NATIVE_MEMORY_H

#include <stddef.h>

typedef struct ZirNativeMapping {
    void *address;
    size_t size;
} ZirNativeMapping;

size_t ZirNativePageSize(void);
/* Reserve zero-filled, page-rounded RW storage. The only executable
 * transition is ZirNativeProtectRX; this API has no RWX state. */
int ZirNativeReserve(size_t requested, ZirNativeMapping *mapping);
int ZirNativeProtectRX(const ZirNativeMapping *mapping);
int ZirNativeProtectR(const ZirNativeMapping *mapping);
int ZirNativeFlushInstructions(const ZirNativeMapping *mapping);
void ZirNativeRelease(ZirNativeMapping *mapping);

#endif
