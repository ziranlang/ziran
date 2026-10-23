#include "zir_context.h"
#include <stdlib.h>
#include <stdatomic.h>

typedef struct Storage {
    void *value;
    void (*destroy)(void *);
} Storage;
struct CompilerContext {
    Storage storage[COMPILER_STORAGE_COUNT];
    atomic_flag lock;
    atomic_size_t references;
    uint64_t identity;
};
static CompilerContext shared = {.lock = ATOMIC_FLAG_INIT, .identity = 1, .references = ATOMIC_VAR_INIT(1)};
static atomic_uint_fast64_t next_identity = ATOMIC_VAR_INIT(2);
static _Thread_local CompilerContext *selected;
static _Thread_local uint64_t storage_identity;
static _Thread_local void *storage_hints[COMPILER_STORAGE_COUNT];

CompilerContext *CompilerContextCurrent(void) { return selected; }
CompilerContext *CompilerContextSelect(CompilerContext *context)
{
    CompilerContext *previous = selected; selected = context; return previous;
}
uint64_t CompilerContextIdentity(void) { return selected ? selected->identity : 1; }
CompilerContext *CompilerContextOpen(void)
{
    CompilerContext *context = calloc(1, sizeof(*context));
    if(context) {
        atomic_flag_clear(&context->lock);
        atomic_init(&context->references, 1);
        context->identity = atomic_fetch_add(&next_identity, 1);
    }
    return context;
}
void CompilerContextRetain(CompilerContext *context)
{
    if(context) atomic_fetch_add(&context->references, 1);
}
static void destroy_context(CompilerContext *context);
void CompilerContextRelease(CompilerContext *context)
{
    if(context && atomic_fetch_sub(&context->references, 1) == 1)
        destroy_context(context);
}
void CompilerContextDispose(CompilerContext *context)
{
    /* Release the creating owner's reference. The last program or linked
     * copy releases the final reference without a close/release race. */
    CompilerContextRelease(context);
}

void *CompilerContextStorage(CompilerContext *context, CompilerStorage key,
                            size_t size, void (*destroy)(void *))
{
    if(!context) context = &shared;
    if(storage_identity != context->identity) {
        for(int i = 0; i < COMPILER_STORAGE_COUNT; i++) storage_hints[i] = NULL;
        storage_identity = context->identity;
    }
    if(storage_hints[key]) return storage_hints[key];
    while(atomic_flag_test_and_set_explicit(&context->lock, memory_order_acquire)) {}
    Storage *slot = &context->storage[key];
    if(!slot->value) { slot->value = calloc(1, size); slot->destroy = destroy; }
    void *value = slot->value;
    atomic_flag_clear_explicit(&context->lock, memory_order_release);
    if(!value) abort();
    storage_hints[key] = value;
    return value;
}
static void destroy_context(CompilerContext *context)
{
    if(selected == context) selected = NULL;
    for(int i = COMPILER_STORAGE_COUNT - 1; i >= 0; i--) {
        Storage *slot = &context->storage[i];
        if(slot->value) {
            if(slot->destroy) slot->destroy(slot->value);
            free(slot->value);
        }
    }
    free(context);
}
int CompilerContextClose(CompilerContext *context)
{
    if(!context || context == selected || atomic_load(&context->references) != 1) return 0;
    CompilerContextRelease(context);
    return 1;
}

struct CompilerScratch {
    const void *key;
    void (*destroy)(void *);
    void *items[16];
    int count;
    atomic_flag lock;
    CompilerScratch *next;
};
typedef struct ScratchPools { CompilerScratch *buckets[256]; atomic_flag lock; } ScratchPools;
static void free_scratch_pools(void *value)
{
    ScratchPools *pools = value;
    for(int b = 0; b < 256; b++) while(pools->buckets[b]) {
        CompilerScratch *pool = pools->buckets[b]; pools->buckets[b] = pool->next;
        while(pool->count) pool->destroy(pool->items[--pool->count]);
        free(pool);
    }
}
CompilerScratch *CompilerContextScratch(const void *key, void (*destroy)(void *))
{
    ScratchPools *pools = CompilerContextStorage(selected, COMPILER_SCRATCH, sizeof(*pools), free_scratch_pools);
    while(atomic_flag_test_and_set_explicit(&pools->lock, memory_order_acquire)) {}
    size_t hash = ((uintptr_t)key >> 3) & 255;
    CompilerScratch *pool = pools->buckets[hash];
    while(pool && pool->key != key) pool = pool->next;
    if(!pool) {
        pool = calloc(1, sizeof(*pool));
        if(!pool) abort();
        pool->key = key; pool->destroy = destroy;
        atomic_flag_clear(&pool->lock);
        pool->next = pools->buckets[hash]; pools->buckets[hash] = pool;
    }
    atomic_flag_clear_explicit(&pools->lock, memory_order_release);
    return pool;
}
CompilerScratch *CompilerScratchCached(CompilerScratchCache *cache,
                                      const void *key, void (*destroy)(void *))
{
    uint64_t identity = CompilerContextIdentity();
    if(cache->identity != identity) {
        cache->pool = CompilerContextScratch(key, destroy);
        cache->identity = identity;
    }
    return cache->pool;
}
void *CompilerScratchTake(CompilerScratch *scratch)
{
    while(atomic_flag_test_and_set_explicit(&scratch->lock, memory_order_acquire)) {}
    void *value = scratch->count ? scratch->items[--scratch->count] : NULL;
    atomic_flag_clear_explicit(&scratch->lock, memory_order_release);
    return value;
}
int CompilerScratchPut(CompilerScratch *scratch, void *value)
{
    while(atomic_flag_test_and_set_explicit(&scratch->lock, memory_order_acquire)) {}
    int accepted = scratch->count < 16;
    if(accepted) scratch->items[scratch->count++] = value;
    atomic_flag_clear_explicit(&scratch->lock, memory_order_release);
    return accepted;
}
