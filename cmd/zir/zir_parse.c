#include "zir_parse_internal.h"

void
die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    DiagnosticV(Span("", 0, 0), "parse.fatal", fmt, ap);
    va_end(ap);
    exit(1);
}

void
die_at(ZirSourceSpan span, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    DiagnosticV(span, "parse.syntax", fmt, args);
    va_end(args);
    exit(1);
}

void
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

int
source_identifier_byte(unsigned char byte)
{
    return isalnum(byte) || byte == '_';
}

/* Rewrite Jai's raw multiline token as a checked ordinary string literal.
 * Keep the original number of line breaks so later source spans stay put. */
char *
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

int
starts_word(const char *s, const char *word)
{
    size_t n = strlen(word);

    return strncmp(s, word, n) == 0 &&
           (s[n] == '\0' || s[n] == ' ' || s[n] == '\t' ||
            s[n] == '(' || s[n] == '"' || s[n] == '{');
}

int
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

int
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
int
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

int
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

void
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

int
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

int
source_column_for_trimmed(const char *line, const char *trimmed)
{
    if(line == NULL || trimmed == NULL || trimmed < line)
        return 1;
    return (int)(trimmed - line) + 1;
}

int
source_end_column_for_trimmed(const char *line, const char *trimmed)
{
    return source_column_for_trimmed(line, trimmed) + (int)strlen(trimmed);
}

int
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
int
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
int
line_is_abi_incomplete(const char *line, size_t length)
{
    static const char directive[] = "#abi_incomplete";
    const char *start = line;
    const char *end = line + length;

    while(start < end && (*start == ' ' || *start == '\t' ||
                          *start == '\r'))
        start++;
    while(end > start && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r'))
        end--;
    if(end > start && end[-1] == ';')
        end--;
    while(end > start && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r'))
        end--;
    return (size_t)(end - start) == sizeof(directive) - 1 &&
        strncmp(start, directive, sizeof(directive) - 1) == 0;
}

int
take_abi_incomplete(ZirType *type)
{
    char body[sizeof(type->body)];
    const char *line = type->body;
    size_t used = 0;
    int found = type->is_abi_incomplete != 0;

    while(*line != '\0') {
        const char *nl = strchr(line, '\n');
        size_t length = nl != NULL ? (size_t)(nl - line) + 1 : strlen(line);
        int skip = 0;

        if(strstr(line, "#abi_incomplete") != NULL) {
            if(found || !line_is_abi_incomplete(line, length))
                return 0;
            found = 1;
            skip = 1;
        }
        if(!skip) {
            if(used + length >= sizeof(body))
                return 0;
            memcpy(body + used, line, length);
            used += length;
        }
        line = nl != NULL ? nl + 1 : line + length;
    }
    body[used] = '\0';
    copy_text(type->body, sizeof(type->body), body);
    type->is_abi_incomplete = found;
    return 1;
}

int
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

const char *
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

int
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

int
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

ZirExternKind
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
int
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

int
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

ZirStmtKind
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
    if(starts_word(s, "break") || strcmp(s, "break;") == 0)
        return ZIR_STMT_BREAK;
    if(starts_word(s, "continue") || strcmp(s, "continue;") == 0)
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

int
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
char *
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

void
prepend_logical_line(char queue[16][SOURCE_LINE_MAX * 2], int *count,
                     const char *line, ZirSourceSpan span)
{
    if(*count >= 16 || strlen(line) >= sizeof(queue[0]))
        die_at(span, "too many statements on one source line");
    memmove(queue[1], queue[0], (size_t)*count * sizeof(queue[0]));
    copy_text(queue[0], sizeof(queue[0]), line);
    (*count)++;
}

int
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

const char *
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
