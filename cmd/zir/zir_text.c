#include "zir_text.h"
#include "zir.h"
#include "compiler_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* C owns interned storage and the old frontend buffer ABI. Text rules come
 * from cmd/compiler_text.zi, compiled during every ordinary tool build. */
static String
source_text(const char *text)
{
    return StringView(text != NULL ? text : "", text != NULL ? strlen(text) : 0);
}

static Slice
output_bytes(void *data, size_t capacity)
{
    return (Slice){data, (int64_t)capacity};
}

int
is_ident_char(int c)
{
    return compiler_text_IdentifierByte((unsigned char)c);
}

const char *
skip_ws(const char *s)
{
    if(s == NULL) return NULL;
    return s + compiler_text_SkipSpace(source_text(s), 0, false);
}

const char *
skip_inline_ws(const char *s)
{
    if(s == NULL) return NULL;
    return s + compiler_text_SkipSpace(source_text(s), 0, true);
}

char *
trim(char *s)
{
    if(s == NULL) return NULL;
    SourceRange range = compiler_text_TrimRange(source_text(s));
    s[range.end] = '\0';
    return s + range.begin;
}

char *
trim_in_place(char *s)
{
    if(s == NULL) return NULL;
    char *p = trim(s);
    if(p != s) memmove(s, p, strlen(p) + 1);
    return s;
}

void
strip_block_brace(char *s)
{
    if(s != NULL) s[compiler_text_BlockHeaderEnd(source_text(s))] = '\0';
}

void
camel_ident(const char *s, char *dst, size_t dst_size)
{
    compiler_text_CamelIdentifier(source_text(s), output_bytes(dst, dst_size));
}

void
go_field_ident(const char *s, char *dst, size_t dst_size)
{
    compiler_text_GoFieldIdentifier(source_text(s), output_bytes(dst, dst_size));
}

size_t
escape_c_string(const char *s, char *dst, size_t dst_size)
{
    return compiler_text_EscapeCString(source_text(s), output_bytes(dst, dst_size));
}

int
split_top_level(const char *s, char *parts, int max, size_t part_size)
{
    if(s == NULL || parts == NULL || max <= 0 || part_size == 0) return 0;
    String source = source_text(s);
    int count = 0;
    int64_t at = 0;
    while(at >= 0 && count < max) {
        SourcePart part = compiler_text_NextSourcePart(source, at);
        size_t length = (size_t)(part.end - part.begin);
        if(length < part_size) {
            char *output = parts + (size_t)count * part_size;
            memcpy(output, s + part.begin, length);
            output[length] = '\0';
            trim_in_place(output);
            count++;
        }
        at = part.next;
    }
    return count;
}

/* Parameter lists by their kept text; a kept text is one pointer for one
 * spelling, so the pointer identifies the list. */
static struct {
    const char **texts;
    const ZirParameters **lists;
    size_t count, slots;
} parameter_lists;

static size_t
parameter_slot(const char *kept)
{
    size_t slot = ((uintptr_t)kept >> 4) * 11400714819323198485ull;
    slot &= parameter_lists.slots - 1;
    while(parameter_lists.texts[slot] != NULL && parameter_lists.texts[slot] != kept)
        slot = (slot + 1) & (parameter_lists.slots - 1);
    return slot;
}

static const ZirParameters *
split_parameters(const char *text)
{
    enum { MAX = 64 };
    char (*parts)[ZIR_TEXT_MAX] = NULL;
    int count = 0;
    if(*skip_ws(text)) {
        parts = AllocateOrExit(MAX * sizeof(*parts));
        count = split_top_level(text, parts[0], MAX, sizeof(parts[0]));
    }
    ZirParameters *list = AllocateOrExit(sizeof(*list) +
                                         (size_t)count * sizeof(list->items[0]));
    list->count = count;
    for(int i = 0; i < count; i++) {
        ZirParameter *parameter = &list->items[i];
        parameter->text = KeepText(parts[i]);
        parameter->name = parameter->type = NULL;
        char *colon = strchr(parts[i], ':');
        if(colon != NULL) {
            parameter->type = KeepText(skip_ws(colon + 1));
            *colon = '\0';
            parameter->name = KeepText(trim(parts[i]));
        }
    }
    free(parts);
    return list;
}

const ZirParameters *
ParametersOf(const char *text)
{
    const char *kept = KeepText(text);
    if(parameter_lists.count * 2 >= parameter_lists.slots) {
        size_t old_slots = parameter_lists.slots;
        const char **old_texts = parameter_lists.texts;
        const ZirParameters **old_lists = parameter_lists.lists;
        parameter_lists.slots = old_slots ? old_slots * 2 : 1024;
        parameter_lists.texts = calloc(parameter_lists.slots, sizeof(*parameter_lists.texts));
        parameter_lists.lists = calloc(parameter_lists.slots, sizeof(*parameter_lists.lists));
        if(parameter_lists.texts == NULL || parameter_lists.lists == NULL) {
            fprintf(stderr, "ziran: out of memory keeping parameter lists\n");
            exit(1);
        }
        for(size_t i = 0; i < old_slots; i++)
            if(old_texts[i] != NULL) {
                size_t slot = parameter_slot(old_texts[i]);
                parameter_lists.texts[slot] = old_texts[i];
                parameter_lists.lists[slot] = old_lists[i];
            }
        free(old_texts);
        free(old_lists);
    }
    size_t slot = parameter_slot(kept);
    if(parameter_lists.texts[slot] == NULL) {
        parameter_lists.texts[slot] = kept;
        parameter_lists.lists[slot] = split_parameters(kept);
        parameter_lists.count++;
    }
    return parameter_lists.lists[slot];
}

char *
top_level_assignment(char *s)
{
    int64_t at = compiler_text_TopLevelAssignment(source_text(s));
    return at < 0 ? NULL : s + at;
}

int
DecodeStringLiteral(const char *source, unsigned char *out, size_t capacity,
                    size_t *length)
{
    DecodedLiteral decoded = compiler_text_DecodeString(source_text(source),
                                                       output_bytes(out, capacity));
    if(!decoded.valid) return 0;
    *length = (size_t)decoded.count;
    return 1;
}

/* Scratch belongs to the C frontend and stays off its recursive stack. */
typedef struct PrintFormatPiecesBuffers {
    unsigned char bytes[4096];
    char literal[4096];
} PrintFormatPiecesBuffers;

static int
print_format_pieces(const char *format, PrintPiece *pieces, int capacity,
                    PrintFormatPiecesBuffers *buffers)
{
    size_t length;
    if(!DecodeStringLiteral(format, buffers->bytes, sizeof(buffers->bytes), &length))
        return -1;
    Slice bytes = output_bytes(buffers->bytes, length);
    int count = 0;
    int64_t at = 0;
    for(;;) {
        FormatStep step = compiler_text_NextPrintPiece(bytes, at,
            output_bytes(buffers->literal, sizeof(buffers->literal)));
        if(!step.valid) return -1;
        if(!step.present) return count;
        if(count >= capacity) return -1;
        pieces[count].is_argument = step.argument;
        memcpy(pieces[count].literal, buffers->literal, strlen(buffers->literal) + 1);
        count++;
        at = step.next;
    }
}

int
PrintFormatPieces(const char *format, PrintPiece *pieces, int capacity)
{
    static _Thread_local PrintFormatPiecesBuffers *spares[16];
    static _Thread_local int spare_count;
    PrintFormatPiecesBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = print_format_pieces(format, pieces, capacity, buffers);
    if(spare_count < 16) spares[spare_count++] = buffers;
    else free(buffers);
    return returned;
}

/* Portable `print` float text: the shortest decimal that reads back to the
 * same value, in positional notation. Native C/C++ output uses the matching
 * print_float helper in include/zir_string.h. */
void
FormatPrintFloat(double value, int single, char *out, size_t capacity)
{
    char scientific[40], digits[24];
    int precision, exponent, count = 0;
    size_t used = 0;
    const char *p;
    if(value != value) {
        snprintf(out, capacity, "nan");
        return;
    }
    if(value - value != 0) {
        snprintf(out, capacity, "%s", value < 0 ? "-inf" : "inf");
        return;
    }
    for(precision = 1; precision <= 17; precision++) {
        snprintf(scientific, sizeof(scientific), "%.*e", precision - 1, value);
        if(single ? strtof(scientific, NULL) == (float)value
                  : strtod(scientific, NULL) == value)
            break;
    }
    p = scientific;
#define PUT(c) do { if(used + 1 < capacity) out[used++] = (c); } while(0)
    if(*p == '-') {
        PUT('-');
        p++;
    }
    for(; *p != 'e'; p++)
        if(*p != '.')
            digits[count++] = *p;
    exponent = atoi(p + 1);
    while(count > 1 && digits[count - 1] == '0')
        count--;
    if(exponent < 0) {
        PUT('0');
        PUT('.');
        for(int i = 1; i < -exponent; i++)
            PUT('0');
        for(int i = 0; i < count; i++)
            PUT(digits[i]);
    } else {
        for(int i = 0; i <= exponent; i++)
            PUT(i < count ? digits[i] : '0');
        if(count > exponent + 1) {
            PUT('.');
            for(int i = exponent + 1; i < count; i++)
                PUT(digits[i]);
        }
    }
#undef PUT
    if(capacity > 0)
        out[used] = '\0';
}

const char *
OperatorProcedureName(const char *op)
{
    String name = compiler_text_OperatorProcedure(source_text(op));
    return name.length > 0 ? name.data : NULL;
}

size_t
OperatorTokenLength(const char *text)
{
    return compiler_text_OperatorTokenLength(source_text(text));
}

const char *
OperatorOfProcedure(const char *name)
{
    String op = compiler_text_ProcedureOperator(source_text(name));
    return op.length > 0 ? op.data : NULL;
}
