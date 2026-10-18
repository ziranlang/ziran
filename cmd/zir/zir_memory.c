#define ZIR_MEMORY_IMPLEMENTATION
#include "zir_memory.h"
#include <stdint.h>

typedef struct Allocation {
    void *pointer;
    int stream;
    void (*destroy)(void *);
    struct Allocation *next;
} Allocation;
struct CompilerMemory {
    Allocation *items;
    CompilerMemory *parent, *resume;
};
static _Thread_local CompilerMemory *current, *active;
static _Thread_local unsigned recovery_depth;

CompilerMemory *CompilerMemoryBegin(void)
{
    CompilerMemory *memory = calloc(1, sizeof(*memory));
    if(memory) { memory->parent = active; memory->resume = current; active = current = memory; recovery_depth++; }
    return memory;
}
CompilerMemory *CompilerMemoryPause(void)
{
    CompilerMemory *memory = current; current = NULL; return memory;
}
void CompilerMemoryResume(CompilerMemory *memory) { current = memory; }
int CompilerMemoryRecovering(void) { return recovery_depth != 0; }

static Allocation **find_allocation(void *pointer)
{
    for(CompilerMemory *memory = active; memory; memory = memory->parent)
        for(Allocation **item = &memory->items; *item; item = &(*item)->next)
            if((*item)->pointer == pointer) return item;
    return NULL;
}
static int track(void *pointer, int stream)
{
    if(!pointer || !current) return 1;
    Allocation *item = malloc(sizeof(*item));
    if(!item) return 0;
    *item = (Allocation){.pointer = pointer, .stream = stream, .next = current->items};
    current->items = item;
    return 1;
}
static void forget(void *pointer)
{
    Allocation **slot = find_allocation(pointer);
    if(slot) { Allocation *item = *slot; *slot = item->next; free(item); }
}
void CompilerMemoryCleanup(void *pointer, void (*destroy)(void *))
{
    Allocation **item = find_allocation(pointer);
    if(item) (*item)->destroy = destroy;
}

void *CompilerOwn(void *pointer)
{
    if(!track(pointer, 0)) { free(pointer); return NULL; }
    return pointer;
}
void *CompilerMalloc(size_t size)
{
    void *pointer = malloc(size);
    if(!track(pointer, 0)) { free(pointer); return NULL; }
    return pointer;
}
void *CompilerCalloc(size_t count, size_t size)
{
    void *pointer = calloc(count, size);
    if(!track(pointer, 0)) { free(pointer); return NULL; }
    return pointer;
}
void *CompilerRealloc(void *pointer, size_t size)
{
    if(!pointer) return CompilerMalloc(size);
    if(!size) { CompilerFree(pointer); return NULL; }
    Allocation **slot = find_allocation(pointer);
    void *next = realloc(pointer, size);
    if(next && slot) (*slot)->pointer = next;
    /* An existing allocation owned by another result keeps its owner. */
    return next;
}
char *CompilerStrdup(const char *text)
{
    size_t size = strlen(text) + 1;
    char *copy = CompilerMalloc(size);
    if(copy) memcpy(copy, text, size);
    return copy;
}
void CompilerFree(void *pointer) { if(pointer) { forget(pointer); free(pointer); } }
FILE *CompilerFopen(const char *path, const char *mode)
{
    FILE *file = fopen(path, mode);
    if(!track(file, 1)) { if(file) fclose(file); return NULL; }
    return file;
}
int CompilerFclose(FILE *file) { forget(file); return fclose(file); }
void CompilerMemoryEnd(CompilerMemory *memory, int success)
{
    if(!success) {
        /* Result destructors can release both tracked storage and allocations
         * supplied by an import resolver. Run them before raw temporaries. */
        for(;;) {
            Allocation *item = memory->items;
            while(item && !item->destroy) item = item->next;
            if(!item) break;
            void (*destroy)(void *) = item->destroy;
            void *pointer = item->pointer;
            item->destroy = NULL;
            destroy(pointer);
        }
    }
    active = memory->parent;
    current = memory->resume;
    recovery_depth--;
    while(memory->items) {
        Allocation *item = memory->items; memory->items = item->next;
        if(!success) {
            if(item->stream) fclose(item->pointer);
            else free(item->pointer);
        }
        free(item);
    }
    free(memory);
}
