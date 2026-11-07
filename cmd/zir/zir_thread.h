#ifndef ZIR_THREAD_H
#define ZIR_THREAD_H
#include "zir_context.h"

/* Each thread's compiler state is one thread-local record. Where TLS is
 * emulated, as on 32-bit Android before API 29, every thread-local access is
 * a call, so the recoverable memory scope, the selected context, its storage
 * hints and recently kept texts share one record. Hot entry points find it
 * once and pass it down. */
typedef struct CompilerKeptText {
    uintptr_t input;
    const char *text;
    size_t length;
} CompilerKeptText;
enum { COMPILER_KEPT_TEXTS = 128 };
typedef struct CompilerThread {
    struct CompilerMemory *memory, *active_memory;
    unsigned recovery_depth;
    CompilerContext *context;
    uint64_t storage_identity;
    void *storage_hints[COMPILER_STORAGE_COUNT];
    /* KeepText inputs and their kept copies in the context kept_identity. */
    uint64_t kept_identity;
    CompilerKeptText kept[COMPILER_KEPT_TEXTS];
} CompilerThread;
extern _Thread_local CompilerThread CompilerThreadState;

uint64_t CompilerThreadIdentity(const CompilerThread *thread);
void *CompilerThreadStorage(CompilerThread *thread, CompilerStorage key,
                            size_t size, void (*destroy)(void *));

static inline struct CompilerMemory *CompilerThreadPause(CompilerThread *thread)
{
    struct CompilerMemory *memory = thread->memory;
    thread->memory = NULL;
    return memory;
}
static inline void CompilerThreadResume(CompilerThread *thread, struct CompilerMemory *memory)
{
    thread->memory = memory;
}
/* Select CONTEXT and pause the recoverable memory scope for one operation on
 * persistent tables; CompilerThreadLeave restores both. */
typedef struct CompilerScope {
    CompilerContext *context;
    struct CompilerMemory *memory;
} CompilerScope;
static inline CompilerScope CompilerThreadEnter(CompilerThread *thread, CompilerContext *context)
{
    CompilerScope scope = {thread->context, CompilerThreadPause(thread)};
    thread->context = context;
    return scope;
}
static inline void CompilerThreadLeave(CompilerThread *thread, CompilerScope scope)
{
    CompilerThreadResume(thread, scope.memory);
    thread->context = scope.context;
}
#endif
