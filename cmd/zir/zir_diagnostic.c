#include "zir_diagnostic.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static atomic_int diagnostic_json = ATOMIC_VAR_INIT(-1);
static atomic_flag diagnostic_lock = ATOMIC_FLAG_INIT;

/* Paths and subprocess output may contain non-UTF-8 bytes. Preserve valid
 * UTF-8, but escape individual invalid bytes so each JSON line is decodable. */
static size_t
utf8_length(const unsigned char *at)
{
    unsigned char first = at[0];
    size_t length = first >= 0xc2 && first <= 0xdf ? 2 :
        first >= 0xe0 && first <= 0xef ? 3 :
        first >= 0xf0 && first <= 0xf4 ? 4 : 0;
    for(size_t i = 1; i < length; i++)
        if(at[i] == 0 || at[i] < 0x80 || at[i] > 0xbf) return 0;
    if((first == 0xe0 && at[1] < 0xa0) ||
       (first == 0xed && at[1] >= 0xa0) ||
       (first == 0xf0 && at[1] < 0x90) ||
       (first == 0xf4 && at[1] >= 0x90)) return 0;
    return length;
}

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
        } else if(*cursor >= 0x80) {
            size_t length = utf8_length(cursor);
            if(length == 0) fprintf(out, "\\u%04x", *cursor);
            else {
                fwrite(cursor, 1, length, out);
                cursor += length - 1;
            }
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
SetDiagnosticFormatFromArguments(int argc, char *const *argv)
{
    /* Even an earlier invalid option must honor a following format flag. */
    for(int i = 1; i < argc && strcmp(argv[i], "--"); i++)
        if(!strncmp(argv[i], "--diagnostics=", 14))
            SetDiagnosticFormat(argv[i] + 14);
}

int
DiagnosticJsonEnabled(void)
{
    if(diagnostic_json < 0) {
        const char *environment = getenv("ZIRAN_DIAGNOSTICS");
        int unset = -1;
        atomic_compare_exchange_strong(&diagnostic_json, &unset,
            environment != NULL && !strcmp(environment, "json"));
    }
    return diagnostic_json;
}

void
DiagnosticOutOfMemory(void)
{
    /* No allocation, source interning, or nested diagnostic buffer needed. */
    if(DiagnosticJsonEnabled())
        fputs("{\"schema_version\":1,\"severity\":\"error\",\"code\":\"compiler.memory\","
              "\"message\":\"out of memory\",\"path\":\"\",\"line\":0,\"column\":0,"
              "\"end_line\":0,\"end_column\":0}\n", stderr);
    else fputs("ziran: out of memory\n", stderr);
}
/* Keep large messages off recursive stacks, releasing them after reporting. */
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
    int json = DiagnosticJsonEnabled();
    int warning = strcmp(severity, "warning") == 0;
    while(atomic_flag_test_and_set_explicit(&diagnostic_lock, memory_order_acquire)) {}
    if(json) {
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
    if(!json && details != NULL && details->related_message != NULL &&
       details->related_span.line > 0)
        fprintf(stderr, "%s:%d:%d: note: %s\n", SpanPath(details->related_span),
                details->related_span.line, details->related_span.column,
                details->related_message);
    atomic_flag_clear_explicit(&diagnostic_lock, memory_order_release);
}

static void
DiagnosticV_with_buffers(ZirSourceSpan span, const char *code, const char *format, va_list args, DiagnosticVBuffers *buffers)
{
    report(span, "error", code, NULL, format, args, buffers);
}

void
DiagnosticV(ZirSourceSpan span, const char *code, const char *format, va_list args)
{
    DiagnosticVBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    DiagnosticV_with_buffers(span, code, format, args, buffers);
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

void
DiagnosticTarget(ZirSourceSpan span, const char *code, const char *target,
                 const char *capability, const char *format, ...)
{
    DiagnosticDetails details = {0};
    details.target = target;
    details.capability = capability;
    DiagnosticVBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    va_list args;
    va_start(args, format);
    report(span, "error", code, &details, format, args, buffers);
    va_end(args);
    free(buffers);
}
