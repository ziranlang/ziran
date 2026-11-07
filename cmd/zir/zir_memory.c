#define ZIR_MEMORY_IMPLEMENTATION
#include "zir_memory.h"
#include "zir_thread.h"
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
_Thread_local CompilerThread CompilerThreadState;

CompilerMemory *CompilerMemoryBegin(void)
{
    CompilerMemory *memory = calloc(1, sizeof(*memory));
    if(memory) {
        CompilerThread *thread = &CompilerThreadState;
        memory->parent = thread->active_memory; memory->resume = thread->memory;
        thread->active_memory = thread->memory = memory; thread->recovery_depth++;
    }
    return memory;
}
CompilerMemory *CompilerMemoryPause(void) { return CompilerThreadPause(&CompilerThreadState); }
void CompilerMemoryResume(CompilerMemory *memory) { CompilerThreadResume(&CompilerThreadState, memory); }
int CompilerMemoryRecovering(void) { return CompilerThreadState.recovery_depth != 0; }

static Allocation **find_allocation(void *pointer)
{
    for(CompilerMemory *memory = CompilerThreadState.active_memory; memory; memory = memory->parent)
        for(Allocation **item = &memory->items; *item; item = &(*item)->next)
            if((*item)->pointer == pointer) return item;
    return NULL;
}
static int track(void *pointer, int stream)
{
    if(!pointer) return 1;
    CompilerMemory *current = CompilerThreadState.memory;
    if(!current) return 1;
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
    CompilerThread *thread = &CompilerThreadState;
    thread->active_memory = memory->parent;
    thread->memory = memory->resume;
    thread->recovery_depth--;
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
