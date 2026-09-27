/*
 * zir_parse.c - shared Ziran frontend: parse .zi source into a ZirProgram.
 * Linked by the IR and native backends.
 */
#include "zir.h"
#include "zir_parse.h"
#include "zir_text.h"
#include "zir_cleanup.h"
#include "zir_expr.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_token.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SOURCE_PATH_MAX = 1024,
    SOURCE_LINE_MAX = 4096
};

static void
die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    DiagnosticV(Span("", 0, 0), "parse.fatal", fmt, ap);
    va_end(ap);
    exit(1);
}

static void
die_at(ZirSourceSpan span, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    DiagnosticV(span, "parse.syntax", fmt, args);
    va_end(args);
    exit(1);
}

typedef struct SourceBuffer {
    char *text;
    size_t length;
    size_t capacity;
} SourceBuffer;

static void
source_append(SourceBuffer *buffer, char byte)
{
    if(buffer->length + 2 > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity * 2 : 8192;
        if(capacity < buffer->capacity || capacity < buffer->length + 2)
            die("source exceeds available memory");
        char *next = realloc(buffer->text, capacity);
        if(next == NULL) die("out of memory reading source");
        buffer->text = next;
        buffer->capacity = capacity;
    }
    buffer->text[buffer->length++] = byte;
    buffer->text[buffer->length] = '\0';
}

static void
source_append_scanned(SourceBuffer *buffer, char byte, int *line_gaps)
{
    source_append(buffer, byte);
    if(byte == '\n') {
        while(*line_gaps > 0) {
            source_append(buffer, '\n');
            (*line_gaps)--;
        }
    }
}

static int
source_identifier_byte(unsigned char byte)
{
    return isalnum(byte) || byte == '_';
}

/* Rewrite Jai's raw multiline token as a checked ordinary string literal.
 * Keep the original number of line breaks so later source spans stay put. */
static char *
lower_jai_multiline_strings(const char *source, const char *path)
{
    SourceBuffer output = {0};
    size_t length = strlen(source);
    size_t pos = 0;
    int line = 1, column = 1, quote = 0, line_comment = 0;
    int block_comment = 0;
    int line_gaps = 0;
    while(pos < length) {
        unsigned char byte = (unsigned char)source[pos];
        if(line_comment) {
            source_append_scanned(&output, (char)byte, &line_gaps);
            pos++;
            if(byte == '\n') { line_comment = 0; line++; column = 1; }
            else column++;
            continue;
        }
        if(block_comment) {
            if(byte == '/' && pos + 1 < length && source[pos + 1] == '*') {
                block_comment++;
                source_append(&output, '/'); source_append(&output, '*');
                pos += 2; column += 2;
                continue;
            }
            if(byte == '*' && pos + 1 < length && source[pos + 1] == '/') {
                block_comment--;
                source_append(&output, '*'); source_append(&output, '/');
                pos += 2; column += 2;
                continue;
            }
            source_append_scanned(&output, (char)byte, &line_gaps);
            pos++;
            if(byte == '\n') { line++; column = 1; }
            else column++;
            continue;
        }
        if(quote) {
            source_append_scanned(&output, (char)byte, &line_gaps);
            pos++;
            if(byte == '\\' && pos < length) {
                source_append_scanned(&output, source[pos++], &line_gaps);
                column += 2;
            } else if(byte == quote) {
                quote = 0;
                column++;
            } else if(byte == '\n') { line++; column = 1; }
            else column++;
            continue;
        }
        if(byte == '/' && pos + 1 < length && source[pos + 1] == '/') {
            line_comment = 1;
            source_append(&output, '/'); source_append(&output, '/');
            pos += 2; column += 2;
            continue;
        }
        if(byte == '/' && pos + 1 < length && source[pos + 1] == '*') {
            block_comment = 1;
            source_append(&output, '/'); source_append(&output, '*');
            pos += 2; column += 2;
            continue;
        }
        if(byte == '"' || byte == '\'') {
            quote = byte;
            source_append(&output, (char)byte);
            pos++; column++;
            continue;
        }
        if(byte == '#' && pos + 7 <= length &&
           strncmp(source + pos, "#string", 7) == 0 &&
           (pos == 0 || !source_identifier_byte((unsigned char)source[pos - 1])) &&
           (pos + 7 == length || isspace((unsigned char)source[pos + 7]))) {
            ZirSourceSpan span = Span(path, line, column);
            size_t cursor = pos + 7, start, marker_length;
            while(cursor < length &&
                  (source[cursor] == ' ' || source[cursor] == '\t')) cursor++;
            start = cursor;
            while(cursor < length &&
                  source_identifier_byte((unsigned char)source[cursor])) cursor++;
            marker_length = cursor - start;
            if(marker_length == 0 || marker_length >= ZIR_NAME_MAX ||
               (!isalpha((unsigned char)source[start]) &&
                source[start] != '_'))
                die_at(span, "#string requires an identifier delimiter");
            while(cursor < length &&
                  (source[cursor] == ' ' || source[cursor] == '\t')) cursor++;
            if(cursor < length && source[cursor] == '\r') cursor++;
            if(cursor >= length || source[cursor] != '\n')
                die_at(span, "#string delimiter must end its line");
            size_t body_start = cursor + 1, close = body_start;
            while(close < length) {
                if(close + marker_length <= length &&
                   strncmp(source + close, source + start,
                           marker_length) == 0 &&
                   (close + marker_length == length ||
                    !source_identifier_byte(
                        (unsigned char)source[close + marker_length])))
                    break;
                const char *newline = memchr(source + close, '\n',
                                             length - close);
                if(newline == NULL) { close = length; break; }
                close = (size_t)(newline - source) + 1;
            }
            if(close >= length)
                die_at(span, "unterminated #string delimiter: %.*s",
                       (int)marker_length, source + start);
            source_append(&output, '"');
            for(size_t i = body_start; i < close; i++) {
                unsigned char raw = (unsigned char)source[i];
                const char *escape = raw == '"' ? "\\\"" :
                                     raw == '\\' ? "\\\\" :
                                     raw == '\n' ? "\\n" :
                                     raw == '\r' ? "\\r" :
                                     raw == '\t' ? "\\t" : NULL;
                if(escape != NULL)
                    for(const char *part = escape; *part; part++)
                        source_append(&output, *part);
                else if(raw < 32 || raw == 127) {
                    static const char digits[] = "0123456789abcdef";
                    source_append(&output, '\\');
                    source_append(&output, 'x');
                    source_append(&output, digits[raw >> 4]);
                    source_append(&output, digits[raw & 15]);
                } else source_append(&output, (char)raw);
            }
            source_append(&output, '"');
            for(size_t i = pos; i < close; i++)
                if(source[i] == '\n') { line++; line_gaps++; }
            column = (int)marker_length + 1;
            pos = close + marker_length;
            continue;
        }
        source_append_scanned(&output, (char)byte, &line_gaps);
        pos++;
        if(byte == '\n') { line++; column = 1; }
        else column++;
    }
    if(output.text == NULL) {
        output.text = malloc(1);
        if(output.text == NULL) die("out of memory reading source");
        output.text[0] = '\0';
    }
    return output.text;
}

static int
starts_word(const char *s, const char *word)
{
    size_t n = strlen(word);

    return strncmp(s, word, n) == 0 &&
           (s[n] == '\0' || s[n] == ' ' || s[n] == '\t' ||
            s[n] == '(' || s[n] == '"' || s[n] == '{');
}

static int
looks_like_non_jai_control(const char *text, const char *word)
{
    const char *rest;

    if(!starts_word(text, word))
        return 0;
    rest = skip_ws(text + strlen(word));
    if(*rest == '(')
        return strchr(rest, '{') != NULL;
    if(*rest == ':' || *rest == '=' || *rest == '.' || *rest == '[' ||
       *rest == '+' || *rest == '-' || *rest == '*' || *rest == '/' ||
       *rest == '%' || *rest == '&' || *rest == '|' || *rest == '^' ||
       *rest == '<' || *rest == '>' || *rest == '!')
        return 0;
    return *rest == '\0' || *rest == ';' || *rest == '{' ||
           isalnum((unsigned char)*rest) || *rest == '_';
}

static int
is_identifier_text(const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    if(cursor == NULL || (!isalpha(*cursor) && *cursor != '_'))
        return 0;
    cursor++;
    while(isalnum(*cursor) || *cursor == '_')
        cursor++;
    return *cursor == '\0';
}

/* Parse a using modifier clause: `, only("a", "b")`, `, except("c")`, or
 * `, map("x" = "a", "y" = "b")` directly after the `using` keyword. On
 * success `cursor` advances past the clause and a compact filter string
 * ("O:a,b" / "E:c" / "M:x=a,y=b") is written for the checker. */
static int
parse_using_modifiers(const char **cursor, char *filter, size_t filter_size,
                      const char *path, int line_no)
{
    const char *start = *cursor;
    const char *kind_text;
    char kind;
    size_t used = 0;
    int entries = 0;
    if(*start != ',')
        return 1;
    kind_text = skip_ws(start + 1);
    if(starts_word(kind_text, "only")) {
        kind = 'O';
        kind_text = kind_text + 4;
    } else if(starts_word(kind_text, "except")) {
        kind = 'E';
        kind_text = kind_text + 6;
    } else if(starts_word(kind_text, "map")) {
        kind = 'M';
        kind_text = kind_text + 3;
    } else {
        die_at(Span(path, line_no, 1),
               "using modifier must be only, except, or map");
        return 0;
    }
    kind_text = skip_ws(kind_text);
    if(*kind_text != '(')
        die_at(Span(path, line_no, 1),
               "using modifier requires a parenthesized name list");
    filter[used++] = kind;
    filter[used++] = ':';
    kind_text++;
    while(*kind_text != '\0' && *kind_text != ')') {
        const char *item = skip_ws(kind_text);
        char name[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX];
        size_t length = 0;
        if(*item != '"')
            die_at(Span(path, line_no, 1),
                   "using modifier entries must be quoted names");
        item++;
        while(*item != '\0' && *item != '"' &&
              length + 1 < sizeof(name))
            name[length++] = *item++;
        if(*item != '"' || length == 0)
            die_at(Span(path, line_no, 1),
                   "using modifier entries must be quoted names");
        name[length] = '\0';
        item = skip_ws(item + 1);
        mapped[0] = '\0';
        if(kind == 'M') {
            if(*item != '=')
                die_at(Span(path, line_no, 1),
                       "using map entries need \"new\" = \"old\"");
            item = skip_ws(item + 1);
            if(*item != '"')
                die_at(Span(path, line_no, 1),
                       "using map entries need \"new\" = \"old\"");
            item++;
            length = 0;
            while(*item != '\0' && *item != '"' &&
                  length + 1 < sizeof(mapped))
                mapped[length++] = *item++;
            if(*item != '"' || length == 0)
                die_at(Span(path, line_no, 1),
                       "using map entries need \"new\" = \"old\"");
            mapped[length] = '\0';
            item = skip_ws(item + 1);
        }
        if(used + strlen(name) + strlen(mapped) + 2 >= filter_size)
            die_at(Span(path, line_no, 1),
                   "using modifier list is too long");
        if(entries++)
            filter[used++] = ',';
        used += (size_t)snprintf(filter + used, filter_size - used, "%s%s%s",
                                 name, mapped[0] ? "=" : "", mapped);
        kind_text = skip_ws(item);
        if(*kind_text == ',') {
            kind_text = skip_ws(kind_text + 1);
            continue;
        }
        break;
    }
    if(*kind_text != ')' || entries == 0)
        die_at(Span(path, line_no, 1),
               "using modifier requires a parenthesized name list");
    *cursor = skip_ws(kind_text + 1);
    return 1;
}

static int
is_member_path_text(const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    if(cursor == NULL || (!isalpha(*cursor) && *cursor != '_'))
        return 0;
    for(;;) {
        while(isalnum(*cursor) || *cursor == '_') cursor++;
        if(*cursor == '\0') return 1;
        if(*cursor++ != '.' || (!isalpha(*cursor) && *cursor != '_'))
            return 0;
    }
}

static void
parse_file_global(ZirModule *module, const char *declaration,
                  ZirSourceSpan span, int scope_public, int scope_file,
                  char *name_out, size_t name_size)
{
    char name[ZIR_NAME_MAX], type[ZIR_TEXT_MAX], init[ZIR_TEXT_MAX];
    const char *colon = strchr(declaration, ':');
    const char *value = colon == NULL ? NULL : colon + 1;
    const char *equal = value == NULL ? NULL : strchr(value, '=');
    const char *type_end = equal == NULL ?
        declaration + strlen(declaration) : equal;
    size_t length = 0;

    if(colon == NULL || declaration[0] == '\0' ||
       declaration[strlen(declaration) - 1] != ';')
        die_at(span, "file-scope variable declaration needs ';'");
    for(const char *cursor = declaration; cursor < colon; cursor++) {
        if(!isalnum((unsigned char)*cursor) && *cursor != '_')
            die_at(span, "invalid file-scope variable name");
        if(length + 1 >= sizeof(name))
            die_at(span, "file-scope variable name is too long");
        name[length++] = *cursor;
    }
    name[length] = '\0';
    if(!is_member_path_text(name) || strchr(name, '.') != NULL)
        die_at(span, "invalid file-scope variable name");
    value = skip_ws(value);
    length = 0;
    while(value < type_end && length + 1 < sizeof(type))
        type[length++] = *value++;
    if(value != type_end)
        die_at(span, "file-scope variable type is too long");
    while(length > 0 && (isspace((unsigned char)type[length - 1]) ||
                         type[length - 1] == ';')) length--;
    type[length] = '\0';
    if(type[0] == '\0')
        die_at(span, "file-scope variable needs a type");
    length = 0;
    if(equal != NULL) {
        value = skip_ws(equal + 1);
        while(*value != '\0' && length + 1 < sizeof(init))
            init[length++] = *value++;
        if(*value != '\0')
            die_at(span, "file-scope variable initializer is too long");
        while(length > 0 && (isspace((unsigned char)init[length - 1]) ||
                             init[length - 1] == ';')) length--;
        if(length == 0)
            die_at(span, "file-scope variable initializer is empty");
    }
    init[length] = '\0';
    if(scope_public)
        ModuleAddGlobal(module, name, type, init, span);
    else
        ModuleAddStatic(module, name, type, init, span);
    if(module->global_count > 0)
        module->globals[module->global_count - 1].is_file_private = scope_file;
    if(name_out != NULL)
        copy_text(name_out, name_size, name);
}

static int
contains_source_directive(const char *source, const char *directive)
{
    size_t length = strlen(directive);
    int quoted = 0;

    for(const char *cursor = source; *cursor != '\0'; cursor++) {
        if(quoted) {
            if(*cursor == '\\' && cursor[1] != '\0')
                cursor++;
            else if(*cursor == '"')
                quoted = 0;
        } else if(cursor[0] == '/' && cursor[1] == '/') {
            return 0;
        } else if(*cursor == '"') {
            quoted = 1;
        } else if(strncmp(cursor, directive, length) == 0 &&
                  !isalnum((unsigned char)cursor[length]) &&
                  cursor[length] != '_') {
            return 1;
        }
    }
    return 0;
}

static int
source_column_for_trimmed(const char *line, const char *trimmed)
{
    if(line == NULL || trimmed == NULL || trimmed < line)
        return 1;
    return (int)(trimmed - line) + 1;
}

static int
source_end_column_for_trimmed(const char *line, const char *trimmed)
{
    return source_column_for_trimmed(line, trimmed) + (int)strlen(trimmed);
}

static int
parse_symbol_before_colons(const char *s, char *out, size_t out_size)
{
    const char *p;
    const char *q;
    size_t n = 0;

    out[0] = '\0';
    p = strstr(s, "::");
    if(p == NULL)
        return 0;
    q = s;
    while(q < p && (*q == ' ' || *q == '\t'))
        q++;
    while(q < p && (isalnum((unsigned char)*q) || *q == '_') &&
          n + 1 < out_size)
        out[n++] = *q++;
    out[n] = '\0';
    while(q < p && (*q == ' ' || *q == '\t'))
        q++;
    return out[0] != '\0' && q == p;
}

/* Jai places polymorphic parameters on the type constructor, not on the
 * declaration name: Box :: struct($T: Type) { value: T; }.  The checker keeps
 * just the bound names for substitution. */
static int
parse_type_parameters(const char *after, const char *keyword,
                      char *names, size_t capacity)
{
    const char *cursor = skip_ws(after + strlen(keyword));
    char source[ZIR_TEXT_MAX];
    char parts[16][ZIR_NAME_MAX];
    size_t used = 0;
    int count;

    names[0] = '\0';
    if(*cursor != '(')
        return 1;
    const char *start = ++cursor;
    int depth = 1;
    while(*cursor && depth) {
        if(*cursor == '(') depth++;
        else if(*cursor == ')') depth--;
        if(depth) cursor++;
    }
    if(depth || cursor == start ||
       (size_t)(cursor - start) >= sizeof(source) ||
       *skip_ws(cursor + 1) != '{')
        return 0;
    memcpy(source, start, (size_t)(cursor - start));
    source[cursor - start] = '\0';
    count = split_top_level(source, parts[0], 16, sizeof(parts[0]));
    if(count < 1 || count >= 16)
        return 0;
    for(int i = 0; i < count; i++) {
        const char *part = parts[i];
        if(*part == '$') part++;
        const char *colon = strchr(part, ':');
        if(colon == NULL) return 0;
        size_t length = (size_t)(colon - part);
        while(length && isspace((unsigned char)part[length - 1])) length--;
        if(length == 0 || length >= ZIR_NAME_MAX ||
           strcmp(skip_ws(colon + 1), "Type") != 0)
            return 0;
        char name[ZIR_NAME_MAX];
        memcpy(name, part, length);
        name[length] = '\0';
        if(!is_identifier_text(name) ||
           used + length + (i ? 1u : 0u) >= capacity)
            return 0;
        if(i) names[used++] = ',';
        memcpy(names + used, name, length);
        used += length;
        names[used] = '\0';
    }
    return 1;
}

/* Resolve Jai's scope-relative type constant before field types enter IR. */
static int
expand_type_this(ZirType *type)
{
    char replacement[ZIR_NAME_MAX];
    char body[sizeof(type->body)];
    size_t used = 0;
    int quote = 0;
    int length = snprintf(replacement, sizeof(replacement), "%s%s%s%s",
        type->name, type->template_params[0] ? "(" : "",
        type->template_params, type->template_params[0] ? ")" : "");
    if(length < 0 || (size_t)length >= sizeof(replacement)) return 0;
    for(const char *cursor = type->body; *cursor; ) {
        if(!quote && strncmp(cursor, "#this", 5) == 0 &&
           !isalnum((unsigned char)cursor[5]) && cursor[5] != '_') {
            if(used + (size_t)length >= sizeof(body)) return 0;
            memcpy(body + used, replacement, (size_t)length);
            used += (size_t)length;
            cursor += 5;
            continue;
        }
        if(used + 1 >= sizeof(body)) return 0;
        if(quote && *cursor == '\\' && cursor[1]) {
            if(used + 2 >= sizeof(body)) return 0;
            body[used++] = *cursor++;
            body[used++] = *cursor++;
            continue;
        }
        if(*cursor == '"' || *cursor == '\'') {
            if(!quote) quote = *cursor;
            else if(quote == *cursor) quote = 0;
        }
        body[used++] = *cursor++;
    }
    body[used] = '\0';
    copy_text(type->body, sizeof(type->body), body);
    return 1;
}

static const char *
relative_path(const char *root, const char *path)
{
    size_t n;

    if(root == NULL || root[0] == '\0')
        return path;
    n = strlen(root);
    if(strncmp(path, root, n) == 0 && (path[n] == '/' || path[n] == '\0')) {
        if(path[n] == '/')
            return path + n + 1;
        return path + n;
    }
    return path;
}

static int
parse_quoted(const char *s, char *out, size_t out_size)
{
    const char *q;
    size_t n = 0;

    q = strchr(s, '"');
    if(q == NULL)
        return 0;
    q++;
    while(*q != '\0' && *q != '"' && n + 1 < out_size)
        out[n++] = *q++;
    out[n] = '\0';
    return *q == '"';
}

static int
is_c_ident(const char *s)
{
    if(s == NULL || s[0] == '\0')
        return 0;
    if(!(isalpha((unsigned char)s[0]) || s[0] == '_'))
        return 0;
    for(const char *p = s + 1; *p != '\0'; p++) {
        if(!(isalnum((unsigned char)*p) || *p == '_'))
            return 0;
    }
    return 1;
}

static ZirExternKind
classify_extern_target(const char *target, char *symbol, size_t symbol_size,
                       const char *path, int line_no)
{
    const char *dot;
    const char *slash;

    symbol[0] = '\0';
    if(target == NULL || target[0] == '\0')
        return ZIR_EXTERN_HOST;
    if(strncmp(target, "c.", 2) == 0) {
        if(!is_c_ident(target + 2))
            die_at(Span(path, line_no, 1), "C extern target must be c.<symbol>");
        snprintf(symbol, symbol_size, "%s", target + 2);
        return ZIR_EXTERN_C;
    }
    dot = strrchr(target, '.');
    slash = strrchr(target, '/');
    if(dot != NULL && slash != NULL && slash < dot)
        return ZIR_EXTERN_GO;
    return ZIR_EXTERN_HOST;
}

/* Net block braces: only '{'/'}' at paren/bracket depth 0 open/close
 * blocks. Braces inside a call argument's record literal are expressions. */
static int
net_block_braces(const char *s)
{
    int pd = 0;
    int in_s = 0;
    int in_c = 0;
    int delta = 0;

    for(const char *p = s; *p != '\0'; p++) {
        if(in_s) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '"')
                in_s = 0;
        } else if(in_c) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '\'')
                in_c = 0;
        } else if(*p == '"') {
            in_s = 1;
        } else if(*p == '\'') {
            in_c = 1;
        } else if(*p == '(' || *p == '[') {
            pd++;
        } else if(*p == ')' || *p == ']') {
            if(pd > 0)
                pd--;
        } else if(pd == 0) {
            if(*p == '{')
                delta++;
            else if(*p == '}')
                delta--;
        }
    }
    return delta;
}

static int
looks_like_label(const char *s)
{
    size_t n = 0;
    while(isalnum((unsigned char)s[n]) || s[n] == '_')
        n++;
    if(n == 0 || s[n] != ':')
        return 0;
    const char *rest = skip_ws(s + n + 1);
    return *rest == '\0' || strcmp(rest, ";") == 0;
}

static ZirStmtKind
classify_stmt(const char *s)
{
    if(s[0] == '}')
        return ZIR_STMT_BLOCK_CLOSE;
    if(strcmp(s, "{") == 0)
        return ZIR_STMT_BLOCK_OPEN;
    if(starts_word(s, "if") || starts_word(s, "else"))
        return ZIR_STMT_IF;
    if(starts_word(s, "while"))
        return ZIR_STMT_WHILE;
    if(starts_word(s, "for"))
        return ZIR_STMT_FOR;
    if(starts_word(s, "case") || strcmp(s, "case;") == 0)
        return ZIR_STMT_CASE;
    if(starts_word(s, "return"))
        return ZIR_STMT_RETURN;
    if(strcmp(s, "unreachable") == 0 || strcmp(s, "unreachable;") == 0)
        return ZIR_STMT_UNREACHABLE;
    if(starts_word(s, "break"))
        return ZIR_STMT_BREAK;
    if(starts_word(s, "continue"))
        return ZIR_STMT_CONTINUE;
    if(starts_word(s, "defer"))
        return ZIR_STMT_DEFER;
    if(starts_word(s, "unused"))
        return ZIR_STMT_UNUSED;
    if(strstr(s, ":=") != NULL)
        return ZIR_STMT_DECL;   /* ':=' wins over the raw 'c' prefix (a
                                   variable may be named 'c') */
    if(strstr(s, ": ") != NULL || strstr(s, ": [") != NULL) {
        /* typed decl only when an identifier precedes the colon */
        const char *c2 = strstr(s, ": ");

        if(c2 == NULL)
            c2 = strstr(s, ": [");
        if(c2 != NULL && c2 > s &&
           (isalpha((unsigned char)s[0]) || s[0] == '_')) {
            int ident_only = 1;

            for(const char *q = s; q < c2; q++)
                if(!(isalnum((unsigned char)*q) || *q == '_'))
                    ident_only = 0;
            if(ident_only)
                return ZIR_STMT_DECL;  /* 'x: T' / 'x: [N] T' */
        }
    }
    /* Classify C-style locals so the source grammar can reject them before
     * any native backend sees an unchecked declaration. */
    {
        static const char *const types[] = {
            "s8 ", "s16 ", "s32 ", "s64 ",
            "float32 ", "float64 ", "bool ",
            "unsigned ", "long ", "const ", "struct ", NULL
        };
        int i;

        for(i = 0; types[i] != NULL; i++)
            if(strncmp(s, types[i], strlen(types[i])) == 0)
                return ZIR_STMT_DECL;
    }
    /* An '=' inside a call's record literal is a field initializer,
     * not an assignment statement (Make(Props.{value = input})). */
    {
        int depth = 0;
        int quote = 0;
        const char *p;

        for(p = s; *p != '\0'; p++) {
            if(quote) {
                if(*p == '\\' && p[1] != '\0')
                    p++;
                else if(*p == quote)
                    quote = 0;
                continue;
            }
            if(*p == '"' || *p == '\'')
                quote = *p;
            else if(*p == '(' || *p == '[' || *p == '{')
                depth++;
            else if(*p == ')' || *p == ']' || *p == '}') {
                if(depth > 0)
                    depth--;
            } else if(*p == '=' && depth == 0)
                return ZIR_STMT_ASSIGN;
        }
    }
    if(strchr(s, '(') != NULL || strchr(s, '+') != NULL ||
       strchr(s, '-') != NULL)
        return ZIR_STMT_EXPR;
    return ZIR_STMT_UNKNOWN;
}

static int
unwrap_outer_parentheses(const char *text, char *out, size_t out_size)
{
    const char *p = text;
    const char *open;
    const char *close = NULL;
    int depth = 0;
    int in_string = 0;

    while(*p == ' ' || *p == '\t')
        p++;
    if(*p != '(')
        return 0;
    open = p++;
    depth = 1;
    while(*p != '\0') {
        if(in_string) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '"')
                in_string = 0;
        } else if(*p == '"') {
            in_string = 1;
        } else if(*p == '(') {
            depth++;
        } else if(*p == ')' && --depth == 0) {
            close = p;
            break;
        }
        p++;
    }
    if(close == NULL)
        return 0;
    p = close + 1;
    while(*p == ' ' || *p == '\t' || *p == ';')
        p++;
    if(*p != '\0' || (size_t)(close - open) >= out_size)
        return 0;
    memcpy(out, open + 1, (size_t)(close - open - 1));
    out[close - open - 1] = '\0';
    trim_in_place(out);
    return out[0] != '\0';
}

static int
parse_direct_call_statement(const char *text, char *name, size_t name_size,
                            char *args, size_t args_size)
{
    const char *p = text;
    const char *open;
    const char *close;
    size_t length;
    int depth = 0;
    int in_string = 0;
    char inner[ZIR_TEXT_MAX];

    while(*p == ' ' || *p == '\t')
        p++;
    if(unwrap_outer_parentheses(p, inner, sizeof(inner)))
        return parse_direct_call_statement(inner, name, name_size, args,
                                           args_size);
    open = p;
    while(isalnum((unsigned char)*p) || *p == '_')
        p++;
    length = (size_t)(p - open);
    if(length == 0 || length >= name_size)
        return 0;
    memcpy(name, open, length);
    name[length] = '\0';
    while(*p == ' ' || *p == '\t')
        p++;
    if(*p != '(')
        return 0;
    open = p++;
    close = NULL;
    depth = 1;
    while(*p != '\0') {
        if(in_string) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '"')
                in_string = 0;
        } else if(*p == '"') {
            in_string = 1;
        } else if(*p == '(') {
            depth++;
        } else if(*p == ')' && --depth == 0) {
            close = p;
            break;
        }
        p++;
    }
    if(close == NULL || (size_t)(close - open) >= args_size)
        return 0;
    p = close + 1;
    while(*p == ' ' || *p == '\t' || *p == ';')
        p++;
    if(*p != '\0')
        return 0;
    memcpy(args, open + 1, (size_t)(close - open - 1));
    args[close - open - 1] = '\0';
    return 1;
}

static int
parse_block_call_header(const char *text, char *callee, size_t callee_size,
                      char *name, size_t name_size)
{
    const char *p = text;
    const char *start;
    size_t n = 0;

    while(*p == ' ' || *p == '\t')
        p++;
    start = p;
    while(isalnum((unsigned char)*p) || *p == '_')
        p++;
    n = (size_t)(p - start);
    if(n == 0 || n >= callee_size)
        return 0;
    memcpy(callee, start, n);
    callee[n] = '\0';
    if(strcmp(callee, "case") == 0)
        return 0;
    while(*p == ' ' || *p == '\t')
        p++;
    start = p;
    n = 0;
    while(isalnum((unsigned char)*p) || *p == '_') {
        if(n + 1 < name_size)
            name[n++] = *p;
        p++;
    }
    if((size_t)(p - start) >= name_size)
        return 0;
    name[n] = '\0';
    while(*p == ' ' || *p == '\t')
        p++;
    int has_colon = *p == ':';
    if(has_colon)
        p++;
    while(*p == ' ' || *p == '\t')
        p++;
    if(p[0] != '{' || p[1] != '\0')
        return 0;
    if(!has_colon)
        return 0;
    return 1;
}

/* A semicolon separates logical statements only outside nested expressions
 * and block bodies. #ifx also uses a semicolon before its else arm. */
static char *
statement_separator(char *line)
{
    int parens = 0, brackets = 0, braces = 0, quote = 0;

    for(char *p = line; *p; p++) {
        if(quote) {
            if(*p == '\\' && p[1]) p++;
            else if(*p == quote) quote = 0;
        } else if(*p == '"' || *p == '\'') {
            quote = *p;
        } else if(*p == '(') parens++;
        else if(*p == ')') parens--;
        else if(*p == '[') brackets++;
        else if(*p == ']') brackets--;
        else if(*p == '{') braces++;
        else if(*p == '}') braces--;
        else if(*p == ';' && parens == 0 && brackets == 0 && braces == 0 &&
                *skip_ws(p + 1)) {
            if(starts_word(line, "for") &&
               (strchr(line, '{') == NULL || p < strchr(line, '{')))
                continue; /* Let the checker reject C-style for headers. */
            const char *ifx = strstr(line, "#ifx");
            if(ifx != NULL && ifx < p &&
               starts_word(skip_ws(p + 1), "else"))
                continue;
            return p;
        }
    }
    return NULL;
}

static void
prepend_logical_line(char queue[16][SOURCE_LINE_MAX * 2], int *count,
                     const char *line, ZirSourceSpan span)
{
    if(*count >= 16 || strlen(line) >= sizeof(queue[0]))
        die_at(span, "too many statements on one source line");
    memmove(queue[1], queue[0], (size_t)*count * sizeof(queue[0]));
    copy_text(queue[0], sizeof(queue[0]), line);
    (*count)++;
}

static int
split_oneline_block(const char *t, char *head, size_t hsz,
                    char *body, size_t bsz, char *tail, size_t tsz)
{
    static const char *kws[] = { "if", "else", "while", "for", "defer", "switch",
                                 "case", "guard", "do" };
    size_t n = strlen(t);
    size_t brace_pos = 0, close_pos = 0;
    int depth = 0, expression_braces = 0;
    int in_str = 0;
    int in_chr = 0;
    char w0[16];
    size_t wl = 0;
    size_t i;

    if(n < 4)
        return 0;
    for(i = 0; t[i] != '\0' && (isalnum((unsigned char)t[i]) || t[i] == '_') &&
        wl + 1 < sizeof(w0); i++)
        w0[wl++] = t[i];
    w0[wl] = '\0';
    {
        int is_kw = 0;

        for(size_t k = 0; k < sizeof(kws) / sizeof(kws[0]); k++)
            if(strcmp(w0, kws[k]) == 0)
                is_kw = 1;
        if(!is_kw)
            return 0;
    }
    for(i = 0; i < n; i++) {
        char ch = t[i];

        if(in_str) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '"')
                in_str = 0;
        } else if(in_chr) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '\'')
                in_chr = 0;
        } else if(ch == '"') {
            in_str = 1;
        } else if(ch == '\'') {
            in_chr = 1;
        } else if(ch == '(' || ch == '[') {
            depth++;
        } else if(ch == ')' || ch == ']') {
            depth--;
        } else if(ch == '{' && depth == 0) {
            /* A record literal in the condition may appear before the body. */
            const char *before = t + i;
            while(before > t && isspace((unsigned char)before[-1])) before--;
            if(expression_braces ||
               (before > t && before[-1] == '.' &&
                (before - t < 2 || before[-2] != '.'))) {
                expression_braces++;
            } else {
                brace_pos = i;
                break;
            }
        } else if(ch == '}' && depth == 0 && expression_braces > 0) {
            expression_braces--;
        }
    }
    if(brace_pos == 0 || expression_braces != 0)
        return 0;
    /* Match the first control block; a following else starts a new logical
     * line and can itself contain another one-line block. */
    depth = 1;
    in_str = in_chr = 0;
    for(i = brace_pos + 1; i < n; i++) {
        char ch = t[i];

        if(in_str) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '"')
                in_str = 0;
        } else if(in_chr) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '\'')
                in_chr = 0;
        } else if(ch == '"') {
            in_str = 1;
        } else if(ch == '\'') {
            in_chr = 1;
        } else if(ch == '{') {
            depth++;
        } else if(ch == '}' && --depth == 0) {
            close_pos = i;
            break;
        }
    }
    if(close_pos == 0)
        return 0;
    const char *after = skip_ws(t + close_pos + 1);
    if(*after &&
       ((strcmp(w0, "if") != 0 && strcmp(w0, "else") != 0) ||
        !starts_word(after, "else")))
        return 0;
    size_t hlen = brace_pos + 1;
    size_t blen = close_pos - brace_pos - 1;
    if(hlen >= hsz || blen >= bsz || strlen(after) >= tsz)
        return 0;
    memcpy(head, t, hlen); head[hlen] = '\0';
    memcpy(body, t + brace_pos + 1, blen); body[blen] = '\0';
    trim_in_place(body);
    copy_text(tail, tsz, after);
    return 1;
}

static const char *
closing_parenthesis(const char *open)
{
    int depth = 0;
    if(open == NULL || *open != '(') return NULL;
    for(const char *cursor = open; *cursor; cursor++) {
        if(*cursor == '"' || *cursor == '\'') {
            char quote = *cursor++;
            while(*cursor && *cursor != quote) {
                if(*cursor == '\\' && cursor[1]) cursor++;
                cursor++;
            }
            if(!*cursor) return NULL;
        } else if(*cursor == '(') depth++;
        else if(*cursor == ')' && --depth == 0) return cursor;
    }
    return NULL;
}

static void
parse_function_header(char *name, size_t name_size, char *args,
                      size_t args_size, char *ret, size_t ret_size,
                      const char *line)
{
    const char *p;
    const char *q;
    size_t n = 0;

    name[0] = '\0';
    args[0] = '\0';
    snprintf(ret, ret_size, "void");
    p = strstr(line, "::");
    if(p != NULL) {
        q = line;
        while(q < p && (*q == ' ' || *q == '\t'))
            q++;
        while(q < p && (isalnum((unsigned char)*q) || *q == '_') &&
              n + 1 < name_size)
            name[n++] = *q++;
        name[n] = '\0';
    }
    p = strchr(line, '(');
    q = closing_parenthesis(p);
    if(p != NULL && q != NULL && q > p) {
        n = (size_t)(q - p - 1);
        if(n >= args_size)
            n = args_size - 1;
        memcpy(args, p + 1, n);
        args[n] = '\0';
        while(n > 0 && isspace((unsigned char)args[n - 1]))
            args[--n] = '\0';
        /* Return type: after the closing ')', an optional '-> T' before any
         * trailing directive such as #foreign. */
        q++;
        while(*q == ' ' || *q == '\t')
            q++;
        if(q[0] == '-' && q[1] == '>') {
            q += 2;
            while(*q == ' ' || *q == '\t')
                q++;
            n = 0;
            while(*q != '\0' && *q != '#' && *q != '{' && n + 1 < ret_size)
                ret[n++] = *q++;
            while(n > 0 && isspace((unsigned char)ret[n - 1]))
                n--;
            ret[n] = '\0';
        }
    }
}

static int
function_must_use(const char *line, const char *return_type,
                  ZirSourceSpan span)
{
    const char *parameters = strchr(line, '(');
    const char *closing = closing_parenthesis(parameters);
    const char *end;
    int count = 0;

    if(closing == NULL)
        return 0;
    end = strchr(closing + 1, '{');
    if(end == NULL)
        end = strchr(closing + 1, ';');
    if(end == NULL)
        end = line + strlen(line);
    for(const char *cursor = closing + 1; cursor < end; cursor++) {
        if(*cursor == '"') {
            for(cursor++; cursor < end && *cursor != '"'; cursor++)
                if(*cursor == '\\' && cursor + 1 < end)
                    cursor++;
            if(cursor == end)
                break;
        } else if((size_t)(end - cursor) >= 5 &&
                  strncmp(cursor, "#must", 5) == 0 &&
                  (cursor + 5 == end ||
                   (!isalnum((unsigned char)cursor[5]) &&
                    cursor[5] != '_'))) {
            const char *after = skip_ws(cursor + 5);
            if(after < end && *after != '#')
                die_at(span, "#must does not take arguments");
            count++;
            cursor += 4;
        }
    }
    if(count > 1)
        die_at(span, "duplicate #must procedure modifier");
    if(count && !strcmp(return_type, "void"))
        die_at(span, "#must requires a return value");
    return count;
}

/* Return 1 for a standalone directive and 2 for an inline declaration. */
static int
strip_program_export(char *line, char *symbol, size_t symbol_size,
                     ZirSourceSpan span)
{
    const char *cursor;
    const char *end;

    symbol[0] = '\0';
    if(!starts_word(line, "#program_export"))
        return 0;
    cursor = skip_ws(line + strlen("#program_export"));
    if(*cursor == '"') {
        end = strchr(cursor + 1, '"');
        if(end == NULL || (size_t)(end - cursor - 1) >= symbol_size)
            die_at(span, "#program_export requires a valid quoted symbol");
        memcpy(symbol, cursor + 1, (size_t)(end - cursor - 1));
        symbol[end - cursor - 1] = '\0';
        if(!is_c_ident(symbol))
            die_at(span, "#program_export symbol must be an identifier");
        cursor = skip_ws(end + 1);
    }
    if(*cursor == '\0')
        return 1;
    memmove(line, cursor, strlen(cursor) + 1);
    return 2;
}

static void
separate_parameter_defaults(char *args, size_t capacity,
                            char *defaults, size_t defaults_capacity,
                            ZirSourceSpan span)
{
    char (*parts)[ZIR_TEXT_MAX] = calloc(65, sizeof(*parts));
    char cleaned[ZIR_TEXT_MAX] = "";
    size_t used = 0;
    int has_default = 0;
    if(parts == NULL)
        die("out of memory reading procedure parameters");
    int count = *skip_ws(args) ?
        split_top_level(args, parts[0], 65, sizeof(parts[0])) : 0;
    if(count > 64)
        die_at(span, "too many procedure parameters");
    for(int i = 0; i < count; i++) {
        char *assignment = top_level_assignment(parts[i]);
        if(assignment != NULL) {
            if(!*skip_ws(assignment + 1))
                die_at(span, "default parameter needs a value");
            *assignment = '\0';
            trim_in_place(parts[i]);
            has_default = 1;
        }
        int written = snprintf(cleaned + used, sizeof(cleaned) - used,
                               "%s%s", i ? ", " : "", parts[i]);
        if(written < 0 || (size_t)written >= sizeof(cleaned) - used)
            die_at(span, "procedure parameters exceed size limit");
        used += (size_t)written;
    }
    if(has_default) {
        copy_text(defaults, defaults_capacity, args);
        copy_text(args, capacity, cleaned);
    }
    free(parts);
}

static uint64_t
strip_using_parameters(char *args, size_t capacity, ZirSourceSpan span)
{
    char (*parts)[ZIR_TEXT_MAX] = calloc(65, sizeof(*parts));
    char cleaned[ZIR_TEXT_MAX] = "";
    size_t used = 0;
    uint64_t flags = 0;
    if(parts == NULL)
        die("out of memory reading using parameters");
    int count = *skip_ws(args) ?
        split_top_level(args, parts[0], 65, sizeof(parts[0])) : 0;
    if(count > 64)
        die_at(span, "too many procedure parameters");
    for(int i = 0; i < count; i++) {
        char *part = trim(parts[i]);
        if(starts_word(part, "using")) {
            if(!isspace((unsigned char)part[5]))
                die_at(span, "using parameter modifiers are not supported");
            part = trim(part + 5);
            char *colon = strchr(part, ':');
            if(colon == NULL || colon == part ||
               (size_t)(colon - part) >= ZIR_NAME_MAX)
                die_at(span, "using parameter needs name: Type");
            char name[ZIR_NAME_MAX];
            memcpy(name, part, (size_t)(colon - part));
            name[colon - part] = '\0';
            trim_in_place(name);
            if(!is_identifier_text(name))
                die_at(span, "using parameter needs a plain name");
            flags |= UINT64_C(1) << i;
        }
        int written = snprintf(cleaned + used, sizeof(cleaned) - used,
                               "%s%s", i ? ", " : "", part);
        if(written < 0 || (size_t)written >= sizeof(cleaned) - used)
            die_at(span, "procedure parameters exceed size limit");
        used += (size_t)written;
    }
    copy_text(args, capacity, cleaned);
    free(parts);
    return flags;
}

static int
default_is_scope_independent(const char *expression, const char *path)
{
    ZirLexer lexer;
    ZirToken previous = {0};
    LexerInit(&lexer, expression, path);
    for(;;) {
        ZirToken token = LexerNext(&lexer);
        if(token.kind == ZIR_TOKEN_EOF) return 1;
        if(token.kind == ZIR_TOKEN_UNKNOWN || token.truncated) return 0;
        if(token.kind == ZIR_TOKEN_DIRECTIVE &&
           strcmp(token.text, "#char")) return 0;
        if(token.kind == ZIR_TOKEN_IDENT &&
           strcmp(token.text, "true") && strcmp(token.text, "false") &&
           strcmp(token.text, "null")) {
            ZirLexer ahead = lexer;
            ZirToken next = LexerNext(&ahead);
            if((strcmp(previous.text, "{") &&
                strcmp(previous.text, ",")) ||
               strcmp(next.text, "=")) return 0;
        }
        previous = token;
    }
}

static int
lower_procedure_name_expression(char *part, size_t capacity,
                                const ZirFunction *function)
{
    ZirLexer lexer;
    char lowered[ZIR_TEXT_MAX];
    size_t copied = 0, used = 0;
    int changed = 0;
    LexerInit(&lexer, part, function->span.path);
    for(;;) {
        ZirToken token = LexerNext(&lexer);
        if(token.kind == ZIR_TOKEN_EOF) break;
        size_t start = lexer.pos - strlen(token.text);
        if(token.kind == ZIR_TOKEN_OPERATOR &&
           strcmp(token.text, "/") == 0 && part[start + 1] == '/')
            break;
        if(token.kind != ZIR_TOKEN_DIRECTIVE ||
           strcmp(token.text, "#procedure_name") != 0)
            continue;
        ZirToken open = LexerNext(&lexer);
        ZirToken close = LexerNext(&lexer);
        if(strcmp(open.text, "(") || strcmp(close.text, ")"))
            die_at(function->span,
                   "#procedure_name() requires empty parentheses");
        char literal[ZIR_NAME_MAX + 3];
        int written = snprintf(literal, sizeof(literal), "\"%s\"",
                               function->name);
        if(written < 0 || (size_t)written >= sizeof(literal) ||
           start < copied || used + start - copied + (size_t)written >=
           sizeof(lowered))
            die_at(function->span, "procedure name expression is too long");
        memcpy(lowered + used, part + copied, start - copied);
        used += start - copied;
        memcpy(lowered + used, literal, (size_t)written);
        used += (size_t)written;
        copied = lexer.pos;
        changed = 1;
    }
    if(!changed) return 0;
    size_t rest = strlen(part + copied);
    if(used + rest >= sizeof(lowered) || used + rest >= capacity)
        die_at(function->span, "procedure name expression is too long");
    memcpy(lowered + used, part + copied, rest + 1);
    copy_text(part, capacity, lowered);
    return 1;
}

static int
lower_template_record_default(ZirModule *module,
                              const ZirFunction *function, int parameter,
                              char *part, size_t capacity)
{
    char *assignment = top_level_assignment(part);
    if(!function->is_template || assignment == NULL) return 0;
    const char *value = skip_ws(assignment + 1);
    const char *dot = value;
    size_t name_length = strlen(function->template_param);
    if(strncmp(value, function->template_param, name_length) == 0)
        dot = skip_ws(value + name_length);
    if(*dot != '.') return 0;
    const char *opening = skip_ws(dot + 1);
    size_t value_length = strlen(value);
    while(value_length > 0 && isspace((unsigned char)value[value_length - 1]))
        value_length--;
    if(*opening != '{' || value_length == 0 ||
       value[value_length - 1] != '}') return 0;
    const char *closing = value + value_length - 1;
    if(closing < opening) return 0;
    char body[ZIR_TEXT_MAX];
    size_t body_length = (size_t)(closing - opening - 1);
    if(body_length >= sizeof(body))
        die_at(function->span, "default record initializer is too long");
    memcpy(body, opening + 1, body_length);
    body[body_length] = '\0';
    char (*fields)[ZIR_TEXT_MAX] = calloc(65, sizeof(*fields));
    if(fields == NULL)
        die("out of memory lowering default record initializer");
    int field_count = *skip_ws(body) ?
        split_top_level(body, fields[0], 65, sizeof(fields[0])) : 0;
    if(field_count > 64)
        die_at(function->span, "too many default record initializer fields");
    char normalized[ZIR_TEXT_MAX];
    int written = snprintf(normalized, sizeof(normalized), "%.*s.{",
                           (int)(assignment + 1 - part), part);
    if(written < 0 || (size_t)written >= sizeof(normalized))
        die_at(function->span, "default parameter expression is too long");
    size_t used = (size_t)written;
    for(int field = 0; field < field_count; field++) {
        char *field_assignment = top_level_assignment(fields[field]);
        const char *field_value = skip_ws(field_assignment != NULL ?
                                          field_assignment + 1 : fields[field]);
        if(!default_is_scope_independent(field_value, function->span.path)) {
            char result_type[ZIR_NAME_MAX];
            if(!InferExpressionType(module, field_value, function->span,
                                    result_type, sizeof(result_type)) ||
               !strcmp(result_type, "void") ||
               !strcmp(result_type, "null"))
                die_at(function->span,
                       "cannot resolve polymorphic default field in declaration scope: %s",
                       function->name);
            if(!strcmp(result_type, "integer"))
                copy_text(result_type, sizeof(result_type), "s64");
            else if(!strcmp(result_type, "real"))
                copy_text(result_type, sizeof(result_type), "float64");
            char base_name[ZIR_NAME_MAX], helper_name[ZIR_NAME_MAX];
            FunctionDefaultHelperName(function, parameter, base_name,
                                      sizeof(base_name));
            written = snprintf(helper_name, sizeof(helper_name),
                               "%s_field_%d", base_name, field);
            if(written < 0 || (size_t)written >= sizeof(helper_name))
                die_at(function->span, "default field helper name is too long");
            for(int f = 0; f < module->function_count; f++)
                if(!strcmp(module->functions[f].name, helper_name))
                    die_at(function->span,
                           "default field helper name conflicts with a procedure");
            char return_text[ZIR_TEXT_MAX];
            written = snprintf(return_text, sizeof(return_text),
                               "return %s", field_value);
            if(written < 0 || (size_t)written >= sizeof(return_text))
                die_at(function->span, "default field expression is too long");
            ZirFunction *helper = ModuleAddFunction(module, helper_name, "",
                                                    result_type, 0,
                                                    function->span);
            if(helper == NULL)
                die("out of memory creating default field helper");
            helper->is_public = function->is_public;
            helper->is_file_private = function->is_file_private;
            if(FunctionAddStmt(helper, ZIR_STMT_RETURN, return_text,
                               function->span) == NULL)
                die("out of memory creating default field helper body");
            char rewritten[ZIR_TEXT_MAX];
            written = field_assignment != NULL ?
                snprintf(rewritten, sizeof(rewritten), "%.*s %s()",
                         (int)(field_assignment + 1 - fields[field]),
                         fields[field], helper_name) :
                snprintf(rewritten, sizeof(rewritten), "%s()", helper_name);
            if(written < 0 || (size_t)written >= sizeof(rewritten))
                die_at(function->span, "default field initializer is too long");
            copy_text(fields[field], sizeof(fields[field]), rewritten);
        }
        written = snprintf(normalized + used, sizeof(normalized) - used,
                           "%s%s", field ? ", " : "", fields[field]);
        if(written < 0 || (size_t)written >= sizeof(normalized) - used)
            die_at(function->span, "default record initializer is too long");
        used += (size_t)written;
    }
    if(used + 2 > sizeof(normalized))
        die_at(function->span, "default record initializer is too long");
    normalized[used++] = '}';
    normalized[used] = '\0';
    copy_text(part, capacity, normalized);
    free(fields);
    return 1;
}

static void
add_default_helpers(ZirProgram *program, ZirModule *module,
                    const char *source_path, const char *root,
                    ZirCompileImportResolver resolver, void *resolver_context)
{
    int declarations = module->function_count;
    int imports_resolved = 0;
    for(int fi = 0; fi < declarations; fi++) {
        const ZirFunction *function = &module->functions[fi];
        if(!function->default_args[0] || function->default_helpers_created)
            continue;
        char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
        char (*defaults)[ZIR_TEXT_MAX] = calloc(64, sizeof(*defaults));
        if(parameters == NULL || defaults == NULL)
            die("out of memory creating default argument helpers");
        int count = split_top_level(function->args, parameters[0], 64,
                                    sizeof(parameters[0]));
        if(split_top_level(function->default_args, defaults[0], 64,
                           sizeof(defaults[0])) != count)
            die_at(function->span, "invalid default parameter signature");
        int inferred = 0;
        for(int i = 0; i < count; i++)
            if(lower_procedure_name_expression(defaults[i],
                                               sizeof(defaults[i]), function))
                inferred = 1;
        unsigned char contextual[64] = {0};
        for(int i = 0; i < count; i++) {
            char *assignment = top_level_assignment(defaults[i]);
            if(assignment == NULL || assignment == defaults[i] ||
               assignment[-1] != ':') continue;
            char *colon = strchr(parameters[i], ':');
            if(colon == NULL || *skip_ws(colon + 1) != '\0')
                die_at(function->span, "inferred default parameter needs a name");
            *colon = '\0';
            char name[ZIR_NAME_MAX], value[ZIR_TEXT_MAX], type[ZIR_NAME_MAX];
            copy_text(name, sizeof(name), trim(parameters[i]));
            copy_text(value, sizeof(value), trim(assignment + 1));
            for(int imported = 0; imported < module->import_count; imported++) {
                ZirImport *import = &module->imports[imported];
                if(import->resolved_module != NULL ||
                   (import->kind != ZIR_IMPORT_OPEN &&
                    import->kind != ZIR_IMPORT_MODULE)) continue;
                for(int m = 0; m < program->module_count; m++)
                    if(&program->modules[m] != module &&
                       !strcmp(import->target, program->modules[m].name)) {
                        import->resolved_module = &program->modules[m];
                        break;
                    }
            }
            if(resolver != NULL && !imports_resolved) {
                if(!resolver(resolver_context, program, module, source_path,
                             root, NULL))
                    die_at(function->span,
                           "cannot resolve imports for default parameter: %s",
                           name);
                imports_resolved = 1;
            }
            int type_known;
            if(strcmp(value, "#caller_location") == 0) {
                copy_text(type, sizeof(type), "Source_Code_Location");
                type_known = 1;
            } else
                type_known = InferExpressionType(module, value,
                                                 function->span, type,
                                                 sizeof(type));
            if(!is_identifier_text(name) || !type_known ||
               !strcmp(type, "void") || !strcmp(type, "null"))
                die_at(function->span,
                       "cannot infer default parameter type: %s", name);
            int written = snprintf(parameters[i], sizeof(parameters[i]),
                                   "%s: %s", name, type);
            if(written < 0 || (size_t)written >= sizeof(parameters[i]))
                die_at(function->span, "procedure parameters exceed size limit");
            written = snprintf(defaults[i], sizeof(defaults[i]),
                               "%s = %s", parameters[i], value);
            if(written < 0 || (size_t)written >= sizeof(defaults[i]))
                die_at(function->span, "default parameters exceed size limit");
            inferred = 1;
        }
        ZirFunction signature = module->functions[fi];
        for(int i = 0; i < count; i++)
            if((contextual[i] = lower_template_record_default(
                    module, &signature, i, defaults[i],
                    sizeof(defaults[i]))))
                inferred = 1;
        if(inferred) {
            char args[ZIR_TEXT_MAX] = "", full[ZIR_TEXT_MAX] = "";
            size_t args_used = 0, full_used = 0;
            for(int i = 0; i < count; i++) {
                int written = snprintf(args + args_used,
                                       sizeof(args) - args_used, "%s%s",
                                       i ? ", " : "", parameters[i]);
                if(written < 0 || (size_t)written >= sizeof(args) - args_used)
                    die_at(function->span,
                           "procedure parameters exceed size limit");
                args_used += (size_t)written;
                written = snprintf(full + full_used,
                                   sizeof(full) - full_used, "%s%s",
                                   i ? ", " : "", defaults[i]);
                if(written < 0 || (size_t)written >= sizeof(full) - full_used)
                    die_at(function->span,
                           "default parameters exceed size limit");
                full_used += (size_t)written;
            }
            copy_text(module->functions[fi].args,
                      sizeof(module->functions[fi].args), args);
            copy_text(module->functions[fi].default_args,
                      sizeof(module->functions[fi].default_args), full);
        }
        for(int i = 0; i < count; i++) {
            function = &module->functions[fi];
            char *assignment = top_level_assignment(defaults[i]);
            if(assignment == NULL) continue;
            const char *value = trim(assignment + 1);
            if(contextual[i]) continue;
            if(strcmp(value, "#caller_location") == 0) {
                char *colon = strchr(parameters[i], ':');
                if(colon == NULL ||
                   strcmp(trim(colon + 1), "Source_Code_Location") != 0)
                    die_at(function->span,
                           "#caller_location requires a Source_Code_Location parameter");
                continue;
            }
            if(function->is_template &&
               default_is_scope_independent(value, function->span.path))
                continue;
            char *colon = strchr(parameters[i], ':');
            if(colon == NULL)
                die_at(function->span, "default parameter needs a type");
            char helper_name[ZIR_NAME_MAX];
            char result_type[ZIR_NAME_MAX];
            char body[ZIR_TEXT_MAX];
            ZirSourceSpan span = function->span;
            int is_public = function->is_public;
            int is_file_private = function->is_file_private;
            copy_text(result_type, sizeof(result_type), trim(colon + 1));
            if(function->is_template &&
               (!InferExpressionType(module, value, span, result_type,
                                     sizeof(result_type)) ||
                !strcmp(result_type, "void") ||
                !strcmp(result_type, "null")))
                die_at(span,
                       "cannot resolve polymorphic default in declaration scope: %s",
                       function->name);
            FunctionDefaultHelperName(function, i, helper_name,
                                      sizeof(helper_name));
            for(int f = 0; f < module->function_count; f++)
                if(!strcmp(module->functions[f].name, helper_name))
                    die_at(span, "default helper name conflicts with a procedure");
            int written = snprintf(body, sizeof(body), "return %s", value);
            if(written < 0 || (size_t)written >= sizeof(body))
                die_at(span, "default parameter expression is too long");
            ZirFunction *helper = ModuleAddFunction(module, helper_name,
                                                    "", result_type, 0, span);
            if(helper == NULL)
                die("out of memory creating default argument helper");
            helper->is_public = is_public;
            helper->is_file_private = is_file_private;
            if(FunctionAddStmt(helper, ZIR_STMT_RETURN, body, span) == NULL)
                die("out of memory creating default argument body");
        }
        module->functions[fi].default_helpers_created = 1;
        free(parameters);
        free(defaults);
    }
}

static int
parse_import_line(ZirModule *module, const char *path, int line_no,
                  const char *line, int scope_public)
{
    const char *directive;
    char target[SOURCE_PATH_MAX];
    char name[ZIR_NAME_MAX];
    char signature[ZIR_TEXT_MAX];
    ZirImportKind kind;
    int quoted;

    directive = strstr(line, "#import");
    if(directive == NULL)
        return 0;
    /* `using Alias :: #import "Module";` re-exports the module's public
     * names into unqualified scope beside the alias. */
    int using_import = starts_word(line, "using");
    const char *body = line;
    if(using_import) {
        body = skip_ws(line + 5);
        if(!isalpha((unsigned char)*body) && *body != '_')
            return 0;
    }
    target[0] = '\0';
    name[0] = '\0';
    signature[0] = '\0';
    quoted = parse_quoted(directive, target, sizeof(target));
    if(!quoted)
        die_at(Span(path, line_no, 1),
               "Jai #import requires a quoted module or file name");
    /* Import targets are emitted verbatim inside #include "..." lines;
     * quotes and control bytes would let a crafted path escape the literal. */
    for(const char *p = target; *p != '\0'; p++)
        if(*p == '"' || *p == '>' || (unsigned char)*p < 0x20)
            die_at(Span(path, line_no, 1), "#import target contains a character that cannot "
                "appear in an include path");
    const char *mode = skip_ws(directive + strlen("#import"));
    if(*mode == ',') {
        mode = skip_ws(mode + 1);
        if(strncmp(mode, "dir", 3) == 0 &&
           isspace((unsigned char)mode[3])) {
            const char *path_arg = skip_ws(mode + 3);
            const char *end = *path_arg == '"' ? strchr(path_arg + 1, '"') : NULL;
            if(end == NULL || strcmp(skip_ws(end + 1), ";") != 0)
                die_at(Span(path, line_no, 1),
                       "#import, dir requires a quoted path and ';'");
            int named = parse_symbol_before_colons(line, name, sizeof(name));
            if(target[0] == '/' || target[0] == '\0' ||
               strchr(target, '\\') != NULL ||
               target[strlen(target) - 1] == '/')
                die_at(Span(path, line_no, 1),
                       "#import, dir requires a relative directory path");
            const char *basename = strrchr(target, '/');
            basename = basename == NULL ? target : basename + 1;
            size_t length = strlen(basename);
            if(length == 0 || length >= sizeof(name) ||
               (!isalpha((unsigned char)basename[0]) && basename[0] != '_'))
                die_at(Span(path, line_no, 1),
                       "#import, dir path needs an identifier directory name");
            for(size_t i = 1; i < length; i++)
                if(!source_identifier_byte((unsigned char)basename[i]))
                    die_at(Span(path, line_no, 1),
                           "#import, dir path needs an identifier directory name");
            if(!named) {
                memcpy(name, basename, length);
                name[length] = '\0';
            }
            snprintf(signature, sizeof(signature), "dir:%s", target);
            memmove(target, basename, length);
            target[length] = '\0';
            kind = named ? ZIR_IMPORT_MODULE : ZIR_IMPORT_OPEN;
        } else {
            if(strncmp(mode, "file", 4) != 0 ||
               !isspace((unsigned char)mode[4]))
                die_at(Span(path, line_no, 1),
                       "unsupported #import mode: %s", mode);
            const char *file_arg = skip_ws(mode + 4);
            const char *end = *file_arg == '"' ? strchr(file_arg + 1, '"') : NULL;
            if(end == NULL || strcmp(skip_ws(end + 1), ";") != 0)
                die_at(Span(path, line_no, 1),
                       "#import, file requires a quoted path and ';'");
            int named = parse_symbol_before_colons(line, name, sizeof(name));
            if(target[0] == '/' || target[0] == '\0' ||
               strlen(target) < 4 ||
               strcmp(target + strlen(target) - 3, ".zi") != 0)
                die_at(Span(path, line_no, 1),
                       "#import, file requires a relative .zi path");
            const char *basename = strrchr(target, '/');
            basename = basename == NULL ? target : basename + 1;
            size_t stem_length = strlen(basename) - 3;
            if(stem_length == 0 || stem_length >= sizeof(name) ||
               (!isalpha((unsigned char)basename[0]) && basename[0] != '_'))
                die_at(Span(path, line_no, 1),
                       "#import, file path needs an identifier filename");
            for(size_t i = 1; i < stem_length; i++)
                if(!source_identifier_byte((unsigned char)basename[i]))
                    die_at(Span(path, line_no, 1),
                           "#import, file path needs an identifier filename");
            if(!named) {
                memcpy(name, basename, stem_length);
                name[stem_length] = '\0';
            }
            snprintf(signature, sizeof(signature), "file:%s", target);
            if(named) {
                memmove(target, basename, stem_length);
                target[stem_length] = '\0';
            } else
                copy_text(target, sizeof(target), name);
            kind = named ? ZIR_IMPORT_MODULE : ZIR_IMPORT_OPEN;
        }
    } else if(parse_symbol_before_colons(body, name, sizeof(name)))
        kind = ZIR_IMPORT_MODULE;
    else {
        copy_text(name, sizeof(name), target);
        kind = ZIR_IMPORT_OPEN;
    }
    if(kind == ZIR_IMPORT_MODULE &&
       !isalpha((unsigned char)name[0]) && name[0] != '_')
        die_at(Span(path, line_no, 1),
               "import alias must start with a letter or underscore");
    if(target[0] == '\0' ||
       (!isalpha((unsigned char)target[0]) && target[0] != '_'))
        die_at(Span(path, line_no, 1),
               "Jai #import requires a module identifier; use #import, file for paths");
    for(const unsigned char *p = (const unsigned char *)target; *p; p++)
        if(!source_identifier_byte(*p))
            die_at(Span(path, line_no, 1),
                   "Jai #import requires a module identifier; use #import, file for paths");
    /* A private-scope include belongs in the implementation, not the
     * generated header. File imports retain their source path in signature. */
    {
        ZirImport *imported = ModuleAddImport(module, kind, name, target,
                                             signature, scope_public,
                                             Span(path, line_no, 1));
        if(imported != NULL)
            imported->is_public = scope_public;
        if(imported != NULL && using_import) {
            if(kind != ZIR_IMPORT_MODULE)
                die_at(Span(path, line_no, 1),
                       "using #import requires a named module alias");
            imported->is_using = 1;
        }
    }
    return 1;
}

static int
parse_foreign_library_line(const char *path, int line_no, const char *line,
                           char names[][ZIR_NAME_MAX],
                           char targets[][ZIR_PATH_MAX], int *count)
{
    char name[ZIR_NAME_MAX];
    char target[ZIR_PATH_MAX];
    const char *colons = strstr(line, "::");
    const char *declaration;
    const char *quote;
    const char *end;

    if(colons == NULL ||
       !parse_symbol_before_colons(line, name, sizeof(name)))
        return 0;
    declaration = skip_ws(colons + 2);
    if(!starts_word(declaration, "#system_library"))
        return 0;
    quote = skip_ws(declaration + strlen("#system_library"));
    if(*quote != '"' || !parse_quoted(quote, target, sizeof(target)) ||
       target[0] == '\0')
        die_at(Span(path, line_no, 1),
               "#system_library requires a quoted library name");
    end = strchr(quote + 1, '"');
    if(end == NULL || strcmp(skip_ws(end + 1), ";") != 0)
        die_at(Span(path, line_no, 1),
               "#system_library declaration must end with ';'");
    for(const unsigned char *byte = (const unsigned char *)target;
        *byte != '\0'; byte++)
        if(*byte < 0x20 || *byte == '"' || *byte == '\\')
            die_at(Span(path, line_no, 1),
                   "#system_library name contains an invalid character");
    for(int i = 0; i < *count; i++)
        if(strcmp(names[i], name) == 0)
            die_at(Span(path, line_no, 1),
                   "duplicate #system_library name: %s", name);
    if(*count >= 32)
        die_at(Span(path, line_no, 1), "too many #system_library declarations");
    copy_text(names[*count], ZIR_NAME_MAX, name);
    copy_text(targets[*count], ZIR_PATH_MAX, target);
    (*count)++;
    return 1;
}

static int
parse_foreign_line(ZirModule *module, const char *path, int line_no,
                   const char *line, int scope_public,
                   char names[][ZIR_NAME_MAX],
                   char targets[][ZIR_PATH_MAX],
                   char paths[][SOURCE_PATH_MAX],
                   const int *file_private, int count)
{
    char name[ZIR_NAME_MAX];
    char target[ZIR_PATH_MAX];
    char symbol[ZIR_NAME_MAX];
    char library[ZIR_NAME_MAX];
    char foreign_name[ZIR_NAME_MAX];
    ZirImport *imp;
    ZirExternKind extern_kind;
    if(!parse_symbol_before_colons(line, name, sizeof(name)))
        return 0;
    const char *declaration = skip_ws(strstr(line, "::") + 2);
    const char *dir = strstr(line, "#foreign");
    if(dir == NULL)
        return 0;
    if(*declaration != '(')
        return 0;
    if(strchr(line, '{') != NULL)
        die_at(Span(path, line_no, 1),
               "#foreign procedure declarations cannot have a body");
    const char *cursor = skip_ws(dir + strlen("#foreign"));
    size_t used = 0;
    while((isalnum((unsigned char)*cursor) || *cursor == '_') &&
          used + 1 < sizeof(library))
        library[used++] = *cursor++;
    library[used] = '\0';
    if(!is_identifier_text(library))
        die_at(Span(path, line_no, 1),
               "#foreign requires a named #system_library");
    cursor = skip_ws(cursor);
    copy_text(foreign_name, sizeof(foreign_name), name);
    if(*cursor == '"') {
        size_t length = 0;
        cursor++;
        while(*cursor != '\0' && *cursor != '"' &&
              length + 1 < sizeof(foreign_name))
            foreign_name[length++] = *cursor++;
        foreign_name[length] = '\0';
        if(*cursor != '"' || !is_c_ident(foreign_name))
            die_at(Span(path, line_no, 1),
                   "#foreign alternate symbol must be an identifier");
        cursor = skip_ws(cursor + 1);
    }
    if(strcmp(cursor, ";") != 0)
        die_at(Span(path, line_no, 1),
               "#foreign declaration must end with ';'");
    const char *library_target = NULL;
    for(int i = 0; i < count; i++)
        if(strcmp(names[i], library) == 0 &&
           (!file_private[i] || strcmp(paths[i], path) == 0)) {
            library_target = targets[i];
            break;
        }
    if(library_target == NULL)
        die_at(Span(path, line_no, 1),
               "#foreign library is not declared: %s", library);
    target[0] = '\0';
    symbol[0] = '\0';
    if(strcmp(library_target, "host_api") == 0) {
        if(strcmp(foreign_name, name) != 0)
            die_at(Span(path, line_no, 1),
                   "host capability cannot rename a #foreign symbol");
    } else if(strchr(library_target, '/') != NULL) {
        if(snprintf(target, sizeof(target), "%s.%s", library_target,
                    foreign_name) >= (int)sizeof(target))
            die_at(Span(path, line_no, 1), "#foreign target is too long");
    } else {
        if(snprintf(target, sizeof(target), "c.%s", foreign_name) >=
           (int)sizeof(target))
            die_at(Span(path, line_no, 1), "#foreign target is too long");
    }
    extern_kind = classify_extern_target(target, symbol, sizeof(symbol),
                                         path, line_no);
    imp = ModuleAddImport(module, ZIR_IMPORT_EXTERN,
                             name, target[0] ? target : name, line, 1,
                             Span(path, line_no, 1));
    if(imp != NULL) {
        char parsed_name[ZIR_NAME_MAX];
        imp->is_public = scope_public;
        parse_function_header(parsed_name, sizeof(parsed_name), imp->args,
                              sizeof(imp->args), imp->return_type,
                              sizeof(imp->return_type), line);
        imp->must_use = function_must_use(line, imp->return_type,
                                           imp->span);
        imp->extern_kind = extern_kind;
        snprintf(imp->extern_symbol, sizeof(imp->extern_symbol), "%s",
                 symbol);
        /* A trailing `..any` parameter marks a variadic C ABI: calls may
         * pass any number of extra arguments after the fixed ones. */
        {
            char parameters[8][ZIR_TEXT_MAX];
            int parameter_count = *skip_ws(imp->args) ?
                split_top_level(imp->args, parameters[0], 8,
                                sizeof(parameters[0])) : 0;
            if(parameter_count > 0) {
                const char *last = skip_ws(parameters[parameter_count - 1]);
                const char *colon = strchr(last, ':');
                const char *tail = colon != NULL ? skip_ws(colon + 1) : last;
                if(!strcmp(tail, "..any") || !strcmp(last, "..any"))
                    imp->is_varargs = 1;
            }
        }
    }
    return 1;
}

static int
paren_params_are_typed(const char *body)
{
    const char *inner = NULL;
    const char *end = NULL;
    const char *p;
    size_t depth = 0;

    /* Find the top-level parameter list and its matching close. */
    for(p = body; *p != '\0'; p++) {
        if(*p == '(') {
            if(depth == 0)
                inner = p + 1;
            depth++;
        } else if(*p == ')') {
            depth--;
            if(depth == 0) {
                end = p;
                break;
            }
        }
    }
    if(inner == NULL || end == NULL)
        return 0;
    while(inner < end && (*inner == ' ' || *inner == '\t'))
        inner++;
    if(inner >= end)
        return 1; /* () — empty parameter list */
    /* Jai-style rule: a binding like 'X :: (expr)' stays a constant unless
     * the parenthesized text is a typed parameter list. Parameter lists
     * name their arguments ('name: Type'); bare expressions — numbers,
     * arithmetic, ternaries, literals — do not. */
    if(*inner == '"' || *inner == '\'' || isdigit((unsigned char)*inner))
        return 0;
    if(strchr(inner, '?') != NULL)
        return 0;
    return strchr(inner, ':') != NULL;
}

/* Does a brace appear outside string/char literals? String values may
 * carry JSON or SQL text with braces. */
static int
brace_outside_literals(const char *text)
{
    int in_string = 0, in_char = 0, escaped = 0;
    for(const char *p = text; *p; p++) {
        if(escaped) { escaped = 0; continue; }
        if(*p == '\\') { escaped = 1; continue; }
        if(in_string) {
            if(*p == '"') in_string = 0;
            continue;
        }
        if(in_char) {
            if(*p == '\'') in_char = 0;
            continue;
        }
        if(*p == '"') { in_string = 1; continue; }
        if(*p == '\'') { in_char = 1; continue; }
        if(*p == '{') return 1;
    }
    return 0;
}

static int
looks_like_function_header(const char *line)
{
    char tmp[SOURCE_LINE_MAX];
    char *p;
    const char *body;

    if(starts_word(line, "#import") || strncmp(line, "#import,", 8) == 0)
        return 0;
    p = strstr(line, "::");
    if(p == NULL)
        return 0;
    snprintf(tmp, sizeof(tmp), "%s", p + 2);
    body = trim(tmp);
    if(starts_word(body, "#as"))
        body = skip_ws(body + 3);
    if(starts_word(body, "#import") || strncmp(body, "#import,", 8) == 0 ||
       starts_word(body, "#defined") ||
       starts_word(body, "#define") || starts_word(body, "struct") ||
       starts_word(body, "enum") || starts_word(body, "union"))
        return 0;
    if(strstr(body, "#type") != NULL)
        return 0;
    if(body[0] != '(')
        return 0;
    /* A body or return type makes it a procedure regardless of params. */
    if(strchr(body, '{') != NULL || strstr(body, "->") != NULL)
        return 1;
    return paren_params_are_typed(body);
}

static int
split_oneline_function(const char *line, char *head, size_t head_size,
                       char *body, size_t body_size)
{
    const char *open = strchr(line, '{');
    size_t length = strlen(line);
    int depth = 0, in_string = 0, in_char = 0;

    if(open == NULL || length == 0 || line[length - 1] != '}' ||
       !looks_like_function_header(line))
        return 0;
    for(const char *p = open; *p; p++) {
        if(in_string || in_char) {
            if(*p == '\\' && p[1] != '\0') {
                p++;
            } else if(*p == (in_string ? '"' : '\'')) {
                in_string = in_char = 0;
            }
        } else if(*p == '"') {
            in_string = 1;
        } else if(*p == '\'') {
            in_char = 1;
        } else if(*p == '{') {
            depth++;
        } else if(*p == '}') {
            depth--;
            if(depth == 0 && p != line + length - 1)
                return 0;
        }
        if(depth < 0)
            return 0;
    }
    if(depth != 0 ||
       snprintf(head, head_size, "%.*s", (int)(open - line + 1), line) < 0 ||
       snprintf(body, body_size, "%.*s", (int)(line + length - open - 2),
                open + 1) < 0)
        return 0;
    trim_in_place(body);
    return 1;
}

/* Locate Jai's optional `then` on an if header. A `then` inside a string,
 * grouped expression, or an unparenthesized ifx condition is not the header
 * separator. The latter remains an expression parse error until that form
 * can be split without guessing where its conditional arms end. */
static char *
if_header_then(char *line)
{
    const char *condition = NULL;
    int parens = 0, brackets = 0, braces = 0, quote = 0;
    if(starts_word(line, "if"))
        condition = skip_ws(line + 2);
    else if(starts_word(line, "else") &&
            starts_word(skip_ws(line + 4), "if"))
        condition = skip_ws(skip_ws(line + 4) + 2);
    if(condition == NULL)
        return NULL;
    for(char *p = (char *)condition; *p; p++) {
        if(quote) {
            if(*p == '\\' && p[1]) p++;
            else if(*p == quote) quote = 0;
            continue;
        }
        if(*p == '"' || *p == '\'') { quote = *p; continue; }
        if(*p == '(') { parens++; continue; }
        if(*p == ')') { parens--; continue; }
        if(*p == '[') { brackets++; continue; }
        if(*p == ']') { brackets--; continue; }
        if(*p == '{') { braces++; continue; }
        if(*p == '}') { braces--; continue; }
        if(parens || brackets || braces) continue;
        if((p == condition || !source_identifier_byte((unsigned char)p[-1])) &&
           strncmp(p, "ifx", 3) == 0 &&
           !source_identifier_byte((unsigned char)p[3]))
            return NULL;
        if((p == condition || !source_identifier_byte((unsigned char)p[-1])) &&
           strncmp(p, "then", 4) == 0 &&
           !source_identifier_byte((unsigned char)p[4]))
            return p;
    }
    return NULL;
}

static void
split_jai_control_line(char *line, size_t capacity,
                       char queue[16][SOURCE_LINE_MAX * 2], int *count,
                       ZirSourceSpan span)
{
    char *then = if_header_then(line);
    if(then != NULL) {
        const char *condition = starts_word(line, "if") ?
            skip_ws(line + 2) : skip_ws(skip_ws(line + 4) + 2);
        if(then == condition)
            die_at(span, "if then requires a condition");
        const char *after = skip_ws(then + 4);
        char body[SOURCE_LINE_MAX * 2];
        if(strlen(after) >= sizeof(body))
            die_at(span, "if body exceeds source limit");
        copy_text(body, sizeof(body), after);
        char *end = then;
        while(end > line && isspace((unsigned char)end[-1]))
            end--;
        *end = '\0';
        if(body[0] == '{') {
            size_t used = strlen(line);
            if(snprintf(line + used, capacity - used, " %s", body) >=
               (int)(capacity - used))
                die_at(span, "if header exceeds source limit");
        } else if(body[0] != '\0') {
            prepend_logical_line(queue, count, body, span);
        }
    }
    if(starts_word(line, "else")) {
        const char *body = skip_ws(line + 4);
        if(*body && *body != '{' &&
           !starts_word(body, "if") &&
           !starts_word(body, "#if")) {
            prepend_logical_line(queue, count, body, span);
            line[4] = '\0';
        }
    }
}


/* ---- compile-time conditionals ------------------------------------------
 * '#if COND { ... } else { ... }' selects a branch while parsing. Its braces
 * are consumed here and do not create a source-level scope. */

typedef struct {
    char name[ZIR_NAME_MAX];
    char expr[ZIR_TEXT_MAX];
    char type[ZIR_NAME_MAX]; /* active evaluator local; empty for file constants */
    char path[SOURCE_PATH_MAX];
    int is_file_private;
    int is_public;
    int source_line;
} ZirConst;

typedef struct {
    ZirConst *items;
    int count;
    int capacity;
} ZirConsts;

typedef struct {
    ZirUsing *items;
    int count;
    int capacity;
} ZirUsings;

typedef struct {
    ZirImport *items;
    int count;
    int capacity;
} ZirImports;

typedef struct {
    ZirType *items;
    int count;
    int capacity;
} ZirTypes;

typedef struct {
    char *source;
    char *path;
    char rel[SOURCE_PATH_MAX];
    int line;
    int is_public;
    int is_file_private;
} ZirDeferredType;

typedef struct {
    ZirDeferredType *items;
    int count;
    int capacity;
} ZirDeferredTypes;

typedef struct {
    ZirFunction *items;
    int count;
    int capacity;
} ZirFunctions;

typedef struct {
    ZirGlobal *items;
    int count;
    int capacity;
} ZirGlobals;

typedef struct {
    char **paths;
    int count;
    int capacity;
} ZirDiscoveredFiles;

typedef struct {
    int braces;                   /* net '{' until the region's closing '}' */
    int selected;
    int active;
    int parent_active;
} ZirCondFrame;

static int
line_is_compile_else(const char *line)
{
    return strcmp(line, "} else {") == 0 || strcmp(line, "else {") == 0;
}

/* '#if COND {' / '} else #if COND {': strips the
 * trailing '{' — region braces are consumed, never emitted. Returns 1 for
 * '#if', 2 for 'else #if', 0 otherwise; *condition points into line. */
static int
parse_cond_start(char *line, char **condition)
{
    char *q = NULL;
    int kind = 0;
    size_t n;

    if(strncmp(line, "#if", 3) == 0 &&
       (line[3] == '\0' || isspace((unsigned char)line[3]))) {
        q = line + 3;
        kind = 1;
    } else if(strncmp(line, "else #if", 8) == 0 &&
              (line[8] == '\0' || isspace((unsigned char)line[8]))) {
        q = line + 8;
        kind = 2;
    } else if(strncmp(line, "} else #if", 10) == 0 &&
              (line[10] == '\0' || isspace((unsigned char)line[10]))) {
        q = line + 10;
        kind = 2;
    } else {
        return 0;
    }
    q = trim(q);
    n = strlen(q);
    if(n == 0 || q[n - 1] != '{')
        return 0;
    q[n - 1] = '\0';
    q = trim(q);
    if(q[0] == '\0')
        return 0;
    *condition = q;
    return kind;
}

static int
line_starts_compile_condition(const char *line)
{
    line = skip_ws(line);
    return starts_word(line, "#if") ||
           starts_word(line, "else #if") ||
           starts_word(line, "} else #if");
}

static void
expand_compile_expr_depth(char *dst, size_t dst_size, const ZirConsts *consts,
                          const char *src, const char *lookup_path, int depth,
                          const ZirConst **active)
{
    size_t n = 0;
    int in_string = 0;
    int escaped = 0;
    int i;

    if(dst_size == 0)
        return;
    if(depth > 16)
        die_at(Span(lookup_path, 1, 1),
               "compile-time constant expansion exceeds 16 levels");
    for(const char *p = src; p != NULL && *p != '\0' && n + 1 < dst_size;) {
        if(in_string) {
            dst[n++] = *p;
            if(escaped)
                escaped = 0;
            else if(*p == '\\')
                escaped = 1;
            else if(*p == '"')
                in_string = 0;
            p++;
            continue;
        }
        if(*p == '"') {
            in_string = 1;
            dst[n++] = *p++;
            continue;
        }
        if(*p == '#' && strncmp(p, "#defined", 8) == 0) {
            const char *word_end = p + 8;

            if(*word_end == '\0' || *word_end == '(' ||
               isspace((unsigned char)*word_end)) {
                if(n + 7 >= dst_size)
                    break;
                memcpy(dst + n, "defined", 7);
                n += 7;
                p += 8;
                /* Copy the probe name verbatim: constants must not expand
                 * inside defined(...). */
                {
                    const char *gap = p;
                    while(*gap == ' ' || *gap == '\t')
                        gap++;
                    if(*gap == '(') {
                        int probe_depth = 0;
                        while(*gap != '\0' && n + 1 < dst_size) {
                            if(*gap == '(')
                                probe_depth++;
                            else if(*gap == ')') {
                                probe_depth--;
                                if(probe_depth == 0) {
                                    dst[n++] = *gap++;
                                    break;
                                }
                            }
                            dst[n++] = *gap++;
                        }
                        p = gap;
                    }
                }
                continue;
            }
        }
        if(isalpha((unsigned char)*p) || *p == '_') {
            char ident[ZIR_NAME_MAX];
            size_t il = 0;
            int found = 0;

            size_t previous = n;
            while(previous > 0 &&
                  isspace((unsigned char)dst[previous - 1])) previous--;
            int member = previous > 0 && dst[previous - 1] == '.';

            while(isalnum((unsigned char)*p) || *p == '_') {
                if(il + 1 < sizeof(ident))
                    ident[il++] = *p;
                p++;
            }
            ident[il] = '\0';
            for(i = 0; !member && i < consts->count; i++) {
                if(strcmp(consts->items[i].name, ident) == 0 &&
                   (!consts->items[i].is_file_private ||
                    strcmp(consts->items[i].path, lookup_path) == 0)) {
                    char expanded[ZIR_TEXT_MAX];
                    int written;

                    for(int ancestor = 0; ancestor < depth; ancestor++)
                        if(active[ancestor] == &consts->items[i])
                            die_at(Span(consts->items[i].path,
                                        consts->items[i].source_line, 1),
                                   "cyclic compile-time constant: %s",
                                   ident);
                    active[depth] = &consts->items[i];

                    expand_compile_expr_depth(expanded, sizeof(expanded),
                                              consts, consts->items[i].expr,
                                              consts->items[i].path,
                                              depth + 1, active);
                    written = snprintf(dst + n, dst_size - n, "(%s)", expanded);
                    if(written < 0)
                        written = 0;
                    if((size_t)written >= dst_size - n)
                        n = dst_size - 1;
                    else
                        n += (size_t)written;
                    found = 1;
                    break;
                }
            }
            if(!found) {
                if(n + il >= dst_size)
                    break;
                memcpy(dst + n, ident, il);
                n += il;
            }
            continue;
        }
        dst[n++] = *p++;
    }
    dst[n] = '\0';
}

static void
expand_compile_expr(char *dst, size_t dst_size, const ZirConsts *consts,
                    const char *src, const char *lookup_path)
{
    const ZirConst *active[17] = {0};
    expand_compile_expr_depth(dst, dst_size, consts, src, lookup_path, 0,
                              active);
}

static char *
find_top_comma(char *s)
{
    int depth = 0;
    int in_string = 0;
    int in_char = 0;

    for(char *p = s; *p != '\0'; p++) {
        if(in_string) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '"')
                in_string = 0;
        } else if(in_char) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '\'')
                in_char = 0;
        } else if(*p == '"') {
            in_string = 1;
        } else if(*p == '\'') {
            in_char = 1;
        } else if(*p == '(' || *p == '[' || *p == '{') {
            depth++;
        } else if(*p == ')' || *p == ']' || *p == '}') {
            if(depth > 0)
                depth--;
        } else if(*p == ',' && depth == 0) {
            return p;
        }
    }
    return NULL;
}

typedef struct ZirEval {
    const char *p;
    int known;
    long value;
    const ZirModule *module;
    const ZirConsts *consts;
    const char *lookup_path;
    int source_line;
    int depth;
    int *fuel;
} ZirEval;

static void
eval_skip(ZirEval *ev)
{
    while(*ev->p == ' ' || *ev->p == '\t')
        ev->p++;
}

static long eval_or(ZirEval *ev);
static void lower_size_of_value(char *value, size_t capacity,
                                const ZirModule *module, ZirSourceSpan span);
static void normalize_jai_source_tokens(char *line, const char *path,
                                        const char *physical_path,
                                        int line_no);
static int eval_const_condition(const char *src, long *value,
                                const ZirModule *module,
                                const ZirConsts *consts,
                                const char *lookup_path, int line, int depth);
static int eval_const_condition_with_fuel(const char *src, long *value,
                                const ZirModule *module,
                                const ZirConsts *consts,
                                const char *lookup_path, int line, int depth,
                                int *fuel);
static int evaluate_typed_integer_function(
    ZirEval *ev, const char *name, const long *values,
    const char argument_names[][ZIR_NAME_MAX], int argument_count,
    long *result);

static int compile_size_of_ready(const ZirModule *module, char *condition);

static int
eval_integer_type(const char *type, long value)
{
    if(!strcmp(type, "s8"))
        return value >= INT8_MIN && value <= INT8_MAX;
    if(!strcmp(type, "s16"))
        return value >= INT16_MIN && value <= INT16_MAX;
    if(!strcmp(type, "s32"))
        return value >= INT32_MIN && value <= INT32_MAX;
    if(!strcmp(type, "u8")) return value >= 0 && value <= UINT8_MAX;
    if(!strcmp(type, "u16")) return value >= 0 && value <= UINT16_MAX;
    if(!strcmp(type, "u32")) return value >= 0 && value <= UINT32_MAX;
    if(!strcmp(type, "s64")) return 1;
    if(!strcmp(type, "bool")) return value == 0 || value == 1;
    return 0;
}

static int
eval_close(const ZirFunction *fn, int opening, int stop)
{
    int depth = 1;
    for(int i = opening + 1; i < stop; i++) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(kind == ZIR_STMT_BLOCK_OPEN || kind == ZIR_STMT_IF ||
           kind == ZIR_STMT_WHILE || kind == ZIR_STMT_FOR)
            depth++;
        else if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0)
            return i;
    }
    return -1;
}

static const ZirDefine *
find_imported_define(const ZirModule *module, const char *lookup_path,
                     const char *name, size_t length,
                     const ZirModule **owner_out)
{
    const char *dot = memchr(name, '.', length);
    const ZirImport *import = NULL;
    const ZirDefine *definition = NULL;
    if(module == NULL || dot == NULL ||
       memchr(dot + 1, '.', length - (size_t)(dot + 1 - name)) != NULL)
        return NULL;
    size_t alias_length = (size_t)(dot - name);
    size_t member_length = length - alias_length - 1;
    if(alias_length == 0 || alias_length >= ZIR_NAME_MAX ||
       member_length == 0 || member_length >= ZIR_NAME_MAX)
        return NULL;
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *candidate = &module->imports[i];
        if(candidate->kind != ZIR_IMPORT_MODULE ||
           (candidate->is_file_private &&
            strcmp(candidate->span.path, lookup_path) != 0) ||
           strlen(candidate->name) != alias_length ||
           strncmp(candidate->name, name, alias_length) != 0)
            continue;
        if(import != NULL && import != candidate)
            return NULL;
        import = candidate;
    }
    if(import == NULL || import->resolved_module == NULL)
        return NULL;
    const ZirModule *owner = import->resolved_module;
    for(int i = 0; i < owner->define_count; i++) {
        const ZirDefine *candidate = &owner->defines[i];
        if(!candidate->is_public || candidate->is_file_private ||
           strlen(candidate->name) != member_length ||
           strncmp(candidate->name, dot + 1, member_length) != 0)
            continue;
        if(definition != NULL)
            return NULL;
        definition = candidate;
    }
    if(definition != NULL) *owner_out = owner;
    return definition;
}

static int
eval_imported_integer_define(ZirEval *ev, const char *name, size_t length,
                             long *value)
{
    const ZirModule *owner = NULL;
    const ZirDefine *definition;
    ZirConsts names = {0};
    char expanded[ZIR_TEXT_MAX];
    int known = 0;
    if(ev->depth >= 16)
        return 0;
    definition = find_imported_define(ev->module, ev->lookup_path, name,
                                      length, &owner);
    if(definition == NULL) return 0;
    names.count = owner->define_count;
    names.items = calloc((size_t)names.count + 1, sizeof(*names.items));
    if(names.items == NULL)
        die("out of memory evaluating imported constant");
    for(int i = 0; i < names.count; i++) {
        const ZirDefine *source = &owner->defines[i];
        ZirConst *constant = &names.items[i];
        copy_text(constant->name, sizeof(constant->name), source->name);
        copy_text(constant->expr, sizeof(constant->expr), source->value);
        copy_text(constant->path, sizeof(constant->path),
                  source->span.path);
        constant->is_file_private = source->is_file_private;
        constant->source_line = source->span.line;
    }
    expand_compile_expr(expanded, sizeof(expanded), &names,
                        definition->value, definition->span.path);
    if(strstr(expanded, "size_of") != NULL)
        lower_size_of_value(expanded, sizeof(expanded), owner,
                            definition->span);
    known = eval_const_condition_with_fuel(
        expanded, value, owner, &names, definition->span.path,
        definition->span.line,
        ev->depth + 1, ev->fuel);
    free(names.items);
    return known;
}

static long
eval_primary(ZirEval *ev)
{
    char *end;
    long value;

    if(ev->fuel != NULL && --*ev->fuel < 0) {
        ev->known = 0;
        return 0;
    }
    eval_skip(ev);
    if(starts_word(ev->p, "#compile_time")) {
        ev->p += strlen("#compile_time");
        if(ev->depth > 0)
            return 1;
        ev->known = 0;
        return 0;
    }
    if(*ev->p == '(') {
        ev->p++;
        value = eval_or(ev);
        eval_skip(ev);
        if(*ev->p == ')')
            ev->p++;
        else
            ev->known = 0;
        return value;
    }
    if(isdigit((unsigned char)*ev->p)) {
        errno = 0;
        value = strtol(ev->p, &end, 0);
        if(end == ev->p || errno == ERANGE) {
            ev->known = 0;
            return 0;
        }
        ev->p = end;
        while(isalnum((unsigned char)*ev->p) || *ev->p == '_')
            ev->p++;   /* integer suffixes: U, L, UL */
        return value;
    }
    if(isalpha((unsigned char)*ev->p) || *ev->p == '_') {
        const char *word = ev->p;
        while(isalnum((unsigned char)*ev->p) || *ev->p == '_')
            ev->p++;
        /* defined(NAME) probes the frontend constant table; platform
         * predicates that were never declared evaluate to 0, matching the
         * C preprocessor. */
        if((size_t)(ev->p - word) == 7 && strncmp(word, "defined", 7) == 0) {
            const char *probe = ev->p;
            int wrapped = 0;
            while(*probe == ' ' || *probe == '\t')
                probe++;
            if(*probe == '(') {
                wrapped = 1;
                probe++;
                while(*probe == ' ' || *probe == '\t')
                    probe++;
            }
            if(isalpha((unsigned char)*probe) || *probe == '_') {
                const char *start = probe;
                size_t length;
                while(isalnum((unsigned char)*probe) || *probe == '_')
                    probe++;
                length = (size_t)(probe - start);
                if(wrapped) {
                    while(*probe == ' ' || *probe == '\t')
                        probe++;
                }
                if(length > 0 && length < ZIR_NAME_MAX &&
                   (!wrapped || *probe == ')')) {
                    char name[ZIR_NAME_MAX];
                    int found = 0;
                    memcpy(name, start, length);
                    name[length] = '\0';
                    if(ev->consts != NULL) {
                        for(int c = 0; c < ev->consts->count; c++) {
                            const ZirConst *constant =
                                &ev->consts->items[c];
                            if(strcmp(constant->name, name) == 0 &&
                               (!constant->is_file_private ||
                                strcmp(constant->path,
                                       ev->lookup_path) == 0)) {
                                found = 1;
                                break;
                            }
                        }
                    }
                    if(!found && ev->module != NULL) {
                        for(int d = 0; d < ev->module->define_count; d++) {
                            const ZirDefine *definition =
                                &ev->module->defines[d];
                            if(strcmp(definition->name, name) == 0 &&
                               (!definition->is_file_private ||
                                strcmp(definition->span.path,
                                       ev->lookup_path) == 0)) {
                                found = 1;
                                break;
                            }
                        }
                    }
                    ev->p = probe + (wrapped ? 1 : 0);
                    return found;
                }
            }
        }
        if(ev->module != NULL) {
            const char *after_name = ev->p;
            const char *call_end = after_name;
            long arguments[16];
            char argument_names[16][ZIR_NAME_MAX] = {{0}};
            int count = 0;
            char name[ZIR_NAME_MAX];
            eval_skip(ev);
            if(*ev->p == '.' &&
               (isalpha((unsigned char)ev->p[1]) || ev->p[1] == '_')) {
                ev->p++;
                while(isalnum((unsigned char)*ev->p) || *ev->p == '_')
                    ev->p++;
                call_end = ev->p;
                eval_skip(ev);
            }
            if(*ev->p == '(') {
                size_t length = (size_t)(call_end - word);
                if(length == 0 || length >= sizeof(name)) {
                    ev->known = 0;
                    return 0;
                }
                memcpy(name, word, length);
                name[length] = '\0';
                ev->p++;
                eval_skip(ev);
                while(*ev->p != ')' && *ev->p != '\0') {
                    if(count == 16) { ev->known = 0; return 0; }
                    const char *candidate = ev->p;
                    if(isalpha((unsigned char)*candidate) || *candidate == '_') {
                        const char *end = candidate + 1;
                        while(isalnum((unsigned char)*end) || *end == '_') end++;
                        const char *equals = skip_ws(end);
                        if(*equals == '=' && equals[1] != '=') {
                            size_t n = (size_t)(end - candidate);
                            if(n >= sizeof(argument_names[count])) {
                                ev->known = 0;
                                return 0;
                            }
                            memcpy(argument_names[count], candidate, n);
                            argument_names[count][n] = '\0';
                            ev->p = equals + 1;
                        }
                    }
                    arguments[count++] = eval_or(ev);
                    eval_skip(ev);
                    if(*ev->p != ',')
                        break;
                    ev->p++;
                    eval_skip(ev);
                    if(*ev->p == ')') { ev->known = 0; return 0; }
                }
                eval_skip(ev);
                if(*ev->p != ')' || !ev->known) {
                    ev->known = 0;
                    return 0;
                }
                ev->p++;
                if(!evaluate_typed_integer_function(ev, name, arguments,
                                                     argument_names, count,
                                                     &value))
                    ev->known = 0;
                return ev->known ? value : 0;
            }
            if(call_end > after_name &&
               eval_imported_integer_define(ev, word,
                   (size_t)(call_end - word), &value)) {
                ev->p = call_end;
                return value;
            }
            ev->p = after_name;
        }
        if((size_t)(ev->p - word) == 4 && !strncmp(word, "true", 4))
            return 1;
        if((size_t)(ev->p - word) == 5 && !strncmp(word, "false", 5))
            return 0;
        if((size_t)(ev->p - word) == 2 && !strncmp(word, "OS", 2)) {
#if defined(_WIN32)
            return 1;
#elif defined(__APPLE__)
            return 2;
#elif defined(__linux__)
            return 3;
#else
            ev->known = 0;
            return 0;
#endif
        }
        ev->known = 0;
        return 0;
    }
    if(*ev->p == '.') {
        const char *word = ++ev->p;
        while(isalnum((unsigned char)*ev->p) || *ev->p == '_')
            ev->p++;
        if((size_t)(ev->p - word) == 7 && !strncmp(word, "WINDOWS", 7))
            return 1;
        if((size_t)(ev->p - word) == 5 && !strncmp(word, "MACOS", 5))
            return 2;
        if((size_t)(ev->p - word) == 5 && !strncmp(word, "LINUX", 5))
            return 3;
        ev->known = 0;
        return 0;
    }
    ev->known = 0;
    return 0;
}

static long
eval_unary(ZirEval *ev)
{
    eval_skip(ev);
    if(*ev->p == '!') {
        long v;

        ev->p++;
        v = eval_unary(ev);
        return ev->known ? !v : 0;
    }
    if(*ev->p == '-') {
        long value;
        if(strncmp(ev->p + 1, "9223372036854775808", 19) == 0) {
            ev->p += 20;
            return LONG_MIN;
        }
        ev->p++;
        value = eval_unary(ev);
        if(value == LONG_MIN) { ev->known = 0; return 0; }
        return -value;
    }
    if(*ev->p == '+') {
        ev->p++;
        return eval_unary(ev);
    }
    return eval_primary(ev);
}

static int
checked_add_long(long left, long right, long *result)
{
    if((right > 0 && left > LONG_MAX - right) ||
       (right < 0 && left < LONG_MIN + right))
        return 0;
    *result = left + right;
    return 1;
}

static int
checked_sub_long(long left, long right, long *result)
{
    if((right < 0 && left > LONG_MAX + right) ||
       (right > 0 && left < LONG_MIN + right))
        return 0;
    *result = left - right;
    return 1;
}

static int
checked_mul_long(long left, long right, long *result)
{
    if(left > 0) {
        if((right > 0 && left > LONG_MAX / right) ||
           (right < 0 && right < LONG_MIN / left))
            return 0;
    } else if(left < 0) {
        if((right > 0 && left < LONG_MIN / right) ||
           (right < 0 && left < LONG_MAX / right))
            return 0;
    }
    *result = left * right;
    return 1;
}

static long
eval_mul(ZirEval *ev)
{
    long left = eval_unary(ev);

    while(1) {
        char op;
        long right;
        int left_known;

        eval_skip(ev);
        if(*ev->p != '*' && *ev->p != '/' && *ev->p != '%')
            return left;
        op = *ev->p++;
        left_known = ev->known;
        right = eval_unary(ev);
        if(!left_known || !ev->known || (right == 0 && op != '*')) {
            ev->known = 0;
            left = 0;
        } else if(left == LONG_MIN && right == -1) {
            left = op == '*' || op == '/' ? LONG_MIN : 0;
        } else if(op == '*') {
            if(!checked_mul_long(left, right, &left)) {
                ev->known = 0;
                left = 0;
            }
        } else if(op == '/') {
            left /= right;
        } else {
            left %= right;
        }
    }
}

static long
eval_add(ZirEval *ev)
{
    long left = eval_mul(ev);

    while(1) {
        char op;
        long right;
        int left_known;

        eval_skip(ev);
        if(*ev->p != '+' && *ev->p != '-')
            return left;
        op = *ev->p++;
        left_known = ev->known;
        right = eval_mul(ev);
        if(!left_known || !ev->known) {
            ev->known = 0;
            left = 0;
        } else if(op == '+') {
            if(!checked_add_long(left, right, &left)) {
                ev->known = 0;
                left = 0;
            }
        } else {
            if(!checked_sub_long(left, right, &left)) {
                ev->known = 0;
                left = 0;
            }
        }
    }
}

static long
eval_rel(ZirEval *ev)
{
    long left = eval_add(ev);

    while(1) {
        const char *op = NULL;
        long right;
        int left_known;

        eval_skip(ev);
        if(strncmp(ev->p, "<=", 2) == 0 || strncmp(ev->p, ">=", 2) == 0)
            op = ev->p, ev->p += 2;
        else if(*ev->p == '<' || *ev->p == '>')
            op = ev->p, ev->p++;
        else
            return left;
        left_known = ev->known;
        right = eval_add(ev);
        if(!left_known || !ev->known) {
            ev->known = 0;
            left = 0;
        } else if(op[0] == '<' && op[1] == '=') {
            left = left <= right;
        } else if(op[0] == '>' && op[1] == '=') {
            left = left >= right;
        } else if(op[0] == '<') {
            left = left < right;
        } else {
            left = left > right;
        }
    }
}

static long
eval_eq(ZirEval *ev)
{
    long left = eval_rel(ev);

    while(1) {
        int neq = 0;
        long right;
        int left_known;

        eval_skip(ev);
        if(strncmp(ev->p, "==", 2) == 0) {
            ev->p += 2;
        } else if(strncmp(ev->p, "!=", 2) == 0) {
            ev->p += 2;
            neq = 1;
        } else {
            return left;
        }
        left_known = ev->known;
        right = eval_rel(ev);
        if(!left_known || !ev->known) {
            ev->known = 0;
            left = 0;
        } else {
            left = neq ? left != right : left == right;
        }
    }
}

static long
eval_and(ZirEval *ev)
{
    long left = eval_eq(ev);

    while(1) {
        long right;
        int left_known;
        long left_value;

        eval_skip(ev);
        if(strncmp(ev->p, "&&", 2) != 0)
            return left;
        ev->p += 2;
        left_known = ev->known;
        left_value = left;
        right = eval_eq(ev);
        if(left_known && !left_value) {
            ev->known = 1;
            left = 0;
        } else if(left_known && ev->known) {
            left = left_value && right;
        } else {
            ev->known = 0;
            left = 0;
        }
    }
}

static long
eval_or(ZirEval *ev)
{
    long left = eval_and(ev);

    while(1) {
        long right;
        int left_known;
        long left_value;

        eval_skip(ev);
        if(strncmp(ev->p, "||", 2) != 0)
            return left;
        ev->p += 2;
        left_known = ev->known;
        left_value = left;
        right = eval_and(ev);
        if(left_known && left_value) {
            ev->known = 1;
            left = 1;
        } else if(left_known && ev->known) {
            left = left_value || right;
        } else {
            ev->known = 0;
            left = 0;
        }
    }
}

static int
contains_compile_time_directive(const char *source, const char *path)
{
    ZirLexer lexer;
    LexerInit(&lexer, source, path);
    for(;;) {
        ZirToken token = LexerNext(&lexer);
        if(token.kind == ZIR_TOKEN_EOF) return 0;
        if(token.kind == ZIR_TOKEN_DIRECTIVE &&
           strcmp(token.text, "#compile_time") == 0) return 1;
    }
}

static int
eval_const_condition_with_fuel(const char *src, long *value,
                     const ZirModule *module, const ZirConsts *consts,
                     const char *lookup_path, int line, int depth, int *fuel)
{
    ZirEval ev;

    if(depth == 0 && contains_compile_time_directive(src, lookup_path))
        return 0;
    ev.p = src;
    ev.known = 1;
    ev.module = module;
    ev.consts = consts;
    ev.lookup_path = lookup_path;
    ev.source_line = line;
    ev.depth = depth;
    ev.fuel = fuel;
    ev.value = eval_or(&ev);
    eval_skip(&ev);
    if(*ev.p != '\0')
        ev.known = 0;
    if(value != NULL)
        *value = ev.value;
    return ev.known;
}

static int
eval_const_condition(const char *src, long *value,
                     const ZirModule *module, const ZirConsts *consts,
                     const char *lookup_path, int line, int depth)
{
    int fuel = 10000;
    return eval_const_condition_with_fuel(src, value, module, consts,
                                          lookup_path, line, depth, &fuel);
}

typedef enum {
    COMPILE_INVALID, COMPILE_INTEGER, COMPILE_REAL, COMPILE_STRING,
    COMPILE_COMPOUND
} CompileKind;

typedef struct {
    CompileKind kind;
    long integer;
    double real;
    char type[ZIR_NAME_MAX];
    char literal[ZIR_TEXT_MAX];
    const ZirModule *type_owner;
} CompileValue;

static int evaluate_typed_expression(const ZirModule *module,
                                     const ZirConsts *names,
                                     const char *source, ZirSourceSpan span,
                                     int depth, int *fuel,
                                     CompileValue *result);

static int
compile_value_literal(CompileValue *value)
{
    if(value->kind == COMPILE_INTEGER)
        return snprintf(value->literal, sizeof(value->literal), "%ld",
                        value->integer) < (int)sizeof(value->literal);
    if(value->kind == COMPILE_REAL) {
        int written = snprintf(value->literal, sizeof(value->literal),
                               "%.17g", value->real);
        if(written < 0 || (size_t)written >= sizeof(value->literal)) return 0;
        if(strpbrk(value->literal, ".eE") == NULL) {
            if((size_t)written + 2 >= sizeof(value->literal)) return 0;
            strcat(value->literal, ".0");
        }
    }
    return value->kind != COMPILE_INVALID;
}

static int
compile_truth(const CompileValue *value, int *truth)
{
    if(value->kind == COMPILE_INTEGER) *truth = value->integer != 0;
    else if(value->kind == COMPILE_REAL) *truth = value->real != 0.0;
    else return 0;
    return 1;
}

static int
compile_type_value(const char *type, CompileValue *value)
{
    if(eval_integer_type(type, 0))
        if(value->kind != COMPILE_INTEGER ||
           !eval_integer_type(type, value->integer)) return 0;
        copy_text(value->type, sizeof(value->type), type);
        return compile_value_literal(value);
    if(!strcmp(type, "float32") || !strcmp(type, "float64")) {
        if(value->kind == COMPILE_INTEGER) {
            value->real = (double)value->integer;
            value->kind = COMPILE_REAL;
        }
        if(value->kind != COMPILE_REAL) return 0;
        if(!strcmp(type, "float32"))
            value->real = (float)value->real;
        copy_text(value->type, sizeof(value->type), type);
        return isfinite(value->real) && compile_value_literal(value);
    }
    if(!strcmp(type, "string")) return value->kind == COMPILE_STRING;
    if(value->kind == COMPILE_COMPOUND &&
       !strcmp(value->type, type)) return 1;
    return 0;
}

static int
wrap_compile_integer(const char *type, long *value)
{
    if(!strcmp(type, "s8")) *value = (int8_t)*value;
    else if(!strcmp(type, "u8")) *value = (uint8_t)*value;
    else if(!strcmp(type, "s16")) *value = (int16_t)*value;
    else if(!strcmp(type, "u16")) *value = (uint16_t)*value;
    else if(!strcmp(type, "s32")) *value = (int32_t)*value;
    else if(!strcmp(type, "u32")) *value = (uint32_t)*value;
    else return !strcmp(type, "s64");
    return 1;
}

static int
compile_values_equal(const CompileValue *left, const CompileValue *right,
                     int *equal)
{
    if(left->kind == COMPILE_STRING && right->kind == COMPILE_STRING) {
        unsigned char a[ZIR_TEXT_MAX], b[ZIR_TEXT_MAX];
        size_t an, bn;
        if(!DecodeStringLiteral(left->literal, a, sizeof(a), &an) ||
           !DecodeStringLiteral(right->literal, b, sizeof(b), &bn)) return 0;
        *equal = an == bn && memcmp(a, b, an) == 0;
        return 1;
    }
    if(left->kind == COMPILE_INTEGER && right->kind == COMPILE_INTEGER) {
        *equal = left->integer == right->integer;
        return 1;
    }
    if((left->kind == COMPILE_REAL || left->kind == COMPILE_INTEGER) &&
       (right->kind == COMPILE_REAL || right->kind == COMPILE_INTEGER)) {
        double a = left->kind == COMPILE_REAL ? left->real :
                   (double)left->integer;
        double b = right->kind == COMPILE_REAL ? right->real :
                   (double)right->integer;
        *equal = a == b;
        return 1;
    }
    return 0;
}

static int evaluate_typed_function(const ZirModule *module,
                                   const char *name, ZirSourceSpan call_span,
                                   CompileValue *arguments,
                                   char argument_names[][ZIR_NAME_MAX],
                                   int argument_count, int depth, int *fuel,
                                   CompileValue *result);
static int evaluate_typed_node(const ZirFunction *probe, int index,
                    const ZirModule *module, const char *path,
                    int depth, int *fuel, CompileValue *result);

static int
compile_compound_value(const ZirFunction *probe, const ZirExpr *expression,
                       const ZirModule *module, const char *path, int depth,
                       int *fuel, CompileValue *result)
{
    char element[ZIR_NAME_MAX] = "";
    int capacity = 0;
    int array = ArrayElementType(expression->name, element,
                                 sizeof(element), &capacity);
    const ZirType *record = array ? NULL :
        FindType(module, expression->name, NULL);
    if((array && capacity < 0) ||
       (!array && (record == NULL || record->is_enum ||
                   record->is_procedure_type || record->is_record_template)))
        return 0;
    int written = array ?
        snprintf(result->literal, sizeof(result->literal), "%s.[", element) :
        snprintf(result->literal, sizeof(result->literal), "%s.{",
                 expression->name);
    if(written < 0 || (size_t)written >= sizeof(result->literal)) return 0;
    size_t used = (size_t)written;
    int ordinal = 0, mode = -1;
    for(int child = expression->first_child; child >= 0;
        child = probe->exprs[child].next_sibling) {
        const ZirExpr *entry = &probe->exprs[child];
        CompileValue value = {0};
        char field_type[ZIR_NAME_MAX];
        char field_name[ZIR_NAME_MAX] = "";
        int named = !strcmp(entry->op, "=");
        if(array) {
            if(named || ordinal >= capacity) return 0;
            copy_text(field_type, sizeof(field_type), element);
        } else {
            ZirTypeField field;
            size_t offset = 0;
            int position = 0, found = 0;
            if(mode >= 0 && mode != named) return 0;
            mode = named;
            while(TypeNextField(record, &offset, &field) == 1) {
                if(named ? !strcmp(field.name, entry->name) :
                           position == ordinal) {
                    copy_text(field_type, sizeof(field_type), field.type);
                    copy_text(field_name, sizeof(field_name), field.name);
                    found = 1;
                    break;
                }
                position++;
            }
            if(!found) return 0;
            for(int previous = expression->first_child; previous != child;
                previous = probe->exprs[previous].next_sibling)
                if(named && !strcmp(probe->exprs[previous].name,
                                    field_name)) return 0;
        }
        if(!evaluate_typed_node(probe, entry->right, module, path,
                                depth + 1, fuel, &value) ||
           !compile_type_value(field_type, &value)) return 0;
        if(array)
            written = snprintf(result->literal + used,
                               sizeof(result->literal) - used, "%s%s",
                               ordinal ? ", " : "", value.literal);
        else
            written = snprintf(result->literal + used,
                               sizeof(result->literal) - used,
                               "%s.%s = %s", ordinal ? ", " : "",
                               field_name, value.literal);
        if(written < 0 || (size_t)written >= sizeof(result->literal) - used)
            return 0;
        used += (size_t)written;
        ordinal++;
    }
    if(array && ordinal != capacity) return 0;
    written = snprintf(result->literal + used, sizeof(result->literal) - used,
                       "%c", array ? ']' : '}');
    if(written != 1 || (size_t)written >= sizeof(result->literal) - used)
        return 0;
    result->kind = COMPILE_COMPOUND;
    copy_text(result->type, sizeof(result->type), expression->name);
    result->type_owner = module;
    return 1;
}

static int
compile_compound_member(const CompileValue *compound, const char *member,
                        const ZirModule *module, const char *path, int depth,
                        int *fuel, CompileValue *result)
{
    if(compound->type_owner != NULL)
        module = compound->type_owner;
    ZirFunction probe = {0};
    int root = ParseExpr(&probe, module, compound->literal,
                         Span(path, 1, 1));
    int found = 0;
    if(root >= 0 && probe.exprs[root].kind == ZIR_EXPR_COMPOUND) {
        const ZirType *record = FindType(module, compound->type, NULL);
        for(int child = probe.exprs[root].first_child; child >= 0;
            child = probe.exprs[child].next_sibling) {
            const ZirExpr *entry = &probe.exprs[child];
            if(!strcmp(entry->name, member)) {
                ZirTypeField field;
                size_t offset = 0;
                while(record != NULL &&
                      TypeNextField(record, &offset, &field) == 1)
                    if(!strcmp(field.name, member)) {
                        found = evaluate_typed_node(&probe, entry->right,
                                                    module, path, depth + 1,
                                                    fuel, result) &&
                                compile_type_value(field.type, result);
                        break;
                    }
                break;
            }
        }
    }
    free(probe.exprs);
    return found;
}

static int
compile_compound_index(const CompileValue *compound, long index,
                       const ZirModule *module, const char *path, int depth,
                       int *fuel, CompileValue *result)
{
    if(compound->type_owner != NULL)
        module = compound->type_owner;
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(!ArrayElementType(compound->type, element, sizeof(element),
                         &capacity) || index < 0 || index >= capacity)
        return 0;
    ZirFunction probe = {0};
    int root = ParseExpr(&probe, module, compound->literal,
                         Span(path, 1, 1));
    int found = 0, ordinal = 0;
    if(root >= 0 && probe.exprs[root].kind == ZIR_EXPR_COMPOUND)
        for(int child = probe.exprs[root].first_child; child >= 0;
            child = probe.exprs[child].next_sibling, ordinal++)
            if(ordinal == index) {
                found = evaluate_typed_node(&probe, probe.exprs[child].right,
                                            module, path, depth + 1, fuel,
                                            result) &&
                        compile_type_value(element, result);
                break;
            }
    free(probe.exprs);
    return found;
}

static int
local_value_name_visible(const ZirModule *module, const char *path,
                         const char *name)
{
    for(int i = 0; i < module->define_count; i++)
        if(!strcmp(module->defines[i].name, name) &&
           (!module->defines[i].is_file_private ||
            !strcmp(module->defines[i].span.path, path))) return 1;
    for(int i = 0; i < module->global_count; i++)
        if(!strcmp(module->globals[i].name, name) &&
           (!module->globals[i].is_file_private ||
            !strcmp(module->globals[i].span.path, path))) return 1;
    for(int i = 0; i < module->type_count; i++)
        if(!strcmp(module->types[i].name, name) &&
           (!module->types[i].is_file_private ||
            !strcmp(module->types[i].span.path, path))) return 1;
    for(int i = 0; i < module->function_count; i++)
        if(!strcmp(module->functions[i].name, name) &&
           (!module->functions[i].is_file_private ||
            !strcmp(module->functions[i].span.path, path))) return 1;
    return 0;
}

static const ZirDefine *
find_open_imported_define(const ZirModule *module, const char *path,
                          const char *name, const ZirModule **owner_out)
{
    const ZirDefine *definition = NULL;
    if(local_value_name_visible(module, path, name)) return NULL;
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *import = &module->imports[i];
        if((import->kind != ZIR_IMPORT_OPEN &&
            !(import->kind == ZIR_IMPORT_MODULE && import->is_using)) ||
           (import->is_file_private &&
            strcmp(import->span.path, path) != 0) ||
           import->resolved_module == NULL) continue;
        const ZirModule *owner = import->resolved_module;
        for(int j = 0; j < owner->define_count; j++) {
            const ZirDefine *candidate = &owner->defines[j];
            if(!candidate->is_public || candidate->is_file_private ||
               strcmp(candidate->name, name)) continue;
            if(definition != NULL && definition != candidate) return NULL;
            definition = candidate;
            *owner_out = owner;
        }
    }
    return definition;
}

static int
evaluate_imported_typed_define(const ZirModule *module, const char *path,
                               const char *name, int depth, int *fuel,
                               CompileValue *result)
{
    const ZirModule *owner = NULL;
    const ZirDefine *definition = strchr(name, '.') != NULL ?
        find_imported_define(module, path, name, strlen(name), &owner) :
        find_open_imported_define(module, path, name, &owner);
    ZirConsts names = {0};
    int ok;
    if(definition == NULL || depth >= 32) return 0;
    names.count = owner->define_count;
    names.items = calloc((size_t)names.count + 1, sizeof(*names.items));
    if(names.items == NULL)
        die("out of memory evaluating imported constant");
    for(int i = 0; i < names.count; i++) {
        const ZirDefine *source = &owner->defines[i];
        ZirConst *constant = &names.items[i];
        copy_text(constant->name, sizeof(constant->name), source->name);
        copy_text(constant->expr, sizeof(constant->expr), source->value);
        copy_text(constant->path, sizeof(constant->path),
                  source->span.path);
        constant->is_file_private = source->is_file_private;
        constant->source_line = source->span.line;
    }
    ok = evaluate_typed_expression(owner, &names, definition->value,
                                   definition->span, depth + 1,
                                   fuel, result);
    free(names.items);
    return ok;
}

static int
evaluate_typed_node(const ZirFunction *probe, int index,
                    const ZirModule *module, const char *path,
                    int depth, int *fuel, CompileValue *result)
{
    if(index < 0 || index >= probe->expr_count || depth > 64 ||
       --*fuel < 0) return 0;
    const ZirExpr *expression = &probe->exprs[index];
    CompileValue left = {0}, right = {0};
    char *end;
    long integer;
    double real;
    int truth;
    switch(expression->kind) {
    case ZIR_EXPR_INT:
        errno = 0;
        integer = strtol(expression->text, &end, 0);
        if(errno == ERANGE || end == expression->text || *end) return 0;
        result->kind = COMPILE_INTEGER;
        result->integer = integer;
        return compile_value_literal(result);
    case ZIR_EXPR_FLOAT:
        errno = 0;
        real = strtod(expression->text, &end);
        if(errno == ERANGE || end == expression->text || *end ||
           !isfinite(real)) return 0;
        result->kind = COMPILE_REAL;
        result->real = real;
        return compile_value_literal(result);
    case ZIR_EXPR_STRING:
        result->kind = COMPILE_STRING;
        copy_text(result->literal, sizeof(result->literal),
                  expression->text);
        return 1;
    case ZIR_EXPR_COMPOUND:
        return compile_compound_value(probe, expression, module, path,
                                      depth, fuel, result);
    case ZIR_EXPR_COMPILE_TIME:
        result->kind = COMPILE_INTEGER;
        result->integer = 1;
        return compile_value_literal(result);
    case ZIR_EXPR_IDENT:
        if(!strcmp(expression->name, "true") ||
           !strcmp(expression->name, "false")) {
            result->kind = COMPILE_INTEGER;
            result->integer = !strcmp(expression->name, "true");
            return compile_value_literal(result);
        }
        return evaluate_imported_typed_define(module, path,
                    expression->name, depth + 1, fuel, result);
    case ZIR_EXPR_SIZE_OF: {
        size_t size, alignment;
        if(!TypeLayout(module, expression->name, &size, &alignment) ||
           size > LONG_MAX) return 0;
        result->kind = COMPILE_INTEGER;
        result->integer = (long)size;
        return compile_value_literal(result);
    }
    case ZIR_EXPR_UNARY: {
        const ZirExpr *operand = &probe->exprs[expression->right];
        if(!strcmp(expression->op, "-") && operand->kind == ZIR_EXPR_INT &&
           !strcmp(operand->text, "9223372036854775808")) {
            result->kind = COMPILE_INTEGER;
            result->integer = LONG_MIN;
            return compile_value_literal(result);
        }
        if(!evaluate_typed_node(probe, expression->right, module, path,
                                depth + 1, fuel, &right)) return 0;
        if(!strcmp(expression->op, "!")) {
            if(!compile_truth(&right, &truth)) return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = !truth;
        } else if(!strcmp(expression->op, "+")) {
            *result = right;
        } else if(!strcmp(expression->op, "-")) {
            *result = right;
            if(right.kind == COMPILE_REAL) result->real = -right.real;
            else if(right.kind == COMPILE_INTEGER && right.integer != LONG_MIN)
                result->integer = -right.integer;
            else return 0;
        } else if(!strcmp(expression->op, "~") &&
                  right.kind == COMPILE_INTEGER) {
            result->kind = COMPILE_INTEGER;
            result->integer = ~right.integer;
        } else return 0;
        return compile_value_literal(result);
    }
    case ZIR_EXPR_BINARY: {
        const char *op = expression->op;
        if(!evaluate_typed_node(probe, expression->left, module, path,
                                depth + 1, fuel, &left)) return 0;
        if(!strcmp(op, "&&") || !strcmp(op, "||")) {
            if(!compile_truth(&left, &truth)) return 0;
            if((!strcmp(op, "&&") && !truth) ||
               (!strcmp(op, "||") && truth)) {
                result->kind = COMPILE_INTEGER;
                result->integer = truth;
                return compile_value_literal(result);
            }
        }
        if(!evaluate_typed_node(probe, expression->right, module, path,
                                depth + 1, fuel, &right)) return 0;
        if(!strcmp(op, "==") || !strcmp(op, "!=")) {
            int equal;
            if(!compile_values_equal(&left, &right, &equal)) return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = !strcmp(op, "==") ? equal : !equal;
            return compile_value_literal(result);
        }
        if(!strcmp(op, "&&") || !strcmp(op, "||")) {
            if(!compile_truth(&right, &truth)) return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = truth;
            return compile_value_literal(result);
        }
        if((left.kind != COMPILE_INTEGER && left.kind != COMPILE_REAL) ||
           (right.kind != COMPILE_INTEGER && right.kind != COMPILE_REAL))
            return 0;
        double a = left.kind == COMPILE_REAL ? left.real :
                   (double)left.integer;
        double b = right.kind == COMPILE_REAL ? right.real :
                   (double)right.integer;
        if(!strcmp(op, "<") || !strcmp(op, "<=") ||
           !strcmp(op, ">") || !strcmp(op, ">=")) {
            result->kind = COMPILE_INTEGER;
            result->integer = !strcmp(op, "<") ? a < b :
                              !strcmp(op, "<=") ? a <= b :
                              !strcmp(op, ">") ? a > b : a >= b;
            return compile_value_literal(result);
        }
        if(left.kind == COMPILE_INTEGER && right.kind == COMPILE_INTEGER) {
            long x = left.integer, y = right.integer, folded;
            if(!strcmp(op, "+")) {
                if(!checked_add_long(x, y, &folded)) return 0;
            } else if(!strcmp(op, "-")) {
                if(!checked_sub_long(x, y, &folded)) return 0;
            } else if(!strcmp(op, "*")) {
                if(!checked_mul_long(x, y, &folded)) return 0;
            } else if(!strcmp(op, "/") && y != 0)
                folded = x == LONG_MIN && y == -1 ? x : x / y;
            else if(!strcmp(op, "%") && y != 0)
                folded = x == LONG_MIN && y == -1 ? 0 : x % y;
            else if(!strcmp(op, "&")) folded = x & y;
            else if(!strcmp(op, "|")) folded = x | y;
            else if(!strcmp(op, "^")) folded = x ^ y;
            else if(!strcmp(op, "<<") && y >= 0 &&
                    y < (long)(sizeof(long) * CHAR_BIT) && x >= 0 &&
                    x <= (LONG_MAX >> y)) folded = x << y;
            else if(!strcmp(op, ">>") && y >= 0 &&
                    y < (long)(sizeof(long) * CHAR_BIT)) folded = x >> y;
            else return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = folded;
            if(wrap_compile_integer(left.type, &result->integer))
                copy_text(result->type, sizeof(result->type), left.type);
            return compile_value_literal(result);
        }
        result->kind = COMPILE_REAL;
        if(!strcmp(op, "+")) result->real = a + b;
        else if(!strcmp(op, "-")) result->real = a - b;
        else if(!strcmp(op, "*")) result->real = a * b;
        else if(!strcmp(op, "/") && b != 0.0) result->real = a / b;
        else return 0;
        return isfinite(result->real) && compile_value_literal(result);
    }
    case ZIR_EXPR_MEMBER:
        if(expression->left >= 0 &&
           probe->exprs[expression->left].kind == ZIR_EXPR_IDENT) {
            char qualified[ZIR_NAME_MAX];
            if(snprintf(qualified, sizeof(qualified), "%s.%s",
                        probe->exprs[expression->left].name,
                        expression->name) < (int)sizeof(qualified) &&
               evaluate_imported_typed_define(module, path, qualified,
                                              depth + 1, fuel, result))
                return 1;
        }
        if(!evaluate_typed_node(probe, expression->left, module, path,
                                depth + 1, fuel, &left)) return 0;
        if(left.kind == COMPILE_STRING &&
           !strcmp(expression->name, "count")) {
            unsigned char bytes[ZIR_TEXT_MAX];
            size_t length;
            if(!DecodeStringLiteral(left.literal, bytes, sizeof(bytes),
                                     &length) || length > LONG_MAX) return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = (long)length;
            return compile_value_literal(result);
        }
        if(left.kind == COMPILE_COMPOUND) {
            int capacity = 0;
            if(ArrayElementType(left.type, NULL, 0, &capacity) &&
               !strcmp(expression->name, "count")) {
                result->kind = COMPILE_INTEGER;
                result->integer = capacity;
                return compile_value_literal(result);
            }
            return compile_compound_member(&left, expression->name,
                                           module, path, depth, fuel, result);
        }
        return 0;
    case ZIR_EXPR_INDEX:
        if(!evaluate_typed_node(probe, expression->left, module, path,
                                depth + 1, fuel, &left) ||
           !evaluate_typed_node(probe, expression->right, module, path,
                                depth + 1, fuel, &right) ||
           left.kind != COMPILE_COMPOUND ||
           right.kind != COMPILE_INTEGER) return 0;
        return compile_compound_index(&left, right.integer, module, path,
                                      depth, fuel, result);
    case ZIR_EXPR_CAST:
        if(!evaluate_typed_node(probe, expression->right, module, path,
                                depth + 1, fuel, result)) return 0;
        if(result->kind == COMPILE_INTEGER &&
           wrap_compile_integer(expression->name, &result->integer))
            copy_text(result->type, sizeof(result->type), expression->name);
        if(eval_integer_type(expression->name, 0) &&
           result->kind == COMPILE_REAL) {
            if(result->real < (double)LONG_MIN ||
               result->real >= -(double)LONG_MIN) return 0;
            result->integer = (long)result->real;
            result->kind = COMPILE_INTEGER;
        }
        return compile_type_value(expression->name, result);
    case ZIR_EXPR_CONDITIONAL:
        if(!evaluate_typed_node(probe, expression->left, module, path,
                                depth + 1, fuel, &left) ||
           !compile_truth(&left, &truth)) return 0;
        return evaluate_typed_node(probe, truth ? expression->right :
                                   expression->third, module, path,
                                   depth + 1, fuel, result);
    case ZIR_EXPR_CALL: {
        char name[ZIR_NAME_MAX];
        CompileValue args[16] = {{0}};
        char names[16][ZIR_NAME_MAX] = {{0}};
        int count = 0;
        copy_text(name, sizeof(name), expression->name);
        if(!name[0] && expression->left >= 0) {
            const ZirExpr *callee = &probe->exprs[expression->left];
            if(callee->kind != ZIR_EXPR_MEMBER || callee->left < 0 ||
               probe->exprs[callee->left].kind != ZIR_EXPR_IDENT ||
               snprintf(name, sizeof(name), "%s.%s",
                        probe->exprs[callee->left].name, callee->name) >=
                   (int)sizeof(name)) return 0;
        }
        for(int child = expression->first_child; child >= 0;
            child = probe->exprs[child].next_sibling) {
            if(count == 16 ||
               !evaluate_typed_node(probe, child, module, path,
                                    depth + 1, fuel, &args[count])) return 0;
            copy_text(names[count], sizeof(names[count]),
                      probe->exprs[child].argument_name);
            count++;
        }
        ZirSourceSpan call_span = expression->left >= 0 ?
            probe->exprs[expression->left].span : expression->span;
        return evaluate_typed_function(module, name, call_span, args, names,
                                       count, depth + 1, fuel, result);
    }
    default:
        return 0;
    }
}

static int
evaluate_typed_expression(const ZirModule *module, const ZirConsts *names,
                          const char *source, ZirSourceSpan span, int depth,
                          int *fuel, CompileValue *result)
{
    ZirConsts empty = {0};
    ZirFunction probe = {0};
    char input[ZIR_TEXT_MAX], expanded[ZIR_TEXT_MAX];
    long integer;
    int root, ok;
    const char *path = span.path;
    if(depth > 32 || source == NULL || --*fuel < 0) return 0;
    copy_text(input, sizeof(input), source);
    trim_in_place(input);
    size_t length = strlen(input);
    if(length && input[length - 1] == ';') {
        input[length - 1] = '\0';
        trim_in_place(input);
    }
    if(!input[0]) return 0;
    if(names == NULL) names = &empty;
    expand_compile_expr(expanded, sizeof(expanded), names, input, path);
    if(eval_const_condition_with_fuel(expanded, &integer, module, names,
                                     path, span.line, depth, fuel)) {
        result->kind = COMPILE_INTEGER;
        result->integer = integer;
        return compile_value_literal(result);
    }
    root = ParseExpr(&probe, module, expanded, span);
    ok = root >= 0;
    if(depth == 0)
        for(int i = 0; i < probe.expr_count; i++)
            if(probe.exprs[i].kind == ZIR_EXPR_COMPILE_TIME)
                ok = 0;
    if(ok)
        ok = evaluate_typed_node(&probe, root, module, path,
                                 depth + 1, fuel, result);
    free(probe.exprs);
    return ok;
}

typedef struct {
    const ZirModule *module;
    const ZirFunction *fn;
    ZirConsts names;
    int local_count;
    int capacity;
    int depth;
    int *fuel;
    ZirSourceSpan current_span;
} TypedBody;

static int
typed_body_expression(TypedBody *body, const char *source,
                      CompileValue *value)
{
    char lowered[ZIR_TEXT_MAX];
    copy_text(lowered, sizeof(lowered), source);
    lower_procedure_name_expression(lowered, sizeof(lowered), body->fn);
    return evaluate_typed_expression(body->module, &body->names, lowered,
                                     body->current_span, body->depth + 1,
                                     body->fuel, value);
}

static int
typed_body_condition(TypedBody *body, const char *header,
                     const char *word, int *truth)
{
    char condition[ZIR_TEXT_MAX];
    const char *source = skip_ws(header + strlen(word));
    copy_text(condition, sizeof(condition), source);
    trim_in_place(condition);
    size_t length = strlen(condition);
    if(!length || condition[length - 1] != '{') return 0;
    condition[length - 1] = '\0';
    trim_in_place(condition);
    length = strlen(condition);
    if(length >= 4 && !strcmp(condition + length - 4, "then") &&
       (length == 4 || isspace((unsigned char)condition[length - 5]))) {
        condition[length - 4] = '\0';
        trim_in_place(condition);
    }
    CompileValue value = {0};
    return typed_body_expression(body, condition, &value) &&
           compile_truth(&value, truth);
}

static void
typed_leave_scope(TypedBody *body, int saved)
{
    int removed = body->local_count - saved;
    if(removed <= 0) return;
    memmove(body->names.items, body->names.items + removed,
            (size_t)(body->names.count - removed) *
                sizeof(*body->names.items));
    body->names.count -= removed;
    body->local_count = saved;
}

static int
typed_add_local(TypedBody *body, const char *name, const char *type,
                const CompileValue *value)
{
    if(!is_identifier_text(name) ||
       body->names.count >= body->capacity) return 0;
    memmove(body->names.items + 1, body->names.items,
            (size_t)body->names.count * sizeof(*body->names.items));
    ZirConst *local = &body->names.items[0];
    memset(local, 0, sizeof(*local));
    copy_text(local->name, sizeof(local->name), name);
    copy_text(local->type, sizeof(local->type), type);
    copy_text(local->path, sizeof(local->path), body->fn->span.path);
    if(value != NULL)
        copy_text(local->expr, sizeof(local->expr), value->literal);
    body->names.count++;
    body->local_count++;
    return 1;
}

static int
typed_local_index(TypedBody *body, const char *name)
{
    for(int i = 0; i < body->local_count; i++)
        if(!strcmp(body->names.items[i].name, name)) return i;
    return -1;
}

static int
typed_body_statements(TypedBody *body, int start, int stop,
                      int *flow, CompileValue *result)
{
    int saved = body->local_count;
    for(int i = start; i < stop && !*flow; i++) {
        const ZirStmt *statement = &body->fn->stmts[i];
        const char *source = skip_ws(statement->text);
        body->current_span = statement->span;
        if(--*body->fuel < 0) goto failed;
        if(statement->kind == ZIR_STMT_IF) {
            int taken = 0;
            for(;;) {
                const ZirStmt *arm = &body->fn->stmts[i];
                const char *header = skip_ws(arm->text);
                body->current_span = arm->span;
                int close = eval_close(body->fn, i, stop);
                int condition = 1;
                if(close < 0) goto failed;
                if(starts_word(header, "else if")) {
                    if(!taken &&
                       !typed_body_condition(body, header, "else if",
                                             &condition)) goto failed;
                } else if(starts_word(header, "if")) {
                    if(!taken &&
                       !typed_body_condition(body, header, "if",
                                             &condition)) goto failed;
                } else if(!starts_word(header, "else")) goto failed;
                if(!taken && condition) {
                    if(!typed_body_statements(body, i + 1, close,
                                              flow, result)) goto failed;
                    taken = 1;
                }
                i = close;
                if(i + 1 >= stop ||
                   body->fn->stmts[i + 1].kind != ZIR_STMT_IF ||
                   !starts_word(skip_ws(body->fn->stmts[i + 1].text),
                                "else")) break;
                i++;
            }
            continue;
        }
        if(statement->kind == ZIR_STMT_WHILE) {
            int close = eval_close(body->fn, i, stop);
            if(close < 0) goto failed;
            for(;;) {
                int condition = 0;
                body->current_span = statement->span;
                if(--*body->fuel < 0 ||
                   !typed_body_condition(body, source, "while",
                                         &condition)) goto failed;
                if(!condition) break;
                if(!typed_body_statements(body, i + 1, close,
                                          flow, result)) goto failed;
                if(*flow == 2) { *flow = 0; break; }
                if(*flow == 3) *flow = 0;
                if(*flow == 1) break;
            }
            i = close;
            continue;
        }
        if(statement->kind == ZIR_STMT_BLOCK_OPEN) {
            int close = eval_close(body->fn, i, stop);
            if(close < 0 ||
               !typed_body_statements(body, i + 1, close,
                                      flow, result)) goto failed;
            i = close;
            continue;
        }
        if(statement->kind == ZIR_STMT_RETURN) {
            if(!typed_body_expression(body, skip_ws(source + 6),
                                      result)) goto failed;
            *flow = 1;
            break;
        }
        if(statement->kind == ZIR_STMT_DECL) {
            char text[ZIR_TEXT_MAX];
            copy_text(text, sizeof(text), source);
            char *colon = strchr(text, ':');
            if(colon == NULL) goto failed;
            *colon++ = '\0';
            trim_in_place(text);
            char *equals = strchr(colon, '=');
            if(equals != NULL) *equals++ = '\0';
            trim_in_place(colon);
            size_t length = strlen(colon);
            if(length && colon[length - 1] == ';') {
                colon[length - 1] = '\0';
                trim_in_place(colon);
            }
            CompileValue value = {0};
            if(equals != NULL) {
                if(!typed_body_expression(body, equals, &value)) goto failed;
            }
            const char *type = colon[0] ? colon :
                value.kind == COMPILE_REAL ? "float64" :
                value.kind == COMPILE_STRING ? "string" :
                value.kind == COMPILE_COMPOUND ? value.type : "s64";
            if(equals != NULL && !compile_type_value(type, &value))
                goto failed;
            if(!typed_add_local(body, text, type,
                                equals != NULL ? &value : NULL)) goto failed;
            continue;
        }
        if(statement->kind == ZIR_STMT_ASSIGN) {
            char text[ZIR_TEXT_MAX];
            copy_text(text, sizeof(text), source);
            char *equals = strchr(text, '=');
            if(equals == NULL) goto failed;
            char op = equals > text ? equals[-1] : '\0';
            if(op && strchr("+-*/%", op) != NULL) equals[-1] = '\0';
            else op = '\0';
            *equals++ = '\0';
            trim_in_place(text);
            int local = typed_local_index(body, text);
            if(local < 0) goto failed;
            CompileValue value = {0};
            if(op) {
                char combined[ZIR_TEXT_MAX];
                int written = snprintf(combined, sizeof(combined),
                    "(%s) %c (%s)", body->names.items[local].expr, op,
                    equals);
                if(written < 0 || (size_t)written >= sizeof(combined) ||
                   !typed_body_expression(body, combined, &value)) goto failed;
            } else if(!typed_body_expression(body, equals, &value)) goto failed;
            if(!compile_type_value(body->names.items[local].type,
                                   &value)) goto failed;
            copy_text(body->names.items[local].expr,
                      sizeof(body->names.items[local].expr), value.literal);
            continue;
        }
        if(statement->kind == ZIR_STMT_EXPR) {
            CompileValue ignored = {0};
            if(!typed_body_expression(body, source, &ignored)) goto failed;
            continue;
        }
        if(statement->kind == ZIR_STMT_BREAK ||
           statement->kind == ZIR_STMT_CONTINUE) {
            char control[ZIR_TEXT_MAX];
            copy_text(control, sizeof(control), source);
            trim_in_place(control);
            size_t length = strlen(control);
            if(length && control[length - 1] == ';')
                control[length - 1] = '\0';
            if(strcmp(control, statement->kind == ZIR_STMT_BREAK ?
                       "break" : "continue") != 0) goto failed;
            *flow = statement->kind == ZIR_STMT_BREAK ? 2 : 3;
            break;
        }
        goto failed;
    }
    typed_leave_scope(body, saved);
    return 1;
failed:
    typed_leave_scope(body, saved);
    return 0;
}

static int
evaluate_typed_function(const ZirModule *module, const char *name,
                        ZirSourceSpan call_span, CompileValue *arguments,
                        char argument_names[][ZIR_NAME_MAX],
                        int argument_count, int depth, int *fuel,
                        CompileValue *result)
{
    const ZirModule *owner = NULL;
    const ZirFunction *fn = NULL;
    char parameters[16][ZIR_TEXT_MAX];
    char defaults[16][ZIR_TEXT_MAX] = {{0}};
    CompileValue ordered[16] = {{0}};
    unsigned used = 0;
    TypedBody body = {0};
    int expected, flow = 0, ok = 0;
    if(depth >= 32 || module == NULL || !name[0] ||
       ResolveFunctionAt(module, name, call_span.path, &owner, &fn) != 1 ||
       fn == NULL || fn->is_extern || fn->is_template ||
       fn->stmt_count == 0) return 0;
    expected = *skip_ws(fn->args) ?
        split_top_level(fn->args, parameters[0], 16,
                        sizeof(parameters[0])) : 0;
    if(expected < 0 || argument_count > expected) return 0;
    if(fn->default_args[0] &&
       split_top_level(fn->default_args, defaults[0], 16,
                       sizeof(defaults[0])) != expected) return 0;
    for(int argument = 0; argument < argument_count; argument++) {
        int position = -1;
        if(argument_names[argument][0]) {
            size_t length = strlen(argument_names[argument]);
            for(int i = 0; i < expected; i++) {
                const char *start = skip_ws(parameters[i]);
                const char *colon = strchr(start, ':');
                if(colon == NULL) continue;
                const char *end = colon;
                while(end > start && isspace((unsigned char)end[-1])) end--;
                if((size_t)(end - start) == length &&
                   !strncmp(start, argument_names[argument], length)) {
                    position = i;
                    break;
                }
            }
        } else {
            for(int i = 0; i < expected; i++)
                if(!(used & (1u << i))) { position = i; break; }
        }
        if(position < 0 || (used & (1u << position))) return 0;
        used |= 1u << position;
        ordered[position] = arguments[argument];
    }
    body.capacity = expected + owner->define_count + fn->stmt_count + 1;
    body.names.items = calloc((size_t)body.capacity,
                              sizeof(*body.names.items));
    if(body.names.items == NULL) die("out of memory evaluating #run procedure");
    body.module = owner;
    body.fn = fn;
    body.depth = depth;
    body.fuel = fuel;
    for(int i = 0; i < expected; i++) {
        char *part = trim(parameters[i]);
        char *colon = strchr(part, ':');
        if(colon == NULL) goto done;
        *colon++ = '\0';
        trim_in_place(part);
        char *type = trim(colon);
        char *default_value = top_level_assignment(defaults[i]);
        if(default_value != NULL) default_value = trim(default_value + 1);
        char *embedded_default = strchr(type, '=');
        if(embedded_default != NULL) {
            *embedded_default++ = '\0';
            trim_in_place(type);
            if(default_value == NULL) default_value = trim(embedded_default);
        }
        if(!type[0] && default_value != NULL &&
           !strcmp(default_value, "#caller_location"))
            type = "Source_Code_Location";
        if(!(used & (1u << i))) {
            if(default_value == NULL) goto done;
            if(!strcmp(default_value, "#caller_location")) {
                ordered[i].kind = COMPILE_COMPOUND;
                copy_text(ordered[i].type, sizeof(ordered[i].type),
                          "Source_Code_Location");
                ordered[i].type_owner = owner;
                if(!CallerLocationLiteral(module, call_span,
                                          ordered[i].literal,
                                          sizeof(ordered[i].literal))) goto done;
            } else if(!evaluate_typed_expression(owner, &body.names,
                       default_value, fn->span, depth + 1, fuel,
                       &ordered[i])) goto done;
        }
        if(!is_identifier_text(part) ||
           !compile_type_value(type, &ordered[i])) goto done;
        ZirConst *binding = &body.names.items[i];
        copy_text(binding->name, sizeof(binding->name), part);
        copy_text(binding->type, sizeof(binding->type), type);
        copy_text(binding->expr, sizeof(binding->expr),
                  ordered[i].literal);
        copy_text(binding->path, sizeof(binding->path), fn->span.path);
        body.names.count++;
    }
    body.local_count = expected;
    for(int i = 0; i < owner->define_count; i++) {
        const ZirDefine *definition = &owner->defines[i];
        ZirConst *constant = &body.names.items[body.names.count++];
        copy_text(constant->name, sizeof(constant->name),
                  definition->name);
        copy_text(constant->expr, sizeof(constant->expr),
                  definition->value);
        copy_text(constant->path, sizeof(constant->path),
                  definition->span.path);
        constant->is_file_private = definition->is_file_private;
    }
    ok = typed_body_statements(&body, 0, fn->stmt_count,
                               &flow, result) && flow == 1 &&
         compile_type_value(fn->return_type, result);
done:
    free(body.names.items);
    return ok;
}

static int
evaluate_typed_integer_function(ZirEval *ev, const char *name,
                                const long *values,
                                const char argument_names[][ZIR_NAME_MAX],
                                int argument_count, long *result)
{
    CompileValue arguments[16] = {{0}}, value = {0};
    char names[16][ZIR_NAME_MAX] = {{0}};
    int local_fuel = 10000;
    int *fuel = ev->fuel != NULL ? ev->fuel : &local_fuel;
    if(ev->module == NULL || argument_count < 0 || argument_count > 16 ||
       ev->depth >= 16) return 0;
    for(int i = 0; i < argument_count; i++) {
        arguments[i].kind = COMPILE_INTEGER;
        arguments[i].integer = values[i];
        if(!compile_value_literal(&arguments[i])) return 0;
        copy_text(names[i], sizeof(names[i]), argument_names[i]);
    }
    if(!evaluate_typed_function(ev->module, name,
                                Span(ev->lookup_path, ev->source_line, 1),
                                arguments, names, argument_count,
                                ev->depth + 1, fuel, &value) ||
       value.kind != COMPILE_INTEGER) return 0;
    *result = value.integer;
    return 1;
}

static int
eval_typed_condition(const char *source, const ZirModule *module,
                     const ZirConsts *names, ZirSourceSpan span,
                     long *result)
{
    int fuel = 10000, truth;
    CompileValue value = {0};
    if(!evaluate_typed_expression(module, names, source, span, 0,
                                  &fuel, &value) ||
       !compile_truth(&value, &truth)) return 0;
    *result = truth;
    return 1;
}

int
EvaluateCompileExpression(const ZirModule *module, const char *source,
                          ZirSourceSpan span, int executing, long *value)
{
    ZirConsts constants = {0};
    char expanded[ZIR_TEXT_MAX];
    if(module == NULL || source == NULL || value == NULL) return 0;
    if(module->define_count > 0) {
        constants.items = calloc((size_t)module->define_count,
                                 sizeof(*constants.items));
        if(constants.items == NULL) return 0;
        constants.count = module->define_count;
        for(int i = 0; i < module->define_count; i++) {
            const ZirDefine *definition = &module->defines[i];
            ZirConst *constant = &constants.items[i];
            copy_text(constant->name, sizeof(constant->name),
                      definition->name);
            copy_text(constant->expr, sizeof(constant->expr),
                      definition->value);
            copy_text(constant->path, sizeof(constant->path),
                      definition->span.path);
            constant->is_file_private = definition->is_file_private;
        }
    }
    expand_compile_expr(expanded, sizeof(expanded), &constants,
                        source, span.path);
    if(strstr(expanded, "size_of") != NULL)
        lower_size_of_value(expanded, sizeof(expanded), module, span);
    int ok = eval_const_condition(expanded, value, module, &constants,
                                  span.path, span.line, executing ? 1 : 0);
    free(constants.items);
    return ok;
}

int
EvaluateCompileLiteral(const ZirModule *module, const char *source,
                       ZirSourceSpan span, int executing, char *literal,
                       size_t literal_size,
                       const ZirModule **type_owner)
{
    ZirConsts constants = {0};
    CompileValue value = {0};
    int fuel = 10000;
    if(module == NULL || source == NULL || literal == NULL ||
       literal_size == 0) return 0;
    constants.count = module->define_count;
    constants.items = calloc((size_t)constants.count + 1,
                             sizeof(*constants.items));
    if(constants.items == NULL) return 0;
    for(int i = 0; i < constants.count; i++) {
        const ZirDefine *definition = &module->defines[i];
        ZirConst *constant = &constants.items[i];
        copy_text(constant->name, sizeof(constant->name),
                  definition->name);
        copy_text(constant->expr, sizeof(constant->expr),
                  definition->value);
        copy_text(constant->path, sizeof(constant->path),
                  definition->span.path);
        constant->is_file_private = definition->is_file_private;
    }
    int ok = evaluate_typed_expression(module, &constants, source,
                                       span, executing ? 1 : 0,
                                       &fuel, &value) &&
             value.kind != COMPILE_INVALID &&
             strlen(value.literal) < literal_size;
    if(ok) {
        copy_text(literal, literal_size, value.literal);
        if(type_owner != NULL) *type_owner = value.type_owner;
    }
    free(constants.items);
    return ok;
}

static void lower_size_of_value(char *value, size_t capacity,
                                const ZirModule *module, ZirSourceSpan span);

typedef struct CompileParseContext {
    ZirProgram *program;
    const char *root;
    ZirCompileImportResolver resolver;
    void *resolver_context;
    const ZirConsts *future_constants;
    const ZirUsings *future_usings;
    const ZirImports *future_imports;
    const ZirTypes *future_types;
    const ZirFunctions *future_functions;
    const ZirGlobals *future_globals;
} CompileParseContext;

static ZirConsts
visible_compile_constants(const ZirConsts *parsed, const ZirConsts *future)
{
    ZirConsts visible = {0};
    int extra = future != NULL ? future->count : 0;
    visible.items = calloc((size_t)parsed->count + (size_t)extra + 1,
                           sizeof(*visible.items));
    if(visible.items == NULL)
        die("out of memory resolving compile-time constants");
    for(int i = 0; i < parsed->count; i++)
        visible.items[visible.count++] = parsed->items[i];
    for(int i = 0; i < extra; i++) {
        const ZirConst *candidate = &future->items[i];
        int present = 0;
        for(int j = 0; j < parsed->count; j++)
            if(!strcmp(parsed->items[j].name, candidate->name) &&
               !strcmp(parsed->items[j].path, candidate->path)) {
                present = 1;
                break;
            }
        if(!present)
            visible.items[visible.count++] = *candidate;
    }
    return visible;
}

static ZirModule
visible_compile_usings(const ZirModule *parsed, const ZirUsings *future)
{
    ZirModule visible = *parsed;
    int extra = future != NULL ? future->count : 0;
    visible.usings = calloc((size_t)parsed->using_count + (size_t)extra + 1,
                            sizeof(*visible.usings));
    if(visible.usings == NULL)
        die("out of memory resolving compile-time using declarations");
    visible.using_count = 0;
    for(int i = 0; i < parsed->using_count; i++)
        visible.usings[visible.using_count++] = parsed->usings[i];
    for(int i = 0; i < extra; i++) {
        const ZirUsing *candidate = &future->items[i];
        int present = 0;
        for(int j = 0; j < parsed->using_count; j++)
            if(!strcmp(parsed->usings[j].span.path, candidate->span.path) &&
               parsed->usings[j].span.line == candidate->span.line) {
                present = 1;
                break;
            }
        if(!present)
            visible.usings[visible.using_count++] = *candidate;
    }
    return visible;
}

static void
add_visible_compile_defines(ZirModule *visible, const ZirModule *parsed,
                            const ZirConsts *constants)
{
    visible->defines = calloc((size_t)parsed->define_count +
                              (size_t)constants->count + 1,
                              sizeof(*visible->defines));
    if(visible->defines == NULL)
        die("out of memory resolving compile-time constants");
    visible->define_count = 0;
    for(int i = 0; i < parsed->define_count; i++)
        visible->defines[visible->define_count++] = parsed->defines[i];
    for(int i = 0; i < constants->count; i++) {
        const ZirConst *constant = &constants->items[i];
        int present = 0;
        for(int j = 0; j < parsed->define_count; j++)
            if(!strcmp(parsed->defines[j].name, constant->name) &&
               !strcmp(parsed->defines[j].span.path, constant->path)) {
                present = 1;
                break;
            }
        if(present) continue;
        ZirDefine *definition = &visible->defines[visible->define_count++];
        copy_text(definition->name, sizeof(definition->name),
                  constant->name);
        copy_text(definition->value, sizeof(definition->value),
                  constant->expr);
        definition->is_public = constant->is_public;
        definition->is_file_private = constant->is_file_private;
        definition->span = Span(constant->path, constant->source_line, 1);
    }
}

static void
add_visible_compile_imports(ZirModule *visible, const ZirModule *parsed,
                            const ZirImports *future)
{
    int extra = future != NULL ? future->count : 0;
    visible->imports = calloc((size_t)parsed->import_count +
                              (size_t)extra + 1,
                              sizeof(*visible->imports));
    if(visible->imports == NULL)
        die("out of memory resolving compile-time imports");
    visible->import_count = 0;
    for(int i = 0; i < parsed->import_count; i++)
        visible->imports[visible->import_count++] = parsed->imports[i];
    for(int i = 0; i < extra; i++) {
        const ZirImport *candidate = &future->items[i];
        int present = 0;
        for(int j = 0; j < parsed->import_count; j++)
            if(!strcmp(parsed->imports[j].span.path, candidate->span.path) &&
               parsed->imports[j].span.line == candidate->span.line) {
                present = 1;
                break;
            }
        if(!present)
            visible->imports[visible->import_count++] = *candidate;
    }
}

static void
add_visible_compile_types(ZirModule *visible, const ZirModule *parsed,
                          const ZirTypes *future)
{
    int extra = future != NULL ? future->count : 0;
    visible->types = calloc((size_t)parsed->type_count + (size_t)extra + 1,
                            sizeof(*visible->types));
    if(visible->types == NULL)
        die("out of memory resolving compile-time types");
    visible->type_count = 0;
    for(int i = 0; i < parsed->type_count; i++)
        visible->types[visible->type_count++] = parsed->types[i];
    for(int i = 0; i < extra; i++) {
        const ZirType *candidate = &future->items[i];
        int present = 0;
        for(int j = 0; j < parsed->type_count; j++)
            if(!strcmp(parsed->types[j].span.path, candidate->span.path) &&
               parsed->types[j].span.line == candidate->span.line) {
                present = 1;
                break;
            }
        if(!present)
            visible->types[visible->type_count++] = *candidate;
    }
}

static void
add_visible_compile_functions(ZirModule *visible, const ZirModule *parsed,
                              const ZirFunctions *future)
{
    int extra = future != NULL ? future->count : 0;
    visible->functions = calloc((size_t)parsed->function_count +
                                (size_t)extra + 1,
                                sizeof(*visible->functions));
    if(visible->functions == NULL)
        die("out of memory resolving compile-time procedures");
    visible->function_count = 0;
    for(int i = 0; i < parsed->function_count; i++)
        visible->functions[visible->function_count++] = parsed->functions[i];
    for(int i = 0; i < extra; i++) {
        const ZirFunction *candidate = &future->items[i];
        int present = 0;
        for(int j = 0; j < parsed->function_count; j++)
            if(!strcmp(parsed->functions[j].span.path,
                       candidate->span.path) &&
               parsed->functions[j].span.line == candidate->span.line) {
                present = 1;
                break;
            }
        if(!present)
            visible->functions[visible->function_count++] = *candidate;
    }
}

static void
add_visible_compile_globals(ZirModule *visible, const ZirModule *parsed,
                            const ZirGlobals *future)
{
    int extra = future != NULL ? future->count : 0;
    visible->globals = calloc((size_t)parsed->global_count +
                              (size_t)extra + 1,
                              sizeof(*visible->globals));
    if(visible->globals == NULL)
        die("out of memory resolving compile-time globals");
    visible->global_count = 0;
    for(int i = 0; i < parsed->global_count; i++)
        visible->globals[visible->global_count++] = parsed->globals[i];
    for(int i = 0; i < extra; i++) {
        const ZirGlobal *candidate = &future->items[i];
        int present = 0;
        for(int j = 0; j < parsed->global_count; j++)
            if(!strcmp(parsed->globals[j].span.path,
                       candidate->span.path) &&
               parsed->globals[j].span.line == candidate->span.line) {
                present = 1;
                break;
            }
        if(!present)
            visible->globals[visible->global_count++] = *candidate;
    }
}

static ZirModule
visible_compile_module(const ZirModule *parsed,
                       const CompileParseContext *context,
                       const ZirConsts *constants)
{
    ZirModule visible = visible_compile_usings(parsed,
        context != NULL ? context->future_usings : NULL);
    add_visible_compile_defines(&visible, parsed, constants);
    add_visible_compile_imports(&visible, parsed,
        context != NULL ? context->future_imports : NULL);
    add_visible_compile_types(&visible, parsed,
        context != NULL ? context->future_types : NULL);
    add_visible_compile_functions(&visible, parsed,
        context != NULL ? context->future_functions : NULL);
    add_visible_compile_globals(&visible, parsed,
        context != NULL ? context->future_globals : NULL);
    return visible;
}

static void
free_visible_compile_module(ZirModule *visible)
{
    free(visible->usings);
    free(visible->defines);
    free(visible->imports);
    free(visible->types);
    free(visible->functions);
    free(visible->globals);
}

static int compile_size_of_ready(const ZirModule *module, char *condition);

static int
select_compile_condition(ZirModule *module, const ZirConsts *consts,
                         const char *source, ZirSourceSpan span,
                         const CompileParseContext *context,
                         const char *source_path, int *deferred)
{
    ZirModule *source_module = module;
    char expanded[ZIR_TEXT_MAX];
    long value = 0;
    int known;
    ZirConsts visible = visible_compile_constants(consts,
        context != NULL ? context->future_constants : NULL);
    ZirModule using_scope = visible_compile_module(module, context, &visible);
    module = &using_scope;
    consts = &visible;
    expand_compile_expr(expanded, sizeof(expanded), consts, source, span.path);
    normalize_jai_source_tokens(expanded, span.path, source_path, span.line);
    if(using_scope.using_count > 0 && context != NULL &&
       context->resolver != NULL &&
       !context->resolver(context->resolver_context, context->program,
                          module, source_path, context->root, NULL)) {
        if(deferred != NULL) { *deferred = 1; goto not_ready; }
        die_at(span, "cannot resolve imports for #if enum member");
    }
    if(!LowerFileScopeUsing(module, expanded, sizeof(expanded), span)) {
        if(deferred != NULL) { *deferred = 1; goto not_ready; }
        die_at(span, "invalid #if enum member");
    }
    if(strstr(expanded, "size_of") != NULL && context != NULL &&
       context->resolver != NULL &&
       !context->resolver(context->resolver_context, context->program,
                          module, source_path, context->root, expanded)) {
        if(deferred != NULL) { *deferred = 1; goto not_ready; }
        die_at(span, "cannot resolve imports for #if condition");
    }
    if(deferred != NULL && strstr(expanded, "size_of") != NULL &&
       !compile_size_of_ready(module, expanded)) {
        *deferred = 1;
        goto not_ready;
    }
    if(strstr(expanded, "size_of") != NULL)
        lower_size_of_value(expanded, sizeof(expanded), module, span);
    known = eval_const_condition(expanded, &value, module, consts,
                                 span.path, span.line, 0);
    if(!known)
        known = eval_typed_condition(expanded, module, consts,
                                     span, &value);
    if(!known && context != NULL) {
        /* Embedded source imports already belong to this partially parsed
         * program. External imports are loaded on demand by ProgramsLoad. */
        for(int i = 0; i < module->import_count; i++) {
            ZirImport *import = &module->imports[i];
            if(import->resolved_module != NULL ||
               (import->kind != ZIR_IMPORT_OPEN &&
                import->kind != ZIR_IMPORT_MODULE)) continue;
            for(int m = 0; m < context->program->module_count; m++) {
                ZirModule *candidate = &context->program->modules[m];
                if(candidate != source_module &&
                   strcmp(candidate->name, import->target) == 0) {
                    import->resolved_module = candidate;
                    break;
                }
            }
        }
        if(context->resolver != NULL &&
           !context->resolver(context->resolver_context, context->program,
                              module, source_path, context->root,
                              expanded)) {
            if(deferred != NULL) { *deferred = 1; goto not_ready; }
            die_at(span, "cannot resolve imports for #if condition");
        }
        if(!LowerFileScopeUsing(module, expanded, sizeof(expanded), span)) {
            if(deferred != NULL) { *deferred = 1; goto not_ready; }
            die_at(span, "invalid #if enum member");
        }
        known = eval_const_condition(expanded, &value, module, consts,
                                     span.path, span.line, 0);
        if(!known)
            known = eval_typed_condition(expanded, module, consts,
                                         span, &value);
    }
    if(!known) {
        if(deferred != NULL) { *deferred = 1; goto not_ready; }
        die_at(span, "#if condition is not a compile-time constant: %s",
               expanded);
    }
    free_visible_compile_module(&using_scope);
    free(visible.items);
    return value != 0;
not_ready:
    free_visible_compile_module(&using_scope);
    free(visible.items);
    return 0;
}

static char *
find_unquoted_text(char *source, const char *needle)
{
    size_t length = strlen(needle);
    for(char *p = source; *p;) {
        if(*p == '"' || *p == '\'') {
            char quote = *p++;
            while(*p && *p != quote) {
                if(*p == '\\' && p[1]) p++;
                p++;
            }
            if(*p) p++;
            continue;
        }
        if(!strncmp(p, needle, length)) return p;
        p++;
    }
    return NULL;
}

static char *
find_unquoted_word(char *source, const char *word)
{
    size_t length = strlen(word);
    for(char *at = find_unquoted_text(source, word); at != NULL;
        at = find_unquoted_text(at + length, word)) {
        int before = at == source ? 0 : (unsigned char)at[-1];
        int after = (unsigned char)at[length];
        if(!(isalnum(before) || before == '_') &&
           !(isalnum(after) || after == '_'))
            return at;
    }
    return NULL;
}

static const ZirExpr *
outer_compile_ifx(const ZirFunction *fn)
{
    const ZirExpr *found = NULL;
    for(int i = 0; i < fn->expr_count; i++) {
        const ZirExpr *expr = &fn->exprs[i];
        if(expr->kind == ZIR_EXPR_CONDITIONAL &&
           !strcmp(expr->op, "#ifx") &&
           (expr->left < 0 ||
            strstr(fn->exprs[expr->left].text, "#ifx") == NULL) &&
           (found == NULL || strlen(expr->text) > strlen(found->text)))
            found = expr;
    }
    return found;
}

static int
compile_size_of_ready(const ZirModule *module, char *condition)
{
    if(find_unquoted_word(condition, "size_of") != NULL)
        for(int i = 0; i < module->import_count; i++)
            if((module->imports[i].kind == ZIR_IMPORT_OPEN ||
                module->imports[i].kind == ZIR_IMPORT_MODULE) &&
               module->imports[i].resolved_module == NULL)
                return 0;
    for(char *at = find_unquoted_word(condition, "size_of"); at != NULL;
        at = find_unquoted_word(at + 7, "size_of")) {
        char *start = (char *)skip_ws(at + 7);
        if(*start != '(') return 0;
        start++;
        char *end = start;
        int depth = 1;
        while(*end && depth > 0) {
            if(*end == '(') depth++;
            else if(*end == ')') depth--;
            if(depth > 0) end++;
        }
        if(depth != 0 || (size_t)(end - start) >= ZIR_TEXT_MAX)
            return 0;
        char type[ZIR_TEXT_MAX];
        size_t length = (size_t)(end - start);
        memcpy(type, start, length);
        type[length] = '\0';
        trim_in_place(type);
        size_t size, alignment;
        if(!TypeLayout(module, type, &size, &alignment)) return 0;
    }
    return 1;
}

static int
replace_compile_ifx(char *source, size_t capacity, const ZirFunction *fn,
                    const ZirModule *module, const ZirExpr *expr,
                    const ZirConsts *consts,
                    ZirSourceSpan span, int allow_deferred)
{
    char condition[ZIR_TEXT_MAX], rewritten[ZIR_TEXT_MAX];
    long value = 0;
    char *at;
    const char *selected;
    int written;

    if(expr->left < 0 || expr->right < 0 || expr->third < 0)
        die_at(span, "malformed #ifx expression");
    expand_compile_expr(condition, sizeof(condition), consts,
                        fn->exprs[expr->left].text, span.path);
    if(allow_deferred && !compile_size_of_ready(module, condition))
        return 0;
    if(strstr(condition, "size_of") != NULL)
        lower_size_of_value(condition, sizeof(condition), module, span);
    if(!eval_const_condition(condition, &value, module, consts,
                             span.path, span.line, 0) &&
       !eval_typed_condition(condition, module, consts,
                             span, &value)) {
        if(allow_deferred) return 0;
        die_at(span, "#ifx condition is not a compile-time constant: %s",
               condition);
    }
    selected = fn->exprs[value ? expr->right : expr->third].text;
    at = find_unquoted_text(source, expr->text);
    if(at == NULL)
        die_at(span, "cannot locate #ifx expression in source");
    written = snprintf(rewritten, sizeof(rewritten), "%.*s(%s)%s",
                       (int)(at - source), source, selected,
                       at + strlen(expr->text));
    if(written < 0 || (size_t)written >= sizeof(rewritten) ||
       (size_t)written >= capacity)
        die_at(span, "#ifx selection exceeds expression limit");
    copy_text(source, capacity, rewritten);
    return 1;
}

static int
lower_compile_ifx_value(char *value, size_t capacity,
                        const ZirModule *module, const ZirConsts *consts,
                        ZirSourceSpan span, int allow_deferred)
{
    for(int pass = 0; pass < 128; pass++) {
        ZirFunction probe = {0};
        ParseExpr(&probe, module, value, span);
        const ZirExpr *expr = outer_compile_ifx(&probe);
        if(expr == NULL) {
            free(probe.exprs);
            if(find_unquoted_text(value, "#ifx") != NULL)
                die_at(span, "invalid #ifx expression");
            return 1;
        }
        int replaced = replace_compile_ifx(value, capacity, &probe, module,
                                           expr, consts, span,
                                           allow_deferred);
        free(probe.exprs);
        if(!replaced) return 0;
    }
    die_at(span, "too many nested #ifx expressions");
}

static int
lower_compile_ifx_function(ZirFunction *fn, const ZirModule *module,
                           const ZirConsts *consts, int allow_deferred)
{
    for(int pass = 0; pass < 128; pass++) {
        StructureFunction(fn, module);
        const ZirExpr *expr = outer_compile_ifx(fn);
        if(expr == NULL) {
            for(int i = 0; i < fn->stmt_count; i++)
                if(find_unquoted_text(fn->stmts[i].text, "#ifx") != NULL)
                    die_at(fn->stmts[i].span, "invalid #ifx expression");
            return 1;
        }
        int replaced = 0;
        for(int i = 0; i < fn->stmt_count; i++) {
            ZirStmt *st = &fn->stmts[i];
            if(find_unquoted_text(st->text, expr->text) == NULL)
                continue;
            if(!replace_compile_ifx(st->text, sizeof(st->text), fn, module,
                                    expr, consts, st->span,
                                    allow_deferred)) return 0;
            replaced = 1;
            break;
        }
        if(!replaced)
            die_at(fn->span, "cannot locate #ifx expression in function");
    }
    die_at(fn->span, "too many nested #ifx expressions");
}

static int
function_has_compile_ifx(const ZirFunction *fn)
{
    for(int i = 0; i < fn->stmt_count; i++)
        if(find_unquoted_text((char *)fn->stmts[i].text, "#ifx") != NULL)
            return 1;
    return 0;
}

static const ZirExpr *
first_size_of(const ZirFunction *fn, int include_type_of)
{
    for(int i = 0; i < fn->expr_count; i++)
        if(fn->exprs[i].kind == ZIR_EXPR_SIZE_OF &&
           (include_type_of ||
            strncmp(skip_ws(fn->exprs[i].name), "type_of", 7) != 0))
            return &fn->exprs[i];
    return NULL;
}

static void
replace_size_of(char *source, size_t capacity, const ZirModule *module,
                const ZirExpr *expr, ZirSourceSpan span)
{
    char rewritten[ZIR_TEXT_MAX];
    char operand[ZIR_TEXT_MAX], inferred[ZIR_NAME_MAX];
    size_t size, alignment;
    char *at = find_unquoted_text(source, expr->text);
    const char *sized_type = expr->name;
    int written;

    if(TypeOfOperand(expr->name, operand, sizeof(operand))) {
        if(!InferExpressionType(module, operand, span, inferred,
                                sizeof(inferred)))
            die_at(span, "size_of(type_of(...)) requires a checked expression");
        sized_type = inferred;
    } else if(!JaiTypeSpelling(span, expr->name)) exit(1);
    if(!TypeLayout(module, sized_type, &size, &alignment) ||
       size > LONG_MAX)
        die_at(span, "size_of requires a known sized type: %s", sized_type);
    if(at == NULL)
        die_at(span, "cannot locate size_of expression in source");
    written = snprintf(rewritten, sizeof(rewritten), "%.*s(%zu)%s",
                       (int)(at - source), source, size,
                       at + strlen(expr->text));
    if(written < 0 || (size_t)written >= sizeof(rewritten) ||
       (size_t)written >= capacity)
        die_at(span, "size_of selection exceeds expression limit");
    copy_text(source, capacity, rewritten);
}

static void
lower_size_of_value(char *value, size_t capacity, const ZirModule *module,
                    ZirSourceSpan span)
{
    for(int pass = 0; pass < 128; pass++) {
        ZirFunction probe = {0};
        ParseExpr(&probe, module, value, span);
        const ZirExpr *expr = first_size_of(&probe, 1);
        if(expr == NULL) {
            free(probe.exprs);
            if(find_unquoted_word(value, "size_of") != NULL)
                die_at(span, "invalid size_of expression");
            return;
        }
        replace_size_of(value, capacity, module, expr, span);
        free(probe.exprs);
    }
    die_at(span, "too many size_of expressions");
}

/* Imports are linked after parsing. Finish any #ifx selection that called
 * into another module, then lower size_of only in the selected branch. */
int
LowerLinkedCompileExpressions(ZirModule *module, int allow_deferred)
{
    ZirConsts constants = {0};
    char saved_lookup_path[ZIR_PATH_MAX];
    int progress = 0;
    copy_text(saved_lookup_path, sizeof(saved_lookup_path),
              module->lookup_path);
    constants.count = module->define_count;
    constants.items = calloc((size_t)constants.count + 1,
                             sizeof(*constants.items));
    if(constants.items == NULL) die("out of memory lowering #ifx");
    for(int i = 0; i < constants.count; i++) {
        ZirDefine *definition = &module->defines[i];
        ZirConst *constant = &constants.items[i];
        copy_text(constant->name, sizeof(constant->name), definition->name);
        copy_text(constant->expr, sizeof(constant->expr), definition->value);
        copy_text(constant->path, sizeof(constant->path),
                  definition->span.path);
        constant->is_file_private = definition->is_file_private;
    }
    for(int i = 0; i < module->define_count; i++) {
        ZirDefine *definition = &module->defines[i];
        char before[ZIR_TEXT_MAX];
        copy_text(module->lookup_path, sizeof(module->lookup_path),
                  definition->span.path);
        copy_text(before, sizeof(before), definition->value);
        if(find_unquoted_text(definition->value, "#ifx") != NULL)
            lower_compile_ifx_value(definition->value,
                                    sizeof(definition->value), module,
                                    &constants, definition->span,
                                    allow_deferred);
        if(!starts_word(definition->value, "#run") &&
           find_unquoted_text(definition->value, "#ifx") == NULL &&
           find_unquoted_word(definition->value, "size_of") != NULL)
            lower_size_of_value(definition->value,
                                sizeof(definition->value), module,
                                definition->span);
        if(strcmp(before, definition->value) != 0) progress++;
        copy_text(constants.items[i].expr,
                  sizeof(constants.items[i].expr), definition->value);
    }
    for(int i = 0; i < module->global_count; i++) {
        ZirGlobal *global = &module->globals[i];
        char before[ZIR_TEXT_MAX];
        copy_text(module->lookup_path, sizeof(module->lookup_path),
                  global->span.path);
        copy_text(before, sizeof(before), global->init);
        if(find_unquoted_text(global->init, "#ifx") != NULL)
            lower_compile_ifx_value(global->init, sizeof(global->init),
                                    module, &constants, global->span,
                                    allow_deferred);
        if(find_unquoted_text(global->init, "#ifx") == NULL &&
           find_unquoted_word(global->init, "size_of") != NULL)
            lower_size_of_value(global->init, sizeof(global->init),
                                module, global->span);
        if(strcmp(before, global->init) != 0) progress++;
    }
    for(int i = 0; i < module->function_count; i++) {
        ZirFunction *fn = &module->functions[i];
        copy_text(module->lookup_path, sizeof(module->lookup_path),
                  fn->span.path);
        if(function_has_compile_ifx(fn)) {
            if(lower_compile_ifx_function(fn, module, &constants,
                                          allow_deferred)) progress++;
        }
    }
    copy_text(module->lookup_path, sizeof(module->lookup_path),
              saved_lookup_path);
    free(constants.items);
    return progress;
}

static int
parse_compile_check(ZirModule *module, const char *path, int line_no,
                    char *line, const ZirConsts *consts)
{
    char cond[ZIR_TEXT_MAX];
    char raw[ZIR_TEXT_MAX];
    char msg[ZIR_TEXT_MAX];

    if(strncmp(line, "#assert", 7) == 0 &&
       (line[7] == '\0' || isspace((unsigned char)line[7]))) {
        char *body = trim(line + 7);
        char *comma;

        if(body[0] == '\0')
            die_at(Span(path, line_no, 1), "#assert needs a condition");
        comma = find_top_comma(body);
        if(comma != NULL) {
            *comma = '\0';
            snprintf(msg, sizeof(msg), "%s", trim(comma + 1));
            if(msg[0] == '\0')
                snprintf(msg, sizeof(msg), "\"Ziran #assert failed\"");
        } else {
            snprintf(msg, sizeof(msg), "\"Ziran #assert failed\"");
        }
        copy_text(raw, sizeof(raw), trim(body));
        expand_compile_expr(cond, sizeof(cond), consts, raw, path);
        {
            long value = 0;
            int known = strstr(cond, "size_of") == NULL &&
                (eval_const_condition(cond, &value, module, consts,
                                      path, line_no, 0) ||
                 eval_typed_condition(cond, module, consts,
                                      Span(path, line_no, 1), &value));

            if(known && !value)
                die_at(Span(path, line_no, 1), "#assert failed: %s", msg);
            if(known)
                copy_text(cond, sizeof(cond), value ? "1" : "0");
            else
                copy_text(cond, sizeof(cond), raw);
            if(!ModuleAddAssert(module, cond, msg, Span(path, line_no, 1)))
                die_at(Span(path, line_no, 1),
                       "out of memory while recording #assert");
        }
        return 1;
    }
    if(strncmp(line, "#error", 6) == 0 &&
       (line[6] == '\0' || isspace((unsigned char)line[6]))) {
        char *body = trim(line + 6);

        if(body[0] == '\0')
            die_at(Span(path, line_no, 1), "#error needs a message");
        die_at(Span(path, line_no, 1), "#error: %s", body);
    }
    return 0;
}

/* Returns 1 when the line is consumed by top-level conditional handling
 * ('#if'/'else #if'/'else' open or retarget a frame; the matching '}'
 * pops one). A plain line inside a region only settles the frame's brace
 * count and returns 0. Only the selected branch is parsed. */
static int
cond_top_step(char *line, ZirCondFrame *frames, int *count,
              ZirModule *module, const ZirConsts *consts,
              const CompileParseContext *context,
              const char *source_path, const char *path, int line_no,
              int *deferred)
{
    char *cnd = NULL;
    int at_branch_boundary = *count == 0 ||
        frames[*count - 1].braces == 1;
    int ck = 0;
    ZirCondFrame *fr;

    /* A runtime `} else {` inside an unselected top-level #if is part of
     * the nested function body. Only the region boundary may retarget the
     * compile-time branch. Do not parse `} else #if` there either: parsing
     * removes its final brace before the depth tracker sees the line. */
    if(at_branch_boundary || starts_word(line, "#if"))
        ck = parse_cond_start(line, &cnd);

    if(ck == 1) {
        if(*count >= 8)
            die_at(Span(path, line_no, 1), "too many nested #if blocks");
        fr = &frames[(*count)++];
        fr->parent_active = *count == 1 || frames[*count - 2].active;
        fr->selected = fr->parent_active &&
            select_compile_condition(module, consts, cnd,
                                     Span(path, line_no, 1),
                                     context, source_path, deferred);
        fr->active = fr->parent_active && fr->selected;
        fr->braces = 1;
        return 1;
    }
    if(*count <= 0) {
        if(ck == 2 || line_is_compile_else(line))
            die_at(Span(path, line_no, 1), "else without a matching #if");
        return 0;
    }
    fr = &frames[*count - 1];
    if(ck == 2) {
        int chosen = fr->parent_active && !fr->selected &&
            select_compile_condition(module, consts, cnd,
                                     Span(path, line_no, 1),
                                     context, source_path, deferred);
        fr->active = fr->parent_active && !fr->selected && chosen;
        fr->selected |= chosen;
        fr->braces = 1;
        return 1;
    }
    if(fr->braces == 1 && line_is_compile_else(line)) {
        fr->active = fr->parent_active && !fr->selected;
        fr->selected = 1;
        fr->braces = 1;
        return 1;
    }
    fr->braces += net_block_braces(line);
    if(fr->braces <= 0) {
        (*count)--;
        return 1;
    }
    return 0;
}

/* A sub-mode (state/type/enum/function) consumed exactly one net '{'
 * from the enclosing region — settle the frame count. */
static void
cond_frame_settle(ZirCondFrame *frames, int count)
{
    if(count > 0)
        frames[count - 1].braces--;
}

/* Strip nested block and line comments in place while preserving newlines.
 * Block depth carries across source lines; quoted delimiters are inert. */
static void
strip_block_comments(char *s, int *comment_depth)
{
    char *w = s;
    char *r = s;
    int in_str = 0;
    int in_chr = 0;

    while(*r != '\0') {
        if(*comment_depth) {
            if(*r == '/' && r[1] == '*') {
                (*comment_depth)++;
                r += 2;
            } else if(*r == '*' && r[1] == '/') {
                (*comment_depth)--;
                r += 2;
                if(*comment_depth == 0)
                    *w++ = ' ';   /* keep tokens on either side apart */
            } else {
                if(*r == '\n') *w++ = '\n';
                r++;
            }
            continue;
        }
        if(in_str || in_chr) {
            if(*r == '\\' && r[1] != '\0') {
                *w++ = *r++;
                *w++ = *r++;
                continue;
            }
            if((in_str && *r == '"') || (in_chr && *r == '\''))
                in_str = in_chr = 0;
            else if(*r == '\n')
                in_str = in_chr = 0;
            *w++ = *r++;
        } else if(*r == '"') {
            in_str = 1;
            *w++ = *r++;
        } else if(*r == '\'') {
            in_chr = 1;
            *w++ = *r++;
        } else if(*r == '/' && r[1] == '/') {
            while(*r != '\0' && *r != '\n') r++;
            if(*r == '\n') *w++ = *r++;
        } else if(*r == '/' && r[1] == '*') {
            *comment_depth = 1;
            r += 2;
        } else if(*r == '\n') {
            *w++ = *r++;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

/* Canonicalize Jai's scalar aliases and source-position expressions before
 * parsing declarations. Quoted text and comments are left intact. */
static void
normalize_jai_source_tokens(char *line, const char *path,
                            const char *physical_path, int line_no)
{
    static const struct { const char *jai, *internal; } names[] = {
        {"int", "s64"}, {"float", "float32"}
    };
    static const char *const old[] = {
        "i8", "i16", "i32", "i64", "f32", "f64", "double", NULL
    };
    char normalized[SOURCE_LINE_MAX];
    size_t used = 0;
    for(const char *p = line; *p; ) {
        if(*p == '/' && p[1] == '/') {
            size_t remaining = strlen(p);
            if(used + remaining >= sizeof(normalized))
                die_at(Span(path, line_no, 1), "source line exceeds size limit");
            memcpy(normalized + used, p, remaining + 1);
            used += remaining;
            break;
        }
        if(*p == '"' || *p == '\'') {
            char quote = *p;
            if(used + 1 >= sizeof(normalized))
                die_at(Span(path, line_no, 1), "source line exceeds size limit");
            normalized[used++] = *p++;
            while(*p) {
                char next = *p++;
                if(used + 1 >= sizeof(normalized))
                    die_at(Span(path, line_no, 1), "source line exceeds size limit");
                normalized[used++] = next;
                if(next == '\\' && *p) {
                    if(used + 1 >= sizeof(normalized))
                        die_at(Span(path, line_no, 1), "source line exceeds size limit");
                    normalized[used++] = *p++;
                } else if(next == quote) {
                    break;
                }
            }
            continue;
        }
        const char *directive = NULL;
        size_t directive_length = 0;
        if(strncmp(p, "#filepath", 9) == 0) {
            directive = "#filepath";
            directive_length = 9;
        } else if(strncmp(p, "#file", 5) == 0) {
            directive = "#file";
            directive_length = 5;
        } else if(strncmp(p, "#line", 5) == 0) {
            directive = "#line";
            directive_length = 5;
        }
        if(directive != NULL &&
           !isalnum((unsigned char)p[directive_length]) &&
           p[directive_length] != '_') {
            char literal[ZIR_PATH_MAX * 4 + 4];
            int written;
            if(strcmp(directive, "#line") == 0)
                written = snprintf(literal, sizeof(literal), "%d", line_no);
            else {
                char directory[ZIR_PATH_MAX];
                const char *value = physical_path;
                if(strcmp(directive, "#filepath") == 0) {
                    const char *slash = strrchr(physical_path, '/');
                    size_t length = slash == NULL ? 0 :
                                    slash == physical_path ? 1 :
                                    (size_t)(slash - physical_path);
                    if(length >= sizeof(directory))
                        die_at(Span(path, line_no, 1),
                               "source filepath exceeds size limit");
                    if(length == 0)
                        copy_text(directory, sizeof(directory), ".");
                    else {
                        memcpy(directory, physical_path, length);
                        directory[length] = '\0';
                    }
                    value = directory;
                }
                literal[0] = '"';
                size_t escaped = escape_c_string(value, literal + 1,
                                                 sizeof(literal) - 2);
                literal[escaped + 1] = '"';
                literal[escaped + 2] = '\0';
                written = (int)escaped + 2;
            }
            if(written < 0 ||
               (size_t)written >= sizeof(normalized) - used)
                die_at(Span(path, line_no, 1),
                       "source line exceeds size limit");
            memcpy(normalized + used, literal, (size_t)written);
            used += (size_t)written;
            p += directive_length;
            continue;
        }
        if(isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while(isalnum((unsigned char)*p) || *p == '_') p++;
            size_t length = (size_t)(p - start);
            const char *replacement = NULL;
            if(length == 3 && strncmp(start, "nil", 3) == 0)
                die_at(Span(path, line_no, (int)(start - line) + 1),
                       "nil is not Jai syntax; use null");
            if(length == 6 && strncmp(start, "sizeof", 6) == 0)
                die_at(Span(path, line_no, (int)(start - line) + 1),
                       "sizeof is not Jai syntax; use size_of(Type)");
            for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
                if(strlen(names[i].jai) == length &&
                   strncmp(start, names[i].jai, length) == 0) {
                    replacement = names[i].internal;
                    break;
                }
            for(int i = 0; old[i]; i++)
                if(strlen(old[i]) == length &&
                   strncmp(start, old[i], length) == 0)
                    die_at(Span(path, line_no, (int)(start - line) + 1),
                           "non-Jai primitive type spelling: %s", old[i]);
            if(replacement != NULL) {
                start = replacement;
                length = strlen(replacement);
            }
            if(used + length >= sizeof(normalized))
                die_at(Span(path, line_no, 1), "source line exceeds size limit");
            memcpy(normalized + used, start, length);
            used += length;
            continue;
        }
        if(used + 1 >= sizeof(normalized))
            die_at(Span(path, line_no, 1), "source line exceeds size limit");
        normalized[used++] = *p++;
    }
    normalized[used] = '\0';
    copy_text(line, SOURCE_LINE_MAX, normalized);
}

/* File and embedded declarations share the complete frontend. A line that
 * does not fit whole is refused: silently splitting it would change meaning. */
static char *
read_source_line(char *line, size_t size, const char **source,
                 const char *path, int line_no)
{
    if(**source == '\0')
        return NULL;
    size_t length = 0;
    while(length + 1 < size && **source != '\0') {
        char next = *(*source)++;
        line[length++] = next;
        if(next == '\n')
            break;
    }
    if(**source != '\0' && length > 0 && line[length - 1] != '\n')
        die_at(Span(path, line_no + 1, 1), "source line exceeds %d characters", (int)size - 1);
    line[length] = '\0';
    return line;
}

static void parse_enum_backing(ZirType *type, const char *header);
static void canonical_enum_values(ZirType *type);
static void lower_enum_values(ZirType *type);

static int
is_named_type_header(const char *line)
{
    const char *colons = strstr(line, "::");
    if(colons == NULL) return 0;
    char name[ZIR_NAME_MAX];
    size_t length = (size_t)(colons - line);
    if(length == 0 || length >= sizeof(name)) return 0;
    memcpy(name, line, length);
    name[length] = '\0';
    trim_in_place(name);
    if(!is_identifier_text(name)) return 0;
    const char *body = skip_ws(colons + 2);
    return starts_word(body, "struct") || starts_word(body, "union") ||
           starts_word(body, "enum") || starts_word(body, "enum_flags");
}

static void
discover_named_type(const char *source, const char *path, const char *rel,
                    int line_no,
                    int scope_public, int scope_file, ZirTypes *future)
{
    char normalized[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX] = "";
    size_t normalized_length = 0;
    int physical_line = line_no;
    if(contains_source_directive(source, "#if")) return;
    for(const char *cursor = source; *cursor;) {
        const char *end = strchr(cursor, '\n');
        size_t length = end == NULL ? strlen(cursor) :
                        (size_t)(end - cursor);
        char line[SOURCE_LINE_MAX];
        if(length >= sizeof(line)) return;
        memcpy(line, cursor, length);
        line[length] = '\0';
        normalize_jai_source_tokens(line, rel, path, physical_line++);
        length = strlen(line);
        if(normalized_length + length + 2 >= sizeof(normalized)) return;
        memcpy(normalized + normalized_length, line, length);
        normalized_length += length;
        normalized[normalized_length++] = '\n';
        normalized[normalized_length] = '\0';
        cursor = end == NULL ? cursor + strlen(cursor) : end + 1;
    }
    source = normalized;
    const char *colons = strstr(source, "::");
    const char *after = colons == NULL ? NULL : skip_ws(colons + 2);
    const char *open = after == NULL ? NULL : strchr(after, '{');
    const char *close = NULL;
    int depth = 0;
    int quote = 0;
    if(open == NULL) return;
    for(const char *p = open; *p; p++) {
        if(quote) {
            if(*p == '\\' && p[1]) p++;
            else if(*p == quote) quote = 0;
            continue;
        }
        if(*p == '"' || *p == '\'') { quote = *p; continue; }
        if(*p == '{') depth++;
        if(*p == '}' && --depth == 0) { close = p; break; }
    }
    if(close == NULL) return;
    const char *tail = skip_ws(close + 1);
    if(*tail != '\0' && strcmp(tail, ";") != 0) return;
    ZirType type = {0};
    size_t name_length = (size_t)(colons - source);
    if(name_length == 0 || name_length >= sizeof(type.name)) return;
    memcpy(type.name, source, name_length);
    type.name[name_length] = '\0';
    trim_in_place(type.name);
    if(!is_identifier_text(type.name)) return;
    type.span = Span(rel, line_no, 1);
    type.is_public = scope_public;
    type.is_file_private = scope_file;
    type.is_union = starts_word(after, "union");
    type.is_enum_flags = starts_word(after, "enum_flags");
    type.is_enum = type.is_enum_flags || starts_word(after, "enum");
    if(type.is_enum)
        parse_enum_backing(&type, after);
    if(!type.is_enum &&
       !parse_type_parameters(after, type.is_union ? "union" : "struct",
                              type.template_params,
                              sizeof(type.template_params))) return;
    type.is_record_template = type.template_params[0] != '\0';
    size_t body_length = (size_t)(close - open - 1);
    if(body_length >= sizeof(type.body)) return;
    memcpy(type.body, open + 1, body_length);
    type.body[body_length] = '\0';
    if(type.is_enum) {
        if(type.is_enum_flags) {
            for(size_t i = 0; i < body_length; i++)
                if(type.body[i] == ';') type.body[i] = '\n';
            lower_enum_values(&type);
        } else {
            char lowered[sizeof(type.body)];
            size_t used = 0;
            for(const char *p = type.body; *p; p++) {
                if(p[0] == ':' && p[1] == ':') {
                    if(used + 3 >= sizeof(lowered)) return;
                    memcpy(lowered + used, " = ", 3);
                    used += 3;
                    p++;
                } else {
                    if(used + 1 >= sizeof(lowered)) return;
                    lowered[used++] = *p == ';' ? '\n' : *p;
                }
            }
            lowered[used] = '\0';
            copy_text(type.body, sizeof(type.body), lowered);
            if(!EnumMemberValue(&type, NULL, NULL)) return;
            canonical_enum_values(&type);
        }
    } else if(type.is_record_template) {
        for(size_t i = 0; i < body_length; i++)
            if(type.body[i] == ';') type.body[i] = '\n';
    }
    if(!expand_type_this(&type)) return;
    if(future->count == future->capacity) {
        int capacity = future->capacity > 0 ? future->capacity * 2 : 8;
        ZirType *items = realloc(future->items,
                                 (size_t)capacity * sizeof(*items));
        if(items == NULL) die("out of memory discovering types");
        future->items = items;
        future->capacity = capacity;
    }
    future->items[future->count++] = type;
}

static void
append_type_source(char *target, size_t capacity, const char *line,
                   int *overflow)
{
    if(*overflow) return;
    size_t length = strlen(target), addition = strlen(line);
    if(length + addition + 2 >= capacity) {
        *overflow = 1;
        return;
    }
    memcpy(target + length, line, addition);
    target[length + addition] = '\n';
    target[length + addition + 1] = '\0';
}

static void
defer_conditional_type(ZirDeferredTypes *deferred, const char *source,
                       const char *path, const char *rel, int line,
                       int is_public, int is_file_private)
{
    if(deferred->count == deferred->capacity) {
        int capacity = deferred->capacity > 0 ? deferred->capacity * 2 : 8;
        ZirDeferredType *items = realloc(deferred->items,
                                         (size_t)capacity * sizeof(*items));
        if(items == NULL) die("out of memory discovering conditional types");
        deferred->items = items;
        deferred->capacity = capacity;
    }
    ZirDeferredType *item = &deferred->items[deferred->count++];
    memset(item, 0, sizeof(*item));
    item->source = strdup(source);
    item->path = strdup(path);
    if(item->source == NULL || item->path == NULL)
        die("out of memory discovering conditional types");
    copy_text(item->rel, sizeof(item->rel), rel);
    item->line = line;
    item->is_public = is_public;
    item->is_file_private = is_file_private;
}

static void
free_deferred_types(ZirDeferredTypes *deferred)
{
    for(int i = 0; i < deferred->count; i++) {
        free(deferred->items[i].source);
        free(deferred->items[i].path);
    }
    free(deferred->items);
}

static void
discover_function_header(const char *source, const char *path,
                         const char *rel, int line_no,
                         int scope_public, int scope_file,
                         ZirFunctions *future)
{
    if(!looks_like_function_header(source) ||
       (strchr(source, '{') == NULL &&
        strstr(source, "#foreign") == NULL)) return;
    char line[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX];
    char name[ZIR_NAME_MAX], args[ZIR_TEXT_MAX], result[ZIR_NAME_MAX];
    copy_text(line, sizeof(line), source);
    normalize_jai_source_tokens(line, rel, path, line_no);
    if(closing_parenthesis(strchr(line, '(')) == NULL) return;
    parse_function_header(name, sizeof(name), args, sizeof(args),
                          result, sizeof(result), line);
    if(!is_identifier_text(name) || result[0] == '\0') return;
    if(future->count == future->capacity) {
        int capacity = future->capacity > 0 ? future->capacity * 2 : 8;
        ZirFunction *items = realloc(future->items,
                                     (size_t)capacity * sizeof(*items));
        if(items == NULL) die("out of memory discovering procedures");
        future->items = items;
        future->capacity = capacity;
    }
    ZirFunction *function = &future->items[future->count++];
    memset(function, 0, sizeof(*function));
    copy_text(function->name, sizeof(function->name), name);
    copy_text(function->args, sizeof(function->args), args);
    copy_text(function->return_type, sizeof(function->return_type), result);
    function->span = Span(rel, line_no, 1);
    function->is_public = scope_public;
    function->is_file_private = scope_file;
}

static int
discover_body_statement(ZirFunction *function, char *source, int line,
                        int block_header, int *saw_return)
{
    char *part = trim(source);
    if(*part == '\0') return !block_header;
    ZirStmtKind kind = classify_stmt(part);
    if(block_header) {
        if(kind != ZIR_STMT_IF && kind != ZIR_STMT_WHILE &&
           kind != ZIR_STMT_BLOCK_OPEN) return 0;
    } else if(kind != ZIR_STMT_DECL && kind != ZIR_STMT_ASSIGN &&
              kind != ZIR_STMT_EXPR && kind != ZIR_STMT_RETURN &&
              kind != ZIR_STMT_BREAK && kind != ZIR_STMT_CONTINUE)
        return 0;
    if(strlen(part) >= ZIR_TEXT_MAX) return 0;
    if(FunctionAddStmt(function, kind, part,
                       Span(function->span.path, line, 1)) == NULL)
        die("out of memory discovering procedure body");
    if(kind == ZIR_STMT_RETURN) *saw_return = 1;
    return 1;
}

/* Retain bounded procedure bodies for forward compile-time calls. Unsupported
 * syntax leaves only the discovered signature; the typed evaluator still
 * decides whether each executed statement is pure. */
static void
discover_compile_body(ZirFunction *function, const char *body,
                      int start_line)
{
    char *text = strdup(body);
    if(text == NULL) die("out of memory discovering procedure body");
    char *close = strrchr(text, '}');
    if(close == NULL || *skip_ws(close + 1) != '\0') goto unsupported;
    *close = '\0';
    int line = start_line, statement_line = start_line;
    int nesting = 0, blocks = 0, quote = 0, saw_return = 0;
    char *statement = text;
    for(char *cursor = text; ; cursor++) {
        char byte = *cursor;
        if(quote) {
            if(byte == '\\' && cursor[1] != '\0') { cursor++; continue; }
            if(byte == quote) quote = 0;
        } else if(byte == '"' || byte == '\'') {
            quote = byte;
        } else if(byte == '/' && cursor[1] == '/') {
            while(*cursor != '\0' && *cursor != '\n')
                *cursor++ = ' ';
            byte = *cursor;
        } else if(byte == '(' || byte == '[') {
            nesting++;
        } else if(byte == ')' || byte == ']') {
            if(--nesting < 0) goto unsupported;
        } else if(byte == '{') {
            if(nesting == 0) {
                char after = cursor[1];
                cursor[1] = '\0';
                char *header = trim(statement);
                ZirStmtKind kind = classify_stmt(header);
                int control = kind == ZIR_STMT_IF ||
                              kind == ZIR_STMT_WHILE ||
                              kind == ZIR_STMT_BLOCK_OPEN;
                if(control && (!discover_body_statement(function, header,
                                    statement_line, 1, &saw_return) ||
                                ++blocks > 32)) goto unsupported;
                cursor[1] = after;
                if(control) {
                    statement = cursor + 1;
                    statement_line = line;
                    continue;
                }
            }
            nesting++;
        } else if(byte == '}') {
            if(nesting > 0) nesting--;
            else if(blocks > 0) {
                *cursor = '\0';
                if(!discover_body_statement(function, statement,
                                            statement_line, 0, &saw_return))
                    goto unsupported;
                if(FunctionAddStmt(function, ZIR_STMT_BLOCK_CLOSE, "}",
                                   Span(function->span.path, line, 1)) == NULL)
                    die("out of memory discovering procedure body");
                blocks--;
                statement = cursor + 1;
                statement_line = line;
                continue;
            } else goto unsupported;
        }
        if(byte == '\0' || ((byte == ';' || byte == '\n') &&
                            quote == 0 && nesting == 0)) {
            *cursor = '\0';
            if(!discover_body_statement(function, statement,
                                        statement_line, 0, &saw_return))
                goto unsupported;
            statement = cursor + 1;
            statement_line = line + (byte == '\n');
        }
        if(byte == '\n') line++;
        if(byte == '\0') break;
    }
    if(quote || nesting != 0 || blocks != 0 || !saw_return)
        goto unsupported;
    free(text);
    return;
unsupported:
    free(function->stmts);
    function->stmts = NULL;
    function->stmt_count = 0;
    function->stmt_cap = 0;
    free(text);
}

static void
discover_typed_global(const char *source, const char *path,
                      const char *rel, int line_no,
                      int scope_public, int scope_file,
                      ZirGlobals *future)
{
    size_t length = strlen(source);
    if(length == 0 || source[length - 1] != ';' ||
       strchr(source, ':') == NULL || strstr(source, "::") != NULL ||
       (!isalpha((unsigned char)source[0]) && source[0] != '_'))
        return;
    char line[SOURCE_LINE_MAX];
    ZirModule parsed = {0};
    copy_text(line, sizeof(line), source);
    normalize_jai_source_tokens(line, rel, path, line_no);
    parse_file_global(&parsed, line, Span(rel, line_no, 1),
                      scope_public, scope_file, NULL, 0);
    if(parsed.global_count != 1) {
        free(parsed.globals);
        return;
    }
    if(future->count == future->capacity) {
        int capacity = future->capacity > 0 ? future->capacity * 2 : 8;
        ZirGlobal *items = realloc(future->items,
                                   (size_t)capacity * sizeof(*items));
        if(items == NULL) die("out of memory discovering globals");
        future->items = items;
        future->capacity = capacity;
    }
    future->items[future->count++] = parsed.globals[0];
    free(parsed.globals);
}

static char *read_lowered_source(const char *path);

static int
remember_discovered_file(ZirDiscoveredFiles *files, const char *path)
{
    char *canonical = realpath(path, NULL);
    if(canonical == NULL)
        canonical = strdup(path);
    if(canonical == NULL)
        die("out of memory discovering loaded files");
    for(int i = 0; i < files->count; i++)
        if(!strcmp(files->paths[i], canonical)) {
            free(canonical);
            return 0;
        }
    if(files->count == files->capacity) {
        int capacity = files->capacity > 0 ? files->capacity * 2 : 8;
        char **paths = realloc(files->paths,
                               (size_t)capacity * sizeof(*paths));
        if(paths == NULL)
            die("out of memory discovering loaded files");
        files->paths = paths;
        files->capacity = capacity;
    }
    files->paths[files->count++] = canonical;
    return 1;
}

static void
free_discovered_files(ZirDiscoveredFiles *files)
{
    for(int i = 0; i < files->count; i++)
        free(files->paths[i]);
    free(files->paths);
}

static void
free_discovered_functions(ZirFunctions *functions)
{
    for(int i = 0; i < functions->count; i++) {
        free(functions->items[i].stmts);
        free(functions->items[i].exprs);
    }
    free(functions->items);
}

static int
possible_function_header(const char *line)
{
    if(looks_like_function_header(line)) return 1;
    const char *colons = strstr(line, "::");
    if(colons == NULL) return 0;
    const char *body = skip_ws(colons + 2);
    return *body == '(' && strchr(body, ';') == NULL;
}

/* Discover only unconditional file-scope declarations. Conditional and type
 * bodies stay opaque; supported procedure bodies are retained for bounded
 * compile-time evaluation. */
static void
discover_file_scope(const char *source, const char *path, const char *rel,
                    const char *root,
                    ZirConsts *future_constants, ZirUsings *future_usings,
                    ZirImports *future_imports, ZirTypes *future_types,
                    ZirDeferredTypes *deferred_types,
                    ZirFunctions *future_functions,
                    ZirGlobals *future_globals,
                    ZirDiscoveredFiles *files, int depth)
{
    if(depth >= 32 || !remember_discovered_file(files, path)) return;
    char line[SOURCE_LINE_MAX];
    char type_source[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX] = "";
    char function_source[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX] = "";
    char simple_body[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX] = "";
    int line_no = 0;
    int comment_depth = 0;
    int brace_depth = 0;
    int scope_file = 0;
    int scope_public = 1;
    int collecting_type = 0;
    int type_overflow = 0;
    int type_start_line = 0;
    int type_scope_file = 0;
    int type_scope_public = 1;
    int collecting_function = 0;
    int function_overflow = 0;
    int function_start_line = 0;
    int function_scope_file = 0;
    int function_scope_public = 1;
    int simple_body_index = -1;
    int simple_body_overflow = 0;
    int simple_body_start_line = 0;

    while(read_source_line(line, sizeof(line), &source, path, line_no) != NULL) {
        line_no++;
        strip_block_comments(line, &comment_depth);
        char *t = trim(line);
        int brace_delta = net_block_braces(t);
        if(simple_body_index >= 0) {
            append_type_source(simple_body, sizeof(simple_body), t,
                               &simple_body_overflow);
            if(brace_depth + brace_delta == 0) {
                if(!simple_body_overflow)
                    discover_compile_body(
                        &future_functions->items[simple_body_index],
                        simple_body, simple_body_start_line);
                simple_body_index = -1;
            }
        } else if(collecting_type ||
           (brace_depth == 0 && is_named_type_header(t))) {
            if(!collecting_type) {
                collecting_type = 1;
                type_overflow = 0;
                type_source[0] = '\0';
                type_start_line = line_no;
                type_scope_file = scope_file;
                type_scope_public = scope_public;
            }
            append_type_source(type_source, sizeof(type_source), t,
                               &type_overflow);
            if(strchr(type_source, '{') != NULL &&
               brace_depth + brace_delta == 0) {
                if(!type_overflow) {
                    if(contains_source_directive(type_source, "#if"))
                        defer_conditional_type(deferred_types, type_source,
                                               path, rel, type_start_line,
                                               type_scope_public,
                                               type_scope_file);
                    else
                        discover_named_type(type_source, path, rel,
                                            type_start_line,
                                            type_scope_public, type_scope_file,
                                            future_types);
                }
                collecting_type = 0;
            }
        } else if(collecting_function ||
                  (brace_depth == 0 && possible_function_header(t))) {
            if(!collecting_function) {
                collecting_function = 1;
                function_overflow = 0;
                function_source[0] = '\0';
                function_start_line = line_no;
                function_scope_file = scope_file;
                function_scope_public = scope_public;
            }
            append_type_source(function_source, sizeof(function_source), t,
                               &function_overflow);
            const char *closing = function_overflow ? NULL :
                closing_parenthesis(strchr(function_source, '('));
            if(closing != NULL &&
               (strchr(closing + 1, '{') != NULL ||
                strstr(closing + 1, "#foreign") != NULL)) {
                int previous = future_functions->count;
                discover_function_header(function_source, path, rel,
                                         function_start_line,
                                         function_scope_public,
                                         function_scope_file,
                                         future_functions);
                const char *open = strchr(closing + 1, '{');
                if(open != NULL && future_functions->count > previous) {
                    simple_body_index = previous;
                    simple_body_overflow = 0;
                    simple_body_start_line = line_no;
                    simple_body[0] = '\0';
                    append_type_source(simple_body, sizeof(simple_body),
                                       open + 1, &simple_body_overflow);
                    if(brace_depth + brace_delta == 0) {
                        if(!simple_body_overflow)
                            discover_compile_body(
                                &future_functions->items[simple_body_index],
                                simple_body, simple_body_start_line);
                        simple_body_index = -1;
                    }
                }
                collecting_function = 0;
            } else if((closing != NULL &&
                       strchr(closing + 1, ';') != NULL) ||
                      (function_overflow && strchr(t, '{') != NULL)) {
                collecting_function = 0;
            }
        } else if(brace_depth == 0) {
            if(!strcmp(t, "#scope_file")) {
                scope_file = 1;
                scope_public = 0;
            } else if(!strcmp(t, "#scope_module") ||
                      !strcmp(t, "#scope_export")) {
                scope_file = 0;
                scope_public = !strcmp(t, "#scope_export");
            } else if(starts_word(t, "#load")) {
                const char *argument = skip_ws(t + strlen("#load"));
                const char *end = *argument == '"' ?
                    strchr(argument + 1, '"') : NULL;
                if(end != NULL && !strcmp(skip_ws(end + 1), ";")) {
                    char requested[SOURCE_PATH_MAX];
                    size_t length = (size_t)(end - argument - 1);
                    if(length > 3 && length < sizeof(requested)) {
                        memcpy(requested, argument + 1, length);
                        requested[length] = '\0';
                        if(requested[0] != '/' &&
                           !strcmp(requested + length - 3, ".zi") &&
                           strchr(requested, '\\') == NULL) {
                            char candidate[SOURCE_PATH_MAX * 2];
                            const char *slash = strrchr(path, '/');
                            int written = slash == NULL ?
                                snprintf(candidate, sizeof(candidate), "%s",
                                         requested) :
                                snprintf(candidate, sizeof(candidate),
                                         "%.*s/%s", (int)(slash - path),
                                         path, requested);
                            if(written > 0 &&
                               (size_t)written < sizeof(candidate)) {
                                char *loaded_path = realpath(candidate, NULL);
                                if(loaded_path != NULL) {
                                    char loaded_rel[SOURCE_PATH_MAX];
                                    copy_text(loaded_rel, sizeof(loaded_rel),
                                              relative_path(root, loaded_path));
                                    char *loaded = read_lowered_source(
                                        loaded_path);
                                    discover_file_scope(loaded, loaded_path,
                                        loaded_rel, root, future_constants,
                                        future_usings, future_imports,
                                        future_types, deferred_types,
                                        future_functions,
                                        future_globals,
                                        files, depth + 1);
                                    free(loaded);
                                    free(loaded_path);
                                }
                            }
                        }
                    }
                }
            } else if((starts_word(t, "using") ||
                     !strncmp(t, "using,", 6)) &&
                    strstr(t, "#import") == NULL) {
                const char *binding = skip_ws(t + 5);
                char filter[160] = "";
                parse_using_modifiers(&binding, filter, sizeof(filter),
                                      rel, line_no);
                size_t length = strlen(binding);
                if(length > 1 && binding[length - 1] == ';' &&
                   length < ZIR_NAME_MAX && strchr(binding, ':') == NULL) {
                    char name[ZIR_NAME_MAX];
                    memcpy(name, binding, length - 1);
                    name[length - 1] = '\0';
                    trim_in_place(name);
                    if(is_member_path_text(name)) {
                        if(future_usings->count == future_usings->capacity) {
                            int capacity = future_usings->capacity > 0 ?
                                future_usings->capacity * 2 : 8;
                            ZirUsing *items = realloc(future_usings->items,
                                (size_t)capacity * sizeof(*items));
                            if(items == NULL)
                                die("out of memory discovering using declarations");
                            future_usings->items = items;
                            future_usings->capacity = capacity;
                        }
                        ZirUsing *using = &future_usings->items[
                            future_usings->count++];
                        memset(using, 0, sizeof(*using));
                        copy_text(using->path, sizeof(using->path), name);
                        copy_text(using->filter, sizeof(using->filter), filter);
                        using->is_file_private = scope_file;
                        using->span = Span(rel, line_no, 1);
                    }
                }
            } else {
                discover_typed_global(t, path, rel, line_no,
                                      scope_public, scope_file,
                                      future_globals);
                const char *colons = strstr(t, "::");
                const char *declaration = colons != NULL ?
                    skip_ws(colons + 2) : t;
                if(starts_word(declaration, "#import") ||
                   !strncmp(declaration, "#import,", 8)) {
                    const char *mode = skip_ws(declaration + 7);
                    int string_import = 0;
                    if(*mode == ',')
                        string_import = starts_word(skip_ws(mode + 1),
                                                    "string");
                    if(!string_import) {
                        ZirModule discovered = {0};
                        if(parse_import_line(&discovered, rel, line_no, t,
                                             scope_public) &&
                           discovered.import_count > 0) {
                            if(future_imports->count ==
                               future_imports->capacity) {
                                int capacity = future_imports->capacity > 0 ?
                                    future_imports->capacity * 2 : 8;
                                ZirImport *items = realloc(
                                    future_imports->items,
                                    (size_t)capacity * sizeof(*items));
                                if(items == NULL)
                                    die("out of memory discovering imports");
                                future_imports->items = items;
                                future_imports->capacity = capacity;
                            }
                            ZirImport *import = &future_imports->items[
                                future_imports->count++];
                            *import = discovered.imports[0];
                            import->is_file_private = scope_file;
                        }
                        free(discovered.imports);
                    }
                }
                if(colons != NULL && !brace_outside_literals(t) &&
                   !looks_like_function_header(t)) {
                    char name[ZIR_NAME_MAX];
                    size_t name_length = (size_t)(colons - t);
                    const char *expr = skip_ws(colons + 2);
                    size_t expr_length = strlen(expr);
                    while(expr_length > 0 &&
                          isspace((unsigned char)expr[expr_length - 1]))
                        expr_length--;
                    size_t value_length = expr_length;
                    if(value_length > 0 && expr[value_length - 1] == ';')
                        value_length--;
                    if((expr[0] != '#' ||
                        starts_word(expr, "#defined")) &&
                       name_length > 0 && name_length < sizeof(name) &&
                       value_length > 0 && value_length < ZIR_TEXT_MAX) {
                        memcpy(name, t, name_length);
                        name[name_length] = '\0';
                        trim_in_place(name);
                        if(is_identifier_text(name)) {
                            if(future_constants->count ==
                               future_constants->capacity) {
                                int capacity = future_constants->capacity > 0 ?
                                    future_constants->capacity * 2 : 16;
                                ZirConst *items = realloc(future_constants->items,
                                    (size_t)capacity * sizeof(*items));
                                if(items == NULL)
                                    die("out of memory discovering constants");
                                future_constants->items = items;
                                future_constants->capacity = capacity;
                            }
                            ZirConst *constant = &future_constants->items[
                                future_constants->count++];
                            memset(constant, 0, sizeof(*constant));
                            copy_text(constant->name, sizeof(constant->name),
                                      name);
                            memcpy(constant->expr, expr, value_length);
                            constant->expr[value_length] = '\0';
                            trim_in_place(constant->expr);
                            copy_text(constant->path, sizeof(constant->path),
                                      rel);
                            constant->is_file_private = scope_file;
                            constant->source_line = line_no;
                        }
                    }
                }
            }
        }
        brace_depth += brace_delta;
        if(brace_depth < 0)
            brace_depth = 0;
    }
}

static int
discover_conditional_type(const ZirDeferredType *item,
                          ZirModule *module, const ZirConsts *constants,
                          const CompileParseContext *context,
                          ZirTypes *future, int allow_defer)
{
    const char *source = item->source;
    char line[SOURCE_LINE_MAX];
    char selected[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX] = "";
    ZirCondFrame frames[8] = {0};
    int frame_count = 0;
    int line_no = item->line - 1;
    int overflow = 0;
    while(read_source_line(line, sizeof(line), &source,
                           item->path, line_no) != NULL) {
        int pending = 0;
        line_no++;
        char *text = trim(line);
        int consumed = cond_top_step(text, frames, &frame_count,
                                     module, constants, context,
                                     item->path, item->rel, line_no,
                                     allow_defer ? &pending : NULL);
        if(pending) return 0;
        if(consumed || (frame_count > 0 && !frames[frame_count - 1].active))
            continue;
        append_type_source(selected, sizeof(selected), text, &overflow);
    }
    if(!overflow && frame_count == 0)
        discover_named_type(selected, item->path, item->rel,
                            item->line, item->is_public,
                            item->is_file_private, future);
    return 1;
}

static void
discover_conditional_types(const ZirDeferredTypes *deferred,
                           ZirModule *module, const ZirConsts *constants,
                           const CompileParseContext *context,
                           ZirTypes *future)
{
    unsigned char *done = calloc((size_t)deferred->count + 1, 1);
    if(done == NULL) die("out of memory discovering conditional types");
    int remaining = deferred->count;
    while(remaining > 0) {
        int progress = 0;
        for(int i = 0; i < deferred->count; i++) {
            if(done[i]) continue;
            if(discover_conditional_type(&deferred->items[i], module,
                    constants, context, future, 1)) {
                done[i] = 1;
                remaining--;
                progress++;
            }
        }
        if(progress > 0) continue;
        for(int i = 0; i < deferred->count; i++)
            if(!done[i]) {
                discover_conditional_type(&deferred->items[i], module,
                                          constants, context, future, 0);
                done[i] = 1;
                remaining--;
                break;
            }
    }
    free(done);
}

/* Jai constant declarations inside enums use 'Member :: value'. The checked
 * IR stores explicit values in the backend-neutral 'Member = value' form. */
static void
canonical_enum_values(ZirType *type)
{
    char canonical[sizeof(type->body)];
    size_t used = 0;
    const char *cursor = type->body;
    if(!EnumMemberValue(type, NULL, NULL))
        die_at(type->span, "invalid enum value or value outside backing type: %s",
               type->name);
    while(*cursor) {
        while(*cursor == ',' || isspace((unsigned char)*cursor)) cursor++;
        if(!*cursor) break;
        const char *start = cursor;
        while(isalnum((unsigned char)*cursor) || *cursor == '_') cursor++;
        char name[ZIR_NAME_MAX];
        int64_t value;
        size_t length = (size_t)(cursor - start);
        if(length == 0 || length >= sizeof(name))
            die_at(type->span, "invalid enum member");
        snprintf(name, sizeof(name), "%.*s", (int)length, start);
        if(!EnumMemberValue(type, name, &value))
            die_at(type->span, "invalid enum member: %s", name);
        int written = snprintf(canonical + used, sizeof(canonical) - used,
                               "%s = %lld\n", name, (long long)value);
        if(written < 0 || (size_t)written >= sizeof(canonical) - used)
            die_at(type->span, "enum body exceeds size limit");
        used += (size_t)written;
        while(*cursor && *cursor != ',' && *cursor != '\n') cursor++;
        if(*cursor) cursor++;
    }
    canonical[used] = '\0';
    copy_text(type->body, sizeof(type->body), canonical);
}

static void
lower_enum_values(ZirType *type)
{
    char lowered[sizeof(type->body)];
    size_t used = 0;
    for(const char *p = type->body; *p; p++) {
        if(type->is_enum && *p == '=')
            die_at(type->span,
                   "enum members use Jai Member :: value syntax, not =");
        if(p[0] == ':' && p[1] == ':') {
            if(used + 3 >= sizeof(lowered))
                die_at(type->span, "enum body exceeds size limit");
            memcpy(lowered + used, " = ", 3);
            used += 3;
            p++;
        } else {
            if(used + 1 >= sizeof(lowered))
                die_at(type->span, "enum body exceeds size limit");
            lowered[used++] = *p;
        }
    }
    lowered[used] = '\0';
    if(type->is_enum_specified) {
        for(const char *p = lowered; *p;) {
            const char *end = p;
            while(*end && *end != '\n' && *end != ',') end++;
            const char *member = p;
            while(member < end && isspace((unsigned char)*member)) member++;
            if(member < end) {
                const char *name = member;
                while(member < end &&
                      (isalnum((unsigned char)*member) || *member == '_'))
                    member++;
                while(member < end && isspace((unsigned char)*member)) member++;
                if(member == name || member == end || *member != '=')
                    die_at(type->span,
                           "#specified enum requires an explicit value for every member");
            }
            p = *end ? end + 1 : end;
        }
    }
    if(!type->is_enum_flags) {
        copy_text(type->body, sizeof(type->body), lowered);
        if(type->is_enum)
            canonical_enum_values(type);
        return;
    }
    char flags[sizeof(type->body)];
    char previous[ZIR_NAME_MAX] = "";
    size_t emitted = 0;
    for(char *p = lowered; *p;) {
        char member[ZIR_TEXT_MAX];
        char name[ZIR_NAME_MAX];
        char *end = p;
        while(*end && *end != '\n' && *end != ',') end++;
        if((size_t)(end - p) >= sizeof(member))
            die_at(type->span, "enum_flags member exceeds size limit");
        snprintf(member, sizeof(member), "%.*s", (int)(end - p), p);
        trim_in_place(member);
        p = *end ? end + 1 : end;
        if(member[0] == '\0') continue;
        size_t n = 0;
        while(isalnum((unsigned char)member[n]) || member[n] == '_') n++;
        if(n == 0 || n >= sizeof(name) ||
           !(isalpha((unsigned char)member[0]) || member[0] == '_'))
            die_at(type->span, "invalid enum_flags member: %s", member);
        snprintf(name, sizeof(name), "%.*s", (int)n, member);
        const char *rest = skip_ws(member + n);
        int written;
        if(*rest == '=')
            written = snprintf(flags + emitted, sizeof(flags) - emitted,
                               "%s\n", member);
        else if(*rest == '\0' && previous[0])
            written = snprintf(flags + emitted, sizeof(flags) - emitted,
                               "%s = %s << 1\n", name, previous);
        else if(*rest == '\0')
            written = snprintf(flags + emitted, sizeof(flags) - emitted,
                               "%s = 1\n", name);
        else
            die_at(type->span, "invalid enum_flags member: %s", member);
        if(written < 0 || (size_t)written >= sizeof(flags) - emitted)
            die_at(type->span, "enum_flags body exceeds size limit");
        emitted += (size_t)written;
        copy_text(previous, sizeof(previous), name);
    }
    flags[emitted] = '\0';
    copy_text(type->body, sizeof(type->body), flags);
    canonical_enum_values(type);
}

static void
parse_enum_backing(ZirType *type, const char *header)
{
    const char *start = skip_ws(header + (type->is_enum_flags ? 10 : 4));
    const char *end = strchr(start, '{');
    char backing[ZIR_NAME_MAX];
    if(end == NULL) end = start + strlen(start);
    if((size_t)(end - start) >= sizeof(backing))
        die_at(type->span, "enum backing type exceeds size limit");
    snprintf(backing, sizeof(backing), "%.*s", (int)(end - start), start);
    trim_in_place(backing);
    char *directive = strstr(backing, "#specified");
    if(directive != NULL) {
        char *after = directive + strlen("#specified");
        if(*skip_ws(after) != '\0' ||
           (directive > backing && !isspace((unsigned char)directive[-1])))
            die_at(type->span, "invalid #specified enum modifier");
        *directive = '\0';
        trim_in_place(backing);
        type->is_enum_specified = 1;
    }
    if(backing[0] == '\0')
        copy_text(backing, sizeof(backing), "s64");
    if(strcmp(backing, "s8") && strcmp(backing, "u8") &&
       strcmp(backing, "s16") && strcmp(backing, "u16") &&
       strcmp(backing, "s32") && strcmp(backing, "u32") &&
       strcmp(backing, "s64") && strcmp(backing, "u64"))
        die_at(type->span, "enum requires an integer backing type: %s",
               backing);
    copy_text(type->enum_backing, sizeof(type->enum_backing), backing);
}

int
SubstituteGenericType(const char *source, char *output, size_t capacity,
                      char params[][ZIR_NAME_MAX],
                      char actual[][ZIR_NAME_MAX], int count)
{
    size_t used = 0;
    while(*source) {
        const char *replacement = NULL;
        size_t length = 1;
        if(isalpha((unsigned char)*source) || *source == '_') {
            while(isalnum((unsigned char)source[length]) ||
                  source[length] == '_')
                length++;
            for(int i = 0; i < count; i++)
                if(strlen(params[i]) == length &&
                   strncmp(source, params[i], length) == 0) {
                    replacement = actual[i];
                    break;
                }
        }
        if(replacement != NULL) {
            size_t replacement_length = strlen(replacement);
            if(used + replacement_length >= capacity)
                return 0;
            memcpy(output + used, replacement, replacement_length);
            used += replacement_length;
        } else {
            if(used + length >= capacity)
                return 0;
            memcpy(output + used, source, length);
            used += length;
        }
        source += length;
    }
    output[used] = '\0';
    return 1;
}

int
InstantiateGenericRecord(ZirType *instance, const ZirType *generic)
{
    char params[16][ZIR_NAME_MAX], actual[16][ZIR_NAME_MAX];
    int parameter_count, argument_count;
    if(!instance->is_type_instance || !generic->is_record_template)
        return 0;
    parameter_count = split_top_level(generic->template_params, params[0],
                                      16, sizeof(params[0]));
    argument_count = split_top_level(instance->template_args, actual[0],
                                     16, sizeof(actual[0]));
    if(parameter_count < 1 || parameter_count != argument_count)
        return 0;
    for(int i = 0; i < parameter_count; i++) {
        if(!is_identifier_text(params[i]) || !actual[i][0])
            return 0;
        for(int previous = 0; previous < i; previous++)
            if(!strcmp(params[previous], params[i]))
                return 0;
    }
    size_t offset = 0, used = 0;
    int members = 0, status;
    ZirTypeField field;
    while((status = TypeNextField(generic, &offset, &field)) == 1) {
        char type[ZIR_NAME_MAX];
        if(!SubstituteGenericType(field.type, type, sizeof(type),
                                   params, actual, parameter_count))
            return 0;
        int length = snprintf(instance->body + used,
            sizeof(instance->body) - used, "%s%s: %s\n",
            field.is_using ? "using " : "", field.name, type);
        if(length < 0 || (size_t)length >= sizeof(instance->body) - used)
            return 0;
        used += (size_t)length;
        members++;
    }
    if(status < 0 || members == 0)
        return 0;
    instance->is_union = generic->is_union;
    if(!strcmp(generic->name, "Vec") && members == 3) {
        size_t position = 0;
        ZirTypeField part;
        if(TypeNextField(generic, &position, &part) == 1 &&
           !strcmp(part.name, "data") && !strcmp(part.type, "*T") &&
           TypeNextField(generic, &position, &part) == 1 &&
           !strcmp(part.name, "count") &&
           !strcmp(part.type, "s64") &&
           TypeNextField(generic, &position, &part) == 1 &&
           !strcmp(part.name, "capacity") &&
           !strcmp(part.type, "s64"))
            instance->is_owned_vec = 1;
    }
    instance->is_type_instance = 0;
    instance->template_name[0] = '\0';
    instance->template_args[0] = '\0';
    return 1;
}

static char *
read_lowered_source(const char *path)
{
    FILE *in = fopen(path, "rb");
    if(in == NULL)
        die_at(Span(path, 0, 0), "open failed: %s", strerror(errno));
    SourceBuffer input = {0};
    unsigned char bytes[8192];
    size_t count;
    while((count = fread(bytes, 1, sizeof(bytes), in)) != 0)
        for(size_t i = 0; i < count; i++) {
            if(bytes[i] == 0)
                die_at(Span(path, 0, 0), "source contains a null byte");
            source_append(&input, (char)bytes[i]);
        }
    if(ferror(in))
        die_at(Span(path, 0, 0), "read failed: %s", strerror(errno));
    fclose(in);
    if(input.text == NULL) {
        input.text = malloc(1);
        if(input.text == NULL)
            die("out of memory reading source");
        input.text[0] = '\0';
    }
    char *lowered = lower_jai_multiline_strings(input.text, path);
    free(input.text);
    return lowered;
}

/* Decode the literal source of Jai's #import, string form. A raw #string
 * body has already been lowered to this same quoted spelling. */
static char *
import_string_source(const char *line, ZirSourceSpan span)
{
    const char *directive = skip_ws(line);
    if(!starts_word(directive, "#import") &&
       strncmp(directive, "#import,", 8) != 0) {
        char alias[ZIR_NAME_MAX];
        if(!parse_symbol_before_colons(directive, alias, sizeof(alias)))
            return NULL;
        directive = skip_ws(strstr(directive, "::") + 2);
        if(!starts_word(directive, "#import") &&
           strncmp(directive, "#import,", 8) != 0)
            return NULL;
    }
    const char *mode = skip_ws(directive + strlen("#import"));
    if(*mode != ',')
        return NULL;
    mode = skip_ws(mode + 1);
    if(strncmp(mode, "string", 6) != 0 ||
       !isspace((unsigned char)mode[6]))
        return NULL;
    const char *cursor = skip_ws(mode + 6);
    if(*cursor++ != '"')
        die_at(span, "#import, string requires a source string");
    SourceBuffer output = {0};
    int closed = 0;
    while(*cursor) {
        unsigned char byte = (unsigned char)*cursor++;
        if(byte == '"') {
            closed = 1;
            break;
        }
        if(byte == '\\') {
            byte = (unsigned char)*cursor++;
            if(byte == '\0')
                die_at(span, "unterminated #import, string literal");
            if(byte == 'n') byte = '\n';
            else if(byte == 'r') byte = '\r';
            else if(byte == 't') byte = '\t';
            else if(byte == 'a') byte = '\a';
            else if(byte == 'b') byte = '\b';
            else if(byte == 'f') byte = '\f';
            else if(byte == 'v') byte = '\v';
            else if(byte == 'x') {
                int high = isxdigit((unsigned char)cursor[0]) ?
                    (isdigit((unsigned char)cursor[0]) ? cursor[0] - '0' :
                     tolower((unsigned char)cursor[0]) - 'a' + 10) : -1;
                int low = high >= 0 && isxdigit((unsigned char)cursor[1]) ?
                    (isdigit((unsigned char)cursor[1]) ? cursor[1] - '0' :
                     tolower((unsigned char)cursor[1]) - 'a' + 10) : -1;
                if(low < 0)
                    die_at(span, "#import, string needs two hex digits after \\x");
                byte = (unsigned char)(high * 16 + low);
                cursor += 2;
            } else if(byte != '"' && byte != '\\')
                die_at(span, "unsupported #import, string escape");
        }
        if(byte == 0)
            die_at(span, "#import, string source cannot contain a null byte");
        source_append(&output, (char)byte);
    }
    if(!closed || strcmp(skip_ws(cursor), ";") != 0)
        die_at(span, "#import, string requires a closing quote and ';'");
    if(output.text == NULL) {
        output.text = malloc(1);
        if(output.text == NULL)
            die("out of memory reading imported string");
        output.text[0] = '\0';
    }
    return output.text;
}

typedef struct LoadFrame {
    const char *path;
    const char *source;
    char *canonical;
    char *owned_source;
    char rel[SOURCE_PATH_MAX];
    char lookahead[SOURCE_LINE_MAX];
    int line_no;
    int physical_line_no;
    int have_look;
    int scope_public;
    int scope_file;
    int in_block_comment;
    int conditional_depth;
} LoadFrame;

static ZirProgram *
parse_source(const char *path, const char *root, const char *source,
             ZirCompileImportResolver resolver, void *resolver_context,
             const char *const *defines, int define_count)
{
    ZirProgram *program;
    CompileParseContext compile_context = {0};
    ZirModule *module;
    ZirFunction *fn = NULL;
    char line[SOURCE_LINE_MAX];
    char module_name[ZIR_NAME_MAX];
    char rel[SOURCE_PATH_MAX];
    int line_no = 0;
    int physical_line_no = 0;
    enum { TOP, TYPE, FUNCTION } mode = TOP;
    int depth = 0;
    char pending[SOURCE_LINE_MAX * 4];
    /* Compile-time imports recurse into parse_source while this line is in
     * use. Keep each invocation's logical source separate. */
    char logical[SOURCE_LINE_MAX * 4];
    pending[0] = '\0';
    char lookahead[SOURCE_LINE_MAX];
    int have_look = 0;
    /* One-line control blocks ('if cond { body }') are split into header /
     * body / '}' logical lines; the body and closer re-enter the main loop
     * through this FIFO so they flow through the normal join machinery. */
    char onelineq[16][SOURCE_LINE_MAX * 2];
    int onelineq_count = 0;
    int from_queue = 0;
    int pending_len = 0;
    int pending_start_line = 1;
    int pending_start_column = 1;
    int pending_end_column = 1;
    int paren_depth = 0;
    int bracket_depth = 0;
    int in_string = 0;
    int expr_brace = 0;
    ZirCondFrame tframes[8];
    int tframe_count = 0;
    ZirCondFrame type_frames[8];
    int type_frame_count = 0;
    ZirConsts consts;
    ZirConsts future_constants = {0};
    ZirUsings future_usings = {0};
    ZirImports future_imports = {0};
    ZirTypes future_types = {0};
    ZirDeferredTypes deferred_types = {0};
    ZirFunctions future_functions = {0};
    ZirGlobals future_globals = {0};
    ZirDiscoveredFiles discovered_files = {0};
    int body_mdepth[8];
    int body_mselected[8];
    int body_mactive[8];
    int body_mparent_active[8];
    int body_mcount = 0;
    int in_block_comment = 0;
    int program_export = 0;
    int program_export_line = 0;
    char program_export_symbol[ZIR_NAME_MAX] = "";
    int scope_public = 1;
    int scope_file = 0;
    char foreign_library_names[32][ZIR_NAME_MAX];
    char foreign_library_targets[32][ZIR_PATH_MAX];
    char foreign_library_paths[32][SOURCE_PATH_MAX];
    int foreign_library_file_private[32] = {0};
    int foreign_library_count = 0;
    LoadFrame load_frames[32];
    int load_depth = 0;
    char *canonical = realpath(path, NULL);
    char *owned_source = NULL;
    if(canonical == NULL)
        canonical = strdup(path);
    if(canonical == NULL)
        die("out of memory tracking source path");
    memset(&consts, 0, sizeof(consts));
    if(define_count > 0) {
        consts.items = calloc((size_t)define_count, sizeof(*consts.items));
        if(consts.items == NULL)
            die("out of memory recording compiler definitions");
        consts.capacity = define_count;
        consts.count = define_count;
        for(int i = 0; i < define_count; i++) {
            copy_text(consts.items[i].name,
                      sizeof(consts.items[i].name), defines[i]);
            copy_text(consts.items[i].expr,
                      sizeof(consts.items[i].expr), "1");
            consts.items[i].is_public = 1;
        }
    }
    snprintf(rel, sizeof(rel), "%s", relative_path(root, path));
    const char *basename = strrchr(path, '/');
    basename = basename != NULL ? basename + 1 : path;
    size_t stem_length = strlen(basename);
    if(stem_length > 3 && strcmp(basename + stem_length - 3, ".zi") == 0)
        stem_length -= 3;
    if(stem_length == 0 || stem_length >= sizeof(module_name))
        die_at(Span(rel, 1, 1), "source filename cannot be used as a module name");
    memcpy(module_name, basename, stem_length);
    module_name[stem_length] = '\0';
    program = ProgramNew();
    if(program == NULL)
        die("out of memory");
    compile_context.program = program;
    compile_context.root = root;
    compile_context.resolver = resolver;
    compile_context.resolver_context = resolver_context;
    compile_context.future_constants = &future_constants;
    compile_context.future_usings = &future_usings;
    compile_context.future_imports = &future_imports;
    compile_context.future_types = &future_types;
    compile_context.future_functions = &future_functions;
    compile_context.future_globals = &future_globals;

    module = ProgramAddModule(program, module_name, rel, Span(rel, 1, 1));
    if(module == NULL)
        die("out of memory");
    if(root != NULL) {
        char *canonical_root = realpath(root, NULL);
        copy_text(module->source_root, sizeof(module->source_root),
                  canonical_root != NULL ? canonical_root : root);
        free(canonical_root);
    }
    discover_file_scope(source, path, rel, root, &future_constants,
                        &future_usings, &future_imports, &future_types,
                        &deferred_types,
                        &future_functions, &future_globals,
                        &discovered_files, 0);
    discover_conditional_types(&deferred_types, module, &consts,
                               &compile_context, &future_types);

    for(;;) {
        if(!have_look && onelineq_count == 0 &&
           read_source_line(line, sizeof(line), &source, path, line_no) == NULL) {
            if(in_block_comment)
                die_at(Span(rel, line_no, 1), "unterminated block comment");
            if(pending_len != 0 || paren_depth != 0 || bracket_depth != 0 ||
               in_string || expr_brace != 0)
                die_at(Span(rel, pending_start_line, pending_start_column),
                       "unterminated source declaration or expression");
            if(load_depth == 0)
                break;
            LoadFrame *frame = &load_frames[--load_depth];
            if(mode != TOP || tframe_count != frame->conditional_depth ||
               type_frame_count != 0 ||
               body_mcount != 0 || program_export)
                die_at(Span(rel, line_no, 1),
                       "unterminated declaration or directive in loaded file");
            free(owned_source);
            free(canonical);
            path = frame->path;
            source = frame->source;
            canonical = frame->canonical;
            owned_source = frame->owned_source;
            copy_text(rel, sizeof(rel), frame->rel);
            copy_text(lookahead, sizeof(lookahead), frame->lookahead);
            line_no = frame->line_no;
            physical_line_no = frame->physical_line_no;
            have_look = frame->have_look;
            scope_public = frame->scope_public;
            scope_file = frame->scope_file;
            in_block_comment = frame->in_block_comment;
            continue;
        }
        if(!have_look && onelineq_count == 0)
            physical_line_no++;
        char raw[SOURCE_LINE_MAX];
        char *t;
        int from_lookahead = 0;
        copy_text(module->lookup_path, sizeof(module->lookup_path), rel);

        /* Queued one-liner parts outrank the stashed lookahead: they belong
         * before the next source line, and have_look persists until the
         * queue drains. */
        from_queue = onelineq_count > 0;
        if(onelineq_count > 0) {
            copy_text(line, sizeof(line), onelineq[0]);
            memmove(onelineq[0], onelineq[1],
                    sizeof(onelineq[0]) * (size_t)(onelineq_count - 1));
            onelineq_count--;
        } else if(have_look) {
            snprintf(line, sizeof(line), "%s", lookahead);
            have_look = 0;
            from_lookahead = 1;
        }

        if(!from_queue)
            line_no++;
        if(!from_queue && !from_lookahead)
            strip_block_comments(line, &in_block_comment);
        if(contains_source_directive(line, "#else_if") ||
           contains_source_directive(line, "#else"))
            die_at(Span(rel, line_no, 1),
                   "Jai compile-time branches use 'else #if' or 'else', not #else_if/#else");
        int compile_condition_line = line_starts_compile_condition(line);
        int active_source =
            (tframe_count == 0 || tframes[tframe_count - 1].active) &&
            (type_frame_count == 0 ||
             type_frames[type_frame_count - 1].active) &&
            (body_mcount == 0 || body_mactive[body_mcount - 1]);
        if(!from_queue && active_source && !compile_condition_line)
            normalize_jai_source_tokens(line, rel, canonical,
                                        physical_line_no);
        snprintf(raw, sizeof(raw), "%s", line);
        {
            char *trimmed = trim(raw);
            int trimmed_column = from_queue ? 1 :
                                 source_column_for_trimmed(raw, trimmed);

            if(trimmed[0] == '\0' || strncmp(trimmed, "//", 2) == 0) {
                if(pending_len == 0)
                    continue;
                continue;
            }
            if(pending_len == 0) {
                pending_start_line = line_no;
                pending_start_column = trimmed_column;
            }
            pending_end_column = source_end_column_for_trimmed(raw, trimmed);
            if(pending_len > 0 && pending_len + 2 < (int)sizeof(pending)) {
                pending[pending_len++] = '\n';
                pending[pending_len] = '\0';
            }
            strncat(pending, trimmed, sizeof(pending) - pending_len - 1);
            pending_len = (int)strlen(pending);
            pending_len = (int)strlen(pending);
            /* Decide whether braces at paren-depth 0 on this logical line are
             * block braces (control/headers open scopes) or expression braces
             * (compound literals / initializers continue the statement). */
            {
                int header_line = 0;
                char w0[16];
                size_t wl = 0;

                for(const char *w = pending;
                    *w != '\0' && (isalnum((unsigned char)*w) || *w == '_') &&
                    wl + 1 < sizeof(w0); w++)
                    w0[wl++] = *w;
                w0[wl] = '\0';
                /* Keyword headers must be followed by ' ', '(' or '{':
                 * 'item->x = ...' / 'state.x' are member statements, not
                 * block headers (their compound-literal braces are
                 * expression braces). */
                {
                    char nc = pending[wl];
                    char block_callee[ZIR_NAME_MAX];
                    char block_name[ZIR_NAME_MAX];
                    header_line =
                        pending[0] == '#' ||
                        strcmp(pending, "{") == 0 ||   /* bare scope-open */
                        parse_block_call_header(pending, block_callee,
                                                sizeof(block_callee), block_name,
                                                sizeof(block_name)) ||
                        /* 'name :: Type = {' carries an initializer, not a
                         * body: its braces are expression braces so the
                         * logical line continues until they balance. Header
                         * forms ('name :: struct {', 'f :: (args) {', typedefs,
                         * externs) never contain ' = '. */
                        looks_like_function_header(pending) ||
                        (strstr(pending, " :: ") != NULL &&
                         strstr(pending, " = ") == NULL) ||
                        (nc != '\0' && nc != '-' && nc != '.' &&
                         (strchr(" ({", nc) != NULL || nc == ':') &&
                         (strcmp(w0, "if") == 0 ||
                          strcmp(w0, "else") == 0 ||
                          strcmp(w0, "while") == 0 ||
                          strcmp(w0, "for") == 0 ||
                          strcmp(w0, "defer") == 0 ||
                          strcmp(w0, "switch") == 0 ||
                          strcmp(w0, "match") == 0 ||
                          strcmp(w0, "do") == 0 ||
                          strcmp(w0, "case") == 0 ||
                          strcmp(w0, "struct") == 0 ||
                          strcmp(w0, "enum") == 0 ||
                          strcmp(w0, "state") == 0));
                }
                /* K&R "} else {" / "} else if (...) {" also covers Jai's
                 * "} else #if COND {" compile-time branch. The leading and
                 * trailing braces balance as one logical header line. */
                if(!header_line && pending[0] == '}') {
                    const char *eq = pending + 1;

                    while(*eq == ' ' || *eq == '\t')
                        eq++;
                    if(starts_word(eq, "else"))
                        header_line = 1;
                }
                {
                int in_chr = 0;

                for(const char *p = trimmed; *p != '\0'; p++) {
                    if(in_string) {
                        if(*p == '\\' && p[1] != '\0')
                            p++;
                        else if(*p == '"')
                            in_string = 0;
                    } else if(in_chr) {
                        if(*p == '\\' && p[1] != '\0')
                            p++;
                        else if(*p == '\'')
                            in_chr = 0;
                    } else if(*p == '"') {
                        in_string = 1;
                    } else if(*p == '\'') {
                        in_chr = 1;
                    } else if(*p == '(') {
                        paren_depth++;
                    } else if(*p == ')') {
                        paren_depth--;
                    } else if(*p == '[') {
                        bracket_depth++;
                    } else if(*p == ']') {
                        bracket_depth--;
                    } else if(paren_depth == 0 && bracket_depth == 0) {
                        if(*p == '{') {
                            if(!header_line)
                                expr_brace++;
                        } else if(*p == '}') {
                            if(!header_line && expr_brace > 0)
                                expr_brace--;
                        }
                    }
                }
                }
            }
            if(paren_depth > 0 || bracket_depth > 0 || in_string ||
               expr_brace > 0)
                continue;
            /* Continuation: a line ending in a binary operator or comma
             * continues onto the next. Exclude ++/-- (they end statements). */
            {
                size_t pl = (size_t)pending_len;
                char last;
                char prev;

                while(pl > 0 && (pending[pl - 1] == ' ' ||
                                 pending[pl - 1] == '\t'))
                    pl--;
                last = pl > 0 ? pending[pl - 1] : '\0';
                prev = pl > 1 ? pending[pl - 2] : '\0';
                /* Continue after a binary operator or comma. The previous
                 * character distinguishes an operator from a postfix form. */
                if(last == ',' || last == '=' || last == '%' ||
                   (last == '/' && prev != '>'))
                    continue;
                if((last == '+' || last == '-' || last == '*' ||
                    last == '<' || last == '>') &&
                   (prev == ' ' || prev == '\t') &&
                   !(prev == last))
                    continue;
                if((last == '&' || last == '|') &&
                   (prev == last || prev == ' ' || prev == '\t'))
                    continue;
                /* Look ahead for Jai continuation tokens. Skipped for queued
                 * one-liner parts and while a stash is pending: their "next
                 * line" is not the next physical source line. */
                if(!from_queue && !have_look) {
                    char la[SOURCE_LINE_MAX];

                    /* Keep consuming lookahead lines while they continue this
                     * statement; the first non-continuation line is stashed
                     * for the next iteration (appending it blindly here is
                     * how block-closing '}'s used to get swallowed). */
                    while(read_source_line(la, sizeof(la), &source, path, line_no) != NULL) {
                        const char *lt;
                        int cont;

                        physical_line_no++;
                        strip_block_comments(la, &in_block_comment);
                        if(contains_source_directive(la, "#else_if") ||
                           contains_source_directive(la, "#else"))
                            die_at(Span(rel, line_no + 1, 1),
                                   "Jai compile-time branches use 'else #if' or 'else', not #else_if/#else");
                        /* Decide continuation from raw source. A stashed line
                         * may become inactive after this line selects #if;
                         * normalize it only when the parser consumes it. */
                        lt = trim(la);
                        if(lt[0] == '\0' || strncmp(lt, "//", 2) == 0) {
                            line_no++;   /* comments and blank lines still count */
                            continue;
                        }
                        cont =
                            (last == '}' && starts_word(lt, "else")) ||
                            *lt == '.' || *lt == ',' || *lt == '+' ||
                            *lt == '/' || *lt == '%' ||
                            (*lt == '-' && lt[1] != '>') ||
                            ((*lt == '&' && lt[1] == '&') ||
                             (*lt == '|' && lt[1] == '|') ||
                             (*lt == '=' && lt[1] == '=') ||
                             (*lt == '!' && lt[1] == '=') ||
                             (*lt == '<' && lt[1] == '=') ||
                             (*lt == '>' && lt[1] == '='));
                        if(!cont) {
                            snprintf(lookahead, sizeof(lookahead), "%s", la);
                            have_look = 1;
                            break;
                        }
                        if(active_source && !compile_condition_line)
                            normalize_jai_source_tokens(la, rel, canonical,
                                                        physical_line_no);
                        lt = trim(la);
                        pending_end_column = source_end_column_for_trimmed(la, lt);
                        if(pending_len > 0 &&
                           pending_len + 2 < (int)sizeof(pending)) {
                            pending[pending_len++] =
                                last == '}' && starts_word(lt, "else") ?
                                ' ' : '\n';
                            pending[pending_len] = '\0';
                        }
                        strncat(pending, lt,
                                sizeof(pending) - pending_len - 1);
                        pending_len = (int)strlen(pending);
                        line_no++;
                    }
                }
            }
        }
        t = pending;
        int export_directive = 0;
        char export_directive_symbol[ZIR_NAME_MAX] = "";
        {
            snprintf(logical, sizeof(logical), "%s", pending);
            t = logical;
            char *separator = statement_separator(logical);
            if(separator != NULL) {
                prepend_logical_line(onelineq, &onelineq_count,
                                     skip_ws(separator + 1),
                                     Span(rel, line_no, 1));
                char *before = separator;
                while(before > logical && isspace((unsigned char)before[-1]))
                    before--;
                if(before > logical && before[-1] == '}')
                    *before = '\0'; /* A block ends at its closing brace. */
                else
                    separator[1] = '\0'; /* Keep declaration terminators. */
                trim_in_place(logical);
            }
            if(mode == TOP) {
                export_directive = strip_program_export(logical,
                    export_directive_symbol,
                    sizeof(export_directive_symbol),
                    Span(rel, line_no, 1));
                t = logical;
            }
            /* One-line control block: keep the header as this logical line
             * and queue the body + closer for the next iterations (nested
             * one-liners split again when their body is finalized). */
            if(mode == TOP) {
                char head[SOURCE_LINE_MAX * 2];
                char body[SOURCE_LINE_MAX * 2];

                if(split_oneline_function(t, head, sizeof(head),
                                          body, sizeof(body))) {
                    prepend_logical_line(onelineq, &onelineq_count, "}",
                                         Span(rel, line_no, 1));
                    if(body[0])
                        prepend_logical_line(onelineq, &onelineq_count,
                                             body, Span(rel, line_no, 1));
                    snprintf(logical, sizeof(logical), "%s", head);
                    t = logical;
                }
            } else {
                char head[SOURCE_LINE_MAX * 2];
                char body[SOURCE_LINE_MAX * 2];
                char tail[SOURCE_LINE_MAX * 2];

                if(split_oneline_block(t, head, sizeof(head),
                                       body, sizeof(body),
                                       tail, sizeof(tail))) {
                    if(tail[0])
                        prepend_logical_line(onelineq, &onelineq_count,
                                             tail, Span(rel, line_no, 1));
                    prepend_logical_line(onelineq, &onelineq_count, "}",
                                         Span(rel, line_no, 1));
                    if(body[0])
                        prepend_logical_line(onelineq, &onelineq_count,
                                             body, Span(rel, line_no, 1));
                    snprintf(logical, sizeof(logical), "%s", head);
                    t = logical;
                }
            }
            if(mode == FUNCTION) {
                split_jai_control_line(logical, sizeof(logical), onelineq,
                                       &onelineq_count,
                                       Span(rel, line_no, 1));
                t = logical;
            }
        }
        pending[0] = '\0';
        pending_len = 0;
        if(*t == '\0') continue;
        char *string_source = NULL;
        if(mode != FUNCTION &&
           contains_source_directive(t, "#procedure_name") &&
           !(mode == TOP && looks_like_function_header(t)))
            die_at(Span(rel, line_no, 1),
                   "#procedure_name() requires a procedure scope");
        if(mode != FUNCTION &&
           contains_source_directive(t, "#caller_location") &&
           !(mode == TOP && looks_like_function_header(t)))
            die_at(Span(rel, line_no, 1),
                   "#caller_location is only valid as a parameter default");
        if(contains_source_directive(t, "#slot"))
            die_at(Span(rel, line_no, 1),
                   "#slot is not Jai syntax; use a procedure type and a named function");
        if(mode == TOP && contains_source_directive(t, "#private"))
            die_at(Span(rel, line_no, 1),
                   "#private is not Jai syntax; use #scope_file");
        if(mode == TOP && looks_like_function_header(t)) {
            const char *parameters = strchr(t, '(');
            const char *closing = closing_parenthesis(parameters);
            const char *body_open = closing == NULL ? NULL :
                                    strchr(closing + 1, '{');
            for(const char *modifier = strchr(closing == NULL ? t :
                                              closing + 1, '#');
                modifier != NULL &&
                (body_open == NULL || modifier < body_open);
                modifier = strchr(modifier + 1, '#'))
                if(!starts_word(modifier, "#foreign") &&
                   !starts_word(modifier, "#must"))
                    die_at(Span(rel, line_no, 1),
                           "unknown function modifier: %s", modifier);
        }
        if(mode == TOP &&
           cond_top_step(t, tframes, &tframe_count,
                         module, &consts, &compile_context,
                         path, rel, line_no, NULL)) {
            continue;
        } else if(mode == TOP && tframe_count > 0 &&
                  !tframes[tframe_count - 1].active) {
            continue;
        }
        if(mode == TOP && export_directive) {
            if(program_export)
                die_at(Span(rel, line_no, 1),
                       "#program_export must precede exactly one function");
            program_export = 1;
            program_export_line = line_no;
            copy_text(program_export_symbol,
                      sizeof(program_export_symbol),
                      export_directive_symbol);
            if(export_directive == 1)
                continue;
        }
        if(mode == TOP &&
           (starts_word(t, "#law") || starts_word(t, "#law_waive"))) {
            if(starts_word(t, "#law_waive")) {
                const char *body = skip_ws(t + strlen("#law_waive"));
                char name[ZIR_NAME_MAX], reason[ZIR_TEXT_MAX];
                size_t length = 0;
                while((isalnum((unsigned char)*body) || *body == '_') &&
                      length + 1 < sizeof(name))
                    name[length++] = *body++;
                name[length] = '\0';
                body = skip_ws(body);
                if(length == 0 || *body != '"') {
                    body = skip_ws(body);
                    const char *semi = body[0] == '"' ? NULL : NULL;
                    (void)semi;
                }
                {
                    const char *quote = *body == '"' ? body + 1 : NULL;
                    const char *end = quote != NULL ?
                        strchr(quote, '"') : NULL;
                    const char *semi = end != NULL ?
                        skip_ws(end + 1) : NULL;
                    if(quote == NULL || end == NULL || semi == NULL ||
                        strcmp(semi, ";") != 0)
                        die_at(Span(rel, line_no, 1),
                               "#law_waive requires NAME \"reason\";");
                    if((size_t)(end - quote) >= sizeof(reason))
                        die_at(Span(rel, line_no, 1),
                               "#law_waive reason is too long");
                    memcpy(reason, quote, (size_t)(end - quote));
                    reason[end - quote] = '\0';
                    if(!ModuleAddLawWaiver(module, name, reason,
                                           Span(rel, line_no, 1)))
                        die_at(Span(rel, line_no, 1),
                               "out of memory recording law waiver");
                }
            } else {
                const char *body = skip_ws(t + strlen("#law"));
                char name[ZIR_NAME_MAX], kind[16];
                char payload[ZIR_TEXT_MAX];
                size_t length = 0;
                const char *semi;
                while((isalnum((unsigned char)*body) || *body == '_') &&
                      length + 1 < sizeof(name))
                    name[length++] = *body++;
                name[length] = '\0';
                body = skip_ws(body);
                length = 0;
                while((isalnum((unsigned char)*body) || *body == '_') &&
                      length + 1 < sizeof(kind))
                    kind[length++] = *body++;
                kind[length] = '\0';
                semi = body;
                while(*semi && *semi != ';') semi++;
                if(name[0] == '\0' || kind[0] == '\0' ||
                   (strcmp(kind, "type") != 0 &&
                    strcmp(kind, "effect") != 0 &&
                    strcmp(kind, "bounds") != 0 &&
                    strcmp(kind, "abi") != 0 &&
                    strcmp(kind, "custom") != 0 &&
                    strcmp(kind, "size") != 0) ||
                   *skip_ws(body) == '\0' || *semi != ';' ||
                   semi == body)
                    die_at(Span(rel, line_no, 1),
                           "#law requires NAME kind payload; with kind "
                           "type, effect, bounds, abi, custom, or size");
                {
                    const char *start = skip_ws(body);
                    size_t payload_length = (size_t)(semi - start);
                    if(payload_length == 0 ||
                       payload_length >= sizeof(payload))
                        die_at(Span(rel, line_no, 1),
                               "#law payload is empty or too long");
                    memcpy(payload, start, payload_length);
                    payload[payload_length] = '\0';
                    trim_in_place(payload);
                }
                if(!ModuleAddLaw(module, name, kind, payload,
                                 Span(rel, line_no, 1)))
                    die_at(Span(rel, line_no, 1),
                           "out of memory recording law");
            }
            continue;
        } else if(mode == TOP &&
                  parse_compile_check(module, rel, line_no, t, &consts)) {
            continue;
        } else if(mode == TOP &&
                  (strcmp(t, "#scope_file") == 0 ||
                   strcmp(t, "#scope_module") == 0 ||
                   strcmp(t, "#scope_export") == 0)) {
            scope_public = strcmp(t, "#scope_export") == 0;
            scope_file = strcmp(t, "#scope_file") == 0;
            continue;
        } else if(mode == TOP && program_export &&
                  !looks_like_function_header(t)) {
            die_at(Span(rel, program_export_line, 1),
                   "#program_export must precede a function declaration");
        } else if(mode == TOP &&
                  (starts_word(t, "using") || strncmp(t, "using,", 6) == 0) &&
                  strstr(t, "#import") == NULL) {
            char name[ZIR_NAME_MAX];
            char filter[160];
            const char *path = skip_ws(t + 5);
            filter[0] = '\0';
            parse_using_modifiers(&path, filter, sizeof(filter), rel, line_no);
            if(strchr(path, ':') != NULL) {
                parse_file_global(module, path, Span(rel, line_no, 1),
                                  scope_public, scope_file,
                                  name, sizeof(name));
            } else {
                size_t length = strlen(path);
                if(length < 2 || path[length - 1] != ';' ||
                   length >= sizeof(name))
                    die_at(Span(rel, line_no, 1),
                           "using requires a type or record binding and ';'");
                memcpy(name, path, length - 1);
                name[length - 1] = '\0';
                trim_in_place(name);
                if(!is_member_path_text(name))
                    die_at(Span(rel, line_no, 1),
                           "using requires a type or record binding name");
            }
            ZirUsing *using = ModuleAddUsing(module, name,
                Span(rel, line_no, 1));
            if(using == NULL)
                die_at(Span(rel, line_no, 1),
                       "out of memory recording using declaration");
            copy_text(using->filter, sizeof(using->filter), filter);
            using->is_file_private = scope_file;
            continue;
        } else if(mode == TOP && starts_word(t, "#load")) {
            char requested[SOURCE_PATH_MAX];
            char candidate[SOURCE_PATH_MAX * 2];
            const char *argument = skip_ws(t + strlen("#load"));
            const char *end = *argument == '"' ?
                              strchr(argument + 1, '"') : NULL;
            const char *slash = strrchr(path, '/');
            if(end == NULL || strcmp(skip_ws(end + 1), ";") != 0 ||
               (size_t)(end - argument - 1) >= sizeof(requested))
                die_at(Span(rel, line_no, 1),
                       "#load requires a quoted relative .zi path and ';'");
            memcpy(requested, argument + 1,
                   (size_t)(end - argument - 1));
            requested[end - argument - 1] = '\0';
            size_t length = strlen(requested);
            if(requested[0] == '/' || length < 4 ||
               strcmp(requested + length - 3, ".zi") != 0 ||
               strchr(requested, '\\') != NULL)
                die_at(Span(rel, line_no, 1),
                       "#load requires a quoted relative .zi path and ';'");
            if(load_depth >= (int)(sizeof(load_frames) / sizeof(load_frames[0])))
                die_at(Span(rel, line_no, 1), "#load nesting limit exceeded");
            if(slash != NULL) {
                if(snprintf(candidate, sizeof(candidate), "%.*s/%s",
                            (int)(slash - path), path, requested) >=
                   (int)sizeof(candidate))
                    die_at(Span(rel, line_no, 1), "#load path is too long");
            } else
                copy_text(candidate, sizeof(candidate), requested);
            char *next_path = realpath(candidate, NULL);
            if(next_path == NULL)
                die_at(Span(rel, line_no, 1),
                       "cannot find loaded file: %s", requested);
            if(strcmp(next_path, canonical) == 0)
                die_at(Span(rel, line_no, 1), "cyclic #load: %s", requested);
            for(int i = 0; i < load_depth; i++)
                if(strcmp(next_path, load_frames[i].canonical) == 0)
                    die_at(Span(rel, line_no, 1), "cyclic #load: %s", requested);
            LoadFrame *frame = &load_frames[load_depth++];
            frame->path = path;
            frame->source = source;
            frame->canonical = canonical;
            frame->owned_source = owned_source;
            copy_text(frame->rel, sizeof(frame->rel), rel);
            copy_text(frame->lookahead, sizeof(frame->lookahead), lookahead);
            frame->line_no = line_no;
            frame->physical_line_no = physical_line_no;
            frame->have_look = have_look;
            frame->scope_public = scope_public;
            frame->scope_file = scope_file;
            frame->in_block_comment = in_block_comment;
            frame->conditional_depth = tframe_count;
            canonical = next_path;
            path = canonical;
            owned_source = read_lowered_source(path);
            source = owned_source;
            copy_text(rel, sizeof(rel), relative_path(root, path));
            line_no = 0;
            physical_line_no = 0;
            have_look = 0;
            lookahead[0] = '\0';
            scope_public = 1;
            scope_file = 0;
            in_block_comment = 0;
            continue;
        } else if(mode == TOP &&
                  (string_source = import_string_source(
                       t, Span(rel, line_no, 1))) != NULL) {
            char alias[ZIR_NAME_MAX];
            char synthetic_name[ZIR_NAME_MAX];
            char synthetic_path[SOURCE_PATH_MAX * 2];
            uint32_t hash = 2166136261u;
            for(const unsigned char *p = (const unsigned char *)rel; *p; p++)
                hash = (hash ^ *p) * 16777619u;
            for(const unsigned char *p = (const unsigned char *)t; *p; p++)
                hash = (hash ^ *p) * 16777619u;
            hash = (hash ^ (uint32_t)line_no) * 16777619u;
            snprintf(synthetic_name, sizeof(synthetic_name),
                     "string_%08x", hash);
            const char *slash = strrchr(path, '/');
            int written = slash == NULL ?
                snprintf(synthetic_path, sizeof(synthetic_path), "%s.zi",
                         synthetic_name) :
                snprintf(synthetic_path, sizeof(synthetic_path),
                         "%.*s/%s.zi", (int)(slash - path), path,
                         synthetic_name);
            if(written < 0 || (size_t)written >= sizeof(synthetic_path))
                die_at(Span(rel, line_no, 1),
                       "#import, string source path is too long");
            int named = parse_symbol_before_colons(t, alias, sizeof(alias));
            ZirProgram *fragment = parse_source(synthetic_path, root,
                                                string_source, resolver,
                                                resolver_context,
                                                defines, define_count);
            free(string_source);
            if(fragment == NULL) {
                ProgramFree(program);
                free(consts.items);
                free(future_constants.items);
                free(future_usings.items);
                free(future_imports.items);
                free(future_types.items);
                free_deferred_types(&deferred_types);
                free_discovered_functions(&future_functions);
                free(future_globals.items);
                free_discovered_files(&discovered_files);
                free(canonical);
                return NULL;
            }
            for(int imported = 0; imported < fragment->module_count;
                imported++) {
                ZirModule *incoming = &fragment->modules[imported];
                for(int existing = 0; existing < program->module_count;
                    existing++)
                    if(strcmp(program->modules[existing].name,
                              incoming->name) == 0)
                        die_at(Span(rel, line_no, 1),
                               "duplicate #import, string module identity");
                ZirModule *added = ProgramAddModule(program, incoming->name,
                    incoming->source_path, incoming->span);
                if(added == NULL)
                    die("out of memory importing source string");
                *added = *incoming;
                *incoming = (ZirModule){0};
            }
            ProgramFree(fragment);
            module = &program->modules[0];
            ZirImport *import = ModuleAddImport(module,
                named ? ZIR_IMPORT_MODULE : ZIR_IMPORT_OPEN,
                named ? alias : synthetic_name, synthetic_name, "string",
                scope_public, Span(rel, line_no, 1));
            if(import == NULL)
                die("out of memory importing source string");
            import->is_file_private = scope_file;
            continue;
        } else if(mode == TOP && t[0] == '#' &&
                  !starts_word(t, "#import") &&
                  strncmp(t, "#import,", 8) != 0 &&
                  !starts_word(t, "#enum")) {
            die_at(Span(rel, line_no, 1),
                   "unknown directive: %s", t);
        } else if(mode == TOP &&
                  parse_foreign_library_line(rel, line_no, t,
                      foreign_library_names, foreign_library_targets,
                      &foreign_library_count)) {
            copy_text(foreign_library_paths[foreign_library_count - 1],
                      sizeof(foreign_library_paths[0]), rel);
            foreign_library_file_private[foreign_library_count - 1] =
                scope_file;
            continue;
        } else if(mode == TOP &&
                  (parse_import_line(module, rel, line_no, t,
                                     scope_public) ||
                   parse_foreign_line(module, rel, line_no, t,
                       scope_public, foreign_library_names,
                       foreign_library_targets, foreign_library_paths,
                       foreign_library_file_private,
                       foreign_library_count))) {
            program_export = 0;
            if(module->import_count > 0)
                module->imports[module->import_count - 1].is_file_private = scope_file;
            continue;
        } else if(mode == TOP && starts_word(t, "state") &&
                  *skip_ws(t + 5) == '{') {
            die_at(Span(rel, line_no, 1),
                   "state blocks are not Jai syntax; declare named globals");
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  starts_word(skip_ws(strstr(t, "::") + 2), "#type")) {
            char name[ZIR_NAME_MAX];
            char args[ZIR_TEXT_MAX];
            char ret[ZIR_NAME_MAX];
            const char *declaration = skip_ws(strstr(t, "::") + 2);
            const char *parameters = skip_ws(declaration + strlen("#type"));
            if(!parse_symbol_before_colons(t, name, sizeof(name)) ||
               *parameters != '(' || strchr(t, '{') != NULL ||
               t[strlen(t) - 1] != ';')
                die_at(Span(rel, line_no, 1),
                       "procedure type requires Name :: #type (Types) -> Result;");
            parse_function_header(name, sizeof(name), args, sizeof(args),
                                  ret, sizeof(ret), t);
            size_t return_length = strlen(ret);
            const char *c_call = strstr(t, "#c_call");
            int is_c_call = c_call != NULL;
            if(is_c_call) {
                if(*skip_ws(c_call + strlen("#c_call")) != ';' ||
                   skip_ws(c_call + strlen("#c_call"))[1] != '\0' ||
                   return_length == 0)
                    die_at(Span(rel, line_no, 1),
                           "procedure type requires Result #c_call;");
            } else {
                if(return_length == 0 || ret[return_length - 1] != ';')
                    die_at(Span(rel, line_no, 1),
                           "procedure type requires a result type and ';'");
                ret[--return_length] = '\0';
                while(return_length > 0 &&
                      isspace((unsigned char)ret[return_length - 1]))
                    ret[--return_length] = '\0';
            }
            if(strcmp(ret, "()") == 0)
                copy_text(ret, sizeof(ret), "void");
            if(program_export)
                die_at(Span(rel, program_export_line, 1),
                       "#program_export requires a function");
            ZirType *slot = ModuleAddType(module, name, Span(rel, line_no, 1));
            if(slot == NULL)
                die("out of memory declaring procedure type");
            slot->is_procedure_type = 1;
            slot->is_c_call = is_c_call;
            slot->is_public = scope_public;
            slot->is_file_private = scope_file;
            copy_text(slot->procedure_return_type,
                      sizeof(slot->procedure_return_type), ret);
            if(*skip_ws(args)) {
                char parts[16][ZIR_TEXT_MAX];
                int count = split_top_level(args, parts[0], 16,
                                            sizeof(parts[0]));
                size_t used = 0;
                if(count < 1 || count > 16)
                    die_at(slot->span, "invalid procedure type parameters");
                for(int parameter = 0; parameter < count; parameter++) {
                    char *part = trim(parts[parameter]);
                    char *colon = strchr(part, ':');
                    const char *type = colon == NULL ? part : skip_ws(colon + 1);
                    char parameter_name[ZIR_NAME_MAX];
                    int written;
                    if(colon != NULL) {
                        *colon = '\0';
                        trim_in_place(part);
                        if(!is_identifier_text(part))
                            die_at(slot->span,
                                   "procedure type parameter needs a name");
                        copy_text(parameter_name, sizeof(parameter_name), part);
                    } else
                        snprintf(parameter_name, sizeof(parameter_name),
                                 "arg%d", parameter);
                    if(*type == '\0' || strchr(type, ':') != NULL)
                        die_at(slot->span,
                               "procedure type parameter needs a type");
                    written = snprintf(slot->body + used,
                                       sizeof(slot->body) - used,
                                       "%s%s: %s", parameter ? ", " : "",
                                       parameter_name, type);
                    if(written < 0 || (size_t)written >=
                        sizeof(slot->body) - used)
                        die_at(slot->span,
                               "procedure type parameters exceed size limit");
                    used += (size_t)written;
                }
            }
            continue;
        } else if(mode == TOP && looks_like_function_header(t)) {
            char name[ZIR_NAME_MAX];
            char args[ZIR_TEXT_MAX];
            char defaults[ZIR_TEXT_MAX] = "";
            char ret[ZIR_NAME_MAX];
            int has_body = strchr(t, '{') != NULL;
            int conversion = 0;
            {
                const char *marker = strstr(t, "::");
                if(marker != NULL) {
                    marker = skip_ws(marker + 2);
                    if(starts_word(marker, "#as")) {
                        conversion = 1;
                        if(*skip_ws(marker + 3) != '(')
                            die_at(Span(rel, line_no, 1),
                                   "#as requires (source: Type) -> Result");
                    }
                }
            }

            parse_function_header(name, sizeof(name), args, sizeof(args),
                                  ret, sizeof(ret), t);
            if(!has_body)
                die_at(Span(rel, line_no, 1),
                       "procedure types require Jai #type syntax");
            if(name[0] != '\0') {
                separate_parameter_defaults(args, sizeof(args), defaults,
                                            sizeof(defaults),
                                            Span(rel, line_no, 1));
                uint64_t using_parameters = strip_using_parameters(
                    args, sizeof(args), Span(rel, line_no, 1));
                if(defaults[0] && strip_using_parameters(
                       defaults, sizeof(defaults),
                       Span(rel, line_no, 1)) != using_parameters)
                    die_at(Span(rel, line_no, 1),
                           "inconsistent using parameter defaults");
                fn = ModuleAddFunction(module, name, args, ret, 0,
                                          Span(rel, line_no, 1));
                if(conversion) {
                    char parameters[64][ZIR_TEXT_MAX];
                    int count = *skip_ws(args) ?
                        split_top_level(args, parameters[0], 1,
                                        sizeof(parameters[0])) : 0;
                    if(count != 1 || strchr(args, '$') != NULL ||
                       strcmp(ret, "void") == 0)
                        die_at(fn->span,
                               "#as requires one source parameter and a result");
                    fn->is_conversion = 1;
                }
                fn->using_parameters = using_parameters;
                fn->must_use = function_must_use(t, ret, fn->span);
                copy_text(fn->default_args, sizeof(fn->default_args), defaults);
                if(strchr(args, '$') != NULL) {
                    char parameters[64][ZIR_TEXT_MAX];
                    int count = split_top_level(args, parameters[0], 64,
                                                sizeof(parameters[0]));
                    if(count < 1)
                        die_at(fn->span, "invalid polymorphic procedure parameters");
                    for(int parameter = 0; parameter < count; parameter++) {
                        char *colon = strchr(parameters[parameter], ':');
                        char *type = colon == NULL ? NULL : trim(colon + 1);
                        if(type == NULL || strchr(type, '$') == NULL)
                            continue;
                        if(type[0] != '$' || !is_identifier_text(type + 1) ||
                           (fn->template_param[0] &&
                            strcmp(fn->template_param, type + 1)))
                            die_at(fn->span,
                                   "polymorphic procedure requires one $Type parameter");
                        copy_text(fn->template_param,
                                  sizeof(fn->template_param), type + 1);
                    }
                    if(fn->template_param[0] == '\0')
                        die_at(fn->span, "invalid polymorphic procedure parameter");
                    if(program_export)
                        die_at(fn->span,
                               "#program_export requires a concrete procedure");
                    fn->is_template = 1;
                }
                /* Preserve the Jai source name and optional linker symbol. */
                fn->exported = program_export;
                copy_text(fn->export_symbol,
                          sizeof(fn->export_symbol),
                          program_export_symbol);
                program_export = 0;
                program_export_symbol[0] = '\0';
                /* Public functions are emitted in headers. */
                fn->is_public = scope_public;
                fn->is_file_private = scope_file;
                mode = FUNCTION;
                depth = 1;
            }
        } else if(mode == TOP && contains_source_directive(t, "#global")) {
            die_at(Span(rel, line_no, 1),
                   "#global is not Jai syntax; declare a variable with name: Type");
        } else if(mode == TOP &&
                  (isalpha((unsigned char)t[0]) || t[0] == '_') &&
                  strchr(t, ':') != NULL && strstr(t, "::") == NULL) {
            /* A Jai file-scope variable uses the same typed declaration as
             * a local: name: Type; or name: Type = initializer; */
            parse_file_global(module, t, Span(rel, line_no, 1),
                              scope_public, scope_file, NULL, 0);
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  strstr(t, "#type") != NULL) {
            die_at(Span(rel, line_no, 1),
                   "C declarator #type is not Jai syntax; use Name :: #type (arguments) -> Result;");
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  starts_word(skip_ws(strstr(t, "::") + 2),
                              "specialize")) {
            die_at(Span(rel, line_no, 1),
                   "use Jai-style type application: Name :: Generic(Type)");
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  !starts_word(skip_ws(strstr(t, "::") + 2), "struct") &&
                  !starts_word(skip_ws(strstr(t, "::") + 2), "union") &&
                  !starts_word(skip_ws(strstr(t, "::") + 2), "enum") &&
                  !starts_word(skip_ws(strstr(t, "::") + 2), "enum_flags") &&
                  !starts_word(skip_ws(strstr(t, "::") + 2), "variant") &&
                  !looks_like_function_header(t)) {
            /* 'Name :: expr' is the Ziran constant declaration. Ordinary
             * constants are also emitted into generated interfaces so public
             * types can use them in array bounds. Platform predicates remain
             * frontend-only because #defined is not a C expression. */
            const char *colons = strstr(t, "::");
            char *expr = (char *)colons + 2;
            char cname[ZIR_NAME_MAX];
            size_t cn = 0;
            const char *q = t;

            while(q < colons && (isalnum((unsigned char)*q) || *q == '_') &&
                  cn + 1 < sizeof(cname))
                cname[cn++] = *q++;
            cname[cn] = '\0';
            while(*expr == ' ' || *expr == '\t')
                expr++;
            size_t expression_length = strlen(expr);
            while(expression_length > 0 &&
                  isspace((unsigned char)expr[expression_length - 1]))
                expr[--expression_length] = '\0';
            if(expression_length > 0 && expr[expression_length - 1] == ';')
                expr[--expression_length] = '\0';
            while(expression_length > 0 &&
                  isspace((unsigned char)expr[expression_length - 1]))
                expr[--expression_length] = '\0';
            if(find_unquoted_word(expr, "#this") != NULL)
                die_at(Span(rel, line_no, 1),
                       "#this requires a procedure or type scope");
            if(cname[0] != '\0' && *expr != '\0') {
                if(starts_word(expr, "#define"))
                    die_at(Span(rel, line_no, 1), "use '%s :: value'; #define is not Ziran syntax", cname);
                else {
                    char run_value[ZIR_TEXT_MAX];
                    char selected_value[ZIR_TEXT_MAX];
                    ZirDefine *def;
                    int deferred_run = 0;

                    if(consts.count == consts.capacity) {
                        int capacity = consts.capacity > 0 ? consts.capacity * 2 : 16;
                        ZirConst *items = realloc(consts.items,
                            (size_t)capacity * sizeof(*items));

                        if(items == NULL)
                            die("out of memory");
                        consts.items = items;
                        consts.capacity = capacity;
                    }
                    if(starts_word(expr, "#run")) {
                        char expanded[ZIR_TEXT_MAX];
                        long value = 0;
                        int folded;
                        ZirConsts visible = visible_compile_constants(
                            &consts, &future_constants);
                        ZirModule run_scope = visible_compile_module(
                            module, &compile_context, &visible);

                        expand_compile_expr(expanded, sizeof(expanded),
                                            &visible, trim((char *)(expr + 4)),
                                            rel);
                        folded = strstr(expanded, "size_of") == NULL &&
                                 eval_const_condition(expanded, &value,
                                                     &run_scope,
                                                     &visible, rel, line_no, 1);
                        if(folded)
                            snprintf(run_value, sizeof(run_value),
                                     "%ld", value);
                        else {
                            CompileValue typed = {0};
                            int fuel = 10000;
                            if(compile_context.resolver != NULL &&
                               !compile_context.resolver(
                                   compile_context.resolver_context, program,
                                   &run_scope, path, root, expanded))
                                die_at(Span(rel, line_no, 1),
                                       "cannot resolve imports for #run expression");
                            if(evaluate_typed_expression(&run_scope, &visible,
                                    expanded, Span(rel, line_no, 1), 1,
                                    &fuel, &typed) &&
                               typed.kind != COMPILE_INVALID) {
                                copy_text(run_value, sizeof(run_value),
                                          typed.literal);
                                folded = 1;
                            }
                        }
                        if(!folded) {
                            int written = snprintf(run_value,
                                sizeof(run_value), "#run %s",
                                trim((char *)(expr + 4)));
                            if(written < 0 ||
                               (size_t)written >= sizeof(run_value))
                                die_at(Span(rel, line_no, 1),
                                       "#run expression is too long");
                            deferred_run = 1;
                        }
                        free_visible_compile_module(&run_scope);
                        free(visible.items);
                        expr = run_value;
                    }
                    if(!deferred_run &&
                       find_unquoted_text(expr, "#ifx") != NULL) {
                        copy_text(selected_value, sizeof(selected_value), expr);
                        lower_compile_ifx_value(selected_value,
                                                sizeof(selected_value), module,
                                                &consts, Span(rel, line_no, 1),
                                                1);
                        expr = selected_value;
                    }
                    if(!deferred_run &&
                       find_unquoted_text(expr, "#ifx") == NULL &&
                       find_unquoted_word(expr, "size_of") != NULL &&
                       find_unquoted_word(expr, "type_of") == NULL) {
                        if(expr != selected_value)
                            copy_text(selected_value,
                                      sizeof(selected_value), expr);
                        lower_size_of_value(selected_value,
                                            sizeof(selected_value), module,
                                            Span(rel, line_no, 1));
                        expr = selected_value;
                    }
                    int known_alias = 0;
                    if(is_identifier_text(expr)) {
                        for(int i = 0; i < module->define_count; i++) {
                            if(!strcmp(module->defines[i].name, expr)) {
                                known_alias = 1;
                                break;
                            }
                        }
                        /* Platform predicates stay out of the define table,
                         * so also consult the local constant table: an alias
                         * of a declared constant is a define, not an open
                         * enum member reference. */
                        if(!known_alias) {
                            for(int i = 0; i < consts.count; i++) {
                                if(!strcmp(consts.items[i].name, expr)) {
                                    known_alias = 1;
                                    break;
                                }
                            }
                        }
                    }
                    int using_alias = module->using_count > 0 &&
                        is_identifier_text(expr) && !known_alias &&
                        strcmp(expr, "true") && strcmp(expr, "false") &&
                        strcmp(expr, "null");
                    if(!starts_word(expr, "#defined") &&
                       (!is_identifier_text(expr) || known_alias ||
                        using_alias)) {
                        def = ModuleAddDefine(module, cname, expr,
                                                 Span(rel, line_no, 1));
                        if(def != NULL) {
                            def->is_public = scope_public;
                            def->is_file_private = scope_file;
                            def->requires_open_enum = using_alias;
                        }
                    }
                    memset(&consts.items[consts.count], 0,
                           sizeof(consts.items[consts.count]));
                    snprintf(consts.items[consts.count].name,
                             sizeof(consts.items[0].name), "%s", cname);
                    snprintf(consts.items[consts.count].expr,
                             sizeof(consts.items[0].expr), "%s", expr);
                    copy_text(consts.items[consts.count].path,
                              sizeof(consts.items[0].path), rel);
                    consts.items[consts.count].is_file_private = scope_file;
                    consts.items[consts.count].is_public = scope_public;
                    consts.items[consts.count].source_line = line_no;
                    consts.count++;
                }
            }
        } else if(mode == TOP && strstr(t, "::") != NULL) {
            /* Named records and enums. */
            const char *colons = strstr(t, "::");
            const char *after = colons + 2;
            char tname[ZIR_NAME_MAX];
            size_t tn = 0;

            after = skip_ws(after);
            if(starts_word(after, "variant"))
                die_at(Span(rel, line_no, 1),
                       "variant declarations are not Jai syntax; use a struct or union");
            int enum_flags = strncmp(after, "enum_flags", 10) == 0 &&
                (after[10] == '\0' || isspace((unsigned char)after[10]) ||
                 after[10] == '{');
            int named_enum = enum_flags ||
                (strncmp(after, "enum", 4) == 0 &&
                 (after[4] == '\0' || isspace((unsigned char)after[4]) ||
                  after[4] == '{'));
            int named_union = strncmp(after, "union", 5) == 0 &&
                (after[5] == '\0' || after[5] == ' ' ||
                 after[5] == '(' || after[5] == '{');
            if((strncmp(after, "struct", 6) == 0 &&
                (after[6] == '\0' || after[6] == ' ' ||
                 after[6] == '(' || after[6] == '{')) || named_union ||
               named_enum) {
                const char *q = t;
                ZirType *ty;

                while(q < colons && (isalnum((unsigned char)*q) || *q == '_') &&
                      tn + 1 < sizeof(tname))
                    tname[tn++] = *q++;
                tname[tn] = '\0';
                char parameters[ZIR_NAME_MAX] = "";
                if(skip_ws(q) != colons)
                    die_at(Span(rel, line_no, 1),
                           "type declarations require a plain name before ::");
                if(!is_identifier_text(tname))
                    die_at(Span(rel, line_no, 1), "invalid type name");
                if((strncmp(after, "struct", 6) == 0 || named_union) &&
                   !parse_type_parameters(after, named_union ? "union" : "struct", parameters,
                                          sizeof(parameters)))
                    die_at(Span(rel, line_no, 1),
                           "polymorphic type parameters require (T: Type, ...)");
                ty = ModuleAddType(module, tname,
                                      Span(rel, line_no, 1));
                if(ty != NULL) {
                    ty->is_public = scope_public;
                    ty->is_file_private = scope_file;
                    ty->is_enum = named_enum;
                    ty->is_union = named_union;
                    ty->is_enum_flags = enum_flags;
                    if(ty->is_enum)
                        parse_enum_backing(ty, after);
                    ty->is_record_template = parameters[0] != '\0' &&
                        (strncmp(after, "struct", 6) == 0 || named_union);
                    copy_text(ty->template_params,
                              sizeof(ty->template_params), parameters);
                    if(strstr(after, "#extern") != NULL)
                        die_at(Span(rel, line_no, 1),
                               "#extern is not Jai syntax for a type declaration");
                    const char *opening = strchr(after, '{');
                    const char *closing = opening ? strchr(opening + 1, '}') : NULL;
                    if(closing != NULL) {
                        size_t body_length = (size_t)(closing - opening - 1);
                        if(ty->is_record_template) {
                            if(body_length + 1 >= sizeof(ty->body))
                                die_at(ty->span, "generic record body exceeds size limit");
                            memcpy(ty->body, opening + 1, body_length);
                            int nesting = 0;
                            for(size_t byte = 0; byte < body_length; byte++) {
                                if(ty->body[byte] == '[' || ty->body[byte] == '(')
                                    nesting++;
                                if(ty->body[byte] == ']' || ty->body[byte] == ')')
                                    nesting--;
                                if((ty->body[byte] == ',' ||
                                    ty->body[byte] == ';') && nesting == 0)
                                    ty->body[byte] = '\n';
                            }
                            ty->body[body_length] = '\n';
                            ty->body[body_length + 1] = '\0';
                        } else {
                            if(body_length >= sizeof(ty->body))
                                die_at(ty->span, "enum body exceeds size limit");
                            memcpy(ty->body, opening + 1, body_length);
                            ty->body[body_length] = '\0';
                            for(size_t byte = 0; byte < body_length; byte++)
                                if(ty->body[byte] == ';') ty->body[byte] = '\n';
                            if(ty->is_enum)
                                lower_enum_values(ty);
                        }
                        if(!expand_type_this(ty))
                            die_at(ty->span, "#this type body exceeds size limit");
                    } else {
                        mode = TYPE;
                        type_frame_count = 0;
                    }
                }
                fn = NULL;
            }
        } else if((mode == TOP || mode == TYPE) &&
                  starts_word(t, "#enum")) {
            die_at(Span(rel, line_no, 1),
                   "#enum is not Jai syntax; use Name :: enum { ... }");
        } else if(mode == TYPE) {
            if(cond_top_step(t, type_frames, &type_frame_count,
                             module, &consts, &compile_context,
                             path, rel, line_no, NULL))
                continue;
            if(type_frame_count > 0 &&
               !type_frames[type_frame_count - 1].active)
                continue;
            if(t[0] == '}') {
                ZirType *ty = &module->types[module->type_count - 1];
                if(!expand_type_this(ty))
                    die_at(ty->span, "#this type body exceeds size limit");
                if(ty->is_enum)
                    lower_enum_values(ty);
                mode = TOP;
                cond_frame_settle(tframes, tframe_count);
            } else if(strcmp(t, "{") == 0 &&
                      module->types[module->type_count - 1].body[0] == '\0') {
                /* Jai commonly places a type's opening brace on the next line. */
            } else if(t[0] == '#') {
                die_at(Span(rel, line_no, 1),
                       "unknown type-body directive: %s", t);
            } else {
                ZirType *ty = &module->types[module->type_count - 1];
                char *body = ty->body;
                size_t capacity = sizeof(ty->body);
                size_t used = strlen(body);
                int written = snprintf(body + used, capacity - used, "%s\n", t);
                if(written < 0 || (size_t)written >= capacity - used)
                    die_at(ty->span, "type body exceeds size limit");
                if(!ty->is_record_template)
                    for(size_t byte = used; byte < used + (size_t)written; byte++)
                        if(body[byte] == ';') body[byte] = '\n';
            }
        } else if(mode == FUNCTION) {
            char *bcnd = NULL;
            int bck;
            int parallel_for = 0;
            int parallel_gpu = 0;
            char parallel_text[SOURCE_LINE_MAX * 2];
            if(starts_word(t, "#parallel_gpu")) {
                const char *after = skip_ws(t + strlen("#parallel_gpu"));
                if(!starts_word(after, "for"))
                    die_at(Span(rel, line_no, 1),
                           "#parallel_gpu requires a for region");
                parallel_for = 1;
                parallel_gpu = 1;
                snprintf(parallel_text, sizeof(parallel_text), "%s", after);
                t = trim(parallel_text);
            } else if(starts_word(t, "#parallel")) {
                const char *after = skip_ws(t + strlen("#parallel"));
                if(!starts_word(after, "for"))
                    die_at(Span(rel, line_no, 1),
                           "#parallel requires a for region");
                parallel_for = 1;
                snprintf(parallel_text, sizeof(parallel_text), "%s", after);
                t = trim(parallel_text);
            }
            bck = parse_cond_start(t, &bcnd);

            int compile_else = body_mcount > 0 &&
                depth == body_mdepth[body_mcount - 1] &&
                line_is_compile_else(t);
            int compile_else_if = bck == 2 && body_mcount > 0 &&
                depth == body_mdepth[body_mcount - 1];

            if(bck == 1) {
                int parent_active = body_mcount == 0 ||
                    body_mactive[body_mcount - 1];
                int chosen = parent_active &&
                    select_compile_condition(module, &consts, bcnd,
                                             Span(rel, line_no, 1),
                                             &compile_context, path, NULL);
                if(body_mcount >= 8)
                    die_at(Span(rel, line_no, 1), "too many nested #if blocks");
                body_mdepth[body_mcount] = depth;
                body_mselected[body_mcount] = chosen;
                body_mparent_active[body_mcount] = parent_active;
                body_mactive[body_mcount] = parent_active && chosen;
                body_mcount++;
            } else if(compile_else_if) {
                int i = body_mcount - 1;
                int chosen = body_mparent_active[i] &&
                    !body_mselected[i] &&
                    select_compile_condition(module, &consts, bcnd,
                                             Span(rel, line_no, 1),
                                             &compile_context, path, NULL);
                body_mactive[i] = body_mparent_active[i] &&
                    !body_mselected[i] && chosen;
                body_mselected[i] |= chosen;
            } else if(compile_else) {
                int i = body_mcount - 1;
                body_mactive[i] = body_mparent_active[i] &&
                    !body_mselected[i];
                body_mselected[i] = 1;
            } else if(bck == 2) {
                die_at(Span(rel, line_no, 1),
                       "else #if without a matching #if");
            } else if(t[0] == '}' && body_mcount > 0 &&
                      depth == body_mdepth[body_mcount - 1]) {
                /* this '}' closes a body-level '#if' region, not a block */
                body_mcount--;
            } else if(body_mcount > 0 &&
                      !body_mactive[body_mcount - 1]) {
                depth += net_block_braces(t);
            } else if(t[0] == '#' && strcmp(t, "#through") != 0 &&
                      strcmp(t, "#through;") != 0 &&
                      !starts_word(t, "#parallel")) {
                die_at(Span(rel, line_no, 1),
                       "unknown function-body directive: %s", t);
            } else if(t[0] == '}') {
                /* A leading brace closes the if-body. A braced else opens a
                 * new body here; a single-statement else is braced during
                 * normalization. Record the close and following else as
                 * separate checked statements. */
                const char *eq = t + 1;

                while(*eq == ' ' || *eq == '\t')
                    eq++;
                if(depth > 1 && starts_word(eq, "else")) {
                    split_jai_control_line((char *)eq,
                                           SOURCE_LINE_MAX * 4 - (size_t)(eq - t),
                                           onelineq, &onelineq_count,
                                           Span(rel, line_no, 1));
                    ZirSourceSpan span = SpanEnd(rel, line_no,
                                                    pending_start_column +
                                                    (int)(eq - t),
                                                    line_no,
                                                    pending_end_column);
                    FunctionAddStmt(fn, ZIR_STMT_BLOCK_CLOSE, "}",
                                       Span(rel, line_no, 1));
                    FunctionAddStmt(fn, ZIR_STMT_IF, eq, span);
                    depth += net_block_braces(t);
                } else {
                    if(depth > 0)
                        depth--;
                    if(depth == 0) {
                        mode = TOP;
                        cond_frame_settle(tframes, tframe_count);
                        fn = NULL;
                    } else {
                        FunctionAddStmt(fn, ZIR_STMT_BLOCK_CLOSE, t,
                                           Span(rel, line_no, 1));
                    }
                }
            } else {
                int using_binding = 0;
                char using_filter[160];
                char using_path_text[SOURCE_LINE_MAX * 2];
                using_filter[0] = '\0';
                using_path_text[0] = '\0';
                if(starts_word(t, "using") ||
                   strncmp(t, "using,", 6) == 0) {
                    if(!isspace((unsigned char)t[5]) && t[5] != ',')
                        die_at(Span(rel, line_no, 1),
                               "using modifiers are not supported");
                    const char *after = skip_ws(t + 5);
                    parse_using_modifiers(&after, using_filter,
                                          sizeof(using_filter), rel, line_no);
                    snprintf(using_path_text, sizeof(using_path_text), "%s",
                             after);
                    t = trim(using_path_text);
                    if(*t == '\0')
                        die_at(Span(rel, line_no, 1),
                               "using needs a record binding");
                    using_binding = 1;
                }
                if(starts_word(t, "match")) {
                    const char *after_match = skip_ws(t + 5);
                    if((t[5] == '!' || t[5] == '?' ||
                        (isspace((unsigned char)t[5]) &&
                         *after_match != '=' && *after_match != ':' &&
                         *after_match != '+' && *after_match != '-' &&
                         *after_match != '*' && *after_match != '/' &&
                         *after_match != '%' && *after_match != '&' &&
                         *after_match != '|' && *after_match != '^' &&
                         *after_match != '.' && *after_match != '(')) &&
                       strchr(t, '{') != NULL)
                        die_at(Span(rel, line_no, 1),
                               "match is not Jai syntax; use if value == { case ... }");
                }
                ZirStmtKind kind = classify_stmt(t);
                if(using_binding && kind == ZIR_STMT_UNKNOWN)
                    kind = ZIR_STMT_EXPR;
                if(using_binding && kind != ZIR_STMT_DECL &&
                   kind != ZIR_STMT_EXPR)
                    die_at(Span(rel, line_no, 1),
                           "using needs a record binding");
                int brace_delta = net_block_braces(t);
                char block_callee[ZIR_NAME_MAX];
                char block_name[ZIR_NAME_MAX];

                if(looks_like_non_jai_control(t, "guard"))
                    die_at(Span(rel, line_no, 1),
                           "guard is not Jai syntax; use if and return");
                if(t[0] == 'c' && isspace((unsigned char)t[1])) {
                    const char *rest = skip_ws(t + 1);
                    if(isalpha((unsigned char)*rest) || *rest == '_')
                        die_at(Span(rel, line_no, 1),
                               "raw C statements are not Jai syntax; use #foreign");
                }
                if(looks_like_non_jai_control(t, "switch"))
                    die_at(Span(rel, line_no, 1),
                           "switch is not Jai syntax; use if value == { case ... }");
                if(looks_like_non_jai_control(t, "goto") ||
                   looks_like_label(t))
                    die_at(Span(rel, line_no, 1),
                           "goto and labels are not Jai syntax; use structured control flow");
                if(kind == ZIR_STMT_DECL && strchr(t, ':') == NULL)
                    die_at(Span(rel, line_no, 1),
                           "C-style local declarations are not Jai syntax; use name: Type");

                if(parse_block_call_header(t, block_callee,
                                         sizeof(block_callee),
                                         block_name, sizeof(block_name))) {
                    die_at(Span(rel, line_no, 1),
                           "block calls are not Jai syntax; call with a record literal");
                }

                {
                    ZirSourceSpan span = SpanEnd(rel, pending_start_line,
                                                    pending_start_column,
                                                    line_no,
                                                    pending_end_column);
                    if(kind == ZIR_STMT_IF && starts_word(t, "if")) {
                        const char *condition = skip_ws(t + 2);
                        if(starts_word(condition, "#complete"))
                            condition = skip_ws(condition + strlen("#complete"));
                        const char *equals = strstr(condition, "==");
                        if(equals != NULL && *skip_ws(equals + 2) == '{' &&
                           *skip_ws(skip_ws(equals + 2) + 1) == '\0')
                            kind = ZIR_STMT_IF_CASE;
                    }
                    ZirStmt *statement = FunctionAddStmt(fn, kind, t, span);
                    if(statement != NULL && parallel_for) {
                        if(kind != ZIR_STMT_FOR)
                            die_at(span, "#parallel requires a for region");
                        statement->is_parallel = 1;
                        statement->is_gpu = parallel_gpu;
                    }
                    if(statement != NULL && using_binding) {
                        statement->is_using = 1;
                        if(kind == ZIR_STMT_EXPR) {
                            char name[ZIR_NAME_MAX];
                            if(strlen(t) >= sizeof(name))
                                die_at(span, "using field path is too long");
                            copy_text(name, sizeof(name), t);
                            size_t length = strlen(name);
                            if(length > 0 && name[length - 1] == ';')
                                name[--length] = '\0';
                            trim_in_place(name);
                            if(!is_member_path_text(name))
                                die_at(span,
                                       "using needs a record binding or field path");
                            copy_text(statement->name,
                                      sizeof(statement->name), name);
                            copy_text(statement->type,
                                      sizeof(statement->type), using_filter);
                        }
                    }
                }
                depth += brace_delta;
                if(depth < 0)
                    depth = 0;
            }
        } else if(mode == TOP && t[0] != '\0') {
            die_at(Span(rel, line_no, 1), "invalid top-level declaration");
        }
    }
    if(mode != TOP)
        die_at(Span(rel, line_no, 1),
               "unterminated declaration or function body");
    if(tframe_count > 0 || type_frame_count > 0 ||
       body_mcount > 0)
        die_at(Span(rel, line_no, 1), "unterminated #if block");
    if(program_export)
        die_at(Span(rel, program_export_line, 1),
               "#program_export must precede a function declaration");
    module = &program->modules[0];
    if(module->using_count > 0)
        for(int i = 0; i < consts.count; i++) {
            const ZirConst *constant = &consts.items[i];
            if(!is_identifier_text(constant->expr) ||
               !strcmp(constant->expr, "true") ||
               !strcmp(constant->expr, "false") ||
               !strcmp(constant->expr, "null")) continue;
            int emitted = 0;
            for(int d = 0; d < module->define_count; d++)
                if(!strcmp(module->defines[d].name, constant->name)) {
                    emitted = 1;
                    break;
                }
            if(emitted) continue;
            ZirDefine *def = ModuleAddDefine(module, constant->name,
                constant->expr, Span(constant->path,
                                      constant->source_line, 1));
            if(def == NULL)
                die_at(Span(constant->path, constant->source_line, 1),
                       "out of memory recording enum constant");
            def->is_public = constant->is_public;
            def->is_file_private = constant->is_file_private;
            def->requires_open_enum = 1;
        }
    for(int mi = 0; mi < program->module_count; mi++) {
        ZirModule *module = &program->modules[mi];
        add_default_helpers(program, module, path, root,
                            resolver, resolver_context);
        for(int gi = 0; gi < module->global_count; gi++) {
            ZirGlobal *global = &module->globals[gi];
            copy_text(module->lookup_path, sizeof(module->lookup_path),
                      global->span.path);
            if(find_unquoted_text(global->init, "#ifx") != NULL)
                lower_compile_ifx_value(global->init, sizeof(global->init),
                                        module, &consts, global->span, 1);
            if(find_unquoted_text(global->init, "#ifx") == NULL &&
               find_unquoted_word(global->init, "size_of") != NULL &&
               find_unquoted_word(global->init, "type_of") == NULL)
                lower_size_of_value(global->init, sizeof(global->init),
                                    module, global->span);
        }
        for(int fi = 0; fi < module->function_count; fi++) {
            ZirFunction *fn = &module->functions[fi];
            copy_text(module->lookup_path, sizeof(module->lookup_path),
                      fn->span.path);
            int has_compile_ifx = 0;
            for(int si = 0; si < fn->stmt_count; si++)
                if(find_unquoted_text(fn->stmts[si].text, "#ifx") != NULL) {
                    has_compile_ifx = 1;
                    break;
                }
            if(has_compile_ifx)
                lower_compile_ifx_function(fn, module, &consts, 1);
            if(!NormalizeJaiBodies(fn) || !BindJaiLoopControls(fn) ||
               !LowerCleanup(fn)) {
                ProgramFree(program);
                free(consts.items);
                free(future_constants.items);
                free(future_usings.items);
                free(future_imports.items);
                free(future_types.items);
                free_deferred_types(&deferred_types);
                free_discovered_functions(&future_functions);
                free(future_globals.items);
                free_discovered_files(&discovered_files);
                free(canonical);
                return NULL;
            }
            if(!LowerJaiFor(fn, module)) {
                ProgramFree(program);
                free(consts.items);
                free(future_constants.items);
                free(future_usings.items);
                free(future_imports.items);
                free(future_types.items);
                free_deferred_types(&deferred_types);
                free_discovered_functions(&future_functions);
                free(future_globals.items);
                free_discovered_files(&discovered_files);
                free(canonical);
                return NULL;
            }
            StructureFunction(fn, module);
        }
        module->lookup_path[0] = '\0';
    }
    free(consts.items);
    free(future_constants.items);
    free(future_usings.items);
    free(future_imports.items);
    free(future_types.items);
    free_deferred_types(&deferred_types);
    free_discovered_functions(&future_functions);
    free(future_globals.items);
    free_discovered_files(&discovered_files);
    free(canonical);
    return program;
}

ZirProgram *
parse_file_with_imports(const char *path, const char *root,
                        ZirCompileImportResolver resolver, void *context)
{
    return parse_file_with_imports_defined(path, root, resolver, context,
                                           NULL, 0);
}

ZirProgram *
parse_file_with_imports_defined(const char *path, const char *root,
                                ZirCompileImportResolver resolver,
                                void *context, const char *const *defines,
                                int define_count)
{
    char *lowered = read_lowered_source(path);
    ZirProgram *program = parse_source(path, root, lowered,
                                       resolver, context,
                                       defines, define_count);
    free(lowered);
    return program;
}

ZirProgram *
parse_file(const char *path, const char *root)
{
    return parse_file_with_imports(path, root, NULL, NULL);
}

ZirProgram *
parse_source_text(const char *path, const char *source)
{
    char *lowered = lower_jai_multiline_strings(source, path);
    ZirProgram *program = parse_source(path, ".", lowered, NULL, NULL,
                                       NULL, 0);
    free(lowered);
    return program;
}
