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

int
EmitRuntimeHeaders(const char *out_dir, const char *generated_path)
{
    FILE *file = fopen(generated_path, "rb");
    char line[4096];
    if(file == NULL)
        return 0;
    while(fgets(line, sizeof(line), file) != NULL) {
        for(const ZirRuntimeHeader *header = ZirRuntimeHeaders; header->name; header++) {
            char include[256], path[4096];
            FILE *out;
            snprintf(include, sizeof(include), "#include \"%s\"", header->name);
            if(strncmp(line, include, strlen(include)) != 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", out_dir, header->name);
            GeneratedOutputRecord(path);
            /* An unchanged header keeps its timestamp for build tools. */
            if(same_contents(path, header->text))
                continue;
            out = fopen(path, "wb");
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
