/*
 * The native loader writes into anonymous RW pages, resolves every
 * relocation, flushes instructions where the platform requires it, and only
 * then publishes code as RX and constants as R. No operation creates RWX.
 */
#include "zir_native_memory.h"

#include <stdint.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
#include <sys/mman.h>
#include <unistd.h>
#endif

static int
native_size(size_t requested, size_t *rounded)
{
    if(requested == 0 || rounded == NULL)
        return 0;
    size_t page = ZirNativePageSize();
    if(page == 0 || requested > SIZE_MAX - (page - 1))
        return 0;
    *rounded = (requested + page - 1) / page * page;
    return *rounded != 0;
}

size_t
ZirNativePageSize(void)
{
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (size_t)info.dwPageSize;
#elif !defined(__EMSCRIPTEN__)
    long value = sysconf(_SC_PAGESIZE);
    return value > 0 ? (size_t)value : 0;
#else
    return 0;
#endif
}

int
ZirNativeReserve(size_t requested, ZirNativeMapping *mapping)
{
    size_t size;
    void *address;
    if(mapping == NULL || mapping->address != NULL || mapping->size != 0 ||
       !native_size(requested, &size))
        return 0;
#if defined(_WIN32)
    address = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if(address == NULL)
        return 0;
#elif !defined(__EMSCRIPTEN__)
    address = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(address == MAP_FAILED)
        return 0;
#else
    return 0;
#endif
    mapping->address = address;
    mapping->size = size;
    return 1;
}

static int
native_protect(const ZirNativeMapping *mapping, int executable)
{
    if(mapping == NULL || mapping->address == NULL || mapping->size == 0)
        return 0;
#if defined(_WIN32)
    DWORD previous;
    DWORD protection = executable ? PAGE_EXECUTE_READ : PAGE_READONLY;
    return VirtualProtect(mapping->address, mapping->size, protection, &previous) != 0;
#elif !defined(__EMSCRIPTEN__)
    int protection = PROT_READ | (executable ? PROT_EXEC : 0);
    return mprotect(mapping->address, mapping->size, protection) == 0;
#else
    (void)executable;
    return 0;
#endif
}

int
ZirNativeProtectRX(const ZirNativeMapping *mapping)
{
    return native_protect(mapping, 1);
}

int
ZirNativeProtectR(const ZirNativeMapping *mapping)
{
    return native_protect(mapping, 0);
}

int
ZirNativeFlushInstructions(const ZirNativeMapping *mapping)
{
    if(mapping == NULL || mapping->address == NULL || mapping->size == 0)
        return 0;
#if defined(_WIN32)
    return FlushInstructionCache(GetCurrentProcess(), mapping->address, mapping->size) != 0;
#elif !defined(__EMSCRIPTEN__)
    char *begin = mapping->address;
    __builtin___clear_cache(begin, begin + mapping->size);
    return 1;
#else
    return 0;
#endif
}

void
ZirNativeRelease(ZirNativeMapping *mapping)
{
    if(mapping == NULL || mapping->address == NULL || mapping->size == 0)
        return;
#if defined(_WIN32)
    (void)VirtualFree(mapping->address, 0, MEM_RELEASE);
#elif !defined(__EMSCRIPTEN__)
    (void)munmap(mapping->address, mapping->size);
#endif
    mapping->address = NULL;
    mapping->size = 0;
}
