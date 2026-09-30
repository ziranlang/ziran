#include "zir_text.h"
#include "zir.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
is_ident_char(int c)
{
    return isalnum((unsigned char)c) || c == '_';
}

const char *
skip_ws(const char *s)
{
    while(s != NULL && isspace((unsigned char)*s))
        s++;
    return s;
}

const char *
skip_inline_ws(const char *s)
{
    while(s != NULL && (*s == ' ' || *s == '\t'))
        s++;
    return s;
}

char *
trim(char *s)
{
    char *e;

    if(s == NULL)
        return NULL;
    while(isspace((unsigned char)*s))
        s++;
    e = s + strlen(s);
    while(e > s && isspace((unsigned char)e[-1]))
        *--e = '\0';
    return s;
}

char *
trim_in_place(char *s)
{
    char *p;

    if(s == NULL)
        return NULL;
    p = trim(s);
    if(p != s)
        memmove(s, p, strlen(p) + 1);
    return s;
}

void
strip_block_brace(char *s)
{
    size_t n;

    if(s == NULL)
        return;
    n = strlen(s);
    while(n > 0 && isspace((unsigned char)s[n - 1]))
        n--;
    if(n > 0 && s[n - 1] == '{')
        n--;
    while(n > 0 && isspace((unsigned char)s[n - 1]))
        n--;
    s[n] = '\0';
}

void
camel_ident(const char *s, char *dst, size_t dst_size)
{
    size_t n = 0;
    int up = 1;

    if(dst_size == 0)
        return;
    for(const char *p = s; p != NULL && *p != '\0' && n + 1 < dst_size; p++) {
        if(isalnum((unsigned char)*p)) {
            dst[n++] = up ? (char)toupper((unsigned char)*p) : *p;
            up = 0;
        } else {
            up = 1;
        }
    }
    if(n == 0 && n + 1 < dst_size)
        dst[n++] = 'X';
    if(n > 0 && isdigit((unsigned char)dst[0]) && n + 1 < dst_size) {
        memmove(dst + 1, dst, n + 1);
        dst[0] = 'M';
        n++;
    }
    dst[n] = '\0';
}

void
go_field_ident(const char *s, char *dst, size_t dst_size)
{
    static const struct {
        const char *camel;
        const char *go;
    } initialisms[] = {
        {"Id", "ID"},
        {"FocusId", "FocusID"},
        {"MenuId", "MenuID"},
        {"SelectedId", "SelectedID"},
        {"ActivatedId", "ActivatedID"},
        {"CanonicalUrl", "CanonicalURL"},
    };

    if(dst_size == 0)
        return;
    camel_ident(s, dst, dst_size);
    for(size_t i = 0; i < sizeof(initialisms) / sizeof(initialisms[0]); i++) {
        if(strcmp(dst, initialisms[i].camel) == 0) {
            snprintf(dst, dst_size, "%s", initialisms[i].go);
            return;
        }
    }
}

/* Escape s for safe inclusion inside a C string literal in generated code:
 * quotes, backslashes, and non-printable bytes cannot terminate or alter the
 * surrounding literal. Returns the number of chars written; dst always holds
 * a NUL-terminated prefix of the escaped text when dst_size > 0. */
size_t
escape_c_string(const char *s, char *dst, size_t dst_size)
{
    size_t n = 0;

    if(dst_size == 0)
        return 0;
    for(const char *p = s != NULL ? s : ""; *p != '\0'; p++) {
        char buf[8];
        const char *rep = NULL;

        switch(*p) {
        case '"': rep = "\\\""; break;
        case '\\': rep = "\\\\"; break;
        case '\n': rep = "\\n"; break;
        case '\r': rep = "\\r"; break;
        case '\t': rep = "\\t"; break;
        default:
            if((unsigned char)*p < 0x20 || (unsigned char)*p == 0x7f) {
                snprintf(buf, sizeof(buf), "\\x%02x", (unsigned char)*p);
                rep = buf;
            }
            break;
        }
        if(rep == NULL) {
            buf[0] = *p;
            buf[1] = '\0';
            rep = buf;
        }
        for(const char *q = rep; *q != '\0'; q++) {
            if(n + 1 < dst_size)
                dst[n++] = *q;
        }
    }
    dst[n] = '\0';
    return n;
}

int
split_top_level(const char *s, char *parts, int max, size_t part_size)
{
    int depth = 0;
    int n = 0;
    const char *start = s;

    if(s == NULL || parts == NULL || max <= 0 || part_size == 0)
        return 0;
    for(const char *p = s; ; p++) {
        if(*p == '"' || *p == '\'') {
            char q = *p++;

            while(*p != '\0' && *p != q) {
                if(*p == '\\' && p[1] != '\0')
                    p++;
                p++;
            }
        } else if(*p == '\0' || (*p == ',' && depth == 0)) {
            size_t len = (size_t)(p - start);

            if(n < max && len < part_size) {
                char *part = parts + ((size_t)n * part_size);

                memcpy(part, start, len);
                part[len] = '\0';
                trim_in_place(part);
                n++;
            }
            if(*p == '\0')
                break;
            start = p + 1;
        } else if(*p == '(' || *p == '[' || *p == '{') {
            depth++;
        } else if(*p == ')' || *p == ']' || *p == '}') {
            if(depth > 0)
                depth--;
        }
    }
    return n;
}

char *
top_level_assignment(char *s)
{
    int depth = 0;
    for(char *p = s; *p != '\0'; p++) {
        if(*p == '"' || *p == '\'') {
            char quote = *p++;
            while(*p != '\0' && *p != quote) {
                if(*p == '\\' && p[1] != '\0') p++;
                p++;
            }
            if(*p == '\0') break;
        } else if(*p == '(' || *p == '[' || *p == '{') {
            depth++;
        } else if(*p == ')' || *p == ']' || *p == '}') {
            if(depth > 0) depth--;
        } else if(*p == '=' && depth == 0 && p[1] != '=' &&
                  (p == s || (p[-1] != '=' && p[-1] != '!' &&
                              p[-1] != '<' && p[-1] != '>'))) {
            return p;
        }
    }
    return NULL;
}

/* Decode one checked source literal into immutable UTF-8 bytes. The same
 * escapes and Unicode validity rules apply to native and portable strings. */
int
DecodeStringLiteral(const char *source, unsigned char *out, size_t capacity,
              size_t *length)
{
    size_t size = strlen(source);
    size_t used = 0;
    int remaining = 0;
    unsigned int scalar = 0, minimum = 0;
    if(size < 2 || source[0] != '"' || source[size - 1] != '"')
        return 0;
    for(size_t i = 1; i + 1 < size; i++) {
        unsigned int value = (unsigned char)source[i];
        int unicode_escape = 0;
        if(value == '\\') {
            if(++i + 1 >= size)
                return 0;
            value = (unsigned char)source[i];
            const char *escapes = "0abfnrtv\\\"";
            const unsigned char values[] = {0, 7, 8, 12, 10, 13, 9, 11, '\\', '"'};
            const char *found = strchr(escapes, (int)value);
            if(found != NULL) {
                value = values[found - escapes];
            } else if(value == 'x' || value == 'u' || value == 'U') {
                unicode_escape = value != 'x';
                int digits = value == 'x' ? 2 : value == 'u' ? 4 : 8;
                value = 0;
                for(int d = 0; d < digits; d++) {
                    if(++i + 1 >= size)
                        return 0;
                    int digit = (unsigned char)source[i];
                    if(digit >= '0' && digit <= '9') digit -= '0';
                    else if(digit >= 'a' && digit <= 'f') digit -= 'a' - 10;
                    else if(digit >= 'A' && digit <= 'F') digit -= 'A' - 10;
                    else return 0;
                    value = value * 16 + (unsigned int)digit;
                }
                if(value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
                    return 0;
            } else {
                return 0;
            }
        } else if(value == '"') {
            return 0;
        }
        unsigned char bytes[4] = {(unsigned char)value, 0, 0, 0};
        int count = 1;
        if(unicode_escape && value >= 128) {
            if(value < 0x800) {
                bytes[0] = 0xc0 | (value >> 6);
                bytes[1] = 0x80 | (value & 63);
                count = 2;
            } else if(value < 0x10000) {
                bytes[0] = 0xe0 | (value >> 12);
                bytes[1] = 0x80 | ((value >> 6) & 63);
                bytes[2] = 0x80 | (value & 63);
                count = 3;
            } else {
                bytes[0] = 0xf0 | (value >> 18);
                bytes[1] = 0x80 | ((value >> 12) & 63);
                bytes[2] = 0x80 | ((value >> 6) & 63);
                bytes[3] = 0x80 | (value & 63);
                count = 4;
            }
        }
        for(int d = 0; d < count; d++) {
            unsigned char byte = bytes[d];
            if(remaining) {
                if((byte & 0xc0) != 0x80)
                    return 0;
                scalar = (scalar << 6) | (byte & 63);
                remaining--;
                if(!remaining && (scalar < minimum || scalar > 0x10ffff ||
                    (scalar >= 0xd800 && scalar <= 0xdfff)))
                    return 0;
            } else if(byte >= 128) {
                if(byte >= 0xc2 && byte <= 0xdf) {
                    remaining = 1;
                    scalar = byte & 31;
                    minimum = 0x80;
                } else if(byte >= 0xe0 && byte <= 0xef) {
                    remaining = 2;
                    scalar = byte & 15;
                    minimum = 0x800;
                } else if(byte >= 0xf0 && byte <= 0xf4) {
                    remaining = 3;
                    scalar = byte & 7;
                    minimum = 0x10000;
                } else {
                    return 0;
                }
            }
            if(used >= capacity)
                return 0;
            out[used++] = byte;
        }
    }
    if(remaining)
        return 0;
    *length = used;
    return 1;
}

static int
encode_string_literal(const unsigned char *bytes, size_t length, char *out,
                      size_t capacity)
{
    size_t used = 0;
    if(capacity < 3)
        return 0;
    out[used++] = '"';
    for(size_t i = 0; i < length; i++) {
        char escaped[8];
        unsigned char byte = bytes[i];
        if(byte == '"' || byte == '\\')
            snprintf(escaped, sizeof(escaped), "\\%c", byte);
        else if(byte == '\n')
            snprintf(escaped, sizeof(escaped), "\\n");
        else if(byte == '\t')
            snprintf(escaped, sizeof(escaped), "\\t");
        else if(byte < 0x20 || byte == 0x7f)
            snprintf(escaped, sizeof(escaped), "\\x%02x", byte);
        else {
            escaped[0] = (char)byte;
            escaped[1] = '\0';
        }
        size_t size = strlen(escaped);
        if(used + size + 2 > capacity)
            return 0;
        memcpy(out + used, escaped, size);
        used += size;
    }
    out[used++] = '"';
    out[used] = '\0';
    return 1;
}
/* Buffers PrintFormatPieces keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct PrintFormatPiecesBuffers {
    unsigned char bytes[4096];
    unsigned char text[4096];
} PrintFormatPiecesBuffers;

int PrintFormatPieces(const char *format, PrintPiece *pieces, int capacity);

static int
PrintFormatPieces_with_buffers(const char *format, PrintPiece *pieces, int capacity, PrintFormatPiecesBuffers *buffers)
{
    size_t length, pending = 0;
    int count = 0;
    if(!DecodeStringLiteral(format, buffers->bytes, sizeof(buffers->bytes), &length))
        return -1;
    for(size_t i = 0; i <= length; i++) {
        int placeholder = i < length && buffers->bytes[i] == '%' &&
                          (i + 1 >= length || buffers->bytes[i + 1] != '%');
        if(i < length && !placeholder) {
            buffers->text[pending++] = buffers->bytes[i];
            if(buffers->bytes[i] == '%' && i + 1 < length && buffers->bytes[i + 1] == '%')
                i++;
            continue;
        }
        if(pending > 0) {
            if(count >= capacity ||
               !encode_string_literal(buffers->text, pending, pieces[count].literal,
                                      sizeof(pieces[count].literal)))
                return -1;
            pieces[count++].is_argument = 0;
            pending = 0;
        }
        if(placeholder) {
            if(count >= capacity)
                return -1;
            pieces[count].literal[0] = '\0';
            pieces[count++].is_argument = 1;
        }
    }
    return count;
}

/* Split a `print` format literal at `%` placeholders; `%%` is one literal
 * percent. Literal pieces are re-encoded as Ziran string literals so every
 * backend lowers them through its ordinary string path. Empty literal pieces
 * are omitted. Returns the piece count, or -1 for an invalid literal or when
 * the pieces do not fit. */
int
PrintFormatPieces(const char *format, PrintPiece *pieces, int capacity)
{
    static _Thread_local PrintFormatPiecesBuffers *spares[16];
    static _Thread_local int spare_count;
    PrintFormatPiecesBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = PrintFormatPieces_with_buffers(format, pieces, capacity, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
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

static const struct { const char *op, *name; } operator_procedures[] = {
    {"==", "operator_equal"}, {"!=", "operator_not_equal"},
    {"<=", "operator_less_equal"}, {">=", "operator_greater_equal"},
    {"<<", "operator_shift_left"}, {">>", "operator_shift_right"},
    {"+", "operator_add"}, {"-", "operator_subtract"},
    {"*", "operator_multiply"}, {"/", "operator_divide"},
    {"%", "operator_modulo"}, {"<", "operator_less"}, {">", "operator_greater"},
    {"&", "operator_bit_and"}, {"|", "operator_bit_or"}, {"^", "operator_bit_xor"},
};

const char *
OperatorProcedureName(const char *op)
{
    for(size_t i = 0; i < sizeof(operator_procedures) / sizeof(operator_procedures[0]); i++)
        if(strcmp(operator_procedures[i].op, op) == 0)
            return operator_procedures[i].name;
    return NULL;
}

size_t
OperatorTokenLength(const char *text)
{
    /* Two-character operators come first in the table. */
    for(size_t i = 0; i < sizeof(operator_procedures) / sizeof(operator_procedures[0]); i++) {
        size_t length = strlen(operator_procedures[i].op);
        if(strncmp(text, operator_procedures[i].op, length) == 0)
            return length;
    }
    return 0;
}
