#ifndef ZIR_MEMORY_H
#define ZIR_MEMORY_H
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

typedef struct CompilerMemory CompilerMemory;
/* A recoverable frontend operation owns its temporary allocations and streams.
 * Successful operations transfer ownership to their ordinary result owners. */
CompilerMemory *CompilerMemoryBegin(void);
void CompilerMemoryEnd(CompilerMemory *memory, int success);
CompilerMemory *CompilerMemoryPause(void);
void CompilerMemoryResume(CompilerMemory *memory);
int CompilerMemoryRecovering(void);
void *CompilerOwn(void *pointer);
void CompilerMemoryCleanup(void *pointer, void (*destroy)(void *));
void *CompilerMalloc(size_t size);
void *CompilerCalloc(size_t count, size_t size);
void *CompilerRealloc(void *pointer, size_t size);
char *CompilerStrdup(const char *text);
void CompilerFree(void *pointer);
FILE *CompilerFopen(const char *path, const char *mode);
int CompilerFclose(FILE *file);
#ifndef ZIR_MEMORY_IMPLEMENTATION
#define malloc CompilerMalloc
#define calloc CompilerCalloc
#define realloc CompilerRealloc
#define strdup CompilerStrdup
#define free CompilerFree
#define fopen CompilerFopen
#define fclose CompilerFclose
#endif
#endif
