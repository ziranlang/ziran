#include "zir_check_internal.h"

static const char *apply_using_filter(const char *filter, const char *name, char *buffer, size_t size);

void
select_lookup_file(ZirModule *module, ZirSourceSpan span)
{
    copy_text(module->lookup_path, sizeof(module->lookup_path), SpanPath(span));
}

int
in_lookup_file(const ZirModule *module, int is_file_private,
               ZirSourceSpan span)
{
    return !is_file_private || module->lookup_path[0] == '\0' ||
           strcmp(module->lookup_path, SpanPath(span)) == 0;
}

static int
file_private_name(const ZirModule *module, const char *name,
                  const char *path)
{
    /* A declaration visible in this file wins over private declarations
     * with the same spelling in other loaded files. */
    for(int i = 0; i < module->function_count; i++)
        if(strcmp(module->functions[i].name, name) == 0 &&
           (!module->functions[i].is_file_private ||
            strcmp(SpanPath(module->functions[i].span), path) == 0)) return 0;
    for(int i = 0; i < module->global_count; i++)
        if(strcmp(module->globals[i].name, name) == 0 &&
           (!module->globals[i].is_file_private ||
            strcmp(SpanPath(module->globals[i].span), path) == 0)) return 0;
    for(int i = 0; i < module->define_count; i++)
        if(strcmp(module->defines[i].name, name) == 0 &&
           (!module->defines[i].is_file_private ||
            strcmp(SpanPath(module->defines[i].span), path) == 0)) return 0;
    for(int i = 0; i < module->type_count; i++)
        if(strcmp(module->types[i].name, name) == 0 &&
           (!module->types[i].is_file_private ||
            strcmp(SpanPath(module->types[i].span), path) == 0)) return 0;
    for(int i = 0; i < module->import_count; i++)
        if(strcmp(module->imports[i].name, name) == 0 &&
           (!module->imports[i].is_file_private ||
            strcmp(SpanPath(module->imports[i].span), path) == 0)) return 0;
    for(int i = 0; i < module->function_count; i++)
        if(module->functions[i].is_file_private &&
           strcmp(SpanPath(module->functions[i].span), path) != 0 &&
           strcmp(module->functions[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->global_count; i++)
        if(module->globals[i].is_file_private &&
           strcmp(SpanPath(module->globals[i].span), path) != 0 &&
           strcmp(module->globals[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->define_count; i++)
        if(module->defines[i].is_file_private &&
           strcmp(SpanPath(module->defines[i].span), path) != 0 &&
           strcmp(module->defines[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->type_count; i++)
        if(module->types[i].is_file_private &&
           strcmp(SpanPath(module->types[i].span), path) != 0 &&
           strcmp(module->types[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->import_count; i++)
        if(module->imports[i].is_file_private &&
           strcmp(SpanPath(module->imports[i].span), path) != 0 &&
           strcmp(module->imports[i].name, name) == 0)
            return 1;
    return 0;
}
/* Buffers check_file_private_expression keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CheckFilePrivateExpressionBuffers {
    ZirToken previous;
    ZirToken current;
    ZirToken next;
} CheckFilePrivateExpressionBuffers;

int check_file_private_expression(const ZirModule *module, const char *source,
                              ZirSourceSpan span);

static int
check_file_private_expression_with_buffers(const ZirModule *module, const char *source,
                              ZirSourceSpan span, CheckFilePrivateExpressionBuffers *buffers)
{
    ZirLexer lexer;
    memset(&buffers->previous, 0, sizeof(buffers->previous));
    LexerInit(&lexer, source, SpanPath(span));
    buffers->current = LexerNext(&lexer);
    buffers->next = LexerNext(&lexer);
    while(buffers->current.kind != ZIR_TOKEN_EOF) {
        if(buffers->current.kind == ZIR_TOKEN_IDENT &&
           strcmp(buffers->previous.text, ".") != 0 &&
           strcmp(buffers->next.text, ":") != 0 &&
           file_private_name(module, buffers->current.text, SpanPath(span))) {
            Diagnostic(span, "check.file_scope",
                       "file-private declaration is not visible: %s",
                       buffers->current.text);
            return 0;
        }
        buffers->previous = buffers->current;
        buffers->current = buffers->next;
        buffers->next = LexerNext(&lexer);
    }
    return 1;
}

int
check_file_private_expression(const ZirModule *module, const char *source,
                              ZirSourceSpan span)
{
    static _Thread_local CheckFilePrivateExpressionBuffers *spares[16];
    static _Thread_local int spare_count;
    CheckFilePrivateExpressionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = check_file_private_expression_with_buffers(module, source, span, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static int
file_scope_symbol_visible(const ZirModule *module, const char *name)
{
    for(int pass = 0; pass < 2; pass++) {
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            const ZirModule *scope = module;
            if(pass != 0) {
                const ZirImport *import = &module->imports[i];
                if(import->kind != ZIR_IMPORT_OPEN ||
                   !in_lookup_file(module, import->is_file_private,
                                   import->span)) continue;
                scope = import->resolved_module;
            }
            if(scope == NULL) continue;
            for(int g = 0; g < scope->global_count; g++)
                if((pass == 0 || (!scope->globals[g].is_static &&
                                  !scope->globals[g].is_file_private)) &&
                   (pass != 0 || in_lookup_file(module,
                       scope->globals[g].is_file_private,
                       scope->globals[g].span)) &&
                   strcmp(scope->globals[g].name, name) == 0) return 1;
            for(int d = 0; d < scope->define_count; d++)
                if((pass == 0 || scope->defines[d].is_public) &&
                   (pass != 0 || in_lookup_file(module,
                       scope->defines[d].is_file_private,
                       scope->defines[d].span)) &&
                   strcmp(scope->defines[d].name, name) == 0) return 1;
            for(int f = 0; f < scope->function_count; f++)
                if((pass == 0 || scope->functions[f].is_public) &&
                   (pass != 0 || in_lookup_file(module,
                       scope->functions[f].is_file_private,
                       scope->functions[f].span)) &&
                   strcmp(scope->functions[f].name, name) == 0) return 1;
            for(int t = 0; t < scope->type_count; t++)
                if((pass == 0 || scope->types[t].is_public) &&
                   (pass != 0 || in_lookup_file(module,
                       scope->types[t].is_file_private,
                       scope->types[t].span)) &&
                   strcmp(scope->types[t].name, name) == 0) return 1;
        }
    }
    for(int i = 0; i < module->import_count; i++)
        if(module->imports[i].kind == ZIR_IMPORT_MODULE &&
           in_lookup_file(module, module->imports[i].is_file_private,
                          module->imports[i].span) &&
           strcmp(module->imports[i].name, name) == 0) return 1;
    return 0;
}

int
opened_file_enum(ZirModule *module, const char *name,
                 ZirSourceSpan span, int64_t *value)
{
    const ZirType *found = NULL;
    char found_member[ZIR_NAME_MAX] = "";
    for(int i = 0; i < module->using_count; i++) {
        const ZirUsing *using = &module->usings[i];
        if(!in_lookup_file(module, using->is_file_private, using->span))
            continue;
        const ZirType *candidate = FindType(module, using->path, NULL);
        int64_t candidate_value;
        char source_name[ZIR_NAME_MAX];
        const char *member = apply_using_filter(using->filter, name,
                                                source_name,
                                                sizeof(source_name));
        if(candidate == NULL || !candidate->is_enum ||
           member == NULL ||
           !EnumMemberValue(candidate, member, &candidate_value))
            continue;
        if(found != NULL &&
           (found != candidate || strcmp(found_member, member) != 0)) {
            Diagnostic(span, "check.enum_scope",
                       "ambiguous using enum member: %s", name);
            return -1;
        }
        found = candidate;
        copy_text(found_member, sizeof(found_member), member);
        *value = candidate_value;
    }
    return found != NULL;
}
/* Buffers LowerFileScopeUsing keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerFileScopeUsingBuffers {
    char saved_path[ZIR_PATH_MAX];
    char output[ZIR_TEXT_MAX];
    ZirToken previous;
    ZirToken current;
    ZirToken next;
} LowerFileScopeUsingBuffers;

int LowerFileScopeUsing(ZirModule *module, char *source, size_t capacity,
                    ZirSourceSpan span);

static int
LowerFileScopeUsing_with_buffers(ZirModule *module, char *source, size_t capacity,
                    ZirSourceSpan span, LowerFileScopeUsingBuffers *buffers)
{
    if(module->using_count == 0 || !source[0]) return 1;
    ZirLexer lexer;
    memset(&buffers->previous, 0, sizeof(buffers->previous));
    size_t used = 0, copied = 0;
    copy_text(buffers->saved_path, sizeof(buffers->saved_path), module->lookup_path);
    select_lookup_file(module, span);
    LexerInit(&lexer, source, SpanPath(span));
    buffers->current = LexerNext(&lexer);
    size_t end = lexer.pos;
    while(buffers->current.kind != ZIR_TOKEN_EOF) {
        buffers->next = LexerNext(&lexer);
        size_t start = end - strlen(buffers->current.text);
        int64_t value = 0;
        int opened = 0;
        if(buffers->current.kind == ZIR_TOKEN_IDENT && !buffers->current.truncated &&
           strcmp(buffers->previous.text, ".") != 0 &&
           strcmp(buffers->next.text, "=") != 0 &&
           strcmp(buffers->next.text, ":") != 0 &&
           !file_scope_symbol_visible(module, buffers->current.text))
            opened = opened_file_enum(module, buffers->current.text, span, &value);
        if(opened < 0) {
            copy_text(module->lookup_path, sizeof(module->lookup_path),
                      buffers->saved_path);
            return 0;
        }
        if(opened > 0) {
            char number[64];
            int length = snprintf(number, sizeof(number), "%lld",
                                  (long long)value);
            if(length < 0 || used + start - copied + (size_t)length >=
                             sizeof(buffers->output)) goto failed;
            memcpy(buffers->output + used, source + copied, start - copied);
            used += start - copied;
            memcpy(buffers->output + used, number, (size_t)length);
            used += (size_t)length;
            copied = end;
        }
        buffers->previous = buffers->current;
        buffers->current = buffers->next;
        end = lexer.pos;
    }
    if(used + strlen(source + copied) >= sizeof(buffers->output) ||
       used + strlen(source + copied) >= capacity) goto failed;
    copy_text(buffers->output + used, sizeof(buffers->output) - used, source + copied);
    copy_text(source, capacity, buffers->output);
    copy_text(module->lookup_path, sizeof(module->lookup_path), buffers->saved_path);
    return 1;
failed:
    copy_text(module->lookup_path, sizeof(module->lookup_path), buffers->saved_path);
    Diagnostic(span, "check.enum_scope",
               "cannot lower file-scope using expression");
    return 0;
}

int
LowerFileScopeUsing(ZirModule *module, char *source, size_t capacity,
                    ZirSourceSpan span)
{
    static _Thread_local LowerFileScopeUsingBuffers *spares[16];
    static _Thread_local int spare_count;
    LowerFileScopeUsingBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = LowerFileScopeUsing_with_buffers(module, source, capacity, span, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

int
check_file_scope_enum_names(const ZirModule *module,
                            const ZirFunction *expression,
                            ZirSourceSpan span)
{
    for(int e = 0; e < expression->expr_count; e++) {
        const ZirExpr *node = &expression->exprs[e];
        if(node->kind != ZIR_EXPR_IDENT || !node->name[0] ||
           node->name[0] == '.' || strchr(node->name, '.') != NULL ||
           file_scope_symbol_visible(module, node->name)) continue;
        for(int pass = 0; pass < 2; pass++) {
            int count = pass == 0 ? 1 : module->import_count;
            for(int i = 0; i < count; i++) {
                const ZirModule *scope = module;
                if(pass != 0) {
                    const ZirImport *import = &module->imports[i];
                    if(import->kind != ZIR_IMPORT_OPEN ||
                       !in_lookup_file(module, import->is_file_private,
                                       import->span)) continue;
                    scope = import->resolved_module;
                }
                if(scope == NULL) continue;
                for(int t = 0; t < scope->type_count; t++) {
                    const ZirType *type = &scope->types[t];
                    int64_t value;
                    if((pass != 0 && !type->is_public) ||
                       (pass == 0 && !in_lookup_file(module,
                            type->is_file_private, type->span)) ||
                       !type->is_enum ||
                       !EnumMemberValue(type, node->name, &value)) continue;
                    Diagnostic(span, "check.enum_scope",
                               "enum member needs a type qualifier or using: %s",
                               node->name);
                    return 0;
                }
            }
        }
    }
    return 1;
}

const char *
ScalarType(const char *type)
{
    static const struct { const char *source, *type; } types[] = {
        {"bool", "bool"}, {"void", "void"}, {"s8", "s8"}, {"u8", "u8"},
        {"s16", "s16"}, {"u16", "u16"}, {"s32", "s32"}, {"u32", "u32"},
        {"s64", "s64"}, {"u64", "u64"}, {"isize", "isize"}, {"usize", "usize"},
        {"float32", "float32"}, {"float64", "float64"}, {"string", "string"}, {NULL, NULL}
    };
    for(int i = 0; types[i].source; i++)
        if(!strcmp(type, types[i].source)) return types[i].type;
    return "";
}

int
contains_vec(const ZirModule *module, const char *type, int depth)
{
    if(depth > 32 || type == NULL || !*type || *type == '*')
        return 0;
    if(VecElementType(module, type, NULL, 0))
        return 1;
    char element[ZIR_NAME_MAX];
    if(ArrayElementType(type, element, sizeof(element), NULL))
        return contains_vec(module, element, depth + 1);
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template || record->is_extern)
        return 0;
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1)
        if(contains_vec(owner ? owner : module, field.type, depth + 1))
            return 1;
    return 0;
}

static int
align_size(size_t value, size_t alignment, size_t *rounded)
{
    if(alignment == 0 || value > SIZE_MAX - (alignment - 1))
        return 0;
    *rounded = ((value + alignment - 1) / alignment) * alignment;
    return 1;
}
/* Buffers layout_type keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LayoutTypeBuffers {
    ZirType instance;
    ZirFunction probe;
} LayoutTypeBuffers;

int layout_type(const ZirModule *module, const char *source, int depth,
            size_t *size, size_t *alignment);

static int
layout_type_with_buffers(const ZirModule *module, const char *source, int depth,
            size_t *size, size_t *alignment, LayoutTypeBuffers *buffers)
{
    char type[ZIR_NAME_MAX], element[ZIR_NAME_MAX];
    const char *scalar;
    const ZirModule *owner = NULL;
    const ZirType *record;
    memset(&buffers->instance, 0, sizeof(buffers->instance));
    int capacity = 0;
    if(depth > 32 || strlen(source) >= sizeof(type)) return 0;
    copy_text(type, sizeof(type), source);
    trim_in_place(type);
    if(type[0] == '(') {
        int nesting = 0, wrapped = 1;
        size_t length = strlen(type);
        for(size_t i = 0; i < length; i++) {
            if(type[i] == '(') nesting++;
            else if(type[i] == ')' && --nesting < 0) wrapped = 0;
            if(nesting == 0 && i + 1 < length) wrapped = 0;
        }
        if(wrapped && nesting == 0 && length > 2 &&
           type[length - 1] == ')') {
            type[length - 1] = '\0';
            return layout_type(module, type + 1, depth + 1,
                               size, alignment);
        }
    }
    scalar = ScalarType(type);
    if(!strcmp(scalar, "void")) { *size = 0; *alignment = 1; return 1; }
    if(!strcmp(scalar, "bool") || !strcmp(scalar, "s8") ||
       !strcmp(scalar, "u8")) {
        *size = *alignment = 1; return 1;
    }
    if(!strcmp(scalar, "s16") || !strcmp(scalar, "u16")) {
        *size = *alignment = 2; return 1;
    }
    if(!strcmp(scalar, "s32") || !strcmp(scalar, "u32") ||
       !strcmp(scalar, "float32")) {
        *size = *alignment = 4; return 1;
    }
    if(!strcmp(scalar, "s64") || !strcmp(scalar, "u64") ||
       !strcmp(scalar, "float64")) {
        *size = *alignment = 8; return 1;
    }
    if(!strcmp(scalar, "string")) {
        *size = 16;
        *alignment = 8;
        return 1;
    }
    if(SliceElementType(type, element, sizeof(element))) {
        if(local_storage_error(module, type) != NULL)
            return 0;
        *size = 16;
        *alignment = 8;
        return 1;
    }
    if(!strcmp(scalar, "isize") || !strcmp(scalar, "usize") ||
       type[0] == '*') {
        *size = *alignment = 8; return 1;
    }
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        size_t item_size, item_alignment;
        if(capacity < 0) {
            const char *close = strchr(type, ']');
            char bound[ZIR_NAME_MAX];
            memset(&buffers->probe, 0, sizeof(buffers->probe));
            int64_t resolved = -1;
            int root, status;
            size_t length = close ? (size_t)(close - type - 1) : 0;
            if(length == 0 || length >= sizeof(bound)) return 0;
            memcpy(bound, type + 1, length);
            bound[length] = '\0';
            trim_in_place(bound);
            root = ParseExpr(&buffers->probe, module, bound, Span("", 0, 0));
            status = bound_expression(module, &buffers->probe, root,
                                      depth + 1, &resolved);
            free(buffers->probe.exprs);
            if(status != 1 || resolved < 0 || resolved > INT32_MAX)
                return 0;
            capacity = (int)resolved;
        }
        if(!layout_type(module, element, depth + 1,
                        &item_size, &item_alignment) ||
           (item_size && (size_t)capacity > SIZE_MAX / item_size))
            return 0;
        *size = (size_t)capacity * item_size;
        *alignment = item_alignment;
        return 1;
    }
    record = FindType(module, type, &owner);
    if(record == NULL) {
        const char *opening = strchr(type, '(');
        const char *closing = strrchr(type, ')');
        if(opening != NULL && closing != NULL && closing[1] == '\0' &&
           opening < closing) {
            char base[ZIR_NAME_MAX];
            size_t length = (size_t)(opening - type);
            int nesting = 0, balanced = 1;
            for(const char *cursor = opening; cursor <= closing; cursor++) {
                if(*cursor == '(') nesting++;
                else if(*cursor == ')' && --nesting < 0) balanced = 0;
                if(nesting == 0 && cursor != closing) balanced = 0;
            }
            if(length > 0 && length < sizeof(base) && balanced &&
               nesting == 0 &&
               (size_t)(closing - opening - 1) <
                   sizeof(buffers->instance.template_args)) {
                memcpy(base, type, length);
                base[length] = '\0';
                trim_in_place(base);
                const ZirType *generic = FindType(module, base, &owner);
                if(generic != NULL && generic->is_record_template) {
                    copy_text(buffers->instance.name, sizeof(buffers->instance.name), type);
                    copy_text(buffers->instance.template_name,
                              sizeof(buffers->instance.template_name), base);
                    memcpy(buffers->instance.template_args, opening + 1,
                           (size_t)(closing - opening - 1));
                    buffers->instance.template_args[closing - opening - 1] = '\0';
                    buffers->instance.is_type_instance = 1;
                    if(!InstantiateGenericRecord(&buffers->instance, generic)) return 0;
                    record = &buffers->instance;
                }
            }
        }
    } else if(record->is_type_instance) {
        const ZirType *generic = FindType(owner ? owner : module,
                                          record->template_name, NULL);
        if(generic == NULL || !generic->is_record_template) return 0;
        buffers->instance = *record;
        buffers->instance.body[0] = '\0';
        if(!InstantiateGenericRecord(&buffers->instance, generic)) return 0;
        record = &buffers->instance;
    }
    if(record == NULL) {
        const ZirDefine *definition = NULL;
        const ZirModule *definition_owner = NULL;
        if(visible_define(module, type, &definition,
                          &definition_owner, NULL) == 1)
            return layout_type(definition_owner, definition->value,
                               depth + 1, size, alignment);
    }
    if(record == NULL || record->is_procedure_type || record->is_extern ||
       record->is_record_template || record->is_type_instance)
        return 0;
    if(record->is_enum)
        return layout_type(owner ? owner : module, record->enum_backing,
                           depth + 1, size, alignment);
    size_t offset = 0, maximum_alignment = 1, field_offset = 0;
    ZirTypeField field;
    int result, field_count = 0;
    while((result = TypeNextField(record, &field_offset, &field)) == 1) {
        size_t field_size, field_alignment;
        if(!layout_type(owner ? owner : module, field.type, depth + 1,
                        &field_size, &field_alignment) ||
           (!record->is_union &&
            (!align_size(offset, field_alignment, &offset) ||
             offset > SIZE_MAX - field_size)))
            return 0;
        if(record->is_union) {
            if(field_size > offset) offset = field_size;
        } else offset += field_size;
        field_count++;
        if(field_alignment > maximum_alignment)
            maximum_alignment = field_alignment;
    }
    if(result < 0 || field_count == 0 ||
       !align_size(offset, maximum_alignment, size))
        return 0;
    *alignment = maximum_alignment;
    return 1;
}

int
layout_type(const ZirModule *module, const char *source, int depth,
            size_t *size, size_t *alignment)
{
    static _Thread_local LayoutTypeBuffers *spares[16];
    static _Thread_local int spare_count;
    LayoutTypeBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = layout_type_with_buffers(module, source, depth, size, alignment, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

int
TypeLayout(const ZirModule *module, const char *type,
           size_t *size, size_t *alignment)
{
    return layout_type(module, type, 0, size, alignment);
}

void
error(Checker *c, ZirSourceSpan span, const char *message, const char *detail)
{
    c->errors++;
    Diagnostic(span, "check.type", "%s%s%s",
            message, detail && *detail ? ": " : "", detail ? detail : "");
}

void
signature_error(Checker *c, ZirSourceSpan span,
                const char *message, const char *name)
{
    error(c, span, message, name);
}

/* Keep children in source order. Their checked indices select the callee
 * parameter while native emission and the VM evaluate them in that order. */
int
bind_varargs_call(Checker *c, ZirExpr *call,
                  char parts[][ZIR_TEXT_MAX], int fixed,
                  const char *display_name)
{
    unsigned char used[64] = {0};
    int next_extra = fixed;
    for(int child = call->first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        ZirExpr *argument = &c->fn->exprs[child];
        int index = -1;
        if(argument->argument_name[0]) {
            size_t wanted = strlen(argument->argument_name);
            for(int i = 0; i < fixed; i++) {
                const char *start = skip_ws(parts[i]);
                const char *colon = strchr(start, ':');
                if(colon == NULL) continue;
                const char *end = colon;
                while(end > start && isspace((unsigned char)end[-1])) end--;
                if((size_t)(end - start) == wanted &&
                   !strncmp(start, argument->argument_name, wanted)) {
                    index = i;
                    break;
                }
            }
            if(index < 0) {
                signature_error(c, argument->span,
                                "unknown named argument", argument->argument_name);
                return 0;
            }
        } else {
            for(int i = 0; i < fixed; i++)
                if(!used[i]) { index = i; break; }
            if(index < 0) {
                if(next_extra >= 64) {
                    signature_error(c, argument->span,
                                    "argument count mismatch", display_name);
                    return 0;
                }
                index = next_extra++;
            }
        }
        if(used[index]) {
            signature_error(c, argument->span,
                            "duplicate call argument", argument->argument_name);
            return 0;
        }
        used[index] = 1;
        argument->argument_index = index;
    }
    return 1;
}

int
bind_call_arguments(Checker *c, ZirExpr *call,
                    char parts[][ZIR_TEXT_MAX], int expected,
                    const char *display_name, const char *default_args)
{
    unsigned char used[64] = {0};
    int actual = 0;
    if(expected < 0 || expected > 64)
        return 0;
    for(int child = call->first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        ZirExpr *argument = &c->fn->exprs[child];
        int index = -1;
        if(argument->argument_name[0]) {
            size_t wanted = strlen(argument->argument_name);
            for(int i = 0; i < expected; i++) {
                const char *start = skip_ws(parts[i]);
                const char *colon = strchr(start, ':');
                if(colon == NULL) continue;
                const char *end = colon;
                while(end > start && isspace((unsigned char)end[-1])) end--;
                if((size_t)(end - start) == wanted &&
                   !strncmp(start, argument->argument_name, wanted)) {
                    index = i;
                    break;
                }
            }
            if(index < 0) {
                signature_error(c, argument->span,
                                "unknown named argument", argument->argument_name);
                return 0;
            }
        } else {
            for(int i = 0; i < expected; i++)
                if(!used[i]) { index = i; break; }
            if(index < 0) {
                signature_error(c, argument->span,
                                "argument count mismatch", display_name);
                return 0;
            }
        }
        if(used[index]) {
            signature_error(c, argument->span,
                            "duplicate call argument", argument->argument_name);
            return 0;
        }
        used[index] = 1;
        argument->argument_index = index;
        actual++;
    }
    if(actual != expected) {
        int complete = 0;
        if(c->inference_only && default_args != NULL && *default_args) {
            char (*defaults)[ZIR_TEXT_MAX] = calloc(64, sizeof(*defaults));
            if(defaults == NULL) { c->failed = 1; return 0; }
            complete = split_top_level(default_args, defaults[0], 64,
                                       sizeof(defaults[0])) == expected;
            for(int i = 0; complete && i < expected; i++)
                if(!used[i] && top_level_assignment(defaults[i]) == NULL)
                    complete = 0;
            free(defaults);
        }
        if(!complete) {
            signature_error(c, call->span, "argument count mismatch", display_name);
            return 0;
        }
    }
    return 1;
}

const char *
import_type_alias(const ZirModule *module, const ZirModule *owner)
{
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *import = &module->imports[i];
        if(import->kind == ZIR_IMPORT_MODULE &&
           in_lookup_file(module, import->is_file_private, import->span) &&
           import->resolved_module == owner)
            return import->name;
    }
    return NULL;
}

int
qualified_global_type(const ZirModule *module, const char *name,
                      const ZirModule *owner, const char *type,
                      char *output, size_t capacity)
{
    const char *base = type;
    const char *pointer = "";
    const char *dot = strchr(name, '.');
    const char *alias = NULL;
    const ZirModule *type_owner = NULL;
    const ZirType *declared;
    int alias_length;
    if(owner == NULL || owner == module)
        return 0;
    if(*base == '*') {
        pointer = "*";
        base = skip_ws(base + 1);
    }
    if(BuiltinType(base) != NULL)
        return 0;
    declared = FindType(owner, base, &type_owner);
    if(declared == NULL || FindType(module, base, NULL) == declared)
        return 0;
    if(type_owner == owner && dot != NULL) {
        alias = name;
        alias_length = (int)(dot - name);
    } else {
        alias = import_type_alias(module, type_owner);
        if(alias == NULL) return 0;
        alias_length = (int)strlen(alias);
    }
    int length = snprintf(output, capacity, "%s%.*s.%s", pointer,
                          alias_length, alias, base);
    return length >= 0 && (size_t)length < capacity ? 1 : -1;
}

void
bind(Checker *c, const char *name, const char *type, ZirSourceSpan span)
{
    if(!*name) return;
    for(int i = 0; i < c->module->import_count; i++)
        if(c->module->imports[i].kind == ZIR_IMPORT_MODULE &&
           in_lookup_file(c->module,
                          c->module->imports[i].is_file_private,
                          c->module->imports[i].span) &&
           strcmp(c->module->imports[i].name, name) == 0) {
            error(c, span, "binding shadows an imported module", name);
            return;
        }
    for(int pass = 0; pass < 2; pass++) {
        int count = pass == 0 ? 1 : c->module->import_count;
        for(int i = 0; i < count; i++) {
            const ZirModule *scope = pass == 0 ? c->module :
                                     c->module->imports[i].resolved_module;
            if(pass != 0 && c->module->imports[i].kind != ZIR_IMPORT_OPEN)
                continue;
            if(scope == NULL)
                continue;
            for(int d = 0; d < scope->define_count; d++)
                if((pass == 0 || scope->defines[d].is_public) &&
                   (pass != 0 || in_lookup_file(c->module,
                       scope->defines[d].is_file_private,
                       scope->defines[d].span)) &&
                   !strcmp(scope->defines[d].name, name)) {
                    error(c, span, "binding shadows a compile-time definition", name);
                    return;
                }
        }
    }
    for(int i = c->count - 1; i >= 0 && c->bindings[i].depth == c->depth; i--)
        if(!c->bindings[i].is_using_namespace &&
           !strcmp(c->bindings[i].name, name)) {
            error(c, span, "duplicate binding", name);
            return;
        }
    if(c->count == c->capacity) {
        int size = c->capacity ? c->capacity * 2 : 32;
        Binding *next = realloc(c->bindings, (size_t)size * sizeof(*next));
        if(!next) { c->errors++; c->failed=1; return; }
        c->bindings = next; c->capacity = size;
    }
    copy_text(c->bindings[c->count].name, ZIR_NAME_MAX, name);
    copy_text(c->bindings[c->count].type, ZIR_NAME_MAX, type);
    c->bindings[c->count].using_path[0] = '\0';
    c->bindings[c->count].depth = c->depth;
    c->bindings[c->count].is_using_namespace = 0;
    c->bindings[c->count].is_enum_namespace = 0;
    c->bindings[c->count].root_index = -1;
    c->bindings[c->count].moved = 0;
    c->bindings[c->count].moved_path_count = 0;
    c->bindings[c->count].touched = 0;
    c->bindings[c->count].borrow_count = 0;
    c->bindings[c->count].borrows_index = -1;
    c->bindings[c->count++].using_filter[0] = '\0';
}

/* Map a promoted name back to its source field name under an only/except/map
 * filter. Returns NULL when the filter hides the name. */
static const char *
apply_using_filter(const char *filter, const char *name,
                   char *buffer, size_t size)
{
    const char *list;
    if(filter[0] == '\0')
        return name;
    if(filter[1] != ':')
        return name;
    list = filter + 2;
    while(*list != '\0') {
        const char *comma = strchr(list, ',');
        size_t length = comma == NULL ? strlen(list) :
                        (size_t)(comma - list);
        const char *equals = memchr(list, '=', length);
        size_t new_length = equals == NULL ? length :
                            (size_t)(equals - list);
        if(new_length == strlen(name) &&
           strncmp(list, name, new_length) == 0) {
            if(filter[0] == 'E')
                return NULL;
            if(filter[0] == 'M' && equals != NULL) {
                size_t old_length = length - new_length - 1;
                if(old_length == 0 || old_length >= size)
                    return NULL;
                memcpy(buffer, equals + 1, old_length);
                buffer[old_length] = '\0';
                return buffer;
            }
            return name;
        }
        list = comma == NULL ? list + length : comma + 1;
    }
    return filter[0] == 'E' ? name : NULL;
}

void
activate_using_filtered(Checker *c, const char *path,
                        const char *filter, ZirSourceSpan span)
{
    const char *dot = strchr(path, '.');
    size_t root_length = dot == NULL ? strlen(path) : (size_t)(dot - path);
    if(root_length == 0 || root_length >= ZIR_NAME_MAX) {
        error(c, span, "using needs a record binding", path);
        return;
    }
    char root[ZIR_NAME_MAX];
    char qualified_type[ZIR_NAME_MAX] = "";
    memcpy(root, path, root_length);
    root[root_length] = '\0';
    int root_index = -1;
    const char *binding_type = NULL;
    for(int i = c->count - 1; i >= 0; i--)
        if(!c->bindings[i].is_using_namespace &&
           strcmp(c->bindings[i].name, root) == 0) {
            root_index = i;
            binding_type = c->bindings[i].type;
            break;
        }
    if(binding_type == NULL) {
        const ZirGlobal *global = global_binding(c, root);
        if(global != NULL) {
            const ZirModule *owner = NULL;
            const ZirGlobal *resolved = NULL;
            ResolveGlobal(c->module, root, &owner, &resolved);
            binding_type = global->type;
            int qualified = qualified_global_type(c->module, root, owner,
                binding_type, qualified_type, sizeof(qualified_type));
            if(qualified < 0) {
                error(c, span, "qualified using type is too long", path);
                return;
            }
            if(qualified > 0) binding_type = qualified_type;
        }
    }
    if(binding_type == NULL && dot != NULL) {
        const char *after_global = strchr(dot + 1, '.');
        size_t qualified_length = after_global == NULL ? strlen(path) :
                                  (size_t)(after_global - path);
        if(qualified_length < sizeof(root)) {
            char qualified[ZIR_NAME_MAX];
            const ZirModule *owner = NULL;
            const ZirGlobal *global = NULL;
            memcpy(qualified, path, qualified_length);
            qualified[qualified_length] = '\0';
            if(global_binding(c, qualified) != NULL &&
               ResolveGlobal(c->module, qualified, &owner, &global) == 1) {
                copy_text(root, sizeof(root), qualified);
                binding_type = global->type;
                int qualified_type_status = qualified_global_type(c->module,
                    qualified, owner, binding_type, qualified_type,
                    sizeof(qualified_type));
                if(qualified_type_status < 0) {
                    error(c, span, "qualified using type is too long", path);
                    return;
                }
                if(qualified_type_status > 0) binding_type = qualified_type;
                dot = after_global;
            }
        }
    }
    if(binding_type == NULL) {
        const ZirType *enumeration = FindType(c->module, path, NULL);
        if(enumeration != NULL && enumeration->is_enum) {
            if(c->count == c->capacity) {
                int size = c->capacity ? c->capacity * 2 : 32;
                Binding *next = realloc(c->bindings,
                    (size_t)size * sizeof(*next));
                if(!next) { c->errors++; c->failed = 1; return; }
                c->bindings = next; c->capacity = size;
            }
            Binding *namespace = &c->bindings[c->count++];
            copy_text(namespace->name, sizeof(namespace->name), path);
            copy_text(namespace->type, sizeof(namespace->type), path);
            copy_text(namespace->using_filter,
                      sizeof(namespace->using_filter), filter);
            namespace->using_path[0] = '\0';
            namespace->depth = c->depth;
            namespace->is_using_namespace = 1;
            namespace->is_enum_namespace = 1;
            namespace->moved = 0;
            namespace->moved_path_count = 0;
            namespace->touched = 0;
            namespace->borrow_count = 0;
            namespace->borrows_index = -1;
            namespace->root_index = -1;
            return;
        }
    }
    if(binding_type == NULL) {
        error(c, span, "using requires a local, parameter, or global binding", root);
        return;
    }
    char current_type[ZIR_NAME_MAX], using_path[ZIR_NAME_MAX] = "";
    copy_text(current_type, sizeof(current_type), binding_type);
    while(dot != NULL) {
        const char *segment = dot + 1;
        dot = strchr(segment, '.');
        size_t length = dot == NULL ? strlen(segment) : (size_t)(dot - segment);
        if(length == 0 || length >= ZIR_NAME_MAX) {
            error(c, span, "invalid using field path", path);
            return;
        }
        char field_name[ZIR_NAME_MAX], field_path[ZIR_NAME_MAX];
        char field_type[ZIR_NAME_MAX];
        memcpy(field_name, segment, length);
        field_name[length] = '\0';
        const char *base = skip_ws(current_type);
        if(*base == '*') base = skip_ws(base + 1);
        const ZirModule *owner = NULL;
        const ZirType *record = FindType(c->module, base, &owner);
        if(record == NULL || record->is_enum || record->is_procedure_type ||
           record->is_record_template) {
            error(c, span, "using field requires a concrete record", path);
            return;
        }
        int found = ResolveRecordField(owner, record, field_name,
                                       field_path, sizeof(field_path),
                                       field_type, sizeof(field_type));
        if(found <= 0) {
            error(c, span, found < 0 ? "ambiguous using field path" :
                  "unknown using field", field_name);
            return;
        }
        size_t used = strlen(using_path), added = strlen(field_path);
        if(used + (used != 0) + added >= sizeof(using_path)) {
            error(c, span, "using field path is too long", path);
            return;
        }
        if(used != 0) strcat(using_path, ".");
        strcat(using_path, field_path);
        copy_text(current_type, sizeof(current_type), field_type);
    }
    const char *base = skip_ws(current_type);
    if(*base == '*') base = skip_ws(base + 1);
    const ZirType *record = FindType(c->module, base, NULL);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template) {
        error(c, span, "using requires a concrete record binding", path);
        return;
    }
    if(c->count == c->capacity) {
        int size = c->capacity ? c->capacity * 2 : 32;
        Binding *next = realloc(c->bindings, (size_t)size * sizeof(*next));
        if(!next) { c->errors++; c->failed = 1; return; }
        c->bindings = next; c->capacity = size;
    }
    Binding *namespace = &c->bindings[c->count++];
    copy_text(namespace->name, sizeof(namespace->name), root);
    copy_text(namespace->type, sizeof(namespace->type), current_type);
    copy_text(namespace->using_path, sizeof(namespace->using_path), using_path);
    copy_text(namespace->using_filter,
              sizeof(namespace->using_filter), filter);
    namespace->depth = c->depth;
    namespace->is_using_namespace = 1;
    namespace->is_enum_namespace = 0;
    namespace->moved = 0;
    namespace->moved_path_count = 0;
    namespace->touched = 0;
    namespace->borrow_count = 0;
    namespace->borrows_index = -1;
    namespace->root_index = root_index;
}

void
activate_using(Checker *c, const char *path, ZirSourceSpan span)
{
    activate_using_filtered(c, path, "", span);
}

static int
opened_file_record(Checker *scope, const char *name, ZirSourceSpan span,
                   char *path, size_t capacity)
{
    path[0] = '\0';
    for(int i = scope->count - 1; i >= 0; i--) {
        const Binding *binding = &scope->bindings[i];
        if(!binding->is_using_namespace || binding->is_enum_namespace)
            continue;
        const char *type = skip_ws(binding->type);
        if(*type == '*') type = skip_ws(type + 1);
        const ZirModule *owner = NULL;
        const ZirType *record = FindType(scope->module, type, &owner);
        if(record == NULL) continue;
        char source_name[ZIR_NAME_MAX];
        const char *promoted = apply_using_filter(binding->using_filter,
                                                  name, source_name,
                                                  sizeof(source_name));
        if(promoted == NULL) continue;
        char field_path[ZIR_NAME_MAX], field_type[ZIR_NAME_MAX];
        int found = ResolveRecordField(owner, record, promoted,
                                       field_path, sizeof(field_path),
                                       field_type, sizeof(field_type));
        if(found == 0) continue;
        char candidate[ZIR_TEXT_MAX];
        int length = snprintf(candidate, sizeof(candidate), "%s%s%s.%s",
                              binding->name,
                              binding->using_path[0] ? "." : "",
                              binding->using_path, field_path);
        if(found < 0 || length < 0 ||
           (size_t)length >= sizeof(candidate) ||
           (path[0] && strcmp(path, candidate))) {
            Diagnostic(span, "check.using_scope",
                       "ambiguous using field: %s", name);
            return -1;
        }
        copy_text(path, capacity, candidate);
    }
    return path[0] != '\0';
}
/* Buffers lower_file_record_using keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerFileRecordUsingBuffers {
    char saved_path[ZIR_PATH_MAX];
    Checker scope;
    char output[ZIR_TEXT_MAX];
    ZirToken previous;
    ZirToken current;
    ZirToken next;
    char path[ZIR_TEXT_MAX];
} LowerFileRecordUsingBuffers;

int lower_file_record_using(ZirModule *module, char *source, size_t capacity,
                        ZirSourceSpan span);

static int
lower_file_record_using_with_buffers(ZirModule *module, char *source, size_t capacity,
                        ZirSourceSpan span, LowerFileRecordUsingBuffers *buffers)
{
    if(module->using_count == 0 || !source[0]) return 1;
    copy_text(buffers->saved_path, sizeof(buffers->saved_path), module->lookup_path);
    select_lookup_file(module, span);
    memset(&buffers->scope, 0, sizeof(buffers->scope)); buffers->scope.module = module;
    for(int i = 0; i < module->using_count; i++) {
        const ZirUsing *using = &module->usings[i];
        if(!in_lookup_file(module, using->is_file_private, using->span))
            continue;
        const ZirType *type = FindType(module, using->path, NULL);
        if(type != NULL && type->is_enum) continue;
        activate_using_filtered(&buffers->scope, using->path,
                                using->filter, using->span);
        if(buffers->scope.failed || buffers->scope.errors) goto failed;
    }
    ZirLexer lexer;
    memset(&buffers->previous, 0, sizeof(buffers->previous));
    size_t used = 0, copied = 0;
    LexerInit(&lexer, source, SpanPath(span));
    buffers->current = LexerNext(&lexer);
    size_t end = lexer.pos;
    while(buffers->current.kind != ZIR_TOKEN_EOF) {
        buffers->next = LexerNext(&lexer);
        size_t start = end - strlen(buffers->current.text);
        buffers->path[0] = '\0';
        int opened = 0;
        if(buffers->current.kind == ZIR_TOKEN_IDENT && !buffers->current.truncated &&
           strcmp(buffers->previous.text, ".") != 0 &&
           strcmp(buffers->next.text, "=") != 0 &&
           strcmp(buffers->next.text, ":") != 0 &&
           !file_scope_symbol_visible(module, buffers->current.text))
            opened = opened_file_record(&buffers->scope, buffers->current.text, span,
                                        buffers->path, sizeof(buffers->path));
        if(opened > 0) {
            int64_t enum_value = 0;
            int enumeration = opened_file_enum(module, buffers->current.text,
                                               span, &enum_value);
            if(enumeration > 0) {
                Diagnostic(span, "check.using_scope",
                           "ambiguous using field: %s", buffers->current.text);
                opened = -1;
            } else if(enumeration < 0)
                opened = -1;
        }
        if(opened < 0) goto failed;
        if(opened > 0) {
            size_t length = strlen(buffers->path);
            if(used + start - copied + length >= sizeof(buffers->output)) {
                Diagnostic(span, "check.using_scope",
                           "file-scope using expression is too long");
                goto failed;
            }
            memcpy(buffers->output + used, source + copied, start - copied);
            used += start - copied;
            memcpy(buffers->output + used, buffers->path, length);
            used += length;
            copied = end;
        }
        buffers->previous = buffers->current;
        buffers->current = buffers->next;
        end = lexer.pos;
    }
    if(used + strlen(source + copied) >= sizeof(buffers->output) ||
       used + strlen(source + copied) >= capacity) {
        Diagnostic(span, "check.using_scope",
                   "file-scope using expression is too long");
        goto failed;
    }
    copy_text(buffers->output + used, sizeof(buffers->output) - used, source + copied);
    copy_text(source, capacity, buffers->output);
    free(buffers->scope.bindings);
    copy_text(module->lookup_path, sizeof(module->lookup_path), buffers->saved_path);
    return 1;
failed:
    free(buffers->scope.bindings);
    copy_text(module->lookup_path, sizeof(module->lookup_path), buffers->saved_path);
    return 0;
}

int
lower_file_record_using(ZirModule *module, char *source, size_t capacity,
                        ZirSourceSpan span)
{
    static _Thread_local LowerFileRecordUsingBuffers *spares[16];
    static _Thread_local int spare_count;
    LowerFileRecordUsingBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = lower_file_record_using_with_buffers(module, source, capacity, span, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

/* Earlier initialized globals have known startup values. Expose those values
 * only while folding another global initializer; they are still mutable at
 * runtime and are not compile-time names in #if or #assert. */
int
evaluate_global_startup_literal(const ZirModule *module, int before,
                                const char *source, ZirSourceSpan span,
                                char *literal, size_t literal_size,
                                const ZirModule **type_owner)
{
    ZirDefine *values = calloc((size_t)module->define_count + before + 1,
                               sizeof(*values));
    if(values == NULL) return 0;
    int count = module->define_count;
    if(count > 0)
        memcpy(values, module->defines, (size_t)count * sizeof(*values));
    for(int g = 0; g < before; g++) {
        const ZirGlobal *global = &module->globals[g];
        if(!global->init[0] ||
           !in_lookup_file(module, global->is_file_private,
                           global->span)) continue;
        int collision = 0;
        for(int d = 0; d < count; d++)
            if(strcmp(values[d].name, global->name) == 0) {
                collision = 1;
                break;
            }
        if(collision) continue;
        ZirDefine *value = &values[count++];
        copy_text(value->name, sizeof(value->name), global->name);
        copy_text(value->value, sizeof(value->value), global->init);
        value->is_public = !global->is_static;
        value->is_file_private = global->is_file_private;
        value->span = global->span;
    }
    ZirModule view = *module;
    view.defines = values;
    view.define_count = count;
    const ZirModule *evaluated_owner = NULL;
    int ok = EvaluateCompileLiteral(&view, source, span, 0,
                                    literal, literal_size,
                                    &evaluated_owner);
    if(type_owner != NULL)
        *type_owner = evaluated_owner == &view ? module : evaluated_owner;
    free(values);
    return ok;
}

int
resolve_using_enum(Checker *c, const char *name,
                   const ZirType **enumeration, char *source, size_t size)
{
    *enumeration = NULL;
    char found_member[ZIR_NAME_MAX] = "";
    for(int i = c->count - 1; i >= 0; i--) {
        const Binding *binding = &c->bindings[i];
        char source_name[ZIR_NAME_MAX];
        const char *promoted;
        if(!binding->is_enum_namespace) continue;
        promoted = apply_using_filter(binding->using_filter, name,
                                      source_name, sizeof(source_name));
        if(promoted == NULL) continue;
        const ZirType *candidate = FindType(c->module, binding->type, NULL);
        int64_t value;
        if(candidate == NULL ||
           !EnumMemberValue(candidate, promoted, &value)) continue;
        if(*enumeration != NULL &&
           (*enumeration != candidate ||
            strcmp(found_member, promoted) != 0))
            return -1;
        *enumeration = candidate;
        copy_text(found_member, sizeof(found_member), promoted);
        if(source != NULL && size > 0)
            copy_text(source, size, promoted);
    }
    return *enumeration != NULL;
}

static void
promote_using_member(Checker *c, int index)
{
    ZirExpr *member = &c->fn->exprs[index];
    if((c->fn->from_ir && !c->fn->is_specialization) ||
       member->kind != ZIR_EXPR_IDENT ||
       member->is_this || !member->name[0] ||
       *lookup_lexical(c, member->name) ||
       global_binding(c, member->name) != NULL)
        return;
    for(int i = 0; i < c->module->import_count; i++) {
        const ZirImport *import = &c->module->imports[i];
        if(import->kind == ZIR_IMPORT_MODULE &&
           in_lookup_file(c->module, import->is_file_private,
                          import->span) &&
           strcmp(import->name, member->name) == 0)
            return;
    }
    const ZirType *named_type = FindType(c->module, member->name, NULL);
    if(named_type != NULL && named_type->is_enum)
        return;
    int selected = -1;
    for(int i = c->count - 1; i >= 0; i--) {
        const Binding *binding = &c->bindings[i];
        if(!binding->is_using_namespace || binding->is_enum_namespace)
            continue;
        int visible_root = -1;
        for(int j = c->count - 1; j >= 0; j--)
            if(!c->bindings[j].is_using_namespace &&
               strcmp(c->bindings[j].name, binding->name) == 0) {
                visible_root = j;
                break;
            }
        if(visible_root != binding->root_index) continue;
        const char *type = skip_ws(binding->type);
        if(*type == '*') type = skip_ws(type + 1);
        const ZirModule *owner = NULL;
        const ZirType *record = FindType(c->module, type, &owner);
        if(record == NULL) continue;
        char source_name[ZIR_NAME_MAX];
        const char *promoted = apply_using_filter(binding->using_filter,
                                                  member->name, source_name,
                                                  sizeof(source_name));
        if(promoted == NULL) continue;
        char field_path[ZIR_NAME_MAX], field_type[ZIR_NAME_MAX];
        int found = ResolveRecordField(owner, record, promoted,
                                       field_path, sizeof(field_path),
                                       field_type, sizeof(field_type));
        if(found > 0 && selected >= 0 &&
           binding->root_index == c->bindings[selected].root_index &&
           strcmp(binding->name, c->bindings[selected].name) == 0 &&
           strcmp(binding->using_path,
                  c->bindings[selected].using_path) == 0)
            continue;
        if(found < 0 || (found > 0 && selected >= 0)) {
            error(c, member->span, "ambiguous using field", member->name);
            return;
        }
        if(found > 0) selected = i;
    }
    if(selected < 0) return;
    char base_name[ZIR_NAME_MAX], using_path[ZIR_NAME_MAX];
    copy_text(base_name, sizeof(base_name), c->bindings[selected].name);
    copy_text(using_path, sizeof(using_path), c->bindings[selected].using_path);
    ZirSourceSpan span = member->span;
    int base_index = c->fn->expr_count;
    ZirExpr *base = FunctionAddExpr(c->fn, ZIR_EXPR_IDENT, base_name, span);
    if(base == NULL) {
        c->failed = 1;
        return;
    }
    copy_text(base->name, sizeof(base->name), base_name);
    for(const char *segment = using_path; *segment != '\0'; ) {
        const char *dot = strchr(segment, '.');
        size_t length = dot == NULL ? strlen(segment) :
                        (size_t)(dot - segment);
        char field_name[ZIR_NAME_MAX];
        memcpy(field_name, segment, length);
        field_name[length] = '\0';
        int next_index = c->fn->expr_count;
        ZirExpr *next = FunctionAddExpr(c->fn, ZIR_EXPR_MEMBER,
                                        field_name, span);
        if(next == NULL) { c->failed = 1; return; }
        next->left = base_index;
        copy_text(next->name, sizeof(next->name), field_name);
        copy_text(next->op, sizeof(next->op), ".");
        base_index = next_index;
        if(dot == NULL) break;
        segment = dot + 1;
    }
    member = &c->fn->exprs[index];
    member->kind = ZIR_EXPR_MEMBER;
    member->left = base_index;
    {
        char source_name[ZIR_NAME_MAX];
        const char *promoted = apply_using_filter(
            c->bindings[selected].using_filter, member->name, source_name,
            sizeof(source_name));
        if(promoted != NULL && promoted != member->name)
            copy_text(member->name, sizeof(member->name), promoted);
    }
    copy_text(member->op, sizeof(member->op), ".");
    c->using_rewritten = 1;
}

void
promote_using_tree(Checker *c, int index)
{
    if(index < 0 || index >= c->fn->expr_count) return;
    const ZirExpr *e = &c->fn->exprs[index];
    int left = e->left, right = e->right, third = e->third;
    int child = e->first_child;
    promote_using_tree(c, left);
    promote_using_tree(c, right);
    promote_using_tree(c, third);
    while(child >= 0) {
        int next = c->fn->exprs[child].next_sibling;
        promote_using_tree(c, child);
        child = next;
    }
    promote_using_member(c, index);
}

static int
order_expression(ExprOrder *order, int index, int depth)
{
    if(index < 0) return 1;
    if(index >= order->function->expr_count || depth > 1024)
        return 0;
    if(order->state[index] == 2) return 1;
    if(order->state[index] == 1) return 0;
    order->state[index] = 1;
    const ZirExpr *expression = &order->function->exprs[index];
    if(!order_expression(order, expression->left, depth + 1) ||
       !order_expression(order, expression->right, depth + 1) ||
       !order_expression(order, expression->third, depth + 1))
        return 0;
    for(int child = expression->first_child; child >= 0;
        child = order->function->exprs[child].next_sibling)
        if(!order_expression(order, child, depth + 1))
            return 0;
    order->map[index] = order->count;
    order->ordered[order->count++] = *expression;
    order->state[index] = 2;
    return 1;
}

int
order_using_expressions(ZirFunction *function)
{
    int count = function->expr_count;
    ExprOrder order = {0};
    order.function = function;
    order.ordered = calloc((size_t)count, sizeof(*order.ordered));
    order.map = malloc((size_t)count * sizeof(*order.map));
    order.state = calloc((size_t)count, sizeof(*order.state));
    if(order.ordered == NULL || order.map == NULL || order.state == NULL)
        goto failed;
    for(int i = 0; i < count; i++) order.map[i] = -1;
    for(int s = 0; s < function->stmt_count; s++) {
        ZirStmt *statement = &function->stmts[s];
        if(!order_expression(&order, statement->lhs_root, 0) ||
           !order_expression(&order, statement->expr_root, 0))
            goto failed;
    }
    for(int i = 0; i < count; i++)
        if(!order_expression(&order, i, 0)) goto failed;
    if(order.count != count) goto failed;
    for(int i = 0; i < count; i++) {
        ZirExpr *expression = &order.ordered[i];
        if(expression->left >= 0) expression->left = order.map[expression->left];
        if(expression->right >= 0) expression->right = order.map[expression->right];
        if(expression->third >= 0) expression->third = order.map[expression->third];
        if(expression->first_child >= 0)
            expression->first_child = order.map[expression->first_child];
        if(expression->next_sibling >= 0)
            expression->next_sibling = order.map[expression->next_sibling];
    }
    for(int s = 0; s < function->stmt_count; s++) {
        ZirStmt *statement = &function->stmts[s];
        if(statement->expr_root >= 0)
            statement->expr_root = order.map[statement->expr_root];
        if(statement->lhs_root >= 0)
            statement->lhs_root = order.map[statement->lhs_root];
    }
    free(function->exprs);
    function->exprs = order.ordered;
    function->expr_cap = count;
    free(order.map);
    free(order.state);
    return 1;
failed:
    free(order.ordered);
    free(order.map);
    free(order.state);
    return 0;
}

const ZirFunction *
function(Checker *c, const char *name, ZirSourceSpan span)
{
    const ZirModule *owner = NULL;
    const ZirFunction *found = NULL;
    if(ResolveFunction(c->module, name, &owner, &found) < 0) {
        error(c, span, "ambiguous imported function", name);
        c->failed = 1;
    }
    return found;
}

int
replace_template_type(char *target, size_t capacity, const char *source,
                      const char *parameter, const char *concrete)
{
    size_t used = 0, parameter_length = strlen(parameter);
    for(const char *p = source; *p;) {
        const char *start = p;
        if(*p == '$' || isalpha((unsigned char)*p) || *p == '_') {
            if(*p == '$') p++;
            while(isalnum((unsigned char)*p) || *p == '_') p++;
        } else p++;
        size_t length = (size_t)(p - start);
        int match = (length == parameter_length &&
                     !strncmp(start, parameter, length)) ||
                    (length == parameter_length + 1 && *start == '$' &&
                     !strncmp(start + 1, parameter, parameter_length));
        const char *piece = match ? concrete : start;
        size_t piece_length = match ? strlen(concrete) : length;
        if(used + piece_length >= capacity) return 0;
        memcpy(target + used, piece, piece_length);
        used += piece_length;
    }
    target[used] = '\0';
    return 1;
}

static uint64_t
specialization_hash(const ZirModule *owner, const ZirModule *instance_owner,
                    const ZirFunction *fn,
                    const char *type)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    const char *pieces[] = {owner->name, owner->source_path, SpanPath(fn->span),
                            fn->name, type, instance_owner->source_path, NULL};
    for(int i = 0; pieces[i]; i++) {
        for(const unsigned char *p = (const unsigned char *)pieces[i]; *p; p++) {
            hash ^= *p;
            hash *= UINT64_C(1099511628211);
        }
        hash ^= 0xff;
        hash *= UINT64_C(1099511628211);
    }
    unsigned int position[] = {(unsigned int)fn->span.line,
                               (unsigned int)fn->span.column};
    for(int i = 0; i < 2; i++) for(int shift = 0; shift < 32; shift += 8) {
        hash ^= (position[i] >> shift) & 0xffu;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int
queue_specialization(Checker *c, const ZirModule *owner,
                     const ZirModule *instance_owner,
                     const ZirFunction *fn, const char *type,
                     char *name, size_t name_size, ZirSourceSpan span)
{
    int length = snprintf(name, name_size, "__zi_spec_%016llx",
                          (unsigned long long)specialization_hash(
                              owner, instance_owner, fn, type));
    if(length < 0 || (size_t)length >= name_size) return 0;
    int index = (int)(fn - owner->functions);
    for(int i = 0; i < instance_owner->function_count; i++) {
        const ZirFunction *other = &instance_owner->functions[i];
        if(strcmp(other->name, name)) continue;
        if(other->is_specialization &&
           !strcmp(other->specialization_type, type) &&
           !strcmp(other->template_param, fn->template_param)) return 1;
        error(c, span, "specialization name collision", name);
        return 0;
    }
    for(int i = 0; i < c->specialization_count; i++) {
        SpecializationRequest *request = &c->specializations[i];
        if(request->instance_owner != instance_owner ||
           strcmp(request->name, name)) continue;
        if(request->template_owner == owner &&
           request->template_index == index && !strcmp(request->type, type))
            return 1;
        error(c, span, "specialization name collision", name);
        return 0;
    }
    if(c->specialization_count == c->specialization_capacity) {
        int capacity = c->specialization_capacity ?
            c->specialization_capacity * 2 : 8;
        SpecializationRequest *next = realloc(c->specializations,
            (size_t)capacity * sizeof(*next));
        if(next == NULL) { c->failed = 1; return 0; }
        c->specializations = next;
        c->specialization_capacity = capacity;
    }
    SpecializationRequest *request =
        &c->specializations[c->specialization_count++];
    request->template_owner = (ZirModule *)owner;
    request->instance_owner = (ZirModule *)instance_owner;
    request->template_index = index;
    request->call_span = span;
    copy_text(request->name, sizeof(request->name), name);
    copy_text(request->type, sizeof(request->type), type);
    return 1;
}
