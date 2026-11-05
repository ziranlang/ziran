#include "zir_document.h"
#include "zir_sha256.h"
#include "zir_memory.h"
#include "zir_diagnostic.h"
#include <stdlib.h>
#include <string.h>

typedef struct DocumentStore { ZirDocument *head; size_t bytes, count; } DocumentStore;

static void free_documents(void *value)
{
    ZirDocument *item = ((DocumentStore *)value)->head;
    while(item) {
        ZirDocument *next = item->next;
        free(item->line_offsets);
        free(item);
        item = next;
    }
}
static DocumentStore *documents(void)
{
    return CompilerContextStorage(CompilerContextCurrent(), COMPILER_DOCUMENTS,
                                  sizeof(DocumentStore), free_documents);
}
void DocumentDigest(const void *bytes, size_t count, char hex[65])
{
    static const char digits[] = "0123456789abcdef";
    unsigned char digest[32];
    Sha256 hash;
    Sha256Begin(&hash);
    Sha256Add(&hash, bytes, count);
    Sha256End(&hash, digest);
    for(size_t i = 0; i < sizeof(digest); i++) {
        hex[i*2] = digits[digest[i] >> 4];
        hex[i*2+1] = digits[digest[i] & 15];
    }
    hex[64] = 0;
}
static ZirDocument *find_document(const char *path)
{
    if(path == NULL || !*path) return NULL;
    for(ZirDocument *item = documents()->head; item; item = item->next) {
        ZirSourceSpan source = {.context=item->context, .file=item->file};
        if(!strcmp(SpanPath(source), path)) return item;
    }
    return NULL;
}
const ZirDocument *DocumentFind(const char *path)
{
    /* A lookup must not intern missing paths in the source-file index. */
    return find_document(path);
}
const ZirDocument *DocumentForSpan(const ZirModule *module, ZirSourceSpan span)
{
    if(!module || !module->document) return NULL;
    if(!strcmp(SpanPath(span), module->source_path)) return module->document;
    char path[ZIR_PATH_MAX];
    const char *source=SpanPath(span);
    if(snprintf(path, sizeof(path), "%s%s%s", source[0]=='/' ? "" : module->source_root,
        source[0]=='/' || !module->source_root[0] ? "" : "/", source) >= (int)sizeof(path)) return NULL;
    CompilerContext *previous=CompilerContextSelect(module->context);
    const ZirDocument *document=DocumentFind(path);
    CompilerContextSelect(previous); return document;
}
void DocumentMarkRewritten(const char *path)
{
    ZirDocument *document = find_document(path);
    if(document) document->rewritten = 1;
}

const ZirDocument *DocumentRecord(const char *path, const char *bytes, size_t count)
{
    CompilerMemory *memory = CompilerMemoryPause();
    char hash[65];
    DocumentDigest(bytes, count, hash);
    const ZirDocument *previous = DocumentFind(path);
    if(previous && previous->bytes == count && !strcmp(previous->sha256, hash)) {
        CompilerMemoryResume(memory);
        return previous;
    }
    DocumentStore *store = documents();
    /* Hash/offset history is bounded even when a host reuses a context for
     * many loads. Large documents retain identity without an offset table. */
    if(store->count >= 4096) { CompilerMemoryResume(memory); return NULL; }
    size_t lines = 1;
    for(size_t i = 0; i < count; i++) if(bytes[i] == '\n') lines++;
    ZirDocument *item = calloc(1, sizeof(*item));
    if(item == NULL) { DiagnosticOutOfMemory(); exit(1); }
    size_t offset_bytes = lines <= SIZE_MAX / sizeof(size_t) ? lines * sizeof(size_t) : SIZE_MAX;
    if(offset_bytes <= 1024 * 1024 && offset_bytes <= 8 * 1024 * 1024 - store->bytes) {
        item->line_offsets = malloc(offset_bytes);
        if(item->line_offsets == NULL) { free(item); DiagnosticOutOfMemory(); exit(1); }
        item->line_offsets[0] = 0;
        size_t line = 1;
        for(size_t i = 0; i < count; i++)
            if(bytes[i] == '\n') item->line_offsets[line++] = i + 1;
        store->bytes += offset_bytes;
    }
    store->count++;
    item->context = CompilerContextCurrent();
    item->file = SourceFile(path);
    memcpy(item->sha256, hash, sizeof(hash));
    item->bytes = count;
    item->line_count = lines;
    item->next = documents()->head;
    documents()->head = item;
    CompilerMemoryResume(memory);
    return item;
}
int DocumentOffset(const ZirDocument *document, int line, int column, size_t *offset)
{
    if(document == NULL || document->rewritten || document->line_offsets == NULL || line < 1 || column < 1 ||
       (size_t)line > document->line_count) return 0;
    size_t begin = document->line_offsets[line - 1];
    size_t end = (size_t)line < document->line_count ?
        document->line_offsets[line] - 1 : document->bytes;
    if((size_t)column - 1 > end - begin) return 0;
    *offset = begin + (size_t)column - 1;
    return 1;
}
