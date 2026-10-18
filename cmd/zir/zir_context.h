#ifndef ZIR_CONTEXT_H
#define ZIR_CONTEXT_H
#include <stddef.h>
#include <stdint.h>

typedef struct CompilerContext CompilerContext;
/* Contexts own immutable spellings and derived compiler caches. Select one on
 * each compiling/executing thread. Programs retain their context; closing a
 * context with live programs fails. Restore the previous selection before
 * closing. A context may be shared by immutable programs on multiple threads. */
CompilerContext *CompilerContextOpen(void);
CompilerContext *CompilerContextSelect(CompilerContext *context);
CompilerContext *CompilerContextCurrent(void);
int CompilerContextClose(CompilerContext *context);
uint64_t CompilerContextIdentity(void);
void CompilerContextRetain(CompilerContext *context);
void CompilerContextRelease(CompilerContext *context);
/* Internal result-owner release: retained copies defer final destruction. */
void CompilerContextDispose(CompilerContext *context);

typedef enum CompilerStorage {
    COMPILER_SOURCES, COMPILER_TEXTS, COMPILER_PARAMETERS,
    COMPILER_FIELDS, COMPILER_ENUMS, COMPILER_SYMBOLS, COMPILER_EXPRESSIONS,
    COMPILER_NATIVE_IDENTITIES, COMPILER_OUTPUTS, COMPILER_SCRATCH,
    COMPILER_STORAGE_COUNT
} CompilerStorage;
void *CompilerContextStorage(CompilerContext *context, CompilerStorage key,
                            size_t size, void (*destroy)(void *));
typedef struct CompilerScratch CompilerScratch;
CompilerScratch *CompilerContextScratch(const void *key, void (*destroy)(void *));
void *CompilerScratchTake(CompilerScratch *scratch);
int CompilerScratchPut(CompilerScratch *scratch, void *value);
#endif
