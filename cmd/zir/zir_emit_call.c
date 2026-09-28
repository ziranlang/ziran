#include "zir_emit_internal.h"

static void
global_slot_wrapper_name(const ZirModule *module, const ZirGlobal *global,
                         int index, char *out, size_t size)
{
    char prefix[64];
    number_prefix(module, prefix, sizeof(prefix));
    format(out, size, "%s_global_slot_%ld_%d", prefix,
           (long)(global - module->globals), index);
}

static int
append_global_literal(char *out, size_t size, size_t *used,
                      const char *format_string, ...)
{
    va_list ap;
    int written;
    if(*used >= size) return 0;
    va_start(ap, format_string);
    written = vsnprintf(out + *used, size - *used, format_string, ap);
    va_end(ap);
    if(written < 0 || (size_t)written >= size - *used) return 0;
    *used += (size_t)written;
    return 1;
}

static int
global_field(const ZirType *record, const char *name, int ordinal,
             ZirTypeField *found)
{
    ZirTypeField field;
    size_t offset = 0;
    int position = 0;
    while(TypeNextField(record, &offset, &field) == 1) {
        if((name[0] && !strcmp(name, field.name)) ||
           (!name[0] && position == ordinal)) {
            *found = field;
            return 1;
        }
        position++;
    }
    return 0;
}

static const ZirType *
global_function_slot(const ZirModule *module, const ZirModule *scope,
                     const char *type, const ZirExpr *expr)
{
    const ZirType *slot = FindType(scope, type, NULL);
    const ZirModule *owner = NULL;
    const ZirFunction *declaration = NULL;
    if(slot == NULL || !slot->is_procedure_type ||
       expr->kind != ZIR_EXPR_IDENT ||
       ResolveFunction(module, expr->name, &owner, &declaration) != 1 ||
       declaration->is_extern)
        return NULL;
    return slot;
}

static int
global_type_at_use(const ZirModule *module, const ZirModule *scope,
                   const char *type, char *out, size_t size)
{
    char element[ZIR_NAME_MAX];
    if(ArrayElementType(type, element, sizeof(element), NULL)) {
        const char *close = strchr(type, ']');
        char mapped[ZIR_NAME_MAX];
        if(close == NULL ||
           !global_type_at_use(module, scope, element, mapped,
                               sizeof(mapped))) return 0;
        int written = snprintf(out, size, "%.*s%s",
                               (int)(close - type + 1), type, mapped);
        return written >= 0 && (size_t)written < size;
    }
    const ZirType *declared = FindType(scope, type, NULL);
    if(scope == module || declared == NULL ||
       FindType(module, type, NULL) == declared) {
        copy_text(out, size, type);
        return strlen(type) < size;
    }
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *import = &module->imports[i];
        if(import->kind != ZIR_IMPORT_MODULE ||
           import->resolved_module != scope) continue;
        int written = snprintf(out, size, "%s.%s", import->name, type);
        if(written >= 0 && (size_t)written < size &&
           FindType(module, out, NULL) == declared)
            return 1;
    }
    return 0;
}

static int
emit_global_literal_node(GlobalLiteralEmit *emit, int index,
                         const ZirModule *scope, const char *type,
                         char *out, size_t size,
                         size_t *used, int depth)
{
    if(index < 0 || index >= emit->probe->expr_count || depth > 32)
        return 0;
    const ZirExpr *expr = &emit->probe->exprs[index];
    if(expr->kind == ZIR_EXPR_COMPOUND) {
        char element[ZIR_NAME_MAX];
        const ZirType *record = NULL;
        const ZirModule *record_owner = NULL;
        int array = ArrayElementType(type, element, sizeof(element), NULL);
        if(!array) record = FindType(scope, type, &record_owner);
        if(!array && (record == NULL || record->is_enum ||
                      record->is_procedure_type || record->is_union))
            return 0;
        if(emit->target == ZIR_GO) {
            char mapped[ZIR_NAME_MAX * 2];
            char visible[ZIR_NAME_MAX];
            if(emit->type_name == NULL ||
               !global_type_at_use(emit->module, scope, type, visible,
                                   sizeof(visible)) ||
               !emit->type_name(emit->module, visible, mapped,
                                sizeof(mapped), emit->context) ||
               !append_global_literal(out, size, used, "%s{", mapped))
                return 0;
        } else if(!append_global_literal(out, size, used, "{"))
            return 0;
        if(record != NULL && emit->target == ZIR_CPP) {
            ZirTypeField field;
            size_t offset = 0;
            int field_index = 0;
            while(TypeNextField(record, &offset, &field) == 1) {
                int selected = -1, entry_ordinal = 0;
                for(int child = expr->first_child; child >= 0;
                    child = emit->probe->exprs[child].next_sibling) {
                    const ZirExpr *entry = &emit->probe->exprs[child];
                    if((entry->name[0] &&
                        !strcmp(entry->name, field.name)) ||
                       (!entry->name[0] && entry_ordinal == field_index)) {
                        selected = entry->right;
                        break;
                    }
                    entry_ordinal++;
                }
                if(field_index++ &&
                   !append_global_literal(out, size, used, ", ")) return 0;
                if(selected >= 0) {
                    if(!emit_global_literal_node(emit, selected,
                                                 record_owner, field.type,
                                                 out, size, used, depth + 1))
                        return 0;
                } else if(!append_global_literal(out, size, used, "{}"))
                    return 0;
            }
        } else {
            int position = 0;
            for(int child = expr->first_child; child >= 0;
                child = emit->probe->exprs[child].next_sibling) {
                const ZirExpr *entry = &emit->probe->exprs[child];
                const char *field_type = element;
                ZirTypeField field;
                if(entry->right < 0 ||
                   (record != NULL &&
                    !global_field(record, entry->name, position, &field)))
                    return 0;
                if(position &&
                   !append_global_literal(out, size, used, ", ")) return 0;
                if(record != NULL) {
                    char mapped[ZIR_NAME_MAX * 2];
                    field_type = field.type;
                    if(emit->field_name != NULL)
                        emit->field_name(record, field.name, mapped,
                                         sizeof(mapped), emit->context);
                    else
                        TargetFieldName(record, emit->target, field.name,
                                        mapped, sizeof(mapped));
                    if(!append_global_literal(out, size, used,
                         emit->target == ZIR_GO ? "%s: " : ".%s = ",
                         mapped)) return 0;
                }
                if(!emit_global_literal_node(emit, entry->right,
                                             record != NULL ? record_owner :
                                             scope, field_type,
                                             out, size, used, depth + 1))
                    return 0;
                position++;
            }
        }
        return append_global_literal(out, size, used, "}");
    }
    const ZirType *slot = global_function_slot(emit->module, scope,
                                                type, expr);
    if(slot != NULL) {
        char call[ZIR_TEXT_MAX], resolved[ZIR_TEXT_MAX];
        size_t length;
        if(emit->target == ZIR_GO || slot->is_c_call) {
            if(emit->scalar == NULL) return 0;
            format(call, sizeof(call), "%s()", expr->name);
            if(!emit->scalar(emit->module, call, resolved,
                             sizeof(resolved), emit->context)) return 0;
            length = strlen(resolved);
            if(length < 2 || strcmp(resolved + length - 2, "()")) return 0;
            resolved[length - 2] = '\0';
            return append_global_literal(out, size, used, "%s", resolved);
        }
        char wrapper[ZIR_NAME_MAX];
        global_slot_wrapper_name(emit->module, emit->global, index,
                                 wrapper, sizeof(wrapper));
        return append_global_literal(out, size, used,
                                     emit->target == ZIR_C ? "{NULL, %s}" :
                                                             "{nullptr, %s}",
                                     wrapper);
    }
    char scalar[ZIR_TEXT_MAX * 2];
    if(!ScalarLiteral(type, expr->text, emit->target, emit->span,
                      scalar, sizeof(scalar))) {
        if(emit->scalar == NULL ||
           !emit->scalar(emit->module, expr->text, scalar,
                         sizeof(scalar), emit->context)) return 0;
    }
    return append_global_literal(out, size, used, "%s", scalar);
}

int
EmitGlobalInitializer(const ZirModule *module, const ZirGlobal *global,
                      ZirTarget target, ZirGlobalScalarRewrite scalar,
                      ZirGlobalTypeRewrite type_name,
                      ZirGlobalFieldRewrite field_name, void *context,
                      char *out, size_t size)
{
    ZirFunction probe = {0};
    int root, ok;
    size_t used = 0;
    if(size == 0 || !global->init[0]) return 0;
    out[0] = '\0';
    root = ParseExprTyped(&probe, module, global->init,
                          global->span, global->type);
    if(root < 0 || (probe.exprs[root].kind != ZIR_EXPR_COMPOUND &&
       global_function_slot(module, module, global->type,
                            &probe.exprs[root]) == NULL)) {
        free(probe.exprs);
        return 0;
    }
    GlobalLiteralEmit emit = {module, global, &probe, target, global->span,
                              scalar, type_name, field_name, context};
    ok = emit_global_literal_node(&emit, root, module, global->type, out, size,
                                  &used, 0);
    free(probe.exprs);
    return ok ? 1 : -1;
}

static void
emit_global_slot_wrappers_node(FILE *out, const ZirModule *module,
                               const ZirGlobal *global,
                               const ZirFunction *probe, int index,
                               const ZirModule *scope, const char *type,
                               ZirTarget target, ZirResolveTarget resolver,
                               void *context, int depth)
{
    if(index < 0 || index >= probe->expr_count || depth > 32) return;
    const ZirExpr *value = &probe->exprs[index];
    if(value->kind == ZIR_EXPR_COMPOUND) {
        char element[ZIR_NAME_MAX];
        const ZirModule *record_owner = NULL;
        int array = ArrayElementType(type, element, sizeof(element), NULL);
        const ZirType *record = array ? NULL :
            FindType(scope, type, &record_owner);
        int position = 0;
        for(int child = value->first_child; child >= 0;
            child = probe->exprs[child].next_sibling) {
            const ZirExpr *entry = &probe->exprs[child];
            ZirTypeField field;
            if(entry->right >= 0 &&
               (array || (record != NULL &&
                          global_field(record, entry->name, position,
                                       &field))))
                emit_global_slot_wrappers_node(out, module, global, probe,
                    entry->right, record ? record_owner : scope,
                    record ? field.type : element, target, resolver,
                    context, depth + 1);
            position++;
        }
        return;
    }
    const ZirType *slot = global_function_slot(module, scope, type, value);
    if(slot == NULL || slot->is_c_call) return;
    char parameters[64][ZIR_TEXT_MAX];
    int count = *skip_ws(slot->body) ?
        split_top_level(slot->body, parameters[0], 64,
                        sizeof(parameters[0])) : 0;
    char wrapper[ZIR_NAME_MAX], call[ZIR_TEXT_MAX], resolved[ZIR_TEXT_MAX];
    char result_type[ZIR_NAME_MAX], resolved_result[ZIR_NAME_MAX];
    slot_native_type(slot->procedure_return_type, target,
                     result_type, sizeof(result_type));
    resolver(context, result_type, resolved_result,
             sizeof(resolved_result));
    global_slot_wrapper_name(module, global, index,
                             wrapper, sizeof(wrapper));
    fprintf(out, "static %s %s(void *context", resolved_result, wrapper);
    size_t length = (size_t)format(call, sizeof(call), "%s(", value->name);
    for(int argument = 0; argument < count; argument++) {
        const char *separator = strchr(parameters[argument], ':');
        if(separator == NULL) break;
        const char *source = skip_ws(separator + 1);
        char native_type[ZIR_NAME_MAX], resolved_type[ZIR_NAME_MAX];
        slot_native_type(source, target, native_type, sizeof(native_type));
        resolver(context, native_type, resolved_type, sizeof(resolved_type));
        fprintf(out, ", %s slot_arg_%d", resolved_type, argument);
        length += (size_t)format(call + length, sizeof(call) - length,
                                  "%sslot_arg_%d",
                                  argument ? ", " : "", argument);
    }
    format(call + length, sizeof(call) - length, ")");
    resolver(context, call, resolved, sizeof(resolved));
    fprintf(out, ")\n{\n    (void)context;\n    %s%s;\n}\n",
            strcmp(slot->procedure_return_type, "void") ? "return " : "",
            resolved);
}

void
EmitGlobalSlotWrappers(FILE *out, const ZirModule *module,
                       const ZirGlobal *global, ZirTarget target,
                       ZirResolveTarget resolver, void *context)
{
    ZirFunction probe = {0};
    if((target != ZIR_C && target != ZIR_CPP) || !global->init[0])
        return;
    int root = ParseExprTyped(&probe, module, global->init,
                              global->span, global->type);
    if(root >= 0)
        emit_global_slot_wrappers_node(out, module, global, &probe, root,
                                       module, global->type, target,
                                       resolver, context, 0);
    free(probe.exprs);
}

static void
call_parameter_type(Emitter *e, const ZirExpr *call, int ordinal,
                    char *out, size_t size)
{
    const char *signature = NULL;
    if(call->slot_type[0]) {
        const ZirType *slot = FindType(e->module, call->slot_type, NULL);
        if(slot != NULL && slot->is_procedure_type)
            signature = slot->body;
    } else {
        const ZirModule *owner = NULL;
        const ZirFunction *function = NULL;
        if(ResolveFunction(e->module, call->name, &owner, &function) == 1 &&
           function != NULL)
            signature = function->args;
        else
            for(int i = 0; i < e->module->import_count; i++)
                if(e->module->imports[i].kind == ZIR_IMPORT_EXTERN &&
                   !strcmp(e->module->imports[i].name, call->name)) {
                    signature = e->module->imports[i].args;
                    break;
                }
    }
    if(signature == NULL || !*signature)
        return;
    char (*parts)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parts));
    if(parts == NULL)
        return;
    int count = split_top_level(signature, parts[0], 64, sizeof(parts[0]));
    if(ordinal >= 0 && ordinal < count) {
        char *colon = strchr(parts[ordinal], ':');
        if(colon != NULL) {
            colon = (char *)skip_ws(colon + 1);
            trim_in_place(colon);
            copy_text(out, size, colon);
        }
    }
    free(parts);
}

static int
identifier_byte(char c)
{
    return c == '_' || (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static void
replace_call_placeholders(const char *text, char arguments[][ZIR_NAME_MAX],
                          int count, char *out, size_t size)
{
    static const char prefix[] = "zir_call_argument_";
    size_t used = 0;
    const char *p = text;
    while(*p && used + 1 < size) {
        int ordinal = 0;
        const char *digits = p + sizeof(prefix) - 1;
        const char *end = digits;
        if((p == text || !identifier_byte(p[-1])) &&
           !strncmp(p, prefix, sizeof(prefix) - 1)) {
            while(*end >= '0' && *end <= '9')
                ordinal = ordinal * 10 + (*end++ - '0');
        }
        if(end > digits && !identifier_byte(*end) && ordinal < count) {
            used += strlen(arguments[ordinal]);
            if(used >= size)
                break;
            memcpy(out + used - strlen(arguments[ordinal]), arguments[ordinal],
                   strlen(arguments[ordinal]));
            p = end;
            continue;
        }
        out[used++] = *p++;
    }
    out[used < size ? used : size - 1] = '\0';
}

void
emit_call(Emitter *e, const ZirExpr *expr, const char *array_result, char *out, size_t size)
{
    char text[ZIR_TEXT_MAX];
    char arguments[64][ZIR_NAME_MAX] = {{0}};
    int count=0;
    int argument_count = 0;
    int has_named = 0;
    size_t n;
    for(int child = expr->first_child; child >= 0;
        child = e->fn->exprs[child].next_sibling)
        has_named |= e->fn->exprs[child].argument_name[0] != '\0';
    if(*expr->slot_type) {
        char callable[ZIR_NAME_MAX];
        fresh(e, callable);
        char source[ZIR_TEXT_MAX];
        if(expr->name[0]) {
            resolve(e, expr->name, source, sizeof(source));
        } else {
            /* Anonymous slot call: the callee expression (a record field,
             * a local, or an index) names the callable. */
            source[0] = '\0';
            emit_expr(e, expr->left, expr->slot_type, source, sizeof(source));
        }
        declare(e, callable, expr->slot_type, source);
        if(e->target == ZIR_C || e->target == ZIR_CPP) {
            const ZirType *slot = FindType(e->module, expr->slot_type, NULL);
            if(slot != NULL && slot->is_c_call) {
                n = (size_t)format(text, sizeof(text), "%s(", callable);
            } else {
                n = (size_t)format(text, sizeof(text), "%s.call(%s.context", callable, callable);
                count = 1;
            }
        } else {
            n = (size_t)format(text, sizeof(text), "%s(", callable);
        }
    } else {
        n = (size_t)format(text, sizeof(text), "%s(", expr->name);
    }
    if(array_result != NULL) {
        n += (size_t)format(text + n, sizeof(text) - n, "%s%s", count ? "," : "", array_result);
        count++;
    }
    for(int child=expr->first_child;child>=0;child=e->fn->exprs[child].next_sibling) {
        int ordinal = e->fn->exprs[child].argument_index;
        char argument[ZIR_TEXT_MAX];
        char argument_type[ZIR_NAME_MAX];
        if(ordinal < 0 || ordinal >= 64 || arguments[ordinal][0])
            fatal(expr, "invalid checked call argument order");
        copy_text(argument_type, sizeof(argument_type),
                  e->fn->exprs[child].type);
        if(!strcmp(argument_type, "null") ||
           !strcmp(argument_type, "integer")) {
            char parameter_type[ZIR_NAME_MAX] = "";
            call_parameter_type(e, expr, ordinal, parameter_type,
                                sizeof(parameter_type));
            if(!strcmp(argument_type, "null") ||
               (enum_type(e->module, parameter_type) &&
                FindType(e->module, parameter_type, NULL)->is_enum_flags))
                copy_text(argument_type, sizeof(argument_type), parameter_type);
        }
        /* Arguments run left to right. One can stay in the call text only
         * when no later argument calls something that could change it, and
         * named arguments, which may be reordered, are captured. */
        int later_calls = 0;
        for(int next = e->fn->exprs[child].next_sibling; next >= 0;
            next = e->fn->exprs[next].next_sibling)
            later_calls |= expression_calls(e->fn, next);
        e->call_in_place = !has_named && !later_calls;
        emit_expr(e,child,argument_type,argument,sizeof(argument));
        if(!plain_identifier(argument) &&
           (has_named || later_calls || strlen(argument) >= ZIR_NAME_MAX)) {
            char captured[ZIR_NAME_MAX];
            fresh(e, captured);
            declare(e, captured, argument_type, argument);
            copy_text(argument, sizeof(argument), captured);
        }
        copy_text(arguments[ordinal], sizeof(arguments[ordinal]), argument);
        argument_count++;
    }
    /* Arguments are already lowered. The resolver maps every identifier in
     * the call text to its global, so it sees placeholders instead: a local
     * or parameter must never become an imported name it shadows. */
    for(int ordinal = 0; ordinal < argument_count; ordinal++) {
        char placeholder[ZIR_NAME_MAX];
        if(!arguments[ordinal][0])
            fatal(expr, "missing checked call argument");
        format(placeholder, sizeof(placeholder), "zir_call_argument_%d", ordinal);
        n += (size_t)format(text+n, sizeof(text)-n, "%s%s", count ? "," : "",
                            *expr->slot_type ? arguments[ordinal] : placeholder);
        count++;
    }
    format(text+n,sizeof(text)-n,")");
    if(*expr->slot_type) {
        copy_text(out, size, text);
        return;
    }
    char resolved[ZIR_TEXT_MAX];
    e->resolve(e->context, text, resolved, sizeof(resolved));
    replace_call_placeholders(resolved, arguments, argument_count, out, size);
}

static void
slot_wrapper_name(const ZirModule *module, const ZirFunction *fn, int index,
                   char *out, size_t size)
{
    char prefix[64];
    number_prefix(module, prefix, sizeof(prefix));
    format(out, size, "%s_slot_%ld_%d", prefix, (long)(fn - module->functions), index);
}

/* C has no lexical function values. A file-scope adapter supplies the uniform
 * borrowed-context slot ABI while ordinary Ziran functions keep their own ABI. */
void
EmitSlotWrappers(FILE *out, const ZirModule *module, const ZirFunction *fn,
                    ZirTarget target, ZirResolveTarget resolver, void *context)
{
    if((target != ZIR_C && target != ZIR_CPP) || !CanEmitBody(module, fn))
        return;
    for(int index = 0; index < fn->expr_count; index++) {
        const ZirExpr *value = &fn->exprs[index];
        if(!value->is_function_value)
            continue;
        const ZirType *slot = FindType(module, value->type, NULL);
        if(slot == NULL || slot->is_c_call)
            continue;
        char parameters[64][ZIR_TEXT_MAX];
        int count = *skip_ws(slot->body) ?
            split_top_level(slot->body, parameters[0], 64, sizeof(parameters[0])) : 0;
        char wrapper[ZIR_NAME_MAX], call[ZIR_TEXT_MAX], resolved[ZIR_TEXT_MAX];
        char result_type[ZIR_NAME_MAX], resolved_result[ZIR_NAME_MAX];
        slot_native_type(slot->procedure_return_type, target,
                         result_type, sizeof(result_type));
        resolver(context, result_type, resolved_result,
                 sizeof(resolved_result));
        slot_wrapper_name(module, fn, index, wrapper, sizeof(wrapper));
        fprintf(out, "static %s %s(void *context", resolved_result, wrapper);
        size_t length = (size_t)format(call, sizeof(call), "%s(", value->name);
        for(int argument = 0; argument < count; argument++) {
            const char *source = skip_ws(strchr(parameters[argument], ':') + 1);
            char native_type[ZIR_NAME_MAX], resolved_type[ZIR_NAME_MAX];
            slot_native_type(source, target, native_type,
                             sizeof(native_type));
            resolver(context, native_type, resolved_type,
                     sizeof(resolved_type));
            fprintf(out, ", %s slot_arg_%d", resolved_type, argument);
            length += (size_t)format(call + length, sizeof(call) - length,
                                      "%sslot_arg_%d", argument ? ", " : "", argument);
        }
        format(call + length, sizeof(call) - length, ")");
        resolver(context, call, resolved, sizeof(resolved));
        fprintf(out, ")\n{\n    (void)context;\n    %s%s;\n}\n",
                strcmp(slot->procedure_return_type, "void") ? "return " : "", resolved);
    }
}

void
emit_function_value(Emitter *e, int index, char *out, size_t size)
{
    const ZirExpr *value = &e->fn->exprs[index];
    const ZirType *slot = FindType(e->module, value->type, NULL);
    if(e->target == ZIR_C || e->target == ZIR_CPP) {
        if(slot != NULL && slot->is_c_call) {
            char call[ZIR_TEXT_MAX], resolved[ZIR_TEXT_MAX];
            size_t length;
            format(call, sizeof(call), "%s()", value->name);
            e->resolve(e->context, call, resolved, sizeof(resolved));
            length = strlen(resolved);
            if(length >= 2 && !strcmp(resolved + length - 2, "()"))
                resolved[length - 2] = '\0';
            copy_text(out, size, resolved);
            return;
        }
        char wrapper[ZIR_NAME_MAX];
        slot_wrapper_name(e->module, e->fn, index, wrapper, sizeof(wrapper));
        format(out, size, "(%s){NULL, %s}", value->type, wrapper);
        return;
    }
    char parameters[64][ZIR_TEXT_MAX];
    int count = *skip_ws(slot->body) ?
        split_top_level(slot->body, parameters[0], 64, sizeof(parameters[0])) : 0;
    char arguments[ZIR_TEXT_MAX] = "", signature[ZIR_TEXT_MAX] = "";
    char call[ZIR_TEXT_MAX], resolved[ZIR_TEXT_MAX];
    size_t length = 0, signature_length = 0;
    for(int argument = 0; argument < count; argument++) {
        char parameter[ZIR_NAME_MAX];
        fresh(e, parameter);
        length += (size_t)format(arguments + length, sizeof(arguments) - length,
                                  "%s%s", argument ? ", " : "", parameter);
        if(e->target == ZIR_GO) {
            const char *source = skip_ws(strchr(parameters[argument], ':') + 1);
            const char *scalar = TargetType(source, ZIR_GO);
            char type[ZIR_NAME_MAX];
            if(scalar)
                copy_text(type, sizeof(type), scalar);
            else
                e->resolve(e->context, source, type, sizeof(type));
            signature_length += (size_t)format(signature + signature_length,
                sizeof(signature) - signature_length, "%s%s %s", argument ? ", " : "", parameter, type);
        }
    }
    format(call, sizeof(call), "%s(%s)", value->name, arguments);
    e->resolve(e->context, call, resolved, sizeof(resolved));
    if(e->target == ZIR_GO) {
        char result_type[ZIR_NAME_MAX] = "";
        if(strcmp(slot->procedure_return_type, "void")) {
            const char *scalar = TargetType(slot->procedure_return_type, ZIR_GO);
            if(scalar)
                copy_text(result_type, sizeof(result_type), scalar);
            else
                e->resolve(e->context, slot->procedure_return_type,
                           result_type, sizeof(result_type));
        }
        format(out, size, "func(%s)%s%s { %s%s }", signature,
               result_type[0] ? " " : "", result_type,
               result_type[0] ? "return " : "", resolved);
    }
    else
        format(out, size, "(%s) => %s", arguments, resolved);
}

int
member_path(const ZirFunction *fn, int index)
{
    const ZirExpr *expr = &fn->exprs[index];
    if(expr->kind == ZIR_EXPR_IDENT)
        return 1;
    return (expr->kind == ZIR_EXPR_MEMBER ||
            expr->kind == ZIR_EXPR_POINTER_MEMBER) && member_path(fn, expr->left);
}

/* Checked `print`: evaluate every argument left to right, then write the
 * format's literal pieces and argument values to standard output in order. */
int
expression_calls(const ZirFunction *fn, int index)
{
    if(index < 0 || index >= fn->expr_count)
        return 0;
    const ZirExpr *expr = &fn->exprs[index];
    if(expr->kind == ZIR_EXPR_CALL)
        return 1;
    for(int child = expr->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        if(expression_calls(fn, child))
            return 1;
    return expression_calls(fn, expr->left) || expression_calls(fn, expr->right) ||
           expression_calls(fn, expr->third);
}

static void
print_run_text(PrintRun *run, const unsigned char *bytes, size_t length)
{
    for(size_t i = 0; i < length; i++) {
        if(run->plain_length + 1 < sizeof(run->plain))
            run->plain[run->plain_length++] = bytes[i];
        if(run->format_length + 2 < sizeof(run->format)) {
            run->format[run->format_length++] = bytes[i];
            if(bytes[i] == '%')
                run->format[run->format_length++] = '%';
        }
    }
}

static void
print_run_value(PrintRun *run, const char *verb, const char *argument)
{
    size_t used = strlen(run->arguments);
    for(const char *c = verb; *c && run->format_length + 1 < sizeof(run->format); c++)
        run->format[run->format_length++] = (unsigned char)*c;
    format(run->arguments + used, sizeof(run->arguments) - used, ", %s", argument);
    run->values++;
}

/* Bytes as a Ziran literal, so each target spells it through string_literal. */
static void
ziran_literal_of(const unsigned char *bytes, size_t length, char *out, size_t size)
{
    size_t used = 0;
    out[used++] = '"';
    for(size_t i = 0; i < length && used + 6 < size; i++) {
        unsigned char byte = bytes[i];
        if(byte == '"' || byte == '\\') {
            out[used++] = '\\';
            out[used++] = (char)byte;
        } else if(byte == '\n') {
            out[used++] = '\\';
            out[used++] = 'n';
        } else if(byte == '\t') {
            out[used++] = '\\';
            out[used++] = 't';
        } else if(byte == '\r') {
            out[used++] = '\\';
            out[used++] = 'r';
        } else if(byte < 0x20 || byte == 0x7f)
            used += (size_t)snprintf(out + used, size - used, "\\x%02x", byte);
        else
            out[used++] = (char)byte;
    }
    out[used++] = '"';
    out[used] = '\0';
}

static void
print_run_flush(Emitter *e, PrintRun *run, int format_expr)
{
    char source[ZIR_TEXT_MAX], literal[ZIR_TEXT_MAX];
    ZirExpr piece;
    if(run->format_length == 0) {
        memset(run, 0, sizeof(*run));
        return;
    }
    piece = e->fn->exprs[format_expr];
    /* Go prints text alone with fmt.Print; C always uses printf. */
    if(run->values == 0 && e->target == ZIR_GO)
        ziran_literal_of(run->plain, run->plain_length, source, sizeof(source));
    else
        ziran_literal_of(run->format, run->format_length, source, sizeof(source));
    copy_text(piece.text, sizeof(piece.text), source);
    string_literal(&piece, e->target, literal, sizeof(literal));
    if(e->target == ZIR_GO)
        line(e, run->values ? "fmt.Printf(%s%s)" : "fmt.Print(%s%s)",
             literal, run->arguments);
    else
        line(e, "printf(%s%s);", literal, run->arguments);
    memset(run, 0, sizeof(*run));
}

void
emit_print(Emitter *e, const ZirExpr *expr)
{
    PrintPiece *pieces = calloc(PRINT_PIECES_MAX, sizeof(*pieces));
    char (*values)[ZIR_TEXT_MAX] = calloc(PRINT_PIECES_MAX, sizeof(*values));
    int capture = 0;
    const char *types[PRINT_PIECES_MAX];
    int children[PRINT_PIECES_MAX];
    char value[ZIR_TEXT_MAX], literal[ZIR_TEXT_MAX];
    int first = expr->first_child, count, argument = 0;
    if(pieces == NULL || values == NULL || first < 0 ||
       (count = PrintFormatPieces(e->fn->exprs[first].text, pieces,
                                  PRINT_PIECES_MAX)) < 0)
        fatal(expr, "invalid checked print format");
    /* Without calls no argument has an effect, so they read in place. */
    for(int child = e->fn->exprs[first].next_sibling; child >= 0;
        child = e->fn->exprs[child].next_sibling)
        capture |= expression_calls(e->fn, child);
    for(int child = e->fn->exprs[first].next_sibling; child >= 0;
        child = e->fn->exprs[child].next_sibling, argument++) {
        types[argument] = ScalarType(e->fn->exprs[child].type);
        children[argument] = child;
        emit_expr(e, child, e->fn->exprs[child].type, value, sizeof(value));
        if(capture) {
            fresh(e, values[argument]);
            declare(e, values[argument], types[argument], value);
        } else
            copy_text(values[argument], ZIR_TEXT_MAX, value);
    }
    /* Each language's standard printing: one printf or fmt.Printf per run
     * of text and values. C has no shortest round-trip float format, so a
     * float prints through print_float between printf calls. */
    PrintRun run = {0};
    argument = 0;
    for(int i = 0; i < count; i++) {
        const char *type, *name;
        char operand[ZIR_TEXT_MAX];
        if(!pieces[i].is_argument) {
            unsigned char bytes[ZIR_TEXT_MAX];
            size_t length;
            if(!DecodeStringLiteral(pieces[i].literal, bytes, sizeof(bytes), &length))
                fatal(expr, "invalid checked print format");
            if(e->target != ZIR_GO && memchr(bytes, 0, length) != NULL) {
                /* printf formats end at NUL; such text is written as bytes. */
                ZirExpr piece = e->fn->exprs[first];
                print_run_flush(e, &run, first);
                copy_text(piece.text, sizeof(piece.text), pieces[i].literal);
                string_literal(&piece, e->target, literal, sizeof(literal));
                line(e, "fwrite(%s, 1, %zu, stdout);", literal, length);
                continue;
            }
            print_run_text(&run, bytes, length);
            continue;
        }
        type = types[argument];
        name = values[argument];
        /* A literal string argument is simply part of the text. */
        if(e->fn->exprs[children[argument++]].kind == ZIR_EXPR_STRING) {
            unsigned char bytes[ZIR_TEXT_MAX];
            size_t length;
            if(DecodeStringLiteral(e->fn->exprs[children[argument - 1]].text, bytes,
                                   sizeof(bytes), &length) &&
               (e->target == ZIR_GO || memchr(bytes, 0, length) == NULL)) {
                print_run_text(&run, bytes, length);
                continue;
            }
        }
        {
            /* An argument stands alone in the call; C wraps it once where
             * an operator follows it. */
            char plain[ZIR_TEXT_MAX];
            bare(name, plain, sizeof(plain));
            copy_text(values[argument - 1], ZIR_TEXT_MAX, plain);
            name = values[argument - 1];
        }
        if(plain_identifier(name) || enclosed(name))
            copy_text(operand, sizeof(operand), name);
        else
            format(operand, sizeof(operand), "(%s)", name);
        if(e->target == ZIR_GO) {
            if(!strcmp(type, "string"))
                print_run_value(&run, "%s", name);
            else if(!strcmp(type, "bool"))
                print_run_value(&run, "%t", name);
            else if(!strcmp(type, "float64") || !strcmp(type, "float32")) {
                char text[ZIR_TEXT_MAX];
                format(text, sizeof(text), !strcmp(type, "float64") ?
                       "formatFloat(%s, 64)" : "formatFloat(float64(%s), 32)", name);
                print_run_value(&run, "%s", text);
            } else
                print_run_value(&run, "%d", name);
            continue;
        }
        if(!strcmp(type, "float32") || !strcmp(type, "float64")) {
            print_run_flush(e, &run, first);
            line(e, !strcmp(type, "float32") ? "print_float((double)%s, 1);" :
                 "print_float(%s, 0);", name);
        } else if(!strcmp(type, "string")) {
            char text[ZIR_TEXT_MAX];
            format(text, sizeof(text), "(int)%s.length, %s.data", operand, operand);
            print_run_value(&run, "%.*s", text);
        } else if(!strcmp(type, "bool")) {
            char text[ZIR_TEXT_MAX];
            format(text, sizeof(text), "%s ? \"true\" : \"false\"", operand);
            print_run_value(&run, "%s", text);
        } else {
            /* A literal is written as a long long constant; other values
             * convert to the width printf expects. */
            char text[ZIR_TEXT_MAX];
            uint64_t bits;
            int is_unsigned = type[0] == 'u';
            if(integer_literal_bits(name, &bits))
                format(text, sizeof(text), is_unsigned ? "%lluULL" : "%lldLL",
                       is_unsigned ? (unsigned long long)bits : (unsigned long long)(long long)bits);
            else
                format(text, sizeof(text), is_unsigned ? "(unsigned long long)%s" :
                       "(long long)%s", operand);
            print_run_value(&run, is_unsigned ? "%llu" : "%lld", text);
        }
    }
    print_run_flush(e, &run, first);
    free(pieces);
    free(values);
}
