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
/* Buffers action_kind keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ActionKindBuffers {
    ZirToken token;
    ZirFunction temporary;
} ActionKindBuffers;

static int action_kind(const char *text, ZirStmt *action);

static int
action_kind_with_buffers(const char *text, ZirStmt *action, ActionKindBuffers *buffers)
{
    ZirLexer lexer;
    memset(&buffers->temporary, 0, sizeof(buffers->temporary));
    int valid;
    action->kind = ZIR_STMT_EXPR;
    LexerInit(&lexer, text, SpanPath(action->span));
    buffers->token = LexerNext(&lexer);
    if(!strcmp(buffers->token.text, "return") || !strcmp(buffers->token.text, "break") ||
       !strcmp(buffers->token.text, "continue") || !strcmp(buffers->token.text, "goto") ||
       !strcmp(buffers->token.text, "defer") || !strcmp(buffers->token.text, "if") ||
       !strcmp(buffers->token.text, "while") || !strcmp(buffers->token.text, "for")) return 0;
    LexerInit(&lexer, text, SpanPath(action->span));
    do {
        buffers->token = LexerNext(&lexer);
        if(!strcmp(buffers->token.text, "=") || !strcmp(buffers->token.text, "+=") ||
           !strcmp(buffers->token.text, "-=") || !strcmp(buffers->token.text, "*=") ||
           !strcmp(buffers->token.text, "/=") || !strcmp(buffers->token.text, "%=") ||
           !strcmp(buffers->token.text, "&=") || !strcmp(buffers->token.text, "|=") ||
           !strcmp(buffers->token.text, "^=") || !strcmp(buffers->token.text, "<<=") || !strcmp(buffers->token.text, ">>="))
            action->kind = ZIR_STMT_ASSIGN;
    } while(buffers->token.kind != ZIR_TOKEN_EOF);
    action->text = KeepText(text);
    if(!append(&buffers->temporary, action)) return 0;
    StructureFunction(&buffers->temporary, NULL);
    valid = buffers->temporary.stmts[0].expr_root >= 0 &&
            buffers->temporary.exprs[buffers->temporary.stmts[0].expr_root].kind != ZIR_EXPR_UNKNOWN;
    free(buffers->temporary.stmts); free(buffers->temporary.exprs);
    return valid;
}

static int
action_kind(const char *text, ZirStmt *action)
{
    static _Thread_local ActionKindBuffers *spares[16];
    static _Thread_local int spare_count;
    ActionKindBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = action_kind_with_buffers(text, action, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
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
        char opened[ZIR_TEXT_MAX];
        if(snprintf(opened, sizeof(opened), "%s {", header.text) >=
           (int)sizeof(opened))
            return fail(source, "control header exceeds statement limit");
        header.text = KeepText(opened);
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
    close.text = KeepText("}");
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
/* Buffers LowerCleanup keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerCleanupBuffers {
    ZirFunction out;
    ZirToken name;
    ZirToken token;
    char text[ZIR_TEXT_MAX];
} LowerCleanupBuffers;

int LowerCleanup(ZirFunction *fn);

static int
LowerCleanup_with_buffers(ZirFunction *fn, LowerCleanupBuffers *buffers)
{
    int has_cleanup = 0, depth = 0, count = 0, serial = 0, ok = 0;
    Cleanup *entries;
    ZirStmtKind *scopes;
    int *scope_loop_id;
    int *scope_start;
    memset(&buffers->out, 0, sizeof(buffers->out));
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
                   !emit(&buffers->out, entries, count, depth))
                    goto done;
                while(count && entries[count - 1].depth >= depth)
                    count--;
                depth--;
            }
            if(depth == 0 || scopes[depth] != ZIR_STMT_IF_CASE) {
                fail(st, "case outside an if-case block");
                goto done;
            }
            if(!append(&buffers->out, st)) goto done;
            scopes[++depth] = ZIR_STMT_CASE;
            scope_start[depth] = i + 1;
            continue;
        }
        if(st->kind == ZIR_STMT_DECL) {
            ZirLexer lexer;
            LexerInit(&lexer, st->text, SpanPath(st->span));
            buffers->name = LexerNext(&lexer);
            for(int d = 0; d < count; d++) {
                if(cleanup_mentions(&entries[d], buffers->name.text)) {
                    fail(st, "a declaration cannot shadow a name referenced by an active defer");
                    goto done;
                }
            }
        }
        if(st->kind == ZIR_STMT_FOR) {
            for(int d = 0; d < count; d++) {
                ZirLexer lexer;
                LexerInit(&lexer, st->text, SpanPath(st->span));
                do {
                    buffers->token = LexerNext(&lexer);
                    if(buffers->token.kind == ZIR_TOKEN_IDENT &&
                       cleanup_mentions(&entries[d], buffers->token.text)) {
                        fail(st, "a for header cannot reuse a name referenced by an active defer; use a while loop");
                        goto done;
                    }
                } while(buffers->token.kind != ZIR_TOKEN_EOF);
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
            action->text = KeepText(body);
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
                if(snprintf(buffers->text, sizeof(buffers->text), "%s: %s = %s",
                            name, fn->return_type, value) >= (int)sizeof(buffers->text)) {
                    fail(st, "return expression exceeds cleanup lowering limit");
                    goto done;
                }
                result.text = KeepText(buffers->text);
                if(!append(&buffers->out, &result))
                    goto done;
                result.kind = ZIR_STMT_RETURN;
                result.expr_root = -1;
                snprintf(buffers->text, sizeof(buffers->text), "return %s", name);
                result.text = KeepText(buffers->text);
            }
            if(!emit(&buffers->out, entries, count, 0) || !append(&buffers->out, &result))
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
            if(!emit(&buffers->out, entries, count, target))
                goto done;
        }
        if((strcmp(st->text, "#through") == 0 ||
            strcmp(st->text, "#through;") == 0) &&
           depth > 0 && scopes[depth] == ZIR_STMT_CASE) {
            if(!emit(&buffers->out, entries, count, depth)) goto done;
            while(count && entries[count - 1].depth >= depth)
                count--;
            scope_start[depth] = i + 1;
        }
        if(st->kind == ZIR_STMT_BLOCK_CLOSE) {
            if(depth > 0 && scopes[depth] == ZIR_STMT_CASE) {
                if(falls_through(fn, scope_start[depth], i) &&
                   !emit(&buffers->out, entries, count, depth))
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
               !emit(&buffers->out, entries, count, depth))
                goto done;
            while(count && entries[count - 1].depth >= depth)
                count--;
            depth--;
        }
        if(!append(&buffers->out, st))
            goto done;
        if(opens_stmt(st)) {
            scopes[++depth] = st->kind;
            scope_loop_id[depth] = st->loop_id;
            scope_start[depth] = i + 1;
        }
    }
    if(falls_through(fn, 0, fn->stmt_count) && !emit(&buffers->out, entries, count, 0))
        goto done;
    free(fn->stmts);
    fn->stmts = buffers->out.stmts;
    fn->stmt_count = buffers->out.stmt_count;
    fn->stmt_cap = buffers->out.stmt_cap;
    buffers->out.stmts = NULL;
    ok = 1;
done:
    free(buffers->out.stmts);
    free(entries);
    free(scopes);
    free(scope_loop_id);
    free(scope_start);
    return ok;
}

int
LowerCleanup(ZirFunction *fn)
{
    static _Thread_local LowerCleanupBuffers *spares[16];
    static _Thread_local int spare_count;
    LowerCleanupBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = LowerCleanup_with_buffers(fn, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
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
/* Buffers collection_header keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CollectionHeaderBuffers {
    char header[ZIR_TEXT_MAX];
    char names[ZIR_TEXT_MAX];
} CollectionHeaderBuffers;

static int collection_header(const ZirStmt *statement, char *value_name,
                  char *index_name, char *collection, int *reverse,
                  int *pointer);

static int
collection_header_with_buffers(const ZirStmt *statement, char *value_name,
                  char *index_name, char *collection, int *reverse,
                  int *pointer, CollectionHeaderBuffers *buffers)
{
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
    copy_text(buffers->header, sizeof(buffers->header), source);
    trim_in_place(buffers->header);
    size_t length = strlen(buffers->header);
    if(length == 0 || buffers->header[length - 1] != '{') return 0;
    buffers->header[length - 1] = '\0';
    trim_in_place(buffers->header);
    int nesting = 0, quote = 0, escaped = 0, colon = -1;
    for(int i = 0; buffers->header[i]; i++) {
        char ch = buffers->header[i];
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
        if(ch == '.' && buffers->header[i + 1] == '.') return 0;
        if(ch == ':') {
            if(colon >= 0) return 0;
            colon = i;
        }
    }
    if(quote || nesting != 0) return 0;
    copy_text(value_name, ZIR_NAME_MAX, "it");
    copy_text(index_name, ZIR_NAME_MAX, "it_index");
    if(colon >= 0) {
        buffers->header[colon] = '\0';
        copy_text(buffers->names, sizeof(buffers->names), buffers->header);
        trim_in_place(buffers->names);
        char *comma = strchr(buffers->names, ',');
        if(comma != NULL) {
            *comma++ = '\0';
            if(strchr(comma, ',') != NULL) return 0;
            copy_text(index_name, ZIR_NAME_MAX, comma);
            trim_in_place(index_name);
        }
        copy_text(value_name, ZIR_NAME_MAX, buffers->names);
        trim_in_place(value_name);
        if(!range_identifier(value_name) ||
           !range_identifier(index_name) ||
           strcmp(value_name, index_name) == 0) return 0;
        copy_text(collection, ZIR_TEXT_MAX, skip_ws(buffers->header + colon + 1));
    } else {
        copy_text(collection, ZIR_TEXT_MAX, buffers->header);
    }
    trim_in_place(collection);
    return *collection != '\0';
}

static int
collection_header(const ZirStmt *statement, char *value_name,
                  char *index_name, char *collection, int *reverse,
                  int *pointer)
{
    static _Thread_local CollectionHeaderBuffers *spares[16];
    static _Thread_local int spare_count;
    CollectionHeaderBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = collection_header_with_buffers(statement, value_name, index_name, collection, reverse, pointer, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
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
/* Buffers lower_named_while keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerNamedWhileBuffers {
    ZirFunction output;
    char declaration[ZIR_TEXT_MAX];
    char check[ZIR_TEXT_MAX];
} LowerNamedWhileBuffers;

static int lower_named_while(ZirFunction *fn, int index, const char *name,
                  const char *condition);

static int
lower_named_while_with_buffers(ZirFunction *fn, int index, const char *name,
                  const char *condition, LowerNamedWhileBuffers *buffers)
{
    memset(&buffers->output, 0, sizeof(buffers->output));
    ZirStmt header = fn->stmts[index];
    int ok = 0;
    if(snprintf(buffers->declaration, sizeof(buffers->declaration), "%s: bool = %s",
                name, condition) >= (int)sizeof(buffers->declaration) ||
       snprintf(buffers->check, sizeof(buffers->check), "if !%s {", name) >=
           (int)sizeof(buffers->check))
        return fail(&header, "named while condition exceeds statement limit");
    for(int i = 0; i < index; i++)
        if(!append(&buffers->output, &fn->stmts[i])) goto done;
    header.text = KeepText("while true {");
    if(!append(&buffers->output, &header) ||
       !range_line(&buffers->output, ZIR_STMT_DECL, buffers->declaration, header.span) ||
       !range_line(&buffers->output, ZIR_STMT_IF, buffers->check, header.span) ||
       !range_line(&buffers->output, ZIR_STMT_BREAK, "break", header.span) ||
       !range_line(&buffers->output, ZIR_STMT_BLOCK_CLOSE, "}", header.span))
        goto done;
    for(int i = index + 1; i < fn->stmt_count; i++)
        if(!append(&buffers->output, &fn->stmts[i])) goto done;
    free(fn->stmts);
    fn->stmts = buffers->output.stmts;
    fn->stmt_count = buffers->output.stmt_count;
    fn->stmt_cap = buffers->output.stmt_cap;
    buffers->output.stmts = NULL;
    ok = 1;
done:
    free(buffers->output.stmts);
    return ok;
}

static int
lower_named_while(ZirFunction *fn, int index, const char *name,
                  const char *condition)
{
    static _Thread_local LowerNamedWhileBuffers *spares[16];
    static _Thread_local int spare_count;
    LowerNamedWhileBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = lower_named_while_with_buffers(fn, index, name, condition, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers BindJaiLoopControls keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct BindJaiLoopControlsBuffers {
    char first[ZIR_TEXT_MAX];
    char last[ZIR_TEXT_MAX];
    char condition[ZIR_TEXT_MAX];
} BindJaiLoopControlsBuffers;

int BindJaiLoopControls(ZirFunction *fn);

static int
BindJaiLoopControls_with_buffers(ZirFunction *fn, BindJaiLoopControlsBuffers *buffers)
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
            int reverse, pointer;
            if(range_header(statement, name, buffers->first, buffers->last, &reverse) ||
               collection_header(statement, name, second, buffers->first,
                                 &reverse, &pointer)) {
                copy_text(scope->name, sizeof(scope->name), name);
                scope->loop_id = statement->loop_id = next_id++;
            }
        } else if(statement->kind == ZIR_STMT_WHILE) {
            if(named_while_header(statement, scope->name, buffers->condition))
                scope->loop_id = statement->loop_id = next_id++;
        }
    }
    if(depth != 0) {
        fail(&fn->stmts[fn->stmt_count - 1], "unterminated loop scope");
        goto done;
    }
    for(int i = fn->stmt_count - 1; i >= 0; i--) {
        char name[ZIR_NAME_MAX];
        if(fn->stmts[i].kind == ZIR_STMT_WHILE &&
           named_while_header(&fn->stmts[i], name, buffers->condition) &&
           !lower_named_while(fn, i, name, buffers->condition))
            goto done;
    }
    ok = 1;
done:
    free(scopes);
    return ok;
}

int
BindJaiLoopControls(ZirFunction *fn)
{
    static _Thread_local BindJaiLoopControlsBuffers *spares[16];
    static _Thread_local int spare_count;
    BindJaiLoopControlsBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = BindJaiLoopControls_with_buffers(fn, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
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
              int target_id, int step_of)
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
                reverse ? "-" : "+") >= (int)sizeof(line) ||
       !range_line(out, ZIR_STMT_ASSIGN, line, span))
        return 0;
    out->stmts[out->stmt_count - 1].for_step = step_of;
    return 1;
}

/* step_of names the loop whose native header can take over this step,
 * or is zero. */
static int
loop_advance(ZirFunction *out, const char *cursor, const char *terminal,
             int reverse, int is_range, ZirSourceSpan span, int target_id,
             int step_of)
{
    char line[ZIR_TEXT_MAX];
    if(is_range)
        return range_advance(out, cursor, terminal, reverse, span,
                             target_id, step_of);
    if(snprintf(line, sizeof(line), "%s += 1", cursor) >= (int)sizeof(line) ||
       !range_line(out, ZIR_STMT_ASSIGN, line, span))
        return 0;
    out->stmts[out->stmt_count - 1].for_step = step_of;
    return 1;
}

static int
copy_loop_body(ZirFunction *out, const ZirFunction *fn, int index, int close,
               const char *cursor, const char *terminal, int reverse,
               int is_range, int loop_id, int step_of)
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
                             statement->span, statement->target_id, step_of)) {
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
        int first = 1, address = 0, operand = 0;
        LexerInit(&lexer, fn->stmts[i].text, "");
        do {
            token = LexerNext(&lexer);
            if(token.kind == ZIR_TOKEN_IDENT && !strcmp(token.text, name) &&
               (address || (first && fn->stmts[i].kind == ZIR_STMT_ASSIGN)))
                return 1;
            /* A * after an operand multiplies; anywhere else it takes an
             * address. */
            address = token.kind != ZIR_TOKEN_EOF && !strcmp(token.text, "*") && !operand;
            operand = token.kind == ZIR_TOKEN_IDENT || token.kind == ZIR_TOKEN_INT ||
                      token.kind == ZIR_TOKEN_FLOAT || token.kind == ZIR_TOKEN_STRING ||
                      token.kind == ZIR_TOKEN_CHAR ||
                      (token.kind != ZIR_TOKEN_EOF &&
                       (!strcmp(token.text, ")") || !strcmp(token.text, "]")));
            first = 0;
        } while(token.kind != ZIR_TOKEN_EOF);
    }
    return 0;
}

/* A bound known before the loop runs: an integer literal, or a module
 * constant that is one and no local or parameter hides. */
static int
literal_bound(const ZirFunction *fn, const ZirModule *module, const char *text,
              long long *value)
{
    char *end, name[ZIR_NAME_MAX];
    size_t length;
    while(*text == ' ') text++;
    if(!*text) return 0;
    errno = 0;
    *value = strtoll(text, &end, 0);
    while(*end == ' ') end++;
    if(end != text && errno == 0 && *end == '\0')
        return 1;
    length = strlen(text);
    while(length > 0 && text[length - 1] == ' ') length--;
    if(length == 0 || length >= sizeof(name))
        return 0;
    memcpy(name, text, length);
    name[length] = '\0';
    if(mentions(fn->args, name))
        return 0;
    for(int i = 0; i < fn->stmt_count; i++)
        if(fn->stmts[i].kind == ZIR_STMT_DECL && !strcmp(fn->stmts[i].name, name))
            return 0;
    for(int i = 0; i < module->define_count; i++)
        if(!strcmp(module->defines[i].name, name)) {
            const char *defined = module->defines[i].value;
            while(*defined == ' ') defined++;
            errno = 0;
            *value = strtoll(defined, &end, 0);
            while(*end == ' ') end++;
            return end != defined && errno == 0 && *end == '\0';
        }
    return 0;
}
/* Buffers lower_one_range keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerOneRangeBuffers {
    char start[ZIR_TEXT_MAX];
    char end[ZIR_TEXT_MAX];
    char line[ZIR_TEXT_MAX];
    char type[ZIR_TEXT_MAX];
    ZirFunction out;
} LowerOneRangeBuffers;

static int lower_one_range(ZirFunction *fn, const ZirModule *module, int index);

static int
lower_one_range_with_buffers(ZirFunction *fn, const ZirModule *module, int index, LowerOneRangeBuffers *buffers)
{
    const ZirStmt *header = &fn->stmts[index];
    int close = end_block(fn, index, fn->stmt_count);
    char binder[ZIR_NAME_MAX];
    char first[ZIR_NAME_MAX], last[ZIR_NAME_MAX], cursor[ZIR_NAME_MAX];
    int reverse;
    if(strchr(header->text, ';') != NULL)
        return fail(header, "C-style for headers are not Jai syntax");
    if(close >= fn->stmt_count ||
       !range_header(header, binder, buffers->start, buffers->end, &reverse))
        return fail(header,
            "for currently supports Jai integer ranges: for index: first..last { ... }");
    if(strcmp(binder, "it_index") == 0)
        return fail(header, "for range binder cannot be it_index");
    long long low, high;
    /* Constant bounds read in place, and a constant last value leaves room
     * to step past it. The binder counts itself unless the body changes it. */
    int constant = literal_bound(fn, module, buffers->start, &low) &&
                   literal_bound(fn, module, buffers->end, &high) &&
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
        snprintf(first, sizeof(first), "%s", buffers->start);
        snprintf(last, sizeof(last), "%s", buffers->end);
    }
    if(own_cursor)
        snprintf(cursor, sizeof(cursor), "%s", binder);
    /* As in Jai, the index has the bounds' type: 0..count with count: s32
     * counts in s32. Constant ranges and parallel regions count in s64. */
    const char *type = "s64";
    if(!constant && !header->is_parallel) {
        if(snprintf(buffers->type, sizeof(buffers->type), "type_of((%s) + (%s))",
                    buffers->start, buffers->end) >= (int)sizeof(buffers->type))
            return fail(header, "for range bounds exceed the statement limit");
        type = buffers->type;
    }
    memset(&buffers->out, 0, sizeof(buffers->out));
    ZirSourceSpan span = header->span;
    int ok = 0;
    for(int i = 0; i < index; i++)
        if(!append(&buffers->out, &fn->stmts[i])) goto done;
    if(!range_line(&buffers->out, ZIR_STMT_BLOCK_OPEN, "{", span)) goto done;
    if(!constant &&
       (snprintf(buffers->line, sizeof(buffers->line), "%s: %s = %s", first, type,
                 buffers->start) >=
        (int)sizeof(buffers->line) || !range_line(&buffers->out, ZIR_STMT_DECL, buffers->line, span) ||
        snprintf(buffers->line, sizeof(buffers->line), "%s: %s = %s", last, type,
                 buffers->end) >=
        (int)sizeof(buffers->line) || !range_line(&buffers->out, ZIR_STMT_DECL, buffers->line, span)))
        goto done;
    if(snprintf(buffers->line, sizeof(buffers->line), "%s: %s = %s", cursor, type,
                reverse ? last : first) >= (int)sizeof(buffers->line) ||
       !range_line(&buffers->out, ZIR_STMT_DECL, buffers->line, span)) goto done;
    if(snprintf(buffers->line, sizeof(buffers->line), "while %s %s %s {", cursor,
                reverse ? ">=" : "<=", reverse ? first : last) >=
       (int)sizeof(buffers->line) || !range_line(&buffers->out, ZIR_STMT_WHILE, buffers->line, span))
        goto done;
    int while_at = buffers->out.stmt_count - 1;
    buffers->out.stmts[while_at].loop_id = header->loop_id;
    buffers->out.stmts[while_at].is_parallel = header->is_parallel;
    buffers->out.stmts[while_at].is_gpu = header->is_gpu;
    if(!own_cursor &&
       (snprintf(buffers->line, sizeof(buffers->line), "%s: %s = %s", binder, type,
                 cursor) >=
        (int)sizeof(buffers->line) || !range_line(&buffers->out, ZIR_STMT_DECL, buffers->line, span)))
        goto done;
    if(wants_index) {
        /* it_index stays s64 whatever the bounds' type. */
        if(constant && !reverse && low == 0)
            snprintf(buffers->line, sizeof(buffers->line), "it_index: s64 = %s", cursor);
        else if(snprintf(buffers->line, sizeof(buffers->line),
                         type == buffers->type ? "it_index: s64 = cast(s64) (%s - %s)" :
                         "it_index: s64 = %s - %s",
                         reverse ? last : cursor, reverse ? cursor : first) >=
                (int)sizeof(buffers->line))
            goto done;
        if(!range_line(&buffers->out, ZIR_STMT_DECL, buffers->line, span)) goto done;
    }
    {
        const char *terminal = constant ? "" : reverse ? first : last;
        /* A constant range counted by its binder is a native counting loop
         * on targets that have one: the header takes over each step. */
        int step_of = constant && own_cursor ? header->loop_id : 0;
        if(step_of)
            buffers->out.stmts[while_at].for_form = 1;
        if(!copy_loop_body(&buffers->out, fn, index, close, cursor, terminal, reverse, 1,
                           header->loop_id, step_of)) goto done;
        if(!range_advance(&buffers->out, cursor, terminal, reverse, span, 0, step_of) ||
           !range_line(&buffers->out, ZIR_STMT_BLOCK_CLOSE, "}", span) ||
           !range_line(&buffers->out, ZIR_STMT_BLOCK_CLOSE, "}", span)) goto done;
    }
    for(int i = close + 1; i < fn->stmt_count; i++)
        if(!append(&buffers->out, &fn->stmts[i])) goto done;
    free(fn->stmts);
    fn->stmts = buffers->out.stmts;
    fn->stmt_count = buffers->out.stmt_count;
    fn->stmt_cap = buffers->out.stmt_cap;
    buffers->out.stmts = NULL;
    ok = 1;
done:
    free(buffers->out.stmts);
    if(!ok) fail(header, "for range lowering exceeds compiler limits");
    return ok;
}

static int
lower_one_range(ZirFunction *fn, const ZirModule *module, int index)
{
    static _Thread_local LowerOneRangeBuffers *spares[16];
    static _Thread_local int spare_count;
    LowerOneRangeBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = lower_one_range_with_buffers(fn, module, index, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
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
/* Buffers lower_one_collection keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerOneCollectionBuffers {
    char collection[ZIR_TEXT_MAX];
    char line[ZIR_TEXT_MAX];
    ZirFunction out;
} LowerOneCollectionBuffers;

static int lower_one_collection(ZirFunction *fn, const ZirModule *module, int index);

static int
lower_one_collection_with_buffers(ZirFunction *fn, const ZirModule *module, int index, LowerOneCollectionBuffers *buffers)
{
    const ZirStmt *header = &fn->stmts[index];
    int close = end_block(fn, index, fn->stmt_count);
    char value_name[ZIR_NAME_MAX], index_name[ZIR_NAME_MAX];
    char view[ZIR_NAME_MAX];
    char count[ZIR_NAME_MAX], cursor[ZIR_NAME_MAX], item_index[ZIR_NAME_MAX];
    int reverse, pointer;
    if(strchr(header->text, ';') != NULL)
        return fail(header, "C-style for headers are not Jai syntax");
    if(close >= fn->stmt_count ||
       !collection_header(header, value_name, index_name, buffers->collection,
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
    int direct = !reverse && !header->is_parallel && plain_name(buffers->collection) &&
                 !body_changes(fn, index, close, buffers->collection) &&
                 !body_changes(fn, index, close, index_name);
    if(direct) {
        snprintf(view, sizeof(view), "%s", buffers->collection);
        snprintf(count, sizeof(count), "%s.count", buffers->collection);
        snprintf(cursor, sizeof(cursor), "%s", index_name);
        snprintf(item_index, sizeof(item_index), "%s", index_name);
    }
    memset(&buffers->out, 0, sizeof(buffers->out));
    ZirSourceSpan span = header->span;
    int ok = 0;
    for(int i = 0; i < index; i++)
        if(!append(&buffers->out, &fn->stmts[i])) goto done;
    if(!range_line(&buffers->out, ZIR_STMT_BLOCK_OPEN, "{", span)) goto done;
    if(!direct &&
       (snprintf(buffers->line, sizeof(buffers->line), "%s := %s[:]", view, buffers->collection) >=
        (int)sizeof(buffers->line) || !range_line(&buffers->out, ZIR_STMT_DECL, buffers->line, span) ||
        snprintf(buffers->line, sizeof(buffers->line), "%s: s64 = %s.count", count, view) >=
        (int)sizeof(buffers->line) || !range_line(&buffers->out, ZIR_STMT_DECL, buffers->line, span)))
        goto done;
    if(snprintf(buffers->line, sizeof(buffers->line), "%s: s64 = 0", cursor) >=
       (int)sizeof(buffers->line) || !range_line(&buffers->out, ZIR_STMT_DECL, buffers->line, span))
        goto done;
    if(snprintf(buffers->line, sizeof(buffers->line), "while %s < %s {", cursor, count) >=
       (int)sizeof(buffers->line) || !range_line(&buffers->out, ZIR_STMT_WHILE, buffers->line, span))
        goto done;
    buffers->out.stmts[buffers->out.stmt_count - 1].loop_id = header->loop_id;
    buffers->out.stmts[buffers->out.stmt_count - 1].is_parallel = header->is_parallel;
    buffers->out.stmts[buffers->out.stmt_count - 1].is_gpu = header->is_gpu;
    /* A direct walk is a native counting loop, or a range loop, where the
     * target has one; the value binding comes first in the body. */
    if(direct && header->loop_id && !pointer)
        buffers->out.stmts[buffers->out.stmt_count - 1].for_form = 2;
    int step_of = direct && header->loop_id && !pointer ? header->loop_id : 0;
    if(!direct) {
        if(snprintf(buffers->line, sizeof(buffers->line), "%s: s64 = %s", item_index,
                    reverse ? "0" : cursor) >= (int)sizeof(buffers->line)) goto done;
        if(reverse && snprintf(buffers->line, sizeof(buffers->line), "%s: s64 = %s - %s - 1",
                               item_index, count, cursor) >= (int)sizeof(buffers->line))
            goto done;
        if(!range_line(&buffers->out, ZIR_STMT_DECL, buffers->line, span)) goto done;
    }
    if(snprintf(buffers->line, sizeof(buffers->line), "%s := %s%s[%s]", value_name,
                pointer ? "*" : "", view, item_index) >=
       (int)sizeof(buffers->line) || !range_line(&buffers->out, ZIR_STMT_DECL, buffers->line, span))
        goto done;
    if(!direct && body_mentions(fn, index, close, index_name) &&
       (snprintf(buffers->line, sizeof(buffers->line), "%s: s64 = %s", index_name,
                 item_index) >= (int)sizeof(buffers->line) ||
        !range_line(&buffers->out, ZIR_STMT_DECL, buffers->line, span))) goto done;
    if(!copy_loop_body(&buffers->out, fn, index, close, cursor, "", 0, 0,
                       header->loop_id, step_of) ||
       !loop_advance(&buffers->out, cursor, "", 0, 0, span, 0, step_of) ||
       !range_line(&buffers->out, ZIR_STMT_BLOCK_CLOSE, "}", span) ||
       !range_line(&buffers->out, ZIR_STMT_BLOCK_CLOSE, "}", span)) goto done;
    for(int i = close + 1; i < fn->stmt_count; i++)
        if(!append(&buffers->out, &fn->stmts[i])) goto done;
    free(fn->stmts);
    fn->stmts = buffers->out.stmts;
    fn->stmt_count = buffers->out.stmt_count;
    fn->stmt_cap = buffers->out.stmt_cap;
    buffers->out.stmts = NULL;
    ok = 1;
done:
    free(buffers->out.stmts);
    if(!ok) fail(header, "for collection lowering exceeds compiler limits");
    return ok;
}

static int
lower_one_collection(ZirFunction *fn, const ZirModule *module, int index)
{
    static _Thread_local LowerOneCollectionBuffers *spares[16];
    static _Thread_local int spare_count;
    LowerOneCollectionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = lower_one_collection_with_buffers(fn, module, index, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers LowerJaiFor keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerJaiForBuffers {
    char first[ZIR_TEXT_MAX];
    char last[ZIR_TEXT_MAX];
} LowerJaiForBuffers;

int LowerJaiFor(ZirFunction *fn, const ZirModule *module);

static int
LowerJaiFor_with_buffers(ZirFunction *fn, const ZirModule *module, LowerJaiForBuffers *buffers)
{
    for(int i = fn->stmt_count - 1; i >= 0; i--) {
        if(fn->stmts[i].kind != ZIR_STMT_FOR) continue;
        char binder[ZIR_NAME_MAX];
        int reverse;
        if(range_header(&fn->stmts[i], binder, buffers->first, buffers->last, &reverse)) {
            if(!lower_one_range(fn, module, i)) return 0;
        } else if(!lower_one_collection(fn, module, i)) {
            return 0;
        }
    }
    return 1;
}

int
LowerJaiFor(ZirFunction *fn, const ZirModule *module)
{
    static _Thread_local LowerJaiForBuffers *spares[16];
    static _Thread_local int spare_count;
    LowerJaiForBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = LowerJaiFor_with_buffers(fn, module, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
