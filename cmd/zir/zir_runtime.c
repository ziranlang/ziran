#include "zir_scratch.h"
#include "zir_memory.h"
#include "zir_runtime.h"
#include "zir_simd.h"
#include "zir.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int
same_contents(const char *path, const char *text, size_t length)
{
    FILE *file = fopen(path, "rb");
    size_t used = 0;
    int same = file != NULL;
    char buffer[4096];
    size_t read;
    while(same && (read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        same = used + read <= length && !memcmp(buffer, text + used, read);
        used += read;
    }
    if(file != NULL)
        fclose(file);
    return same && used == length;
}
/* Buffers EmitRuntimeHeaders keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitRuntimeHeadersBuffers {
    char line[4096];
    char path[4096];
} EmitRuntimeHeadersBuffers;

int EmitRuntimeHeaders(const char *out_dir, const char *generated_path, struct ZirSimdUses *simd);

static const char simd_ops_name[] = "ziran_simd_ops.h";

/* Write text to path unless it already holds it, which keeps its timestamp
 * for build tools. Emission workers may write the same header at once; each
 * moves a complete private copy into place. */
static int
write_generated(const char *path, const char *text, size_t length)
{
    GeneratedOutputRecord(path);
    if(same_contents(path, text, length))
        return 1;
    char temporary[4096 + 32];
    snprintf(temporary, sizeof(temporary), "%s.%ld.tmp", path, (long)getpid());
    FILE *out = fopen(temporary, "wb");
    if(out == NULL)
        return 0;
    int ok = fwrite(text, 1, length, out) == length;
    if(fclose(out) != 0) ok = 0;
#ifdef _WIN32
    if(ok) remove(path); /* rename never replaces there */
#endif
    if(ok && rename(temporary, path) != 0) ok = 0;
    if(!ok) remove(temporary);
    return ok;
}

static int
EmitRuntimeHeaders_with_buffers(const char *out_dir, const char *generated_path, ZirSimdUses *simd,
                                EmitRuntimeHeadersBuffers *buffers)
{
    FILE *file = fopen(generated_path, "rb");
    size_t count = 0;
    int ok = 1, changed;
    if(file == NULL)
        return 0;
    while(ZirRuntimeHeaders[count].name) count++;
    unsigned char *needed = AllocateOrExit(count ? count : 1);
    memset(needed, 0, count);
    while(fgets(buffers->line, sizeof(buffers->line), file) != NULL) {
        if(strncmp(buffers->line, "#include \"", 10) != 0)
            continue;
        for(size_t i = 0; i < count; i++) {
            size_t length = strlen(ZirRuntimeHeaders[i].name);
            if(strncmp(buffers->line + 10, ZirRuntimeHeaders[i].name, length) == 0 &&
               buffers->line[10 + length] == '"')
                needed[i] = 1;
        }
    }
    if(ferror(file)) ok = 0;
    if(fclose(file) != 0) ok = 0;
    /* Embedded headers may depend on other embedded headers. Close the set
     * before writing, including cycles, without reopening generated files. */
    do {
        changed = 0;
        for(size_t i = 0; i < count; i++) {
            if(!needed[i]) continue;
            for(size_t j = 0; j < count; j++)
                if(!needed[j] && (ZirRuntimeHeaders[i].includes >> j & 1u)) {
                    needed[j] = 1;
                    changed = 1;
                }
        }
    } while(changed);
    for(size_t i = 0; ok && i < count; i++) {
        const ZirRuntimeHeader *header = &ZirRuntimeHeaders[i];
        if(!needed[i]) continue;
        if(simd != NULL && strcmp(header->name, simd_ops_name) == 0) {
            simd->header = 1;
            continue;
        }
        snprintf(buffers->path, sizeof(buffers->path), "%s/%s", out_dir, header->name);
        ok = write_generated(buffers->path, header->text, strlen(header->text));
    }
    free(needed);
    return ok;
}

COMPILER_SCRATCH_RETURN(, int, EmitRuntimeHeaders,
    (const char *out_dir, const char *generated_path, ZirSimdUses *simd),
    EmitRuntimeHeadersBuffers, RECOVERABLE, free,
    EmitRuntimeHeaders_with_buffers(out_dir, generated_path, simd, buffers))

/* ---- The SIMD helpers a program calls ---- */

static void *
grown(void *items, size_t used, size_t size)
{
    void *bigger = AllocateOrExit(size);
    if(used) memcpy(bigger, items, used);
    free(items);
    return bigger;
}

/* The full header's segments: each starts after a marker line, such as
 * the comment @add_s16x8; an empty name marks lines every build keeps. */
typedef struct SimdSegment {
    long helper;          /* entry in the helper names, or -1 */
    const char *text;
    size_t length;
    size_t next;          /* the helper's next segment + 1, or 0 */
} SimdSegment;

static int
word_byte(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static void
need_helper(long helper, unsigned char *needed, size_t *work, size_t *pending)
{
    if(helper >= 0 && !needed[helper]) {
        needed[helper] = 1;
        work[(*pending)++] = (size_t)helper;
    }
}

/* Mark the helpers a segment calls: ZI_N(name) and words such as zi_name. */
static void
need_calls(SimdNames *helpers, const SimdSegment *segment, unsigned char *needed,
           size_t *work, size_t *pending)
{
    const char *p = segment->text, *end = p + segment->length;
    while(p < end) {
        if(!word_byte(*p)) {
            p++;
            continue;
        }
        const char *word = p;
        while(p < end && word_byte(*p)) p++;
        size_t length = (size_t)(p - word);
        if(length == 4 && !memcmp(word, "ZI_N", 4) && p < end && *p == '(') {
            const char *name = ++p;
            while(p < end && word_byte(*p)) p++;
            length = (size_t)(p - name);
            if(length > 3 && !memcmp(name + length - 3, "_p_", 3))
                length -= 3;
            need_helper(SimdNamesFind(helpers, name, length, 0), needed, work, pending);
        } else if(word[0] == 'z' && SimdHelperWord(&word, &length)) {
            need_helper(SimdNamesFind(helpers, word, length, 0), needed, work, pending);
        }
    }
}

int
EmitSimdOps(const char *out_dir, const ZirSimdUses *uses)
{
    const char *full = NULL;
    for(size_t i = 0; ZirRuntimeHeaders[i].name != NULL; i++)
        if(strcmp(ZirRuntimeHeaders[i].name, simd_ops_name) == 0)
            full = ZirRuntimeHeaders[i].text;
    if(full == NULL)
        return 0;
    /* Split the text at its marker lines, a helper's segments chained in order. */
    SimdNames helpers = {0};
    SimdSegment *segments = NULL;
    size_t count = 0, capacity = 0, *last = NULL, last_capacity = 0;
    const char *common_end = NULL;
    for(const char *line = full; *line;) {
        const char *newline = strchr(line, '\n');
        const char *after = newline ? newline + 1 : line + strlen(line);
        size_t length = (size_t)(after - line);
        if(length >= 6 && line[0] == '/' && line[1] == '*' && line[2] == '@' &&
           newline != NULL && newline[-1] == '/' && newline[-2] == '*') {
            if(common_end == NULL) common_end = line;
            if(count == capacity) {
                capacity = capacity ? capacity * 2 : 4096;
                segments = grown(segments, count * sizeof(*segments), capacity * sizeof(*segments));
            }
            SimdSegment *segment = &segments[count];
            size_t name_length = length - 6;
            segment->helper = name_length ? SimdNamesFind(&helpers, line + 3, name_length, 1) : -1;
            segment->text = after;
            segment->length = 0;
            segment->next = 0;
            if(segment->helper >= 0) {
                size_t helper = (size_t)segment->helper;
                if(helper >= last_capacity) {
                    size_t bigger = last_capacity ? last_capacity * 2 : 1024;
                    while(bigger <= helper) bigger *= 2;
                    last = grown(last, last_capacity * sizeof(*last), bigger * sizeof(*last));
                    memset(last + last_capacity, 0, (bigger - last_capacity) * sizeof(*last));
                    last_capacity = bigger;
                }
                if(last[helper]) segments[last[helper] - 1].next = count + 1;
                last[helper] = count + 1;
            }
            count++;
        } else if(count > 0) {
            segments[count - 1].length += length;
        }
        line = after;
    }
    if(common_end == NULL)
        common_end = full + strlen(full);
    /* The first segment of each helper, then the ones the program calls and
     * those they call. */
    size_t *first = AllocateOrExit((helpers.count + 1) * sizeof(*first));
    memset(first, 0, (helpers.count + 1) * sizeof(*first));
    for(size_t i = count; i-- > 0;)
        if(segments[i].helper >= 0) first[segments[i].helper] = i + 1;
    unsigned char *needed = AllocateOrExit(helpers.count + 1);
    memset(needed, 0, helpers.count + 1);
    size_t *work = AllocateOrExit((helpers.count + 1) * sizeof(*work)), pending = 0;
    for(size_t i = 0; i < uses->names.count; i++)
        need_helper(SimdNamesFind(&helpers, uses->names.text[i], uses->names.length[i], 0),
                    needed, work, &pending);
    while(pending > 0) {
        size_t helper = work[--pending];
        for(size_t s = first[helper]; s != 0; s = segments[s - 1].next)
            need_calls(&helpers, &segments[s - 1], needed, work, &pending);
    }
    size_t length = (size_t)(common_end - full);
    for(size_t i = 0; i < count; i++)
        if(segments[i].helper < 0 || needed[segments[i].helper])
            length += segments[i].length;
    char *text = AllocateOrExit(length + 1), *at = text;
    memcpy(at, full, (size_t)(common_end - full));
    at += common_end - full;
    for(size_t i = 0; i < count; i++)
        if(segments[i].helper < 0 || needed[segments[i].helper]) {
            memcpy(at, segments[i].text, segments[i].length);
            at += segments[i].length;
        }
    *at = '\0';
    char path[4096];
    int ok = snprintf(path, sizeof(path), "%s/%s", out_dir, simd_ops_name) < (int)sizeof(path) &&
             write_generated(path, text, length);
    free(text);
    free(work);
    free(needed);
    free(first);
    free(last);
    free(segments);
    SimdNamesFree(&helpers);
    return ok;
}
