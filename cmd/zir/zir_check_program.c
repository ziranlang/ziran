#include "zir_check_internal.h"
/* Buffers CheckPrograms keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CheckProgramsBuffers {
    Checker c;
    char saved_path[ZIR_PATH_MAX];
    Checker scope;
    ZirToken token;
    char literal[ZIR_TEXT_MAX];
    ZirFunction raw;
    char lowered[ZIR_TEXT_MAX];
    char folded[ZIR_TEXT_MAX];
    ZirFunction expression;
    Checker initializer;
    char assignment[ZIR_TEXT_MAX];
    char body[sizeof(((ZirType *)0)->body)];
    char expanded[sizeof(((ZirType *)0)->body)];
} CheckProgramsBuffers;

int CheckPrograms(ZirProgram **programs, int count);

static int
CheckPrograms_with_buffers(ZirProgram **programs, int count, CheckProgramsBuffers *buffers)
{
    memset(&buffers->c, 0, sizeof(buffers->c));
    buffers->c.programs = programs; buffers->c.program_count = count;
    if(!LinkImports(programs, count))
        return 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int g = 0; g < module->global_count; g++)
                module->globals[g].native_name_collision = 0;
            for(int d = 0; d < module->define_count; d++)
                module->defines[d].native_name_collision = 0;
            for(int t = 0; t < module->type_count; t++) {
                ZirType *type = &module->types[t];
                type->native_name_mangled = 0;
                if(type->is_record_template ||
                   (type->is_extern && !type->foreign_target[0])) continue;
                for(int target = ZIR_C; target <= ZIR_GO; target++) {
                    char mapped[ZIR_NAME_MAX * 2];
                    TargetBindingName(NULL, (ZirTarget)target, type->name,
                                      mapped, sizeof(mapped));
                    if(strcmp(mapped, type->name))
                        type->native_name_mangled = 1;
                }
            }
        }
    if(!MarkNativeNameCollisions(programs, count)) {
        fprintf(stderr, "out of memory while naming native values\n");
        return 0;
    }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int t = 0; t < module->type_count; t++) {
                ZirType *type = &module->types[t];
                if(type->is_record_template ||
                   (type->is_extern && !type->foreign_target[0])) continue;
                for(int q = p; q < count; q++)
                    for(int n = q == p ? m : 0;
                        n < programs[q]->module_count; n++) {
                        ZirModule *other_module = &programs[q]->modules[n];
                        for(int u = q == p && n == m ? t + 1 : 0;
                            u < other_module->type_count; u++) {
                            ZirType *other = &other_module->types[u];
                            if(other->is_record_template ||
                               (other->is_extern && !other->foreign_target[0]) ||
                               strcmp(type->name, other->name)) continue;
                            if(same_type_application(module, type, other_module, other) ||
                               SameMapType(module, type, other_module, other))
                                continue;
                            type->native_name_mangled = 1;
                            other->native_name_mangled = 1;
                        }
                    }
            }
        }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            copy_text(buffers->saved_path, sizeof(buffers->saved_path), module->lookup_path);
            for(int i = 0; i < module->using_count; i++) {
                const ZirUsing *using = &module->usings[i];
                select_lookup_file(module, using->span);
                const ZirType *type = FindType(module, using->path, NULL);
                if(type != NULL && !type->is_enum) {
                    Diagnostic(using->span, "check.enum_scope",
                               "top-level using requires an enum type: %s",
                               using->path);
                    return 0;
                }
                if(type == NULL) {
                    memset(&buffers->scope, 0, sizeof(buffers->scope));
                    buffers->scope.module = module;
                    activate_using_filtered(&buffers->scope, using->path,
                                            using->filter, using->span);
                    free(buffers->scope.bindings);
                    if(buffers->scope.errors || buffers->scope.failed)
                        return 0;
                }
            }
            copy_text(module->lookup_path, sizeof(module->lookup_path),
                      buffers->saved_path);
            TypeLookupsChanged();
            for(int g = 0; g < module->global_count; g++)
                if(!lower_file_record_using(module, module->globals[g].init,
                         sizeof(module->globals[g].init),
                         module->globals[g].span) ||
                   !LowerFileScopeUsing(module, module->globals[g].init,
                         sizeof(module->globals[g].init),
                         module->globals[g].span)) return 0;
            for(int d = 0; d < module->define_count; d++)
            {
                ZirDefine *definition = &module->defines[d];
                if(definition->requires_open_enum) {
                    int64_t value = 0;
                    select_lookup_file(module, definition->span);
                    int opened = opened_file_enum(module, definition->value,
                                                   definition->span, &value);
                    copy_text(module->lookup_path,
                              sizeof(module->lookup_path), buffers->saved_path);
                    TypeLookupsChanged();
                    if(opened <= 0) {
                        if(opened == 0)
                            Diagnostic(definition->span, "check.enum_scope",
                                       "unresolved opened enum member: %s",
                                       definition->value);
                        return 0;
                    }
                }
                if(!lower_file_record_using(module, definition->value,
                         sizeof(definition->value),
                         definition->span) ||
                   !LowerFileScopeUsing(module, definition->value,
                         sizeof(definition->value),
                         definition->span)) return 0;
                definition->requires_open_enum = 0;
            }
            for(int a = 0; a < module->assert_count; a++)
                if(!lower_file_record_using(module,
                         module->asserts[a].condition,
                         sizeof(module->asserts[a].condition),
                         module->asserts[a].span) ||
                   !LowerFileScopeUsing(module,
                         module->asserts[a].condition,
                         sizeof(module->asserts[a].condition),
                         module->asserts[a].span)) return 0;
        }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int d = 0; d < module->define_count; d++) {
                ZirDefine *definition = &module->defines[d];
                if(starts_word(definition->value, "#run")) continue;
                ZirLexer lexer;
                LexerInit(&lexer, definition->value, SpanPath(definition->span));
                for(;;) {
                    buffers->token = LexerNext(&lexer);
                    if(buffers->token.kind == ZIR_TOKEN_EOF) break;
                    if(buffers->token.kind == ZIR_TOKEN_DIRECTIVE &&
                       strcmp(buffers->token.text, "#compile_time") == 0) {
                        Diagnostic(definition->span, "check.compile_time",
                                   "#compile_time cannot be used as a constant");
                        return 0;
                    }
                }
            }
        }
    /* Select expressions that depend on imports and resolve #run constants
     * together: either may refer to a result from the other. */
    for(int pass = 0; pass < 128; pass++) {
        int pending = 0, progress = 0;
        for(int p = 0; p < count; p++)
            for(int m = 0; m < programs[p]->module_count; m++)
                progress += LowerLinkedCompileExpressions(
                    &programs[p]->modules[m], 1);
        for(int p = 0; p < count; p++)
            for(int m = 0; m < programs[p]->module_count; m++) {
                ZirModule *module = &programs[p]->modules[m];
                for(int d = 0; d < module->define_count; d++) {
                    ZirDefine *definition = &module->defines[d];
                    if(!starts_word(definition->value, "#run")) continue;
                    pending++;
                    long value = 0;
                    const char *expression = skip_ws(definition->value + 4);
                    if(EvaluateCompileExpression(module, expression,
                                                 definition->span, 1, &value)) {
                        snprintf(definition->value,
                                 sizeof(definition->value), "%ld", value);
                    } else {
                        if(!EvaluateCompileLiteral(module, expression,
                                                   definition->span, 1, buffers->literal,
                                                   sizeof(buffers->literal), NULL))
                            continue;
                        copy_text(definition->value,
                                  sizeof(definition->value), buffers->literal);
                    }
                    progress++;
                }
            }
        if(progress == 0) {
            if(pending == 0) break;
            for(int p = 0; p < count; p++)
                for(int m = 0; m < programs[p]->module_count; m++) {
                    ZirModule *module = &programs[p]->modules[m];
                    for(int d = 0; d < module->define_count; d++) {
                        ZirDefine *definition = &module->defines[d];
                        if(!starts_word(definition->value, "#run")) continue;
                        Diagnostic(definition->span, "check.consteval",
                                   "#run expression is not a constant: %s",
                                   skip_ws(definition->value + 4));
                        return 0;
                    }
                }
        }
        if(pass == 127) {
            Diagnostic((ZirSourceSpan){0}, "check.consteval",
                       "too many linked compile-time evaluation passes");
            return 0;
        }
    }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            LowerLinkedCompileExpressions(&programs[p]->modules[m], 0);
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int a = 0; a < module->assert_count; a++) {
                ZirAssert *assertion = &module->asserts[a];
                long value = 0;
                if(!EvaluateCompileExpression(module,
                        assertion->condition, assertion->span, 0, &value)) {
                    if(!EvaluateCompileLiteral(module,
                            assertion->condition, assertion->span, 0,
                            buffers->literal, sizeof(buffers->literal), NULL) ||
                       (strcmp(buffers->literal, "0") && strcmp(buffers->literal, "1"))) {
                        Diagnostic(assertion->span, "check.assert",
                                   "#assert requires a compile-time constant condition");
                        return 0;
                    }
                    value = buffers->literal[0] == '1';
                }
                snprintf(assertion->condition,
                         sizeof(assertion->condition), "%d", value != 0);
                if(!value) {
                    Diagnostic(assertion->span, "check.assert",
                               "#assert failed: %s", assertion->message);
                    return 0;
                }
            }
            for(int g = 0; g < module->global_count; g++) {
                ZirGlobal *global = &module->globals[g];
                if(!global->init[0]) continue;
                select_lookup_file(module, global->span);
                memset(&buffers->raw, 0, sizeof(buffers->raw));
                int raw_root = ParseExpr(&buffers->raw, module, global->init,
                                         global->span);
                int literal_initializer = raw_root >= 0 &&
                    buffers->raw.exprs[raw_root].kind == ZIR_EXPR_COMPOUND;
                char constant_name[ZIR_NAME_MAX] = "";
                if(raw_root >= 0 &&
                   buffers->raw.exprs[raw_root].kind == ZIR_EXPR_IDENT)
                    copy_text(constant_name, sizeof(constant_name),
                              buffers->raw.exprs[raw_root].name);
                else if(raw_root >= 0 &&
                        buffers->raw.exprs[raw_root].kind == ZIR_EXPR_MEMBER &&
                        buffers->raw.exprs[raw_root].left >= 0 &&
                        buffers->raw.exprs[buffers->raw.exprs[raw_root].left].kind ==
                            ZIR_EXPR_IDENT) {
                    const ZirExpr *base =
                        &buffers->raw.exprs[buffers->raw.exprs[raw_root].left];
                    int written = snprintf(constant_name,
                        sizeof(constant_name), "%s.%s", base->name,
                        buffers->raw.exprs[raw_root].name);
                    if(written < 0 ||
                       (size_t)written >= sizeof(constant_name))
                        constant_name[0] = '\0';
                }
                if(constant_name[0]) {
                    CompoundConstant compound = {0};
                    if(bound_compound_constant(module, constant_name, 0,
                            &compound) == 1) {
                        if(!lower_compound_global(module, global, &compound,
                                                  buffers->lowered,
                                                  sizeof(buffers->lowered))) {
                            Diagnostic(global->span, "check.constant_type",
                                       "aggregate constant type is unavailable or mismatched: %s",
                                       constant_name);
                            free(buffers->raw.exprs);
                            return 0;
                        }
                        copy_text(global->init, sizeof(global->init),
                                  buffers->lowered);
                        literal_initializer = 1;
                    }
                }
                free(buffers->raw.exprs);
                const ZirModule *folded_owner = NULL;
                if(!literal_initializer && strcmp(global->type, "bool") &&
                   (EvaluateCompileLiteral(module, global->init,
                        global->span, 0, buffers->folded, sizeof(buffers->folded),
                        &folded_owner) ||
                    evaluate_global_startup_literal(module, g, global->init,
                        global->span, buffers->folded, sizeof(buffers->folded),
                        &folded_owner))) {
                    if(folded_owner != NULL) {
                        CompoundConstant compound = {0};
                        copy_text(compound.literal,
                                  sizeof(compound.literal), buffers->folded);
                        compound.owner = folded_owner;
                        compound_qualifier_for_global(&compound,
                                                      global->type);
                        if(!lower_compound_global(module, global, &compound,
                                                  buffers->folded, sizeof(buffers->folded))) {
                            Diagnostic(global->span, "check.constant_type",
                                       "aggregate initializer type is unavailable or mismatched: %s",
                                       global->init);
                            return 0;
                        }
                    }
                    copy_text(global->init, sizeof(global->init), buffers->folded);
                }
                if(!check_file_private_expression(module, global->init,
                                                  global->span))
                    return 0;
                memset(&buffers->expression, 0, sizeof(buffers->expression));
                int root = ParseExprTyped(&buffers->expression, module,
                                          global->init, global->span,
                                          global->type);
                int valid = root >= 0 &&
                    buffers->expression.exprs[root].kind != ZIR_EXPR_UNKNOWN;
                if(valid && !check_file_scope_enum_names(module, &buffers->expression,
                                                         global->span)) {
                    free(buffers->expression.exprs);
                    return 0;
                }
                if(valid) {
                    memset(&buffers->initializer, 0, sizeof(buffers->initializer));
                    buffers->initializer.module = module;
                    buffers->initializer.fn = &buffers->expression;
                    buffers->initializer.programs = programs;
                    buffers->initializer.program_count = count;
                    copy_text(buffers->initializer.expected_type,
                              sizeof(buffers->initializer.expected_type),
                              global->type);
                    if(!reserve_compound_constants(module, &buffers->expression)) {
                        Diagnostic(global->span, "check.global",
                                   "global initializer expression is too large");
                        free(buffers->expression.exprs);
                        return 0;
                    }
                    const char *actual = expression_type(&buffers->initializer, root);
                    if(buffers->initializer.errors == 0) {
                        if(!actual[0])
                            error(&buffers->initializer, global->span,
                                  "cannot infer initializer type", global->name);
                        else if(!compatible_checked(&buffers->initializer,
                                                    global->type, actual)) {
                            char detail[ZIR_TEXT_MAX];
                            error(&buffers->initializer, global->span,
                                  "initializer type mismatch",
                                  mismatch_detail(detail, sizeof(detail),
                                                  global->name, global->type,
                                                  actual));
                        }
                    }
                    int typed_valid = buffers->initializer.errors == 0 &&
                                      !buffers->initializer.failed;
                    for(int i = 0; i < buffers->initializer.restore_count; i++)
                        free(buffers->initializer.restores[i].states);
                    free(buffers->initializer.bindings);
                    free(buffers->initializer.specializations);
                    if(!typed_valid) {
                        if(buffers->initializer.errors == 0)
                            Diagnostic(global->span, "check.global",
                                       "cannot check file-scope initializer: %s",
                                       global->name);
                        free(buffers->expression.exprs);
                        return 0;
                    }
                }
                int runtime_initializer = 0;
                if(valid)
                    for(int e = 0; e < buffers->expression.expr_count; e++) {
                        const ZirExpr *node = &buffers->expression.exprs[e];
                        const ZirModule *owner = NULL;
                        const ZirGlobal *referenced = NULL;
                        if(node->kind == ZIR_EXPR_CALL ||
                           (node->kind == ZIR_EXPR_IDENT &&
                            ResolveGlobalAt(module, node->name,
                                            SpanPath(global->span), &owner,
                                            &referenced) == 1)) {
                            runtime_initializer = 1;
                            break;
                        }
                    }
                free(buffers->expression.exprs);
                if(!valid) {
                    Diagnostic(global->span, "check.global",
                               "invalid file-scope initializer: %s",
                               global->init);
                    return 0;
                }
                if(runtime_initializer) {
                    char name[ZIR_NAME_MAX];
                    int serial = 0, collision;
                    do {
                        snprintf(name, sizeof(name), "__global_init_%d_%d",
                                 g, serial++);
                        collision = 0;
                        for(int f = 0; f < module->function_count; f++)
                            collision |= strcmp(module->functions[f].name,
                                                name) == 0;
                    } while(collision);
                    int written = snprintf(buffers->assignment, sizeof(buffers->assignment),
                                           "%s = %s", global->name,
                                           global->init);
                    if(written < 0 || (size_t)written >= sizeof(buffers->assignment)) {
                        Diagnostic(global->span, "check.global",
                                   "global initializer expression is too large");
                        return 0;
                    }
                    ZirFunction *startup = ModuleAddFunction(module, name,
                        "", "void", 0, global->span);
                    if(startup == NULL ||
                       FunctionAddStmt(startup, ZIR_STMT_ASSIGN, buffers->assignment,
                                       global->span) == NULL)
                        return 0;
                    startup->is_global_initializer = 1;
                    startup->is_file_private = 1;
                    global->init[0] = '\0';
                }
            }
            for(int d = 0; d < module->define_count; d++)
                if(!check_file_private_expression(module,
                       module->defines[d].value,
                       module->defines[d].span))
                    return 0;
        }
    /* A Jai constant may hold a type. Resolve call-shaped constants after
     * imports are linked so ordinary compile-time calls stay constants while
     * Generic(T) becomes a concrete type declaration. */
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int d = 0; d < module->define_count; ) {
                ZirDefine *def = &module->defines[d];
                select_lookup_file(module, def->span);
                const char *value = skip_ws(def->value);
                const char *cursor = value;
                char base[ZIR_NAME_MAX];
                size_t length = 0;
                while((isalnum((unsigned char)*cursor) || *cursor == '_') &&
                      length + 1 < sizeof(base))
                    base[length++] = *cursor++;
                base[length] = '\0';
                cursor = skip_ws(cursor);
                if(length == 0 || *cursor != '(') { d++; continue; }
                const char *start = ++cursor;
                int depth = 1;
                while(*cursor && depth) {
                    if(*cursor == '(') depth++;
                    else if(*cursor == ')') depth--;
                    if(depth) cursor++;
                }
                const char *tail = skip_ws(cursor + 1);
                if(*tail == ';') tail = skip_ws(tail + 1);
                if(depth || cursor == start || *tail != '\0') {
                    d++; continue;
                }
                const ZirType *generic = FindType(module, base, NULL);
                if(generic == NULL || !generic->is_record_template) {
                    d++; continue;
                }
                ZirType *instance = ModuleAddType(module, def->name, def->span);
                if(instance == NULL) return 0;
                instance->is_public = def->is_public;
                instance->is_file_private = def->is_file_private;
                instance->is_type_instance = 1;
                copy_text(instance->template_name,
                          sizeof(instance->template_name), base);
                if((size_t)(cursor - start) >= sizeof(instance->template_args))
                    return 0;
                memcpy(instance->template_args, start,
                       (size_t)(cursor - start));
                instance->template_args[cursor - start] = '\0';
                memmove(def, def + 1,
                        (size_t)(module->define_count - d - 1) * sizeof(*def));
                module->define_count--;
            }
        }
    /* Resolve generic record array bounds in their defining module before
     * instances copy the fields into a caller with a different namespace. */
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            if(!normalize_record_arrays(&programs[p]->modules[m], 1))
                return 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            if(!normalize_type_applications(&programs[p]->modules[m]))
                return 0;
    if(!check_foreign_slice_returns(programs, count))
        return 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int g = 0; g < module->global_count; g++)
                if(module->globals[g].init[0] &&
                   contains_vec(module, module->globals[g].type, 0)) {
                    Diagnostic(module->globals[g].span, "check.vec_copy",
                               "Vec globals require default initialization");
                    return 0;
                }
        }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int t = 0; t < module->type_count; t++) {
                ZirType *type = &module->types[t];
                select_lookup_file(module, type->span);
                if(type->is_type_instance) {
                    const ZirModule *generic_owner = NULL;
                    const ZirType *generic = FindType(module,
                        type->template_name, &generic_owner);
                    if(generic == NULL || !generic->is_record_template ||
                       !InstantiateGenericRecordAt(type, generic, generic_owner, module)) {
                        Diagnostic(type->span, "check.specialize",
                                   "invalid generic type specialization: %s",
                                   type->name);
                        return 0;
                    }
                    type = &module->types[t];
                    if(type->body[0]) {
                        copy_text(buffers->body, sizeof(buffers->body), type->body);
                        if(!rewrite_type_applications(module, buffers->body, buffers->expanded,
                                sizeof(buffers->expanded), type->span, 0))
                            return 0;
                        copy_text(module->types[t].body,
                                  sizeof(module->types[t].body), buffers->expanded);
                    }
                }
                type = &module->types[t];
            }
        }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            if(!order_local_types(&programs[p]->modules[m]))
                return 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            if(!jai_module_types(&programs[p]->modules[m]))
                return 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int d = 0; d < module->define_count; d++) {
                ZirDefine *definition = &module->defines[d];
                const char *value = skip_ws(definition->value);
                memset(&buffers->expression, 0, sizeof(buffers->expression));
                int root;
                int valid;
                /* Array type aliases name a storage shape, not a value. */
                if(value[0] == '[')
                    continue;
                root = ParseExprNoDefaults(&buffers->expression, module,
                                           definition->value,
                                           definition->span);
                valid = root >= 0 &&
                    buffers->expression.exprs[root].kind != ZIR_EXPR_UNKNOWN;
                free(buffers->expression.exprs);
                if(!valid) {
                    Diagnostic(definition->span, "check.constant",
                               "invalid file-scope constant: %s",
                               definition->value);
                    return 0;
                }
            }
        }
    for(int p = 0; p < count; p++) {
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int f = 0; f < module->function_count; f++) {
                copy_text(buffers->saved_path, sizeof(buffers->saved_path),
                          module->lookup_path);
                select_lookup_file(module, module->functions[f].span);
                int normalized = normalize_function_arrays(
                    module, &module->functions[f]);
                copy_text(module->lookup_path, sizeof(module->lookup_path),
                          buffers->saved_path);
                TypeLookupsChanged();
                if(!normalized)
                    return 0;
            }
        }
    }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            if(!check_type_declarations(&programs[p]->modules[m]) ||
               !normalize_record_arrays(&programs[p]->modules[m], 0))
                return 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            copy_text(buffers->saved_path, sizeof(buffers->saved_path), module->lookup_path);
            for(int g = 0; g < module->global_count; g++) {
                ZirGlobal *global = &module->globals[g];
                select_lookup_file(module, global->span);
                normalize_array(module, global->type, sizeof(global->type));
            }
            copy_text(module->lookup_path, sizeof(module->lookup_path),
                      buffers->saved_path);
            TypeLookupsChanged();
        }
    for(int p = 0; p < count; p++) for(int m = 0; m < programs[p]->module_count; m++) {
        buffers->c.module = &programs[p]->modules[m];
        for(int i = 0; i < buffers->c.module->global_count; i++) {
            select_lookup_file(buffers->c.module, buffers->c.module->globals[i].span);
            const char *type = buffers->c.module->globals[i].type;
            ValidatedRecords checked = {0};
            const char *error = storage_type_error(buffers->c.module, type, NULL, 0,
                                                   &checked, NULL, 0);
            free(checked.items);
            if(error != NULL) {
                ZirSourceSpan span = buffers->c.module->globals[i].span;
                Diagnostic(span, "check.storage", "%s: %s", error, type);
                free(buffers->c.bindings);
                return 0;
            }
        }
        for(int f = 0; f < buffers->c.module->function_count; f++) {
            if(!check_go_method(&buffers->c, &buffers->c.module->functions[f])) {
                free(buffers->c.bindings);
                free(buffers->c.specializations);
                free(buffers->c.enum_names);
                return 0;
            }
            if(buffers->c.module->functions[f].is_template) {
                if(!check_template_declaration(&buffers->c,
                        &buffers->c.module->functions[f])) {
                    free(buffers->c.bindings);
                    free(buffers->c.specializations);
                        free(buffers->c.enum_names);
                    return 0;
                }
                continue;
            }
            if(!check_function(&buffers->c, &buffers->c.module->functions[f])) {
                free(buffers->c.bindings);
                free(buffers->c.specializations);
                        free(buffers->c.enum_names);
                return 0;
            }
        }
        buffers->c.module->lookup_path[0] = '\0';
    }
    for(;;) {
        int pending = buffers->c.specialization_count + buffers->c.enum_name_count;
        if(pending == 0) break;
        if(!instantiate_enum_names(&buffers->c) ||
           !instantiate_specializations(&buffers->c)) {
            free(buffers->c.bindings);
            free(buffers->c.specializations);
                        free(buffers->c.enum_names);
            return 0;
        }
        for(int p = 0; p < count; p++)
            for(int m = 0; m < programs[p]->module_count; m++) {
                buffers->c.module = &programs[p]->modules[m];
                for(int f = 0; f < buffers->c.module->function_count; f++) {
                    ZirFunction *instance = &buffers->c.module->functions[f];
                    if(!instance->is_specialization || instance->checked)
                        continue;
                    if(!check_function(&buffers->c, instance)) {
                        free(buffers->c.bindings);
                        free(buffers->c.specializations);
                        free(buffers->c.enum_names);
                        return 0;
                    }
                }
                buffers->c.module->lookup_path[0] = '\0';
            }
    }
    /* Runtime implementations become host methods when they need host services.
     * Propagate through resolved calls, including mutually recursive modules. */
    int changed;
    do {
        changed = 0;
        for(int p = 0; p < count; p++) {
            for(int m = 0; m < programs[p]->module_count; m++) {
                ZirModule *module = &programs[p]->modules[m];
                for(int f = 0; f < module->function_count; f++) {
                    ZirFunction *fn = &module->functions[f];
                    if(fn->uses_host)
                        continue;
                    for(int x = 0; x < fn->expr_count; x++) {
                        const ZirFunction *callee = NULL;
                        const ZirModule *owner = NULL;
                        if((fn->exprs[x].kind == ZIR_EXPR_CALL || fn->exprs[x].is_function_value) &&
                           ResolveFunction(module, fn->exprs[x].name, &owner, &callee) == 1 &&
                           callee->uses_host) {
                            fn->uses_host = 1;
                            changed = 1;
                            break;
                        }
                    }
                }
            }
        }
    } while(changed);
    free(buffers->c.bindings);
    free(buffers->c.specializations);
                        free(buffers->c.enum_names);
    if(buffers->c.failed || buffers->c.errors != 0 || !CheckSliceLifetimes(programs, count))
        return 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            if(!name_private_functions(&programs[p]->modules[m]) ||
               !name_private_defines(&programs[p]->modules[m]) ||
               !name_private_globals(&programs[p]->modules[m]) ||
               !name_private_types(&programs[p]->modules[m]))
                return 0;
    DeriveEffectClasses(programs, count);
    if(!CheckParallelRegions(programs, count))
        return 0;
    if(!CheckLawGates(programs, count))
        return 0;
    return 1;
}

int
CheckPrograms(ZirProgram **programs, int count)
{
    static _Thread_local CheckProgramsBuffers *spares[16];
    static _Thread_local int spare_count;
    CheckProgramsBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = CheckPrograms_with_buffers(programs, count, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

/* A cast the program wrote, as opposed to one the checker inserted when it
 * widened a value: those keep their operand's source text, which may itself
 * be a written cast. */
static int
written_cast(const ZirFunction *fn, int index)
{
    if(index < 0 || index >= fn->expr_count)
        return 0;
    const ZirExpr *cast = &fn->exprs[index];
    return cast->kind == ZIR_EXPR_CAST && cast->right >= 0 && cast->text != NULL &&
           strncmp(skip_ws(cast->text), "cast", 4) == 0 &&
           (fn->exprs[cast->right].text == NULL ||
            strcmp(cast->text, fn->exprs[cast->right].text) != 0) &&
           fn->exprs[cast->right].kind != ZIR_EXPR_INT &&
           fn->exprs[cast->right].kind != ZIR_EXPR_FLOAT;
}

static int
lint_cast(const ZirFunction *fn, int index, int whole_value)
{
    if(!written_cast(fn, index))
        return 0;
    const ZirExpr *cast = &fn->exprs[index];
    const char *from = fn->exprs[cast->right].type;
    if(!*from || !*cast->name)
        return 0;
    if(!strcmp(cast->name, from)) {
        Warning(cast->span, "lint.cast", "cast(%s) is not needed: the value is already %s",
                cast->name, from);
        return 1;
    }
    if(whole_value && widens_losslessly(cast->name, from)) {
        Warning(cast->span, "lint.cast", "cast(%s) is not needed: %s widens to %s implicitly",
                cast->name, from, cast->name);
        return 1;
    }
    return 0;
}

/* Whether TEXT holds NUMBER as a whole token. */
static int
holds_number_token(const char *text, const char *number)
{
    size_t length = strlen(number);
    for(const char *at = strstr(text, number); at != NULL; at = strstr(at + 1, number))
        if((at == text || !isalnum((unsigned char)at[-1])) &&
           !isalnum((unsigned char)at[length]) && at[length] != '.')
            return 1;
    return 0;
}

/* A byte compared with a printable character's code written as a number,
 * as C ports do: text[i] == 65 reads better as text[i] == #char "A". */
static int
lint_character(const ZirFunction *fn, int index)
{
    const ZirExpr *compare = &fn->exprs[index];
    static const char *const ops[] = {"==", "!=", "<", "<=", ">", ">=", NULL};
    int comparison = 0;
    for(int i = 0; ops[i] != NULL; i++)
        comparison |= !strcmp(compare->op, ops[i]);
    if(compare->kind != ZIR_EXPR_BINARY || !comparison || compare->left < 0 ||
       compare->right < 0 || compare->text == NULL || strstr(compare->text, "#char"))
        return 0;
    const ZirExpr *left = &fn->exprs[compare->left], *right = &fn->exprs[compare->right];
    const ZirExpr *number = left->kind == ZIR_EXPR_INT ? left :
                            right->kind == ZIR_EXPR_INT ? right : NULL;
    const ZirExpr *byte = number == left ? right : left;
    if(number == NULL || byte->kind == ZIR_EXPR_INT || strcmp(byte->type, "u8") ||
       number->text == NULL)
        return 0;
    char *end = NULL;
    long value = strtol(number->text, &end, 10);
    if(end == number->text || *end != '\0' || value < 32 || value > 126 ||
       !holds_number_token(compare->text, number->text))
        return 0;
    char spelled[8];
    if(value == '"' || value == '\\')
        snprintf(spelled, sizeof(spelled), "\\%c", (int)value);
    else
        snprintf(spelled, sizeof(spelled), "%c", (int)value);
    Warning(compare->span, "lint.char", "compare with #char \"%s\" rather than %ld",
            spelled, value);
    return 1;
}

/* Whether a procedure or foreign procedure named like call NAME, in
 * MODULE or a module it imports, takes variadic arguments (args: ..any):
 * those are not widened to a parameter type, so their casts matter. */
static int
variadic_callee(const ZirModule *module, const char *name)
{
    const char *dot = strrchr(name, '.');
    const char *base = dot != NULL ? dot + 1 : name;
    for(int m = -1; m < module->import_count; m++) {
        const ZirModule *scope = m < 0 ? module : module->imports[m].resolved_module;
        if(scope == NULL) continue;
        for(int f = 0; f < scope->function_count; f++)
            if((!strcmp(scope->functions[f].name, base) ||
                !strcmp(scope->functions[f].overload_name, base)) &&
               strstr(FunctionArgs(&scope->functions[f]), "..") != NULL)
                return 1;
        for(int i = 0; i < scope->import_count; i++)
            if(scope->imports[i].kind == ZIR_IMPORT_EXTERN &&
               !strcmp(scope->imports[i].name, base) &&
               strstr(scope->imports[i].args, "..") != NULL)
                return 1;
    }
    return 0;
}

/* Whether declaration ST names its type, so its initializer can widen. */
static int
declared_type_written(const ZirStmt *st)
{
    /* A range's bounds set its index type, so their casts are not whole
     * values even though the lowering declares them. */
    if(!strncmp(st->name, "range_", 6))
        return 0;
    const char *text = st->text != NULL ? skip_ws(st->text) : "";
    size_t length = strlen(st->name);
    if(strncmp(text, st->name, length) != 0)
        return 0;
    text = skip_ws(text + length);
    return text[0] == ':' && text[1] != '=';
}

/* Whether expression INDEX is the literal 0 or 1 (a flag's value), or a
 * cast of a bool to an integer (whose value is also 0 or 1). */
static int
flag_value(const ZirFunction *fn, int index)
{
    if(index < 0 || index >= fn->expr_count)
        return 0;
    const ZirExpr *e = &fn->exprs[index];
    if(e->kind == ZIR_EXPR_INT && e->text != NULL)
        return !strcmp(e->text, "0") || !strcmp(e->text, "1");
    return e->kind == ZIR_EXPR_CAST && e->right >= 0 &&
           !strcmp(fn->exprs[e->right].type, "bool");
}

/* An integer local used only as a flag: declared with an integer type,
 * given only 0, 1, or a bool cast to an integer, and read only in
 * comparisons with 0 or 1. Such a local reads better as a bool. */
static int
lint_flags(const ZirFunction *fn)
{
    int warnings = 0;
    int *parent = malloc((size_t)(fn->expr_count ? fn->expr_count : 1) * sizeof(int));
    if(parent == NULL)
        return 0;
    for(int e = 0; e < fn->expr_count; e++)
        parent[e] = -1;
    for(int e = 0; e < fn->expr_count; e++) {
        const ZirExpr *x = &fn->exprs[e];
        if(x->left >= 0 && x->left < fn->expr_count) parent[x->left] = e;
        if(x->right >= 0 && x->right < fn->expr_count) parent[x->right] = e;
        if(x->third >= 0 && x->third < fn->expr_count) parent[x->third] = e;
        for(int child = x->first_child; child >= 0 && child < fn->expr_count;
            child = fn->exprs[child].next_sibling)
            parent[child] = e;
    }
    for(int d = 0; d < fn->stmt_count; d++) {
        const ZirStmt *decl = &fn->stmts[d];
        if(decl->kind != ZIR_STMT_DECL || !integer_type(decl->type) ||
           !strcmp(decl->type, "bool") || !strncmp(decl->name, "range_", 6) ||
           !strncmp(decl->name, "case_value_", 11) || !declared_type_written(decl) ||
           (decl->expr_root >= 0 && !flag_value(fn, decl->expr_root)))
            continue;
        int ok = 1, reads = 0;
        /* One declaration of the name in the procedure, and not a parameter. */
        for(int other = 0; other < fn->stmt_count && ok; other++)
            if(other != d && fn->stmts[other].kind == ZIR_STMT_DECL &&
               !strcmp(fn->stmts[other].name, decl->name))
                ok = 0;
        char needle[ZIR_NAME_MAX + 2];
        snprintf(needle, sizeof(needle), "%s:", decl->name);
        if(strstr(FunctionArgs(fn), needle) != NULL)
            ok = 0;
        for(int s = 0; s < fn->stmt_count && ok; s++) {
            const ZirStmt *st = &fn->stmts[s];
            if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0 &&
               fn->exprs[st->lhs_root].kind == ZIR_EXPR_IDENT &&
               !strcmp(fn->exprs[st->lhs_root].name, decl->name) &&
               (strcmp(st->assignment_op, "=") || !flag_value(fn, st->expr_root)))
                ok = 0;
        }
        for(int e = 0; e < fn->expr_count && ok; e++) {
            const ZirExpr *x = &fn->exprs[e];
            if(x->kind != ZIR_EXPR_IDENT || strcmp(x->name, decl->name))
                continue;
            int assigned = 0;
            for(int s = 0; s < fn->stmt_count; s++)
                if(fn->stmts[s].kind == ZIR_STMT_ASSIGN && fn->stmts[s].lhs_root == e)
                    assigned = 1;
            if(assigned)
                continue;
            int p = parent[e];
            if(p < 0) { ok = 0; break; }
            const ZirExpr *compare = &fn->exprs[p];
            int other = compare->left == e ? compare->right : compare->left;
            if(compare->kind != ZIR_EXPR_BINARY ||
               (strcmp(compare->op, "==") && strcmp(compare->op, "!=")) ||
               other < 0 || fn->exprs[other].kind != ZIR_EXPR_INT ||
               fn->exprs[other].text == NULL ||
               (strcmp(fn->exprs[other].text, "0") && strcmp(fn->exprs[other].text, "1")))
                ok = 0;
            reads++;
        }
        if(ok && reads > 0) {
            Warning(decl->span, "lint.bool",
                    "%s holds only 0 or 1 and is only compared with them; declare it bool",
                    decl->name);
            warnings++;
        }
    }
    free(parent);
    return warnings;
}

int
LintPrograms(ZirProgram **programs, int count)
{
    int warnings = 0;
    const char *root = count > 0 && programs[0]->module_count > 0 ?
        programs[0]->modules[0].source_root : "";
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            if(strcmp(module->source_root, root) != 0)
                continue;
            for(int f = 0; f < module->function_count; f++) {
                const ZirFunction *fn = &module->functions[f];
                if(!fn->checked || fn->from_ir || fn->is_template ||
                   fn->is_specialization || !strncmp(fn->name, "zi_", 3))
                    continue;
                /* Whole values: roots of typed declarations, assignments,
                 * and returns, and arguments of calls that no overload or
                 * type parameter chooses by argument type. */
                char *whole = calloc((size_t)fn->expr_count + 1, 1);
                if(whole == NULL)
                    return warnings;
                for(int s = 0; s < fn->stmt_count; s++) {
                    const ZirStmt *st = &fn->stmts[s];
                    if(st->expr_root < 0 || st->expr_root >= fn->expr_count)
                        continue;
                    if((st->kind == ZIR_STMT_DECL && declared_type_written(st)) ||
                       (st->kind == ZIR_STMT_ASSIGN && !strcmp(st->assignment_op, "=")) ||
                       st->kind == ZIR_STMT_RETURN)
                        whole[st->expr_root] = 1;
                }
                for(int e = 0; e < fn->expr_count; e++) {
                    const ZirExpr *call = &fn->exprs[e];
                    if(call->kind != ZIR_EXPR_CALL || !call->name[0] ||
                       strstr(call->name, "__overload_") || strstr(call->name, "__zi_spec") ||
                       !strncmp(call->name, "zi_", 3) || !strcmp(call->name, "print") ||
                       !strncmp(call->name, "Vec", 3) || !strncmp(call->name, "Builder", 7) ||
                       MapPrimitiveName(call->name) || variadic_callee(module, call->name))
                        continue;
                    for(int child = call->first_child; child >= 0;
                        child = fn->exprs[child].next_sibling)
                        whole[child] = 1;
                }
                for(int e = 0; e < fn->expr_count; e++)
                    warnings += lint_cast(fn, e, whole[e]) + lint_character(fn, e);
                warnings += lint_flags(fn);
                free(whole);
            }
        }
    return warnings;
}
