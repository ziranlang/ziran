#include "zir_diagnostic.h"
#include <stdlib.h>
#include <string.h>

static int diagnostic_json = -1;

static void
json_string(FILE *out, const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;

    fputc('"', out);
    while(*cursor != '\0') {
        if(*cursor == '"' || *cursor == '\\') {
            fputc('\\', out);
            fputc(*cursor, out);
        } else if(*cursor < 0x20) {
            fprintf(out, "\\u%04x", *cursor);
        } else {
            fputc(*cursor, out);
        }
        cursor++;
    }
    fputc('"', out);
}

int
SetDiagnosticFormat(const char *format)
{
    if(strcmp(format, "json") == 0)
        diagnostic_json = 1;
    else if(strcmp(format, "text") == 0)
        diagnostic_json = 0;
    else
        return 0;
    return 1;
}
/* Buffers DiagnosticV keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct DiagnosticVBuffers {
    char message[ZIR_TEXT_MAX * 2];
} DiagnosticVBuffers;

void DiagnosticV(ZirSourceSpan span, const char *code, const char *format, va_list args);

static void
DiagnosticV_with_buffers(ZirSourceSpan span, const char *code, const char *format, va_list args, DiagnosticVBuffers *buffers)
{
    vsnprintf(buffers->message, sizeof(buffers->message), format, args);
    if(diagnostic_json < 0) {
        const char *environment = getenv("ZIRAN_DIAGNOSTICS");
        diagnostic_json = environment != NULL && strcmp(environment, "json") == 0;
    }
    if(diagnostic_json) {
        fputs("{\"severity\":\"error\",\"code\":", stderr);
        json_string(stderr, code);
        fputs(",\"message\":", stderr);
        json_string(stderr, buffers->message);
        fputs(",\"path\":", stderr);
        json_string(stderr, SpanPath(span));
        fprintf(stderr, ",\"line\":%d,\"column\":%d,"
                "\"end_line\":%d,\"end_column\":%d}\n",
                span.line, span.column,
                span.end_line > 0 ? span.end_line : span.line,
                span.end_column > 0 ? span.end_column : span.column);
    } else if(SpanPath(span)[0] != '\0') {
        fprintf(stderr, "%s:%d:%d: %s\n", SpanPath(span), span.line, span.column, buffers->message);
    } else {
        fprintf(stderr, "ziran: %s\n", buffers->message);
    }
}

void
DiagnosticV(ZirSourceSpan span, const char *code, const char *format, va_list args)
{
    static _Thread_local DiagnosticVBuffers *spares[16];
    static _Thread_local int spare_count;
    DiagnosticVBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    DiagnosticV_with_buffers(span, code, format, args, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

void
Diagnostic(ZirSourceSpan span, const char *code, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    DiagnosticV(span, code, format, args);
    va_end(args);
}
