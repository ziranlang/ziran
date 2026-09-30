#include "zir_expr.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_token.h"
#include "zir_text.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct ExprParser {
    ZirLexer lexer;
    ZirToken token;
    ZirFunction *fn;
    const ZirModule *module;
    ZirSourceSpan span;
    const char *source;
    size_t begin;
    const char *expected_type;
    int stmt_index;
    int expand_defaults;
    int failed, depth;
} ExprParser;

static void
next(ExprParser *p)
{
    p->token = LexerNext(&p->lexer);
    if(p->token.truncated)
        p->failed = 1;   /* a cut token cannot round-trip to source text */
    p->begin = p->lexer.pos - strlen(p->token.text);
}

static int
is(ExprParser *p, const char *s)
{
    return strcmp(p->token.text, s) == 0;
}

static int
take(ExprParser *p, const char *s)
{
    if(!is(p, s)) return 0;
    next(p);
    return 1;
}

static void
expect(ExprParser *p, const char *s)
{
    if(!take(p, s)) p->failed = 1;
}

static int
node(ExprParser *p, ZirExprKind kind, size_t start, const char *name,
     const char *op, int left, int right)
{
    char buffer[ZIR_TEXT_MAX];
    size_t n = p->begin > start ? p->begin - start : 0;
    ZirSourceSpan span = p->span;
    ZirExpr *e;
    /* A long literal, such as a table of records, keeps all its text. */
    char *text = n < sizeof(buffer) ? buffer : malloc(n + 1);
    if(text == NULL) { p->failed = 1; return -1; }
    memcpy(text, p->source + start, n);
    text[n] = 0;
    trim_in_place(text);
    for(size_t i = 0; i < start; i++) {
        if(p->source[i] == '\n') {
            span.line++;
            span.column = 1;
        } else {
            span.column++;
        }
    }
    e = FunctionAddExpr(p->fn, kind, text, span);
    if(text != buffer) free(text);
    if(!e) { p->failed = 1; return -1; }
    copy_text(e->name, sizeof(e->name), name);
    copy_text(e->op, sizeof(e->op), op);
    e->left = left;
    e->right = right;
    return p->fn->expr_count - 1;
}

static int
type_name(const ExprParser *p, const char *s)
{
    if(*ScalarType(s)) return 1;
    return p->module != NULL && FindType(p->module, s, NULL) != NULL;
}

static int expression(ExprParser *p, int minimum);
static int parse_expr(ZirFunction *fn, const ZirModule *module,
                      const char *text, ZirSourceSpan span,
                      const char *expected_type, int stmt_index,
                      int expand_defaults);

static int default_expansion_depth;

int
CallerLocationLiteral(const ZirModule *module, ZirSourceSpan location,
                      char *output, size_t capacity)
{
    char candidate[ZIR_PATH_MAX * 2];
    char escaped[ZIR_TEXT_MAX];
    const char *path = SpanPath(location);
    if(path[0] != '/' && module->source_root[0] != '\0') {
        int written = snprintf(candidate, sizeof(candidate), "%s/%s",
                               module->source_root, path);
        if(written < 0 || (size_t)written >= sizeof(candidate))
            return 0;
        path = candidate;
    }
    char *canonical = realpath(path, NULL);
    if(canonical != NULL)
        path = canonical;
    size_t length = strlen(path);
    if(path[0] != '/' || length >= ZIR_PATH_MAX) {
        free(canonical);
        return 0;
    }
    size_t escaped_length = escape_c_string(path, escaped, sizeof(escaped));
    free(canonical);
    if(escaped_length >= sizeof(escaped) - 1)
        return 0;
    int written = snprintf(output, capacity,
        "Source_Code_Location.{.fully_pathed_filename = \"%s\", "
        ".line_number = %d}", escaped, location.line);
    return written >= 0 && (size_t)written < capacity;
}

static int
qualify_default_field_helpers(const char *value, const char *base,
                              const char *target, const char *path,
                              char *output, size_t capacity)
{
    const char *dot = strchr(target, '.');
    ZirLexer lexer;
    size_t used = 0, copied = 0;
    LexerInit(&lexer, value, path);
    for(;;) {
        ZirToken token = LexerNext(&lexer);
        if(token.kind == ZIR_TOKEN_EOF) break;
        size_t length = strlen(token.text);
        size_t base_length = strlen(base);
        if(dot == NULL || token.kind != ZIR_TOKEN_IDENT ||
           token.truncated || length <= base_length + 7 ||
           strncmp(token.text, base, base_length) != 0 ||
           strncmp(token.text + base_length, "_field_", 7) != 0)
            continue;
        const char *number = token.text + base_length + 7;
        if(!*number) continue;
        for(const char *digit = number; *digit; digit++)
            if(!isdigit((unsigned char)*digit)) goto next_token;
        size_t start = lexer.pos - length;
        size_t prefix = (size_t)(dot - target);
        if(start < copied || used + start - copied + prefix + 1 + length >=
                             capacity)
            return 0;
        memcpy(output + used, value + copied, start - copied);
        used += start - copied;
        memcpy(output + used, target, prefix);
        used += prefix;
        output[used++] = '.';
        memcpy(output + used, token.text, length);
        used += length;
        copied = lexer.pos;
next_token: ;
    }
    size_t rest = strlen(value + copied);
    if(used + rest >= capacity) return 0;
    memcpy(output + used, value + copied, rest + 1);
    return 1;
}

static int
call_name_shadowed(const ExprParser *p, const char *name)
{
    if(p->stmt_index < 0 || strchr(name, '.') != NULL)
        return 0;
    char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
    if(parameters == NULL) return 1;
    int count = *skip_ws(p->fn->args) ?
        split_top_level(p->fn->args, parameters[0], 64,
                        sizeof(parameters[0])) : 0;
    for(int i = 0; i < count; i++) {
        char *colon = strchr(parameters[i], ':');
        if(colon == NULL) continue;
        *colon = '\0';
        if(!strcmp(trim(parameters[i]), name)) {
            free(parameters);
            return 1;
        }
    }
    free(parameters);
    int depth = 0;
    int binding_depths[128];
    int bindings = 0;
    for(int i = 0; i < p->stmt_index; i++) {
        const ZirStmt *statement = &p->fn->stmts[i];
        if(statement->kind == ZIR_STMT_BLOCK_CLOSE) {
            while(bindings > 0 && binding_depths[bindings - 1] == depth)
                bindings--;
            if(depth > 0) depth--;
        }
        if(statement->kind == ZIR_STMT_DECL &&
           !strcmp(statement->name, name) &&
           bindings < (int)(sizeof(binding_depths) / sizeof(binding_depths[0])))
            binding_depths[bindings++] = depth;
        if(statement->kind == ZIR_STMT_BLOCK_OPEN ||
           statement->kind == ZIR_STMT_IF ||
           statement->kind == ZIR_STMT_WHILE ||
           statement->kind == ZIR_STMT_FOR)
            depth++;
    }
    return bindings > 0;
}
/* Buffers append_default_arguments keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct AppendDefaultArgumentsBuffers {
    char qualified_default[ZIR_TEXT_MAX];
    char location_default[ZIR_TEXT_MAX];
} AppendDefaultArgumentsBuffers;

static void append_default_arguments(ExprParser *p, int callee,
                         const char *name, int *first, int *last);

static void
append_default_arguments_with_buffers(ExprParser *p, int callee,
                         const char *name, int *first, int *last, AppendDefaultArgumentsBuffers *buffers)
{
    char qualified[ZIR_NAME_MAX];
    const char *target = name;
    if(!*target && callee >= 0 &&
       p->fn->exprs[callee].kind == ZIR_EXPR_MEMBER) {
        const ZirExpr *member = &p->fn->exprs[callee];
        if(member->left < 0 ||
           p->fn->exprs[member->left].kind != ZIR_EXPR_IDENT)
            return;
        if(snprintf(qualified, sizeof(qualified), "%s.%s",
                    p->fn->exprs[member->left].name, member->name) >=
           (int)sizeof(qualified)) return;
        target = qualified;
    }
    if(!*target || call_name_shadowed(p, target) ||
       default_expansion_depth >= 32 || p->module == NULL)
        return;
    const ZirModule *owner = NULL;
    const ZirFunction *function = NULL;
    if(ResolveFunctionAt(p->module, target, SpanPath(p->span),
                         &owner, &function) != 1 ||
       function == NULL || !function->default_args[0])
        return;
    char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
    char (*defaults)[ZIR_TEXT_MAX] = calloc(64, sizeof(*defaults));
    if(parameters == NULL || defaults == NULL) {
        free(parameters); free(defaults);
        p->failed = 1;
        return;
    }
    int count = split_top_level(function->args, parameters[0], 64,
                                sizeof(parameters[0]));
    int default_count = split_top_level(function->default_args, defaults[0],
                                        64, sizeof(defaults[0]));
    unsigned char used[64] = {0};
    if(count != default_count) goto done;
    for(int child = *first; child >= 0;
        child = p->fn->exprs[child].next_sibling) {
        const char *named = p->fn->exprs[child].argument_name;
        int index = -1;
        if(*named) {
            for(int i = 0; i < count; i++) {
                char *colon = strchr(parameters[i], ':');
                if(colon == NULL) continue;
                *colon = '\0';
                int matches = !strcmp(trim(parameters[i]), named);
                *colon = ':';
                if(matches) { index = i; break; }
            }
        } else {
            for(int i = 0; i < count; i++)
                if(!used[i]) { index = i; break; }
        }
        if(index < 0 || used[index]) goto done;
        used[index] = 1;
    }
    for(int i = 0; i < count; i++) {
        if(used[i]) continue;
        char *assignment = top_level_assignment(defaults[i]);
        if(assignment == NULL) continue;
        char *colon = strchr(parameters[i], ':');
        if(colon == NULL) continue;
        *colon = '\0';
        char *parameter_name = trim(parameters[i]);
        const char *value = skip_ws(assignment + 1);
        char helper_name[ZIR_NAME_MAX];
        char helper_call[ZIR_NAME_MAX * 2];
        FunctionDefaultHelperName(function, i, helper_name,
                                  sizeof(helper_name));
        if(strcmp(value, "#caller_location") == 0) {
            if(!CallerLocationLiteral(p->module, p->fn->exprs[callee].span,
                                        buffers->location_default,
                                        sizeof(buffers->location_default))) {
                p->failed = 1;
                break;
            }
            value = buffers->location_default;
        } else if(DefaultIsLiteral(value)) {
            /* The literal itself: it reads the same at every call site. */
        } else {
            int has_helper = !function->is_template;
            if(!has_helper && owner != NULL)
                for(int f = 0; f < owner->function_count; f++)
                    if(!strcmp(owner->functions[f].name, helper_name)) {
                        has_helper = 1;
                        break;
                    }
            if(has_helper) {
                const char *dot = strchr(target, '.');
                int written = dot == NULL ?
                    snprintf(helper_call, sizeof(helper_call), "%s()",
                             helper_name) :
                    snprintf(helper_call, sizeof(helper_call), "%.*s.%s()",
                             (int)(dot - target), target, helper_name);
                if(written < 0 || (size_t)written >= sizeof(helper_call)) {
                    p->failed = 1;
                    break;
                }
                value = helper_call;
            } else if(!qualify_default_field_helpers(
                          value, helper_name, target, SpanPath(p->span),
                          buffers->qualified_default, sizeof(buffers->qualified_default))) {
                p->failed = 1;
                break;
            } else {
                value = buffers->qualified_default;
            }
        }
        default_expansion_depth++;
        int child = parse_expr(p->fn, p->module, value, p->span,
                               NULL, p->stmt_index, p->expand_defaults);
        default_expansion_depth--;
        if(child < 0) { p->failed = 1; break; }
        p->fn->exprs[child].argument_name = KeepName(parameter_name);
        if(*last >= 0) p->fn->exprs[*last].next_sibling = child;
        else *first = child;
        *last = child;
        used[i] = 1;
    }
done:
    free(parameters); free(defaults);
}

static void
append_default_arguments(ExprParser *p, int callee,
                         const char *name, int *first, int *last)
{
    static _Thread_local AppendDefaultArgumentsBuffers *spares[16];
    static _Thread_local int spare_count;
    AppendDefaultArgumentsBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    append_default_arguments_with_buffers(p, callee, name, first, last, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

static int
jai_char_byte(const char *text, int *value)
{
    size_t length = strlen(text);
    if(length == 3 && text[0] == '"' && text[2] == '"' &&
       (unsigned char)text[1] < 128) {
        *value = (unsigned char)text[1];
        return 1;
    }
    if(length == 4 && text[0] == '"' && text[1] == '\\' && text[3] == '"') {
        switch(text[2]) {
        case '0': *value = 0; return 1;
        case 'n': *value = '\n'; return 1;
        case 'r': *value = '\r'; return 1;
        case 't': *value = '\t'; return 1;
        case '"': *value = '"'; return 1;
        case '\\': *value = '\\'; return 1;
        default: return 0;
        }
    }
    if(length == 6 && text[0] == '"' && text[1] == '\\' &&
       text[2] == 'x' && text[5] == '"' &&
       isxdigit((unsigned char)text[3]) &&
       isxdigit((unsigned char)text[4])) {
        char digits[3] = {text[3], text[4], 0};
        *value = (int)strtol(digits, NULL, 16);
        return 1;
    }
    return 0;
}
/* Buffers record_initializer keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct RecordInitializerBuffers {
    ZirToken field;
    ZirToken equals;
    ZirToken following;
} RecordInitializerBuffers;

static int record_initializer(ExprParser *p, size_t start, const char *type,
                   const char *open, const char *close);

static int
record_initializer_with_buffers(ExprParser *p, size_t start, const char *type,
                   const char *open, const char *close, RecordInitializerBuffers *buffers)
{
    int first = -1;
    int last = -1;
    int ordinal = 0;
    const ZirModule *record_owner = NULL;
    const ZirType *record = p->module ?
        FindType(p->module, type, &record_owner) : NULL;
    expect(p, open);
    while(!p->failed && !is(p, close) && p->token.kind != ZIR_TOKEN_EOF) {
        size_t field_start = p->begin;
        char name[ZIR_NAME_MAX] = "";
        int named = 0;
        if(is(p, ".")) {
            ZirLexer lookahead = p->lexer;
            buffers->field = LexerNext(&lookahead);
            buffers->equals = LexerNext(&lookahead);
            if(buffers->field.kind == ZIR_TOKEN_IDENT &&
               !strcmp(buffers->equals.text, "=")) {
                next(p);
                named = 1;
            }
        }
        if(!named && p->token.kind == ZIR_TOKEN_IDENT) {
            ZirLexer lookahead = p->lexer;
            buffers->following = LexerNext(&lookahead);
            named = !strcmp(buffers->following.text, "=");
        }
        if(named) {
            if(p->token.kind != ZIR_TOKEN_IDENT) {
                p->failed = 1;
                break;
            }
            copy_text(name, sizeof(name), p->token.text);
            next(p);
            expect(p, "=");
        }
        int value;
        if(is(p, "{")) {
            ZirTypeField field;
            size_t offset = 0;
            int position = 0;
            char field_type[ZIR_NAME_MAX] = "";
            ArrayElementType(type, field_type, sizeof(field_type), NULL);
            while(record != NULL && TypeNextField(record, &offset, &field) == 1) {
                if(named ? !strcmp(field.name, name) : position == ordinal) {
                    copy_text(field_type, sizeof(field_type), field.type);
                    break;
                }
                position++;
            }
            if(record_owner != NULL && record_owner != p->module) {
                const char *dot = strchr(type, '.');
                const ZirType *declared = FindType(record_owner,
                                                   field_type, NULL);
                char qualified[ZIR_NAME_MAX];
                int length = dot != NULL && declared != NULL ?
                    snprintf(qualified, sizeof(qualified), "%.*s.%s",
                             (int)(dot - type), type, field_type) : -1;
                if(length >= 0 && (size_t)length < sizeof(qualified) &&
                   FindType(p->module, qualified, NULL) == declared)
                    copy_text(field_type, sizeof(field_type), qualified);
            }
            if(++p->depth > 128) {
                p->failed = 1;
                p->depth--;
                return -1;
            }
            value = record_initializer(p, p->begin, field_type, "{", "}");
            p->depth--;
        } else {
            value = expression(p, 1);
        }
        ordinal++;
        if(value < 0) {
            p->failed = 1;
            break;
        }
        int field = node(p, ZIR_EXPR_FIELD_INIT, field_start, name,
                         named ? "=" : "", -1, value);
        if(field < 0)
            break;
        if(last >= 0)
            p->fn->exprs[last].next_sibling = field;
        else
            first = field;
        last = field;
        if(!take(p, ","))
            break;
    }
    expect(p, close);
    int result = node(p, ZIR_EXPR_COMPOUND, start, type, "", -1, -1);
    if(result >= 0)
        p->fn->exprs[result].first_child = first;
    return result;
}

static int
record_initializer(ExprParser *p, size_t start, const char *type,
                   const char *open, const char *close)
{
    static _Thread_local RecordInitializerBuffers *spares[16];
    static _Thread_local int spare_count;
    RecordInitializerBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = record_initializer_with_buffers(p, start, type, open, close, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static int
typed_array_initializer(ExprParser *p, size_t start,
                        const char *element_type)
{
    char type[ZIR_NAME_MAX];
    int written = snprintf(type, sizeof(type), "[1]%s", element_type);
    if(written < 0 || (size_t)written >= sizeof(type)) {
        p->failed = 1;
        return -1;
    }
    int result = record_initializer(p, start, type, "[", "]");
    if(result < 0 || p->failed) return result;
    int count = 0;
    for(int child = p->fn->exprs[result].first_child; child >= 0;
        child = p->fn->exprs[child].next_sibling)
        count++;
    written = snprintf(p->fn->exprs[result].name,
                       sizeof(p->fn->exprs[result].name),
                       "[%d]%s", count, element_type);
    if(written < 0 ||
       (size_t)written >= sizeof(p->fn->exprs[result].name))
        p->failed = 1;
    return result;
}
/* Buffers prefix keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct PrefixBuffers {
    ZirToken tok;
    ZirToken after_type;
    ZirToken following;
    ZirToken dot;
    ZirToken opener;
} PrefixBuffers;

static int prefix(ExprParser *p);

static int
prefix_with_buffers(ExprParser *p, PrefixBuffers *buffers)
{
    size_t start = p->begin;
    buffers->tok = p->token;
    int result = -1;
    if(++p->depth > 128) { p->failed = 1; p->depth--; return -1; }
    if(take(p, "#this")) {
        if(p->fn->name[0] == '\0') {
            Diagnostic(p->span, "parse.this",
                       "#this requires a procedure or type scope");
            exit(1);
        }
        result = node(p, ZIR_EXPR_IDENT, start, p->fn->name, "", -1, -1);
        if(result >= 0)
            p->fn->exprs[result].is_this = 1;
    } else if(is(p, "#caller_location")) {
        Diagnostic(p->span, "parse.caller_location",
                   "#caller_location is only valid as a parameter default");
        exit(1);
    } else if(take(p, "#compile_time")) {
        result = node(p, ZIR_EXPR_COMPILE_TIME, start, "", "", -1, -1);
    } else if(take(p, "#procedure_name")) {
        if(p->fn->name[0] == '\0') {
            Diagnostic(p->span, "parse.procedure_name",
                       "#procedure_name() requires a procedure scope");
            exit(1);
        }
        expect(p, "(");
        expect(p, ")");
        result = node(p, ZIR_EXPR_STRING, start, "", "", -1, -1);
        if(result >= 0) {
            char literal[ZIR_NAME_MAX + 3];
            int written = snprintf(literal, sizeof(literal), "\"%s\"",
                                   p->fn->name);
            if(written < 0 || (size_t)written >= sizeof(literal))
                p->failed = 1;
            else
                p->fn->exprs[result].text = KeepText(literal);
        }
    } else if(take(p, "#char")) {
        int value;
        if(p->token.kind != ZIR_TOKEN_STRING ||
           !jai_char_byte(p->token.text, &value)) {
            Diagnostic(p->span, "parse.jai_char",
                       "#char requires a one-byte string literal");
            exit(1);
        }
        next(p);
        result = node(p, ZIR_EXPR_INT, start, "", "", -1, -1);
        if(result >= 0) {
            char digits[32];
            snprintf(digits, sizeof(digits), "%d", value);
            p->fn->exprs[result].text = KeepText(digits);
        }
    } else if(is(p, "ifx") || is(p, "#ifx")) {
        char op[8];
        copy_text(op, sizeof(op), p->token.text);
        next(p);
        int condition = expression(p, 2);
        take(p, "then");
        int selected = expression(p, 1);
        take(p, ";");
        expect(p, "else");
        int alternative = expression(p, 1);
        result = node(p, ZIR_EXPR_CONDITIONAL, start, "", op,
                      condition, selected);
        if(result >= 0)
            p->fn->exprs[result].third = alternative;
    } else if(is(p, "sizeof")) {
        Diagnostic(p->span, "parse.jai_syntax",
                   "sizeof is not Jai syntax; use size_of(Type)");
        exit(1);
    } else if(take(p, "size_of")) {
        char type[ZIR_NAME_MAX];
        size_t begin, length;
        int nested = 0;
        expect(p, "(");
        begin = p->begin;
        while(p->token.kind != ZIR_TOKEN_EOF) {
            if(is(p, ")") && nested == 0) break;
            if(is(p, "(")) nested++;
            if(is(p, ")")) nested--;
            next(p);
        }
        length = p->begin - begin;
        if(length >= sizeof(type)) { p->failed = 1; length = 0; }
        memcpy(type, p->source + begin, length);
        type[length] = '\0';
        trim_in_place(type);
        expect(p, ")");
        result = node(p, ZIR_EXPR_SIZE_OF, start, type, "", -1, -1);
    } else if(take(p, "cast")) {
        char type[ZIR_NAME_MAX];
        size_t ts, length;
        expect(p, "(");
        ts = p->begin;
        if(is(p, "char")) {
            Diagnostic(p->span, "parse.jai_syntax",
                       "non-Jai primitive type spelling: char");
            exit(1);
        }
        while(p->token.kind != ZIR_TOKEN_EOF && !is(p, ")")) next(p);
        length = p->begin - ts;
        if(length >= sizeof(type)) { p->failed = 1; length = 0; }
        memcpy(type, p->source + ts, length);
        type[length] = 0;
        trim_in_place(type);
        if(!type_name(p, type) && type[0] != '[' && type[0] != '*')
            p->failed = 1;
        expect(p, ")");
        int right = prefix(p);
        result = node(p, ZIR_EXPR_CAST, start, type, "", -1, right);
    } else if(is(p, "++") || is(p, "--")) {
        Diagnostic(p->span, "parse.jai_syntax",
                   "Jai has no increment or decrement operators; use += 1 or -= 1");
        exit(1);
    } else if(take(p, "<<")) {
        int right = prefix(p);
        result = node(p, ZIR_EXPR_UNARY, start, "", "*", -1, right);
    } else if(is(p, "+") || is(p, "-") || is(p, "!") || is(p, "~") ||
              is(p, "*")) {
        int right;
        const char *op = !strcmp(buffers->tok.text, "*") ? "&" : buffers->tok.text;
        next(p);
        right = prefix(p);
        result = node(p, ZIR_EXPR_UNARY, start, "", op, -1, right);
    } else if(is(p, "&")) {
        Diagnostic(p->span, "parse.jai_syntax",
                   "C-style address-of is not valid Jai syntax; use *value");
        exit(1);
    } else if(take(p, "(")) {
        if(type_name(p, p->token.text) || is(p, "[")) {
            /* `(Color.{...})` is a parenthesized record literal, not a
             * C-style cast: the record marker `.` follows the type name. */
            ZirLexer lookahead = p->lexer;
            buffers->after_type = LexerNext(&lookahead);
            int record_literal = is(p, ".") || !strcmp(buffers->after_type.text, ".");
            if(!record_literal) {
                Diagnostic(p->span, "parse.jai_syntax",
                           "C-style cast or literal is not valid Jai syntax; use cast(Type) value or Type.{...}");
                exit(1);
            }
            result = expression(p, 1);
            expect(p, ")");
        } else {
            result = expression(p, 1);
            expect(p, ")");
            if(is(p, "{")) {
                Diagnostic(p->span, "parse.jai_syntax",
                           "C-style literal is not valid Jai syntax; use Type.{...}");
                exit(1);
            }
        }
    } else if(take(p, ".")) {
        char name[ZIR_NAME_MAX];
        if(is(p, "{")) {
            result = record_initializer(p, start, "", "{", "}");
        } else if(is(p, "[") && p->expected_type != NULL &&
                  ArrayElementType(p->expected_type, NULL, 0, NULL)) {
            result = record_initializer(p, start, p->expected_type, "[", "]");
        } else if(p->token.kind != ZIR_TOKEN_IDENT) {
            p->failed = 1;
        } else {
            int length = snprintf(name, sizeof(name), ".%s", p->token.text);
            if(length < 0 || (size_t)length >= sizeof(name))
                p->failed = 1;
            next(p);
            if(!p->failed)
                result = node(p, ZIR_EXPR_IDENT, start, name, "", -1, -1);
        }
    } else {
        ZirExprKind kind;
        switch(buffers->tok.kind) {
        case ZIR_TOKEN_IDENT: kind = ZIR_EXPR_IDENT; break;
        case ZIR_TOKEN_INT: kind = ZIR_EXPR_INT; break;
        case ZIR_TOKEN_FLOAT: kind = ZIR_EXPR_FLOAT; break;
        case ZIR_TOKEN_STRING: kind = ZIR_EXPR_STRING; break;
        case ZIR_TOKEN_CHAR:
            Diagnostic(p->span, "parse.jai_char",
                       "single-quoted character literals are not valid Jai syntax; use #char \"x\"");
            exit(1);
        default: p->failed = 1; p->depth--; return -1;
        }
        next(p);
        if(kind == ZIR_EXPR_IDENT && is(p, ".")) {
            ZirLexer lookahead = p->lexer;
            buffers->following = LexerNext(&lookahead);
            if(!strcmp(buffers->following.text, "{")) {
                const ZirType *record = p->module ?
                    FindType(p->module, buffers->tok.text, NULL) : NULL;
                if(record == NULL || record->is_enum) p->failed = 1;
                next(p);
                result = record_initializer(p, start, buffers->tok.text, "{", "}");
            } else if(!strcmp(buffers->following.text, "[") &&
                      type_name(p, buffers->tok.text)) {
                next(p);
                result = typed_array_initializer(p, start, buffers->tok.text);
            } else if(buffers->following.kind == ZIR_TOKEN_IDENT) {
                buffers->dot = LexerNext(&lookahead);
                buffers->opener = LexerNext(&lookahead);
                char qualified[ZIR_NAME_MAX];
                int length = snprintf(qualified, sizeof(qualified), "%s.%s",
                                      buffers->tok.text, buffers->following.text);
                if(!strcmp(buffers->dot.text, ".") &&
                   length >= 0 &&
                   (size_t)length < sizeof(qualified) &&
                   type_name(p, qualified)) {
                    if(!strcmp(buffers->opener.text, "[") ||
                       !strcmp(buffers->opener.text, "{")) {
                        next(p);
                        next(p);
                        next(p);
                        if(!strcmp(buffers->opener.text, "["))
                            result = typed_array_initializer(p, start,
                                                             qualified);
                        else {
                            const ZirType *record = FindType(p->module,
                                                             qualified, NULL);
                            if(record == NULL || record->is_enum)
                                p->failed = 1;
                            else
                                result = record_initializer(p, start,
                                                            qualified, "{", "}");
                        }
                    }
                }
            }
        }
        if(result < 0 && !p->failed)
            result = node(p, kind, start,
                          kind == ZIR_EXPR_IDENT ? buffers->tok.text : "", "", -1, -1);
    }
    while(!p->failed) {
        if(take(p, "[")) {
            int low = is(p, ":") ? -1 : expression(p, 1);
            if(take(p, ":")) {
                int high = is(p, "]") ? -1 : expression(p, 1);
                expect(p, "]");
                result = node(p, ZIR_EXPR_SLICE, start, "", "", result, low);
                if(result >= 0)
                    p->fn->exprs[result].third = high;
            } else {
                expect(p, "]");
                result = node(p, ZIR_EXPR_INDEX, start, "", "", result, low);
            }
        } else if(is(p, "->")) {
            Diagnostic(p->span, "parse.jai_syntax",
                       "C-style pointer member access is not valid Jai syntax; use .field");
            exit(1);
        } else if(is(p, ".")) {
            char name[ZIR_NAME_MAX];
            next(p);
            if(take(p, "*")) {
                result = node(p, ZIR_EXPR_UNARY, start, "", "*", -1, result);
                continue;
            }
            if(p->token.kind != ZIR_TOKEN_IDENT) p->failed = 1;
            copy_text(name, sizeof(name), p->token.text);
            next(p);
            result = node(p, ZIR_EXPR_MEMBER, start, name, ".", result, -1);
        } else if(take(p, "(")) {
            int first = -1, last = -1;
            char name[ZIR_NAME_MAX] = "";
            int callee = result;
            if(callee >= 0 && p->fn->exprs[callee].kind == ZIR_EXPR_IDENT)
                copy_text(name, sizeof(name), p->fn->exprs[callee].name);
            if(!is(p, ")")) do {
                char argument_name[ZIR_NAME_MAX] = "";
                if(p->token.kind == ZIR_TOKEN_IDENT) {
                    ZirLexer lookahead = p->lexer;
                    buffers->following = LexerNext(&lookahead);
                    if(!strcmp(buffers->following.text, "=")) {
                        copy_text(argument_name, sizeof(argument_name),
                                  p->token.text);
                        next(p);
                        expect(p, "=");
                    }
                }
                int child = expression(p, 1);
                if(child < 0) { p->failed = 1; break; }
                p->fn->exprs[child].argument_name = KeepName(argument_name);
                if(last >= 0) p->fn->exprs[last].next_sibling = child;
                else first = child;
                last = child;
            } while(take(p, ","));
            expect(p, ")");
            if(!p->failed && p->expand_defaults)
                append_default_arguments(p, callee, name, &first, &last);
            result = node(p, ZIR_EXPR_CALL, start, name, "", name[0] ? -1 : callee, -1);
            if(result >= 0) {
                p->fn->exprs[result].first_child = first;
                if(callee >= 0 && p->fn->exprs[callee].is_this)
                    p->fn->exprs[result].is_this = 1;
            }
        } else if(is(p, "++") || is(p, "--")) {
            Diagnostic(p->span, "parse.jai_syntax",
                       "Jai has no increment or decrement operators; use += 1 or -= 1");
            exit(1);
        } else if(is(p, "?") && p->begin > 0 &&
                  !isspace((unsigned char)p->source[p->begin - 1])) {
            Diagnostic(p->span, "parse.jai_syntax",
                       "postfix ? is not Jai syntax; handle the result explicitly");
            exit(1);
        } else break;
    }
    p->depth--;
    return result;
}

static int
prefix(ExprParser *p)
{
    static _Thread_local PrefixBuffers *spares[16];
    static _Thread_local int spare_count;
    PrefixBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = prefix_with_buffers(p, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static int
precedence(const char *op)
{
    if(!strcmp(op, "||")) return 2;
    if(!strcmp(op, "&&")) return 3;
    if(!strcmp(op, "|")) return 4;
    if(!strcmp(op, "^")) return 5;
    if(!strcmp(op, "&")) return 6;
    if(!strcmp(op, "==") || !strcmp(op, "!=")) return 7;
    if(!strcmp(op, "<") || !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=")) return 8;
    if(!strcmp(op, "<<") || !strcmp(op, ">>")) return 9;
    if(!strcmp(op, "+") || !strcmp(op, "-")) return 10;
    if(!strcmp(op, "*") || !strcmp(op, "/") || !strcmp(op, "%")) return 11;
    return 0;
}

static int
expression(ExprParser *p, int minimum)
{
    size_t start = p->begin;
    int left = prefix(p), prec;
    if(++p->depth > 128) { p->failed = 1; p->depth--; return -1; }
    while(!p->failed && (prec = precedence(p->token.text)) >= minimum) {
        char op[8];
        int right;
        copy_text(op, sizeof(op), p->token.text);
        next(p);
        right = expression(p, prec + 1);
        left = node(p, ZIR_EXPR_BINARY, start, "", op, left, right);
    }
    if(is(p, "?")) {
        Diagnostic(p->span, "parse.jai_syntax",
                   "C-style conditional is not valid Jai syntax; use ifx ... then ... else ...");
        exit(1);
    }
    p->depth--;
    return left;
}

static int
parse_expr(ZirFunction *fn, const ZirModule *module, const char *text,
           ZirSourceSpan span, const char *expected_type, int stmt_index,
           int expand_defaults)
{
    ExprParser p = {0};
    int initial = fn->expr_count, result;
    if(!*skip_ws(text)) return -1;
    p.fn = fn; p.module = module; p.span = span; p.source = text;
    p.expected_type = expected_type;
    p.stmt_index = stmt_index;
    p.expand_defaults = expand_defaults;
    LexerInit(&p.lexer, text, SpanPath(span));
    next(&p);
    result = expression(&p, 1);
    take(&p, ";");
    if(p.failed || p.token.kind != ZIR_TOKEN_EOF || result < 0) {
        fn->expr_count = initial;
        if(!FunctionAddExpr(fn, ZIR_EXPR_UNKNOWN, text, span)) return -1;
        return fn->expr_count - 1;
    }
    return result;
}

int
ParseExpr(ZirFunction *fn, const ZirModule *module, const char *text,
          ZirSourceSpan span)
{
    return parse_expr(fn, module, text, span, NULL, -1, 1);
}

int
ParseExprNoDefaults(ZirFunction *fn, const ZirModule *module,
                    const char *text, ZirSourceSpan span)
{
    return parse_expr(fn, module, text, span, NULL, -1, 0);
}

int
ParseExprTyped(ZirFunction *fn, const ZirModule *module,
               const char *text, ZirSourceSpan span, const char *expected_type)
{
    return parse_expr(fn, module, text, span, expected_type, -1, 0);
}
/* Buffers StructureFunction keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct StructureFunctionBuffers {
    char text[ZIR_TEXT_MAX];
    ZirToken tok;
} StructureFunctionBuffers;

void StructureFunction(ZirFunction *fn, const ZirModule *module);

static void
StructureFunction_with_buffers(ZirFunction *fn, const ZirModule *module, StructureFunctionBuffers *buffers)
{
    free(fn->exprs); fn->exprs = NULL; fn->expr_count = fn->expr_cap = 0;
    /* A statement longer than the buffer is parsed from its own copy. */
    char *owned = NULL;
    for(int i = 0; i < fn->stmt_count; i++) {
        ZirStmt *st = &fn->stmts[i];
        char *value = NULL;
        char *text = buffers->text;
        if(strlen(st->text) >= sizeof(buffers->text)) {
            free(owned);
            owned = AllocateOrExit(strlen(st->text) + 1);
            strcpy(owned, st->text);
            text = owned;
        } else
            copy_text(buffers->text, sizeof(buffers->text), st->text);
        st->expr_root = st->lhs_root = -1;
        if(st->is_using && st->kind == ZIR_STMT_EXPR) {
            st->expr_root = ParseExprNoDefaults(fn, module, "0", st->span);
            continue;
        }
        st->is_else = st->kind == ZIR_STMT_IF &&
                      strncmp(text, "else", 4) == 0 &&
                      (text[4] == 0 || isspace((unsigned char)text[4]));
        if(st->kind == ZIR_STMT_DECL) {
            char *colon = strchr(text, ':');
            if(colon) {
                *colon++ = 0; trim_in_place(text);
                copy_text(st->name, sizeof(st->name), text);
                value = strchr(colon, '=');
                if(value) *value++ = 0;
                trim_in_place(colon);
                size_t type_length = strlen(colon);
                if(type_length && colon[type_length - 1] == ';') {
                    colon[--type_length] = '\0';
                    trim_in_place(colon);
                }
                if(strchr(colon, '#') != NULL) {
                    Diagnostic(st->span, "parse.modifier",
                        "unknown declaration modifier: %s", strchr(colon, '#'));
                    exit(1);
                }
                copy_text(st->type, sizeof(st->type), colon);
            }
        } else if(st->kind == ZIR_STMT_ASSIGN) {
            ZirLexer lexer; 
            LexerInit(&lexer, text, SpanPath(st->span));
            do {
                buffers->tok = LexerNext(&lexer);
                if(!strcmp(buffers->tok.text, "=") || !strcmp(buffers->tok.text, "+=") ||
                   !strcmp(buffers->tok.text, "-=") || !strcmp(buffers->tok.text, "*=") ||
                   !strcmp(buffers->tok.text, "/=") || !strcmp(buffers->tok.text, "%=") ||
                   !strcmp(buffers->tok.text, "&=") || !strcmp(buffers->tok.text, "|=") ||
                   !strcmp(buffers->tok.text, "^=") || !strcmp(buffers->tok.text, "<<=") || !strcmp(buffers->tok.text, ">>=")) {
                    value = text + lexer.pos;
                    copy_text(st->assignment_op, sizeof(st->assignment_op), buffers->tok.text);
                    text[lexer.pos - strlen(buffers->tok.text)] = 0;
                    st->lhs_root = parse_expr(fn, module, text, st->span,
                                              NULL, i, 1);
                    break;
                }
            } while(buffers->tok.kind != ZIR_TOKEN_EOF);
        } else if(st->kind == ZIR_STMT_RETURN) value = text + 6;
        else if(st->kind == ZIR_STMT_UNUSED) value = text + 6;
        else if(st->kind == ZIR_STMT_EXPR) value = text;
        else if(st->kind == ZIR_STMT_IF_CASE) {
            char *condition = (char *)skip_ws(text + 2);
            if(strncmp(condition, "#complete", 9) == 0 &&
               isspace((unsigned char)condition[9]))
                condition = (char *)skip_ws(condition + 9);
            char *equals = strstr(condition, "==");
            if(equals != NULL) {
                *equals = '\0';
                trim_in_place(condition);
            }
            value = condition;
        }
        else if(st->kind == ZIR_STMT_WHILE || st->kind == ZIR_STMT_IF) {
            strip_block_brace(text);
            value = text;
            if(!strncmp(value, "else", 4)) value = (char *)skip_ws(value + 4);
            while(*value && !isspace((unsigned char)*value) && *value != '(') value++;
        }
        if(value && strcmp(skip_ws(value), ";")) {
            if(st->kind == ZIR_STMT_DECL && st->type[0] == '[' &&
               *skip_ws(value) == '{') {
                Diagnostic(st->span, "parse.jai_syntax",
                           "C-style array literal is not valid Jai syntax; use .[...]");
                exit(1);
            }
            st->expr_root = parse_expr(fn, module, value, st->span,
                st->kind == ZIR_STMT_DECL ? st->type : NULL, i, 1);
        }
    }
    free(owned);
}

void
StructureFunction(ZirFunction *fn, const ZirModule *module)
{
    static _Thread_local StructureFunctionBuffers *spares[16];
    static _Thread_local int spare_count;
    StructureFunctionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    StructureFunction_with_buffers(fn, module, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}
