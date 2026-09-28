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

void
DiagnosticV(ZirSourceSpan span, const char *code, const char *format, va_list args)
{
    char message[ZIR_TEXT_MAX * 2];

    vsnprintf(message, sizeof(message), format, args);
    if(diagnostic_json < 0) {
        const char *environment = getenv("ZIRAN_DIAGNOSTICS");

        diagnostic_json = environment != NULL && strcmp(environment, "json") == 0;
    }
    if(diagnostic_json) {
        fputs("{\"severity\":\"error\",\"code\":", stderr);
        json_string(stderr, code);
        fputs(",\"message\":", stderr);
        json_string(stderr, message);
        fputs(",\"path\":", stderr);
        json_string(stderr, SpanPath(span));
        fprintf(stderr, ",\"line\":%d,\"column\":%d,"
                "\"end_line\":%d,\"end_column\":%d}\n",
                span.line, span.column,
                span.end_line > 0 ? span.end_line : span.line,
                span.end_column > 0 ? span.end_column : span.column);
    } else if(SpanPath(span)[0] != '\0') {
        fprintf(stderr, "%s:%d:%d: %s\n", SpanPath(span), span.line, span.column, message);
    } else {
        fprintf(stderr, "ziran: %s\n", message);
    }
}

void
Diagnostic(ZirSourceSpan span, const char *code, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    DiagnosticV(span, code, format, args);
    va_end(args);
}
