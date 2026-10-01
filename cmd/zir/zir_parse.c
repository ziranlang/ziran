#include "zir_parse_internal.h"
#include "compiler_source.h"

static String
source_view(const char *source)
{
    return StringView(source != NULL ? source : "", source != NULL ? strlen(source) : 0);
}

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

int
source_identifier_byte(unsigned char byte)
{
    return compiler_source_SourceIdentifier(byte);
}

/* C owns parser storage and diagnostics; scanning rules live in Ziran. */
char *
lower_jai_multiline_strings(const char *source, const char *path)
{
    String input = source_view(source);
    SourceRewrite result = compiler_source_RewriteRawStrings(input, (Slice){0}, ZIR_NAME_MAX);
    ZirSourceSpan span = Span(path, result.line, result.column);
    if(result.error == SourceError_Delimiter)
        die_at(span, "#string requires an identifier delimiter");
    if(result.error == SourceError_DelimiterLine)
        die_at(span, "#string delimiter must end its line");
    if(result.error == SourceError_Unterminated)
        die_at(span, "unterminated #string delimiter: %.*s",
               (int)(result.marker_end - result.marker_begin),
               source + result.marker_begin);
    if(result.count < 0 || (uint64_t)result.count >= SIZE_MAX)
        die("source exceeds available memory");
    char *output = malloc((size_t)result.count + 1);
    if(output == NULL) die("out of memory reading source");
    compiler_source_RewriteRawStrings(input, (Slice){output, result.count}, ZIR_NAME_MAX);
    output[result.count] = '\0';
    return output;
}

int
starts_word(const char *s, const char *word)
{
    return compiler_source_StartsWord(source_view(s), source_view(word));
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
    return compiler_source_IdentifierText(source_view(text), false);
}

int
TemplateParameterIndex(const char *list, const char *name, size_t length)
{
    int index = 0;
    for(const char *p = list; *p;) {
        const char *end = strchr(p, ',');
        size_t part = end ? (size_t)(end - p) : strlen(p);
        if(part == length && !strncmp(p, name, length))
            return index;
        if(end == NULL)
            break;
        p = end + 1;
        index++;
    }
    return -1;
}

int
TemplateBinderPrefix(const char *type)
{
    int prefix = !strncmp(type, "[]", 2) ? 2 : *type == '*' ? 1 : 0;
    if(type[0] == '[' && type[1] != ']') {
        const char *close = strchr(type, ']');
        if(close == NULL || !ArrayElementType(type, NULL, 0, NULL) ||
           memchr(type, '$', (size_t)(close - type)) != NULL)
            return -1;
        prefix = (int)(skip_ws(close + 1) - type);
    }
    return type[prefix] == '$' ? prefix : -1;
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
    return compiler_source_IdentifierText(source_view(text), true);
}

/* Buffers parse_file_global keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ParseFileGlobalBuffers {
    char type[ZIR_TEXT_MAX];
    char init[ZIR_TEXT_MAX];
} ParseFileGlobalBuffers;

void parse_file_global(ZirModule *module, const char *declaration,
                  ZirSourceSpan span, int scope_public, int scope_file,
                  char *name_out, size_t name_size);

static void
parse_file_global_with_buffers(ZirModule *module, const char *declaration,
                  ZirSourceSpan span, int scope_public, int scope_file,
                  char *name_out, size_t name_size, ParseFileGlobalBuffers *buffers)
{
    char name[ZIR_NAME_MAX];
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
    while(value < type_end && length + 1 < sizeof(buffers->type))
        buffers->type[length++] = *value++;
    if(value != type_end)
        die_at(span, "file-scope variable type is too long");
    while(length > 0 && (isspace((unsigned char)buffers->type[length - 1]) ||
                         buffers->type[length - 1] == ';')) length--;
    buffers->type[length] = '\0';
    if(buffers->type[0] == '\0')
        die_at(span, "file-scope variable needs a type");
    length = 0;
    if(equal != NULL) {
        value = skip_ws(equal + 1);
        while(*value != '\0' && length + 1 < sizeof(buffers->init))
            buffers->init[length++] = *value++;
        if(*value != '\0')
            die_at(span, "file-scope variable initializer is too long");
        while(length > 0 && (isspace((unsigned char)buffers->init[length - 1]) ||
                             buffers->init[length - 1] == ';')) length--;
        if(length == 0)
            die_at(span, "file-scope variable initializer is empty");
    }
    buffers->init[length] = '\0';
    if(scope_public)
        ModuleAddGlobal(module, name, buffers->type, buffers->init, span);
    else
        ModuleAddStatic(module, name, buffers->type, buffers->init, span);
    if(module->global_count > 0)
        module->globals[module->global_count - 1].is_file_private = scope_file;
    if(name_out != NULL)
        copy_text(name_out, name_size, name);
}

void
parse_file_global(ZirModule *module, const char *declaration,
                  ZirSourceSpan span, int scope_public, int scope_file,
                  char *name_out, size_t name_size)
{
    static _Thread_local ParseFileGlobalBuffers *spares[16];
    static _Thread_local int spare_count;
    ParseFileGlobalBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    parse_file_global_with_buffers(module, declaration, span, scope_public, scope_file, name_out, name_size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

int
contains_source_directive(const char *source, const char *directive)
{
    return compiler_source_ContainsDirective(source_view(source), source_view(directive));
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
/* Buffers take_abi_incomplete keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct TakeAbiIncompleteBuffers {
    char body[sizeof(((ZirType *)0)->body)];
} TakeAbiIncompleteBuffers;

int take_abi_incomplete(ZirType *type);

void
normalize_record_separators(char *body, int split_commas)
{
    int quote = 0, nesting = 0;
    for(char *cursor = body; *cursor; cursor++) {
        if(quote) {
            if(*cursor == '\\' && cursor[1])
                cursor++;
            else if(*cursor == '"')
                quote = 0;
        } else if(*cursor == '"') {
            quote = 1;
        } else if(*cursor == '[' || *cursor == '(') {
            nesting++;
        } else if(*cursor == ']' || *cursor == ')') {
            nesting--;
        } else if(nesting == 0 &&
                  (*cursor == ';' || (split_commas && *cursor == ','))) {
            *cursor = '\n';
        }
    }
}

static int
take_abi_incomplete_with_buffers(ZirType *type, TakeAbiIncompleteBuffers *buffers)
{
    const char *line = type->body;
    size_t used = 0;
    int found = type->is_abi_incomplete != 0;
    while(*line != '\0') {
        const char *nl = strchr(line, '\n');
        size_t length = nl != NULL ? (size_t)(nl - line) + 1 : strlen(line);
        int skip = 0;
        if(find_unquoted_text(line, "#abi_incomplete") != NULL) {
            if(found || !line_is_abi_incomplete(line, length))
                return 0;
            found = 1;
            skip = 1;
        }
        if(!skip) {
            if(used + length >= sizeof(buffers->body))
                return 0;
            memcpy(buffers->body + used, line, length);
            used += length;
        }
        line = nl != NULL ? nl + 1 : line + length;
    }
    buffers->body[used] = '\0';
    copy_text(type->body, sizeof(type->body), buffers->body);
    type->is_abi_incomplete = found;
    return 1;
}

int
take_abi_incomplete(ZirType *type)
{
    static _Thread_local TakeAbiIncompleteBuffers *spares[16];
    static _Thread_local int spare_count;
    TakeAbiIncompleteBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = take_abi_incomplete_with_buffers(type, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers expand_type_this keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ExpandTypeThisBuffers {
    char body[sizeof(((ZirType *)0)->body)];
} ExpandTypeThisBuffers;

int expand_type_this(ZirType *type);

static int
expand_type_this_with_buffers(ZirType *type, ExpandTypeThisBuffers *buffers)
{
    char replacement[ZIR_NAME_MAX];
    size_t used = 0;
    int quote = 0;
    int length = snprintf(replacement, sizeof(replacement), "%s%s%s%s",
        type->name, type->template_params[0] ? "(" : "",
        type->template_params, type->template_params[0] ? ")" : "");
    if(length < 0 || (size_t)length >= sizeof(replacement)) return 0;
    for(const char *cursor = type->body; *cursor; ) {
        if(!quote && strncmp(cursor, "#this", 5) == 0 &&
           !isalnum((unsigned char)cursor[5]) && cursor[5] != '_') {
            if(used + (size_t)length >= sizeof(buffers->body)) return 0;
            memcpy(buffers->body + used, replacement, (size_t)length);
            used += (size_t)length;
            cursor += 5;
            continue;
        }
        if(used + 1 >= sizeof(buffers->body)) return 0;
        if(quote && *cursor == '\\' && cursor[1]) {
            if(used + 2 >= sizeof(buffers->body)) return 0;
            buffers->body[used++] = *cursor++;
            buffers->body[used++] = *cursor++;
            continue;
        }
        if(*cursor == '"' || *cursor == '\'') {
            if(!quote) quote = *cursor;
            else if(quote == *cursor) quote = 0;
        }
        buffers->body[used++] = *cursor++;
    }
    buffers->body[used] = '\0';
    copy_text(type->body, sizeof(type->body), buffers->body);
    return 1;
}

int
expand_type_this(ZirType *type)
{
    static _Thread_local ExpandTypeThisBuffers *spares[16];
    static _Thread_local int spare_count;
    ExpandTypeThisBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = expand_type_this_with_buffers(type, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
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
    if(strncmp(target, "go:", 3) == 0) {
        if(!GoForeignCallParts(target, NULL, 0, NULL, 0, NULL, 0))
            die_at(Span(path, line_no, 1),
                   "Go extern target requires a package path and valid symbol");
        return ZIR_EXTERN_GO;
    }
    if(strncmp(target, "py:", 3) == 0) {
        if(!PyForeignCallParts(target, NULL, 0, NULL, 0, NULL, 0))
            die_at(Span(path, line_no, 1),
                   "Python extern target requires a module path and valid symbol");
        return ZIR_EXTERN_PY;
    }
    if(dot != NULL && slash != NULL && slash < dot)
        return ZIR_EXTERN_GO;
    return ZIR_EXTERN_HOST;
}

int
net_block_braces(const char *s)
{
    return compiler_source_NetBlockBraces(source_view(s));
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
    if(starts_word(s, "return") || strcmp(s, "return;") == 0)
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

char *
statement_separator(char *line)
{
    int64_t at = compiler_source_StatementSeparator(source_view(line));
    return at < 0 ? NULL : line + at;
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
    String input = source_view(t);
    ControlBlock block = compiler_source_SplitControlBlock(input);
    if(!block.valid || (uint64_t)block.head_end >= hsz ||
       (uint64_t)(block.body_end - block.body_begin) >= bsz ||
       (uint64_t)(input.length - block.tail_begin) >= tsz)
        return 0;
    memcpy(head, t, (size_t)block.head_end);
    head[block.head_end] = '\0';
    size_t body_length = (size_t)(block.body_end - block.body_begin);
    memcpy(body, t + block.body_begin, body_length);
    body[body_length] = '\0';
    trim_in_place(body);
    copy_text(tail, tsz, t + block.tail_begin);
    return 1;
}

const char *
closing_parenthesis(const char *open)
{
    int64_t at = compiler_source_ClosingParenthesis(source_view(open));
    return at < 0 ? NULL : open + at;
}
