#include "zir_memory.h"
#include "zir.h"
#include "zir_diagnostic.h"
#include <stdarg.h>
#include <stdatomic.h>

/* Every span names its file, and the compiler makes many spans, so each
 * path is stored once here and a span keeps its number. Number 0 is the
 * empty path, so a zeroed span has none. */
typedef struct SourceFiles {
    char **paths;
    int count;
    int capacity;
    int *slots; /* open addressing over path numbers; 0 is an empty slot */
    size_t slot_count;
    atomic_int lock;
} SourceFiles;
static void free_source_files(void *value)
{
    SourceFiles *files = value;
    for(int i = 1; i < files->count; i++) free(files->paths[i]);
    free(files->paths); free(files->slots);
}
static SourceFiles *source_files_in(CompilerContext *context)
{
    return CompilerContextStorage(context, COMPILER_SOURCES, sizeof(SourceFiles), free_source_files);
}

/* Each context's tables have their own lock, taken once a table is found:
 * bundles opening on separate threads read into separate contexts and never
 * wait for each other. */
static void
lock_table(atomic_int *lock)
{
    while(atomic_exchange_explicit(lock, 1, memory_order_acquire)) {}
}

static void
unlock_table(atomic_int *lock)
{
    atomic_store_explicit(lock, 0, memory_order_release);
}

static size_t
source_file_slot(SourceFiles *files, const char *path)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for(const unsigned char *p = (const unsigned char *)path; *p; p++) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }
    size_t slot = (size_t)hash & (files->slot_count - 1);
    while(files->slots[slot] != 0 &&
          strcmp(files->paths[files->slots[slot]], path) != 0)
        slot = (slot + 1) & (files->slot_count - 1);
    return slot;
}

static int
SourceFile_persistent(const char *path)
{
    if(path == NULL || *path == '\0')
        return 0;
    SourceFiles *files = source_files_in(CompilerContextCurrent());
    lock_table(&files->lock);
    if((size_t)files->count * 2 >= files->slot_count) {
        size_t old_count = files->slot_count;
        int *old_slots = files->slots;
        files->slot_count = old_count ? old_count * 2 : 256;
        files->slots = calloc(files->slot_count, sizeof(int));
        if(files->slots == NULL) {
            DiagnosticOutOfMemory();
            exit(1);
        }
        for(size_t i = 0; i < old_count; i++)
            if(old_slots[i] != 0)
                files->slots[source_file_slot(files,
                    files->paths[old_slots[i]])] = old_slots[i];
        free(old_slots);
    }
    if(files->count == 0)
        files->count = 1;
    size_t slot = source_file_slot(files, path);
    if(files->slots[slot] != 0) {
        int file = files->slots[slot];
        unlock_table(&files->lock);
        return file;
    }
    if(files->count >= files->capacity) {
        int capacity = files->capacity ? files->capacity * 2 : 64;
        char **paths = realloc(files->paths, (size_t)capacity * sizeof(*paths));
        if(paths == NULL) {
            DiagnosticOutOfMemory();
            exit(1);
        }
        files->paths = paths;
        files->capacity = capacity;
    }
    char *copy = strdup(path);
    if(copy == NULL) {
        DiagnosticOutOfMemory();
        exit(1);
    }
    files->paths[files->count] = copy;
    files->slots[slot] = files->count;
    int file = files->count++;
    unlock_table(&files->lock);
    return file;
}

const char *
SpanPath(ZirSourceSpan span)
{
    SourceFiles *files = source_files_in(span.context);
    lock_table(&files->lock);
    const char *path = span.file > 0 && span.file < files->count ? files->paths[span.file] : "";
    unlock_table(&files->lock);
    return path;
}

/* Statement and expression text is immutable and shared. Each distinct
 * text is stored once, packed into blocks owned by the compilation
 * context, so copied nodes share it and reloading a program adds nothing.
 * Text keeps the ZIR_TEXT_MAX limit node buffers always had. */
typedef struct KeptBlock { struct KeptBlock *next; char data[]; } KeptBlock;
typedef struct KeptTexts {
    const char **texts;
    size_t count;
    size_t slot_count;
    struct KeptBlock *blocks;
    char *block;
    size_t block_used;
    size_t block_size;
    atomic_int lock;
} KeptTexts;
static void free_kept_texts(void *value)
{
    KeptTexts *texts = value;
    while(texts->blocks) {
        KeptBlock *block = texts->blocks; texts->blocks = block->next; free(block);
    }
    free(texts->texts);
}
static KeptTexts *context_texts(void)
{
    return CompilerContextStorage(CompilerContextCurrent(), COMPILER_TEXTS, sizeof(KeptTexts), free_kept_texts);
}
static const char empty_text[] = "";

/* Canonical return pointers remain immutable and alive after table growth or
 * individual program destruction within their live context. Other input addresses are hints only: compare current
 * bytes with the kept copy before reusing it. Keep addresses as integers so a
 * caller can free/reuse a buffer without leaving an expired pointer value. */
typedef struct KeptText {
    uintptr_t input;
    const char *text;
    size_t length;
} KeptText;
enum { KEPT_TEXT_CACHE_SIZE = 128 };
static _Thread_local KeptText kept_text_cache[KEPT_TEXT_CACHE_SIZE];
static _Thread_local uint64_t kept_text_cache_context;

static KeptText *
kept_text_cache_slot(const char *text)
{
    uint64_t identity = CompilerContextIdentity();
    if(kept_text_cache_context != identity) {
        memset(kept_text_cache, 0, sizeof(kept_text_cache));
        kept_text_cache_context = identity;
    }
    uintptr_t hash = (uintptr_t)text;
    hash ^= hash >> 7;
    hash ^= hash >> 13;
    return &kept_text_cache[hash & (KEPT_TEXT_CACHE_SIZE - 1)];
}

static const char *
kept_text_cached(const char *text, size_t capacity)
{
    KeptText *entry = kept_text_cache_slot(text);
    if(entry->input != (uintptr_t)text || !entry->text || entry->length >= capacity)
        return NULL;
    if(entry->text == text) {
        ProfileCount("kept_text_pointer");
        return entry->text;
    }
    /* Include the terminator and respect the bounded KeepName/Parameters
     * input contract, including buffers with no terminator within capacity. */
    if(strncmp(entry->text, text, entry->length + 1)) return NULL;
    ProfileCount("kept_text_compare");
    return entry->text;
}

static void
remember_kept_text(const char *input, const char *text, size_t length)
{
    *kept_text_cache_slot(text) = (KeptText){(uintptr_t)text, text, length};
    *kept_text_cache_slot(input) = (KeptText){(uintptr_t)input, text, length};
}

static uint64_t
text_hash(const char *text, size_t length)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for(size_t i = 0; i < length; i++) {
        hash ^= (unsigned char)text[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static size_t
kept_text_slot(KeptTexts *texts, const char *text, size_t length)
{
    size_t slot = (size_t)text_hash(text, length) & (texts->slot_count - 1);
    while(texts->texts[slot] != NULL &&
          (strncmp(texts->texts[slot], text, length) != 0 ||
           texts->texts[slot][length] != '\0'))
        slot = (slot + 1) & (texts->slot_count - 1);
    return slot;
}

/* Scratch memory the compiler cannot work without; there is no useful
 * way to continue when it is missing. */
void *
AllocateOrExit(size_t size)
{
    ProfileAllocation(size);
    void *memory = malloc(size);
    if(memory == NULL) {
        DiagnosticOutOfMemory();
        exit(1);
    }
    return memory;
}

static void
kept_text_failed(void)
{
    DiagnosticOutOfMemory();
    exit(1);
}

/* TEXT kept, cut to fit a CAPACITY-byte buffer as copy_text would. */
static const char *keep_text(const char *text, size_t length);

static const char *
keep_cut(const char *text, size_t capacity)
{
    if(text == NULL || *text == '\0')
        return empty_text;
    const char *cached = kept_text_cached(text, capacity);
    if(cached) return cached;
    size_t length = strnlen(text, capacity);
    if(length < capacity)
        return keep_text(text, length);
    char *cut = AllocateOrExit(capacity);
    memcpy(cut, text, capacity - 1);
    cut[capacity - 1] = '\0';
    const char *kept = keep_text(cut, capacity - 1);
    free(cut);
    return kept;
}

static const char *
KeepName_persistent(const char *text)
{
    return keep_cut(text, ZIR_NAME_MAX);
}

static const char *
KeepParameters_persistent(const char *text)
{
    return keep_cut(text, ZIR_TEXT_MAX);
}

const char *
KeepNameFormat(const char *format, ...)
{
    char name[ZIR_NAME_MAX];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(name, sizeof(name), format, arguments);
    va_end(arguments);
    return KeepText(name);
}

static const char *
keep_text(const char *text, size_t length)
{
    ProfileCount("kept_text_lookup");
    /* VM instances share immutable spellings, but insertion and table growth
     * still mutate the context's table. Publish complete entries together. */
    KeptTexts *texts = context_texts();
    lock_table(&texts->lock);
    if(texts->count * 2 >= texts->slot_count) {
        size_t old_count = texts->slot_count;
        const char **old_texts = texts->texts;
        texts->slot_count = old_count ? old_count * 2 : 4096;
        texts->texts = calloc(texts->slot_count, sizeof(*texts->texts));
        if(texts->texts == NULL)
            kept_text_failed();
        for(size_t i = 0; i < old_count; i++)
            if(old_texts[i] != NULL)
                texts->texts[kept_text_slot(texts, old_texts[i],
                                                      strlen(old_texts[i]))] = old_texts[i];
        free(old_texts);
    }
    size_t slot = kept_text_slot(texts, text, length);
    if(texts->texts[slot] != NULL) {
        const char *kept = texts->texts[slot];
        unlock_table(&texts->lock);
        remember_kept_text(text, kept, length);
        return kept;
    }
    if(texts->block == NULL ||
       texts->block_used + length + 1 > texts->block_size) {
        texts->block_size = length + 1 > 65536 ? length + 1 : 65536;
        KeptBlock *block = malloc(sizeof(*block) + texts->block_size);
        if(block == NULL) kept_text_failed();
        block->next = texts->blocks;
        texts->blocks = block;
        texts->block = block->data;
        texts->block_used = 0;
    }
    char *copy = texts->block + texts->block_used;
    memcpy(copy, text, length);
    copy[length] = '\0';
    texts->block_used += length + 1;
    texts->texts[slot] = copy;
    texts->count++;
    unlock_table(&texts->lock);
    remember_kept_text(text, copy, length);
    return copy;
}

static const char *
KeepText_persistent(const char *text)
{
    if(text == NULL || *text == '\0') return empty_text;
    const char *cached = kept_text_cached(text, SIZE_MAX);
    if(cached) return cached;
    return keep_text(text, strlen(text));
}

ZirSourceSpan
Span(const char *path, int line, int column)
{
    return SpanEnd(path, line, column, line, column);
}

ZirSourceSpan
SpanEnd(const char *path, int line, int column, int end_line, int end_column)
{
    ZirSourceSpan span;

    memset(&span, 0, sizeof(span));
    span.context = CompilerContextCurrent();
    span.file = SourceFile(path);
    span.line = line;
    span.column = column;
    span.end_line = end_line;
    span.end_column = end_column;
    return span;
}

int SourceFile(const char *path)
{
    CompilerMemory *memory = CompilerMemoryPause();
    int result = SourceFile_persistent(path);
    CompilerMemoryResume(memory);
    return result;
}

const char *KeepText(const char *text)
{
    CompilerMemory *memory = CompilerMemoryPause();
    const char *result = KeepText_persistent(text);
    CompilerMemoryResume(memory);
    return result;
}

const char *KeepName(const char *text)
{
    CompilerMemory *memory = CompilerMemoryPause();
    const char *result = KeepName_persistent(text);
    CompilerMemoryResume(memory);
    return result;
}

const char *KeepParameters(const char *text)
{
    CompilerMemory *memory = CompilerMemoryPause();
    const char *result = KeepParameters_persistent(text);
    CompilerMemoryResume(memory);
    return result;
}
