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
json_span(FILE *out, ZirSourceSpan span)
{
    fputs("\"path\":", out);
    json_string(out, SpanPath(span));
    fprintf(out, ",\"line\":%d,\"column\":%d,\"end_line\":%d,\"end_column\":%d",
            span.line, span.column,
            span.end_line > 0 ? span.end_line : span.line,
            span.end_column > 0 ? span.end_column : span.column);
}

static void
report(ZirSourceSpan span, const char *severity, const char *code,
       const DiagnosticDetails *details, const char *format,
       va_list args, DiagnosticVBuffers *buffers)
{
    vsnprintf(buffers->message, sizeof(buffers->message), format, args);
    if(diagnostic_json < 0) {
        const char *environment = getenv("ZIRAN_DIAGNOSTICS");
        diagnostic_json = environment != NULL && strcmp(environment, "json") == 0;
    }
    int warning = strcmp(severity, "warning") == 0;
    if(diagnostic_json) {
        fprintf(stderr, "{\"schema_version\":1,\"severity\":\"%s\",\"code\":", severity);
        json_string(stderr, code);
        fputs(",\"message\":", stderr);
        json_string(stderr, buffers->message);
        fputc(',', stderr);
        json_span(stderr, span);
        if(details != NULL) {
            if(details->expected_type != NULL) {
                fputs(",\"expected_type\":", stderr);
                json_string(stderr, details->expected_type);
            }
            if(details->actual_type != NULL) {
                fputs(",\"actual_type\":", stderr);
                json_string(stderr, details->actual_type);
            }
            if(details->related_message != NULL && details->related_span.line > 0) {
                fputs(",\"related\":[{", stderr);
                json_span(stderr, details->related_span);
                fputs(",\"message\":", stderr);
                json_string(stderr, details->related_message);
                fputs("}]", stderr);
            }
            if(details->target != NULL) {
                fputs(",\"target\":", stderr);
                json_string(stderr, details->target);
            }
            if(details->capability != NULL) {
                fputs(",\"capability\":", stderr);
                json_string(stderr, details->capability);
            }
            if(details->suggested_name != NULL) {
                fputs(",\"suggested_name\":", stderr);
                json_string(stderr, details->suggested_name);
            }
            if(details->replacement != NULL && details->edit_span.line > 0) {
                fputs(",\"edits\":[{", stderr);
                json_span(stderr, details->edit_span);
                fputs(",\"replacement\":", stderr);
                json_string(stderr, details->replacement);
                if(details->original != NULL) {
                    fputs(",\"original\":", stderr);
                    json_string(stderr, details->original);
                }
                fputs(",\"message\":", stderr);
                json_string(stderr, details->edit_message != NULL ?
                            details->edit_message : "Replace this name");
                fputs("}]", stderr);
            }
        }
        fputs("}\n", stderr);
    } else if(SpanPath(span)[0] != '\0') {
        fprintf(stderr, "%s:%d:%d: %s%s\n", SpanPath(span), span.line, span.column,
                warning ? "warning: " : "", buffers->message);
    } else {
        fprintf(stderr, "ziran: %s%s\n", warning ? "warning: " : "", buffers->message);
    }
    if(!diagnostic_json && details != NULL && details->related_message != NULL &&
       details->related_span.line > 0)
        fprintf(stderr, "%s:%d:%d: note: %s\n", SpanPath(details->related_span),
                details->related_span.line, details->related_span.column,
                details->related_message);
}

static void
DiagnosticV_with_buffers(ZirSourceSpan span, const char *code, const char *format, va_list args, DiagnosticVBuffers *buffers)
{
    report(span, "error", code, NULL, format, args, buffers);
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

void
DiagnosticDetailed(ZirSourceSpan span, const char *code,
                   const DiagnosticDetails *details, const char *format, ...)
{
    DiagnosticVBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    va_list args;
    va_start(args, format);
    report(span, "error", code, details, format, args, buffers);
    va_end(args);
    free(buffers);
}

void
Warning(ZirSourceSpan span, const char *code, const char *format, ...)
{
    va_list args;
    DiagnosticVBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    va_start(args, format);
    report(span, "warning", code, NULL, format, args, buffers);
    va_end(args);
    free(buffers);
}
