/*
 * zir_c_prune.c - Keeps only the C interface an entry-linked program uses.
 *
 * Each module header of a closed program arrives as marked chunks
 * (see zir_c_prune.h). A declaration stays in its header when another
 * module's source or another kept header chunk names it, and the module's
 * own source takes the rest. An inline copy stays only for a procedure that
 * another module calls. The emitter keeps C names unique across a program,
 * so matching words is a safe over-approximation: a false match keeps a
 * declaration, and no use is ever missed.
 */
#include "zir_c_prune.h"
#include "zir.h"
#include "zir_emit.h"
#include "zir_simd.h"
#include "zir_stream.h"
#include "zir_memory.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* One marked chunk of a header. */
typedef struct PruneChunk {
    int kind;
    char *names;            /* the C names it declares, space-separated */
    size_t begin, end;      /* its text in the header, after its mark */
    unsigned char kept;     /* it stays in the header */
    unsigned char moved;    /* it goes to the module's source */
} PruneChunk;

typedef struct PruneChunks {
    PruneChunk *items;
    int count, capacity;
    size_t top;             /* the header text before the first mark */
    unsigned char written;  /* the header keeps a declaration */
    unsigned char inlines;  /* the header keeps an inline copy */
} PruneChunks;

/* An identifier that the program's generated text names. */
typedef struct PruneWord {
    const char *text;
    uint32_t length, hash;
    int source_owner;            /* the first module whose source names it */
    unsigned char source_shared; /* another module's source names it too */
    unsigned char header_use;    /* a kept header chunk names it */
    unsigned stamp;              /* the module being composed names it */
} PruneWord;

typedef struct PruneWords {
    PruneWord *items;
    size_t capacity, count;      /* the capacity is zero or a power of two */
    unsigned stamp;
} PruneWords;

enum { NOTE_SOURCE, NOTE_HEADER, NOTE_STAMP };

void
PruneMark(FILE *out, int kind, const char *names)
{
    fprintf(out, "%c%c%s%s\n", PRUNE_MARK, kind, names ? " " : "", names ? names : "");
}

int
HeaderInlineSection(FILE *out, const char *stem, const char *guard,
                    int vector_operations, const char *declarations,
                    const char *bodies, struct ZirSimdUses *simd)
{
    size_t size = strlen(RUNTIME_INCLUDES_MARK) + strlen(NUMBER_HELPERS_MARK) +
                  strlen(declarations) + strlen(bodies) + 1;
    char *section = AllocateOrExit(size);
    snprintf(section, size, "%s%s%s%s", RUNTIME_INCLUDES_MARK, NUMBER_HELPERS_MARK,
             declarations, bodies);
    /* ZIRAN_NO_HEADER_INLINE keeps every call on the external symbol,
     * so link-time interposition such as ld --wrap still sees it. */
    fprintf(out, "\n/* Small procedures other modules inline; %s.c defines them. */\n"
                 "#if !defined(__cplusplus) && !defined(%s_SOURCE) && "
                 "!defined(ZIRAN_NO_HEADER_INLINE)\n",
            stem, guard);
    if(vector_operations)
        fputs("#include \"zir_vec.h\"\n", out);
    int failed = EmitNativeHelpers(out, section, simd) != 0;
    fputs("#endif\n", out);
    free(section);
    return failed ? -1 : 0;
}

static int
word_byte(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static uint32_t
word_hash(const char *text, size_t length)
{
    uint32_t hash = 2166136261u;
    for(size_t i = 0; i < length; i++)
        hash = (hash ^ (unsigned char)text[i]) * 16777619u;
    return hash;
}

static PruneWord *
word_slot(PruneWord *items, size_t capacity, const char *text, size_t length,
          uint32_t hash)
{
    size_t mask = capacity - 1;
    for(size_t i = hash & mask;; i = (i + 1) & mask) {
        PruneWord *word = &items[i];
        if(word->text == NULL ||
           (word->hash == hash && word->length == length &&
            !memcmp(word->text, text, length)))
            return word;
    }
}

static void
words_grow(PruneWords *words)
{
    size_t capacity = words->capacity ? words->capacity * 2 : 4096;
    PruneWord *items = AllocateOrExit(capacity * sizeof(*items));
    memset(items, 0, capacity * sizeof(*items));
    for(size_t i = 0; i < words->capacity; i++) {
        const PruneWord *word = &words->items[i];
        if(word->text != NULL)
            *word_slot(items, capacity, word->text, word->length, word->hash) = *word;
    }
    free(words->items);
    words->items = items;
    words->capacity = capacity;
}

/* The entry for TEXT. ADD creates a missing entry; otherwise it is NULL. */
static PruneWord *
word_entry(PruneWords *words, const char *text, size_t length, int add)
{
    if(add && (words->count + 1) * 2 > words->capacity)
        words_grow(words);
    if(words->capacity == 0)
        return NULL;
    uint32_t hash = word_hash(text, length);
    PruneWord *word = word_slot(words->items, words->capacity, text, length, hash);
    if(word->text != NULL || !add)
        return word->text != NULL ? word : NULL;
    *word = (PruneWord){text, (uint32_t)length, hash, -1, 0, 0, 0};
    words->count++;
    return word;
}

/* The next name in a space-separated list, or NULL after the last one. */
static const char *
next_name(const char *cursor, size_t *length)
{
    while(*cursor == ' ')
        cursor++;
    *length = 0;
    while(cursor[*length] != '\0' && cursor[*length] != ' ')
        (*length)++;
    return *length ? cursor : NULL;
}

static int
name_listed(const char *names, const char *text, size_t length)
{
    size_t found;
    for(const char *name = names; (name = next_name(name, &found)) != NULL; name += found)
        if(found == length && !memcmp(name, text, length))
            return 1;
    return 0;
}

/* The next identifier at or after *CURSOR before END outside comments,
 * literals and marks, or NULL; *CURSOR moves past it. */
static const char *
next_identifier(const char **cursor, const char *end, size_t *length)
{
    const char *p = *cursor;
    while(p < end) {
        if(*p == PRUNE_MARK || (*p == '/' && p + 1 < end && p[1] == '/')) {
            while(p < end && *p != '\n')
                p++;
        } else if(*p == '/' && p + 1 < end && p[1] == '*') {
            for(p += 2; p < end && !(*p == '*' && p + 1 < end && p[1] == '/'); p++) {}
            p = p < end ? p + 2 : end;
        } else if(*p == '"' || *p == '\'') {
            char quote = *p++;
            for(; p < end && *p != quote; p++)
                if(*p == '\\' && p + 1 < end)
                    p++;
            if(p < end)
                p++;
        } else if(word_byte(*p)) {
            const char *start = p;
            while(p < end && word_byte(*p))
                p++;
            if(*start >= '0' && *start <= '9')
                continue;
            *cursor = p;
            *length = (size_t)(p - start);
            return start;
        } else
            p++;
    }
    *cursor = p;
    return NULL;
}

/* Notes each identifier TEXT names. A header chunk does not count its own
 * names as a use of them. */
static void
note_words(PruneWords *words, const char *text, size_t length, int how,
           int module, const char *own_names)
{
    const char *cursor = text, *start;
    size_t size;
    while((start = next_identifier(&cursor, text + length, &size)) != NULL) {
        if(own_names != NULL && name_listed(own_names, start, size))
            continue;
        PruneWord *word = word_entry(words, start, size, 1);
        if(how == NOTE_HEADER)
            word->header_use = 1;
        else if(how == NOTE_STAMP)
            word->stamp = words->stamp;
        else if(word->source_owner < 0)
            word->source_owner = module;
        else if(word->source_owner != module)
            word->source_shared = 1;
    }
}

void
PruneNoteNames(const char *text, struct SimdNames *names)
{
    /* The end is measured once: a generated file can be megabytes long. */
    const char *cursor = text, *end = text + strlen(text), *start;
    size_t size;
    while((start = next_identifier(&cursor, end, &size)) != NULL)
        SimdNamesFind(names, start, size, 1);
}

static void
chunk_add(PruneChunks *chunks, int kind, const char *names, size_t length,
          size_t begin)
{
    if(chunks->count == chunks->capacity) {
        int capacity = chunks->capacity ? chunks->capacity * 2 : 64;
        PruneChunk *items = AllocateOrExit((size_t)capacity * sizeof(*items));
        if(chunks->count)
            memcpy(items, chunks->items, (size_t)chunks->count * sizeof(*items));
        free(chunks->items);
        chunks->items = items;
        chunks->capacity = capacity;
    }
    PruneChunk *chunk = &chunks->items[chunks->count++];
    *chunk = (PruneChunk){kind, AllocateOrExit(length + 1), begin, begin, 0, 0};
    memcpy(chunk->names, names, length);
    chunk->names[length] = '\0';
}

static void
chunk_name(PruneChunk *chunk, const char *names, size_t length)
{
    size_t old = strlen(chunk->names);
    char *joined = AllocateOrExit(old + length + 2);
    memcpy(joined, chunk->names, old);
    joined[old] = ' ';
    memcpy(joined + old + 1, names, length);
    joined[old + 1 + length] = '\0';
    free(chunk->names);
    chunk->names = joined;
}

/* Splits a marked header into its leading text and its chunks. */
static void
parse_chunks(const char *text, PruneChunks *chunks)
{
    size_t length = strlen(text);
    int marked = 0;
    chunks->top = length;
    for(size_t line = 0; line < length;) {
        size_t next = line;
        while(next < length && text[next] != '\n')
            next++;
        size_t names = next - line;
        if(next < length)
            next++;
        if(text[line] == PRUNE_MARK && names >= 2) {
            int kind = (unsigned char)text[line + 1];
            if(!marked)
                chunks->top = line;
            marked = 1;
            if(kind == PRUNE_NAME && chunks->count > 0)
                chunk_name(&chunks->items[chunks->count - 1], text + line + 2, names - 2);
            else {
                if(chunks->count > 0)
                    chunks->items[chunks->count - 1].end = line;
                chunk_add(chunks, kind, text + line + 2, names - 2, next);
            }
        }
        line = next;
    }
    if(chunks->count > 0)
        chunks->items[chunks->count - 1].end = length;
}

/* Writes TEXT[BEGIN, END) without its mark lines. */
static void
write_text(FILE *out, const char *text, size_t begin, size_t end)
{
    while(begin < end) {
        size_t next = begin;
        while(next < end && text[next] != '\n')
            next++;
        if(next < end)
            next++;
        if(text[begin] != PRUNE_MARK)
            fwrite(text + begin, 1, next - begin, out);
        begin = next;
    }
}

static int
declares(int kind)
{
    return kind == PRUNE_DECLARATION || kind == PRUNE_ROOT ||
           kind == PRUNE_INLINE || kind == PRUNE_INLINE_BODY;
}

/* Whether another module's source, or a kept header chunk, names one of
 * CHUNK's declarations. */
static int
chunk_wanted(PruneWords *words, const PruneChunk *chunk, int module)
{
    size_t length;
    for(const char *name = chunk->names; (name = next_name(name, &length)) != NULL;
        name += length) {
        const PruneWord *word = word_entry(words, name, length, 0);
        if(word != NULL &&
           (word->header_use ||
            (word->source_owner >= 0 &&
             (word->source_shared || word->source_owner != module))))
            return 1;
    }
    return 0;
}

/* Whether the text stamped last names one of CHUNK's declarations. */
static int
chunk_stamped(PruneWords *words, const PruneChunk *chunk)
{
    size_t length;
    for(const char *name = chunk->names; (name = next_name(name, &length)) != NULL;
        name += length) {
        const PruneWord *word = word_entry(words, name, length, 0);
        if(word != NULL && word->stamp == words->stamp)
            return 1;
    }
    return 0;
}

/* Whether module B's written header declares a name the stamped text uses. */
static int
header_needed(PruneWords *words, const PruneChunks *chunks, int b)
{
    if(!chunks[b].written)
        return 0;
    for(int i = 0; i < chunks[b].count; i++) {
        const PruneChunk *chunk = &chunks[b].items[i];
        if(chunk->kept && declares(chunk->kind) && chunk_stamped(words, chunk))
            return 1;
    }
    return 0;
}

static void
write_includes(FILE *out, PruneWords *words, const PruneChunks *chunks,
               const ZirCPruneModule *modules, int self, const int *scope, int count)
{
    for(int i = 0; i < count; i++)
        if(scope[i] != self && header_needed(words, chunks, scope[i]))
            fprintf(out, "#include \"%s.h\"\n", modules[scope[i]].stem);
}

static void
keep_chunk(PruneWords *words, const char *header, PruneChunk *chunk)
{
    chunk->kept = 1;
    note_words(words, header + chunk->begin, chunk->end - chunk->begin,
               NOTE_HEADER, -1, chunk->names);
}

/* Stamps the words the kept declarations of a header name, and keeps each
 * helper those declarations use. */
static void
stamp_header(PruneWords *words, const ZirCPruneModule *module, PruneChunks *chunks)
{
    words->stamp++;
    for(int i = 0; i < chunks->count; i++) {
        const PruneChunk *chunk = &chunks->items[i];
        if(chunk->kept && declares(chunk->kind))
            note_words(words, module->header + chunk->begin, chunk->end - chunk->begin,
                       NOTE_STAMP, -1, chunk->names);
    }
    for(int i = 0; i < chunks->count; i++) {
        PruneChunk *chunk = &chunks->items[i];
        if(chunk->kind == PRUNE_HELPER)
            chunk->kept = chunk->names[0] == '\0' || chunk_stamped(words, chunk);
    }
}

static char *
compose_header(PruneWords *words, ZirCPruneModule *modules, PruneChunks *all,
               int self, struct ZirSimdUses *simd)
{
    const ZirCPruneModule *module = &modules[self];
    const PruneChunks *chunks = &all[self];
    const char *header = module->header;
    TextStream stream;
    char *text = NULL;
    size_t size = 0;
    FILE *out = TextStreamOpen(&stream, &text, &size);
    if(out == NULL)
        return NULL;
    fwrite(header, 1, chunks->top, out);
    for(int i = 0; i < chunks->count; i++) {
        const PruneChunk *chunk = &chunks->items[i];
        if(chunk->kind == PRUNE_MODULES)
            write_includes(out, words, all, modules, self, module->header_scope,
                           module->header_scope_count);
        else if(chunk->kind == PRUNE_END && chunks->inlines) {
            TextStream declaration_stream, body_stream;
            char *declarations = NULL, *bodies = NULL;
            size_t declaration_size = 0, body_size = 0;
            FILE *declaration_file = TextStreamOpen(&declaration_stream, &declarations,
                                                    &declaration_size);
            FILE *body_file = TextStreamOpen(&body_stream, &bodies, &body_size);
            if(declaration_file == NULL || body_file == NULL) {
                if(declaration_file != NULL) TextStreamClose(&declaration_stream);
                if(body_file != NULL) TextStreamClose(&body_stream);
                free(declarations);
                free(bodies);
                TextStreamClose(&stream);
                free(text);
                return NULL;
            }
            for(int j = 0; j < chunks->count; j++) {
                const PruneChunk *copy = &chunks->items[j];
                if(copy->kept && copy->kind == PRUNE_INLINE)
                    write_text(declaration_file, header, copy->begin, copy->end);
                else if(copy->kept && copy->kind == PRUNE_INLINE_BODY)
                    write_text(body_file, header, copy->begin, copy->end);
            }
            int failed = TextStreamClose(&declaration_stream) != 0;
            failed |= TextStreamClose(&body_stream) != 0;
            if(!failed)
                failed = HeaderInlineSection(out, module->stem, module->guard,
                                             module->vector_operations, declarations,
                                             bodies, simd) != 0;
            free(declarations);
            free(bodies);
            if(failed) {
                TextStreamClose(&stream);
                free(text);
                return NULL;
            }
        }
        if(chunk->kind == PRUNE_FRAME || chunk->kind == PRUNE_END ||
           (chunk->kept && chunk->kind != PRUNE_INLINE &&
            chunk->kind != PRUNE_INLINE_BODY))
            write_text(out, header, chunk->begin, chunk->end);
    }
    if(TextStreamClose(&stream) != 0) {
        free(text);
        return NULL;
    }
    return text;
}

/* The source with its marks replaced: the inline-source define when the
 * header keeps inline copies, then the headers it reads and the
 * declarations its header moved into it. */
static char *
compose_source(PruneWords *words, ZirCPruneModule *modules, PruneChunks *all,
               int self)
{
    ZirCPruneModule *module = &modules[self];
    PruneChunks *chunks = &all[self];
    const char *source = module->source;
    const char *define = strstr(source, PRUNE_SOURCE_MARK);
    const char *includes = strstr(source, PRUNE_INCLUDES_MARK);
    if(includes == NULL || (define != NULL && define > includes))
        return NULL;
    words->stamp++;
    note_words(words, source, strlen(source), NOTE_STAMP, -1, NULL);
    for(int i = 0; i < chunks->count; i++) {
        PruneChunk *chunk = &chunks->items[i];
        chunk->moved = !chunk->kept && chunk->kind == PRUNE_DECLARATION;
        if(chunk->moved)
            note_words(words, module->header + chunk->begin, chunk->end - chunk->begin,
                       NOTE_STAMP, -1, NULL);
    }
    for(int i = 0; i < chunks->count; i++) {
        PruneChunk *chunk = &chunks->items[i];
        if(chunk->kind == PRUNE_HELPER && !(chunk->kept && chunks->written))
            chunk->moved = chunk->names[0] == '\0' || chunk_stamped(words, chunk);
    }
    TextStream stream;
    char *text = NULL;
    size_t size = 0;
    FILE *out = TextStreamOpen(&stream, &text, &size);
    if(out == NULL)
        return NULL;
    const char *rest = source;
    if(define != NULL) {
        fwrite(rest, 1, (size_t)(define - rest), out);
        if(chunks->inlines)
            fprintf(out, "#define %s_SOURCE 1\n", module->guard);
        rest = define + strlen(PRUNE_SOURCE_MARK);
    }
    fwrite(rest, 1, (size_t)(includes - rest), out);
    if(chunks->written)
        fprintf(out, "#include \"%s.h\"\n", module->stem);
    write_includes(out, words, all, modules, self, module->source_scope,
                   module->source_scope_count);
    for(int i = 0; i < chunks->count; i++)
        if(chunks->items[i].moved)
            write_text(out, module->header, chunks->items[i].begin, chunks->items[i].end);
    fputs(includes + strlen(PRUNE_INCLUDES_MARK), out);
    if(TextStreamClose(&stream) != 0) {
        free(text);
        return NULL;
    }
    return text;
}

int
PruneProgram(ZirCPruneModule *modules, int count, struct ZirSimdUses *simd)
{
    PruneWords words = {0};
    PruneChunks *chunks = AllocateOrExit((size_t)(count > 0 ? count : 1) * sizeof(*chunks));
    int ok = 1;
    memset(chunks, 0, (size_t)(count > 0 ? count : 1) * sizeof(*chunks));
    for(int m = 0; m < count; m++)
        if(modules[m].header != NULL)
            parse_chunks(modules[m].header, &chunks[m]);
    /* A source also needs whatever the declarations its header could move
     * into it name. */
    for(int m = 0; m < count; m++) {
        if(modules[m].source == NULL)
            continue;
        note_words(&words, modules[m].source, strlen(modules[m].source), NOTE_SOURCE, m, NULL);
        for(int i = 0; i < chunks[m].count; i++) {
            const PruneChunk *chunk = &chunks[m].items[i];
            if(chunk->kind == PRUNE_DECLARATION)
                note_words(&words, modules[m].header + chunk->begin,
                           chunk->end - chunk->begin, NOTE_SOURCE, m, NULL);
        }
    }
    for(int m = 0; m < count; m++)
        for(int i = 0; i < chunks[m].count; i++)
            if(chunks[m].items[i].kind == PRUNE_ROOT)
                keep_chunk(&words, modules[m].header, &chunks[m].items[i]);
    /* Keeping a chunk can make others wanted; repeat until nothing changes. */
    for(int changed = 1; changed;) {
        changed = 0;
        for(int m = 0; m < count; m++)
            for(int i = 0; i < chunks[m].count; i++) {
                PruneChunk *chunk = &chunks[m].items[i];
                if(chunk->kept || !declares(chunk->kind) || chunk->kind == PRUNE_ROOT ||
                   !chunk_wanted(&words, chunk, m))
                    continue;
                keep_chunk(&words, modules[m].header, chunk);
                changed = 1;
            }
    }
    for(int m = 0; m < count; m++)
        for(int i = 0; i < chunks[m].count; i++) {
            const PruneChunk *chunk = &chunks[m].items[i];
            if(chunk->kept && declares(chunk->kind))
                chunks[m].written = 1;
            if(chunk->kept && (chunk->kind == PRUNE_INLINE ||
                               chunk->kind == PRUNE_INLINE_BODY))
                chunks[m].inlines = 1;
        }
    for(int m = 0; ok && m < count; m++) {
        if(chunks[m].written) {
            stamp_header(&words, &modules[m], &chunks[m]);
            modules[m].final_header = compose_header(&words, modules, chunks, m, simd);
            ok = modules[m].final_header != NULL;
        }
        if(ok && modules[m].source != NULL) {
            modules[m].final_source = compose_source(&words, modules, chunks, m);
            ok = modules[m].final_source != NULL;
        }
    }
    for(int m = 0; m < count; m++) {
        for(int i = 0; i < chunks[m].count; i++)
            free(chunks[m].items[i].names);
        free(chunks[m].items);
    }
    free(chunks);
    free(words.items);
    return ok;
}

void
PruneModuleFree(ZirCPruneModule *module)
{
    free(module->stem);
    free(module->guard);
    free(module->header);
    free(module->source);
    free(module->header_scope);
    free(module->source_scope);
    free(module->final_header);
    free(module->final_source);
    memset(module, 0, sizeof(*module));
}
