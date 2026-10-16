#include "zir_runtime.h"
#include "zir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
same_contents(const char *path, const char *text)
{
    FILE *file = fopen(path, "rb");
    size_t length = strlen(text), used = 0;
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

int EmitRuntimeHeaders(const char *out_dir, const char *generated_path);

static int
EmitRuntimeHeaders_with_buffers(const char *out_dir, const char *generated_path, EmitRuntimeHeadersBuffers *buffers)
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
        for(size_t i = 0; i < count; i++) {
            char include[256];
            snprintf(include, sizeof(include), "#include \"%s\"", ZirRuntimeHeaders[i].name);
            if(strncmp(buffers->line, include, strlen(include)) == 0)
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
            for(size_t j = 0; j < count; j++) {
                char include[256];
                if(needed[j]) continue;
                snprintf(include, sizeof(include), "#include \"%s\"", ZirRuntimeHeaders[j].name);
                if(strstr(ZirRuntimeHeaders[i].text, include) != NULL) {
                    needed[j] = 1;
                    changed = 1;
                }
            }
        }
    } while(changed);
    for(size_t i = 0; ok && i < count; i++) {
        const ZirRuntimeHeader *header = &ZirRuntimeHeaders[i];
        FILE *out;
        if(!needed[i]) continue;
        snprintf(buffers->path, sizeof(buffers->path), "%s/%s", out_dir, header->name);
        GeneratedOutputRecord(buffers->path);
        /* An unchanged header keeps its timestamp for build tools. */
        if(same_contents(buffers->path, header->text)) continue;
        out = fopen(buffers->path, "wb");
        if(out == NULL) { ok = 0; break; }
        if(fputs(header->text, out) < 0) ok = 0;
        if(fclose(out) != 0) ok = 0;
    }
    free(needed);
    return ok;
}

int
EmitRuntimeHeaders(const char *out_dir, const char *generated_path)
{
    static _Thread_local EmitRuntimeHeadersBuffers *spares[16];
    static _Thread_local int spare_count;
    EmitRuntimeHeadersBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = EmitRuntimeHeaders_with_buffers(out_dir, generated_path, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
