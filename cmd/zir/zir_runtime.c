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
    if(file == NULL)
        return 0;
    while(fgets(buffers->line, sizeof(buffers->line), file) != NULL) {
        for(const ZirRuntimeHeader *header = ZirRuntimeHeaders; header->name; header++) {
            char include[256];
            FILE *out;
            snprintf(include, sizeof(include), "#include \"%s\"", header->name);
            if(strncmp(buffers->line, include, strlen(include)) != 0)
                continue;
            snprintf(buffers->path, sizeof(buffers->path), "%s/%s", out_dir, header->name);
            GeneratedOutputRecord(buffers->path);
            /* An unchanged header keeps its timestamp for build tools. */
            if(same_contents(buffers->path, header->text))
                continue;
            out = fopen(buffers->path, "wb");
            if(out == NULL || fputs(header->text, out) < 0) {
                if(out != NULL) fclose(out);
                fclose(file);
                return 0;
            }
            if(fclose(out) != 0) {
                fclose(file);
                return 0;
            }
        }
    }
    fclose(file);
    return 1;
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
