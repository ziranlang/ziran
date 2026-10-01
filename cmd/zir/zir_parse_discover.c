#include "zir_parse_internal.h"

static void canonical_enum_values(ZirType *type);
/* Buffers normalize_jai_source_tokens keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct NormalizeJaiSourceTokensBuffers {
    char normalized[SOURCE_LINE_MAX];
    char literal[ZIR_PATH_MAX * 4 + 4];
    char directory[ZIR_PATH_MAX];
} NormalizeJaiSourceTokensBuffers;

void normalize_jai_source_tokens(char *line, const char *path,
                            const char *physical_path, int line_no);

/* Set when a line used [..]T, so the module imports std/vec for Vec. */
_Thread_local int ZirSourceUsesResizableArrays;

/* Length of the type written at TEXT, such as `*Node`, `[4]u8`, or
 * `Pair(s32, s64)`, including leading spaces; 0 when there is none. */
static size_t
type_text_length(const char *text)
{
    const char *p = text;
    while(*p == ' ' || *p == '\t') p++;
    for(;;) {
        if(*p == '*') {
            p++;
            while(*p == ' ' || *p == '\t') p++;
        } else if(*p == '[') {
            int depth = 0;
            do {
                if(*p == '[') depth++;
                else if(*p == ']') depth--;
                else if(*p == '\0') return 0;
                p++;
            } while(depth > 0);
            while(*p == ' ' || *p == '\t') p++;
        } else
            break;
    }
    const char *name = p;
    while(isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '$') p++;
    if(p == name) return 0;
    const char *after = p;
    while(*after == ' ' || *after == '\t') after++;
    if(*after == '(') {
        int depth = 0;
        p = after;
        do {
            if(*p == '(') depth++;
            else if(*p == ')') depth--;
            else if(*p == '\0') return 0;
            p++;
        } while(depth > 0);
    }
    return (size_t)(p - text);
}

static void
normalize_jai_source_tokens_with_buffers(char *line, const char *path,
                            const char *physical_path, int line_no, NormalizeJaiSourceTokensBuffers *buffers)
{
    static const struct { const char *jai, *internal; } names[] = {
        {"int", "s64"}, {"float", "float32"}
    };
    static const char *const old[] = {
        "i8", "i16", "i32", "i64", "f32", "f64", "double", NULL
    };
    size_t used = 0;
    for(const char *p = line; *p; ) {
        if(*p == '/' && p[1] == '/') {
            size_t remaining = strlen(p);
            if(used + remaining >= sizeof(buffers->normalized))
                die_at(Span(path, line_no, 1), "source line exceeds size limit");
            memcpy(buffers->normalized + used, p, remaining + 1);
            used += remaining;
            break;
        }
        if(*p == '"' || *p == '\'') {
            char quote = *p;
            if(used + 1 >= sizeof(buffers->normalized))
                die_at(Span(path, line_no, 1), "source line exceeds size limit");
            buffers->normalized[used++] = *p++;
            while(*p) {
                char next = *p++;
                if(used + 1 >= sizeof(buffers->normalized))
                    die_at(Span(path, line_no, 1), "source line exceeds size limit");
                buffers->normalized[used++] = next;
                if(next == '\\' && *p) {
                    if(used + 1 >= sizeof(buffers->normalized))
                        die_at(Span(path, line_no, 1), "source line exceeds size limit");
                    buffers->normalized[used++] = *p++;
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
            int written;
            if(strcmp(directive, "#line") == 0)
                written = snprintf(buffers->literal, sizeof(buffers->literal), "%d", line_no);
            else {
                const char *value = physical_path;
                if(strcmp(directive, "#filepath") == 0) {
                    const char *slash = strrchr(physical_path, '/');
                    size_t length = slash == NULL ? 0 :
                                    slash == physical_path ? 1 :
                                    (size_t)(slash - physical_path);
                    if(length >= sizeof(buffers->directory))
                        die_at(Span(path, line_no, 1),
                               "source filepath exceeds size limit");
                    if(length == 0)
                        copy_text(buffers->directory, sizeof(buffers->directory), ".");
                    else {
                        memcpy(buffers->directory, physical_path, length);
                        buffers->directory[length] = '\0';
                    }
                    value = buffers->directory;
                }
                buffers->literal[0] = '"';
                size_t escaped = escape_c_string(value, buffers->literal + 1,
                                                 sizeof(buffers->literal) - 2);
                buffers->literal[escaped + 1] = '"';
                buffers->literal[escaped + 2] = '\0';
                written = (int)escaped + 2;
            }
            if(written < 0 ||
               (size_t)written >= sizeof(buffers->normalized) - used)
                die_at(Span(path, line_no, 1),
                       "source line exceeds size limit");
            memcpy(buffers->normalized + used, buffers->literal, (size_t)written);
            used += (size_t)written;
            p += directive_length;
            continue;
        }
        if(*p == '[') {
            /* Jai's resizable array [..]T is the owned Vec(T). */
            const char *q = p + 1;
            while(*q == ' ' || *q == '\t') q++;
            if(q[0] == '.' && q[1] == '.' && q[2] != '.') {
                q += 2;
                while(*q == ' ' || *q == '\t') q++;
                if(*q == ']') {
                    size_t element_length = type_text_length(q + 1);
                    char element[SOURCE_LINE_MAX];
                    if(element_length == 0 || element_length >= sizeof(element))
                        die_at(Span(path, line_no, (int)(p - line) + 1),
                               "[..] needs an element type");
                    memcpy(element, q + 1, element_length);
                    element[element_length] = '\0';
                    normalize_jai_source_tokens(element, path, physical_path, line_no);
                    int written = snprintf(buffers->normalized + used,
                                           sizeof(buffers->normalized) - used,
                                           "Vec(%s)", skip_ws(element));
                    if(written < 0 || (size_t)written >= sizeof(buffers->normalized) - used)
                        die_at(Span(path, line_no, 1), "source line exceeds size limit");
                    used += (size_t)written;
                    p = q + 1 + element_length;
                    ZirSourceUsesResizableArrays = 1;
                    continue;
                }
            }
        }
        if(isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while(isalnum((unsigned char)*p) || *p == '_') p++;
            size_t length = (size_t)(p - start);
            const char *replacement = NULL;
            /* Jai's array procedures on a resizable array take its address:
             * array_add(*values, x) is VecPush(values, x). */
            static const struct { const char *jai, *vec; } array_calls[] = {
                {"array_add", "VecPush"}, {"array_reset", "VecFree"},
                {"array_free", "VecFree"},
                {"array_reset_keeping_memory", "VecClear"}
            };
            const char *after = p;
            while(*after == ' ' || *after == '\t') after++;
            for(size_t i = 0; *after == '(' &&
                i < sizeof(array_calls) / sizeof(array_calls[0]); i++)
                if(strlen(array_calls[i].jai) == length &&
                   strncmp(start, array_calls[i].jai, length) == 0) {
                    size_t vec_length = strlen(array_calls[i].vec);
                    if(used + vec_length + 1 >= sizeof(buffers->normalized))
                        die_at(Span(path, line_no, 1), "source line exceeds size limit");
                    memcpy(buffers->normalized + used, array_calls[i].vec, vec_length);
                    used += vec_length;
                    buffers->normalized[used++] = '(';
                    p = skip_ws(after + 1);
                    if(*p == '*')
                        p++;
                    ZirSourceUsesResizableArrays = 1;
                    replacement = "";
                    break;
                }
            if(replacement != NULL)
                continue;
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
            if(used + length >= sizeof(buffers->normalized))
                die_at(Span(path, line_no, 1), "source line exceeds size limit");
            memcpy(buffers->normalized + used, start, length);
            used += length;
            continue;
        }
        if(used + 1 >= sizeof(buffers->normalized))
            die_at(Span(path, line_no, 1), "source line exceeds size limit");
        buffers->normalized[used++] = *p++;
    }
    buffers->normalized[used] = '\0';
    copy_text(line, SOURCE_LINE_MAX, buffers->normalized);
}

/* Canonicalize Jai's scalar aliases and source-position expressions before
 * parsing declarations. Quoted text and comments are left intact. */
void
normalize_jai_source_tokens(char *line, const char *path,
                            const char *physical_path, int line_no)
{
    /* `operator + :: (a: V, b: V) -> V` declares the procedure operator_add;
     * several such declarations are overloads of it. */
    char *start = (char *)skip_inline_ws(line);
    if(strncmp(start, "operator", 8) == 0 && !is_ident_char((unsigned char)start[8])) {
        const char *op = skip_inline_ws(start + 8);
        size_t length = OperatorTokenLength(op);
        const char *after = skip_inline_ws(op + length);
        if(length > 0 && strncmp(after, "::", 2) == 0) {
            char token[4], rewritten[SOURCE_LINE_MAX * 2];
            memcpy(token, op, length);
            token[length] = '\0';
            int written = snprintf(rewritten, sizeof(rewritten), "%.*s%s %s",
                                   (int)(start - line), line,
                                   OperatorProcedureName(token), after);
            if(written < 0 || (size_t)written >= SOURCE_LINE_MAX)
                die_at(Span(path, line_no, 1), "source line exceeds size limit");
            strcpy(line, rewritten);
        } else if(length == 0 && *op != ':' && strstr(start, "::") != NULL) {
            die_at(Span(path, line_no, 1),
                   "operator procedures are written operator + :: (a: T, b: T) -> T, "
                   "for + - * / %% == != < <= > >= & | ^ << >>");
        }
    }
    static _Thread_local NormalizeJaiSourceTokensBuffers *spares[16];
    static _Thread_local int spare_count;
    NormalizeJaiSourceTokensBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    normalize_jai_source_tokens_with_buffers(line, path, physical_path, line_no, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

/* File and embedded declarations share the complete frontend. A line that
 * does not fit whole is refused: silently splitting it would change meaning. */
char *
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
/* Buffers discover_named_type keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct DiscoverNamedTypeBuffers {
    char normalized[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX];
    char line[SOURCE_LINE_MAX];
    ZirType type;
    char lowered[sizeof(((ZirType *)0)->body)];
} DiscoverNamedTypeBuffers;

static void discover_named_type(const char *source, const char *path, const char *rel,
                    int line_no,
                    int scope_public, int scope_file, ZirTypes *future);

static void
discover_named_type_with_buffers(const char *source, const char *path, const char *rel,
                    int line_no,
                    int scope_public, int scope_file, ZirTypes *future, DiscoverNamedTypeBuffers *buffers)
{
    buffers->normalized[0] = '\0';
    size_t normalized_length = 0;
    int physical_line = line_no;
    if(contains_source_directive(source, "#if")) return;
    for(const char *cursor = source; *cursor;) {
        const char *end = strchr(cursor, '\n');
        size_t length = end == NULL ? strlen(cursor) :
                        (size_t)(end - cursor);
        if(length >= sizeof(buffers->line)) return;
        memcpy(buffers->line, cursor, length);
        buffers->line[length] = '\0';
        normalize_jai_source_tokens(buffers->line, rel, path, physical_line++);
        length = strlen(buffers->line);
        if(normalized_length + length + 2 >= sizeof(buffers->normalized)) return;
        memcpy(buffers->normalized + normalized_length, buffers->line, length);
        normalized_length += length;
        buffers->normalized[normalized_length++] = '\n';
        buffers->normalized[normalized_length] = '\0';
        cursor = end == NULL ? cursor + strlen(cursor) : end + 1;
    }
    source = buffers->normalized;
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
    memset(&buffers->type, 0, sizeof(buffers->type));
    size_t name_length = (size_t)(colons - source);
    if(name_length == 0 || name_length >= sizeof(buffers->type.name)) return;
    memcpy(buffers->type.name, source, name_length);
    buffers->type.name[name_length] = '\0';
    trim_in_place(buffers->type.name);
    if(!is_identifier_text(buffers->type.name)) return;
    buffers->type.span = Span(rel, line_no, 1);
    buffers->type.is_public = scope_public;
    buffers->type.is_file_private = scope_file;
    buffers->type.is_union = starts_word(after, "union");
    buffers->type.is_enum_flags = starts_word(after, "enum_flags");
    buffers->type.is_enum = buffers->type.is_enum_flags || starts_word(after, "enum");
    if(buffers->type.is_enum)
        parse_enum_backing(&buffers->type, after);
    if(!buffers->type.is_enum &&
       !parse_type_parameters(after, buffers->type.is_union ? "union" : "struct",
                              buffers->type.template_params,
                              sizeof(buffers->type.template_params))) return;
    buffers->type.is_record_template = buffers->type.template_params[0] != '\0';
    size_t body_length = (size_t)(close - open - 1);
    if(body_length >= sizeof(buffers->type.body)) return;
    memcpy(buffers->type.body, open + 1, body_length);
    buffers->type.body[body_length] = '\0';
    if(buffers->type.is_enum) {
        if(buffers->type.is_enum_flags) {
            for(size_t i = 0; i < body_length; i++)
                if(buffers->type.body[i] == ';') buffers->type.body[i] = '\n';
            lower_enum_values(&buffers->type);
        } else {
            size_t used = 0;
            for(const char *p = buffers->type.body; *p; p++) {
                if(p[0] == ':' && p[1] == ':') {
                    if(used + 3 >= sizeof(buffers->lowered)) return;
                    memcpy(buffers->lowered + used, " = ", 3);
                    used += 3;
                    p++;
                } else {
                    if(used + 1 >= sizeof(buffers->lowered)) return;
                    buffers->lowered[used++] = *p == ';' ? '\n' : *p;
                }
            }
            buffers->lowered[used] = '\0';
            copy_text(buffers->type.body, sizeof(buffers->type.body), buffers->lowered);
            if(!EnumMemberValue(&buffers->type, NULL, NULL)) return;
            canonical_enum_values(&buffers->type);
        }
    } else if(buffers->type.is_record_template) {
        normalize_record_separators(buffers->type.body, 0);
    }
    if(!buffers->type.is_enum && !take_abi_incomplete(&buffers->type))
        return;
    if(!expand_type_this(&buffers->type)) return;
    if(future->count == future->capacity) {
        int capacity = future->capacity > 0 ? future->capacity * 2 : 8;
        ZirType *items = realloc(future->items,
                                 (size_t)capacity * sizeof(*items));
        if(items == NULL) die("out of memory discovering types");
        future->items = items;
        future->capacity = capacity;
    }
    future->items[future->count++] = buffers->type;
}

static void
discover_named_type(const char *source, const char *path, const char *rel,
                    int line_no,
                    int scope_public, int scope_file, ZirTypes *future)
{
    static _Thread_local DiscoverNamedTypeBuffers *spares[16];
    static _Thread_local int spare_count;
    DiscoverNamedTypeBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    discover_named_type_with_buffers(source, path, rel, line_no, scope_public, scope_file, future, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
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

void
free_deferred_types(ZirDeferredTypes *deferred)
{
    for(int i = 0; i < deferred->count; i++) {
        free(deferred->items[i].source);
        free(deferred->items[i].path);
    }
    free(deferred->items);
}
/* Buffers discover_function_header keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct DiscoverFunctionHeaderBuffers {
    char line[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX];
    char args[ZIR_TEXT_MAX];
} DiscoverFunctionHeaderBuffers;

static void discover_function_header(const char *source, const char *path,
                         const char *rel, int line_no,
                         int scope_public, int scope_file,
                         ZirFunctions *future);

static void
discover_function_header_with_buffers(const char *source, const char *path,
                         const char *rel, int line_no,
                         int scope_public, int scope_file,
                         ZirFunctions *future, DiscoverFunctionHeaderBuffers *buffers)
{
    if(!looks_like_function_header(source) ||
       (strchr(source, '{') == NULL &&
        strstr(source, "#foreign") == NULL)) return;
    char name[ZIR_NAME_MAX], result[ZIR_NAME_MAX];
    copy_text(buffers->line, sizeof(buffers->line), source);
    normalize_jai_source_tokens(buffers->line, rel, path, line_no);
    if(closing_parenthesis(strchr(buffers->line, '(')) == NULL) return;
    parse_function_header(name, sizeof(name), buffers->args, sizeof(buffers->args),
                          result, sizeof(result), buffers->line);
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
    function->args_text = KeepParameters(buffers->args);
    copy_text(function->return_type, sizeof(function->return_type), result);
    function->span = Span(rel, line_no, 1);
    function->is_public = scope_public;
    function->is_file_private = scope_file;
}

static void
discover_function_header(const char *source, const char *path,
                         const char *rel, int line_no,
                         int scope_public, int scope_file,
                         ZirFunctions *future)
{
    static _Thread_local DiscoverFunctionHeaderBuffers *spares[16];
    static _Thread_local int spare_count;
    DiscoverFunctionHeaderBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    discover_function_header_with_buffers(source, path, rel, line_no, scope_public, scope_file, future, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
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
                       Span(SpanPath(function->span), line, 1)) == NULL)
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
                                   Span(SpanPath(function->span), line, 1)) == NULL)
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

void
free_discovered_files(ZirDiscoveredFiles *files)
{
    for(int i = 0; i < files->count; i++)
        free(files->paths[i]);
    free(files->paths);
}

void
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
/* Buffers discover_file_scope keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct DiscoverFileScopeBuffers {
    char line[SOURCE_LINE_MAX];
    char type_source[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX];
    char function_source[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX];
    char simple_body[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX];
    char requested[SOURCE_PATH_MAX];
    char candidate[SOURCE_PATH_MAX * 2];
    char loaded_rel[SOURCE_PATH_MAX];
    ZirModule discovered;
} DiscoverFileScopeBuffers;

void discover_file_scope(const char *source, const char *path, const char *rel,
                    const char *root,
                    ZirConsts *future_constants, ZirUsings *future_usings,
                    ZirImports *future_imports, ZirTypes *future_types,
                    ZirDeferredTypes *deferred_types,
                    ZirFunctions *future_functions,
                    ZirGlobals *future_globals,
                    ZirDiscoveredFiles *files, int depth);

static void
discover_file_scope_with_buffers(const char *source, const char *path, const char *rel,
                    const char *root,
                    ZirConsts *future_constants, ZirUsings *future_usings,
                    ZirImports *future_imports, ZirTypes *future_types,
                    ZirDeferredTypes *deferred_types,
                    ZirFunctions *future_functions,
                    ZirGlobals *future_globals,
                    ZirDiscoveredFiles *files, int depth, DiscoverFileScopeBuffers *buffers)
{
    if(depth >= 32 || !remember_discovered_file(files, path)) return;
    buffers->type_source[0] = '\0';
    buffers->function_source[0] = '\0';
    buffers->simple_body[0] = '\0';
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
    while(read_source_line(buffers->line, sizeof(buffers->line), &source, path, line_no) != NULL) {
        line_no++;
        strip_block_comments(buffers->line, &comment_depth);
        char *t = trim(buffers->line);
        int brace_delta = net_block_braces(t);
        if(simple_body_index >= 0) {
            append_type_source(buffers->simple_body, sizeof(buffers->simple_body), t,
                               &simple_body_overflow);
            if(brace_depth + brace_delta == 0) {
                if(!simple_body_overflow)
                    discover_compile_body(
                        &future_functions->items[simple_body_index],
                        buffers->simple_body, simple_body_start_line);
                simple_body_index = -1;
            }
        } else if(collecting_type ||
           (brace_depth == 0 && is_named_type_header(t))) {
            if(!collecting_type) {
                collecting_type = 1;
                type_overflow = 0;
                buffers->type_source[0] = '\0';
                type_start_line = line_no;
                type_scope_file = scope_file;
                type_scope_public = scope_public;
            }
            append_type_source(buffers->type_source, sizeof(buffers->type_source), t,
                               &type_overflow);
            if(strchr(buffers->type_source, '{') != NULL &&
               brace_depth + brace_delta == 0) {
                if(!type_overflow) {
                    if(contains_source_directive(buffers->type_source, "#if"))
                        defer_conditional_type(deferred_types, buffers->type_source,
                                               path, rel, type_start_line,
                                               type_scope_public,
                                               type_scope_file);
                    else
                        discover_named_type(buffers->type_source, path, rel,
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
                buffers->function_source[0] = '\0';
                function_start_line = line_no;
                function_scope_file = scope_file;
                function_scope_public = scope_public;
            }
            append_type_source(buffers->function_source, sizeof(buffers->function_source), t,
                               &function_overflow);
            const char *closing = function_overflow ? NULL :
                closing_parenthesis(strchr(buffers->function_source, '('));
            if(closing != NULL &&
               (strchr(closing + 1, '{') != NULL ||
                strstr(closing + 1, "#foreign") != NULL)) {
                int previous = future_functions->count;
                discover_function_header(buffers->function_source, path, rel,
                                         function_start_line,
                                         function_scope_public,
                                         function_scope_file,
                                         future_functions);
                const char *open = strchr(closing + 1, '{');
                if(open != NULL && future_functions->count > previous) {
                    simple_body_index = previous;
                    simple_body_overflow = 0;
                    simple_body_start_line = line_no;
                    buffers->simple_body[0] = '\0';
                    append_type_source(buffers->simple_body, sizeof(buffers->simple_body),
                                       open + 1, &simple_body_overflow);
                    if(brace_depth + brace_delta == 0) {
                        if(!simple_body_overflow)
                            discover_compile_body(
                                &future_functions->items[simple_body_index],
                                buffers->simple_body, simple_body_start_line);
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
                    size_t length = (size_t)(end - argument - 1);
                    if(length > 3 && length < sizeof(buffers->requested)) {
                        memcpy(buffers->requested, argument + 1, length);
                        buffers->requested[length] = '\0';
                        if(buffers->requested[0] != '/' &&
                           !strcmp(buffers->requested + length - 3, ".zi") &&
                           strchr(buffers->requested, '\\') == NULL) {
                            const char *slash = strrchr(path, '/');
                            int written = slash == NULL ?
                                snprintf(buffers->candidate, sizeof(buffers->candidate), "%s",
                                         buffers->requested) :
                                snprintf(buffers->candidate, sizeof(buffers->candidate),
                                         "%.*s/%s", (int)(slash - path),
                                         path, buffers->requested);
                            if(written > 0 &&
                               (size_t)written < sizeof(buffers->candidate)) {
                                char *loaded_path = realpath(buffers->candidate, NULL);
                                if(loaded_path != NULL) {
                                    copy_text(buffers->loaded_rel, sizeof(buffers->loaded_rel),
                                              relative_path(root, loaded_path));
                                    char *loaded = read_lowered_source(
                                        loaded_path);
                                    discover_file_scope(loaded, loaded_path,
                                        buffers->loaded_rel, root, future_constants,
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
                if(starts_word(declaration, "#type") &&
                   starts_word(skip_ws(declaration + strlen("#type")), "#foreign")) {
                    char name[ZIR_NAME_MAX];
                    if(parse_symbol_before_colons(t, name, sizeof(name))) {
                        if(future_types->count == future_types->capacity) {
                            int capacity = future_types->capacity > 0 ?
                                future_types->capacity * 2 : 8;
                            ZirType *items = realloc(future_types->items,
                                (size_t)capacity * sizeof(*items));
                            if(items == NULL)
                                die("out of memory discovering foreign types");
                            future_types->items = items;
                            future_types->capacity = capacity;
                        }
                        ZirType *type = &future_types->items[future_types->count++];
                        memset(type, 0, sizeof(*type));
                        copy_text(type->name, sizeof(type->name), name);
                        type->is_extern = 1;
                        type->is_public = scope_public;
                        type->is_file_private = scope_file;
                        type->span = Span(rel, line_no, 1);
                    }
                }
                if(starts_word(declaration, "#import") ||
                   !strncmp(declaration, "#import,", 8)) {
                    const char *mode = skip_ws(declaration + 7);
                    int string_import = 0;
                    if(*mode == ',')
                        string_import = starts_word(skip_ws(mode + 1),
                                                    "string");
                    if(!string_import) {
                        memset(&buffers->discovered, 0, sizeof(buffers->discovered));
                        if(parse_import_line(&buffers->discovered, rel, line_no, t,
                                             scope_public) &&
                           buffers->discovered.import_count > 0) {
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
                            *import = buffers->discovered.imports[0];
                            import->is_file_private = scope_file;
                        }
                        free(buffers->discovered.imports);
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

/* Discover only unconditional file-scope declarations. Conditional and type
 * bodies stay opaque; supported procedure bodies are retained for bounded
 * compile-time evaluation. */
void
discover_file_scope(const char *source, const char *path, const char *rel,
                    const char *root,
                    ZirConsts *future_constants, ZirUsings *future_usings,
                    ZirImports *future_imports, ZirTypes *future_types,
                    ZirDeferredTypes *deferred_types,
                    ZirFunctions *future_functions,
                    ZirGlobals *future_globals,
                    ZirDiscoveredFiles *files, int depth)
{
    static _Thread_local DiscoverFileScopeBuffers *spares[16];
    static _Thread_local int spare_count;
    DiscoverFileScopeBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    discover_file_scope_with_buffers(source, path, rel, root, future_constants, future_usings, future_imports, future_types, deferred_types, future_functions, future_globals, files, depth, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}
/* Buffers discover_conditional_type keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct DiscoverConditionalTypeBuffers {
    char line[SOURCE_LINE_MAX];
    char selected[ZIR_TEXT_MAX * 2 + SOURCE_LINE_MAX];
    ZirCondFrame frames[8];
} DiscoverConditionalTypeBuffers;

static int discover_conditional_type(const ZirDeferredType *item,
                          ZirModule *module, const ZirConsts *constants,
                          const CompileParseContext *context,
                          ZirTypes *future, int allow_defer);

static int
discover_conditional_type_with_buffers(const ZirDeferredType *item,
                          ZirModule *module, const ZirConsts *constants,
                          const CompileParseContext *context,
                          ZirTypes *future, int allow_defer, DiscoverConditionalTypeBuffers *buffers)
{
    const char *source = item->source;
    buffers->selected[0] = '\0';
    memset(buffers->frames, 0, sizeof(buffers->frames));
    int frame_count = 0;
    int line_no = item->line - 1;
    int overflow = 0;
    while(read_source_line(buffers->line, sizeof(buffers->line), &source,
                           item->path, line_no) != NULL) {
        int pending = 0;
        line_no++;
        char *text = trim(buffers->line);
        int consumed = cond_top_step(text, buffers->frames, &frame_count,
                                     module, constants, context,
                                     item->path, item->rel, line_no,
                                     allow_defer ? &pending : NULL);
        if(pending) return 0;
        if(consumed || (frame_count > 0 && !buffers->frames[frame_count - 1].active))
            continue;
        append_type_source(buffers->selected, sizeof(buffers->selected), text, &overflow);
    }
    if(!overflow && frame_count == 0)
        discover_named_type(buffers->selected, item->path, item->rel,
                            item->line, item->is_public,
                            item->is_file_private, future);
    return 1;
}

static int
discover_conditional_type(const ZirDeferredType *item,
                          ZirModule *module, const ZirConsts *constants,
                          const CompileParseContext *context,
                          ZirTypes *future, int allow_defer)
{
    static _Thread_local DiscoverConditionalTypeBuffers *spares[16];
    static _Thread_local int spare_count;
    DiscoverConditionalTypeBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = discover_conditional_type_with_buffers(item, module, constants, context, future, allow_defer, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

void
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
/* Buffers canonical_enum_values keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CanonicalEnumValuesBuffers {
    char canonical[sizeof(((ZirType *)0)->body)];
} CanonicalEnumValuesBuffers;

static void canonical_enum_values(ZirType *type);

static void
canonical_enum_values_with_buffers(ZirType *type, CanonicalEnumValuesBuffers *buffers)
{
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
        int written = snprintf(buffers->canonical + used, sizeof(buffers->canonical) - used,
                               "%s = %lld\n", name, (long long)value);
        if(written < 0 || (size_t)written >= sizeof(buffers->canonical) - used)
            die_at(type->span, "enum body exceeds size limit");
        used += (size_t)written;
        while(*cursor && *cursor != ',' && *cursor != '\n') cursor++;
        if(*cursor) cursor++;
    }
    buffers->canonical[used] = '\0';
    copy_text(type->body, sizeof(type->body), buffers->canonical);
}

/* Jai constant declarations inside enums use 'Member :: value'. The checked
 * IR stores explicit values in the backend-neutral 'Member = value' form. */
static void
canonical_enum_values(ZirType *type)
{
    static _Thread_local CanonicalEnumValuesBuffers *spares[16];
    static _Thread_local int spare_count;
    CanonicalEnumValuesBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    canonical_enum_values_with_buffers(type, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}
/* Buffers lower_enum_values keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerEnumValuesBuffers {
    char lowered[sizeof(((ZirType *)0)->body)];
    char flags[sizeof(((ZirType *)0)->body)];
    char member[ZIR_TEXT_MAX];
} LowerEnumValuesBuffers;

void lower_enum_values(ZirType *type);

static void
lower_enum_values_with_buffers(ZirType *type, LowerEnumValuesBuffers *buffers)
{
    size_t used = 0;
    for(const char *p = type->body; *p; p++) {
        if(type->is_enum && *p == '=')
            die_at(type->span,
                   "enum members use Jai Member :: value syntax, not =");
        if(p[0] == ':' && p[1] == ':') {
            if(used + 3 >= sizeof(buffers->lowered))
                die_at(type->span, "enum body exceeds size limit");
            memcpy(buffers->lowered + used, " = ", 3);
            used += 3;
            p++;
        } else {
            if(used + 1 >= sizeof(buffers->lowered))
                die_at(type->span, "enum body exceeds size limit");
            buffers->lowered[used++] = *p;
        }
    }
    buffers->lowered[used] = '\0';
    if(type->is_enum_specified) {
        for(const char *p = buffers->lowered; *p;) {
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
        copy_text(type->body, sizeof(type->body), buffers->lowered);
        if(type->is_enum)
            canonical_enum_values(type);
        return;
    }
    char previous[ZIR_NAME_MAX] = "";
    size_t emitted = 0;
    for(char *p = buffers->lowered; *p;) {
        char name[ZIR_NAME_MAX];
        char *end = p;
        while(*end && *end != '\n' && *end != ',') end++;
        if((size_t)(end - p) >= sizeof(buffers->member))
            die_at(type->span, "enum_flags member exceeds size limit");
        snprintf(buffers->member, sizeof(buffers->member), "%.*s", (int)(end - p), p);
        trim_in_place(buffers->member);
        p = *end ? end + 1 : end;
        if(buffers->member[0] == '\0') continue;
        size_t n = 0;
        while(isalnum((unsigned char)buffers->member[n]) || buffers->member[n] == '_') n++;
        if(n == 0 || n >= sizeof(name) ||
           !(isalpha((unsigned char)buffers->member[0]) || buffers->member[0] == '_'))
            die_at(type->span, "invalid enum_flags member: %s", buffers->member);
        snprintf(name, sizeof(name), "%.*s", (int)n, buffers->member);
        const char *rest = skip_ws(buffers->member + n);
        int written;
        if(*rest == '=')
            written = snprintf(buffers->flags + emitted, sizeof(buffers->flags) - emitted,
                               "%s\n", buffers->member);
        else if(*rest == '\0' && previous[0])
            written = snprintf(buffers->flags + emitted, sizeof(buffers->flags) - emitted,
                               "%s = %s << 1\n", name, previous);
        else if(*rest == '\0')
            written = snprintf(buffers->flags + emitted, sizeof(buffers->flags) - emitted,
                               "%s = 1\n", name);
        else
            die_at(type->span, "invalid enum_flags member: %s", buffers->member);
        if(written < 0 || (size_t)written >= sizeof(buffers->flags) - emitted)
            die_at(type->span, "enum_flags body exceeds size limit");
        emitted += (size_t)written;
        copy_text(previous, sizeof(previous), name);
    }
    buffers->flags[emitted] = '\0';
    copy_text(type->body, sizeof(type->body), buffers->flags);
    canonical_enum_values(type);
}

void
lower_enum_values(ZirType *type)
{
    static _Thread_local LowerEnumValuesBuffers *spares[16];
    static _Thread_local int spare_count;
    LowerEnumValuesBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    lower_enum_values_with_buffers(type, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

void
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

/* Spell the names a generic record's field type uses so they resolve in
 * USER, the module holding the instance, as they do in OWNER, the module
 * declaring the record: Vec becomes vec.Vec when only the record's module
 * imports vec. Type parameters and names USER already sees stay as they
 * are. */
static int
qualify_generic_field_type(const char *type, char *out, size_t size,
                           char params[][ZIR_NAME_MAX], int parameter_count,
                           const ZirModule *owner, const ZirModule *user)
{
    size_t used = 0;
    for(const char *p = type; *p;) {
        if(!isalpha((unsigned char)*p) && *p != '_') {
            if(used + 1 >= size) return 0;
            out[used++] = *p++;
            continue;
        }
        const char *start = p;
        while(isalnum((unsigned char)*p) || *p == '_' ||
              (*p == '.' && (isalpha((unsigned char)p[1]) || p[1] == '_')))
            p++;
        char name[ZIR_NAME_MAX], qualified[ZIR_NAME_MAX];
        size_t length = (size_t)(p - start);
        if(length >= sizeof(name)) return 0;
        memcpy(name, start, length);
        name[length] = '\0';
        const char *spelled = name;
        int parameter = 0;
        for(int i = 0; i < parameter_count; i++)
            parameter |= strcmp(params[i], name) == 0;
        if(!parameter && strchr(name, '.') == NULL) {
            const ZirModule *declared_in = NULL;
            const ZirType *declared = FindType(owner, name, &declared_in);
            if(declared != NULL && declared_in != NULL &&
               FindType(user, name, NULL) != declared &&
               snprintf(qualified, sizeof(qualified), "%s.%s", declared_in->name,
                        name) < (int)sizeof(qualified) &&
               FindType(user, qualified, NULL) == declared)
                spelled = qualified;
        }
        size_t spelled_length = strlen(spelled);
        if(used + spelled_length >= size) return 0;
        memcpy(out + used, spelled, spelled_length);
        used += spelled_length;
    }
    out[used] = '\0';
    return 1;
}

int
InstantiateGenericRecord(ZirType *instance, const ZirType *generic)
{
    return InstantiateGenericRecordAt(instance, generic, NULL, NULL);
}

int
InstantiateGenericRecordAt(ZirType *instance, const ZirType *generic,
                           const ZirModule *owner, const ZirModule *user)
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
        if(owner != NULL && user != NULL && owner != user) {
            char qualified[ZIR_NAME_MAX];
            if(!qualify_generic_field_type(field.type, qualified, sizeof(qualified),
                                           params, parameter_count, owner, user))
                return 0;
            copy_text(field.type, sizeof(field.type), qualified);
        }
        if(!SubstituteGenericType(field.type, type, sizeof(type),
                                   params, actual, parameter_count))
            return 0;
        int length = snprintf(instance->body + used,
            sizeof(instance->body) - used, "%s%s: %s%s%s\n",
            field.is_using ? "using " : "", field.name, type,
            field.go_tag[0] ? " #go_tag " : "", field.go_tag);
        if(length < 0 || (size_t)length >= sizeof(instance->body) - used)
            return 0;
        used += (size_t)length;
        members++;
    }
    if(status < 0 || members == 0)
        return 0;
    instance->is_union = generic->is_union;
    instance->is_results = generic->is_results;
    if(!strcmp(generic->name, "Map") && members == 2 && parameter_count == 2) {
        size_t position = 0;
        ZirTypeField key, value, end;
        if(TypeNextField(generic, &position, &key) == 1 &&
           TypeNextField(generic, &position, &value) == 1 &&
           TypeNextField(generic, &position, &end) == 0 &&
           !strcmp(key.name, "key") && !strcmp(key.type, params[0]) &&
           !strcmp(value.name, "value") && !strcmp(value.type, params[1]) &&
           !key.is_using && !value.is_using && !key.go_tag[0] && !value.go_tag[0])
            instance->is_map = 1;
    }
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
    if(!instance->is_synthetic_application) {
        instance->template_name[0] = '\0';
        instance->template_args[0] = '\0';
    }
    return 1;
}
/* Buffers read_lowered_source keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ReadLoweredSourceBuffers {
    unsigned char bytes[8192];
} ReadLoweredSourceBuffers;

char *read_lowered_source(const char *path);

static char *
read_lowered_source_with_buffers(const char *path, ReadLoweredSourceBuffers *buffers)
{
    FILE *in = fopen(path, "rb");
    if(in == NULL)
        die_at(Span(path, 0, 0), "open failed: %s", strerror(errno));
    SourceBuffer input = {0};
    size_t count;
    while((count = fread(buffers->bytes, 1, sizeof(buffers->bytes), in)) != 0)
        for(size_t i = 0; i < count; i++) {
            if(buffers->bytes[i] == 0)
                die_at(Span(path, 0, 0), "source contains a null byte");
            source_append(&input, (char)buffers->bytes[i]);
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

char *
read_lowered_source(const char *path)
{
    static _Thread_local ReadLoweredSourceBuffers *spares[16];
    static _Thread_local int spare_count;
    ReadLoweredSourceBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    char *returned = read_lowered_source_with_buffers(path, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

/* Decode the literal source of Jai's #import, string form. A raw #string
 * body has already been lowered to this same quoted spelling. */
char *
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
