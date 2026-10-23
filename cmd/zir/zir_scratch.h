#ifndef ZIR_SCRATCH_H
#define ZIR_SCRATCH_H

/* Heap-backed compiler frames share one wrapper. Recoverable parser calls
 * keep their allocations in the active transaction; ordinary backend calls
 * reuse their context's pool. MODE preserves the caller's existing policy. */
#define COMPILER_SCRATCH_TAKE_RECOVERABLE \
    !CompilerMemoryRecovering() ? CompilerScratchTake(scratch_hint) : NULL
#define COMPILER_SCRATCH_TAKE_POOLED CompilerScratchTake(scratch_hint)
#define COMPILER_SCRATCH_RELEASE_RECOVERABLE \
    CompilerMemoryRecovering() || !CompilerScratchPut(scratch_hint, buffers)
#define COMPILER_SCRATCH_RELEASE_POOLED !CompilerScratchPut(scratch_hint, buffers)

#define COMPILER_SCRATCH_BEGIN(Type, Mode, Destroy) \
    static const char scratch_key; \
    static _Thread_local CompilerScratchCache scratch_cache; \
    CompilerScratch *scratch_hint = CompilerScratchCached(&scratch_cache, \
        &scratch_key, Destroy); \
    Type *buffers = COMPILER_SCRATCH_TAKE_##Mode; \
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));

#define COMPILER_SCRATCH_END(Mode) \
    if(COMPILER_SCRATCH_RELEASE_##Mode) \
        free(buffers);

#define COMPILER_SCRATCH_RETURN(Storage, Result, Name, Parameters, Type, Mode, Destroy, Call) \
Storage Result Name Parameters \
{ \
    COMPILER_SCRATCH_BEGIN(Type, Mode, Destroy) \
    Result returned = Call; \
    COMPILER_SCRATCH_END(Mode) \
    return returned; \
}

#define COMPILER_SCRATCH_VOID(Storage, Name, Parameters, Type, Mode, Destroy, Call) \
Storage void Name Parameters \
{ \
    COMPILER_SCRATCH_BEGIN(Type, Mode, Destroy) \
    Call; \
    COMPILER_SCRATCH_END(Mode) \
}

#endif
