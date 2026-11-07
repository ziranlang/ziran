#include "zir_scratch.h"
#include <stddef.h>
#include "zir_memory.h"
#include "zir_parse_internal.h"
#include "compiler_source.h"
#include "compiler_text.h"

static int compile_size_of_ready(const ZirModule *module, char *condition);

ZirConsts
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

/* PARSED's COUNT items of SIZE bytes, then each of FUTURE's not declared at
 * the same place, which SPAN_OFFSET locates in every item. */
static void *
visible_items(const void *parsed, int count, const void *future,
              int future_count, size_t size, size_t span_offset, int *visible)
{
    char *items = calloc((size_t)count + (size_t)future_count + 1, size);
    if(items == NULL)
        die("out of memory resolving compile-time declarations");
    if(count > 0) memcpy(items, parsed, (size_t)count * size);
    *visible = count;
    for(int i = 0; i < future_count; i++) {
        const char *candidate = (const char *)future + (size_t)i * size;
        const ZirSourceSpan *at = (const void *)(candidate + span_offset);
        int present = 0;
        for(int j = 0; j < count && !present; j++) {
            const ZirSourceSpan *seen =
                (const void *)((const char *)parsed + (size_t)j * size + span_offset);
            present = seen->line == at->line && !strcmp(SpanPath(*seen), SpanPath(*at));
        }
        if(!present) memcpy(items + (size_t)(*visible)++ * size, candidate, size);
    }
    return items;
}

#define VISIBLE_ITEMS(field, count_field, future, type)                       \
    (visible.field = visible_items(parsed->field, parsed->count_field,        \
         (future) != NULL ? (future)->items : NULL,                           \
         (future) != NULL ? (future)->count : 0, sizeof(type),                \
         offsetof(type, span), &visible.count_field))

static void
add_visible_compile_defines(ZirModule *visible, const ZirModule *parsed,
                            const ZirConsts *constants)
{
    SymbolLookupsChanged();
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
               !strcmp(SpanPath(parsed->defines[j].span), constant->path)) {
                present = 1;
                break;
            }
        if(present) continue;
        ZirDefine *definition = &visible->defines[visible->define_count++];
        copy_text(definition->name, sizeof(definition->name),
                  constant->name);
        definition->value = KeepText(constant->expr);
        definition->is_public = constant->is_public;
        definition->is_file_private = constant->is_file_private;
        definition->span = Span(constant->path, constant->source_line, 1);
    }
}

ZirModule
visible_compile_module(const ZirModule *parsed,
                       const CompileParseContext *context,
                       const ZirConsts *constants)
{
    ZirModule visible = *parsed;
    SymbolLookupsChanged();
    VISIBLE_ITEMS(usings, using_count, context ? context->future_usings : NULL, ZirUsing);
    add_visible_compile_defines(&visible, parsed, constants);
    VISIBLE_ITEMS(imports, import_count, context ? context->future_imports : NULL, ZirImport);
    VISIBLE_ITEMS(types, type_count, context ? context->future_types : NULL, ZirType);
    VISIBLE_ITEMS(functions, function_count, context ? context->future_functions : NULL, ZirFunction);
    VISIBLE_ITEMS(globals, global_count, context ? context->future_globals : NULL, ZirGlobal);
    return visible;
}

void
free_visible_compile_module(ZirModule *visible)
{
    SymbolLookupsChanged();
    free(visible->usings);
    free(visible->defines);
    free(visible->imports);
    free(visible->types);
    free(visible->functions);
    free(visible->globals);
}

int
select_compile_condition(ZirModule *module, const ZirConsts *consts,
                         const char *source, ZirSourceSpan span,
                         const CompileParseContext *context,
                         const char *source_path, int *deferred)
{
    ZirModule *source_module = module;
    char expanded[ZIR_TEXT_MAX];
    int64_t value = 0;
    int known;
    ZirConsts visible = visible_compile_constants(consts,
        context != NULL ? context->future_constants : NULL);
    ZirModule using_scope = visible_compile_module(module, context, &visible);
    module = &using_scope;
    consts = &visible;
    expand_compile_expr(expanded, sizeof(expanded), consts, source, SpanPath(span));
    normalize_jai_source_tokens(expanded, SpanPath(span), source_path, span.line);
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
    known = eval_typed_condition(expanded, module, consts, span, &value);
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
                    SymbolLookupsChanged();
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
        known = eval_typed_condition(expanded, module, consts, span, &value);
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

/* Like strstr, the result points into source and is writable exactly when
 * the caller's source is. */
char *
find_unquoted_text(const char *source, const char *needle)
{
    size_t length = strlen(needle);
    for(char *p = (char *)source; *p;) {
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

char *
find_unquoted_word(const char *source, const char *word)
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
/* Buffers replace_compile_ifx keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ReplaceCompileIfxBuffers {
    char condition[ZIR_TEXT_MAX];
    char rewritten[ZIR_TEXT_MAX];
} ReplaceCompileIfxBuffers;

static int replace_compile_ifx(char *source, size_t capacity, const ZirFunction *fn,
                    const ZirModule *module, const ZirExpr *expr,
                    const ZirConsts *consts,
                    ZirSourceSpan span, int allow_deferred);

static int
replace_compile_ifx_with_buffers(char *source, size_t capacity, const ZirFunction *fn,
                    const ZirModule *module, const ZirExpr *expr,
                    const ZirConsts *consts,
                    ZirSourceSpan span, int allow_deferred, ReplaceCompileIfxBuffers *buffers)
{
    int64_t value = 0;
    char *at;
    const char *selected;
    int written;
    if(expr->left < 0 || expr->right < 0 || expr->third < 0)
        die_at(span, "malformed #ifx expression");
    expand_compile_expr(buffers->condition, sizeof(buffers->condition), consts,
                        fn->exprs[expr->left].text, SpanPath(span));
    if(allow_deferred && !compile_size_of_ready(module, buffers->condition))
        return 0;
    if(strstr(buffers->condition, "size_of") != NULL)
        lower_size_of_value(buffers->condition, sizeof(buffers->condition), module, span);
    if(!eval_typed_condition(buffers->condition, module, consts, span, &value)) {
        if(allow_deferred) return 0;
        die_at(span, "#ifx condition is not a compile-time constant: %s",
               buffers->condition);
    }
    selected = fn->exprs[value ? expr->right : expr->third].text;
    at = find_unquoted_text(source, expr->text);
    if(at == NULL)
        die_at(span, "cannot locate #ifx expression in source");
    written = snprintf(buffers->rewritten, sizeof(buffers->rewritten), "%.*s(%s)%s",
                       (int)(at - source), source, selected,
                       at + strlen(expr->text));
    if(written < 0 || (size_t)written >= sizeof(buffers->rewritten) ||
       (size_t)written >= capacity)
        die_at(span, "#ifx selection exceeds expression limit");
    copy_text(source, capacity, buffers->rewritten);
    return 1;
}

COMPILER_SCRATCH_RETURN(static, int, replace_compile_ifx,
    (char *source, size_t capacity, const ZirFunction *fn,
                    const ZirModule *module, const ZirExpr *expr,
                    const ZirConsts *consts,
                    ZirSourceSpan span, int allow_deferred),
    ReplaceCompileIfxBuffers, RECOVERABLE, free,
    replace_compile_ifx_with_buffers(source, capacity, fn, module, expr, consts, span, allow_deferred, buffers))
/* Buffers lower_compile_ifx_value keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerCompileIfxValueBuffers {
    ZirFunction probe;
} LowerCompileIfxValueBuffers;

int lower_compile_ifx_value(char *value, size_t capacity,
                        const ZirModule *module, const ZirConsts *consts,
                        ZirSourceSpan span, int allow_deferred);

static int
lower_compile_ifx_value_with_buffers(char *value, size_t capacity,
                        const ZirModule *module, const ZirConsts *consts,
                        ZirSourceSpan span, int allow_deferred, LowerCompileIfxValueBuffers *buffers)
{
    for(int pass = 0; pass < 128; pass++) {
        memset(&buffers->probe, 0, sizeof(buffers->probe));
        ParseExpr(&buffers->probe, module, value, span);
        const ZirExpr *expr = outer_compile_ifx(&buffers->probe);
        if(expr == NULL) {
            free(buffers->probe.exprs);
            if(find_unquoted_text(value, "#ifx") != NULL)
                die_at(span, "invalid #ifx expression");
            return 1;
        }
        int replaced = replace_compile_ifx(value, capacity, &buffers->probe, module,
                                           expr, consts, span,
                                           allow_deferred);
        free(buffers->probe.exprs);
        if(!replaced) return 0;
    }
    die_at(span, "too many nested #ifx expressions");
}

COMPILER_SCRATCH_RETURN(, int, lower_compile_ifx_value,
    (char *value, size_t capacity,
                        const ZirModule *module, const ZirConsts *consts,
                        ZirSourceSpan span, int allow_deferred),
    LowerCompileIfxValueBuffers, RECOVERABLE, free,
    lower_compile_ifx_value_with_buffers(value, capacity, module, consts, span, allow_deferred, buffers))

int
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
            char text[ZIR_TEXT_MAX];
            copy_text(text, sizeof(text), st->text);
            if(!replace_compile_ifx(text, sizeof(text), fn, module,
                                    expr, consts, st->span,
                                    allow_deferred)) return 0;
            st->text = KeepText(text);
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
/* Buffers replace_size_of keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ReplaceSizeOfBuffers {
    char rewritten[ZIR_TEXT_MAX];
    char operand[ZIR_TEXT_MAX];
} ReplaceSizeOfBuffers;

static void replace_size_of(char *source, size_t capacity, const ZirModule *module,
                const ZirExpr *expr, ZirSourceSpan span);

static void
replace_size_of_with_buffers(char *source, size_t capacity, const ZirModule *module,
                const ZirExpr *expr, ZirSourceSpan span, ReplaceSizeOfBuffers *buffers)
{
    char inferred[ZIR_NAME_MAX];
    size_t size, alignment;
    char *at = find_unquoted_text(source, expr->text);
    const char *sized_type = expr->name;
    int written;
    if(TypeOfOperand(expr->name, buffers->operand, sizeof(buffers->operand))) {
        if(!InferExpressionType(module, buffers->operand, span, inferred,
                                sizeof(inferred)))
            die_at(span, "size_of(type_of(...)) requires a checked expression");
        sized_type = inferred;
    } else if(!JaiTypeSpelling(span, expr->name)) exit(1);
    if(!TypeLayout(module, sized_type, &size, &alignment) ||
       size > LONG_MAX)
        die_at(span, "size_of requires a known sized type: %s", sized_type);
    if(at == NULL)
        die_at(span, "cannot locate size_of expression in source");
    written = snprintf(buffers->rewritten, sizeof(buffers->rewritten), "%.*s(%zu)%s",
                       (int)(at - source), source, size,
                       at + strlen(expr->text));
    if(written < 0 || (size_t)written >= sizeof(buffers->rewritten) ||
       (size_t)written >= capacity)
        die_at(span, "size_of selection exceeds expression limit");
    copy_text(source, capacity, buffers->rewritten);
}

COMPILER_SCRATCH_VOID(static, replace_size_of,
    (char *source, size_t capacity, const ZirModule *module,
                const ZirExpr *expr, ZirSourceSpan span),
    ReplaceSizeOfBuffers, RECOVERABLE, free,
    replace_size_of_with_buffers(source, capacity, module, expr, span, buffers))
/* Buffers lower_size_of_value keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerSizeOfValueBuffers {
    ZirFunction probe;
} LowerSizeOfValueBuffers;

void lower_size_of_value(char *value, size_t capacity, const ZirModule *module,
                    ZirSourceSpan span);

static void
lower_size_of_value_with_buffers(char *value, size_t capacity, const ZirModule *module,
                    ZirSourceSpan span, LowerSizeOfValueBuffers *buffers)
{
    for(int pass = 0; pass < 128; pass++) {
        memset(&buffers->probe, 0, sizeof(buffers->probe));
        ParseExpr(&buffers->probe, module, value, span);
        const ZirExpr *expr = first_size_of(&buffers->probe, 1);
        if(expr == NULL) {
            free(buffers->probe.exprs);
            if(find_unquoted_word(value, "size_of") != NULL)
                die_at(span, "invalid size_of expression");
            return;
        }
        replace_size_of(value, capacity, module, expr, span);
        free(buffers->probe.exprs);
    }
    die_at(span, "too many size_of expressions");
}

COMPILER_SCRATCH_VOID(, lower_size_of_value,
    (char *value, size_t capacity, const ZirModule *module,
                    ZirSourceSpan span),
    LowerSizeOfValueBuffers, RECOVERABLE, free,
    lower_size_of_value_with_buffers(value, capacity, module, span, buffers))

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
        constant->expr = KeepText(definition->value);
        copy_text(constant->path, sizeof(constant->path),
                  SpanPath(definition->span));
        constant->is_file_private = definition->is_file_private;
    }
    for(int i = 0; i < module->define_count; i++) {
        ZirDefine *definition = &module->defines[i];
        const char *before = definition->value;
        copy_text(module->lookup_path, sizeof(module->lookup_path),
                  SpanPath(definition->span));
        SymbolLookupsChanged();
        if(find_unquoted_text(definition->value, "#ifx") != NULL) {
            size_t capacity;
            char *edit = TextEditBegin(definition->value, &capacity);
            lower_compile_ifx_value(edit, capacity, module,
                                    &constants, definition->span,
                                    allow_deferred);
            definition->value = TextEditEnd(edit);
        }
        if(!starts_word(definition->value, "#run") &&
           find_unquoted_text(definition->value, "#ifx") == NULL &&
           find_unquoted_word(definition->value, "size_of") != NULL) {
            size_t capacity;
            char *edit = TextEditBegin(definition->value, &capacity);
            lower_size_of_value(edit, capacity, module, definition->span);
            definition->value = TextEditEnd(edit);
        }
        if(strcmp(before, definition->value) != 0) progress++;
        constants.items[i].expr = KeepText(definition->value);
    }
    for(int i = 0; i < module->global_count; i++) {
        ZirGlobal *global = &module->globals[i];
        const char *before = global->init;
        copy_text(module->lookup_path, sizeof(module->lookup_path),
                  SpanPath(global->span));
        SymbolLookupsChanged();
        if(find_unquoted_text(global->init, "#ifx") != NULL) {
            size_t capacity;
            char *edit = TextEditBegin(global->init, &capacity);
            lower_compile_ifx_value(edit, capacity, module, &constants,
                                    global->span, allow_deferred);
            global->init = TextEditEnd(edit);
        }
        if(find_unquoted_text(global->init, "#ifx") == NULL &&
           find_unquoted_word(global->init, "size_of") != NULL) {
            size_t capacity;
            char *edit = TextEditBegin(global->init, &capacity);
            lower_size_of_value(edit, capacity, module, global->span);
            global->init = TextEditEnd(edit);
        }
        if(strcmp(before, global->init) != 0) progress++;
    }
    for(int i = 0; i < module->function_count; i++) {
        ZirFunction *fn = &module->functions[i];
        copy_text(module->lookup_path, sizeof(module->lookup_path),
                  SpanPath(fn->span));
        SymbolLookupsChanged();
        if(function_has_compile_ifx(fn)) {
            if(lower_compile_ifx_function(fn, module, &constants,
                                          allow_deferred)) progress++;
        }
    }
    copy_text(module->lookup_path, sizeof(module->lookup_path),
              saved_lookup_path);
    SymbolLookupsChanged();
    free(constants.items);
    return progress;
}
/* Buffers parse_compile_check keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ParseCompileCheckBuffers {
    char cond[ZIR_TEXT_MAX];
    char raw[ZIR_TEXT_MAX];
    char msg[ZIR_TEXT_MAX];
} ParseCompileCheckBuffers;

int parse_compile_check(ZirModule *module, const char *path, int line_no,
                    char *line, const ZirConsts *consts);

static int
parse_compile_check_with_buffers(ZirModule *module, const char *path, int line_no,
                    char *line, const ZirConsts *consts, ParseCompileCheckBuffers *buffers)
{
    if(strncmp(line, "#assert", 7) == 0 &&
       (line[7] == '\0' || isspace((unsigned char)line[7]))) {
        char *body = trim(line + 7);
        if(body[0] == '\0')
            die_at(Span(path, line_no, 1), "#assert needs a condition");
        SourcePart part = compiler_text_NextSourcePart(StringView(body, strlen(body)), 0);
        if(part.next >= 0) {
            char *comma = body + part.end;
            *comma = '\0';
            snprintf(buffers->msg, sizeof(buffers->msg), "%s", trim(comma + 1));
            if(buffers->msg[0] == '\0')
                snprintf(buffers->msg, sizeof(buffers->msg), "\"Ziran #assert failed\"");
        } else {
            snprintf(buffers->msg, sizeof(buffers->msg), "\"Ziran #assert failed\"");
        }
        copy_text(buffers->raw, sizeof(buffers->raw), trim(body));
        expand_compile_expr(buffers->cond, sizeof(buffers->cond), consts, buffers->raw, path);
        {
            int64_t value = 0;
            int known = strstr(buffers->cond, "size_of") == NULL &&
                eval_typed_condition(buffers->cond, module, consts,
                                     Span(path, line_no, 1), &value);
            if(known && !value)
                die_at(Span(path, line_no, 1), "#assert failed: %s", buffers->msg);
            if(known)
                copy_text(buffers->cond, sizeof(buffers->cond), value ? "1" : "0");
            else
                copy_text(buffers->cond, sizeof(buffers->cond), buffers->raw);
            if(!ModuleAddAssert(module, buffers->cond, buffers->msg, Span(path, line_no, 1)))
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

COMPILER_SCRATCH_RETURN(, int, parse_compile_check,
    (ZirModule *module, const char *path, int line_no,
                    char *line, const ZirConsts *consts),
    ParseCompileCheckBuffers, RECOVERABLE, free,
    parse_compile_check_with_buffers(module, path, line_no, line, consts, buffers))

/* Returns 1 when the line is consumed by top-level conditional handling
 * ('#if'/'else #if'/'else' open or retarget a frame; the matching '}'
 * pops one). A plain line inside a region only settles the frame's brace
 * count and returns 0. Only the selected branch is parsed. */
int
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
void
cond_frame_settle(ZirCondFrame *frames, int count)
{
    if(count > 0)
        frames[count - 1].braces--;
}

/* The output aliases the input; the Ziran scanner never overwrites unread bytes. */
void
strip_block_comments(char *s, int *comment_depth)
{
    size_t length = strlen(s);
    CommentScan result = compiler_source_StripComments(StringView(s, length),
                                                       (Slice){s, (int64_t)length},
                                                       *comment_depth);
    s[result.count] = '\0';
    *comment_depth = result.depth;
}
