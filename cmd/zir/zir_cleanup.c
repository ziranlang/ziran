#include "zir_cleanup.h"
#include "zir_diagnostic.h"
#include "zir_text.h"
#include "zir_token.h"
#include "zir_expr.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct Cleanup {
    ZirStmt statement;
    const ZirStmt *body;
    int body_count;
    int is_block;
    int depth;
} Cleanup;

static int append(ZirFunction *out, const ZirStmt *st);

static int
fail(const ZirStmt *st, const char *message)
{
    Diagnostic(st->span, "check.cleanup", "%s", message);
    return 0;
}

static int
mentions(const char *text, const char *name)
{
    ZirLexer lexer;
    ZirToken token;
    LexerInit(&lexer, text, "");
    do {
        token = LexerNext(&lexer);
        if(token.kind == ZIR_TOKEN_IDENT && !strcmp(token.text, name)) return 1;
    } while(token.kind != ZIR_TOKEN_EOF);
    return 0;
}

static int
cleanup_mentions(const Cleanup *entry, const char *name)
{
    if(mentions(entry->statement.text, name)) return 1;
    for(int i = 0; i < entry->body_count; i++)
        if(mentions(entry->body[i].text, name)) return 1;
    return 0;
}

static int
defer_block(const ZirStmt *st)
{
    if(st->kind != ZIR_STMT_DEFER || strncmp(st->text, "defer", 5))
        return 0;
    const char *body = skip_ws(st->text + 5);
    return *body == '{' && *skip_ws(body + 1) == '\0';
}

static int
action_kind(const char *text, ZirStmt *action)
{
    ZirLexer lexer;
    ZirToken token;
    ZirFunction temporary = {0};
    int valid;
    action->kind = ZIR_STMT_EXPR;
    LexerInit(&lexer, text, SpanPath(action->span));
    token = LexerNext(&lexer);
    if(!strcmp(token.text, "return") || !strcmp(token.text, "break") ||
       !strcmp(token.text, "continue") || !strcmp(token.text, "goto") ||
       !strcmp(token.text, "defer") || !strcmp(token.text, "if") ||
       !strcmp(token.text, "while") || !strcmp(token.text, "for")) return 0;
    LexerInit(&lexer, text, SpanPath(action->span));
    do {
        token = LexerNext(&lexer);
        if(!strcmp(token.text, "=") || !strcmp(token.text, "+=") ||
           !strcmp(token.text, "-=") || !strcmp(token.text, "*=") ||
           !strcmp(token.text, "/=") || !strcmp(token.text, "%=") ||
           !strcmp(token.text, "&=") || !strcmp(token.text, "|=") ||
           !strcmp(token.text, "^=") || !strcmp(token.text, "<<=") || !strcmp(token.text, ">>="))
            action->kind = ZIR_STMT_ASSIGN;
    } while(token.kind != ZIR_TOKEN_EOF);
    copy_text(action->text, sizeof(action->text), text);
    if(!append(&temporary, action)) return 0;
    StructureFunction(&temporary, NULL);
    valid = temporary.stmts[0].expr_root >= 0 &&
            temporary.exprs[temporary.stmts[0].expr_root].kind != ZIR_EXPR_UNKNOWN;
    free(temporary.stmts); free(temporary.exprs);
    return valid;
}

static int
append(ZirFunction *out, const ZirStmt *st)
{
    ZirStmt *copy = FunctionAddStmt(out, st->kind, st->text, st->span);
    if(!copy)
        return 0;
    *copy = *st;
    return 1;
}

typedef struct BodyNormalizer {
    const ZirFunction *source;
    ZirFunction output;
    int cursor;
} BodyNormalizer;

static int normalize_one(BodyNormalizer *n, int depth);

static int
header_opens_block(const char *text)
{
    size_t length = strlen(text);
    while(length > 0 && isspace((unsigned char)text[length - 1]))
        length--;
    return length > 0 && text[length - 1] == '{';
}

static int
starts_else(const char *text)
{
    return strncmp(text, "else", 4) == 0 &&
           (text[4] == '\0' || isspace((unsigned char)text[4]));
}

static int
normalize_block(BodyNormalizer *n, const ZirStmt *header, int depth)
{
    while(n->cursor < n->source->stmt_count &&
          n->source->stmts[n->cursor].kind != ZIR_STMT_BLOCK_CLOSE)
        if(!normalize_one(n, depth + 1))
            return 0;
    if(n->cursor >= n->source->stmt_count)
        return fail(header, "unterminated block body");
    return append(&n->output, &n->source->stmts[n->cursor++]);
}

static int
normalize_control_body(BodyNormalizer *n, int depth)
{
    const ZirStmt *source = &n->source->stmts[n->cursor++];
    ZirStmt header = *source;
    int explicit_block = header_opens_block(header.text);
    int separate_block = !explicit_block &&
        n->cursor < n->source->stmt_count &&
        n->source->stmts[n->cursor].kind == ZIR_STMT_BLOCK_OPEN;
    if(!explicit_block) {
        size_t length = strlen(header.text);
        if(length + 2 >= sizeof(header.text))
            return fail(source, "control header exceeds statement limit");
        memcpy(header.text + length, " {", 3);
    }
    if(!append(&n->output, &header))
        return 0;
    if(separate_block)
        n->cursor++;
    if(explicit_block || separate_block)
        return normalize_block(n, source, depth);
    if(n->cursor >= n->source->stmt_count ||
       n->source->stmts[n->cursor].kind == ZIR_STMT_BLOCK_CLOSE ||
       (n->source->stmts[n->cursor].kind == ZIR_STMT_IF &&
        starts_else(n->source->stmts[n->cursor].text)))
        return fail(source, "control header requires a body");
    if(!normalize_one(n, depth + 1))
        return 0;
    ZirStmt close = {0};
    close.kind = ZIR_STMT_BLOCK_CLOSE;
    copy_text(close.text, sizeof(close.text), "}");
    close.expr_root = close.lhs_root = -1;
    close.span = source->span;
    return append(&n->output, &close);
}

static int
normalize_one(BodyNormalizer *n, int depth)
{
    if(n->cursor >= n->source->stmt_count)
        return 0;
    const ZirStmt *statement = &n->source->stmts[n->cursor];
    if(depth > 128)
        return fail(statement, "control nesting exceeds limit");
    if(statement->kind == ZIR_STMT_BLOCK_CLOSE)
        return fail(statement, "unexpected block close");
    if(statement->kind == ZIR_STMT_IF &&
       starts_else(statement->text))
        return fail(statement, "else without matching if");
    if(statement->kind == ZIR_STMT_IF ||
       statement->kind == ZIR_STMT_WHILE ||
       statement->kind == ZIR_STMT_FOR) {
        ZirStmtKind kind = statement->kind;
        if(!normalize_control_body(n, depth))
            return 0;
        if(kind == ZIR_STMT_IF) {
            while(n->cursor < n->source->stmt_count &&
                  n->source->stmts[n->cursor].kind == ZIR_STMT_IF &&
                  starts_else(n->source->stmts[n->cursor].text))
                if(!normalize_control_body(n, depth))
                    return 0;
        }
        return 1;
    }
    if(statement->kind == ZIR_STMT_BLOCK_OPEN ||
       statement->kind == ZIR_STMT_IF_CASE ||
       (statement->kind == ZIR_STMT_CASE &&
        header_opens_block(statement->text)) ||
       defer_block(statement)) {
        const ZirStmt *header = statement;
        if(!append(&n->output, header))
            return 0;
        n->cursor++;
        return normalize_block(n, header, depth);
    }
    n->cursor++;
    return append(&n->output, statement);
}

int
NormalizeJaiBodies(ZirFunction *fn)
{
    BodyNormalizer normalizer = {0};
    normalizer.source = fn;
    while(normalizer.cursor < fn->stmt_count)
        if(!normalize_one(&normalizer, 0)) {
            free(normalizer.output.stmts);
            return 0;
        }
    free(fn->stmts);
    fn->stmts = normalizer.output.stmts;
    fn->stmt_count = normalizer.output.stmt_count;
    fn->stmt_cap = normalizer.output.stmt_cap;
    return 1;
}

static int
emit(ZirFunction *out, Cleanup *entries, int count, int minimum)
{
    for(int i = count - 1; i >= 0 && entries[i].depth >= minimum; i--) {
        const Cleanup *entry = &entries[i];
        if(entry->is_block) {
            if(FunctionAddStmt(out, ZIR_STMT_BLOCK_OPEN, "{",
                               entry->statement.span) == NULL)
                return 0;
            for(int body = 0; body < entry->body_count; body++)
                if(!append(out, &entry->body[body]))
                    return 0;
            if(FunctionAddStmt(out, ZIR_STMT_BLOCK_CLOSE, "}",
                               entry->statement.span) == NULL)
                return 0;
        } else if(!append(out, &entry->statement)) {
            return 0;
        }
    }
    return 1;
}

static int
opens(ZirStmtKind kind)
{
    return kind == ZIR_STMT_BLOCK_OPEN || kind == ZIR_STMT_IF ||
           kind == ZIR_STMT_WHILE || kind == ZIR_STMT_FOR ||
           kind == ZIR_STMT_IF_CASE;
}

static int
opens_stmt(const ZirStmt *st)
{
    return opens(st->kind) ||
           defer_block(st) ||
           (st->kind == ZIR_STMT_CASE && strchr(st->text, '{') != NULL);
}

static int
end_block(const ZirFunction *fn, int start, int end)
{
    int depth = 1;
    for(int i = start + 1; i < end; i++) {
        if(opens_stmt(&fn->stmts[i])) depth++;
        if(fn->stmts[i].kind == ZIR_STMT_BLOCK_CLOSE && !--depth) return i;
    }
    return end;
}

static int
falls_through(const ZirFunction *fn, int start, int end)
{
    for(int i = start; i < end; i++) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(defer_block(&fn->stmts[i])) {
            i = end_block(fn, i, end);
            continue;
        }
        if(kind == ZIR_STMT_RETURN || kind == ZIR_STMT_BREAK || kind == ZIR_STMT_CONTINUE)
            return 0;
        if(opens_stmt(&fn->stmts[i])) {
            int close = end_block(fn, i, end);
            int falls = falls_through(fn, i + 1, close);
            if(kind == ZIR_STMT_BLOCK_OPEN && !falls) return 0;
            if(kind == ZIR_STMT_IF) {
                int has_else = 0;
                while(close + 1 < end && fn->stmts[close + 1].kind == ZIR_STMT_IF &&
                      strncmp(fn->stmts[close + 1].text, "else", 4) == 0) {
                    int next = close + 1;
                    has_else = strncmp(skip_ws(fn->stmts[next].text + 4), "if", 2) != 0;
                    close = end_block(fn, next, end);
                    falls |= falls_through(fn, next + 1, close);
                }
                if(has_else && !falls) return 0;
            }
            i = close;
        }
    }
    return 1;
}

int
LowerCleanup(ZirFunction *fn)
{
    int has_cleanup = 0, depth = 0, count = 0, serial = 0, ok = 0;
    Cleanup *entries;
    ZirStmtKind *scopes;
    int *scope_loop_id;
    int *scope_start;
    ZirFunction out = {0};

    for(int i = 0; i < fn->stmt_count; i++)
        has_cleanup |= fn->stmts[i].kind == ZIR_STMT_DEFER;
    if(!has_cleanup)
        return 1;
    entries = calloc((size_t)fn->stmt_count + 1, sizeof(*entries));
    scopes = calloc((size_t)fn->stmt_count + 1, sizeof(*scopes));
    scope_loop_id = calloc((size_t)fn->stmt_count + 1,
                           sizeof(*scope_loop_id));
    scope_start = calloc((size_t)fn->stmt_count + 1, sizeof(*scope_start));
    if(!entries || !scopes || !scope_loop_id || !scope_start)
        goto done;
    for(int i = 0; i < fn->stmt_count; i++) {
        const ZirStmt *st = &fn->stmts[i];
        if(st->kind == ZIR_STMT_CASE) {
            if(depth > 0 && scopes[depth] == ZIR_STMT_CASE) {
                if(falls_through(fn, scope_start[depth], i) &&
                   !emit(&out, entries, count, depth))
                    goto done;
                while(count && entries[count - 1].depth >= depth)
                    count--;
                depth--;
            }
            if(depth == 0 || scopes[depth] != ZIR_STMT_IF_CASE) {
                fail(st, "case outside an if-case block");
                goto done;
            }
            if(!append(&out, st)) goto done;
            scopes[++depth] = ZIR_STMT_CASE;
            scope_start[depth] = i + 1;
            continue;
        }
        if(st->kind == ZIR_STMT_DECL) {
            ZirLexer lexer;
            ZirToken name;
            LexerInit(&lexer, st->text, SpanPath(st->span));
            name = LexerNext(&lexer);
            for(int d = 0; d < count; d++) {
                if(cleanup_mentions(&entries[d], name.text)) {
                    fail(st, "a declaration cannot shadow a name referenced by an active defer");
                    goto done;
                }
            }
        }
        if(st->kind == ZIR_STMT_FOR) {
            for(int d = 0; d < count; d++) {
                ZirLexer lexer;
                ZirToken token;
                LexerInit(&lexer, st->text, SpanPath(st->span));
                do {
                    token = LexerNext(&lexer);
                    if(token.kind == ZIR_TOKEN_IDENT &&
                       cleanup_mentions(&entries[d], token.text)) {
                        fail(st, "a for header cannot reuse a name referenced by an active defer; use a while loop");
                        goto done;
                    }
                } while(token.kind != ZIR_TOKEN_EOF);
            }
        }
        if(st->kind == ZIR_STMT_DEFER) {
            const char *body = skip_ws(st->text + 5);
            Cleanup *entry = &entries[count];
            ZirStmt *action = &entry->statement;
            if(*body == '{') {
                const char *after_open = skip_ws(body + 1);
                if(*after_open == '}' && *skip_ws(after_open + 1) == '\0')
                    continue;
            }
            if(defer_block(st)) {
                int close = end_block(fn, i, fn->stmt_count);
                if(close >= fn->stmt_count) {
                    fail(st, "unterminated defer block");
                    goto done;
                }
                for(int body_index = i + 1; body_index < close; body_index++) {
                    ZirStmtKind kind = fn->stmts[body_index].kind;
                    if(kind == ZIR_STMT_DEFER || kind == ZIR_STMT_RETURN ||
                       kind == ZIR_STMT_BREAK || kind == ZIR_STMT_CONTINUE) {
                        fail(&fn->stmts[body_index],
                             "unsupported control flow inside a defer block");
                        goto done;
                    }
                }
                entry->statement = *st;
                entry->body = &fn->stmts[i + 1];
                entry->body_count = close - i - 1;
                entry->is_block = 1;
                entry->depth = depth;
                count++;
                i = close;
                continue;
            }
            /* Cleanup actions are expressions or assignments, never jumps or
             * declarations. Keep their source span for backend diagnostics. */
            *action = *st;
            if(!action_kind(body, action)) {
                fail(st, "defer requires one expression or assignment");
                goto done;
            }
            action->expr_root = -1;
            copy_text(action->text, sizeof(action->text), body);
            entry->depth = depth;
            count++;
            continue;
        }
        if(st->kind == ZIR_STMT_RETURN && count) {
            ZirStmt result = *st;
            const char *value = skip_ws(st->text + 6);
            if(*value && strcmp(value, ";")) {
                char name[ZIR_NAME_MAX];
                int collision;
                /* Reserve a fresh local without reserving user identifiers. */
                do {
                    snprintf(name, sizeof(name), "cleanup_return_%d", serial++);
                    collision = strstr(fn->args, name) != NULL;
                    for(int k = 0; k < fn->stmt_count; k++)
                        collision |= strstr(fn->stmts[k].text, name) != NULL;
                } while(collision);
                result.kind = ZIR_STMT_DECL;
                result.expr_root = st->expr_root;
                if(snprintf(result.text, sizeof(result.text), "%s: %s = %s",
                            name, fn->return_type, value) >= (int)sizeof(result.text)) {
                    fail(st, "return expression exceeds cleanup lowering limit");
                    goto done;
                }
                if(!append(&out, &result))
                    goto done;
                result.kind = ZIR_STMT_RETURN;
                result.expr_root = -1;
                snprintf(result.text, sizeof(result.text), "return %s", name);
            }
            if(!emit(&out, entries, count, 0) || !append(&out, &result))
                goto done;
            continue;
        }
        if(st->kind == ZIR_STMT_BREAK || st->kind == ZIR_STMT_CONTINUE) {
            int target = depth;
            while(target > 0 && (st->target_id ?
                  scope_loop_id[target] != st->target_id :
                  (scopes[target] != ZIR_STMT_FOR &&
                   scopes[target] != ZIR_STMT_WHILE)))
                target--;
            if(!target) {
                fail(st, "loop control outside a loop");
                goto done;
            }
            if(!emit(&out, entries, count, target))
                goto done;
        }
        if((strcmp(st->text, "#through") == 0 ||
            strcmp(st->text, "#through;") == 0) &&
           depth > 0 && scopes[depth] == ZIR_STMT_CASE) {
            if(!emit(&out, entries, count, depth)) goto done;
            while(count && entries[count - 1].depth >= depth)
                count--;
            scope_start[depth] = i + 1;
        }
        if(st->kind == ZIR_STMT_BLOCK_CLOSE) {
            if(depth > 0 && scopes[depth] == ZIR_STMT_CASE) {
                if(falls_through(fn, scope_start[depth], i) &&
                   !emit(&out, entries, count, depth))
                    goto done;
                while(count && entries[count - 1].depth >= depth)
                    count--;
                depth--;
            }
            if(!depth) {
                fail(st, "unbalanced cleanup scope");
                goto done;
            }
            if(falls_through(fn, scope_start[depth], i) &&
               !emit(&out, entries, count, depth))
                goto done;
            while(count && entries[count - 1].depth >= depth)
                count--;
            depth--;
        }
        if(!append(&out, st))
            goto done;
        if(opens_stmt(st)) {
            scopes[++depth] = st->kind;
            scope_loop_id[depth] = st->loop_id;
            scope_start[depth] = i + 1;
        }
    }
    if(falls_through(fn, 0, fn->stmt_count) && !emit(&out, entries, count, 0))
        goto done;
    free(fn->stmts);
    fn->stmts = out.stmts;
    fn->stmt_count = out.stmt_count;
    fn->stmt_cap = out.stmt_cap;
    out.stmts = NULL;
    ok = 1;
done:
    free(out.stmts);
    free(entries);
    free(scopes);
    free(scope_loop_id);
    free(scope_start);
    return ok;
}

static int
range_identifier(const char *name)
{
    if(!isalpha((unsigned char)*name) && *name != '_') return 0;
    for(const char *p = name + 1; *p; p++)
        if(!isalnum((unsigned char)*p) && *p != '_') return 0;
    return 1;
}

static int
range_header(const ZirStmt *statement, char *name, char *start, char *end,
             int *reverse)
{
    char header[ZIR_TEXT_MAX];
    const char *source = skip_ws(statement->text + 3);
    *reverse = *source == '<';
    if(*reverse) source = skip_ws(source + 1);
    copy_text(header, sizeof(header), source);
    trim_in_place(header);
    size_t length = strlen(header);
    if(length == 0 || header[length - 1] != '{') return 0;
    header[length - 1] = '\0';
    trim_in_place(header);
    int nesting = 0, quote = 0, escaped = 0;
    int colon = -1, dots = -1;
    for(int i = 0; header[i]; i++) {
        char ch = header[i];
        if(quote) {
            if(escaped) escaped = 0;
            else if(ch == '\\') escaped = 1;
            else if(ch == quote) quote = 0;
            continue;
        }
        if(ch == '\'' || ch == '"') { quote = ch; continue; }
        if(ch == '(' || ch == '[') { nesting++; continue; }
        if(ch == ')' || ch == ']') { nesting--; continue; }
        if(nesting != 0) continue;
        if(ch == ':' && colon < 0 && dots < 0) colon = i;
        if(ch == '.' && header[i + 1] == '.') {
            if(dots >= 0) return 0;
            dots = i;
            i++;
        }
    }
    if(quote || nesting != 0 || dots < 0 ||
       (colon >= 0 && colon > dots)) return 0;
    if(colon >= 0) {
        header[colon] = '\0';
        copy_text(name, ZIR_NAME_MAX, header);
        trim_in_place(name);
        if(!range_identifier(name)) return 0;
    } else {
        copy_text(name, ZIR_NAME_MAX, "it");
    }
    header[dots] = '\0';
    copy_text(start, ZIR_TEXT_MAX, skip_ws(header + (colon >= 0 ? colon + 1 : 0)));
    copy_text(end, ZIR_TEXT_MAX, skip_ws(header + dots + 2));
    trim_in_place(start);
    trim_in_place(end);
    return *start && *end;
}

static int
collection_header(const ZirStmt *statement, char *value_name,
                  char *index_name, char *collection, int *reverse,
                  int *pointer)
{
    char header[ZIR_TEXT_MAX], names[ZIR_TEXT_MAX];
    const char *source = skip_ws(statement->text + 3);
    *reverse = *pointer = 0;
    while(*source == '<' || *source == '*') {
        if(*source == '<') {
            if(*reverse) return 0;
            *reverse = 1;
        } else {
            if(*pointer) return 0;
            *pointer = 1;
        }
        source = skip_ws(source + 1);
    }
    copy_text(header, sizeof(header), source);
    trim_in_place(header);
    size_t length = strlen(header);
    if(length == 0 || header[length - 1] != '{') return 0;
    header[length - 1] = '\0';
    trim_in_place(header);
    int nesting = 0, quote = 0, escaped = 0, colon = -1;
    for(int i = 0; header[i]; i++) {
        char ch = header[i];
        if(quote) {
            if(escaped) escaped = 0;
            else if(ch == '\\') escaped = 1;
            else if(ch == quote) quote = 0;
            continue;
        }
        if(ch == '\'' || ch == '"') { quote = ch; continue; }
        if(ch == '(' || ch == '[' || ch == '{') { nesting++; continue; }
        if(ch == ')' || ch == ']' || ch == '}') { nesting--; continue; }
        if(nesting != 0) continue;
        if(ch == '.' && header[i + 1] == '.') return 0;
        if(ch == ':') {
            if(colon >= 0) return 0;
            colon = i;
        }
    }
    if(quote || nesting != 0) return 0;
    copy_text(value_name, ZIR_NAME_MAX, "it");
    copy_text(index_name, ZIR_NAME_MAX, "it_index");
    if(colon >= 0) {
        header[colon] = '\0';
        copy_text(names, sizeof(names), header);
        trim_in_place(names);
        char *comma = strchr(names, ',');
        if(comma != NULL) {
            *comma++ = '\0';
            if(strchr(comma, ',') != NULL) return 0;
            copy_text(index_name, ZIR_NAME_MAX, comma);
            trim_in_place(index_name);
        }
        copy_text(value_name, ZIR_NAME_MAX, names);
        trim_in_place(value_name);
        if(!range_identifier(value_name) ||
           !range_identifier(index_name) ||
           strcmp(value_name, index_name) == 0) return 0;
        copy_text(collection, ZIR_TEXT_MAX, skip_ws(header + colon + 1));
    } else {
        copy_text(collection, ZIR_TEXT_MAX, header);
    }
    trim_in_place(collection);
    return *collection != '\0';
}

static int
named_while_header(const ZirStmt *statement, char *name,
                   char *condition)
{
    char header[ZIR_TEXT_MAX];
    copy_text(header, sizeof(header), skip_ws(statement->text + 5));
    trim_in_place(header);
    size_t length = strlen(header);
    if(length == 0 || header[length - 1] != '{')
        return 0;
    header[length - 1] = '\0';
    trim_in_place(header);
    char *assignment = strstr(header, ":=");
    if(assignment == NULL)
        return 0;
    *assignment = '\0';
    copy_text(name, ZIR_NAME_MAX, header);
    trim_in_place(name);
    copy_text(condition, ZIR_TEXT_MAX, skip_ws(assignment + 2));
    trim_in_place(condition);
    return range_identifier(name) && *condition;
}

typedef struct LoopScope {
    ZirStmtKind kind;
    int loop_id;
    char name[ZIR_NAME_MAX];
} LoopScope;

static int range_line(ZirFunction *out, ZirStmtKind kind,
                      const char *line, ZirSourceSpan span);

static int
control_name(const ZirStmt *statement, char *name)
{
    const char *keyword = statement->kind == ZIR_STMT_BREAK ?
        "break" : "continue";
    const char *cursor = skip_ws(statement->text + strlen(keyword));
    size_t length = 0;
    name[0] = '\0';
    if(*cursor == ';')
        cursor = skip_ws(cursor + 1);
    else if(*cursor != '\0') {
        while(isalnum((unsigned char)cursor[length]) ||
              cursor[length] == '_')
            length++;
        if(length == 0 || length >= ZIR_NAME_MAX)
            return 0;
        memcpy(name, cursor, length);
        name[length] = '\0';
        if(!range_identifier(name))
            return 0;
        cursor = skip_ws(cursor + length);
        if(*cursor == ';')
            cursor = skip_ws(cursor + 1);
    }
    return *cursor == '\0';
}

static int
lower_named_while(ZirFunction *fn, int index, const char *name,
                  const char *condition)
{
    ZirFunction output = {0};
    ZirStmt header = fn->stmts[index];
    char declaration[ZIR_TEXT_MAX];
    char check[ZIR_TEXT_MAX];
    int ok = 0;
    if(snprintf(declaration, sizeof(declaration), "%s: bool = %s",
                name, condition) >= (int)sizeof(declaration) ||
       snprintf(check, sizeof(check), "if !%s {", name) >=
           (int)sizeof(check))
        return fail(&header, "named while condition exceeds statement limit");
    for(int i = 0; i < index; i++)
        if(!append(&output, &fn->stmts[i])) goto done;
    copy_text(header.text, sizeof(header.text), "while true {");
    if(!append(&output, &header) ||
       !range_line(&output, ZIR_STMT_DECL, declaration, header.span) ||
       !range_line(&output, ZIR_STMT_IF, check, header.span) ||
       !range_line(&output, ZIR_STMT_BREAK, "break", header.span) ||
       !range_line(&output, ZIR_STMT_BLOCK_CLOSE, "}", header.span))
        goto done;
    for(int i = index + 1; i < fn->stmt_count; i++)
        if(!append(&output, &fn->stmts[i])) goto done;
    free(fn->stmts);
    fn->stmts = output.stmts;
    fn->stmt_count = output.stmt_count;
    fn->stmt_cap = output.stmt_cap;
    output.stmts = NULL;
    ok = 1;
done:
    free(output.stmts);
    return ok;
}

int
BindJaiLoopControls(ZirFunction *fn)
{
    LoopScope *scopes = calloc((size_t)fn->stmt_count + 1, sizeof(*scopes));
    int depth = 0, next_id = 1, ok = 0;
    if(scopes == NULL)
        return 0;
    for(int i = 0; i < fn->stmt_count; i++) {
        ZirStmt *statement = &fn->stmts[i];
        if(statement->kind == ZIR_STMT_BLOCK_CLOSE) {
            if(depth == 0) {
                fail(statement, "unexpected block close");
                goto done;
            }
            depth--;
            continue;
        }
        if(statement->kind == ZIR_STMT_BREAK ||
           statement->kind == ZIR_STMT_CONTINUE) {
            char target[ZIR_NAME_MAX];
            if(!control_name(statement, target)) {
                fail(statement, "invalid named loop control");
                goto done;
            }
            int scope = depth - 1;
            for(; scope >= 0; scope--)
                if((scopes[scope].kind == ZIR_STMT_FOR ||
                    scopes[scope].kind == ZIR_STMT_WHILE) &&
                   (!*target || !strcmp(scopes[scope].name, target)))
                    break;
            if(scope < 0) {
                fail(statement, *target ?
                    "named loop target is not an enclosing loop" :
                    "loop control outside a loop");
                goto done;
            }
            statement->target_id = *target ? scopes[scope].loop_id : 0;
        }
        int opens_scope = opens_stmt(statement);
        if(!opens_scope)
            continue;
        LoopScope *scope = &scopes[depth++];
        scope->kind = statement->kind;
        if(statement->kind == ZIR_STMT_FOR) {
            char name[ZIR_NAME_MAX], second[ZIR_NAME_MAX];
            char first[ZIR_TEXT_MAX], last[ZIR_TEXT_MAX];
            int reverse, pointer;
            if(range_header(statement, name, first, last, &reverse) ||
               collection_header(statement, name, second, first,
                                 &reverse, &pointer)) {
                copy_text(scope->name, sizeof(scope->name), name);
                scope->loop_id = statement->loop_id = next_id++;
            }
        } else if(statement->kind == ZIR_STMT_WHILE) {
            char condition[ZIR_TEXT_MAX];
            if(named_while_header(statement, scope->name, condition))
                scope->loop_id = statement->loop_id = next_id++;
        }
    }
    if(depth != 0) {
        fail(&fn->stmts[fn->stmt_count - 1], "unterminated loop scope");
        goto done;
    }
    for(int i = fn->stmt_count - 1; i >= 0; i--) {
        char name[ZIR_NAME_MAX], condition[ZIR_TEXT_MAX];
        if(fn->stmts[i].kind == ZIR_STMT_WHILE &&
           named_while_header(&fn->stmts[i], name, condition) &&
           !lower_named_while(fn, i, name, condition))
            goto done;
    }
    ok = 1;
done:
    free(scopes);
    return ok;
}

static int
range_name_used(const ZirFunction *fn, const ZirModule *module,
                const char *name)
{
    if(mentions(fn->args, name)) return 1;
    for(int i = 0; i < fn->stmt_count; i++)
        if(mentions(fn->stmts[i].text, name)) return 1;
    for(int i = 0; i < module->define_count; i++)
        if(strcmp(module->defines[i].name, name) == 0) return 1;
    for(int i = 0; i < module->global_count; i++)
        if(strcmp(module->globals[i].name, name) == 0) return 1;
    return 0;
}

static int
range_line(ZirFunction *out, ZirStmtKind kind, const char *line,
           ZirSourceSpan span)
{
    return FunctionAddStmt(out, kind, line, span) != NULL;
}

static int
range_advance(ZirFunction *out, const char *cursor,
              const char *terminal, int reverse, ZirSourceSpan span,
              int target_id)
{
    char line[ZIR_TEXT_MAX];
    /* Stepping past a constant bound cannot overflow, so only a computed
     * bound stops at the last value before it steps. */
    if(*terminal && (snprintf(line, sizeof(line), "if %s == %s {", cursor, terminal) >=
       (int)sizeof(line) ||
       !range_line(out, ZIR_STMT_IF, line, span) ||
       !range_line(out, ZIR_STMT_BREAK, "break", span) ||
       !range_line(out, ZIR_STMT_BLOCK_CLOSE, "}", span)))
        return 0;
    if(*terminal)
        out->stmts[out->stmt_count - 2].target_id = target_id;
    if(snprintf(line, sizeof(line), "%s %s= 1", cursor,
                reverse ? "-" : "+") >= (int)sizeof(line))
        return 0;
    return range_line(out, ZIR_STMT_ASSIGN, line, span);
}

static int
loop_advance(ZirFunction *out, const char *cursor, const char *terminal,
             int reverse, int is_range, ZirSourceSpan span, int target_id)
{
    char line[ZIR_TEXT_MAX];
    if(is_range)
        return range_advance(out, cursor, terminal, reverse, span,
                             target_id);
    if(snprintf(line, sizeof(line), "%s += 1", cursor) >= (int)sizeof(line))
        return 0;
    return range_line(out, ZIR_STMT_ASSIGN, line, span);
}

static int
copy_loop_body(ZirFunction *out, const ZirFunction *fn, int index, int close,
               const char *cursor, const char *terminal, int reverse,
               int is_range, int loop_id)
{
    ZirStmtKind *scopes = calloc((size_t)(close - index + 1), sizeof(*scopes));
    if(scopes == NULL) return 0;
    int depth = 0, ok = 1;
    for(int i = index + 1; i < close; i++) {
        const ZirStmt *statement = &fn->stmts[i];
        if(statement->kind == ZIR_STMT_BLOCK_CLOSE && depth > 0) depth--;
        if(statement->kind == ZIR_STMT_CONTINUE) {
            int nested_loop = 0;
            for(int scope = depth - 1; scope >= 0; scope--)
                if(scopes[scope] == ZIR_STMT_WHILE ||
                   scopes[scope] == ZIR_STMT_FOR) {
                    nested_loop = 1;
                    break;
                }
            if(((statement->target_id == loop_id && loop_id != 0) ||
                (statement->target_id == 0 && !nested_loop)) &&
               !loop_advance(out, cursor, terminal, reverse, is_range,
                             statement->span, statement->target_id)) {
                ok = 0;
                break;
            }
        }
        if(!append(out, statement)) {
            ok = 0;
            break;
        }
        if(opens_stmt(statement)) scopes[depth++] = statement->kind;
    }
    free(scopes);
    return ok;
}

/* Whether the loop body between index and close names name, or might
 * change it: assign to it or take its address. */
static int
body_mentions(const ZirFunction *fn, int index, int close, const char *name)
{
    for(int i = index + 1; i < close; i++)
        if(mentions(fn->stmts[i].text, name)) return 1;
    return 0;
}

static int
body_changes(const ZirFunction *fn, int index, int close, const char *name)
{
    for(int i = index + 1; i < close; i++) {
        ZirLexer lexer;
        ZirToken token;
        int first = 1, address = 0;
        LexerInit(&lexer, fn->stmts[i].text, "");
        do {
            token = LexerNext(&lexer);
            if(token.kind == ZIR_TOKEN_IDENT && !strcmp(token.text, name) &&
               (address || (first && fn->stmts[i].kind == ZIR_STMT_ASSIGN)))
                return 1;
            address = token.kind != ZIR_TOKEN_EOF && !strcmp(token.text, "*");
            first = 0;
        } while(token.kind != ZIR_TOKEN_EOF);
    }
    return 0;
}

/* An integer literal bound, which the loop can use in place. */
static int
literal_bound(const char *text, long long *value)
{
    char *end;
    while(*text == ' ') text++;
    if(!*text) return 0;
    errno = 0;
    *value = strtoll(text, &end, 0);
    while(*end == ' ') end++;
    return errno == 0 && *end == '\0';
}

static int
lower_one_range(ZirFunction *fn, const ZirModule *module, int index)
{
    const ZirStmt *header = &fn->stmts[index];
    int close = end_block(fn, index, fn->stmt_count);
    char binder[ZIR_NAME_MAX], start[ZIR_TEXT_MAX], end[ZIR_TEXT_MAX];
    char first[ZIR_NAME_MAX], last[ZIR_NAME_MAX], cursor[ZIR_NAME_MAX];
    char line[ZIR_TEXT_MAX];
    int reverse;
    if(strchr(header->text, ';') != NULL)
        return fail(header, "C-style for headers are not Jai syntax");
    if(close >= fn->stmt_count ||
       !range_header(header, binder, start, end, &reverse))
        return fail(header,
            "for currently supports Jai integer ranges: for index: first..last { ... }");
    if(strcmp(binder, "it_index") == 0)
        return fail(header, "for range binder cannot be it_index");
    long long low, high;
    /* Constant bounds read in place, and a constant last value leaves room
     * to step past it. The binder counts itself unless the body changes it. */
    int constant = literal_bound(start, &low) && literal_bound(end, &high) &&
                   (reverse ? low > LLONG_MIN : high < LLONG_MAX);
    int own_cursor = !body_changes(fn, index, close, binder);
    int wants_index = body_mentions(fn, index, close, "it_index");
    /* Parallel regions keep the full form their workers are built from. */
    if(header->is_parallel) {
        constant = 0;
        own_cursor = 0;
        wants_index = 1;
    }
    for(int serial = index;; serial++) {
        snprintf(first, sizeof(first), "range_first_%d", serial);
        snprintf(last, sizeof(last), "range_last_%d", serial);
        snprintf(cursor, sizeof(cursor), "range_cursor_%d", serial);
        if(!range_name_used(fn, module, first) &&
           !range_name_used(fn, module, last) &&
           !range_name_used(fn, module, cursor)) break;
    }
    if(constant) {
        snprintf(first, sizeof(first), "%lld", low);
        snprintf(last, sizeof(last), "%lld", high);
    }
    if(own_cursor)
        snprintf(cursor, sizeof(cursor), "%s", binder);
    ZirFunction out = {0};
    ZirSourceSpan span = header->span;
    int ok = 0;
    for(int i = 0; i < index; i++)
        if(!append(&out, &fn->stmts[i])) goto done;
    if(!range_line(&out, ZIR_STMT_BLOCK_OPEN, "{", span)) goto done;
    if(!constant &&
       (snprintf(line, sizeof(line), "%s: s64 = %s", first, start) >=
        (int)sizeof(line) || !range_line(&out, ZIR_STMT_DECL, line, span) ||
        snprintf(line, sizeof(line), "%s: s64 = %s", last, end) >=
        (int)sizeof(line) || !range_line(&out, ZIR_STMT_DECL, line, span)))
        goto done;
    if(snprintf(line, sizeof(line), "%s: s64 = %s", cursor,
                reverse ? last : first) >= (int)sizeof(line) ||
       !range_line(&out, ZIR_STMT_DECL, line, span)) goto done;
    if(snprintf(line, sizeof(line), "while %s %s %s {", cursor,
                reverse ? ">=" : "<=", reverse ? first : last) >=
       (int)sizeof(line) || !range_line(&out, ZIR_STMT_WHILE, line, span))
        goto done;
    out.stmts[out.stmt_count - 1].loop_id = header->loop_id;
    out.stmts[out.stmt_count - 1].is_parallel = header->is_parallel;
    out.stmts[out.stmt_count - 1].is_gpu = header->is_gpu;
    if(!own_cursor &&
       (snprintf(line, sizeof(line), "%s: s64 = %s", binder, cursor) >=
        (int)sizeof(line) || !range_line(&out, ZIR_STMT_DECL, line, span)))
        goto done;
    if(wants_index) {
        if(constant && !reverse && low == 0)
            snprintf(line, sizeof(line), "it_index: s64 = %s", cursor);
        else if(snprintf(line, sizeof(line), "it_index: s64 = %s - %s",
                         reverse ? last : cursor, reverse ? cursor : first) >=
                (int)sizeof(line))
            goto done;
        if(!range_line(&out, ZIR_STMT_DECL, line, span)) goto done;
    }
    {
        const char *terminal = constant ? "" : reverse ? first : last;
        if(!copy_loop_body(&out, fn, index, close, cursor, terminal, reverse, 1,
                           header->loop_id)) goto done;
        if(!range_advance(&out, cursor, terminal, reverse, span, 0) ||
           !range_line(&out, ZIR_STMT_BLOCK_CLOSE, "}", span) ||
           !range_line(&out, ZIR_STMT_BLOCK_CLOSE, "}", span)) goto done;
    }
    for(int i = close + 1; i < fn->stmt_count; i++)
        if(!append(&out, &fn->stmts[i])) goto done;
    free(fn->stmts);
    fn->stmts = out.stmts;
    fn->stmt_count = out.stmt_count;
    fn->stmt_cap = out.stmt_cap;
    out.stmts = NULL;
    ok = 1;
done:
    free(out.stmts);
    if(!ok) fail(header, "for range lowering exceeds compiler limits");
    return ok;
}

/* A lone identifier, such as a local array or slice. */
static int
plain_name(const char *text)
{
    ZirLexer lexer;
    ZirToken token;
    LexerInit(&lexer, text, "");
    token = LexerNext(&lexer);
    if(token.kind != ZIR_TOKEN_IDENT) return 0;
    return LexerNext(&lexer).kind == ZIR_TOKEN_EOF;
}

static int
lower_one_collection(ZirFunction *fn, const ZirModule *module, int index)
{
    const ZirStmt *header = &fn->stmts[index];
    int close = end_block(fn, index, fn->stmt_count);
    char value_name[ZIR_NAME_MAX], index_name[ZIR_NAME_MAX];
    char collection[ZIR_TEXT_MAX], view[ZIR_NAME_MAX];
    char count[ZIR_NAME_MAX], cursor[ZIR_NAME_MAX], item_index[ZIR_NAME_MAX];
    char line[ZIR_TEXT_MAX];
    int reverse, pointer;
    if(strchr(header->text, ';') != NULL)
        return fail(header, "C-style for headers are not Jai syntax");
    if(close >= fn->stmt_count ||
       !collection_header(header, value_name, index_name, collection,
                          &reverse, &pointer))
        return fail(header,
            "for currently supports Jai integer ranges or array and slice iteration");
    for(int serial = index;; serial++) {
        snprintf(view, sizeof(view), "loop_view_%d", serial);
        snprintf(count, sizeof(count), "loop_count_%d", serial);
        snprintf(cursor, sizeof(cursor), "loop_cursor_%d", serial);
        snprintf(item_index, sizeof(item_index), "loop_index_%d", serial);
        if(!range_name_used(fn, module, view) &&
           !range_name_used(fn, module, count) &&
           !range_name_used(fn, module, cursor) &&
           !range_name_used(fn, module, item_index)) break;
    }
    /* A forward loop over a name the body leaves alone indexes it
     * directly, counting with its own index name. */
    int direct = !reverse && !header->is_parallel && plain_name(collection) &&
                 !body_changes(fn, index, close, collection) &&
                 !body_changes(fn, index, close, index_name);
    if(direct) {
        snprintf(view, sizeof(view), "%s", collection);
        snprintf(count, sizeof(count), "%s.count", collection);
        snprintf(cursor, sizeof(cursor), "%s", index_name);
        snprintf(item_index, sizeof(item_index), "%s", index_name);
    }
    ZirFunction out = {0};
    ZirSourceSpan span = header->span;
    int ok = 0;
    for(int i = 0; i < index; i++)
        if(!append(&out, &fn->stmts[i])) goto done;
    if(!range_line(&out, ZIR_STMT_BLOCK_OPEN, "{", span)) goto done;
    if(!direct &&
       (snprintf(line, sizeof(line), "%s := %s[:]", view, collection) >=
        (int)sizeof(line) || !range_line(&out, ZIR_STMT_DECL, line, span) ||
        snprintf(line, sizeof(line), "%s: s64 = %s.count", count, view) >=
        (int)sizeof(line) || !range_line(&out, ZIR_STMT_DECL, line, span)))
        goto done;
    if(snprintf(line, sizeof(line), "%s: s64 = 0", cursor) >=
       (int)sizeof(line) || !range_line(&out, ZIR_STMT_DECL, line, span))
        goto done;
    if(snprintf(line, sizeof(line), "while %s < %s {", cursor, count) >=
       (int)sizeof(line) || !range_line(&out, ZIR_STMT_WHILE, line, span))
        goto done;
    out.stmts[out.stmt_count - 1].loop_id = header->loop_id;
    out.stmts[out.stmt_count - 1].is_parallel = header->is_parallel;
    out.stmts[out.stmt_count - 1].is_gpu = header->is_gpu;
    if(!direct) {
        if(snprintf(line, sizeof(line), "%s: s64 = %s", item_index,
                    reverse ? "0" : cursor) >= (int)sizeof(line)) goto done;
        if(reverse && snprintf(line, sizeof(line), "%s: s64 = %s - %s - 1",
                               item_index, count, cursor) >= (int)sizeof(line))
            goto done;
        if(!range_line(&out, ZIR_STMT_DECL, line, span)) goto done;
    }
    if(snprintf(line, sizeof(line), "%s := %s%s[%s]", value_name,
                pointer ? "*" : "", view, item_index) >=
       (int)sizeof(line) || !range_line(&out, ZIR_STMT_DECL, line, span))
        goto done;
    if(!direct && body_mentions(fn, index, close, index_name) &&
       (snprintf(line, sizeof(line), "%s: s64 = %s", index_name,
                 item_index) >= (int)sizeof(line) ||
        !range_line(&out, ZIR_STMT_DECL, line, span))) goto done;
    if(!copy_loop_body(&out, fn, index, close, cursor, "", 0, 0,
                       header->loop_id) ||
       !loop_advance(&out, cursor, "", 0, 0, span, 0) ||
       !range_line(&out, ZIR_STMT_BLOCK_CLOSE, "}", span) ||
       !range_line(&out, ZIR_STMT_BLOCK_CLOSE, "}", span)) goto done;
    for(int i = close + 1; i < fn->stmt_count; i++)
        if(!append(&out, &fn->stmts[i])) goto done;
    free(fn->stmts);
    fn->stmts = out.stmts;
    fn->stmt_count = out.stmt_count;
    fn->stmt_cap = out.stmt_cap;
    out.stmts = NULL;
    ok = 1;
done:
    free(out.stmts);
    if(!ok) fail(header, "for collection lowering exceeds compiler limits");
    return ok;
}

int
LowerJaiFor(ZirFunction *fn, const ZirModule *module)
{
    for(int i = fn->stmt_count - 1; i >= 0; i--) {
        if(fn->stmts[i].kind != ZIR_STMT_FOR) continue;
        char binder[ZIR_NAME_MAX], first[ZIR_TEXT_MAX], last[ZIR_TEXT_MAX];
        int reverse;
        if(range_header(&fn->stmts[i], binder, first, last, &reverse)) {
            if(!lower_one_range(fn, module, i)) return 0;
        } else if(!lower_one_collection(fn, module, i)) {
            return 0;
        }
    }
    return 1;
}
