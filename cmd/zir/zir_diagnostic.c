#include "zir_memory.h"
#include "zir_diagnostic.h"
#include "zir_document.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

static atomic_int diagnostic_json = ATOMIC_VAR_INIT(-1);
static atomic_flag diagnostic_lock = ATOMIC_FLAG_INIT;

/* Preserve valid UTF-8; escape invalid bytes so every record is decodable. */
static size_t utf8_length(const unsigned char *at)
{
    unsigned char first = at[0];
    size_t length = first >= 0xc2 && first <= 0xdf ? 2 :
        first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
    for(size_t i = 1; i < length; i++)
        if(at[i] == 0 || at[i] < 0x80 || at[i] > 0xbf) return 0;
    if((first == 0xe0 && at[1] < 0xa0) || (first == 0xed && at[1] >= 0xa0) ||
       (first == 0xf0 && at[1] < 0x90) || (first == 0xf4 && at[1] >= 0x90)) return 0;
    return length;
}

/* Build complete bounded records. Optional structures roll back as units;
 * exact edit preimages/replacements are never truncated. Reserve the tail. */
#define DIAGNOSTIC_BYTES 32768
#define DIAGNOSTIC_MESSAGE_BYTES 2048
typedef struct JsonBuffer { char bytes[DIAGNOSTIC_BYTES]; size_t count; int failed; } JsonBuffer;
static void put(JsonBuffer *out, const char *text)
{
    size_t size = strlen(text);
    if(size > sizeof(out->bytes) - 128 - out->count) { out->failed = 1; return; }
    memcpy(out->bytes + out->count, text, size); out->count += size;
}
static void number(JsonBuffer *out, size_t value)
{
    char text[32]; snprintf(text, sizeof(text), "%zu", value); put(out, text);
}
static int string(JsonBuffer *out, const char *value, size_t limit)
{
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");
    size_t count = 0;
    put(out, "\"");
    while(*cursor) {
        size_t length = *cursor >= 0x80 ? utf8_length(cursor) : 1;
        if(!length) length = 1;
        if(length > limit - count) break;
        char text[8] = {0};
        if(*cursor == '"' || *cursor == '\\') { text[0]='\\'; text[1]=(char)*cursor; }
        else if(*cursor < 0x20 || (*cursor >= 0x80 && !utf8_length(cursor)))
            snprintf(text, sizeof(text), "\\u%04x", *cursor);
        else memcpy(text, cursor, length);
        put(out, text); cursor += length; count += length;
        if(out->failed) break;
    }
    put(out, "\"");
    return *cursor != 0;
}
static void nullable(JsonBuffer *out, const char *value)
{
    if(value) string(out, value, SIZE_MAX); else put(out, "null");
}
static void json_span(JsonBuffer *out, ZirSourceSpan span)
{
    const char *path = SpanPath(span);
    put(out, "\"path\":");
    if(strlen(path) <= 1024) string(out, path, 1024); else put(out, "null");
    put(out, ",\"line\":"); number(out, span.line > 0 ? (size_t)span.line : 0);
    put(out, ",\"column\":"); number(out, span.column > 0 ? (size_t)span.column : 0);
    put(out, ",\"end_line\":"); number(out, span.end_line > 0 ? (size_t)span.end_line : span.line > 0 ? (size_t)span.line : 0);
    put(out, ",\"end_column\":"); number(out, span.end_column > 0 ? (size_t)span.end_column : span.column > 0 ? (size_t)span.column : 0);
}
static int keep_optional(JsonBuffer *out, size_t checkpoint)
{
    if(!out->failed && out->count <= DIAGNOSTIC_BYTES - 1024) return 0;
    out->failed = 0; out->count = checkpoint; return 1;
}

int SetDiagnosticFormat(const char *format)
{
    if(strcmp(format, "json") == 0) diagnostic_json = 1;
    else if(strcmp(format, "text") == 0) diagnostic_json = 0;
    else return 0;
    return 1;
}
void SetDiagnosticFormatFromArguments(int argc, char *const *argv)
{
    for(int i = 1; i < argc && strcmp(argv[i], "--"); i++)
        if(!strncmp(argv[i], "--diagnostics=", 14)) SetDiagnosticFormat(argv[i] + 14);
}
int DiagnosticJsonEnabled(void)
{
    if(diagnostic_json < 0) {
        const char *environment = getenv("ZIRAN_DIAGNOSTICS"); int unset = -1;
        atomic_compare_exchange_strong(&diagnostic_json, &unset,
            environment != NULL && !strcmp(environment, "json"));
    }
    return diagnostic_json;
}
void DiagnosticOutOfMemory(void)
{
    /* No allocation, source interning, or nested diagnostic buffer. */
    if(DiagnosticJsonEnabled())
        fputs("{\"schema_version\":2,\"severity\":\"error\",\"code\":\"compiler.memory\","
              "\"message\":\"out of memory\",\"path\":\"\",\"line\":0,\"column\":0,"
              "\"end_line\":0,\"end_column\":0,\"semantic_id\":null,\"cause\":null,\"document\":null,"
              "\"related_graph\":{\"nodes\":[],\"edges\":[]},\"alternatives\":[],"
              "\"truncation\":{\"message\":false,\"metadata\":false}}\n", stderr);
    else fputs("ziran: out of memory\n", stderr);
}
typedef struct DiagnosticVBuffers { char message[ZIR_TEXT_MAX * 2]; JsonBuffer json; } DiagnosticVBuffers;

static void report(ZirSourceSpan span, const char *severity, const char *code,
                   const DiagnosticDetails *details, const char *format,
                   va_list args, DiagnosticVBuffers *buffers)
{
    int message_length = vsnprintf(buffers->message, sizeof(buffers->message), format, args);
    int json = DiagnosticJsonEnabled(), warning = strcmp(severity, "warning") == 0;
#ifdef __ANDROID__
    __android_log_print(warning ? ANDROID_LOG_WARN : ANDROID_LOG_ERROR, "Ziran",
                        "%s:%d:%d: %s [%s]", SpanPath(span), span.line,
                        span.column, buffers->message, code);
#endif
    while(atomic_flag_test_and_set_explicit(&diagnostic_lock, memory_order_acquire)) {}
    if(json) {
        JsonBuffer *out = &buffers->json; out->count = 0; out->failed = 0;
        int omitted = 0;
        put(out, "{\"schema_version\":2,\"severity\":"); string(out, severity, 16);
        put(out, ",\"code\":"); omitted |= string(out, code, 128);
        put(out, ",\"message\":");
        int clipped = string(out, buffers->message, DIAGNOSTIC_MESSAGE_BYTES) ||
            message_length < 0 || (size_t)message_length >= sizeof(buffers->message);
        put(out, ","); json_span(out, span);
        omitted |= strlen(SpanPath(span)) > 1024;
        put(out, ",\"semantic_id\":");
        if(details && details->semantic_id && strlen(details->semantic_id)==67) nullable(out, details->semantic_id);
        else put(out, "null");
        const ZirDocument *doc = details ? details->document : NULL;
        put(out, ",\"document\":");
        if(doc) {
            put(out, "{\"sha256\":"); string(out, doc->sha256, 64);
            put(out, ",\"bytes\":"); number(out, doc->bytes);
            put(out, ",\"coordinate_space\":"); string(out, doc->rewritten ? "rewritten_source" : "source", 32);
            put(out, "}");
        } else put(out, "null");
        size_t cause_checkpoint=out->count;
        put(out, ",\"cause\":");
        const char *kind=details ? details->cause_kind : NULL;
        if(kind && (!strcmp(kind,"type_mismatch") || !strcmp(kind,"unresolved_name") ||
                    !strcmp(kind,"unresolved_function") || !strcmp(kind,"target_restriction"))) {
            put(out, "{\"kind\":"); nullable(out, kind);
            put(out, ",\"subject\":"); nullable(out, details->subject); put(out, "}");
        } else { put(out, "null"); omitted |= kind!=NULL; }
        if(keep_optional(out,cause_checkpoint)) { omitted=1; put(out, ",\"cause\":null"); }
        const char *keys[] = {"expected_type", "actual_type", "target", "capability", "suggested_name"};
        const char *values[] = {details ? details->expected_type : NULL, details ? details->actual_type : NULL,
            details ? details->target : NULL, details ? details->capability : NULL, details ? details->suggested_name : NULL};
        for(size_t i=0; i<5; i++) if(values[i]) {
            size_t checkpoint=out->count;
            put(out, ","); string(out, keys[i], 32); put(out, ":"); nullable(out, values[i]);
            omitted |= keep_optional(out, checkpoint);
        }
        size_t checkpoint=out->count;
        put(out, ",\"related_graph\":{\"nodes\":[");
        if(details && details->related_message && details->related_span.line>0) {
            omitted |= strlen(SpanPath(details->related_span)) > 1024;
            put(out, "{\"id\":0,\"role\":\"primary\","); json_span(out, span);
            put(out, "},{\"id\":1,\"role\":\"declaration\","); json_span(out, details->related_span);
            put(out, ",\"message\":"); nullable(out, details->related_message);
            put(out, "}],\"edges\":[{\"from\":0,\"to\":1,\"relation\":");
            nullable(out, details->related_relation); put(out, "}]");
        } else put(out, "],\"edges\":[]");
        put(out, "}");
        if(keep_optional(out, checkpoint)) { omitted=1; put(out, ",\"related_graph\":{\"nodes\":[],\"edges\":[]}"); }
        checkpoint=out->count;
        put(out, ",\"alternatives\":[");
        size_t start, end;
        if(details && details->replacement && details->original && doc &&
           DocumentOffset(doc, details->edit_span.line, details->edit_span.column, &start) &&
           DocumentOffset(doc, details->edit_span.end_line, details->edit_span.end_column, &end) && start<=end &&
           end-start==strlen(details->original) && strlen(SpanPath(details->edit_span))<=1024 &&
           !strcmp(SpanPath(details->edit_span), SpanPath(span))) {
            put(out, "{\"id\":\"replace_name\",\"edits\":[{"); json_span(out, details->edit_span);
            put(out, ",\"start_byte\":"); number(out, start);
            put(out, ",\"end_byte\":"); number(out, end);
            put(out, ",\"document_sha256\":"); nullable(out, doc->sha256);
            put(out, ",\"original\":"); nullable(out, details->original);
            put(out, ",\"replacement\":"); nullable(out, details->replacement);
            put(out, "}],\"message\":"); nullable(out, details->edit_message ? details->edit_message : "Replace this name"); put(out, "}");
        }
        put(out, "]");
        if(keep_optional(out, checkpoint)) { omitted=1; put(out, ",\"alternatives\":[]"); }
        char tail[100];
        int count=snprintf(tail, sizeof(tail), ",\"truncation\":{\"message\":%s,\"metadata\":%s}}\n", clipped ? "true" : "false", omitted ? "true" : "false");
        memcpy(out->bytes+out->count, tail, (size_t)count); out->count+=(size_t)count;
        fwrite(out->bytes, 1, out->count, stderr);
    } else if(SpanPath(span)[0] != '\0') {
        fprintf(stderr, "%s:%d:%d: %s%s\n", SpanPath(span), span.line, span.column,
                warning ? "warning: " : "", buffers->message);
    } else fprintf(stderr, "ziran: %s%s\n", warning ? "warning: " : "", buffers->message);
    if(!json && details && details->related_message && details->related_span.line > 0)
        fprintf(stderr, "%s:%d:%d: note: %s\n", SpanPath(details->related_span),
                details->related_span.line, details->related_span.column, details->related_message);
    atomic_flag_clear_explicit(&diagnostic_lock, memory_order_release);
}
void DiagnosticV(ZirSourceSpan span, const char *code, const char *format, va_list args)
{
    DiagnosticVBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    report(span, "error", code, NULL, format, args, buffers); free(buffers);
}
void Diagnostic(ZirSourceSpan span, const char *code, const char *format, ...)
{
    va_list args; va_start(args, format); DiagnosticV(span, code, format, args); va_end(args);
}
void DiagnosticDetailed(ZirSourceSpan span, const char *code, const DiagnosticDetails *details, const char *format, ...)
{
    DiagnosticVBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    va_list args; va_start(args, format); report(span, "error", code, details, format, args, buffers);
    va_end(args); free(buffers);
}
void Warning(ZirSourceSpan span, const char *code, const char *format, ...)
{
    DiagnosticVBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    va_list args; va_start(args, format); report(span, "warning", code, NULL, format, args, buffers);
    va_end(args); free(buffers);
}
void DiagnosticTarget(ZirSourceSpan span, const char *code, const char *target, const char *capability, const char *format, ...)
{
    DiagnosticDetails details = {.target=target, .capability=capability, .cause_kind="target_restriction"};
    DiagnosticVBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    va_list args; va_start(args, format); report(span, "error", code, &details, format, args, buffers);
    va_end(args); free(buffers);
}
