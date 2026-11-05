#ifndef ZIR_DOCUMENT_H
#define ZIR_DOCUMENT_H
#include "zir.h"
#include <stddef.h>

/* Immutable raw-document identity, captured before source rewriting. It is
 * compiler-context data, never reconstructed from saved IR or serialized. */
typedef struct ZirDocument {
    struct ZirDocument *next;
    CompilerContext *context;
    int file;
    char sha256[65];
    size_t bytes;
    size_t line_count;
    int rewritten; /* checked coordinates need a rewrite map to name raw bytes */
    size_t *line_offsets;
} ZirDocument;

const ZirDocument *DocumentRecord(const char *path, const char *bytes, size_t count);
const ZirDocument *DocumentFind(const char *path);
const ZirDocument *DocumentForSpan(const ZirModule *module, ZirSourceSpan span);
void DocumentMarkRewritten(const char *path);
/* Byte coordinates are one-based, half-open, as checked source spans are.
 * Refuse unavailable/out-of-document coordinates rather than clamp them. */
int DocumentOffset(const ZirDocument *document, int line, int column, size_t *offset);
void DocumentDigest(const void *bytes, size_t count, char hex[65]);
#endif
