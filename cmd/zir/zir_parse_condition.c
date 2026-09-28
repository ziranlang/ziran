#include "zir_parse_internal.h"

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

ZirModule
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

void
free_visible_compile_module(ZirModule *visible)
{
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

char *
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

char *
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

int
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

void
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

int
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

/* Strip nested block and line comments in place while preserving newlines.
 * Block depth carries across source lines; quoted delimiters are inert. */
void
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
