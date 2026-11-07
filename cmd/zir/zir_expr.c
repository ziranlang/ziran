#include "zir_scratch.h"
#include "zir_memory.h"
#include "zir_files.h"
#include "zir_expr.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_token.h"
#include "zir_text.h"
#include "compiler_expression.h"
#include "compiler_parse.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int parse_expr(ZirFunction *fn, const ZirModule *module,
                      const char *text, ZirSourceSpan span,
                      const char *expected_type, int stmt_index,
                      int expand_defaults);

_Thread_local int ZirDefaultExpansionDepth;

int
CallerLocationLiteral(const ZirModule *module, ZirSourceSpan location,
                      char *output, size_t capacity)
{
    char candidate[ZIR_PATH_MAX * 2];
    char escaped[ZIR_TEXT_MAX];
    const char *path = SpanPath(location);
    if(!PathIsAbsolute(path) && module->source_root[0] != '\0') {
        int written = snprintf(candidate, sizeof(candidate), "%s/%s",
                               module->source_root, path);
        if(written < 0 || (size_t)written >= sizeof(candidate))
            return 0;
        path = candidate;
    }
    char *canonical = CanonicalPath(path);
    if(canonical != NULL)
        path = canonical;
    size_t length = strlen(path);
    if(!PathIsAbsolute(path) || length >= ZIR_PATH_MAX) {
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
call_name_shadowed(const ZirFunction *fn, int stmt_index, const char *name)
{
    if(stmt_index < 0 || strchr(name, '.') != NULL)
        return 0;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    for(int i = 0; i < parameters->count; i++)
        if(parameters->items[i].name != NULL &&
           !strcmp(parameters->items[i].name, name))
            return 1;
    int depth = 0;
    int binding_depths[128];
    int bindings = 0;
    for(int i = 0; i < stmt_index; i++) {
        const ZirStmt *statement = &fn->stmts[i];
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
/* Buffers ExpandDefaultArguments keeps on the heap so deep nesting fits the
 * stack; freed blocks are kept for reuse, one per nesting level. */
typedef struct ExpandDefaultsBuffers {
    char qualified_default[ZIR_TEXT_MAX];
    char location_default[ZIR_TEXT_MAX];
} ExpandDefaultsBuffers;

static bool
expand_defaults_with_buffers(ZirFunction *fn, const ZirModule *module,
                             ZirSourceSpan span, int stmt_index, int callee,
                             int32_t *first, int32_t *last,
                             ExpandDefaultsBuffers *buffers)
{
    char qualified[ZIR_NAME_MAX];
    const char *target = callee >= 0 && fn->exprs[callee].kind == ZIR_EXPR_IDENT ?
        fn->exprs[callee].name : "";
    if(!*target && callee >= 0 &&
       fn->exprs[callee].kind == ZIR_EXPR_MEMBER) {
        const ZirExpr *member = &fn->exprs[callee];
        if(member->left < 0 ||
           fn->exprs[member->left].kind != ZIR_EXPR_IDENT)
            return true;
        if(snprintf(qualified, sizeof(qualified), "%s.%s",
                    fn->exprs[member->left].name, member->name) >=
           (int)sizeof(qualified)) return true;
        target = qualified;
    }
    if(!*target || call_name_shadowed(fn, stmt_index, target) ||
       ZirDefaultExpansionDepth >= 32 || module == NULL)
        return true;
    const ZirModule *owner = NULL;
    const ZirFunction *function = NULL;
    if(ResolveFunctionAt(module, target, SpanPath(span),
                         &owner, &function) != 1 ||
       function == NULL || !FunctionDefaultArgs(function)[0])
        return true;
    char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
    char (*defaults)[ZIR_TEXT_MAX] = calloc(64, sizeof(*defaults));
    bool ok = parameters != NULL && defaults != NULL;
    if(!ok) goto done;
    int count = split_top_level(FunctionArgs(function), parameters[0], 64,
                                sizeof(parameters[0]));
    int default_count = split_top_level(FunctionDefaultArgs(function), defaults[0],
                                        64, sizeof(defaults[0]));
    unsigned char used[64] = {0};
    if(count != default_count) goto done;
    for(int child = *first; child >= 0;
        child = fn->exprs[child].next_sibling) {
        const char *named = fn->exprs[child].argument_name;
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
    for(int i = 0; i < count && ok; i++) {
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
            ok = CallerLocationLiteral(module, fn->exprs[callee].span,
                                       buffers->location_default,
                                       sizeof(buffers->location_default));
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
                ok = written >= 0 && (size_t)written < sizeof(helper_call);
                value = helper_call;
            } else {
                ok = qualify_default_field_helpers(
                    value, helper_name, target, SpanPath(span),
                    buffers->qualified_default, sizeof(buffers->qualified_default));
                value = buffers->qualified_default;
            }
        }
        if(!ok) break;
        ZirDefaultExpansionDepth++;
        int child = parse_expr(fn, module, value, span, NULL, stmt_index, 1);
        ZirDefaultExpansionDepth--;
        if(child < 0) { ok = false; break; }
        fn->exprs[child].argument_name = KeepName(parameter_name);
        if(*last >= 0) fn->exprs[*last].next_sibling = child;
        else *first = child;
        *last = child;
        used[i] = 1;
    }
done:
    free(parameters); free(defaults);
    return ok;
}

/* The default arguments a call to CALLEE omits, appended after LAST unless
 * STMT_INDEX's scope shadows the callee; false when one does not parse. */
COMPILER_SCRATCH_RETURN(, bool, ExpandDefaultArguments,
    (ZirFunction *fn, const ZirModule *module, ZirSourceSpan span,
     int32_t stmt_index, int32_t callee, int32_t *first, int32_t *last),
    ExpandDefaultsBuffers, RECOVERABLE, free,
    expand_defaults_with_buffers(fn, module, span, stmt_index, callee,
                                 first, last, buffers))

/* Text still holding raw-string markers is parsed in full. */
static int
parse_expr(ZirFunction *fn, const ZirModule *module, const char *text,
           ZirSourceSpan span, const char *expected_type, int stmt_index,
           int expand_defaults)
{
    char *expanded = RawTextExpand(text);
    const char *source = expanded != NULL ? expanded : text;
    const char *expected = expected_type != NULL ? expected_type : "";
    int result = compiler_parse_ParseExpression(fn, (ZirModule *)module,
        StringView(source, strlen(source)), span,
        StringView(expected, strlen(expected)), stmt_index, expand_defaults);
    free(expanded);
    return result;
}

int
ParseExpr(ZirFunction *fn, const ZirModule *module, const char *text,
          ZirSourceSpan span)
{
    return parse_expr(fn, module, text, span, NULL, -1, 1);
}

int
DefineIsAggregate(const ZirModule *module, const ZirDefine *define)
{
    ZirFunction probe = {0};
    int root = ParseExpr(&probe, module, define->value, define->span);
    int aggregate = root >= 0 && probe.exprs[root].kind == ZIR_EXPR_COMPOUND;
    free(probe.exprs);
    return aggregate;
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
static const char *
expression_name(String part)
{
    char text[ZIR_NAME_MAX];
    size_t length = (size_t)part.length;
    if(length >= sizeof(text)) length = sizeof(text) - 1;
    if(length != 0) memcpy(text, part.data, length);
    text[length] = 0;
    return KeepName(text);
}

static char *
expression_copy(String part)
{
    char *text = AllocateOrExit((size_t)part.length + 1);
    if(part.length != 0) memcpy(text, part.data, (size_t)part.length);
    text[part.length] = 0;
    return text;
}

void
StructureFunction(ZirFunction *fn, const ZirModule *module)
{
    free(fn->exprs); fn->exprs = NULL; fn->expr_count = fn->expr_cap = 0;
    for(int i = 0; i < fn->stmt_count; i++) {
        ZirStmt *st = &fn->stmts[i];
        st->expr_root = st->lhs_root = -1;
        if(st->kind == ZIR_STMT_BLOCK_OPEN && !strncmp(st->text, "#asm", 4) &&
           !is_ident_char(st->text[4])) {
            /* Restructuring must retain constant strings already resolved in
             * block metadata. The checker still compares it with the source. */
            if(!st->assembly_text || !*st->assembly_text)
                st->assembly_text = AssemblyBlockText(st->text, st->span);
            if(st->assembly_text == NULL) exit(1);
            continue;
        }
        if(st->kind == ZIR_STMT_BLOCK_OPEN) st->assembly_text = "";
        if(st->is_using && st->kind == ZIR_STMT_EXPR) {
            st->expr_root = ParseExprNoDefaults(fn, module, "0", st->span);
            continue;
        }
        StatementExpression parts = compiler_expression_StatementParts(
            StringView(st->text, strlen(st->text)), (StatementKind)st->kind);
        st->is_else = parts.is_else;
        if(parts.error == StatementExpressionError_Modifier) {
            char *modifier = expression_copy(parts.modifier);
            Diagnostic(st->span, "parse.modifier",
                       "unknown declaration modifier: %s", modifier);
            free(modifier);
            exit(1);
        }
        if(parts.error == StatementExpressionError_ArrayLiteral) {
            Diagnostic(st->span, "parse.jai_syntax",
                       "C-style array literal is not valid Jai syntax; use .[...]");
            exit(1);
        }
        if(parts.has_declaration) {
            st->name = expression_name(parts.name);
            st->type = expression_name(parts.type);
        }
        if(parts.assignment.length != 0) {
            copy_text(st->assignment_op, sizeof(st->assignment_op),
                      expression_name(parts.assignment));
            char *lhs = expression_copy(parts.lhs);
            st->lhs_root = parse_expr(fn, module, lhs, st->span, NULL, i, 1);
            free(lhs);
        }
        if(parts.has_value) {
            char *value = expression_copy(parts.value);
            st->expr_root = parse_expr(fn, module, value, st->span,
                st->kind == ZIR_STMT_DECL ? st->type : NULL, i, 1);
            free(value);
        }
    }
}
