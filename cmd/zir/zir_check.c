#include "zir_check.h"
#include "zir_borrow.h"
#include "zir_law.h"
#include "zir_text.h"
#include "zir_emit.h"
#include "zir_expr.h"
#include "zir_token.h"
#include "zir_parse.h"
#include "zir_diagnostic.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Binding {
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    char using_path[ZIR_NAME_MAX];
    int depth;
    int is_using_namespace;
    int is_enum_namespace;
    int root_index;
    int moved;
    char moved_paths[32][ZIR_NAME_MAX];
    int moved_path_count;
    int touched;
    int borrow_count;
    int borrows_index;
    char using_filter[160];
} Binding;

typedef struct SpecializationRequest {
    ZirModule *template_owner;
    ZirModule *instance_owner;
    int template_index;
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    ZirSourceSpan call_span;
} SpecializationRequest;

typedef struct MoveState {
    int moved;
    int moved_path_count;
    char moved_paths[32][ZIR_NAME_MAX];
} MoveState;

typedef struct Checker {
    ZirProgram **programs;
    int program_count;
    ZirModule *module;
    ZirFunction *fn;
    ZirStmt *current_stmt;
    char expected_type[ZIR_NAME_MAX];
    Binding *bindings;
    int count, capacity, depth, errors, failed;
    int inference_only;
    int using_rewritten;
    int aggregate_rewritten;
    int assign_destination;
    int destination_was_moved;
    int conversions_applied;
    char vec_slice_type[ZIR_NAME_MAX];
    struct {
        int at;
        int count;
        MoveState *states;
    } restores[64];
    int restore_count;
    SpecializationRequest *specializations;
    int specialization_count, specialization_capacity;
} Checker;

static void
select_lookup_file(ZirModule *module, ZirSourceSpan span)
{
    copy_text(module->lookup_path, sizeof(module->lookup_path), span.path);
}

static int
in_lookup_file(const ZirModule *module, int is_file_private,
               ZirSourceSpan span)
{
    return !is_file_private || module->lookup_path[0] == '\0' ||
           strcmp(module->lookup_path, span.path) == 0;
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
            strcmp(module->functions[i].span.path, path) == 0)) return 0;
    for(int i = 0; i < module->global_count; i++)
        if(strcmp(module->globals[i].name, name) == 0 &&
           (!module->globals[i].is_file_private ||
            strcmp(module->globals[i].span.path, path) == 0)) return 0;
    for(int i = 0; i < module->define_count; i++)
        if(strcmp(module->defines[i].name, name) == 0 &&
           (!module->defines[i].is_file_private ||
            strcmp(module->defines[i].span.path, path) == 0)) return 0;
    for(int i = 0; i < module->type_count; i++)
        if(strcmp(module->types[i].name, name) == 0 &&
           (!module->types[i].is_file_private ||
            strcmp(module->types[i].span.path, path) == 0)) return 0;
    for(int i = 0; i < module->import_count; i++)
        if(strcmp(module->imports[i].name, name) == 0 &&
           (!module->imports[i].is_file_private ||
            strcmp(module->imports[i].span.path, path) == 0)) return 0;
    for(int i = 0; i < module->function_count; i++)
        if(module->functions[i].is_file_private &&
           strcmp(module->functions[i].span.path, path) != 0 &&
           strcmp(module->functions[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->global_count; i++)
        if(module->globals[i].is_file_private &&
           strcmp(module->globals[i].span.path, path) != 0 &&
           strcmp(module->globals[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->define_count; i++)
        if(module->defines[i].is_file_private &&
           strcmp(module->defines[i].span.path, path) != 0 &&
           strcmp(module->defines[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->type_count; i++)
        if(module->types[i].is_file_private &&
           strcmp(module->types[i].span.path, path) != 0 &&
           strcmp(module->types[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->import_count; i++)
        if(module->imports[i].is_file_private &&
           strcmp(module->imports[i].span.path, path) != 0 &&
           strcmp(module->imports[i].name, name) == 0)
            return 1;
    return 0;
}

static int
check_file_private_expression(const ZirModule *module, const char *source,
                              ZirSourceSpan span)
{
    ZirLexer lexer;
    ZirToken previous = {0};
    ZirToken current, next;
    LexerInit(&lexer, source, span.path);
    current = LexerNext(&lexer);
    next = LexerNext(&lexer);
    while(current.kind != ZIR_TOKEN_EOF) {
        if(current.kind == ZIR_TOKEN_IDENT &&
           strcmp(previous.text, ".") != 0 &&
           strcmp(next.text, ":") != 0 &&
           file_private_name(module, current.text, span.path)) {
            Diagnostic(span, "check.file_scope",
                       "file-private declaration is not visible: %s",
                       current.text);
            return 0;
        }
        previous = current;
        current = next;
        next = LexerNext(&lexer);
    }
    return 1;
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

static const char *
apply_using_filter(const char *filter, const char *name,
                   char *buffer, size_t size);

static int
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

int
LowerFileScopeUsing(ZirModule *module, char *source, size_t capacity,
                    ZirSourceSpan span)
{
    if(module->using_count == 0 || !source[0]) return 1;
    char saved_path[ZIR_PATH_MAX];
    char output[ZIR_TEXT_MAX];
    ZirLexer lexer;
    ZirToken previous = {0};
    size_t used = 0, copied = 0;
    copy_text(saved_path, sizeof(saved_path), module->lookup_path);
    select_lookup_file(module, span);
    LexerInit(&lexer, source, span.path);
    ZirToken current = LexerNext(&lexer);
    size_t end = lexer.pos;
    while(current.kind != ZIR_TOKEN_EOF) {
        ZirToken next = LexerNext(&lexer);
        size_t start = end - strlen(current.text);
        int64_t value = 0;
        int opened = 0;
        if(current.kind == ZIR_TOKEN_IDENT && !current.truncated &&
           strcmp(previous.text, ".") != 0 &&
           strcmp(next.text, "=") != 0 &&
           strcmp(next.text, ":") != 0 &&
           !file_scope_symbol_visible(module, current.text))
            opened = opened_file_enum(module, current.text, span, &value);
        if(opened < 0) {
            copy_text(module->lookup_path, sizeof(module->lookup_path),
                      saved_path);
            return 0;
        }
        if(opened > 0) {
            char number[64];
            int length = snprintf(number, sizeof(number), "%lld",
                                  (long long)value);
            if(length < 0 || used + start - copied + (size_t)length >=
                             sizeof(output)) goto failed;
            memcpy(output + used, source + copied, start - copied);
            used += start - copied;
            memcpy(output + used, number, (size_t)length);
            used += (size_t)length;
            copied = end;
        }
        previous = current;
        current = next;
        end = lexer.pos;
    }
    if(used + strlen(source + copied) >= sizeof(output) ||
       used + strlen(source + copied) >= capacity) goto failed;
    copy_text(output + used, sizeof(output) - used, source + copied);
    copy_text(source, capacity, output);
    copy_text(module->lookup_path, sizeof(module->lookup_path), saved_path);
    return 1;
failed:
    copy_text(module->lookup_path, sizeof(module->lookup_path), saved_path);
    Diagnostic(span, "check.enum_scope",
               "cannot lower file-scope using expression");
    return 0;
}

static int
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

static int
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

static int bound_expression(const ZirModule *module,
                            const ZirFunction *expression, int index,
                            int depth, int64_t *value);
static const char *local_storage_error(const ZirModule *module,
                                       const char *type);
static void normalize_array(const ZirModule *module, char *type, size_t size);
static int visible_define(const ZirModule *module, const char *name,
                          const ZirDefine **definition,
                          const ZirModule **owner,
                          const ZirImport **selected_import);

static int
layout_type(const ZirModule *module, const char *source, int depth,
            size_t *size, size_t *alignment)
{
    char type[ZIR_NAME_MAX], element[ZIR_NAME_MAX];
    const char *scalar;
    const ZirModule *owner = NULL;
    const ZirType *record;
    ZirType instance = {0};
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
            ZirFunction probe = {0};
            int64_t resolved = -1;
            int root, status;
            size_t length = close ? (size_t)(close - type - 1) : 0;
            if(length == 0 || length >= sizeof(bound)) return 0;
            memcpy(bound, type + 1, length);
            bound[length] = '\0';
            trim_in_place(bound);
            root = ParseExpr(&probe, module, bound, Span("", 0, 0));
            status = bound_expression(module, &probe, root,
                                      depth + 1, &resolved);
            free(probe.exprs);
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
                   sizeof(instance.template_args)) {
                memcpy(base, type, length);
                base[length] = '\0';
                trim_in_place(base);
                const ZirType *generic = FindType(module, base, &owner);
                if(generic != NULL && generic->is_record_template) {
                    copy_text(instance.name, sizeof(instance.name), type);
                    copy_text(instance.template_name,
                              sizeof(instance.template_name), base);
                    memcpy(instance.template_args, opening + 1,
                           (size_t)(closing - opening - 1));
                    instance.template_args[closing - opening - 1] = '\0';
                    instance.is_type_instance = 1;
                    if(!InstantiateGenericRecord(&instance, generic)) return 0;
                    record = &instance;
                }
            }
        }
    } else if(record->is_type_instance) {
        const ZirType *generic = FindType(owner ? owner : module,
                                          record->template_name, NULL);
        if(generic == NULL || !generic->is_record_template) return 0;
        instance = *record;
        instance.body[0] = '\0';
        if(!InstantiateGenericRecord(&instance, generic)) return 0;
        record = &instance;
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
TypeLayout(const ZirModule *module, const char *type,
           size_t *size, size_t *alignment)
{
    return layout_type(module, type, 0, size, alignment);
}

static void
error(Checker *c, ZirSourceSpan span, const char *message, const char *detail)
{
    c->errors++;
    Diagnostic(span, "check.type", "%s%s%s",
            message, detail && *detail ? ": " : "", detail ? detail : "");
}

static void
signature_error(Checker *c, ZirSourceSpan span,
                const char *message, const char *name)
{
    error(c, span, message, name);
}

/* Keep children in source order. Their checked indices select the callee
 * parameter while native emission and the VM evaluate them in that order. */
static int
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

static int
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

static const char *lookup_lexical(Checker *c, const char *name);
static const ZirGlobal *global_binding(Checker *c, const char *name);

static const char *
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

static int
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

static void
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

static void
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

static void
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

static int
lower_file_record_using(ZirModule *module, char *source, size_t capacity,
                        ZirSourceSpan span)
{
    if(module->using_count == 0 || !source[0]) return 1;
    char saved_path[ZIR_PATH_MAX];
    copy_text(saved_path, sizeof(saved_path), module->lookup_path);
    select_lookup_file(module, span);
    Checker scope = {.module = module};
    for(int i = 0; i < module->using_count; i++) {
        const ZirUsing *using = &module->usings[i];
        if(!in_lookup_file(module, using->is_file_private, using->span))
            continue;
        const ZirType *type = FindType(module, using->path, NULL);
        if(type != NULL && type->is_enum) continue;
        activate_using_filtered(&scope, using->path,
                                using->filter, using->span);
        if(scope.failed || scope.errors) goto failed;
    }
    char output[ZIR_TEXT_MAX];
    ZirLexer lexer;
    ZirToken previous = {0};
    size_t used = 0, copied = 0;
    LexerInit(&lexer, source, span.path);
    ZirToken current = LexerNext(&lexer);
    size_t end = lexer.pos;
    while(current.kind != ZIR_TOKEN_EOF) {
        ZirToken next = LexerNext(&lexer);
        size_t start = end - strlen(current.text);
        char path[ZIR_TEXT_MAX] = "";
        int opened = 0;
        if(current.kind == ZIR_TOKEN_IDENT && !current.truncated &&
           strcmp(previous.text, ".") != 0 &&
           strcmp(next.text, "=") != 0 &&
           strcmp(next.text, ":") != 0 &&
           !file_scope_symbol_visible(module, current.text))
            opened = opened_file_record(&scope, current.text, span,
                                        path, sizeof(path));
        if(opened > 0) {
            int64_t enum_value = 0;
            int enumeration = opened_file_enum(module, current.text,
                                               span, &enum_value);
            if(enumeration > 0) {
                Diagnostic(span, "check.using_scope",
                           "ambiguous using field: %s", current.text);
                opened = -1;
            } else if(enumeration < 0)
                opened = -1;
        }
        if(opened < 0) goto failed;
        if(opened > 0) {
            size_t length = strlen(path);
            if(used + start - copied + length >= sizeof(output)) {
                Diagnostic(span, "check.using_scope",
                           "file-scope using expression is too long");
                goto failed;
            }
            memcpy(output + used, source + copied, start - copied);
            used += start - copied;
            memcpy(output + used, path, length);
            used += length;
            copied = end;
        }
        previous = current;
        current = next;
        end = lexer.pos;
    }
    if(used + strlen(source + copied) >= sizeof(output) ||
       used + strlen(source + copied) >= capacity) {
        Diagnostic(span, "check.using_scope",
                   "file-scope using expression is too long");
        goto failed;
    }
    copy_text(output + used, sizeof(output) - used, source + copied);
    copy_text(source, capacity, output);
    free(scope.bindings);
    copy_text(module->lookup_path, sizeof(module->lookup_path), saved_path);
    return 1;
failed:
    free(scope.bindings);
    copy_text(module->lookup_path, sizeof(module->lookup_path), saved_path);
    return 0;
}

/* Earlier initialized globals have known startup values. Expose those values
 * only while folding another global initializer; they are still mutable at
 * runtime and are not compile-time names in #if or #assert. */
static int
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

static int
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

static void
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

typedef struct ExprOrder {
    const ZirFunction *function;
    ZirExpr *ordered;
    int *map;
    unsigned char *state;
    int count;
} ExprOrder;

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

static int
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

static const ZirFunction *
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

static int
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
    const char *pieces[] = {owner->name, owner->source_path, fn->span.path,
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

static int
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

static const char *
lookup_lexical(Checker *c, const char *name)
{
    for(int i = c->count - 1; i >= 0; i--)
        if(!c->bindings[i].is_using_namespace &&
           !strcmp(c->bindings[i].name, name))
            return c->bindings[i].type;
    return "";
}

static const ZirGlobal *
global_binding(Checker *c, const char *name)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    int status = ResolveGlobal(c->module, name, &owner, &global);
    if(status < 0 && !c->failed) {
        error(c, c->current_stmt != NULL ? c->current_stmt->span :
              c->fn != NULL ? c->fn->span : c->module->span,
              "ambiguous global name", name);
        c->failed = 1;
    }
    if(status == 1 && owner != c->module && strchr(name, '.') == NULL) {
        const char *base = global->type;
        if(base[0] == '*') base = skip_ws(base + 1);
        const ZirType *declared = FindType(owner, base, NULL);
        const ZirType *visible = FindType(c->module, base, NULL);
        if(declared != NULL && visible != NULL &&
           declared != visible &&
           import_type_alias(c->module, owner) == NULL && !c->failed) {
            error(c, c->current_stmt != NULL ? c->current_stmt->span :
                  c->fn != NULL ? c->fn->span : c->module->span,
                  "imported global type is shadowed", name);
            c->failed = 1;
            return NULL;
        }
    }
    return status == 1 ? global : NULL;
}

static const char *
lookup(Checker *c, const char *name)
{
    const char *lexical = lookup_lexical(c, name);
    if(*lexical)
        return lexical;
    const ZirGlobal *global = global_binding(c, name);
    if(global != NULL) return global->type;
    const ZirType *type = NULL;
    int resolved = resolve_using_enum(c, name, &type, NULL, 0);
    if(resolved < 0) {
        error(c, c->fn->span, "ambiguous enum member", name);
        c->failed = 1;
    }
    if(resolved > 0)
        return type->name;
    return "";
}

/* Owned Vec bindings move on assignment, argument passing, return, VecFree,
 * and BuilderFinish. A moved binding is empty storage; the checker rejects
 * any further use so every value is dropped exactly once. */
static int
owned_vec_binding_type(Checker *c, const char *type)
{
    return VecElementType(c->module, type, NULL, 0);
}

static Binding *
lexical_vec_binding(Checker *c, int index)
{
    const ZirExpr *e;
    if(index < 0 || index >= c->fn->expr_count)
        return NULL;
    e = &c->fn->exprs[index];
    if(e->kind != ZIR_EXPR_IDENT || e->is_this ||
       !strcmp(e->name, "true") || !strcmp(e->name, "false") ||
       !strcmp(e->name, "null"))
        return NULL;
    for(int i = c->count - 1; i >= 0; i--)
        if(!c->bindings[i].is_using_namespace &&
           !strcmp(c->bindings[i].name, e->name))
            return owned_vec_binding_type(c, c->bindings[i].type) ?
                &c->bindings[i] : NULL;
    return NULL;
}

/* Aggregate ownership follows the same move discipline as a direct Vec. */
static Binding *
lexical_owned_binding(Checker *c, int index)
{
    const ZirExpr *e;
    if(index < 0 || index >= c->fn->expr_count)
        return NULL;
    e = &c->fn->exprs[index];
    if(e->kind != ZIR_EXPR_IDENT || e->is_this ||
       !strcmp(e->name, "true") || !strcmp(e->name, "false") ||
       !strcmp(e->name, "null"))
        return NULL;
    for(int i = c->count - 1; i >= 0; i--)
        if(!c->bindings[i].is_using_namespace &&
           !strcmp(c->bindings[i].name, e->name) &&
           contains_vec(c->module, c->bindings[i].type, 0))
            return &c->bindings[i];
    return NULL;
}

/* Individual ownership moves are tracked by the lexical root and a
 * member-field path. Index and pointer paths stay whole-binding moves until
 * their aliasing can be represented safely. */
static Binding *
owned_member_path(Checker *c, int index, char *path, size_t capacity)
{
    const ZirExpr *e;
    Binding *root;
    size_t length;
    int written;

    if(index < 0 || index >= c->fn->expr_count || path == NULL ||
       capacity == 0)
        return NULL;
    e = &c->fn->exprs[index];
    if(e->kind != ZIR_EXPR_MEMBER)
        return NULL;
    if(e->left < 0 || e->left >= c->fn->expr_count)
        return NULL;

    const ZirExpr *base = &c->fn->exprs[e->left];
    if(base->kind == ZIR_EXPR_IDENT && !base->is_this &&
       strcmp(base->name, "true") && strcmp(base->name, "false") &&
       strcmp(base->name, "null")) {
        for(int i = c->count - 1; i >= 0; i--)
            if(!c->bindings[i].is_using_namespace &&
               !strcmp(c->bindings[i].name, base->name) &&
               contains_vec(c->module, c->bindings[i].type, 0)) {
                copy_text(path, capacity, e->name);
                return &c->bindings[i];
            }
        return NULL;
    }

    root = owned_member_path(c, e->left, path, capacity);
    if(root == NULL)
        return NULL;
    length = strlen(path);
    written = snprintf(path + length, capacity - length, "%s%s",
                       length != 0 ? "." : "", e->name);
    if(written < 0 || (size_t)written >= capacity - length) {
        path[length] = '\0';
        return NULL;
    }
    return root;
}

static int
path_prefix(const char *prefix, const char *value)
{
    size_t length = strlen(prefix);
    if(strncmp(prefix, value, length) != 0)
        return 0;
    return value[length] == '\0' || value[length] == '.';
}

static int
moved_path_conflict(const Binding *binding, const char *path, int owns_storage)
{
    if(binding == NULL || binding->moved)
        return 1;
    for(int i = 0; i < binding->moved_path_count; i++) {
        const char *moved = binding->moved_paths[i];
        if(path_prefix(moved, path) ||
           (owns_storage && path_prefix(path, moved)))
            return 1;
    }
    return 0;
}

static void
check_moved_path_use(Checker *c, int index)
{
    const ZirExpr *e;
    char path[ZIR_TEXT_MAX];
    Binding *binding;

    if(index < 0 || index >= c->fn->expr_count)
        return;
    e = &c->fn->exprs[index];
    if(c->assign_destination)
        return;
    if(e->kind != ZIR_EXPR_MEMBER)
        return;
    binding = owned_member_path(c, index, path, sizeof(path));
    if(binding != NULL &&
       moved_path_conflict(binding, path,
                           contains_vec(c->module, e->type, 0)))
        error(c, e->span, "owned record field is used after moving", path);
}

static int
mark_moved_member_path(Checker *c, int index)
{
    const ZirExpr *e;
    char path[ZIR_TEXT_MAX];
    Binding *binding;

    if(index < 0 || index >= c->fn->expr_count)
        return 0;
    e = &c->fn->exprs[index];
    if(e->kind != ZIR_EXPR_MEMBER ||
       !VecElementType(c->module, e->type, NULL, 0))
        return 0;
    binding = owned_member_path(c, index, path, sizeof(path));
    if(binding == NULL)
        return 0;
    if(moved_path_conflict(binding, path, 1)) {
        error(c, e->span, "owned record field is used after moving", path);
        return 1;
    }
    if(binding->borrow_count > 0) {
        error(c, e->span,
              "cannot move a Vec with a live borrowed view", binding->name);
        return 1;
    }
    if(binding->moved_path_count >=
       (int)(sizeof(binding->moved_paths) /
             sizeof(binding->moved_paths[0]))) {
        error(c, e->span,
              "too many moved fields in one aggregate", binding->name);
        return 1;
    }
    copy_text(binding->moved_paths[binding->moved_path_count++],
              sizeof(binding->moved_paths[0]), path);
    c->fn->exprs[index].is_move = 1;
    return 1;
}

/* Only local bindings move. A global keeps shared module state, so moving it
 * would leave every other function reading transferred storage. */
static int
global_vec_source(Checker *c, int index)
{
    const ZirExpr *e;
    const ZirGlobal *global;
    if(index < 0 || index >= c->fn->expr_count)
        return 0;
    e = &c->fn->exprs[index];
    if(e->kind == ZIR_EXPR_COMPOUND) {
        for(int child = e->first_child; child >= 0;
            child = c->fn->exprs[child].next_sibling) {
            const ZirExpr *entry = &c->fn->exprs[child];
            if(entry->kind == ZIR_EXPR_FIELD_INIT &&
               contains_vec(c->module, entry->type, 0) &&
               global_vec_source(c, entry->right))
                return 1;
        }
        return 0;
    }
    if(e->kind == ZIR_EXPR_MEMBER)
        return global_vec_source(c, e->left);
    if(e->kind != ZIR_EXPR_IDENT || *lookup_lexical(c, e->name))
        return 0;
    global = global_binding(c, e->name);
    return global != NULL && contains_vec(c->module, global->type, 0);
}

/* A constructing aggregate moves each owned field from a local binding, fresh
 * call result, or another constructing aggregate with the same discipline. */
static int
owned_initializer_shape(Checker *c, int index)
{
    const ZirExpr *e;
    if(index < 0 || index >= c->fn->expr_count)
        return 0;
    e = &c->fn->exprs[index];
    if(e->kind == ZIR_EXPR_IDENT)
        return !global_vec_source(c, index);
    if(e->kind == ZIR_EXPR_CALL)
        return 1;
    if(e->kind == ZIR_EXPR_MEMBER &&
       VecElementType(c->module, e->type, NULL, 0)) {
        char path[ZIR_TEXT_MAX];
        return owned_member_path(c, index, path, sizeof(path)) != NULL;
    }
    if(e->kind != ZIR_EXPR_COMPOUND)
        return 0;
    for(int child = e->first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        const ZirExpr *entry = &c->fn->exprs[child];
        if(entry->kind == ZIR_EXPR_FIELD_INIT &&
           contains_vec(c->module, entry->type, 0) &&
           !owned_initializer_shape(c, entry->right))
            return 0;
    }
    return 1;
}

static int
numeric(const char *type)
{
    const char *scalar = ScalarType(type);
    return !strcmp(type, "integer") || !strcmp(type, "real") ||
           (*scalar && strcmp(scalar, "string") &&
            strchr("suf", scalar[0]) != NULL);
}

static int
integer_type(const char *type)
{
    const char *scalar = ScalarType(type);
    return !strcmp(type, "integer") ||
           (scalar[0] != '\0' && strcmp(scalar, "string") != 0 &&
            (scalar[0] == 's' || scalar[0] == 'u'));
}

/* Bounds use a checked integer subset: every intermediate must fit s32.
 * This avoids accepting a size whose C constant expression overflows while
 * Go evaluates it with arbitrary precision. Unknown host macros stay opaque. */
static int bound_constant(const ZirModule *module, const char *name, int depth, int64_t *value);

static int
bound_expression(const ZirModule *module, const ZirFunction *expression, int index,
                 int depth, int64_t *value)
{
    if(index < 0 || depth > 128)
        return -1;
    const ZirExpr *node = &expression->exprs[index];
    if(node->kind == ZIR_EXPR_IDENT)
        return bound_constant(module, node->name, depth + 1, value);
    if(node->kind == ZIR_EXPR_SIZE_OF) {
        size_t size, alignment;
        if(!layout_type(module, node->name, depth + 1,
                        &size, &alignment) ||
           size > INT32_MAX)
            return -1;
        *value = (int64_t)size;
        return 1;
    }
    if(node->kind == ZIR_EXPR_INT) {
        char *end;
        errno = 0;
        *value = strtoll(node->text, &end, 0);
        if(errno || end == node->text || *end || *value < 0 || *value > INT32_MAX)
            return -1;
        return 1;
    }
    int64_t left = 0, right = 0;
    if(node->kind == ZIR_EXPR_UNARY) {
        int status = bound_expression(module, expression, node->right, depth + 1, &right);
        if(status != 1)
            return status;
        if(!strcmp(node->op, "+"))
            *value = right;
        else if(!strcmp(node->op, "-"))
            *value = -right;
        else
            return -1;
    } else if(node->kind == ZIR_EXPR_BINARY) {
        int status = bound_expression(module, expression, node->left, depth + 1, &left);
        if(status != 1)
            return status;
        status = bound_expression(module, expression, node->right, depth + 1, &right);
        if(status != 1)
            return status;
        if(left == INT32_MIN && right == -1 &&
           (!strcmp(node->op, "/") || !strcmp(node->op, "%")))
            return -1;
        if(!strcmp(node->op, "+"))
            *value = left + right;
        else if(!strcmp(node->op, "-"))
            *value = left - right;
        else if(!strcmp(node->op, "*"))
            *value = left * right;
        else if(!strcmp(node->op, "/") && right != 0)
            *value = left / right;
        else if(!strcmp(node->op, "%") && right != 0)
            *value = left % right;
        else
            return -1;
    } else {
        return -1;
    }
    return *value >= INT32_MIN && *value <= INT32_MAX ? 1 : -1;
}

static int
bound_constant(const ZirModule *module, const char *name, int depth, int64_t *value)
{
    if(depth > 128)
        return -1;
    const ZirDefine *definition = NULL;
    const ZirModule *owner = NULL;
    const char *dot = strchr(name, '.');
    const char *symbol = dot == NULL ? name : dot + 1;
    for(int pass = 0; pass < 2; pass++) {
        if(dot != NULL && pass == 0)
            continue;
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            if(pass != 0) {
                const ZirImport *import = &module->imports[i];
                if(!in_lookup_file(module, import->is_file_private,
                                   import->span))
                    continue;
                if(dot == NULL && import->kind != ZIR_IMPORT_OPEN &&
                   !(import->kind == ZIR_IMPORT_MODULE &&
                     import->is_using))
                    continue;
                if(dot != NULL &&
                   (import->kind != ZIR_IMPORT_MODULE ||
                    strlen(import->name) != (size_t)(dot - name) ||
                    strncmp(import->name, name, (size_t)(dot - name)) != 0))
                    continue;
            }
            const ZirModule *scope = pass == 0 ? module : module->imports[i].resolved_module;
            if(scope == NULL)
                continue;
            for(int j = 0; j < scope->define_count; j++) {
                const ZirDefine *candidate = &scope->defines[j];
                if((pass != 0 && !candidate->is_public) ||
                   (pass == 0 && !in_lookup_file(module,
                       candidate->is_file_private, candidate->span)) ||
                   strcmp(candidate->name, symbol))
                    continue;
                if(definition != NULL && definition != candidate)
                    return -1;
                definition = candidate;
                owner = scope;
            }
        }
        if(definition != NULL)
            break;
    }
    if(definition == NULL)
        return 0;
    ZirFunction expression = {0};
    int index = ParseExpr(&expression, owner, definition->value, definition->span);
    int status = bound_expression(owner, &expression, index, depth + 1, value);
    free(expression.exprs);
    return status;
}

/* Resolve a literal string definition into the expression graph. The source
 * macro spelling must not become a backend dependency in saved .zir or .zib. */
static int
bound_string_constant(const ZirModule *module, const char *name, int depth,
                      char *literal, size_t size)
{
    if(depth > 128)
        return -1;
    const ZirDefine *definition = NULL;
    const ZirModule *owner = NULL;
    const char *dot = strchr(name, '.');
    const char *symbol = dot == NULL ? name : dot + 1;
    for(int pass = 0; pass < 2; pass++) {
        if(dot != NULL && pass == 0)
            continue;
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            if(pass != 0) {
                const ZirImport *import = &module->imports[i];
                if(!in_lookup_file(module, import->is_file_private,
                                   import->span))
                    continue;
                if(dot == NULL && import->kind != ZIR_IMPORT_OPEN &&
                   !(import->kind == ZIR_IMPORT_MODULE &&
                     import->is_using))
                    continue;
                if(dot != NULL &&
                   (import->kind != ZIR_IMPORT_MODULE ||
                    strlen(import->name) != (size_t)(dot - name) ||
                    strncmp(import->name, name, (size_t)(dot - name)) != 0))
                    continue;
            }
            const ZirModule *scope = pass == 0 ? module :
                                     module->imports[i].resolved_module;
            if(scope == NULL)
                continue;
            for(int j = 0; j < scope->define_count; j++) {
                const ZirDefine *candidate = &scope->defines[j];
                if((pass != 0 && !candidate->is_public) ||
                   (pass == 0 && !in_lookup_file(module,
                       candidate->is_file_private, candidate->span)) ||
                   strcmp(candidate->name, symbol))
                    continue;
                if(definition != NULL && definition != candidate)
                    return -1;
                definition = candidate;
                owner = scope;
            }
        }
        if(definition != NULL)
            break;
    }
    if(definition == NULL)
        return 0;
    ZirFunction expression = {0};
    int index = ParseExpr(&expression, owner, definition->value, definition->span);
    int status = 0;
    if(index < 0)
        status = -1;
    else if(expression.exprs[index].kind == ZIR_EXPR_STRING) {
        copy_text(literal, size, expression.exprs[index].text);
        status = 1;
    } else if(expression.exprs[index].kind == ZIR_EXPR_IDENT) {
        status = bound_string_constant(owner, expression.exprs[index].name,
                                       depth + 1, literal, size);
    }
    free(expression.exprs);
    return status;
}

/* Fold real definitions into typed expression nodes before writing .zir.
 * Backends then see the same literal from source and saved IR. */
static int
bound_real_constant(const ZirModule *module, const char *name, int depth,
                    char *literal, size_t size)
{
    if(depth > 128) return -1;
    const ZirDefine *definition = NULL;
    const ZirModule *owner = NULL;
    const char *dot = strchr(name, '.');
    const char *symbol = dot == NULL ? name : dot + 1;
    for(int pass = 0; pass < 2; pass++) {
        if(dot != NULL && pass == 0) continue;
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            if(pass != 0) {
                const ZirImport *import = &module->imports[i];
                if(!in_lookup_file(module, import->is_file_private,
                                   import->span)) continue;
                if(dot == NULL && import->kind != ZIR_IMPORT_OPEN) continue;
                if(dot != NULL &&
                   (import->kind != ZIR_IMPORT_MODULE ||
                    strlen(import->name) != (size_t)(dot - name) ||
                    strncmp(import->name, name,
                            (size_t)(dot - name)) != 0)) continue;
            }
            const ZirModule *scope = pass == 0 ? module :
                                     module->imports[i].resolved_module;
            if(scope == NULL) continue;
            for(int j = 0; j < scope->define_count; j++) {
                const ZirDefine *candidate = &scope->defines[j];
                if((pass != 0 && !candidate->is_public) ||
                   (pass == 0 && !in_lookup_file(module,
                       candidate->is_file_private, candidate->span)) ||
                   strcmp(candidate->name, symbol)) continue;
                if(definition != NULL && definition != candidate) return -1;
                definition = candidate;
                owner = scope;
            }
        }
        if(definition != NULL) break;
    }
    if(definition == NULL) return 0;
    ZirFunction expression = {0};
    int index = ParseExpr(&expression, owner, definition->value,
                          definition->span);
    int status = 0;
    if(index < 0) status = -1;
    else if(expression.exprs[index].kind == ZIR_EXPR_FLOAT) {
        copy_text(literal, size, expression.exprs[index].text);
        status = 1;
    } else if(expression.exprs[index].kind == ZIR_EXPR_IDENT)
        status = bound_real_constant(owner, expression.exprs[index].name,
                                     depth + 1, literal, size);
    free(expression.exprs);
    return status;
}

/* Aggregate definitions become checked expression nodes at their use site.
 * Limit this path to literal graphs; the compile-time evaluator has already
 * reduced #run results to this form before function checking starts. */
static int
literal_compound_graph(const ZirFunction *expression)
{
    for(int i = 0; i < expression->expr_count; i++) {
        const ZirExpr *node = &expression->exprs[i];
        switch(node->kind) {
        case ZIR_EXPR_COMPOUND:
        case ZIR_EXPR_FIELD_INIT:
        case ZIR_EXPR_INT:
        case ZIR_EXPR_FLOAT:
        case ZIR_EXPR_STRING:
            break;
        case ZIR_EXPR_UNARY:
            if(strcmp(node->op, "+") && strcmp(node->op, "-")) return 0;
            break;
        case ZIR_EXPR_IDENT:
            if(strcmp(node->name, "true") &&
               strcmp(node->name, "false")) return 0;
            break;
        default:
            return 0;
        }
    }
    return 1;
}

typedef struct CompoundConstant {
    char literal[ZIR_TEXT_MAX];
    char qualifier[ZIR_NAME_MAX];
    const ZirModule *owner;
} CompoundConstant;

static int
compound_type_at_use(const ZirModule *module,
                     const CompoundConstant *compound, const char *source,
                     char *target, size_t size)
{
    char element[ZIR_NAME_MAX];
    if(ArrayElementType(source, element, sizeof(element), NULL)) {
        const char *close = strchr(source, ']');
        char qualified[ZIR_NAME_MAX];
        if(close == NULL ||
           !compound_type_at_use(module, compound, element, qualified,
                                 sizeof(qualified))) return 0;
        int written = snprintf(target, size, "%.*s%s",
                               (int)(close - source + 1), source, qualified);
        return written >= 0 && (size_t)written < size;
    }
    const ZirModule *declaring = NULL;
    const ZirType *declared = FindType(compound->owner, source, &declaring);
    if(declared == NULL) {
        copy_text(target, size, source);
        return strlen(source) < size;
    }
    if(compound->owner == module) {
        copy_text(target, size, source);
        return strlen(source) < size;
    }
    const ZirType *visible = FindType(module, source, NULL);
    if(visible == declared) {
        copy_text(target, size, source);
        return strlen(source) < size;
    }
    if(declaring == compound->owner && compound->qualifier[0]) {
        int written = snprintf(target, size, "%s.%s",
                               compound->qualifier, source);
        if(written >= 0 && (size_t)written < size &&
           FindType(module, target, NULL) == declared)
            return 1;
    }
    /* The user reaches the type only through another module, for example
     * a field of an imported record whose type module it does not import.
     * Name it by its declaring module. */
    if(visible == NULL && declaring != NULL && declared->is_public &&
       !declared->is_file_private) {
        int written = snprintf(target, size, "%s.%s", declaring->name,
                               source);
        if(written >= 0 && (size_t)written < size &&
           FindType(module, target, NULL) == declared)
            return 1;
    }
    if(visible != declared)
        return 0;
    copy_text(target, size, source);
    return strlen(source) < size;
}

static int
record_field_type_at_use(const ZirModule *module,
                         const ZirModule *record_owner,
                         const char *record_name, char *field_type,
                         size_t size)
{
    CompoundConstant context = {0};
    context.owner = record_owner;
    const char *dot = strchr(record_name, '.');
    if(dot != NULL && (size_t)(dot - record_name) <
                       sizeof(context.qualifier)) {
        memcpy(context.qualifier, record_name,
               (size_t)(dot - record_name));
        context.qualifier[dot - record_name] = '\0';
    } else if(record_owner != module) {
        const char *alias = import_type_alias(module, record_owner);
        if(alias != NULL)
            copy_text(context.qualifier, sizeof(context.qualifier), alias);
    }
    char qualified[ZIR_NAME_MAX];
    if(!compound_type_at_use(module, &context, field_type, qualified,
                             sizeof(qualified)) ||
       strlen(qualified) >= size) return 0;
    copy_text(field_type, size, qualified);
    return 1;
}

static int
append_compound_text(char *out, size_t size, size_t *used,
                     const char *part)
{
    size_t length = strlen(part);
    if(length >= size - *used) return 0;
    memcpy(out + *used, part, length);
    *used += length;
    out[*used] = '\0';
    return 1;
}

static int
render_compound_at_use(const ZirFunction *expression, int index,
                       char *out, size_t size, size_t *used, int depth)
{
    if(index < 0 || index >= expression->expr_count || depth > 64)
        return 0;
    const ZirExpr *node = &expression->exprs[index];
    if(node->kind != ZIR_EXPR_COMPOUND)
        return append_compound_text(out, size, used, node->text);
    int array = node->name[0] == '[';
    if(!append_compound_text(out, size, used,
                             array ? ".[" : ".{")) return 0;
    int ordinal = 0;
    for(int child = node->first_child; child >= 0;
        child = expression->exprs[child].next_sibling) {
        const ZirExpr *entry = &expression->exprs[child];
        if(ordinal++ && !append_compound_text(out, size, used, ", "))
            return 0;
        if(!array && !strcmp(entry->op, "=")) {
            if(!append_compound_text(out, size, used, ".") ||
               !append_compound_text(out, size, used, entry->name) ||
               !append_compound_text(out, size, used, " = "))
                return 0;
        }
        if(!render_compound_at_use(expression, entry->right, out, size,
                                   used, depth + 1)) return 0;
    }
    return append_compound_text(out, size, used, array ? "]" : "}");
}

static int
lower_compound_global(const ZirModule *module, const ZirGlobal *global,
                      const CompoundConstant *compound,
                      char *result, size_t size)
{
    ZirFunction expression = {0};
    int root = ParseExpr(&expression, compound->owner, compound->literal,
                         global->span);
    int ok = 0;
    if(root >= 0 && expression.exprs[root].kind == ZIR_EXPR_COMPOUND &&
       literal_compound_graph(&expression)) {
        char actual[ZIR_NAME_MAX];
        if(compound_type_at_use(module, compound,
                expression.exprs[root].name, actual, sizeof(actual)) &&
           !strcmp(actual, global->type)) {
            size_t used = 0;
            result[0] = '\0';
            ok = render_compound_at_use(&expression, root, result,
                                        size, &used, 0);
        }
    }
    free(expression.exprs);
    return ok;
}

static void
compound_qualifier_for_global(CompoundConstant *compound,
                              const char *expected_type)
{
    char current[ZIR_NAME_MAX], element[ZIR_NAME_MAX];
    copy_text(current, sizeof(current), expected_type);
    while(ArrayElementType(current, element, sizeof(element), NULL))
        copy_text(current, sizeof(current), element);
    const char *dot = strchr(current, '.');
    if(dot == NULL || (size_t)(dot - current) >=
                      sizeof(compound->qualifier)) return;
    memcpy(compound->qualifier, current,
           (size_t)(dot - current));
    compound->qualifier[dot - current] = '\0';
}

static int
exported_define(const ZirModule *module, const char *name, int depth,
                const ZirDefine **definition, const ZirModule **owner)
{
    if(depth >= 32) return -1;
    *definition = NULL;
    *owner = NULL;
    for(int d = 0; d < module->define_count; d++) {
        const ZirDefine *candidate = &module->defines[d];
        if(!candidate->is_public || candidate->is_file_private ||
           strcmp(candidate->name, name)) continue;
        if(*definition != NULL && *definition != candidate) return -1;
        *definition = candidate;
        *owner = module;
    }
    if(*definition != NULL) return 1;
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *import = &module->imports[i];
        if(!import->is_using || !import->is_public ||
           import->is_file_private || import->resolved_module == NULL)
            continue;
        const ZirDefine *candidate = NULL;
        const ZirModule *candidate_owner = NULL;
        int found = exported_define(import->resolved_module, name,
                                    depth + 1, &candidate, &candidate_owner);
        if(found < 0 || (found == 1 && *definition != NULL &&
                         *definition != candidate))
            return -1;
        if(found == 1) {
            *definition = candidate;
            *owner = candidate_owner;
        }
    }
    return *definition != NULL;
}

static int
visible_define(const ZirModule *module, const char *name,
               const ZirDefine **definition, const ZirModule **owner,
               const ZirImport **selected_import)
{
    *definition = NULL;
    *owner = NULL;
    if(selected_import != NULL) *selected_import = NULL;
    const char *dot = strchr(name, '.');
    const char *symbol = dot == NULL ? name : dot + 1;
    for(int pass = 0; pass < 2; pass++) {
        if(dot != NULL && pass == 0) continue;
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            const ZirImport *import = pass == 0 ? NULL :
                                      &module->imports[i];
            if(import != NULL) {
                if(!in_lookup_file(module, import->is_file_private,
                                   import->span)) continue;
                if(dot == NULL && import->kind != ZIR_IMPORT_OPEN &&
                   !(import->kind == ZIR_IMPORT_MODULE &&
                     import->is_using)) continue;
                if(dot != NULL &&
                   (import->kind != ZIR_IMPORT_MODULE ||
                    strlen(import->name) != (size_t)(dot - name) ||
                    strncmp(import->name, name,
                            (size_t)(dot - name)) != 0)) continue;
            }
            const ZirModule *scope = import == NULL ? module :
                                     import->resolved_module;
            if(scope == NULL) continue;
            if(import == NULL) {
                for(int j = 0; j < scope->define_count; j++) {
                    const ZirDefine *candidate = &scope->defines[j];
                    if(!in_lookup_file(module, candidate->is_file_private,
                                       candidate->span) ||
                       strcmp(candidate->name, symbol)) continue;
                    if(*definition != NULL && *definition != candidate)
                        return -1;
                    *definition = candidate;
                    *owner = scope;
                }
            } else {
                const ZirDefine *candidate = NULL;
                const ZirModule *candidate_owner = NULL;
                int found = exported_define(scope, symbol, 0,
                                            &candidate, &candidate_owner);
                if(found < 0 || (found == 1 && *definition != NULL &&
                                 *definition != candidate))
                    return -1;
                if(found == 1) {
                    *definition = candidate;
                    *owner = candidate_owner;
                    if(selected_import != NULL) *selected_import = import;
                }
            }
        }
        if(*definition != NULL) break;
    }
    return *definition != NULL;
}

static int
bound_compound_constant(const ZirModule *module, const char *name, int depth,
                        CompoundConstant *result)
{
    if(depth > 128) return -1;
    const ZirDefine *definition = NULL;
    const ZirModule *owner = NULL;
    const ZirImport *selected_import = NULL;
    int found = visible_define(module, name, &definition, &owner,
                               &selected_import);
    if(found != 1) return found;
    ZirFunction expression = {0};
    int root = ParseExpr(&expression, owner, definition->value,
                         definition->span);
    int status = 0;
    if(root < 0) status = -1;
    else if(expression.exprs[root].kind == ZIR_EXPR_COMPOUND &&
            literal_compound_graph(&expression)) {
        if(strlen(definition->value) >= sizeof(result->literal)) status = -1;
        else {
            copy_text(result->literal, sizeof(result->literal),
                      definition->value);
            result->owner = owner;
            result->qualifier[0] = '\0';
            if(selected_import != NULL &&
               selected_import->kind == ZIR_IMPORT_MODULE)
                copy_text(result->qualifier,
                          sizeof(result->qualifier),
                          selected_import->name);
            else if(selected_import != NULL &&
                    selected_import->kind == ZIR_IMPORT_OPEN) {
                const char *alias = import_type_alias(module, owner);
                if(alias != NULL)
                    copy_text(result->qualifier,
                              sizeof(result->qualifier), alias);
            }
            status = 1;
        }
    } else if(expression.exprs[root].kind == ZIR_EXPR_IDENT) {
        status = bound_compound_constant(owner,
                    expression.exprs[root].name, depth + 1,
                    result);
        if(status == 1 && owner != module) {
            if(result->owner != owner) status = 0;
            else if(selected_import != NULL &&
                    selected_import->kind == ZIR_IMPORT_MODULE)
                copy_text(result->qualifier,
                          sizeof(result->qualifier),
                          selected_import->name);
            else if(selected_import != NULL &&
                    selected_import->kind == ZIR_IMPORT_OPEN) {
                const char *alias = import_type_alias(module, owner);
                if(alias != NULL)
                    copy_text(result->qualifier,
                              sizeof(result->qualifier), alias);
            }
        }
    }
    free(expression.exprs);
    return status;
}

static int
array_capacity(const ZirModule *module, const char *type, int *capacity)
{
    if(!ArrayElementType(type, NULL, 0, capacity))
        return -1;
    if(*capacity >= 0)
        return 1;
    char expression_text[ZIR_NAME_MAX];
    size_t length = (size_t)(strchr(type, ']') - type - 1);
    if(length >= sizeof(expression_text))
        return -1;
    memcpy(expression_text, type + 1, length);
    expression_text[length] = '\0';
    ZirFunction expression = {0};
    int root = ParseExpr(&expression, module, expression_text,
                         (ZirSourceSpan){0});
    int64_t value = 0;
    int status = bound_expression(module, &expression, root, 0, &value);
    free(expression.exprs);
    if(status != 1)
        return status;
    if(value < 0 || value > 1048576)
        return -1;
    *capacity = (int)value;
    return 1;
}

static void
normalize_array_at(const ZirModule *module, char *type, size_t size, int depth)
{
    char element[ZIR_NAME_MAX];
    int capacity;
    if(depth >= 64) return;
    if(!ArrayElementType(type, element, sizeof(element), NULL)) {
        for(int t = 0; t < module->type_count; t++)
            if(!strcmp(module->types[t].name, type) &&
               in_lookup_file(module, module->types[t].is_file_private,
                              module->types[t].span))
                return;
        /* Resolve local and imported aliases in the declaring module so
         * symbolic bounds retain their original constant namespace. */
        char resolved[ZIR_NAME_MAX];
        if(strlen(type) >= sizeof(resolved)) return;
        copy_text(resolved, sizeof(resolved), type);
        const ZirModule *owner = module;
        for(int alias_depth = 0; alias_depth < 8; alias_depth++) {
            const ZirDefine *found = NULL;
            const ZirModule *next_owner = NULL;
            if(visible_define(owner, resolved, &found, &next_owner,
                              NULL) != 1)
                return;
            const char *value = skip_ws(found->value);
            if(strlen(value) >= sizeof(resolved)) return;
            copy_text(resolved, sizeof(resolved), value);
            owner = next_owner;
            if(resolved[0] == '[')
                break;
        }
        if(resolved[0] != '[' || strlen(resolved) >= size)
            return;
        normalize_array_at(owner, resolved, sizeof(resolved), depth + 1);
        copy_text(type, size, resolved);
        if(!ArrayElementType(type, element, sizeof(element), NULL))
            return;
        module = owner;
    }
    normalize_array_at(module, element, sizeof(element), depth + 1);
    char normalized[ZIR_NAME_MAX];
    int length;
    if(array_capacity(module, type, &capacity) == 1)
        length = snprintf(normalized, sizeof(normalized), "[%d]%s", capacity,
                          element);
    else {
        const char *close = strchr(type, ']');
        length = snprintf(normalized, sizeof(normalized), "%.*s%s",
                          (int)(close - type + 1), type, element);
    }
    if(length >= 0 && (size_t)length < sizeof(normalized))
        copy_text(type, size, normalized);
}

static void
normalize_array(const ZirModule *module, char *type, size_t size)
{
    normalize_array_at(module, type, size, 0);
}

static int
pointer_type(const char *type)
{
    return type[0] != '[' && strchr(type, '*') != NULL;
}

static int
compatible(const char *to, const char *from)
{
    const char *canonical = ScalarType(to);
    if(*canonical) to = canonical;
    if(!*to || !*from) return 1;
    if(!strcmp(to, "null") && !strcmp(from, "null")) return 0;
    if(!strcmp(to, from)) return 1;
    if(!strcmp(from, "null")) return pointer_type(to);
    if(SliceElementType(to, NULL, 0) || SliceElementType(from, NULL, 0)) {
        char a[ZIR_NAME_MAX], b[ZIR_NAME_MAX];
        if(!SliceElementType(to, a, sizeof(a)) || !SliceElementType(from, b, sizeof(b)))
            return 0;
        const char *ca = ScalarType(a), *cb = ScalarType(b);
        return !strcmp(*ca ? ca : a, *cb ? cb : b);
    }
    if(to[0] == '[' || from[0] == '[') {
        char to_element[ZIR_NAME_MAX], from_element[ZIR_NAME_MAX];
        int to_capacity, from_capacity;
        if(!ArrayElementType(to, to_element, sizeof(to_element), &to_capacity) ||
           !ArrayElementType(from, from_element, sizeof(from_element), &from_capacity))
            return 0;
        if(to_capacity != from_capacity)
            return 0;
        if(to_capacity < 0) {
            size_t length = (size_t)(strchr(to, ']') - to);
            if(length != (size_t)(strchr(from, ']') - from) || strncmp(to, from, length))
                return 0;
        }
        const char *to_scalar = ScalarType(to_element);
        const char *from_scalar = ScalarType(from_element);
        return !strcmp(*to_scalar ? to_scalar : to_element,
                       *from_scalar ? from_scalar : from_element);
    }
    if(!strcmp(from, "integer") && numeric(to)) return 1;
    if(!strcmp(from, "real") && (to[0] == 'f')) return 1;
    return 0;
}

static const ZirType *
flags_type(Checker *c, const char *name)
{
    const ZirType *type = FindType(c->module, name, NULL);
    return type != NULL && type->is_enum_flags ? type : NULL;
}

static int
same_declared_type(const ZirModule *module, const char *to,
                   const char *from, int depth)
{
    if(depth > 16) return 0;
    const ZirType *target = FindType(module, to, NULL);
    const ZirType *source = FindType(module, from, NULL);
    if(target != NULL && target == source) return 1;
    char to_element[ZIR_NAME_MAX], from_element[ZIR_NAME_MAX];
    int to_capacity, from_capacity;
    if(ArrayElementType(to, to_element, sizeof(to_element), &to_capacity) &&
       ArrayElementType(from, from_element, sizeof(from_element),
                        &from_capacity) &&
       to_capacity >= 0 && to_capacity == from_capacity)
        return same_declared_type(module, to_element, from_element,
                                  depth + 1);
    if(*to == '*' && *from == '*')
        return same_declared_type(module, skip_ws(to + 1),
                                  skip_ws(from + 1), depth + 1);
    return 0;
}

static int
compatible_checked(Checker *c, const char *to, const char *from)
{
    if(flags_type(c, to) != NULL && integer_type(from)) return 1;
    return compatible(to, from) ||
           same_declared_type(c->module, to, from, 0);
}

static int
text_type(const char *type)
{
    return !strcmp(type, "string");
}

static int
assignable(Checker *c, int index)
{
    const ZirExpr *e;
    if(index < 0 || index >= c->fn->expr_count) return 0;
    e = &c->fn->exprs[index];
    return (e->kind == ZIR_EXPR_IDENT && !e->is_this &&
            strcmp(e->name, "true") &&
            strcmp(e->name, "false") && strcmp(e->name, "null")) ||
           e->kind == ZIR_EXPR_INDEX || e->kind == ZIR_EXPR_MEMBER ||
           e->kind == ZIR_EXPR_POINTER_MEMBER || (e->kind == ZIR_EXPR_UNARY && !strcmp(e->op, "*"));
}

/* Collection counts and borrowed string bytes are read-only views. */
static int
readonly_text_destination(Checker *c, int index)
{
    const ZirFunction *fn = c->fn;
    const ZirExpr *e;
    if(index < 0 || index >= fn->expr_count)
        return 0;
    e = &fn->exprs[index];
    if(e->kind == ZIR_EXPR_MEMBER &&
       (!strcmp(e->name, "count") || !strcmp(e->name, "capacity")) &&
       VecElementType(c->module, fn->exprs[e->left].type, NULL, 0))
        return 1;
    if(e->kind == ZIR_EXPR_MEMBER && !strcmp(e->name, "count")) {
        const char *base = fn->exprs[e->left].type;
        if(!strcmp(base, "string") ||
           SliceElementType(base, NULL, 0) ||
           ArrayElementType(base, NULL, 0, NULL)) return 1;
    }
    if(e->kind == ZIR_EXPR_INDEX || e->kind == ZIR_EXPR_MEMBER) {
        if(fn->exprs[e->left].kind == ZIR_EXPR_IDENT &&
           !strcmp(fn->exprs[e->left].type, "string"))
            return 1;
        return readonly_text_destination(c, e->left);
    }
    return 0;
}

static int check_function(Checker *c, ZirFunction *fn);

/* Function values require a slot context; ordinary names retain lexical lookup.
 * Annotate before recursively checking expressions so a declaration identifier
 * is not mistaken for an unresolved variable. */
static void
contextual_slot(Checker *c, int index, const char *expected)
{
    const ZirModule *slot_owner = NULL;
    const ZirType *slot = FindType(c->module, expected, &slot_owner);
    if(index < 0 || slot == NULL || !slot->is_procedure_type)
        return;
    ZirExpr *value = &c->fn->exprs[index];
    if(value->kind == ZIR_EXPR_CONDITIONAL) {
        contextual_slot(c, value->right, expected);
        contextual_slot(c, value->third, expected);
        return;
    }
    if(value->kind != ZIR_EXPR_IDENT ||
       (!value->is_this && *lookup(c, value->name)))
        return;
    const ZirModule *owner = NULL;
    const ZirFunction *declaration = NULL;
    int resolved = ResolveFunction(c->module, value->name, &owner, &declaration);
    int matches = resolved == 1 && !declaration->is_extern;
    if(matches) {
        const char *actual_scalar = ScalarType(declaration->return_type);
        const char *wanted_scalar = ScalarType(slot->procedure_return_type);
        if(*actual_scalar || *wanted_scalar) {
            matches = !strcmp(actual_scalar, wanted_scalar);
        } else {
            const ZirType *actual_record = FindType(owner, declaration->return_type, NULL);
            const ZirType *wanted_record = FindType(slot_owner, slot->procedure_return_type, NULL);
            matches = actual_record || wanted_record ? actual_record == wanted_record :
                !strcmp(declaration->return_type, slot->procedure_return_type);
        }
    }
    char actual[64][ZIR_TEXT_MAX], wanted[64][ZIR_TEXT_MAX];
    int actual_count = matches && *skip_ws(declaration->args) ?
        split_top_level(declaration->args, actual[0], 64, sizeof(actual[0])) : 0;
    int wanted_count = *skip_ws(slot->body) ?
        split_top_level(slot->body, wanted[0], 64, sizeof(wanted[0])) : 0;
    matches &= actual_count == wanted_count;
    for(int i = 0; matches && i < wanted_count; i++) {
        const char *actual_type = strchr(actual[i], ':');
        const char *wanted_type = strchr(wanted[i], ':');
        if(actual_type == NULL || wanted_type == NULL) {
            matches = 0;
            break;
        }
        actual_type = skip_ws(actual_type + 1);
        wanted_type = skip_ws(wanted_type + 1);
        const char *actual_scalar = ScalarType(actual_type);
        const char *wanted_scalar = ScalarType(wanted_type);
        if(*actual_scalar || *wanted_scalar) {
            matches = !strcmp(actual_scalar, wanted_scalar);
        } else {
            const ZirType *actual_record = FindType(owner, actual_type, NULL);
            const ZirType *wanted_record = FindType(slot_owner, wanted_type, NULL);
            matches = actual_record || wanted_record ? actual_record == wanted_record :
                !strcmp(actual_type, wanted_type);
        }
    }
    if(!matches) {
        Diagnostic(value->span, "check.slot_signature",
                      "function does not match slot signature %s: %s", expected, value->name);
        c->errors++;
        c->failed = 1;
        return;
    }
    value->is_function_value = 1;
    copy_text(value->type, sizeof(value->type), expected);
}

int
TypeOfOperand(const char *source, char *operand, size_t capacity)
{
    const char *open, *close = NULL;
    int depth = 0, quote = 0;
    source = skip_ws(source);
    if(strncmp(source, "type_of", 7) != 0 ||
       (isalnum((unsigned char)source[7]) || source[7] == '_'))
        return 0;
    open = skip_ws(source + 7);
    if(*open != '(') return 0;
    for(const char *p = open; *p; p++) {
        if(quote) {
            if(*p == '\\' && p[1]) p++;
            else if(*p == quote) quote = 0;
        } else if(*p == '"' || *p == '\'') {
            quote = *p;
        } else if(*p == '(') {
            depth++;
        } else if(*p == ')' && --depth == 0) {
            close = p;
            break;
        }
    }
    if(close == NULL || *skip_ws(close + 1) != '\0' ||
       (size_t)(close - open - 1) >= capacity)
        return 0;
    snprintf(operand, capacity, "%.*s", (int)(close - open - 1), open + 1);
    trim_in_place(operand);
    return operand[0] != '\0';
}

static char *
find_unquoted_expression(char *source, const char *needle)
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
        } else if(strncmp(p, needle, length) == 0) {
            return p;
        } else {
            p++;
        }
    }
    return NULL;
}

static int
replace_checked_text(char *source, size_t capacity, const char *needle,
                     const char *replacement)
{
    char rewritten[ZIR_TEXT_MAX];
    char *at = find_unquoted_expression(source, needle);
    int written;
    if(at == NULL) return 0;
    written = snprintf(rewritten, sizeof(rewritten), "%.*s%s%s",
                       (int)(at - source), source, replacement,
                       at + strlen(needle));
    if(written < 0 || (size_t)written >= sizeof(rewritten) ||
       (size_t)written >= capacity) return 0;
    copy_text(source, capacity, rewritten);
    return 1;
}

static int
rewrite_checked_text(Checker *c, const ZirExpr *expr, const char *replacement)
{
    char needle[ZIR_TEXT_MAX];
    if(c->current_stmt == NULL || c->fn->from_ir) return 1;
    copy_text(needle, sizeof(needle), expr->text);
    if(!replace_checked_text(c->current_stmt->text,
                             sizeof(c->current_stmt->text), needle, replacement))
        return 0;
    for(int i = 0; i < c->fn->expr_count; i++) {
        ZirExpr *candidate = &c->fn->exprs[i];
        if(candidate != expr &&
           candidate->span.line == expr->span.line &&
           candidate->kind != ZIR_EXPR_STRING &&
           candidate->kind != ZIR_EXPR_SIZE_OF &&
           candidate->kind != ZIR_EXPR_IDENT &&
           candidate->kind != ZIR_EXPR_INT &&
           candidate->kind != ZIR_EXPR_FLOAT &&
           candidate->kind != ZIR_EXPR_MEMBER &&
           candidate->kind != ZIR_EXPR_POINTER_MEMBER &&
           find_unquoted_expression(candidate->text, needle) != NULL &&
           !replace_checked_text(candidate->text, sizeof(candidate->text),
                                 needle, replacement))
            return 0;
    }
    return 1;
}

static int
lower_enum_reference(Checker *c, ZirExpr *expr, const ZirType *enumeration,
                     const char *member)
{
    int64_t value;
    char replacement[64];
    if(!EnumMemberValue(enumeration, member, &value)) {
        error(c, expr->span, "unknown enum member", member);
        return 0;
    }
    snprintf(replacement, sizeof(replacement), "%lld", (long long)value);
    if(!rewrite_checked_text(c, expr, replacement)) {
        error(c, expr->span, "cannot lower enum member", expr->text);
        return 0;
    }
    copy_text(expr->text, sizeof(expr->text), replacement);
    expr->name[0] = '\0';
    expr->kind = ZIR_EXPR_INT;
    expr->left = expr->right = expr->third = -1;
    copy_text(expr->type, sizeof(expr->type), enumeration->name);
    return 1;
}

static int rewrite_type_applications(ZirModule *module, const char *source,
                                     char *output, size_t capacity,
                                     ZirSourceSpan span, int recursion);
static int canonical_type_arguments(const char *source, char *output,
                                    size_t capacity);

/* VecPop and VecGet return Option(element). Find or register the concrete
 * application under the same deterministic name the declaration rewriter
 * produces, so source and saved IR agree without re-running the rewriter on
 * loaded graphs. */
static int
vec_option_result_type(Checker *c, const char *element, ZirSourceSpan span,
                       char *out, size_t size)
{
    char canonical[ZIR_TEXT_MAX];
    char name[ZIR_NAME_MAX];
    uint64_t hash = UINT64_C(14695981039346656037);
    if(!canonical_type_arguments(element, canonical, sizeof(canonical))) {
        error(c, span, "Vec result element type is too long", element);
        return 0;
    }
    for(const unsigned char *p = (const unsigned char *)"Option"; *p; p++)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    hash = (hash ^ '(') * UINT64_C(1099511628211);
    for(const unsigned char *p = (const unsigned char *)canonical; *p; p++)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    snprintf(name, sizeof(name), "__type_%016llx", (unsigned long long)hash);
    /* Saved IR carries the instantiated record directly; the Option template
     * and its module may have been pruned by linking. */
    for(int t = 0; t < c->module->type_count; t++)
        if(strcmp(c->module->types[t].name, name) == 0) {
            copy_text(out, size, name);
            return 1;
        }
    const ZirType *generic = FindType(c->module, "Option", NULL);
    char parameter[ZIR_NAME_MAX] = "";
    const char *cursor, *dollar;
    size_t length = 0;
    size_t offset = 0;
    ZirTypeField field;
    if(generic == NULL || !generic->is_record_template) {
        error(c, span, "Vec result requires the Option record; import option",
              element);
        return 0;
    }
    cursor = skip_ws(generic->template_params);
    dollar = *cursor == '$' ? cursor + 1 : cursor;
    while((isalnum((unsigned char)dollar[length]) || dollar[length] == '_') &&
          length + 1 < sizeof(parameter)) {
        parameter[length] = dollar[length];
        length++;
    }
    parameter[length] = '\0';
    if(TypeNextField(generic, &offset, &field) != 1 ||
       strcmp(field.name, "has_value") || strcmp(field.type, "bool") ||
       TypeNextField(generic, &offset, &field) != 1 ||
       strcmp(field.name, "value") || strcmp(field.type, parameter) ||
       TypeNextField(generic, &offset, &field) != 0) {
        error(c, span, "Vec result requires Option fields has_value and value",
              generic->name);
        return 0;
    }
    ZirType *instance = ModuleAddType(c->module, name, span);
    if(instance == NULL) {
        error(c, span, "cannot register Vec result record", name);
        return 0;
    }
    instance->is_public = generic->is_public;
    instance->is_file_private = generic->is_file_private;
    instance->is_type_instance = 1;
    instance->is_synthetic_application = 1;
    copy_text(instance->template_name, sizeof(instance->template_name),
              "Option");
    copy_text(instance->template_args, sizeof(instance->template_args),
              canonical);
    generic = FindType(c->module, "Option", NULL);
    instance = NULL;
    for(int t = 0; t < c->module->type_count; t++)
        if(strcmp(c->module->types[t].name, name) == 0) {
            instance = &c->module->types[t];
            break;
        }
    if(instance == NULL || generic == NULL ||
       !InstantiateGenericRecord(instance, generic)) {
        error(c, span, "cannot instantiate Vec result record", name);
        return 0;
    }
    instance = NULL;
    for(int t = 0; t < c->module->type_count; t++)
        if(strcmp(c->module->types[t].name, name) == 0) {
            instance = &c->module->types[t];
            break;
        }
    if(instance != NULL && instance->body[0]) {
        char expanded[sizeof(instance->body)];
        if(!rewrite_type_applications(c->module, instance->body, expanded,
                                      sizeof(expanded), span, 0)) {
            error(c, span, "cannot expand Vec result record", name);
            return 0;
        }
        copy_text(instance->body, sizeof(instance->body), expanded);
    }
    copy_text(out, size, name);
    return 1;
}

static const char *try_conversion(Checker *c, int index, const char *to,
                                  ZirSourceSpan span);

static int
reserve_compound_constants(const ZirModule *module, ZirFunction *fn)
{
    size_t needed = (size_t)fn->expr_count;
    for(int i = 0; i < fn->expr_count; i++) {
        const ZirExpr *node = &fn->exprs[i];
        char name[ZIR_NAME_MAX];
        if(node->kind == ZIR_EXPR_IDENT)
            copy_text(name, sizeof(name), node->name);
        else if(node->kind == ZIR_EXPR_MEMBER && node->left >= 0 &&
                fn->exprs[node->left].kind == ZIR_EXPR_IDENT) {
            int written = snprintf(name, sizeof(name), "%s.%s",
                fn->exprs[node->left].name, node->name);
            if(written < 0 || (size_t)written >= sizeof(name)) continue;
        } else continue;
        CompoundConstant compound = {0};
        if(bound_compound_constant(module, name, 0,
                                   &compound) != 1)
            continue;
        ZirFunction probe = {0};
        int root = ParseExpr(&probe, compound.owner,
                             compound.literal, node->span);
        if(root >= 0 && probe.expr_count > 0)
            needed += (size_t)probe.expr_count - 1;
        free(probe.exprs);
        if(needed > INT_MAX || needed > SIZE_MAX / sizeof(*fn->exprs))
            return 0;
    }
    if(needed <= (size_t)fn->expr_cap) return 1;
    ZirExpr *expanded = realloc(fn->exprs, needed * sizeof(*expanded));
    if(expanded == NULL) return 0;
    fn->exprs = expanded;
    fn->expr_cap = (int)needed;
    return 1;
}

static int
inline_compound_constant(Checker *c, int index,
                         const CompoundConstant *compound)
{
    ZirFunction probe = {0};
    int root = ParseExpr(&probe, compound->owner, compound->literal,
                         c->fn->exprs[index].span);
    if(root < 0 || probe.exprs[root].kind != ZIR_EXPR_COMPOUND ||
       c->fn->expr_count + probe.expr_count - 1 > c->fn->expr_cap) {
        free(probe.exprs);
        return 0;
    }
    for(int i = 0; i < probe.expr_count; i++) {
        ZirExpr *node = &probe.exprs[i];
        if(node->kind != ZIR_EXPR_COMPOUND) continue;
        char qualified[ZIR_NAME_MAX];
        if(!compound_type_at_use(c->module, compound, node->name,
                                 qualified, sizeof(qualified))) {
            free(probe.exprs);
            return 0;
        }
        copy_text(node->name, sizeof(node->name), qualified);
    }
    int *map = malloc((size_t)probe.expr_count * sizeof(*map));
    if(map == NULL) {
        free(probe.exprs);
        return 0;
    }
    ZirExpr previous = c->fn->exprs[index];
    for(int i = 0; i < probe.expr_count; i++)
        map[i] = i == root ? index : c->fn->expr_count++;
    for(int i = 0; i < probe.expr_count; i++)
        c->fn->exprs[map[i]] = probe.exprs[i];
    for(int i = 0; i < probe.expr_count; i++) {
        ZirExpr *node = &c->fn->exprs[map[i]];
        if(node->left >= 0) node->left = map[node->left];
        if(node->right >= 0) node->right = map[node->right];
        if(node->third >= 0) node->third = map[node->third];
        if(node->first_child >= 0)
            node->first_child = map[node->first_child];
        if(node->next_sibling >= 0)
            node->next_sibling = map[node->next_sibling];
    }
    ZirExpr *replacement = &c->fn->exprs[index];
    replacement->next_sibling = previous.next_sibling;
    replacement->argument_index = previous.argument_index;
    copy_text(replacement->argument_name,
              sizeof(replacement->argument_name), previous.argument_name);
    replacement->span = previous.span;
    c->aggregate_rewritten = 1;
    free(map);
    free(probe.exprs);
    return 1;
}

static const char *
expression_type(Checker *c, int index)
{
    ZirExpr *e;
    const char *type = "", *left = "", *right = "";
    char member_type[ZIR_NAME_MAX] = "";
    if(index < 0 || index >= c->fn->expr_count) return "";
    e = &c->fn->exprs[index];
    if(e->is_this && e->kind != ZIR_EXPR_IDENT && e->kind != ZIR_EXPR_CALL) {
        error(c, e->span, "invalid #this expression", e->name);
        return "";
    }
    if(e->kind == ZIR_EXPR_CALL && e->type[0]) {
        const char *unqualified = strrchr(e->name, '.');
        unqualified = unqualified == NULL ? e->name : unqualified + 1;
        for(int request = 0; request < c->specialization_count; request++)
            if(!strcmp(unqualified, c->specializations[request].name))
                return e->type;
    }
    if(e->kind == ZIR_EXPR_MEMBER && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_MEMBER) {
        const ZirExpr *middle = &c->fn->exprs[e->left];
        if(middle->left >= 0 &&
           c->fn->exprs[middle->left].kind == ZIR_EXPR_IDENT) {
            const char *alias = c->fn->exprs[middle->left].name;
            char qualified[ZIR_NAME_MAX];
            int length = snprintf(qualified, sizeof(qualified), "%s.%s",
                                  alias, middle->name);
            if(!*lookup_lexical(c, alias) && length >= 0 &&
               (size_t)length < sizeof(qualified)) {
                const ZirType *enumeration = FindType(c->module, qualified,
                                                      NULL);
                if(enumeration != NULL && enumeration->is_enum) {
                    if(lower_enum_reference(c, e, enumeration, e->name)) {
                        copy_text(e->type, sizeof(e->type), qualified);
                        return e->type;
                    }
                    return "";
                }
            }
        }
    }
    if(e->kind == ZIR_EXPR_MEMBER && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_IDENT) {
        const char *alias = c->fn->exprs[e->left].name;
        for(int i = 0; i < c->module->import_count; i++) {
            const ZirImport *import = &c->module->imports[i];
            if(import->kind != ZIR_IMPORT_MODULE ||
               !in_lookup_file(c->module, import->is_file_private,
                               import->span) ||
               strcmp(import->name, alias) != 0)
                continue;
            char qualified[ZIR_NAME_MAX];
            int length = snprintf(qualified, sizeof(qualified), "%s.%s",
                                  alias, e->name);
            if(length < 0 || (size_t)length >= sizeof(qualified)) {
                error(c, e->span, "qualified name is too long", alias);
                return "";
            }
            e->kind = ZIR_EXPR_IDENT;
            e->left = -1;
            copy_text(e->name, sizeof(e->name), qualified);
            break;
        }
    }
    if(e->kind == ZIR_EXPR_MEMBER && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_IDENT) {
        const ZirType *enumeration = FindType(c->module,
            c->fn->exprs[e->left].name, NULL);
        if(enumeration != NULL && enumeration->is_enum) {
            if(lower_enum_reference(c, e, enumeration, e->name))
                return e->type;
            return "";
        }
    }
    if(e->kind == ZIR_EXPR_IDENT && e->name[0] == '.') {
        const ZirType *enumeration = FindType(c->module, c->expected_type, NULL);
        if(enumeration == NULL || !enumeration->is_enum) {
            error(c, e->span, "inferred enum member needs an enum type", e->name);
            return "";
        }
        if(lower_enum_reference(c, e, enumeration, e->name + 1))
            return e->type;
        return "";
    }
    if(e->kind == ZIR_EXPR_CONDITIONAL && !strcmp(e->op, "#ifx")) {
        error(c, e->span, "unlowered #ifx expression", e->text);
        return "";
    }
    if(e->kind == ZIR_EXPR_SIZE_OF) {
        size_t size, alignment;
        char operand[ZIR_TEXT_MAX], resolved[ZIR_NAME_MAX];
        const char *sized_type = e->name;
        int from_value = TypeOfOperand(e->name, operand, sizeof(operand));
        if(!from_value && !JaiTypeSpelling(e->span, e->name)) {
            c->errors++;
            return "";
        }
        if(from_value) {
            ZirFunction probe = {0};
            ZirFunction *saved_fn = c->fn;
            ZirStmt *saved_stmt = c->current_stmt;
            int root = ParseExpr(&probe, c->module, operand, e->span);
            c->fn = &probe;
            c->current_stmt = NULL;
            const char *inferred = expression_type(c, root);
            copy_text(resolved, sizeof(resolved),
                      !strcmp(inferred, "integer") ? "s64" :
                      !strcmp(inferred, "real") ? "float64" : inferred);
            c->fn = saved_fn;
            c->current_stmt = saved_stmt;
            free(probe.exprs);
            sized_type = resolved;
        }
        if(!TypeLayout(c->module, sized_type, &size, &alignment)) {
            error(c, e->span, "size_of requires a known sized type", e->name);
            return "";
        }
        char replacement[ZIR_TEXT_MAX];
        snprintf(replacement, sizeof(replacement), "size_of(%s)", sized_type);
        if(!rewrite_checked_text(c, e, replacement)) {
            error(c, e->span, "cannot lower size_of expression", e->text);
            return "";
        }
        if(!c->fn->from_ir)
            copy_text(e->text, sizeof(e->text), replacement);
        if(sized_type != e->name)
            copy_text(e->name, sizeof(e->name), sized_type);
        e->left = e->right = e->third = -1;
        copy_text(e->type, sizeof(e->type), "integer");
        return e->type;
    }
    if(e->is_function_value)
        return e->type;
    if(e->kind == ZIR_EXPR_CALL && e->name[0] == '\0' && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_MEMBER) {
        const ZirExpr *member = &c->fn->exprs[e->left];
        if(member->left >= 0 &&
           c->fn->exprs[member->left].kind == ZIR_EXPR_IDENT) {
            const char *alias = c->fn->exprs[member->left].name;
            for(int i = 0; i < c->module->import_count; i++) {
                const ZirImport *import = &c->module->imports[i];
                if(import->kind != ZIR_IMPORT_MODULE ||
                   !in_lookup_file(c->module, import->is_file_private,
                                   import->span) ||
                   strcmp(import->name, alias) != 0)
                    continue;
                int length = snprintf(e->name, sizeof(e->name),
                                      "%s.%s", alias, member->name);
                if(length < 0 || (size_t)length >= sizeof(e->name))
                    error(c, e->span, "qualified call name is too long", alias);
                else
                    e->left = -1;
                break;
            }
        }
    }
    if(e->left >= 0 && !(e->kind == ZIR_EXPR_CALL && e->name[0]))
        left = expression_type(c, e->left);
    if(e->right >= 0) {
        char saved_expected[ZIR_NAME_MAX];
        copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
        if(e->kind == ZIR_EXPR_BINARY) {
            const ZirType *enumeration = FindType(c->module, left, NULL);
            if(enumeration != NULL && enumeration->is_enum)
                copy_text(c->expected_type, sizeof(c->expected_type), left);
        }
        right = expression_type(c, e->right);
        copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
    }
    switch(e->kind) {
    case ZIR_EXPR_COMPOUND: {
        if(!e->name[0]) {
            if(!c->expected_type[0]) {
                error(c, e->span,
                      "inferred record literal needs a record type", e->text);
                break;
            }
            copy_text(e->name, sizeof(e->name), c->expected_type);
        }
        if(SliceElementType(e->name, NULL, 0)) {
            error(c, e->span, "slice literals require a backing range", e->name);
            break;
        }
        if(e->name[0] == '[') {
            char element[ZIR_NAME_MAX];
            int capacity = 0;
            const char *problem = local_storage_error(c->module, e->name);
            if(problem != NULL) {
                error(c, e->span, problem, e->name);
                break;
            }
            normalize_array(c->module, e->name, sizeof(e->name));
            ArrayElementType(e->name, element, sizeof(element), &capacity);
            if(capacity < 0)
                error(c, e->span, "array literals require a resolved capacity", e->name);
            int count = 0;
            for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
                ZirExpr *entry = &c->fn->exprs[child];
                if(!strcmp(entry->op, "="))
                    error(c, entry->span, "array literals require positional elements", entry->name);
                char saved_expected[ZIR_NAME_MAX];
                copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
                copy_text(c->expected_type, sizeof(c->expected_type), element);
                const char *value_type = expression_type(c, entry->right);
                copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
                if(!compatible_checked(c, element, value_type))
                    error(c, entry->span, "array initializer element type mismatch", element);
                copy_text(entry->type, sizeof(entry->type), element);
                count++;
            }
            if(capacity >= 0 && count > capacity)
                error(c, e->span, "too many array initializer elements", e->name);
            type = e->name;
            break;
        }
        const ZirModule *record_owner = NULL;
        const ZirType *record = FindType(c->module, e->name, &record_owner);
        int ordinal = 0;
        int mode = -1;
        if(record == NULL || record->is_enum || record->is_procedure_type ||
           record->is_record_template) {
            error(c, e->span, "initializer requires a declared record type", e->name);
            break;
        }
        for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
            ZirExpr *entry = &c->fn->exprs[child];
            int named = !strcmp(entry->op, "=");
            if(mode >= 0 && mode != named)
                error(c, entry->span, "cannot mix named and positional record fields", e->name);
            mode = named;
            size_t offset = 0;
            ZirTypeField field;
            int position = 0;
            int found = 0;
            while(TypeNextField(record, &offset, &field) == 1) {
                if(named ? !strcmp(field.name, entry->name) : position == ordinal) {
                    found = 1;
                    break;
                }
                position++;
            }
            if(found) {
                normalize_array(record_owner, field.type, sizeof(field.type));
                if(!record_field_type_at_use(c->module, record_owner,
                                             e->name, field.type,
                                             sizeof(field.type)))
                    error(c, entry->span,
                          "imported record field type is shadowed",
                          field.name);
                ZirExpr *initializer = &c->fn->exprs[entry->right];
                if(initializer->kind == ZIR_EXPR_COMPOUND)
                    normalize_array(record_owner, initializer->name, sizeof(initializer->name));
            }
            char saved_expected[ZIR_NAME_MAX];
            copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
            if(found)
                copy_text(c->expected_type, sizeof(c->expected_type), field.type);
            const char *value_type = expression_type(c, entry->right);
            copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
            if(!found) {
                error(c, entry->span, named ? "unknown initializer field" : "too many positional record fields", entry->name);
                ordinal++;
                continue;
            }
            for(int previous = e->first_child; previous != child; previous = c->fn->exprs[previous].next_sibling) {
                if(!strcmp(c->fn->exprs[previous].name, field.name))
                    error(c, entry->span, "duplicate initializer field", field.name);
            }
            copy_text(entry->name, sizeof(entry->name), field.name);
            copy_text(entry->type, sizeof(entry->type), field.type);
            if(!compatible_checked(c, field.type, value_type))
                error(c, entry->span, "initializer field type mismatch", field.name);
            if(contains_vec(c->module, field.type, 0) &&
               !owned_initializer_shape(c, entry->right))
                error(c, entry->span,
                      "owned record field moves a binding or takes a call result",
                      field.name);
            ordinal++;
        }
        type = e->name;
        break;
    }
    case ZIR_EXPR_MEMBER:
    case ZIR_EXPR_POINTER_MEMBER: {
        char record_name[ZIR_NAME_MAX] = "";
        if(e->kind == ZIR_EXPR_MEMBER && left[0] == '*') {
            e->kind = ZIR_EXPR_POINTER_MEMBER;
            copy_text(e->op, sizeof(e->op), "->");
        }
        if(e->kind == ZIR_EXPR_POINTER_MEMBER) {
            const char *base = skip_ws(left);
            if(!strncmp(base, "const ", 6))
                base = skip_ws(base + 6);
            if(*base == '*') {
                base = skip_ws(base + 1);
                if(!*base || strlen(base) >= sizeof(record_name)) {
                    error(c, e->span, "pointer member requires a record pointer", left);
                    break;
                }
                copy_text(record_name, sizeof(record_name), base);
            } else {
                const char *star = strchr(base, '*');
                if(star == NULL || *skip_ws(star + 1) ||
                   star == base || (size_t)(star - base) >= sizeof(record_name)) {
                    error(c, e->span, "pointer member requires a record pointer", left);
                    break;
                }
                size_t length = (size_t)(star - base);
                while(length > 0 && isspace((unsigned char)base[length - 1]))
                    length--;
                memcpy(record_name, base, length);
                record_name[length] = '\0';
            }
        } else {
            copy_text(record_name, sizeof(record_name), left);
        }
        const ZirModule *record_owner = NULL;
        const ZirType *record = FindType(c->module, record_name, &record_owner);
        if(e->kind == ZIR_EXPR_MEMBER && !strcmp(e->name, "length") &&
           (!strcmp(left, "string") || SliceElementType(left, NULL, 0) ||
            ArrayElementType(left, NULL, 0, NULL))) {
            error(c, e->span, "length is not Jai syntax; use count", e->name);
            break;
        }
        if(e->kind == ZIR_EXPR_MEMBER && record == NULL &&
           (!strcmp(left, "string") || SliceElementType(left, NULL, 0) ||
            ArrayElementType(left, NULL, 0, NULL))) {
            if(!strcmp(e->name, "count")) {
                type = "s64";
                break;
            }
            char element[ZIR_NAME_MAX];
            if(!strcmp(e->name, "data") &&
               ArrayElementType(left, element, sizeof(element), NULL)) {
                snprintf(member_type, sizeof(member_type), "*%s", element);
                type = member_type;
                break;
            }
        }
        if(VecElementType(c->module, record_name, NULL, 0) &&
           strcmp(e->name, "count") && strcmp(e->name, "capacity")) {
            error(c, e->span, "Vec storage is private", e->name);
            break;
        }
        if(record != NULL && !record->is_enum) {
            if(strchr(e->name, '.') != NULL) {
                if(!RecordFieldPathType(record_owner, record, e->name,
                                        member_type, sizeof(member_type)))
                    error(c, e->span, "invalid promoted record field path",
                          e->name);
            } else {
                char path[ZIR_NAME_MAX];
                int found = ResolveRecordField(record_owner, record, e->name,
                                               path, sizeof(path), member_type,
                                               sizeof(member_type));
                if(found < 0)
                    error(c, e->span, "ambiguous using record field", e->name);
                else if(found > 0)
                    copy_text(e->name, sizeof(e->name), path);
            }
            if(*member_type) {
                normalize_array(record_owner, member_type, sizeof(member_type));
                if(!record_field_type_at_use(c->module, record_owner,
                                             record_name, member_type,
                                             sizeof(member_type)))
                    error(c, e->span,
                          "imported record field type is shadowed",
                          e->name);
            }
        }
        if(!*member_type)
            error(c, e->span, "unknown record field", e->name);
        type = member_type;
        check_moved_path_use(c, index);
        break;
    }
    case ZIR_EXPR_SLICE: {
        char element[ZIR_NAME_MAX];
        int array = ArrayElementType(left, element, sizeof(element), NULL);
        int string = !strcmp(left, "string");
        if(!string && !array && !SliceElementType(left, element, sizeof(element))) {
            error(c, e->span, "slice source requires a string, array, or slice", e->text);
            break;
        }
        if(array && !assignable(c, e->left))
            error(c, e->span, "slice source requires persistent array storage", e->text);
        if(!string && ((!strcmp(element, "char") || !strcmp(element, "const char")) || element[0] == '['))
            error(c, e->span, "unsupported slice element type", element);
        if(e->right >= 0 && !integer_type(right))
            error(c, e->span, "slice lower bound requires an integer", e->text);
        if(e->third >= 0 && !integer_type(expression_type(c, e->third)))
            error(c, e->span, "slice upper bound requires an integer", e->text);
        if(string) type = "string";
        else {
            snprintf(member_type, sizeof(member_type), "[]%s", element);
            type = member_type;
        }
        break;
    }
    case ZIR_EXPR_INDEX: {
        char element[ZIR_NAME_MAX];

        /* Native pointer indexing follows the same element type as a
         * borrowed array. Portable bundles reject reachable raw pointers. */
        if(!strcmp(left, "string")) {
            if(!integer_type(right))
                error(c, e->span, "string index requires an integer operand", e->text);
            copy_text(element, sizeof(element), "u8");
        } else if(*left == '*' && *skip_ws(left + 1) &&
                  strcmp(skip_ws(left + 1), "void")) {
            copy_text(element, sizeof(element), skip_ws(left + 1));
            if(!integer_type(right))
                error(c, e->span, "pointer index requires an integer operand", e->text);
        } else if(!ArrayElementType(left, element, sizeof(element), NULL) &&
                  !SliceElementType(left, element, sizeof(element)) &&
                  !VecElementType(c->module, left, element, sizeof(element))) {
            error(c, e->span, "index requires an array, slice, pointer, or string", e->text);
            copy_text(element, sizeof(element), "s32");
        } else if(!integer_type(right)) {
            error(c, e->span, "array index requires an integer operand", e->text);
        }
        copy_text(member_type, sizeof(member_type), element);
        type = member_type;
        break;
    }
    case ZIR_EXPR_INT: {
        const ZirType *enumeration = FindType(c->module, e->type, NULL);
        type = enumeration != NULL && enumeration->is_enum ? e->type : "integer";
        break;
    }
    case ZIR_EXPR_FLOAT: type = "real"; break;
    case ZIR_EXPR_COMPILE_TIME: type = "bool"; break;
    case ZIR_EXPR_STRING: type = "string"; break;
    case ZIR_EXPR_IDENT:
        if(e->is_this) {
            error(c, e->span, "#this needs a procedure type or call", e->name);
            break;
        }
        if(!strcmp(e->name, "true") || !strcmp(e->name, "false")) type = "bool";
        else if(!strcmp(e->name, "null")) type = "null";
        else {
            const ZirType *enumeration = NULL;
            char source_member[ZIR_NAME_MAX];
            int enum_member = !*lookup_lexical(c, e->name) &&
                              global_binding(c, e->name) == NULL ?
                resolve_using_enum(c, e->name, &enumeration, source_member,
                                   sizeof(source_member)) : 0;
            if(enum_member < 0) {
                error(c, e->span, "ambiguous using enum member", e->name);
                break;
            }
            if(enum_member == 1) {
                if(!lower_enum_reference(c, e, enumeration, source_member))
                    break;
                type = "integer";
                break;
            }
            type = lookup(c, e->name);
            e->is_global_value = !*lookup_lexical(c, e->name) &&
                                  global_binding(c, e->name) != NULL;
            if(e->is_global_value) {
                const ZirModule *owner = NULL;
                const ZirGlobal *global = NULL;
                if(ResolveGlobal(c->module, e->name,
                                 &owner, &global) == 1) {
                    int qualified = qualified_global_type(c->module,
                        e->name, owner, global->type,
                        e->type, sizeof(e->type));
                    if(qualified < 0)
                        error(c, e->span, "qualified global type is too long",
                              e->name);
                    else if(qualified > 0)
                        type = e->type;
                }
            }
            for(int i = c->count - 1; i >= 0; i--)
                if(!c->bindings[i].is_using_namespace &&
                   !strcmp(c->bindings[i].name, e->name)) {
                    c->bindings[i].touched = 1;
                    if(c->bindings[i].moved && !c->assign_destination)
                        error(c, e->span, "Vec binding is used after moving",
                              e->name);
                    break;
                }
            check_moved_path_use(c, index);
        }
        if(!*type) {
            CompoundConstant compound = {0};
            if(bound_compound_constant(c->module, e->name, 0,
                                       &compound) == 1) {
                if(!inline_compound_constant(c, index, &compound)) {
                    error(c, e->span,
                          "cannot bind aggregate constant", e->name);
                    break;
                }
                return expression_type(c, index);
            }
            /* A bare procedure name is a value when the surrounding context
             * expects a matching slot (return, assignment, argument). */
            if(c->expected_type[0] && !e->is_this &&
               !*lookup_lexical(c, e->name)) {
                const ZirType *expected_slot =
                    FindType(c->module, c->expected_type, NULL);
                const ZirModule *decl_owner = NULL;
                const ZirFunction *declaration = NULL;
                if(expected_slot != NULL && expected_slot->is_procedure_type &&
                   ResolveFunction(c->module, e->name, &decl_owner,
                                   &declaration) == 1) {
                    contextual_slot(c, index, c->expected_type);
                    if(c->failed)
                        break;
                    if(e->is_function_value) {
                        type = e->type;
                        break;
                    }
                }
            }
            char literal[ZIR_TEXT_MAX];
            int string_status = bound_string_constant(c->module, e->name, 0,
                                                       literal, sizeof(literal));
            int real_status = string_status == 0 ?
                bound_real_constant(c->module, e->name, 0,
                                    literal, sizeof(literal)) : 0;
            int64_t value = 0;
            int status = string_status == 0 && real_status == 0 ?
                bound_constant(c->module, e->name, 0, &value) :
                string_status != 0 ? string_status : real_status;
            if(string_status == 1) {
                copy_text(e->text, sizeof(e->text), literal);
                e->kind = ZIR_EXPR_STRING;
                type = "string";
            } else if(real_status == 1) {
                copy_text(e->text, sizeof(e->text), literal);
                e->kind = ZIR_EXPR_FLOAT;
                type = "real";
            } else if(status == 1) {
                snprintf(e->text, sizeof(e->text), "%lld", (long long)value);
                e->kind = ZIR_EXPR_INT;
                type = "integer";
            } else if(status < 0) {
                error(c, e->span, "invalid or ambiguous constant", e->name);
            } else {
                error(c, e->span, "unresolved name", e->name);
            }
        }
        break;
    case ZIR_EXPR_CALL: {
        char display_name[ZIR_NAME_MAX];
        copy_text(display_name, sizeof(display_name), e->name);
        if(!strcmp(e->name, "TextView")) {
            int first = e->first_child;
            if(first < 0 || c->fn->exprs[first].next_sibling >= 0 ||
               c->fn->exprs[first].argument_name[0] ||
               strcmp(expression_type(c, first), "[]u8"))
                error(c, e->span, "TextView requires one []u8 argument", e->name);
            type = "string";
            break;
        }
        if(!strcmp(e->name, "print")) {
            PrintPiece *pieces = calloc(PRINT_PIECES_MAX, sizeof(*pieces));
            int first = e->first_child, placeholders = 0, arguments = 0;
            int count;
            for(int child = first; child >= 0;
                child = c->fn->exprs[child].next_sibling) {
                const char *arg_type = expression_type(c, child);
                const char *scalar = ScalarType(arg_type);
                if(c->fn->exprs[child].argument_name[0])
                    error(c, c->fn->exprs[child].span,
                          "print has no named parameters",
                          c->fn->exprs[child].argument_name);
                if(child == first)
                    continue;
                arguments++;
                if(!strcmp(arg_type, "integer"))
                    copy_text(c->fn->exprs[child].type, ZIR_NAME_MAX, "s64");
                else if(!strcmp(arg_type, "real"))
                    copy_text(c->fn->exprs[child].type, ZIR_NAME_MAX, "float64");
                else if(scalar == NULL ||
                        (!integer_type(arg_type) &&
                         strcmp(scalar, "bool") &&
                         strcmp(scalar, "float32") &&
                         strcmp(scalar, "float64") &&
                         strcmp(scalar, "string")))
                    error(c, c->fn->exprs[child].span,
                          "print argument must be an integer, float, bool, or string",
                          arg_type);
            }
            if(pieces == NULL) {
                c->failed = 1;
                break;
            }
            if(first < 0 || c->fn->exprs[first].kind != ZIR_EXPR_STRING)
                error(c, e->span, "print requires a string literal format",
                      e->name);
            else if((count = PrintFormatPieces(c->fn->exprs[first].text,
                                               pieces, PRINT_PIECES_MAX)) < 0)
                error(c, c->fn->exprs[first].span,
                      "print format is invalid or too long", e->name);
            else {
                for(int i = 0; i < count; i++)
                    placeholders += pieces[i].is_argument;
                if(placeholders != arguments) {
                    char detail[64];
                    snprintf(detail, sizeof(detail), "%d %% for %d argument%s",
                             placeholders, arguments, arguments == 1 ? "" : "s");
                    error(c, e->span,
                          "print format placeholders do not match arguments",
                          detail);
                }
            }
            free(pieces);
            type = "void";
            break;
        }
        if(!strcmp(e->name, "VecPush") || !strcmp(e->name, "VecClear") ||
           !strcmp(e->name, "VecFree") || !strcmp(e->name, "VecSwap") ||
           !strcmp(e->name, "VecPop") || !strcmp(e->name, "VecGet") ||
           !strcmp(e->name, "VecClone") || !strcmp(e->name, "VecSlice") ||
           !strcmp(e->name, "BuilderAppend") ||
           !strcmp(e->name, "BuilderFinish")) {
            for(int child = e->first_child; child >= 0;
                child = c->fn->exprs[child].next_sibling)
                if(c->fn->exprs[child].argument_name[0])
                    error(c, c->fn->exprs[child].span,
                          "Vec operation has no named parameters",
                          c->fn->exprs[child].argument_name);
            int first = e->first_child;
            int second = first >= 0 ? c->fn->exprs[first].next_sibling : -1;
            int push = !strcmp(e->name, "VecPush");
            int swap = !strcmp(e->name, "VecSwap");
            int pop = !strcmp(e->name, "VecPop");
            int get = !strcmp(e->name, "VecGet");
            int text_append = !strcmp(e->name, "BuilderAppend");
            int text_finish = !strcmp(e->name, "BuilderFinish");
            int byte_builder = text_append || text_finish;
            char element[ZIR_NAME_MAX];
            char option_type[ZIR_NAME_MAX];
            Binding *first_binding = lexical_vec_binding(c, first);
            int first_touched = first_binding != NULL &&
                                first_binding->touched;
            const char *vector_type = expression_type(c, first);
            int clone = !strcmp(e->name, "VecClone");
            int view = !strcmp(e->name, "VecSlice");
            if(first < 0 || !VecElementType(c->module, vector_type,
                                           element, sizeof(element)) ||
               !(get || assignable(c, first)))
                error(c, e->span, "Vec operation requires mutable Vec storage",
                      e->name);
            if(byte_builder && strcmp(element, "u8"))
                error(c, e->span,
                      "string builder requires a Vec(u8) place", element);
            if(first_binding != NULL && first_binding->borrow_count > 0 &&
               !get && !view)
                error(c, e->span,
                      "cannot mutate a Vec with a live borrowed view",
                      e->name);
            if(push) {
                if(contains_vec(c->module, element, 0))
                    error(c, e->span, "nested Vec elements are not supported", element);
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0)
                    error(c, e->span, "VecPush requires a value", e->name);
                else {
                    const char *item_type = expression_type(c, second);
                    if(!compatible_checked(c, element, item_type))
                        error(c, e->span, "VecPush element type mismatch", element);
                    else if(!strcmp(item_type, "integer") ||
                            !strcmp(item_type, "real")) {
                        const char *scalar = ScalarType(element);
                        if(*scalar)
                            copy_text(c->fn->exprs[second].type,
                                      ZIR_NAME_MAX, scalar);
                    }
                }
            } else if(swap) {
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0 ||
                   !assignable(c, second) ||
                   strcmp(vector_type, expression_type(c, second)))
                    error(c, e->span, "VecSwap requires two matching Vec places", e->name);
            } else if(pop || text_finish) {
                if(second >= 0)
                    error(c, e->span, "Vec operation takes one argument", e->name);
            } else if(get) {
                const char *index_type;
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0)
                    error(c, e->span, "VecGet requires an index", e->name);
                else {
                    index_type = expression_type(c, second);
                    if(!integer_type(index_type))
                        error(c, e->span, "VecGet requires an integer index",
                              index_type);
                }
            } else if(text_append) {
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0)
                    error(c, e->span, "BuilderAppend requires text", e->name);
                else if(strcmp(expression_type(c, second), "string"))
                    error(c, e->span, "BuilderAppend requires string text",
                          expression_type(c, second));
            } else if(clone) {
                int third = second >= 0 ?
                    c->fn->exprs[second].next_sibling : -1;
                if(second < 0 || third >= 0)
                    error(c, e->span, "VecClone requires a source Vec", e->name);
                else {
                    const char *source_type = expression_type(c, second);
                    if(!VecElementType(c->module, source_type, NULL, 0) ||
                       strcmp(vector_type, source_type))
                        error(c, e->span,
                              "VecClone requires a matching Vec source",
                              e->name);
                    else {
                        Binding *destination = lexical_vec_binding(c, first);
                        if(c->fn->exprs[first].kind != ZIR_EXPR_IDENT)
                            error(c, e->span,
                                  "VecClone requires a simple binding destination",
                                  e->name);
                        else if(destination == NULL)
                            error(c, e->span,
                                  "global Vec storage cannot clone; use a local",
                                  e->name);
                        else if(destination->borrow_count > 0)
                            error(c, e->span,
                                  "cannot mutate a Vec with a live borrowed view",
                                  e->name);
                        else if(first_touched && !destination->moved)
                            error(c, e->span,
                                  "clone destination must be fresh or moved-from",
                                  destination->name);
                    }
                }
            } else if(view) {
                int third = second >= 0 ?
                    c->fn->exprs[second].next_sibling : -1;
                Binding *source = lexical_vec_binding(c, first);
                if(source == NULL)
                    error(c, e->span,
                          "VecSlice requires a local Vec binding", e->name);
                if(second < 0 || third < 0 ||
                   c->fn->exprs[third].next_sibling >= 0)
                    error(c, e->span, "VecSlice requires low and high bounds",
                          e->name);
                else {
                    if(!integer_type(expression_type(c, second)) ||
                       !integer_type(expression_type(c, third)))
                        error(c, e->span,
                              "VecSlice requires integer bounds", e->name);
                }
            } else if(second >= 0)
                error(c, e->span, "Vec operation takes one argument", e->name);
            if(pop) {
                if(vec_option_result_type(c, element, e->span, option_type,
                                           sizeof(option_type)))
                    type = option_type;
                else
                    type = "";
            } else if(get) {
                if(vec_option_result_type(c, element, e->span, option_type,
                                           sizeof(option_type)))
                    type = option_type;
                else
                    type = "";
            } else if(view) {
                if(snprintf(c->vec_slice_type, sizeof(c->vec_slice_type),
                            "[]%s", element) < (int)sizeof(c->vec_slice_type))
                    type = c->vec_slice_type;
                else
                    type = "";
            } else if(push || text_append || clone)
                type = "bool";
            else if(text_finish)
                type = "string";
            else
                type = "void";
            break;
        }
        const char *binding = e->is_this ? "" : lookup(c, e->name);
        e->is_global_value = !e->is_this && !*lookup_lexical(c, e->name) &&
                              global_binding(c, e->name) != NULL;
        const ZirType *slot = FindType(c->module, binding, NULL);
        if(slot != NULL && !slot->is_procedure_type)
            slot = NULL;
        copy_text(e->slot_type, sizeof(e->slot_type), slot ? binding : "");
        const ZirFunction *callee = *binding ? NULL : function(c, e->name, e->span);
        const ZirModule *callee_owner = NULL;
        if(callee != NULL) {
            const ZirFunction *resolved = NULL;
            if(ResolveFunctionAt(c->module, e->name, e->span.path,
                                 &callee_owner, &resolved) != 1 ||
               resolved != callee) callee_owner = NULL;
        }
        char specialized_args[ZIR_TEXT_MAX] = "";
        char specialized_return[ZIR_NAME_MAX] = "";
        if(callee != NULL && callee->is_template) {
            const ZirModule *owner = NULL;
            const ZirFunction *resolved = NULL;
            if(ResolveFunction(c->module, e->name, &owner, &resolved) != 1 ||
               resolved != callee || owner == NULL) {
                error(c, e->span, "cannot resolve polymorphic procedure", e->name);
                break;
            }
            char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
            if(parameters == NULL) { c->failed = 1; break; }
            int parameter_count = *skip_ws(callee->args) ?
                split_top_level(callee->args, parameters[0], 64,
                                sizeof(parameters[0])) : 0;
            if(!bind_call_arguments(c, e, parameters, parameter_count,
                                    display_name, callee->default_args)) {
                free(parameters);
                break;
            }
            char concrete[ZIR_NAME_MAX] = "";
            for(int child = e->first_child; child >= 0;
                child = c->fn->exprs[child].next_sibling) {
                int argument = c->fn->exprs[child].argument_index;
                const char *colon = strchr(parameters[argument], ':');
                if(colon == NULL) continue;
                const char *parameter_type = skip_ws(colon + 1);
                if(*parameter_type != '$' ||
                   strcmp(parameter_type + 1, callee->template_param))
                    continue;
                char saved_expected[ZIR_NAME_MAX];
                copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
                if(concrete[0])
                    copy_text(c->expected_type, sizeof(c->expected_type), concrete);
                const char *actual_type = expression_type(c, child);
                copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
                if(!strcmp(actual_type, "integer") || !strcmp(actual_type, "real")) {
                    if(concrete[0]) actual_type = concrete;
                    else if(!strcmp(callee->return_type, callee->template_param) &&
                            ScalarType(saved_expected)[0])
                        actual_type = saved_expected;
                    else {
                        error(c, c->fn->exprs[child].span,
                              "polymorphic literal needs a concrete type", e->name);
                        continue;
                    }
                }
                if(!*actual_type || !strcmp(actual_type, "null")) {
                    error(c, c->fn->exprs[child].span,
                          "cannot infer polymorphic type", e->name);
                    continue;
                }
                if(!concrete[0])
                    copy_text(concrete, sizeof(concrete), actual_type);
                else if(strcmp(concrete, actual_type))
                    signature_error(c, c->fn->exprs[child].span,
                                    "polymorphic type mismatch", e->name);
            }
            free(parameters);
            if(!concrete[0]) {
                error(c, e->span, "cannot infer polymorphic type", e->name);
                break;
            }
            if(!replace_template_type(specialized_args, sizeof(specialized_args),
                                      callee->args, callee->template_param, concrete) ||
               !replace_template_type(specialized_return,
                                      sizeof(specialized_return),
                                      callee->return_type,
                                      callee->template_param, concrete)) {
                error(c, e->span, "specialized signature is too long", e->name);
                break;
            }
            char name[ZIR_NAME_MAX];
            const ZirModule *instance_owner = owner;
            if(owner != c->module &&
               FindType(owner, concrete, NULL) == NULL &&
               FindType(c->module, concrete, NULL) != NULL)
                instance_owner = c->module;
            if(!queue_specialization(c, owner, instance_owner,
                                     callee, concrete,
                                     name, sizeof(name), e->span)) break;
            const char *dot = instance_owner == owner ?
                strchr(e->name, '.') : NULL;
            if(dot) {
                char qualified[ZIR_NAME_MAX];
                int written = snprintf(qualified, sizeof(qualified), "%.*s.%s",
                                       (int)(dot - e->name), e->name, name);
                if(written < 0 || (size_t)written >= sizeof(qualified)) {
                    error(c, e->span, "specialized name is too long", e->name);
                    break;
                }
                copy_text(e->name, sizeof(e->name), qualified);
            } else copy_text(e->name, sizeof(e->name), name);
        }
        if(callee != NULL && callee->is_extern && callee->extern_kind == ZIR_EXTERN_HOST)
            c->fn->uses_host = 1;
        const char *args = slot ? slot->body :
                           specialized_args[0] ? specialized_args :
                           callee ? callee->args : NULL;
        const char *return_type = slot ? slot->procedure_return_type :
                                  specialized_return[0] ? specialized_return :
                                  callee ? callee->return_type : "";
        char (*parts)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parts));
        int actual = 0, expected;
        int varargs = 0;
        if(!parts) { c->errors++; c->failed=1; break; }
        if(*binding && slot == NULL)
            error(c, e->span, "binding is not a callable function", e->name);
        if(!callee && slot == NULL) for(int i = 0; i < c->module->import_count; i++) {
            const ZirImport *imp = &c->module->imports[i];
            if(imp->kind == ZIR_IMPORT_EXTERN &&
               in_lookup_file(c->module, imp->is_file_private, imp->span) &&
               !strcmp(imp->name, e->name)) {
                if(imp->extern_kind == ZIR_EXTERN_HOST)
                    c->fn->uses_host = 1;
                args = imp->args; return_type = imp->return_type;
                varargs = imp->is_varargs;
                break;
            }
        }
        /* Calling a procedure-typed record field (value.is_active(app)) or a
         * field behind a record pointer (practice.draw(app)): resolve the
         * field's slot type and type the call from its signature. Anonymous
         * calls name their callee in e->left. */
        if(!callee && slot == NULL) {
            if(e->name[0] == '\0' && e->left >= 0) {
                const ZirType *indirect = FindType(c->module, left, NULL);
                if(indirect != NULL && indirect->is_procedure_type) {
                    copy_text(e->slot_type, sizeof(e->slot_type), left);
                    slot = indirect;
                    args = indirect->body;
                    return_type = indirect->procedure_return_type;
                }
            } else if(strchr(e->name, '.') != NULL) {
                char base[ZIR_NAME_MAX];
                copy_text(base, sizeof(base), e->name);
                char *field = strrchr(base, '.');
                *field++ = '\0';
                const char *base_type = lookup(c, base);
                const char *record_name = base_type;
                if(*record_name == '*')
                    record_name = skip_ws(record_name + 1);
                const ZirModule *field_owner = NULL;
                const ZirType *record = FindType(c->module, record_name, &field_owner);
                if(record != NULL && !record->is_enum) {
                    char member_type[ZIR_NAME_MAX] = "";
                    char field_path[ZIR_NAME_MAX];
                    int found = strchr(field, '.') != NULL ?
                        RecordFieldPathType(field_owner, record, field,
                                            member_type, sizeof(member_type)) :
                        ResolveRecordField(field_owner, record, field,
                                           field_path, sizeof(field_path),
                                           member_type, sizeof(member_type));
                    const ZirType *field_slot = *member_type ?
                        FindType(c->module, member_type, NULL) : NULL;
                    if(found > 0 && field_slot != NULL &&
                       field_slot->is_procedure_type) {
                        copy_text(e->slot_type, sizeof(e->slot_type), member_type);
                        slot = field_slot;
                        args = field_slot->body;
                        return_type = field_slot->procedure_return_type;
                    }
                }
            }
        }
        expected = args && *skip_ws(args) ? split_top_level(args, parts[0], 64, sizeof(parts[0])) : 0;
        if(callee_owner != NULL && callee_owner != c->module)
            for(int parameter = 0; parameter < expected; parameter++) {
                char *colon = strchr(parts[parameter], ':');
                if(colon == NULL) continue;
                char mapped[ZIR_NAME_MAX];
                copy_text(mapped, sizeof(mapped), skip_ws(colon + 1));
                trim_in_place(mapped);
                if(!record_field_type_at_use(c->module, callee_owner,
                                             e->name, mapped,
                                             sizeof(mapped))) {
                    error(c, e->span,
                          "imported procedure parameter type is shadowed",
                          e->name);
                    continue;
                }
                int written = snprintf(colon + 1,
                    sizeof(parts[parameter]) - (size_t)(colon + 1 - parts[parameter]),
                    " %s", mapped);
                if(written < 0 || (size_t)written >=
                   sizeof(parts[parameter]) -
                       (size_t)(colon + 1 - parts[parameter]))
                    error(c, e->span, "imported parameter type is too long",
                          e->name);
            }
        int fixed = varargs && expected > 0 ? expected - 1 : expected;
        if(varargs && expected > 0) {
            /* bind_call_arguments assigns slots from the fixed parameters;
             * variadic extras take the next sequential slots. */
            if(!bind_varargs_call(c, e, parts, fixed, display_name)) {
                free(parts);
                break;
            }
        } else if(args && !bind_call_arguments(c, e, parts, expected, display_name,
                                       callee ? callee->default_args : NULL)) {
            free(parts);
            break;
        }
        for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
            int parameter = c->fn->exprs[child].argument_index;
            const char *expected_type = parameter >= 0 && parameter < expected &&
                parameter < fixed ?
                strchr(parts[parameter], ':') : NULL;
            char saved_expected[ZIR_NAME_MAX];
            copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
            if(expected_type != NULL)
                contextual_slot(c, child, skip_ws(expected_type + 1));
            if(expected_type != NULL)
                copy_text(c->expected_type, sizeof(c->expected_type),
                          skip_ws(expected_type + 1));
            const char *arg_type = expression_type(c, child);
            if(contains_vec(c->module, arg_type, 0) &&
               c->fn->exprs[child].kind != ZIR_EXPR_IDENT &&
               c->fn->exprs[child].kind != ZIR_EXPR_CALL &&
               !owned_initializer_shape(c, child))
                error(c, c->fn->exprs[child].span,
                      "Vec arguments move a binding or pass a call result",
                      display_name);
            if(contains_vec(c->module, arg_type, 0) &&
               global_vec_source(c, child))
                error(c, c->fn->exprs[child].span,
                      "global Vec storage cannot move; use a local",
                      c->fn->exprs[child].name);
            copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
            if(args && parameter >= 0 && parameter < fixed) {
                char *colon = strchr(parts[parameter], ':');
                if(colon && !compatible_checked(c, skip_ws(colon + 1), arg_type)) {
                    const char *converted = try_conversion(c, child,
                                                           skip_ws(colon + 1),
                                                           c->fn->exprs[child].span);
                    e = &c->fn->exprs[index];
                    if(converted != NULL)
                        arg_type = converted;
                    else
                        signature_error(c, c->fn->exprs[child].span,
                                        "argument type mismatch", display_name);
                }
                if(colon && (!strcmp(arg_type, "integer") || !strcmp(arg_type, "real"))) {
                    const char *context = ScalarType(skip_ws(colon + 1));
                    if(*context) copy_text(c->fn->exprs[child].type, ZIR_NAME_MAX, context);
                }
            }
            actual++;
        }
        if(args) {
            char saved_checked_type[ZIR_NAME_MAX];
            copy_text(saved_checked_type, sizeof(saved_checked_type), e->type);
            type = return_type;
            if(specialized_return[0]) {
                copy_text(e->type, sizeof(e->type), specialized_return);
                type = e->type;
            }
            if(callee_owner != NULL && callee_owner != c->module) {
                if(type != e->type)
                    copy_text(e->type, sizeof(e->type), type);
                if(!record_field_type_at_use(c->module, callee_owner,
                                             e->name, e->type,
                                             sizeof(e->type)))
                    error(c, e->span,
                          "imported procedure result type is shadowed",
                          e->name);
                if(saved_checked_type[0] &&
                   same_declared_type(c->module, e->type,
                                      saved_checked_type, 0))
                    copy_text(e->type, sizeof(e->type), saved_checked_type);
                type = e->type;
            }
            if(actual < fixed && !c->inference_only)
                signature_error(c, e->span, "argument count mismatch", display_name);
        } else
            error(c, e->span, "unresolved function", e->name);
        free(parts);
        break;
    }
    case ZIR_EXPR_BINARY: {
        if(left[0] == '[' || right[0] == '[')
            error(c, e->span, "array values do not support binary operations", e->op);
        const ZirType *left_slot = FindType(c->module, left, NULL);
        const ZirType *right_slot = FindType(c->module, right, NULL);
        /* Procedure slots compare against null (and identical slot types)
         * the same way pointers do; every other operation stays rejected. */
        int slot_null_compare =
            (!strcmp(e->op, "==") || !strcmp(e->op, "!=")) &&
            ((left_slot != NULL && left_slot->is_procedure_type &&
              (!strcmp(right, "null") ||
               (right_slot != NULL && right_slot->is_procedure_type &&
                !strcmp(left, right)))) ||
             (right_slot != NULL && right_slot->is_procedure_type &&
              !strcmp(left, "null")));
        if(((left_slot && left_slot->is_procedure_type) ||
            (right_slot && right_slot->is_procedure_type)) && !slot_null_compare)
            error(c, e->span, "slot values do not support binary operations", e->op);
        if((text_type(left) || text_type(right)) &&
           strcmp(e->op, "==") && strcmp(e->op, "!="))
            error(c, e->span, "string operation is not supported", e->op);
        if((!strcmp(e->op, "&") || !strcmp(e->op, "|") || !strcmp(e->op, "^") ||
            !strcmp(e->op, "<<") || !strcmp(e->op, ">>") || !strcmp(e->op, "%")) &&
           (left[0] == 'f' || right[0] == 'f' || !strcmp(left, "real") || !strcmp(right, "real")))
            error(c, e->span, "integer operands required", e->op);
        if((!strcmp(e->op, "&&") || !strcmp(e->op, "||")) &&
           (strcmp(left, "bool") || strcmp(right, "bool")))
            error(c, e->span, "logical operands require bool", e->op);
        const ZirType *left_flags = flags_type(c, left);
        const ZirType *right_flags = flags_type(c, right);
        if(left_flags != NULL || right_flags != NULL) {
            const char *flag_name = left_flags != NULL ? left : right;
            const char *other = left_flags != NULL ? right : left;
            if((left_flags != NULL && right_flags != NULL && strcmp(left, right)) ||
               (strcmp(other, flag_name) && !integer_type(other)))
                error(c, e->span, "flag operands require the same enum or an integer", e->op);
            if(!strcmp(e->op, "==") || !strcmp(e->op, "!="))
                type = "bool";
            else if(!strcmp(e->op, "&") || !strcmp(e->op, "|") ||
                    !strcmp(e->op, "^") || !strcmp(e->op, "+") ||
                    !strcmp(e->op, "-"))
                type = flag_name;
            else
                error(c, e->span, "unsupported enum_flags operation", e->op);
            break;
        }
        if(!compatible_checked(c, left, right) &&
           !compatible_checked(c, right, left) && !slot_null_compare)
            error(c, e->span, "operand types differ; use an explicit cast", e->op);
        if(!strcmp(e->op, "==") || !strcmp(e->op, "!=") || !strcmp(e->op, "<") ||
           !strcmp(e->op, "<=") || !strcmp(e->op, ">") || !strcmp(e->op, ">=") ||
           !strcmp(e->op, "&&") || !strcmp(e->op, "||")) type = "bool";
        else if(numeric(left) && numeric(right))
            type = (!strcmp(left, "integer") || !strcmp(left, "real")) ? right : left;
        else if(*left && *right) error(c, e->span, "numeric operands required", e->op);
        break;
    }
    case ZIR_EXPR_UNARY:
        if(strcmp(e->op, "+") && strcmp(e->op, "-") &&
           strcmp(e->op, "!") && strcmp(e->op, "~") &&
           strcmp(e->op, "&") && strcmp(e->op, "*"))
            error(c, e->span, "unsupported unary operator", e->op);
        if(!strcmp(e->op, "!") && strcmp(right, "bool"))
            error(c, e->span, "logical operand requires bool", e->op);
        if(!strcmp(e->op, "~") && !integer_type(right))
            error(c, e->span, "bitwise operand requires an integer", e->op);
        if(!strcmp(e->op, "&")) {
            if(!assignable(c, e->right))
                error(c, e->span, "address-of requires an assignable expression", e->text);
            if(!*right || !strcmp(right, "null") || strlen(right) + 1 >= sizeof(e->type))
                error(c, e->span, "address-of requires a known type", e->text);
            snprintf(e->type, sizeof(e->type), "*%s", right);
            type = e->type;
        } else if(!strcmp(e->op, "*")) {
            if(right[0] != '*' || !*skip_ws(right + 1))
                error(c, e->span, "dereference requires a pointer", right);
            type = skip_ws(right + 1);
        } else if(!strcmp(e->op, "!")) type = "bool";
        else if(numeric(right)) type = right;
        else
            error(c, e->span, "unresolved unary operation", e->op);
        break;
    case ZIR_EXPR_CAST: {
        if(!JaiTypeSpelling(e->span, e->name)) {
            c->errors++;
            return "";
        }
        if(right[0] == '[' || e->name[0] == '[')
            error(c, e->span, "array casts are not supported", e->name);
        const ZirType *destination = FindType(c->module, e->name, NULL);
        const ZirType *source = FindType(c->module, right, NULL);
        if(destination != NULL && destination->is_enum &&
           !numeric(right) && strcmp(right, "bool") &&
           (source == NULL || !source->is_enum))
            error(c, e->span, "enum casts require a numeric, bool, or enum value", e->name);
        if((text_type(right) || text_type(e->name)) && strcmp(right, e->name))
            error(c, e->span, "string casts require an explicit conversion API", e->name);
        type = e->name;
        break;
    }
    case ZIR_EXPR_CONDITIONAL: {
        const char *third = expression_type(c, e->third);
        if(strcmp(left, "bool")) error(c, e->span, "conditional requires bool", left);
        if(!compatible_checked(c, right, third) &&
           !compatible_checked(c, third, right))
            error(c, e->span, "conditional arms have different types", "");
        type = !strcmp(right, "integer") || !strcmp(right, "null") ?
               third : right;
        break;
    }
    default: error(c, e->span, "expression is not supported by language checking", e->text); break;
    }
    if(*ScalarType(type)) type = ScalarType(type);
    if(type != e->type)
        copy_text(e->type, sizeof(e->type), type);
    normalize_array(c->module, e->type, sizeof(e->type));
    return e->type;
}

int
InferExpressionType(const ZirModule *module, const char *expression,
                    ZirSourceSpan span, char *type, size_t capacity)
{
    ZirModule lookup = *module;
    ZirFunction probe = {0};
    Checker checker = {0};
    int root, valid;
    const char *inferred;

    copy_text(lookup.lookup_path, sizeof(lookup.lookup_path), span.path);
    probe.span = span;
    root = ParseExprNoDefaults(&probe, &lookup, expression, span);
    checker.module = &lookup;
    checker.fn = &probe;
    checker.inference_only = 1;
    inferred = root >= 0 ? expression_type(&checker, root) : "";
    if(strcmp(inferred, "integer") == 0) inferred = "s64";
    else if(strcmp(inferred, "real") == 0) inferred = "float64";
    valid = checker.errors == 0 && *inferred != '\0' &&
            strlen(inferred) < capacity;
    if(valid) copy_text(type, capacity, inferred);
    free(checker.bindings);
    free(checker.specializations);
    free(probe.exprs);
    return valid;
}

static int
record_declaration_error(const ZirType *record, const char *message,
                         const char *field)
{
    Diagnostic(record->span, "check.record", "%s: %s%s%s",
            message, record->name, field && *field ? "." : "",
            field ? field : "");
    return 0;
}

/* Structural errors are invalid in every backend. Check them before function
 * eligibility can select a native emitter. */
typedef struct RecordPath {
    const ZirType *record;
    const struct RecordPath *parent;
    int depth;
} RecordPath;

typedef struct ValidatedRecords {
    const ZirType **items;
    size_t count;
} ValidatedRecords;

/* Validate stored shapes before choosing an emitter. In particular, a slice
 * must never be mistaken for a fixed array, nor a recursive value layout for
 * an opaque host type. Pointers stop layout recursion but still name a type. */
static int declared_application_valid(const ZirModule *module,
                                      const char *text, int depth);

static const char *
storage_type_error(const ZirModule *module, const char *source,
                   const RecordPath *path, int indirect, ValidatedRecords *checked,
                   char *detail, size_t detail_capacity)
{
    char type[ZIR_NAME_MAX];
    char element[ZIR_NAME_MAX];
    const ZirModule *owner = NULL;
    const ZirType *record;
    int capacity;

    copy_text(type, sizeof(type), source);
    trim_in_place(type);
    normalize_array(module, type, sizeof(type));
    if(!strcmp(type, "void"))
        return indirect ? NULL : "stored values cannot have void type";
    {
        char slice_element[ZIR_NAME_MAX];
        if(SliceElementType(type, slice_element, sizeof(slice_element))) {
            if(SliceElementType(slice_element, NULL, 0) ||
               !strcmp(slice_element, "char") ||
               !strcmp(slice_element, "const char"))
                return "nested slice descriptors are not storable";
            return storage_type_error(module, slice_element, path, 1,
                                      checked, detail, detail_capacity);
        }
    }
    if(*ScalarType(type) != '\0' || TargetType(type, ZIR_C) != NULL)
        return NULL;
    if(type[0] == '*') {
        const char *pointee = skip_ws(type + 1);
        if(!*pointee)
            return "pointer requires an element type";
        return storage_type_error(module, pointee, path, 1, checked,
                                  detail, detail_capacity);
    }
    if(type[0] == '[') {
        if(type[1] == ']')
            return "slices do not yet have portable storage semantics";
        if(!ArrayElementType(type, element, sizeof(element), &capacity))
            return "malformed fixed array type";
        if(capacity < 0) {
            int status = array_capacity(module, type, &capacity);
            if(status < 0)
                return "array capacity is not a valid bounded integer constant";
            if(status == 0)
                return "array capacity requires a known integer constant";
        }
        return storage_type_error(module, element, path, indirect, checked,
                                  detail, detail_capacity);
    }
    size_t length = strlen(type);
    if(length > 0 && type[length - 1] == '*') {
        type[length - 1] = '\0';
        trim_in_place(type);
        if(!strncmp(type, "const ", 6))
            memmove(type, type + 6, strlen(type + 6) + 1);
        return storage_type_error(module, type, path, 1, checked,
                                  detail, detail_capacity);
    }
    record = FindType(module, type, &owner);
    if(record == NULL) {
        if(indirect && declared_application_valid(module, type, 0))
            return NULL;
        if(detail != NULL && detail_capacity > 0)
            copy_text(detail, detail_capacity, type);
        return "unknown stored type";
    }
    if(record->is_record_template)
        return "generic types require a concrete specialization";
    if(record->is_procedure_type)
        return NULL;
    if(record->is_enum || indirect)
        return NULL;
    for(const RecordPath *ancestor = path; ancestor != NULL; ancestor = ancestor->parent) {
        if(ancestor->record == record)
            return "record has a recursive value layout";
    }
    for(size_t i = 0; i < checked->count; i++) {
        if(checked->items[i] == record)
            return NULL;
    }
    if(path != NULL && path->depth >= 128)
        return "record nesting exceeds the portable limit";
    RecordPath current = {record, path, path == NULL ? 1 : path->depth + 1};
    ZirTypeField field;
    size_t offset = 0;
    int status;
    while((status = TypeNextField(record, &offset, &field)) == 1) {
        const char *error = storage_type_error(owner, field.type, &current, 0,
                                               checked, detail, detail_capacity);
        if(error != NULL)
            return error;
    }
    if(status < 0)
        return "malformed record field";
    const ZirType **items = realloc(checked->items, (checked->count + 1) * sizeof(*items));
    if(items == NULL)
        return "cannot allocate record type validation state";
    checked->items = items;
    checked->items[checked->count++] = record;
    return NULL;
}

static const char *
local_storage_error(const ZirModule *module, const char *type)
{
    ValidatedRecords checked = {0};
    char element[ZIR_NAME_MAX];
    int slice = SliceElementType(type, element, sizeof(element));
    const char *problem;
    if(slice && (element[0] == '[' || !strcmp(element, "char") || !strcmp(element, "const char")))
        problem = "unsupported slice element type";
    else
        problem = storage_type_error(module, slice ? element : type, NULL, 0,
                                     &checked, NULL, 0);
    free(checked.items);
    return problem;
}

static int
declared_application_valid(const ZirModule *module, const char *text, int depth)
{
    char name[ZIR_NAME_MAX];
    char arguments[ZIR_TEXT_MAX];
    char actual[16][ZIR_NAME_MAX], parameters[16][ZIR_NAME_MAX];
    size_t length = 0;
    const char *cursor = skip_ws(text);
    if(depth > 16) return 0;
    while((isalnum((unsigned char)*cursor) || *cursor == '_') &&
          length + 1 < sizeof(name))
        name[length++] = *cursor++;
    name[length] = '\0';
    cursor = skip_ws(cursor);
    if(length == 0 || *cursor++ != '(') return 0;
    const char *start = cursor;
    int nesting = 1;
    while(*cursor && nesting) {
        if(*cursor == '(') nesting++;
        else if(*cursor == ')') nesting--;
        if(nesting) cursor++;
    }
    if(nesting || *skip_ws(cursor + 1) != '\0' ||
       (size_t)(cursor - start) >= sizeof(arguments)) return 0;
    memcpy(arguments, start, (size_t)(cursor - start));
    arguments[cursor - start] = '\0';
    const ZirType *generic = FindType(module, name, NULL);
    if(generic == NULL || !generic->is_record_template)
        return 0;
    int expected = split_top_level(generic->template_params, parameters[0],
                                   16, sizeof(parameters[0]));
    int count = split_top_level(arguments, actual[0],
                                16, sizeof(actual[0]));
    if(count != expected || count < 1 || count >= 16) return 0;
    for(int i = 0; i < count; i++)
        if(!declared_application_valid(module, actual[i], depth + 1) &&
           local_storage_error(module, actual[i]) != NULL)
            return 0;
    return 1;
}

static int
check_type_declarations(ZirModule *module)
{
    for(int i = 0; i < module->type_count; i++) {
        const ZirType *record = &module->types[i];
        select_lookup_file(module, record->span);
        size_t offset = 0;
        int status;
        ZirTypeField field;

        if(record->name[0] == '#')
            return record_declaration_error(record,
                "retired compiler-generated type", NULL);
        if(BuiltinType(record->name) != NULL)
            return record_declaration_error(record,
                "built-in type cannot be redeclared", NULL);
        for(int previous = 0; previous < i; previous++) {
            const ZirType *other = &module->types[previous];
            if(strcmp(record->name, other->name) == 0 &&
               !(strcmp(record->span.path, other->span.path) != 0 &&
                 (record->is_file_private || other->is_file_private)))
                return record_declaration_error(record, "duplicate type declaration", NULL);
        }
        if(record->is_record_template) {
            char parameters[16][ZIR_NAME_MAX];
            char concrete[16][ZIR_NAME_MAX];
            int parameter_count = split_top_level(record->template_params,
                parameters[0], 16, sizeof(parameters[0]));
            if(parameter_count < 1 || parameter_count >= 16)
                return record_declaration_error(record,
                    "invalid generic type parameters", NULL);
            for(int parameter = 0; parameter < parameter_count; parameter++) {
                const unsigned char *name =
                    (const unsigned char *)parameters[parameter];
                if(!isalpha(*name) && *name != '_')
                    return record_declaration_error(record,
                        "invalid generic type parameter", parameters[parameter]);
                for(name++; *name; name++)
                    if(!isalnum(*name) && *name != '_')
                        return record_declaration_error(record,
                            "invalid generic type parameter", parameters[parameter]);
                for(int earlier = 0; earlier < parameter; earlier++)
                    if(!strcmp(parameters[earlier], parameters[parameter]))
                        return record_declaration_error(record,
                            "duplicate generic type parameter", parameters[parameter]);
                copy_text(concrete[parameter], sizeof(concrete[parameter]),
                          "s32");
            }
            int members = 0;
            size_t member_offset = 0;
            {
                ZirTypeField item, previous;
                while((status = TypeNextField(record, &member_offset,
                                              &item)) == 1) {
                    size_t prior_offset = 0;
                    while(prior_offset < member_offset) {
                        if(TypeNextField(record, &prior_offset,
                                         &previous) != 1)
                            return record_declaration_error(record,
                                "malformed generic record field", NULL);
                        if(prior_offset == member_offset) break;
                        if(!strcmp(previous.name, item.name))
                            return record_declaration_error(record,
                                "duplicate generic record field", item.name);
                    }
                    char resolved[ZIR_NAME_MAX];
                    if(!SubstituteGenericType(item.type, resolved,
                            sizeof(resolved), parameters, concrete,
                            parameter_count))
                        return record_declaration_error(record,
                            "generic record field type exceeds size limit",
                            item.name);
                    const char *problem = local_storage_error(module, resolved);
                    if(problem != NULL &&
                       !declared_application_valid(module, resolved, 0))
                        return record_declaration_error(record, problem,
                                                        item.name);
                    members++;
                }
            }
            if(status < 0 || members == 0)
                return record_declaration_error(record,
                    "invalid generic type declaration", NULL);
            continue;
        }
        if(record->is_enum)
            continue;
        if(record->is_procedure_type) {
            const ZirType *result = FindType(module,
                record->procedure_return_type, NULL);
            const char *result_base = record->procedure_return_type;
            while(*result_base == '*')
                result_base++;
            const ZirType *result_base_type = result != NULL ? result :
                FindType(module, result_base, NULL);
            if(!record->procedure_return_type[0] ||
               (record->is_c_call ?
                (strcmp(record->procedure_return_type, "void") &&
                 local_storage_error(module, record->procedure_return_type) != NULL) :
                (TargetType(record->procedure_return_type, ZIR_C) == NULL &&
                 TargetType(result_base, ZIR_C) == NULL &&
                 result_base_type == NULL)) ||
               (result_base_type && result_base_type->is_procedure_type))
                return record_declaration_error(record,
                    "invalid procedure type result", record->procedure_return_type);
            char parameters[64][ZIR_TEXT_MAX];
            int count = *skip_ws(record->body) ?
                split_top_level(record->body, parameters[0], 64, sizeof(parameters[0])) : 0;
            for(int parameter = 0; parameter < count; parameter++) {
                char *colon = strchr(parameters[parameter], ':');
                if(colon == NULL)
                    return record_declaration_error(record, "slot parameters require name: type", NULL);
                *colon++ = '\0';
                trim_in_place(parameters[parameter]);
                trim_in_place(colon);
                /* Pointer slot parameters name a pointee type: strip the
                 * leading '*' chain before resolving the base type. */
                const char *base = colon;
                while(*base == '*')
                    base++;
                const ZirType *type = FindType(module, colon, NULL);
                const ZirType *base_type = type != NULL ? type :
                    FindType(module, base, NULL);
                if(!*parameters[parameter] || !strcmp(colon, "void") ||
                   (record->is_c_call ?
                    local_storage_error(module, colon) != NULL :
                    (TargetType(colon, ZIR_C) == NULL &&
                     TargetType(base, ZIR_C) == NULL && base_type == NULL)) ||
                   (base_type && base_type->is_procedure_type))
                    return record_declaration_error(record, "invalid slot parameter", parameters[parameter]);
                for(int previous = 0; previous < parameter; previous++)
                    if(!strcmp(parameters[previous], parameters[parameter]))
                        return record_declaration_error(record, "duplicate slot parameter", parameters[parameter]);
            }
            continue;
        }
        while((status = TypeNextField(record, &offset, &field)) == 1) {
            size_t previous_offset = 0;
            ZirTypeField previous;

            if(strcmp(field.type, "void") == 0)
                return record_declaration_error(record, "record field cannot have void type", field.name);
            if(field.is_using) {
                const char *nested_name = skip_ws(field.type);
                if(*nested_name == '*') nested_name = skip_ws(nested_name + 1);
                const ZirType *nested = FindType(module, nested_name, NULL);
                if(nested == NULL || nested->is_enum ||
                   nested->is_procedure_type || nested->is_record_template)
                    return record_declaration_error(record,
                        "using field requires a concrete record type", field.name);
            }
            while(TypeNextField(record, &previous_offset, &previous) == 1 &&
                  previous_offset < offset) {
                if(strcmp(previous.name, field.name) == 0)
                    return record_declaration_error(record, "duplicate record field", field.name);
            }
        }
        if(status < 0)
            return record_declaration_error(record, "malformed record field", NULL);
        ValidatedRecords checked = {0};
        char detail[ZIR_NAME_MAX] = "";
        const char *error = storage_type_error(module, record->name, NULL, 0,
                                               &checked, detail, sizeof(detail));
        free(checked.items);
        if(error != NULL)
            return record_declaration_error(record, error,
                                            detail[0] ? detail : NULL);
    }
    return 1;
}

/* Checked record layouts carry concrete array sizes into saved IR and ZIB.
 * A bundle intentionally omits source definitions, so a field must not keep
 * depending on a compile-time name after this point. */
static int
normalize_record_arrays(ZirModule *module)
{
    for(int i = 0; i < module->type_count; i++) {
        ZirType *record = &module->types[i];
        select_lookup_file(module, record->span);
        if(record->is_procedure_type || record->is_enum)
            continue;
        size_t offset = 0;
        ZirTypeField field;
        int status, changed = 0;
        while((status = TypeNextField(record, &offset, &field)) == 1) {
            char normalized[sizeof(field.type)];
            copy_text(normalized, sizeof(normalized), field.type);
            normalize_array(module, normalized, sizeof(normalized));
            if(strcmp(normalized, field.type) != 0)
                changed = 1;
        }
        if(status < 0)
            return 0;
        if(!changed)
            continue;
        char body[sizeof(record->body)] = "";
        size_t used = 0;
        offset = 0;
        while((status = TypeNextField(record, &offset, &field)) == 1) {
            normalize_array(module, field.type, sizeof(field.type));
            int length = snprintf(body + used, sizeof(body) - used,
                                  "%s%s: %s\n", field.is_using ? "using " : "",
                                  field.name, field.type);
            if(length < 0 || (size_t)length >= sizeof(body) - used) {
                Diagnostic(record->span, "check.record", "normalized record is too large: %s",
                           record->name);
                return 0;
            }
            used += (size_t)length;
        }
        if(status < 0)
            return 0;
        copy_text(record->body, sizeof(record->body), body);
    }
    return 1;
}

static int
return_block_end(const ZirFunction *fn, int begin, int end)
{
    int depth = 1;
    for(int i = begin + 1; i < end; i++) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE || kind == ZIR_STMT_BLOCK_OPEN)
            depth++;
        if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0)
            return i;
    }
    return end;
}

static int sequence_returns(const ZirFunction *fn, int begin, int end);

static int
branch_returns(const ZirFunction *fn, int begin, int end, int *last)
{
    *last = return_block_end(fn, begin, end);
    int returns = sequence_returns(fn, begin + 1, *last);
    if(fn->stmts[begin].expr_root < 0)
        return returns;
    int next = *last + 1;
    if(next < end && fn->stmts[next].kind == ZIR_STMT_IF &&
       fn->stmts[next].is_else) {
        int alternative = branch_returns(fn, next, end, last);
        return returns && alternative;
    }
    return 0;
}

/* True when every explicit arm of the if chain at `begin` returns. The
 * fall-through state of such a chain is the state before it. */
static int
all_arms_return(const ZirFunction *fn, int begin, int *last)
{
    int arms = 0;
    int index = begin;
    while(index < fn->stmt_count && fn->stmts[index].kind == ZIR_STMT_IF &&
          (arms == 0 || fn->stmts[index].is_else)) {
        int close = return_block_end(fn, index, fn->stmt_count);
        if(close <= index + 1 || !sequence_returns(fn, index + 2, close))
            return 0;
        *last = close;
        index = close + 1;
        arms++;
    }
    return arms > 0;
}

static int
sequence_returns(const ZirFunction *fn, int begin, int end)
{
    for(int i = begin; i < end; i++) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(kind == ZIR_STMT_RETURN || kind == ZIR_STMT_UNREACHABLE)
            return 1;
        if(kind == ZIR_STMT_BREAK || kind == ZIR_STMT_CONTINUE)
            return 0;
        if(kind == ZIR_STMT_IF) {
            int last;
            if(branch_returns(fn, i, end, &last))
                return 1;
            i = last;
        } else if(kind == ZIR_STMT_WHILE || kind == ZIR_STMT_BLOCK_OPEN) {
            int last = return_block_end(fn, i, end);
            if(kind == ZIR_STMT_BLOCK_OPEN && sequence_returns(fn, i + 1, last))
                return 1;
            i = last;
        }
    }
    return 0;
}

static int
starts_word(const char *source, const char *word)
{
    size_t length = strlen(word);
    return strncmp(source, word, length) == 0 &&
           (source[length] == '\0' || isspace((unsigned char)source[length]));
}

static int
if_case_opens_block(ZirStmtKind kind)
{
    return kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE ||
           kind == ZIR_STMT_FOR || kind == ZIR_STMT_IF_CASE ||
           kind == ZIR_STMT_BLOCK_OPEN;
}

static void
if_case_generated(ZirStmt *statement, ZirStmtKind kind,
                const char *text, ZirSourceSpan span)
{
    memset(statement, 0, sizeof(*statement));
    statement->kind = kind;
    copy_text(statement->text, sizeof(statement->text), text);
    statement->expr_root = statement->lhs_root = -1;
    statement->span = span;
}

static void
if_case_error(Checker *c, ZirSourceSpan span, const char *message,
            const char *detail)
{
    c->errors++;
    c->failed = 1;
    Diagnostic(span, "check.if_case", "%s%s%s", message,
               detail && *detail ? ": " : "", detail ? detail : "");
}

static int
if_case_through(Checker *c, const ZirFunction *fn, const int *cases,
                   int case_count, int close, unsigned char *through)
{
    int has_through = 0;
    for(int arm = 0; arm < case_count; arm++) {
        int arm_end = arm + 1 < case_count ? cases[arm + 1] : close;
        int nested = 0;
        for(int i = cases[arm] + 1; i < arm_end; i++) {
            const ZirStmt *statement = &fn->stmts[i];
            if(statement->kind == ZIR_STMT_IF_CASE) {
                int inner_depth = 1;
                while(++i < arm_end && inner_depth > 0) {
                    if(if_case_opens_block(fn->stmts[i].kind)) inner_depth++;
                    if(fn->stmts[i].kind == ZIR_STMT_BLOCK_CLOSE)
                        inner_depth--;
                }
                if(inner_depth != 0) {
                    if_case_error(c, statement->span,
                                "unterminated nested if-case", "");
                    return -1;
                }
                continue;
            }
            if(strcmp(statement->text, "#through") == 0 ||
               strcmp(statement->text, "#through;") == 0) {
                if(nested != 0 || i != arm_end - 1 ||
                   arm == case_count - 1) {
                    if_case_error(c, statement->span,
                        "#through must end a case before another case", "");
                    return -1;
                }
                through[arm] = has_through = 1;
            }
            if(if_case_opens_block(statement->kind)) nested++;
            if(statement->kind == ZIR_STMT_BLOCK_CLOSE) nested--;
        }
    }
    return has_through;
}

static int
if_cases_return(const ZirFunction *fn, const int *cases,
                   int case_count, int close, const unsigned char *through)
{
    int next_returns = 0, all_return = 1;
    for(int arm = case_count - 1; arm >= 0; arm--) {
        int end = arm + 1 < case_count ? cases[arm + 1] : close;
        if(through[arm]) end--;
        int escapes = 0;
        for(int i = cases[arm] + 1; i < end; i++)
            escapes |= fn->stmts[i].kind == ZIR_STMT_BREAK ||
                       fn->stmts[i].kind == ZIR_STMT_CONTINUE ||
                       fn->stmts[i].kind == ZIR_STMT_IF_CASE;
        int returns = !escapes &&
            sequence_returns(fn, cases[arm] + 1, end);
        if(!returns && !escapes && through[arm])
            returns = next_returns;
        all_return &= returns;
        next_returns = returns;
    }
    return all_return;
}

static int
scalar_case_label(const ZirStmt *statement, char *label, size_t capacity)
{
    const char *cursor = skip_ws(statement->text);
    if(strncmp(cursor, "case", 4) != 0 ||
       (cursor[4] != ';' && !isspace((unsigned char)cursor[4])))
        return 0;
    cursor = skip_ws(cursor + 4);
    size_t length = strlen(cursor);
    while(length && isspace((unsigned char)cursor[length - 1])) length--;
    if(length == 0 || cursor[length - 1] != ';' || length >= capacity)
        return 0;
    memcpy(label, cursor, length - 1);
    label[length - 1] = '\0';
    trim_in_place(label);
    return 1;
}

static int
scalar_case_type(const char *type)
{
    return !strcmp(type, "bool") || !strcmp(type, "string") ||
           numeric(type);
}

static int
enum_case_members(const ZirType *enumeration,
                  char (*names)[ZIR_NAME_MAX], int capacity)
{
    const char *cursor = enumeration->body;
    int count = 0;
    while(*cursor) {
        while(isspace((unsigned char)*cursor) ||
              *cursor == ',' || *cursor == ';') cursor++;
        if(!*cursor) break;
        const char *start = cursor;
        if(!isalpha((unsigned char)*cursor) && *cursor != '_') return -1;
        while(isalnum((unsigned char)*cursor) || *cursor == '_') cursor++;
        size_t length = (size_t)(cursor - start);
        if(count >= capacity || length >= ZIR_NAME_MAX) return -1;
        memcpy(names[count], start, length);
        names[count++][length] = '\0';
        while(*cursor && *cursor != ',' && *cursor != ';' &&
              *cursor != '\n') cursor++;
    }
    return count;
}

static int
enum_case_member(const ZirType *enumeration, const char *label,
                 char *member, size_t capacity)
{
    const char *start = label;
    if(*start == '.') start++;
    else {
        size_t length = strlen(enumeration->name);
        if(strncmp(start, enumeration->name, length) != 0 ||
           start[length] != '.') return 0;
        start += length + 1;
    }
    if(!isalpha((unsigned char)*start) && *start != '_') return 0;
    const char *end = start + 1;
    while(isalnum((unsigned char)*end) || *end == '_') end++;
    size_t length = (size_t)(end - start);
    if(*end || length >= capacity) return 0;
    memcpy(member, start, length);
    member[length] = '\0';
    return EnumMemberValue(enumeration, member, NULL);
}

/* A Jai if-case evaluates its selector once. With #through, an active flag
 * carries execution into the next arm without retesting its label. */
static int
lower_if_case(Checker *c, int index, const char *checked_type)
{
    ZirFunction *fn = c->fn;
    ZirStmt *head = &fn->stmts[index];
    const ZirType *enumeration = FindType(c->module, checked_type, NULL);
    int complete = enumeration != NULL &&
        starts_word(skip_ws(head->text + 2), "#complete");
    int *cases = calloc((size_t)fn->stmt_count + 1, sizeof(*cases));
    unsigned char *through = calloc((size_t)fn->stmt_count + 1,
                                    sizeof(*through));
    int member_capacity = enumeration != NULL ?
        (int)strlen(enumeration->body) + 1 : 0;
    char (*members)[ZIR_NAME_MAX] = enumeration != NULL ?
        calloc((size_t)member_capacity, sizeof(*members)) : NULL;
    unsigned char *seen = enumeration != NULL ?
        calloc((size_t)member_capacity, sizeof(*seen)) : NULL;
    int member_count = enumeration != NULL ?
        (members != NULL && seen != NULL ?
         enum_case_members(enumeration, members, member_capacity) : -1) : 0;
    ZirStmt *output = NULL;
    int count = 0, depth = 1, close = -1, default_arm = -1, ok = 0;
    char label[ZIR_TEXT_MAX], line[ZIR_TEXT_MAX];
    char source[ZIR_TEXT_MAX], temporary[ZIR_NAME_MAX];
    char active[ZIR_NAME_MAX], matched_name[ZIR_NAME_MAX];
    int length;
    if(cases == NULL || through == NULL ||
       (enumeration != NULL && member_count <= 0)) {
        if_case_error(c, head->span, "out of memory lowering if-case", "");
        goto done;
    }
    for(int i = index + 1; i < fn->stmt_count; i++) {
        ZirStmt *statement = &fn->stmts[i];
        if(statement->kind == ZIR_STMT_BLOCK_CLOSE) {
            if(--depth == 0) { close = i; break; }
        } else if(depth == 1 && statement->kind == ZIR_STMT_CASE) {
            if(!scalar_case_label(statement, label, sizeof(label))) {
                if_case_error(c, statement->span,
                            "scalar case requires 'case expression;' or 'case;'",
                            statement->text);
                goto done;
            }
            if(enumeration != NULL) {
                char member[ZIR_NAME_MAX];
                if(!enum_case_member(enumeration, label, member,
                                     sizeof(member))) {
                    if_case_error(c, statement->span,
                        "enum case requires 'case .Member;' or 'case Enum.Member;'",
                        statement->text);
                    goto done;
                }
                int selected = -1;
                for(int m = 0; m < member_count; m++)
                    if(!strcmp(member, members[m])) { selected = m; break; }
                if(selected < 0 || seen[selected]) {
                    if_case_error(c, statement->span,
                        selected < 0 ? "case is not a member of the selected enum" :
                                       "duplicate if-case member", member);
                    goto done;
                }
                seen[selected] = 1;
            }
            if(default_arm >= 0) {
                if_case_error(c, statement->span, "default case must be last", "");
                goto done;
            }
            if(label[0] == '\0') default_arm = count;
            cases[count++] = i;
        } else if(depth == 1 && count == 0) {
            if_case_error(c, statement->span,
                        "if-case body must begin with a case", "");
            goto done;
        }
        if(if_case_opens_block(statement->kind)) depth++;
    }
    if(close < 0 || count == 0) {
        if_case_error(c, head->span, "unterminated or empty if-case", "");
        goto done;
    }
    for(int m = 0; complete && m < member_count; m++)
        if(!seen[m]) {
            if_case_error(c, head->span,
                          "non-exhaustive if-case; missing case", members[m]);
            goto done;
        }
    int has_through = if_case_through(c, fn, cases, count,
                                         close, through);
    if(has_through < 0) goto done;
    int all_return = has_through && (default_arm >= 0 || complete) &&
        if_cases_return(fn, cases, count, close, through);
    copy_text(source, sizeof(source), head->text);
    char *equals = strstr(source, "==");
    if(equals == NULL) {
        if_case_error(c, head->span, "if-case requires a block", "");
        goto done;
    }
    *equals = '\0';
    const char *value = skip_ws(source + 2);
    if(complete) value = skip_ws(value + strlen("#complete"));
    trim_in_place((char *)value);
    if(!*value) {
        if_case_error(c, head->span, "if-case requires a value", "");
        goto done;
    }
    int serial = index, collision;
    do {
        snprintf(temporary, sizeof(temporary), "case_value_%d", serial++);
        collision = strstr(fn->args, temporary) != NULL;
        for(int i = 0; i < fn->stmt_count; i++)
            collision |= strstr(fn->stmts[i].text, temporary) != NULL ||
                         strcmp(fn->stmts[i].name, temporary) == 0;
    } while(collision);
    length = snprintf(active, sizeof(active), "%s_active", temporary);
    if(length < 0 || (size_t)length >= sizeof(active)) goto too_long;
    length = snprintf(matched_name, sizeof(matched_name),
                      "%s_matched", temporary);
    if(length < 0 || (size_t)length >= sizeof(matched_name)) goto too_long;
    int capacity = fn->stmt_count + count * 4 + 8;
    output = calloc((size_t)capacity, sizeof(*output));
    if(output == NULL) {
        if_case_error(c, head->span, "out of memory lowering if-case", "");
        goto done;
    }
    int next = 0;
    for(int i = 0; i < index; i++) output[next++] = fn->stmts[i];
    const char *storage = !strcmp(checked_type, "integer") ? "s64" :
                          !strcmp(checked_type, "real") ? "float64" :
                          checked_type;
    length = snprintf(line, sizeof(line), "%s: %s = %s",
                      temporary, storage, value);
    if(length < 0 || (size_t)length >= sizeof(line)) goto too_long;
    if_case_generated(&output[next++], ZIR_STMT_DECL, line, head->span);
    if(has_through) {
        length = snprintf(line, sizeof(line), "%s: bool = false", active);
        if(length < 0 || (size_t)length >= sizeof(line)) goto too_long;
        if_case_generated(&output[next++], ZIR_STMT_DECL, line, head->span);
        if(default_arm >= 0 || complete) {
            length = snprintf(line, sizeof(line), "%s: bool = false",
                              matched_name);
            if(length < 0 || (size_t)length >= sizeof(line)) goto too_long;
            if_case_generated(&output[next++], ZIR_STMT_DECL, line, head->span);
        }
    }
    for(int arm = 0; arm < count; arm++) {
        ZirSourceSpan span = fn->stmts[cases[arm]].span;
        char comparison[ZIR_TEXT_MAX];
        if(!scalar_case_label(&fn->stmts[cases[arm]], label,
                              sizeof(label))) goto done;
        if(enumeration != NULL) {
            char member[ZIR_NAME_MAX];
            if(!enum_case_member(enumeration, label, member,
                                 sizeof(member))) goto done;
            /* Enum references lower to integer literals during a later
             * checker restart. Keep the generated comparison enum typed. */
            length = snprintf(label, sizeof(label), "cast(%s) %s.%s",
                              enumeration->name, enumeration->name, member);
            if(length < 0 || (size_t)length >= sizeof(label)) goto too_long;
        }
        length = enumeration == NULL ?
            snprintf(comparison, sizeof(comparison), "(%s)", label) :
            snprintf(comparison, sizeof(comparison), "%s", label);
        if(length < 0 || (size_t)length >= sizeof(comparison)) goto too_long;
        if(label[0] == '\0')
            length = has_through ?
                snprintf(line, sizeof(line), "if %s || !%s {",
                         active, matched_name) :
                snprintf(line, sizeof(line), "else {");
        else
            length = has_through ?
                snprintf(line, sizeof(line), "if %s || %s == %s {",
                         active, temporary, comparison) :
                snprintf(line, sizeof(line), "%s %s == %s {",
                         arm ? "else if" : "if", temporary, comparison);
        if(length < 0 || (size_t)length >= sizeof(line)) goto too_long;
        if_case_generated(&output[next++], ZIR_STMT_IF, line, span);
        if(has_through && (default_arm >= 0 || complete)) {
            length = snprintf(line, sizeof(line), "%s = true", matched_name);
            if(length < 0 || (size_t)length >= sizeof(line)) goto too_long;
            if_case_generated(&output[next++], ZIR_STMT_ASSIGN, line, span);
        }
        int arm_end = arm + 1 < count ? cases[arm + 1] : close;
        for(int i = cases[arm] + 1; i < arm_end; i++)
            if(!(through[arm] && i == arm_end - 1))
                output[next++] = fn->stmts[i];
        if(has_through) {
            length = snprintf(line, sizeof(line), "%s = %s", active,
                              through[arm] ? "true" : "false");
            if(length < 0 || (size_t)length >= sizeof(line)) goto too_long;
            if_case_generated(&output[next++], ZIR_STMT_ASSIGN, line, span);
        }
        if_case_generated(&output[next++], ZIR_STMT_BLOCK_CLOSE, "}", span);
    }
    if(complete) {
        length = has_through ?
            snprintf(line, sizeof(line), "if !%s {", matched_name) :
            snprintf(line, sizeof(line), "else {");
        if(length < 0 || (size_t)length >= sizeof(line)) goto too_long;
        if_case_generated(&output[next++], ZIR_STMT_IF, line, head->span);
        if_case_generated(&output[next++], ZIR_STMT_UNREACHABLE,
                          "unreachable", head->span);
        if_case_generated(&output[next++], ZIR_STMT_BLOCK_CLOSE, "}", head->span);
    }
    if(all_return)
        if_case_generated(&output[next++], ZIR_STMT_UNREACHABLE,
                        "unreachable", head->span);
    for(int i = close + 1; i < fn->stmt_count; i++)
        output[next++] = fn->stmts[i];
    if(next > capacity) {
        if_case_error(c, head->span, "internal if-case lowering overflow", "");
        goto done;
    }
    free(fn->stmts);
    fn->stmts = output;
    fn->stmt_count = fn->stmt_cap = next;
    output = NULL;
    ok = 1;
    goto done;
too_long:
    if_case_error(c, head->span, "if-case expression exceeds statement limit", "");
done:
    free(output); free(cases); free(through); free(members); free(seen);
    return ok;
}

static int
validate_loop_targets(Checker *c, const ZirFunction *fn)
{
    int depth = 0, ok = 1;
    int *scopes = calloc((size_t)fn->stmt_count + 1, sizeof(*scopes));
    unsigned char *seen = calloc((size_t)fn->stmt_count + 1,
                                 sizeof(*seen));
    if(scopes == NULL || seen == NULL) {
        free(scopes); free(seen);
        error(c, fn->span, "out of memory checking loop targets", "");
        c->failed = 1;
        return 0;
    }
    for(int i = 0; i < fn->stmt_count; i++) {
        const ZirStmt *st = &fn->stmts[i];
        if(st->kind == ZIR_STMT_BLOCK_CLOSE) {
            if(depth == 0) {
                error(c, st->span, "unexpected block close", "");
                ok = 0;
                break;
            }
            depth--;
        }
        if(st->loop_id < 0 || st->target_id < 0 ||
           (st->loop_id && st->kind != ZIR_STMT_WHILE) ||
           (st->target_id && st->kind != ZIR_STMT_BREAK &&
            st->kind != ZIR_STMT_CONTINUE)) {
            error(c, st->span, "invalid loop control metadata", "");
            ok = 0;
            break;
        }
        if(st->kind == ZIR_STMT_BREAK || st->kind == ZIR_STMT_CONTINUE) {
            int found = 0;
            for(int scope = depth - 1; scope >= 0; scope--)
                if(st->target_id ? scopes[scope] == st->target_id :
                   scopes[scope] != 0) {
                    found = 1;
                    break;
                }
            if(!found) {
                error(c, st->span, "loop control requires an enclosing target", st->text);
                ok = 0;
                break;
            }
        }
        if(st->kind == ZIR_STMT_WHILE) {
            if(st->loop_id > fn->stmt_count ||
               (st->loop_id && seen[st->loop_id])) {
                error(c, st->span, "duplicate or invalid loop target", st->text);
                ok = 0;
                break;
            }
            if(st->loop_id) seen[st->loop_id] = 1;
            scopes[depth++] = st->loop_id ? st->loop_id : -1;
        } else if(st->kind == ZIR_STMT_IF ||
                  st->kind == ZIR_STMT_BLOCK_OPEN) {
            scopes[depth++] = 0;
        }
    }
    if(ok && depth != 0) {
        error(c, fn->span, "unclosed block in function", fn->name);
        ok = 0;
    }
    free(scopes); free(seen);
    if(!ok) c->failed = 1;
    return ok;
}

static int
call_must_use(Checker *c, const ZirExpr *call)
{
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    const char *name = strrchr(call->name, '.');

    if(call->slot_type[0])
        return 0;
    if(ResolveFunctionAt(c->module, call->name, call->span.path,
                         &owner, &callee) == 1 && callee != NULL)
        return callee->must_use;
    name = name != NULL ? name + 1 : call->name;
    for(int i = 0; i < c->specialization_count; i++) {
        const SpecializationRequest *request = &c->specializations[i];
        if(strcmp(request->name, name) == 0)
            return request->template_owner->functions[
                request->template_index].must_use;
    }
    for(int i = 0; i < c->module->import_count; i++) {
        const ZirImport *import = &c->module->imports[i];
        if(import->kind == ZIR_IMPORT_EXTERN &&
           in_lookup_file(c->module, import->is_file_private,
                          import->span) &&
           strcmp(import->name, call->name) == 0)
            return import->must_use;
    }
    return 0;
}

static int
discarded_must_call(Checker *c, int index)
{
    if(index < 0)
        return -1;
    const ZirExpr *expression = &c->fn->exprs[index];
    if(expression->kind == ZIR_EXPR_CALL)
        return call_must_use(c, expression) ? index : -1;
    int found = discarded_must_call(c, expression->left);
    if(found >= 0)
        return found;
    found = discarded_must_call(c, expression->right);
    if(found >= 0)
        return found;
    found = discarded_must_call(c, expression->third);
    if(found >= 0)
        return found;
    for(int child = expression->first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        found = discarded_must_call(c, child);
        if(found >= 0)
            return found;
    }
    return -1;
}

/* A conversion applies when its single parameter accepts `from` and its
 * result is exactly `to`. */
static int
conversion_matches(Checker *c, const ZirFunction *conversion,
                   const char *from, const char *to)
{
    char parameters[64][ZIR_TEXT_MAX];
    int count;
    char *colon;
    if(conversion->is_template || conversion->is_extern ||
       strcmp(conversion->return_type, to) != 0)
        return 0;
    count = *skip_ws(conversion->args) ?
        split_top_level(conversion->args, parameters[0], 64,
                        sizeof(parameters[0])) : 0;
    if(count != 1)
        return 0;
    colon = strchr(parameters[0], ':');
    if(colon == NULL)
        return 0;
    return compatible(skip_ws(colon + 1), from);
}

/* Rewrite an incompatible expression into a call on a visible `#as`
 * conversion from its own type to `to`. The original expression moves into
 * the call's first argument; parent references keep their index. Returns
 * the conversion's result type, or NULL when no unique conversion applies. */
static const char *
try_conversion(Checker *c, int index, const char *to, ZirSourceSpan span)
{
    const char *from = c->fn->exprs[index].type;
    const ZirFunction *conversion = NULL;
    const ZirModule *owner = NULL;
    if(!*from || !strcmp(from, to) || c->inference_only)
        return NULL;
    for(int pass = 0; pass < 2; pass++) {
        const ZirModule *scope = pass == 0 ? c->module : NULL;
        if(pass != 0) {
            for(int i = 0; i < c->module->import_count; i++) {
                const ZirImport *import = &c->module->imports[i];
                if((import->kind != ZIR_IMPORT_OPEN &&
                    !(import->kind == ZIR_IMPORT_MODULE &&
                      import->is_using)) ||
                   import->resolved_module == NULL)
                    continue;
                for(int f = 0; f < import->resolved_module->function_count;
                    f++) {
                    const ZirFunction *candidate =
                        &import->resolved_module->functions[f];
                    if(candidate->is_conversion && candidate->is_public &&
                       conversion_matches(c, candidate, from, to)) {
                        if(conversion != NULL && conversion != candidate) {
                            error(c, span, "ambiguous #as conversion", to);
                            return NULL;
                        }
                        conversion = candidate;
                        owner = import->resolved_module;
                    }
                }
            }
            continue;
        }
        for(int f = 0; f < scope->function_count; f++) {
            const ZirFunction *candidate = &scope->functions[f];
            if(!candidate->is_conversion ||
               !conversion_matches(c, candidate, from, to))
                continue;
            if(conversion != NULL && conversion != candidate) {
                error(c, span, "ambiguous #as conversion", to);
                return NULL;
            }
            conversion = candidate;
            owner = scope;
        }
    }
    if(conversion == NULL)
        return NULL;
    c->conversions_applied = 1;
    (void)owner;
    {
        int chain_next = c->fn->exprs[index].next_sibling;
        int saved_argument = c->fn->exprs[index].argument_index;
        ZirExpr *copy_slot = FunctionAddExpr(c->fn, c->fn->exprs[index].kind,
                                             c->fn->exprs[index].name,
                                             c->fn->exprs[index].span);
        ZirExpr *call;
        ZirExpr saved;
        int copy_index;
        if(copy_slot == NULL) {
            c->failed = 1;
            return NULL;
        }
        saved = c->fn->exprs[index];
        *copy_slot = saved;
        copy_slot->next_sibling = -1;
        copy_slot->argument_index = 0;
        copy_index = (int)(copy_slot - c->fn->exprs);
        call = &c->fn->exprs[index];
        memset(call, 0, sizeof(*call));
        call->kind = ZIR_EXPR_CALL;
        copy_text(call->name, sizeof(call->name), conversion->name);
        call->argument_index = saved_argument;
        call->first_child = copy_index;
        call->left = -1;
        call->right = -1;
        call->third = -1;
        call->next_sibling = chain_next;
        call->span = saved.span;
        copy_text(call->type, sizeof(call->type), conversion->return_type);
        return call->type;
    }
}

/* Rewritten #as calls append their argument after the call node, but the IR
 * layout requires every child to precede its parent and sibling chains to
 * ascend. Re-emit each statement tree in postorder with remapped links. */
static int
rebuild_postorder(ZirFunction *fn, int index, ZirExpr *out, int *count,
                  int *remap)
{
    int self;
    ZirExpr *node;
    if(index < 0 || index >= fn->expr_count)
        return 1;
    if(remap[index] >= 0)
        return 1;
    node = &fn->exprs[index];
    {
        int left = node->left, right = node->right, third = node->third;
        int child = node->first_child;
        if(!rebuild_postorder(fn, left, out, count, remap) ||
           !rebuild_postorder(fn, right, out, count, remap) ||
           !rebuild_postorder(fn, third, out, count, remap))
            return 0;
        while(child >= 0) {
            int next = fn->exprs[child].next_sibling;
            if(!rebuild_postorder(fn, child, out, count, remap))
                return 0;
            child = next;
        }
    }
    self = *count;
    out[self] = fn->exprs[index];
    remap[index] = self;
    node = &out[self];
    node->left = node->left >= 0 ? remap[node->left] : -1;
    node->right = node->right >= 0 ? remap[node->right] : -1;
    node->third = node->third >= 0 ? remap[node->third] : -1;
    node->first_child = node->first_child >= 0 ? remap[node->first_child] : -1;
    node->next_sibling = -1;
    (*count)++;
    return 1;
}

static int
rebuild_conversion_layout(ZirFunction *fn)
{
    ZirExpr *out = calloc((size_t)fn->expr_count, sizeof(*out));
    int *remap = calloc((size_t)fn->expr_count, sizeof(*remap));
    int count = 0;
    if(out == NULL || remap == NULL) {
        free(out);
        free(remap);
        return 0;
    }
    for(int i = 0; i < fn->expr_count; i++)
        remap[i] = -1;
    for(int s = 0; s < fn->stmt_count; s++) {
        ZirStmt *st = &fn->stmts[s];
        if(!rebuild_postorder(fn, st->expr_root, out, &count, remap) ||
           !rebuild_postorder(fn, st->lhs_root, out, &count, remap)) {
            free(out);
            free(remap);
            return 0;
        }
    }
    for(int s = 0; s < fn->stmt_count; s++) {
        ZirStmt *st = &fn->stmts[s];
        if(st->expr_root >= 0)
            st->expr_root = remap[st->expr_root];
        if(st->lhs_root >= 0)
            st->lhs_root = remap[st->lhs_root];
    }
    memcpy(fn->exprs, out, (size_t)count * sizeof(*out));
    fn->expr_count = count;
    free(out);
    free(remap);
    return 1;
}

static int
vec_primitive_name(const char *name)
{
    return !strcmp(name, "VecPush") || !strcmp(name, "VecClear") ||
           !strcmp(name, "VecFree") || !strcmp(name, "VecSwap") ||
           !strcmp(name, "VecPop") || !strcmp(name, "VecGet") ||
           !strcmp(name, "VecClone") || !strcmp(name, "VecSlice") ||
           !strcmp(name, "BuilderAppend") || !strcmp(name, "BuilderFinish");
}

/* Mark bindings whose ownership transfers in this statement: VecFree and
 * BuilderFinish consume their place, and an owned Vec passed as an ordinary
 * call argument moves into the callee. Runs after the statement checked
 * cleanly so erroneous calls cannot poison later statements. */
static void
mark_expr_moves(Checker *c, int index)
{
    const ZirExpr *e;
    int primitive;
    if(index < 0 || index >= c->fn->expr_count)
        return;
    e = &c->fn->exprs[index];
    if(e->kind == ZIR_EXPR_FIELD_INIT &&
       contains_vec(c->module, e->type, 0)) {
        int right = e->right;
        Binding *binding = lexical_owned_binding(c, right);
        if(binding != NULL) {
            if(binding->moved || binding->moved_path_count != 0) {
                error(c, c->fn->exprs[right].span,
                      "owned binding is used after moving", binding->name);
                return;
            }
            if(binding->borrow_count > 0) {
                error(c, e->span,
                      "cannot move a Vec with a live borrowed view",
                      binding->name);
                return;
            }
            binding->moved = 1;
            c->fn->exprs[right].is_move = 1;
            return;
        }
        if(mark_moved_member_path(c, right))
            return;
        if(c->fn->exprs[right].kind == ZIR_EXPR_COMPOUND ||
           c->fn->exprs[right].kind == ZIR_EXPR_CALL) {
            mark_expr_moves(c, right);
            return;
        }
        error(c, c->fn->exprs[right].span,
              "owned record field moves a binding or takes a call result",
              e->name);
        return;
    }
    if(e->kind == ZIR_EXPR_IDENT) {
        Binding *binding = lexical_vec_binding(c, index);
        if(binding != NULL && binding->moved)
            error(c, e->span, "Vec binding is used after moving", e->name);
        return;
    }
    primitive = e->kind == ZIR_EXPR_CALL && vec_primitive_name(e->name);
    Binding *storage = primitive && e->first_child >= 0 ?
        lexical_vec_binding(c, e->first_child) : NULL;
    int storage_was_moved = storage != NULL && storage->moved;
    for(int child = e->first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        Binding *binding = e->kind == ZIR_EXPR_CALL ?
            lexical_owned_binding(c, child) : NULL;
        int consumes = !primitive || !strcmp(e->name, "VecFree") ||
                       !strcmp(e->name, "BuilderFinish");
        if(binding != NULL && consumes) {
            if(binding->moved || binding->moved_path_count != 0) {
                error(c, c->fn->exprs[child].span,
                      "owned binding is used after moving", binding->name);
                continue;
            }
            if(binding->borrow_count > 0)
                error(c, e->span,
                      "cannot move a Vec with a live borrowed view",
                      binding->name);
            binding->moved = 1;
            if(!primitive)
                c->fn->exprs[child].is_move = 1;
        } else if(consumes && mark_moved_member_path(c, child))
            continue;
        else
            mark_expr_moves(c, child);
    }
    if(primitive && storage != NULL && !storage_was_moved &&
       storage->moved && strcmp(e->name, "VecFree") &&
       strcmp(e->name, "BuilderFinish"))
        error(c, e->span, "Vec operation moves its storage in an argument",
              storage->name);
    mark_expr_moves(c, e->left);
    mark_expr_moves(c, e->right);
    mark_expr_moves(c, e->third);
}

/* A fresh vector returned by a call has one owner. It can become a binding,
 * return value, or ordinary call argument, or be dropped as a whole statement.
 * Reading a member of an unbound temporary would otherwise lose its owner. */
static void
check_vec_call_results(Checker *c, int index, int transferred, int discarded)
{
    if(index < 0 || index >= c->fn->expr_count)
        return;
    const ZirExpr *expr = &c->fn->exprs[index];
    if(expr->kind == ZIR_EXPR_CALL &&
       contains_vec(c->module, expr->type, 0) &&
       !transferred && !discarded)
        error(c, expr->span,
              "temporary owned result must be bound or passed to an owning call",
              expr->name);
    for(int child = expr->first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        int child_transfer =
            (expr->kind == ZIR_EXPR_CALL &&
             !vec_primitive_name(expr->name) &&
             contains_vec(c->module, c->fn->exprs[child].type, 0)) ||
            (expr->kind == ZIR_EXPR_COMPOUND &&
             c->fn->exprs[child].kind == ZIR_EXPR_FIELD_INIT &&
             contains_vec(c->module, c->fn->exprs[child].type, 0));
        check_vec_call_results(c, child, child_transfer, 0);
    }
    check_vec_call_results(c, expr->left, 0, 0);
    check_vec_call_results(c, expr->right, 0, 0);
    check_vec_call_results(c, expr->third, 0, 0);
}

static int
check_function(Checker *c, ZirFunction *fn)
{
    int errors_before = c->errors;
    int has_slots;
    int has_arrays;
    char params[64][ZIR_TEXT_MAX];
    int n;
    c->fn = fn;
    if(contains_vec(c->module, fn->return_type, 0) && fn->is_extern)
        error(c, fn->span, "Vec cannot cross an extern signature", fn->name);
    select_lookup_file(c->module, fn->span);
    const ZirType *return_slot = FindType(c->module, c->fn->return_type, NULL);
    if(return_slot != NULL && return_slot->is_record_template) {
        Diagnostic(c->fn->span, "check.specialize",
                   "generic types require a concrete specialization: %s",
                   c->fn->return_type);
        return 0;
    }
    /* Source expressions need imported types to disambiguate casts. Saved
     * IR already contains the checked graph: type-check that graph directly
     * so its statement text cannot redefine program meaning. */
    if(!c->fn->from_ir)
        StructureFunction(c->fn, c->module);
    if(!reserve_compound_constants(c->module, c->fn)) {
        error(c, fn->span, "too many aggregate constant expressions",
              fn->name);
        return 0;
    }
restart:
    for(int expression = 0; expression < fn->expr_count; expression++)
        fn->exprs[expression].is_move = 0;
    while(c->restore_count > 0)
        free(c->restores[--c->restore_count].states);
    c->count = 0; c->depth = 0;
    c->using_rewritten = 0;
    c->aggregate_rewritten = 0;
    if(!fn->from_ir || fn->is_specialization)
        for(int i = 0; i < c->module->using_count; i++) {
            const ZirUsing *using = &c->module->usings[i];
            if(!in_lookup_file(c->module, using->is_file_private,
                               using->span))
                continue;
            activate_using_filtered(c, using->path, using->filter,
                                    using->span);
        }
    has_slots = 0;
    has_arrays = fn->return_type[0] == '[';
    fn->uses_host = fn->is_extern && fn->extern_kind == ZIR_EXTERN_HOST;
    n = *skip_ws(c->fn->args) ? split_top_level(c->fn->args, params[0], 64, sizeof(params[0])) : 0;
    for(int a = 0; a < n; a++) {
        char *colon = strchr(params[a], ':');
        if(colon) {
            *colon++ = 0; trim_in_place(params[a]); trim_in_place(colon);
            const ZirType *parameter_type = FindType(c->module, colon, NULL);
            if(parameter_type != NULL && parameter_type->is_record_template) {
                Diagnostic(c->fn->span, "check.specialize",
                           "generic types require a concrete specialization: %s",
                           colon);
                return 0;
            }
            has_slots |= parameter_type != NULL && parameter_type->is_procedure_type;
            if(contains_vec(c->module, colon, 0) && fn->is_extern)
                error(c, fn->span, "Vec cannot cross an extern signature", params[a]);
            has_arrays |= ArrayValueType(colon) || SliceElementType(colon, NULL, 0);
            bind(c, params[a], colon, c->fn->span);
            if((!fn->from_ir || fn->is_specialization) &&
               (fn->using_parameters & (UINT64_C(1) << a)))
                activate_using(c, params[a], fn->span);
        } else error(c, c->fn->span, "parameters require name: type", params[a]);
    }
    c->restore_count = 0;
    for(int i = 0; i < c->fn->stmt_count; i++) {
        ZirStmt *st = &c->fn->stmts[i];
        int errors_at_statement;
        c->current_stmt = st;
        const char *type;
        if(st->kind == ZIR_STMT_IF && c->restore_count < 64) {
            /* Moves inside an if whose every arm returns never reach the
             * join; the state after it is the state before it. */
            int last = i;
            if(all_arms_return(fn, i, &last)) {
                MoveState *states = calloc((size_t)(c->count > 0 ?
                                                      c->count : 1),
                                            sizeof(*states));
                if(states != NULL) {
                    for(int b = 0; b < c->count; b++) {
                        states[b].moved = c->bindings[b].moved;
                        states[b].moved_path_count =
                            c->bindings[b].moved_path_count;
                        memcpy(states[b].moved_paths,
                               c->bindings[b].moved_paths,
                               sizeof(states[b].moved_paths));
                    }
                    c->restores[c->restore_count].at = last;
                    c->restores[c->restore_count].count = c->count;
                    c->restores[c->restore_count].states = states;
                    c->restore_count++;
                }
            }
        }
        for(int r = 0; r < c->restore_count; r++) {
            if(c->restores[r].at != i)
                continue;
            for(int b = 0; b < c->restores[r].count && b < c->count; b++) {
                c->bindings[b].moved =
                    c->restores[r].states[b].moved != 0;
                c->bindings[b].moved_path_count =
                    c->restores[r].states[b].moved_path_count;
                memcpy(c->bindings[b].moved_paths,
                       c->restores[r].states[b].moved_paths,
                       sizeof(c->bindings[b].moved_paths));
            }
            free(c->restores[r].states);
            c->restores[r] = c->restores[c->restore_count - 1];
            c->restore_count--;
            r--;
        }
        if(st->kind == ZIR_STMT_BLOCK_CLOSE) {
            while(c->count && c->bindings[c->count - 1].depth == c->depth) {
                Binding *popping = &c->bindings[c->count - 1];
                if(popping->borrows_index >= 0 &&
                   popping->borrows_index < c->count - 1)
                    c->bindings[popping->borrows_index].borrow_count--;
                c->count--;
            }
            if(c->depth) c->depth--;
        }
        if((!fn->from_ir || fn->is_specialization) &&
           st->is_using && st->kind == ZIR_STMT_EXPR) {
            activate_using_filtered(c, st->name, st->type, st->span);
            expression_type(c, st->expr_root);
            continue;
        }
        if(!fn->from_ir || fn->is_specialization) {
            promote_using_tree(c, st->lhs_root);
            promote_using_tree(c, st->expr_root);
        }
        if(st->kind == ZIR_STMT_DECL)
            normalize_array(c->module, st->type, sizeof(st->type));
        if(st->kind == ZIR_STMT_DECL)
            contextual_slot(c, st->expr_root, st->type);
        if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0 &&
           c->fn->exprs[st->lhs_root].kind == ZIR_EXPR_IDENT)
            contextual_slot(c, st->expr_root, lookup(c, c->fn->exprs[st->lhs_root].name));
        else if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0 &&
                (c->fn->exprs[st->lhs_root].kind == ZIR_EXPR_MEMBER ||
                 c->fn->exprs[st->lhs_root].kind == ZIR_EXPR_POINTER_MEMBER))
            contextual_slot(c, st->expr_root, expression_type(c, st->lhs_root));
        c->expected_type[0] = '\0';
        if(st->kind == ZIR_STMT_DECL)
            copy_text(c->expected_type, sizeof(c->expected_type), st->type);
        else if(st->kind == ZIR_STMT_RETURN)
            copy_text(c->expected_type, sizeof(c->expected_type), c->fn->return_type);
        else if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0) {
            c->assign_destination = 1;
            copy_text(c->expected_type, sizeof(c->expected_type),
                      expression_type(c, st->lhs_root));
            c->assign_destination = 0;
        }
        type = expression_type(c, st->expr_root);
        c->expected_type[0] = '\0';
        errors_at_statement = c->errors;
        if(st->kind == ZIR_STMT_EXPR || st->kind == ZIR_STMT_UNUSED) {
            int discarded = discarded_must_call(c, st->expr_root);
            if(discarded >= 0) {
                const ZirExpr *call = &fn->exprs[discarded];
                error(c, call->span, "#must return value is ignored",
                      call->name);
            }
        }
        if(st->kind == ZIR_STMT_IF_CASE) {
            const ZirType *enumeration = FindType(c->module, type, NULL);
            int complete_case = starts_word(skip_ws(st->text + 2), "#complete");
            if(fn->from_ir || c->errors != errors_before ||
               !starts_word(st->text, "if") ||
               strstr(st->text, "==") == NULL ||
               (enumeration == NULL && !scalar_case_type(type)) ||
               (enumeration != NULL && !enumeration->is_enum) ||
               (enumeration == NULL && complete_case)) {
                if_case_error(c, st->span,
                              "if-case requires a checked enum or scalar value", type);
                return 0;
            }
            if(!lower_if_case(c, i, type)) return 0;
            StructureFunction(fn, c->module);
            goto restart;
        }
        c->destination_was_moved = -1;
        if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0) {
            Binding *destination = lexical_owned_binding(c, st->lhs_root);
            c->destination_was_moved = destination != NULL && destination->moved;
        }
        if(c->errors == errors_at_statement && st->expr_root >= 0) {
            Binding *moved_from = lexical_owned_binding(c, st->expr_root);
            int consumed_root = 0;
            if(moved_from != NULL && moved_from->moved_path_count != 0 &&
               (st->kind == ZIR_STMT_DECL || st->kind == ZIR_STMT_ASSIGN ||
                st->kind == ZIR_STMT_RETURN)) {
                error(c, st->span,
                      "owned aggregate is used after moving one of its fields",
                      moved_from->name);
                consumed_root = 1;
            }
            if(moved_from != NULL && moved_from->moved_path_count == 0 &&
               (st->kind == ZIR_STMT_DECL || st->kind == ZIR_STMT_ASSIGN ||
                st->kind == ZIR_STMT_RETURN)) {
                if(moved_from->borrow_count > 0)
                    error(c, st->span,
                          "cannot move a Vec with a live borrowed view",
                          moved_from->name);
                moved_from->moved = 1;
                c->fn->exprs[st->expr_root].is_move = 1;
                consumed_root = 1;
            }
            if(!consumed_root &&
               (st->kind == ZIR_STMT_DECL || st->kind == ZIR_STMT_ASSIGN ||
                st->kind == ZIR_STMT_RETURN))
                consumed_root = mark_moved_member_path(c, st->expr_root);
            if(!consumed_root)
                mark_expr_moves(c, st->expr_root);
            int root_transfer =
                (st->kind == ZIR_STMT_DECL || st->kind == ZIR_STMT_ASSIGN ||
                 st->kind == ZIR_STMT_RETURN) &&
                contains_vec(c->module, fn->exprs[st->expr_root].type, 0);
            int root_discard = st->kind == ZIR_STMT_EXPR ||
                               st->kind == ZIR_STMT_UNUSED;
            check_vec_call_results(c, st->expr_root, root_transfer,
                                   root_discard);
            if(root_discard &&
               contains_vec(c->module, fn->exprs[st->expr_root].type, 0) &&
               fn->exprs[st->expr_root].kind != ZIR_EXPR_IDENT &&
               fn->exprs[st->expr_root].kind != ZIR_EXPR_CALL)
                error(c, st->span,
                      "discarded Vec value must be a binding or call result",
                      st->text);
            /* An assignment into a moved-from Vec binding re-owns it with
             * the transferred or fresh value. The handoff form
             * `v = Take(v)` also re-owns: the right side moved this binding
             * into the call and stores the result back into it. */
            if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0) {
                Binding *destination = lexical_owned_binding(c, st->lhs_root);
                if(destination != NULL) {
                    if(destination->moved)
                        c->destination_was_moved = 1;
                    destination->moved = 0;
                }
            }
        }
        if(st->kind == ZIR_STMT_DECL) {
            if(!*st->type) copy_text(st->type, sizeof(st->type),
                !strcmp(type, "integer") ? "s64" : !strcmp(type, "real") ? "float64" : type);
            else if(!compatible_checked(c, st->type, type)) {
                const char *converted = try_conversion(c, st->expr_root,
                                                       st->type, st->span);
                if(converted != NULL)
                    type = converted;
                else
                    error(c, st->span, "initializer type mismatch", st->name);
            }
            if(!strcmp(st->type, "null"))
                error(c, st->span, "null requires an explicit pointer type", st->name);
            if(st->type[0] == '[') {
                const char *problem = local_storage_error(c->module, st->type);
                if(problem != NULL)
                    error(c, st->span, problem, st->name);
            }
            const ZirType *local_type = FindType(c->module, st->type, NULL);
            if(local_type == NULL && st->type[0] != '[' &&
               !TargetType(st->type, ZIR_C) &&
               !SliceElementType(st->type, NULL, 0)) {
                const char *base = st->type;
                int resolvable = 0;
                while(*base == '*')
                    base++;
                if(TargetType(base, ZIR_C) != NULL ||
                   FindType(c->module, base, NULL) != NULL)
                    resolvable = 1;
                if(!resolvable)
                    error(c, st->span, "unknown declaration type", st->type);
            }
            if(local_type != NULL && local_type->is_record_template)
                error(c, st->span,
                      "generic types require a concrete specialization",
                      st->type);
            if(local_type != NULL && local_type->is_procedure_type) {
                has_slots = 1;
                if(st->expr_root < 0) {
                    Diagnostic(st->span, "check.slot_initializer", "slot bindings require an initializer");
                    c->failed = 1;
                }
            }
            if(st->expr_root >= 0 &&
               contains_vec(c->module, st->type, 0) &&
               c->fn->exprs[st->expr_root].kind != ZIR_EXPR_IDENT &&
               c->fn->exprs[st->expr_root].kind != ZIR_EXPR_CALL &&
               !owned_initializer_shape(c, st->expr_root))
                error(c, st->span,
                      "Vec initialization moves a binding or takes a call result",
                      st->name);
            if(st->expr_root >= 0 &&
               contains_vec(c->module, st->type, 0) &&
               global_vec_source(c, st->expr_root))
                error(c, st->span,
                      "global Vec storage cannot move; use a local",
                      st->name);
            bind(c, st->name, st->type, st->span);
            /* A declared VecSlice view borrows its source until the view's
             * scope closes; the source cannot move or mutate meanwhile. */
            if(st->expr_root >= 0 &&
               c->fn->exprs[st->expr_root].kind == ZIR_EXPR_CALL &&
               !strcmp(c->fn->exprs[st->expr_root].name, "VecSlice") &&
               SliceElementType(st->type, NULL, 0)) {
                int vector = c->fn->exprs[st->expr_root].first_child;
                if(vector >= 0 &&
                   c->fn->exprs[vector].kind == ZIR_EXPR_IDENT) {
                    for(int b = c->count - 2; b >= 0; b--)
                        if(!c->bindings[b].is_using_namespace &&
                           !strcmp(c->bindings[b].name,
                                   c->fn->exprs[vector].name) &&
                           owned_vec_binding_type(c, c->bindings[b].type)) {
                            c->bindings[c->count - 1].borrows_index = b;
                            c->bindings[b].borrow_count++;
                            break;
                        }
                }
            }
            if((!fn->from_ir || fn->is_specialization) && st->is_using)
                activate_using_filtered(c, st->name, st->type, st->span);
        } else if(st->kind == ZIR_STMT_ASSIGN) {
            c->assign_destination = 1;
            const char *lhs = expression_type(c, st->lhs_root);
            c->assign_destination = 0;
            if(contains_vec(c->module, lhs, 0)) {
                if(c->fn->exprs[st->lhs_root].kind != ZIR_EXPR_IDENT)
                    error(c, st->span,
                          "Vec assignment requires a simple binding destination",
                          st->text);
                else if(lexical_owned_binding(c, st->lhs_root) == NULL)
                    error(c, st->span,
                          "global Vec assignment is not supported; move it into a local first",
                          st->text);
                else if(st->expr_root >= 0 &&
                        c->fn->exprs[st->expr_root].kind != ZIR_EXPR_IDENT &&
                        c->fn->exprs[st->expr_root].kind != ZIR_EXPR_CALL &&
                        !owned_initializer_shape(c, st->expr_root))
                    error(c, st->span,
                          "Vec assignment moves a binding or takes a call result",
                          st->text);
                else if(st->expr_root >= 0 &&
                        global_vec_source(c, st->expr_root))
                    error(c, st->span,
                          "global Vec storage cannot move; use a local",
                          st->text);
                else if(c->destination_was_moved == 0)
                    error(c, st->span,
                          "assignment over an owned Vec leaks it; free or move it first",
                          c->fn->exprs[st->lhs_root].name);
            }
            const ZirType *destination = FindType(c->module, lhs, NULL);
            if(lhs[0] == '[' && strcmp(st->assignment_op, "="))
                error(c, st->span, "array compound assignment is not supported", st->assignment_op);
            if(destination != NULL && destination->is_enum &&
               !destination->is_enum_flags && strcmp(st->assignment_op, "="))
                error(c, st->span, "enum compound assignment requires an explicit numeric cast", st->assignment_op);
            if(destination != NULL && destination->is_enum_flags &&
               strcmp(st->assignment_op, "=") &&
               strcmp(st->assignment_op, "|=") &&
               strcmp(st->assignment_op, "&=") &&
               strcmp(st->assignment_op, "^=") &&
               strcmp(st->assignment_op, "+=") &&
               strcmp(st->assignment_op, "-="))
                error(c, st->span, "unsupported enum_flags compound assignment", st->assignment_op);
            if(text_type(lhs) && strcmp(st->assignment_op, "="))
                error(c, st->span, "string compound assignment is not supported", st->assignment_op);
            if(!assignable(c, st->lhs_root)) error(c, st->span, "assignment requires an assignable destination", "");
            if(readonly_text_destination(c, st->lhs_root))
                error(c, st->span, "collection count and borrowed string bytes are read-only", "");
            if(!compatible_checked(c, lhs, type)) {
                const char *converted = try_conversion(c, st->expr_root, lhs,
                                                       st->span);
                if(converted != NULL)
                    type = converted;
                else
                    error(c, st->span, "assignment type mismatch", st->text);
            }
        } else if(st->kind == ZIR_STMT_RETURN) {
            if(contains_vec(c->module, c->fn->return_type, 0) &&
               st->expr_root >= 0 &&
               fn->exprs[st->expr_root].kind != ZIR_EXPR_IDENT &&
               fn->exprs[st->expr_root].kind != ZIR_EXPR_CALL &&
               !owned_initializer_shape(c, st->expr_root))
                error(c, st->span,
                      "owned return moves a binding or takes a call result",
                      c->fn->name);
            if(!compatible_checked(c, c->fn->return_type, type)) {
                const char *converted = try_conversion(c, st->expr_root,
                                                       c->fn->return_type,
                                                       st->span);
                if(converted != NULL)
                    type = converted;
                else
                    error(c, st->span, "return type mismatch", c->fn->name);
            }
            if((st->expr_root < 0) != !strcmp(c->fn->return_type, "void"))
                error(c, st->span, "return value does not match function signature", c->fn->name);
            if(contains_vec(c->module, c->fn->return_type, 0) &&
               global_vec_source(c, st->expr_root))
                error(c, st->span,
                      "global Vec storage cannot move; use a local",
                      c->fn->exprs[st->expr_root].name);
        } else if(st->kind == ZIR_STMT_IF || st->kind == ZIR_STMT_WHILE) {
            if(st->expr_root < 0 &&
               (st->kind == ZIR_STMT_WHILE || !st->is_else ||
                starts_word(skip_ws(st->text + 4), "if")))
                error(c, st->span, "condition requires an expression", st->text);
            if(*type && strcmp(type, "bool")) error(c, st->span, "condition requires bool", type);
        } else if(st->kind == ZIR_STMT_UNKNOWN || st->kind == ZIR_STMT_FOR) {
            error(c, st->span, "statement is not supported by language checking", st->text);
        }
        if(st->kind == ZIR_STMT_BLOCK_OPEN || st->kind == ZIR_STMT_IF ||
           st->kind == ZIR_STMT_WHILE || st->kind == ZIR_STMT_FOR)
            c->depth++;
    }
    for(int i = 0; i < fn->expr_count; i++) {
        const ZirExpr *call = &fn->exprs[i];
        has_arrays |= SliceElementType(call->type, NULL, 0);
        if(call->kind != ZIR_EXPR_CALL)
            continue;
        const ZirFunction *callee = NULL;
        const ZirModule *owner = NULL;
        if(ResolveFunction(c->module, call->name, &owner, &callee) <= 0 ||
           callee == NULL || callee->is_extern)
            continue;
        has_arrays |= ArrayValueType(callee->return_type);
        char parameters[64][ZIR_TEXT_MAX];
        int count = *skip_ws(callee->args) ?
            split_top_level(callee->args, parameters[0], 64, sizeof(parameters[0])) : 0;
        for(int parameter = 0; parameter < count; parameter++) {
            const char *colon = strchr(parameters[parameter], ':');
            if(colon != NULL)
                has_arrays |= ArrayValueType(skip_ws(colon + 1));
        }
    }
    if(!fn->is_extern && fn->return_type[0] == '[' &&
       !sequence_returns(fn, 0, fn->stmt_count))
        error(c, fn->span, "array or slice result requires a return on every path", fn->name);
    if(c->conversions_applied &&
       !rebuild_conversion_layout(fn)) {
        error(c, fn->span, "cannot reorder #as conversion graph", fn->name);
        return 0;
    }
    c->conversions_applied = 0;
    if(!validate_loop_targets(c, fn))
        return 0;
    if((c->using_rewritten || c->aggregate_rewritten) &&
       !order_using_expressions(fn)) {
        error(c, fn->span, "cannot order checked expressions", fn->name);
        return 0;
    }
    c->fn->checked = c->errors == errors_before;
    for(int expression = 0; expression < c->fn->expr_count; expression++)
        has_slots |= c->fn->exprs[expression].is_function_value;
    /* Portable scalar emission classifies functions for bundles; a body the
     * emitter cannot express is excluded there and re-verified by the bundle
     * gate. Type checking itself stays complete, so native-only programs
     * with host-bound state (raw pointers, foreign handles) check clean. */
    if((has_slots || has_arrays) && !c->fn->is_extern && !c->fn->checked)
        c->fn->checked = 0;
    if(c->fn->checked && !c->fn->is_extern && !CanEmitBody(c->module, c->fn))
        c->fn->checked = 0;
    if(c->fn->checked) {
        fn->using_parameters = 0;
        for(int i = 0; i < fn->stmt_count; i++)
            fn->stmts[i].is_using = 0;
    }
    return !c->failed;
}

static int
check_template_declaration(Checker *c, ZirFunction *fn)
{
    c->fn = fn;
    select_lookup_file(c->module, fn->span);
    if(!fn->from_ir)
        StructureFunction(fn, c->module);
    if(!fn->template_param[0] || fn->is_extern || fn->exported ||
       strchr(fn->return_type, '$') != NULL) {
        Diagnostic(fn->span, "check.template",
                   "invalid polymorphic procedure declaration");
        return 0;
    }
    char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
    if(parameters == NULL) return 0;
    int count = *skip_ws(fn->args) ?
        split_top_level(fn->args, parameters[0], 64,
                        sizeof(parameters[0])) : 0;
    int binders = 0, valid = count > 0;
    uint64_t allowed_using = count >= 64 ? UINT64_MAX :
                             (UINT64_C(1) << count) - 1;
    valid = valid && (fn->using_parameters & ~allowed_using) == 0;
    for(int i = 0; i < count && valid; i++) {
        char *colon = strchr(parameters[i], ':');
        if(colon == NULL) { valid = 0; break; }
        char *type = colon + 1;
        trim_in_place(type);
        if(type[0] == '$') {
            if(strcmp(type + 1, fn->template_param)) valid = 0;
            else binders++;
        } else if(strchr(type, '$') != NULL)
            valid = 0;
    }
    free(parameters);
    if(!valid || binders == 0) {
        Diagnostic(fn->span, "check.template",
                   "polymorphic procedure requires a direct $Type parameter");
        return 0;
    }
    for(int i = 0; i < fn->stmt_count; i++)
        if(fn->stmts[i].kind == ZIR_STMT_UNKNOWN ||
           fn->stmts[i].kind == ZIR_STMT_FOR) {
            Diagnostic(fn->stmts[i].span, "check.template",
                       "unsupported statement in polymorphic procedure");
            return 0;
        }
    for(int i = 0; i < fn->expr_count; i++)
        if(fn->exprs[i].kind == ZIR_EXPR_UNKNOWN) {
            Diagnostic(fn->exprs[i].span, "check.template",
                       "unsupported expression in polymorphic procedure");
            return 0;
        }
    return validate_loop_targets(c, fn);
}

/* Resolve every declaration before checking bodies, so imported and forward
 * calls compare the same array shapes regardless of traversal order. */
static int
normalize_function_arrays(const ZirModule *module, ZirFunction *fn)
{
    char parts[64][ZIR_TEXT_MAX];
    char arguments[sizeof(fn->args)];
    size_t used = 0;
    int array_arguments = strchr(fn->args, '[') != NULL;
    int argument_alias_changed = 0;
    int count = *skip_ws(fn->args) ?
        split_top_level(fn->args, parts[0], 64, sizeof(parts[0])) : 0;
    arguments[0] = '\0';
    for(int i = -1; i < count; i++) {
        char *type = fn->return_type;
        size_t capacity = sizeof(fn->return_type);
        if(i >= 0) {
            char *colon = strchr(parts[i], ':');
            if(colon == NULL) {
                Diagnostic(fn->span, "check.signature", "parameters require name: type: %s", parts[i]);
                return 0;
            }
            type = colon + 1;
            trim_in_place(type);
            capacity = sizeof(parts[i]) - (size_t)(type - parts[i]);
        }
        char original_type[ZIR_TEXT_MAX];
        copy_text(original_type, sizeof(original_type), type);
        normalize_array(module, type, capacity);
        if(i >= 0 && strcmp(original_type, type) != 0)
            argument_alias_changed = 1;
        int host_buffer = i >= 0 && ArrayElementType(type, NULL, 0, NULL) &&
                          !ArrayValueType(type);
        if(type[0] == '[' && !host_buffer) {
            const char *problem = local_storage_error(module, type);
            if(problem == NULL && fn->is_extern &&
               (i < 0 || !SliceElementType(type, NULL, 0)))
                problem = "direct array signatures require an ordinary Ziran function";
            int bound = -1;
            if(problem == NULL && !SliceElementType(type, NULL, 0) &&
               array_capacity(module, type, &bound) != 1)
                problem = "array signatures require a resolved capacity";
            if(problem != NULL) {
                Diagnostic(fn->span, "check.array_signature", "%s: %s", problem, type);
                return 0;
            }
        }
        if(i >= 0) {
            int length = snprintf(arguments + used, sizeof(arguments) - used,
                                  "%s%s", used ? ", " : "", parts[i]);
            if(length < 0 || (size_t)length >= sizeof(arguments) - used) {
                Diagnostic(fn->span, "check.array_signature", "function signature exceeds size limit");
                return 0;
            }
            used += (size_t)length;
        }
    }
    if(array_arguments || argument_alias_changed)
        copy_text(fn->args, sizeof(fn->args), arguments);
    if(argument_alias_changed && fn->default_args[0]) {
        char (*defaults)[ZIR_TEXT_MAX] = calloc(64, sizeof(*defaults));
        if(defaults == NULL) return 0;
        int default_count = split_top_level(fn->default_args, defaults[0],
                                            64, sizeof(defaults[0]));
        char normalized[sizeof(fn->default_args)];
        size_t written = 0;
        normalized[0] = '\0';
        if(default_count != count) {
            free(defaults);
            Diagnostic(fn->span, "check.signature",
                       "default arguments do not match procedure parameters");
            return 0;
        }
        for(int i = 0; i < count; i++) {
            char *assignment = top_level_assignment(defaults[i]);
            const char *default_value = assignment == NULL ? "" :
                                        skip_ws(assignment + 1);
            int length = snprintf(normalized + written,
                                  sizeof(normalized) - written,
                                  "%s%s%s%s", i ? ", " : "", parts[i],
                                  assignment == NULL ? "" : " = ",
                                  default_value);
            if(length < 0 || (size_t)length >=
                             sizeof(normalized) - written) {
                free(defaults);
                Diagnostic(fn->span, "check.signature",
                           "default argument signature exceeds size limit");
                return 0;
            }
            written += (size_t)length;
        }
        copy_text(fn->default_args, sizeof(fn->default_args), normalized);
        free(defaults);
    }
    return 1;
}

int
LinkImports(ZirProgram **programs, int count)
{
    /* Open imports have no source alias, but checked types need a stable
     * qualifier when the consumer declares a type with the same name. */
    for(int p = 0; p < count; p++) {
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            int source_count = module->import_count;
            for(int i = 0; i < source_count; i++) {
                if(module->imports[i].kind != ZIR_IMPORT_OPEN)
                    continue;
                int existing = 0;
                for(int j = 0; j < module->import_count; j++)
                    if(module->imports[j].kind == ZIR_IMPORT_MODULE &&
                       strcmp(module->imports[j].signature,
                              "internal-open") == 0 &&
                       strcmp(module->imports[j].target,
                              module->imports[i].target) == 0) {
                        existing = 1;
                        break;
                    }
                if(existing) continue;
                char target[ZIR_PATH_MAX];
                copy_text(target, sizeof(target), module->imports[i].target);
                ZirSourceSpan span = module->imports[i].span;
                char alias[ZIR_NAME_MAX];
                for(int suffix = 0;; suffix++) {
                    int length = snprintf(alias, sizeof(alias),
                                          "__zi_open_%d", suffix);
                    if(length < 0 || (size_t)length >= sizeof(alias)) {
                        Diagnostic(span, "check.import",
                                   "too many internal open imports");
                        return 0;
                    }
                    int occupied = 0;
                    for(int j = 0; j < module->import_count; j++)
                        if(module->imports[j].kind == ZIR_IMPORT_MODULE &&
                           strcmp(module->imports[j].name, alias) == 0) {
                            occupied = 1;
                            break;
                        }
                    if(!occupied) break;
                }
                if(ModuleAddImport(module, ZIR_IMPORT_MODULE, alias, target,
                                   "internal-open", 0, span) == NULL) {
                    Diagnostic(span, "check.import",
                               "cannot allocate internal open import");
                    return 0;
                }
            }
        }
    }
    /* Every source import names a Ziran module in the current build. */
    for(int p = 0; p < count; p++) {
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int i = 0; i < module->import_count; i++) {
                ZirImport *import = &module->imports[i];
                import->resolved_module = NULL;
                if(import->kind == ZIR_IMPORT_MODULE)
                    for(int previous = 0; previous < i; previous++)
                        if(module->imports[previous].kind == ZIR_IMPORT_MODULE &&
                           strcmp(module->imports[previous].name,
                                  import->name) == 0) {
                            Diagnostic(import->span, "check.import",
                                       "duplicate import alias: %s", import->name);
                            return 0;
                        }
                if(import->kind == ZIR_IMPORT_EXTERN &&
                   SliceElementType(import->return_type, NULL, 0)) {
                    char element[ZIR_NAME_MAX];
                    if(!SliceElementType(import->return_type, element,
                                         sizeof(element)) ||
                       !*element || strchr(element, '[') ||
                       !strcmp(element, "char") ||
                       !strcmp(element, "const char") ||
                       (!*ScalarType(element) &&
                        FindType(module, element, NULL) == NULL)) {
                        Diagnostic(import->span, "check.slice_signature",
                                      "host slice returns need a supported element type");
                        return 0;
                    }
                }
                if(import->kind != ZIR_IMPORT_OPEN &&
                   import->kind != ZIR_IMPORT_MODULE)
                    continue;
                if(import->target[0] == '\0' ||
                   (!isalpha((unsigned char)import->target[0]) &&
                    import->target[0] != '_')) {
                    Diagnostic(import->span, "check.import",
                               "invalid Jai module name: %s", import->target);
                    return 0;
                }
                for(const unsigned char *cursor =
                        (const unsigned char *)import->target;
                    *cursor; cursor++)
                    if(!isalnum(*cursor) && *cursor != '_') {
                        Diagnostic(import->span, "check.import",
                                   "invalid Jai module name: %s", import->target);
                        return 0;
                    }
                for(int q = 0; q < count; q++) {
                    for(int n = 0; n < programs[q]->module_count; n++) {
                        const ZirModule *candidate = &programs[q]->modules[n];
                        char stem[ZIR_PATH_MAX];
                        size_t length;
                        copy_text(stem, sizeof(stem), candidate->source_path);
                        length = strlen(stem);
                        if(length > 3 && strcmp(stem + length - 3, ".zi") == 0)
                            stem[length - 3] = '\0';
                        if(strcmp(import->target, candidate->name) != 0 &&
                           strcmp(import->target, stem) != 0)
                            continue;
                        if(import->resolved_module && import->resolved_module != candidate) {
                            Diagnostic(import->span, "check.import", "ambiguous Ziran import: %s",
                                          import->target);
                            return 0;
                        }
                        import->resolved_module = candidate;
                    }
                }
                if(import->resolved_module == NULL) {
                    Diagnostic(import->span, "check.import",
                               "unresolved Jai module: %s", import->target);
                    return 0;
                }
            }
        }
    }
    return 1;
}

/* Resolve Jai type-constructor calls before the ordinary checker sees type
 * names. Concrete applications get a stable private name so the existing IR
 * and target backends can refer to the same instantiated record. */
static int
canonical_type_arguments(const char *source, char *output, size_t capacity)
{
    size_t used = 0;
    int space = 0;
    char quote = '\0';
    for(const unsigned char *p = (const unsigned char *)source; *p; p++) {
        if(quote == '\0' && isspace(*p)) {
            space = 1;
            continue;
        }
        if(space && used &&
           (isalnum((unsigned char)output[used - 1]) || output[used - 1] == '_') &&
           (isalnum(*p) || *p == '_')) {
            if(used + 1 >= capacity) return 0;
            output[used++] = ' ';
        }
        space = 0;
        if(used + 1 >= capacity) return 0;
        output[used++] = (char)*p;
        if(quote && *p == '\\' && p[1]) {
            if(used + 1 >= capacity) return 0;
            output[used++] = (char)*++p;
        } else if(quote && *p == (unsigned char)quote) {
            quote = '\0';
        } else if(!quote && (*p == '"' || *p == '\'')) {
            quote = (char)*p;
        }
    }
    if(used >= capacity) return 0;
    output[used] = '\0';
    return 1;
}

static int
rewrite_type_applications(ZirModule *module, const char *source,
                          char *output, size_t capacity,
                          ZirSourceSpan span, int recursion)
{
    size_t used = 0;
    select_lookup_file(module, span);
    if(recursion > 16) {
        Diagnostic(span, "check.type_application", "type application is nested too deeply");
        return 0;
    }
    for(const char *cursor = source; *cursor; ) {
        if(*cursor == '"' || *cursor == '\'') {
            char quote = *cursor;
            if(used + 1 >= capacity) return 0;
            output[used++] = *cursor++;
            while(*cursor) {
                char next = *cursor++;
                if(used + 1 >= capacity) return 0;
                output[used++] = next;
                if(next == '\\' && *cursor) {
                    if(used + 1 >= capacity) return 0;
                    output[used++] = *cursor++;
                } else if(next == quote) {
                    break;
                }
            }
            continue;
        }
        if(isalpha((unsigned char)*cursor) || *cursor == '_') {
            const char *start = cursor;
            while(isalnum((unsigned char)*cursor) || *cursor == '_') cursor++;
            size_t length = (size_t)(cursor - start);
            const char *opening = skip_ws(cursor);
            char base[ZIR_NAME_MAX];
            const ZirType *generic = NULL;
            if(length < sizeof(base) && *opening == '(') {
                memcpy(base, start, length);
                base[length] = '\0';
                generic = FindType(module, base, NULL);
            }
            if(generic != NULL && generic->is_record_template) {
                const char *closing = opening + 1;
                int depth = 1;
                while(*closing && depth) {
                    if(*closing == '(') depth++;
                    else if(*closing == ')') depth--;
                    if(depth) closing++;
                }
                if(depth || closing == opening + 1 ||
                   (size_t)(closing - opening - 1) >= ZIR_TEXT_MAX) {
                    Diagnostic(span, "check.type_application", "invalid type application: %s", base);
                    return 0;
                }
                char arguments[ZIR_TEXT_MAX];
                char expanded[ZIR_TEXT_MAX];
                memcpy(arguments, opening + 1,
                       (size_t)(closing - opening - 1));
                arguments[closing - opening - 1] = '\0';
                if(!rewrite_type_applications(module, arguments, expanded,
                        sizeof(expanded), span, recursion + 1))
                    return 0;
                char canonical[ZIR_TEXT_MAX];
                if(!canonical_type_arguments(expanded, canonical,
                        sizeof(canonical))) {
                    Diagnostic(span, "check.type_application",
                               "type application arguments are too long: %s", base);
                    return 0;
                }
                uint64_t hash = UINT64_C(14695981039346656037);
                for(const unsigned char *p = (const unsigned char *)base; *p; p++)
                    hash = (hash ^ *p) * UINT64_C(1099511628211);
                hash = (hash ^ '(') * UINT64_C(1099511628211);
                for(const unsigned char *p = (const unsigned char *)canonical; *p; p++)
                    hash = (hash ^ *p) * UINT64_C(1099511628211);
                char name[ZIR_NAME_MAX];
                snprintf(name, sizeof(name), "__type_%016llx",
                         (unsigned long long)hash);
                ZirType *instance = NULL;
                for(int t = 0; t < module->type_count; t++)
                    if(strcmp(module->types[t].name, name) == 0) {
                        instance = &module->types[t];
                        break;
                    }
                if(instance != NULL &&
                   (!instance->is_synthetic_application ||
                    (instance->is_type_instance &&
                     (strcmp(instance->template_name, base) != 0 ||
                      strcmp(instance->template_args, canonical) != 0)))) {
                    Diagnostic(span, "check.type_application",
                               "type application name collision: %s", base);
                    return 0;
                }
                if(instance == NULL) {
                    int generic_public = generic->is_public;
                    int generic_file_private = generic->is_file_private;
                    instance = ModuleAddType(module, name, span);
                    if(instance == NULL) return 0;
                    instance->is_public = generic_public;
                    instance->is_file_private = generic_file_private;
                    instance->is_type_instance = 1;
                    instance->is_synthetic_application = 1;
                    copy_text(instance->template_name,
                              sizeof(instance->template_name), base);
                    copy_text(instance->template_args,
                              sizeof(instance->template_args), canonical);
                }
                size_t name_length = strlen(name);
                if(used + name_length >= capacity) return 0;
                memcpy(output + used, name, name_length);
                used += name_length;
                cursor = closing + 1;
                continue;
            }
            if(used + length >= capacity) return 0;
            memcpy(output + used, start, length);
            used += length;
            continue;
        }
        if(used + 1 >= capacity) return 0;
        output[used++] = *cursor++;
    }
    output[used] = '\0';
    return 1;
}

static int
rewrite_function_type_applications(ZirModule *module, ZirFunction *fn)
{
    char expanded[ZIR_TEXT_MAX * 2];
    if(!rewrite_type_applications(module, fn->args, expanded,
            sizeof(expanded), fn->span, 0)) return 0;
    if(strlen(expanded) >= sizeof(fn->args)) return 0;
    copy_text(fn->args, sizeof(fn->args), expanded);
    if(!rewrite_type_applications(module, fn->return_type, expanded,
            sizeof(expanded), fn->span, 0)) return 0;
    if(strlen(expanded) >= sizeof(fn->return_type)) return 0;
    copy_text(fn->return_type, sizeof(fn->return_type), expanded);
    for(int s = 0; s < fn->stmt_count; s++) {
        ZirStmt *statement = &fn->stmts[s];
        if(!rewrite_type_applications(module, statement->text, expanded,
                sizeof(expanded), statement->span, 0)) return 0;
        if(strlen(expanded) >= sizeof(statement->text)) return 0;
        copy_text(statement->text, sizeof(statement->text), expanded);
    }
    StructureFunction(fn, module);
    return 1;
}

static int
normalize_type_applications(ZirModule *module)
{
    char expanded[ZIR_TEXT_MAX * 2];
    int original_types = module->type_count;
    for(int t = 0; t < original_types; t++) {
        ZirType *type = &module->types[t];
        if(type->is_type_instance) {
            char arguments[sizeof(type->template_args)];
            copy_text(arguments, sizeof(arguments), type->template_args);
            if(!rewrite_type_applications(module, arguments,
                    expanded, sizeof(expanded), type->span, 0)) return 0;
            char canonical[sizeof(type->template_args)];
            if(!canonical_type_arguments(expanded, canonical,
                    sizeof(canonical)))
                return 0;
            copy_text(module->types[t].template_args,
                      sizeof(module->types[t].template_args), canonical);
        } else if(!type->is_record_template &&
                  !type->is_enum && type->body[0]) {
            char body[sizeof(type->body)];
            copy_text(body, sizeof(body), type->body);
            if(!rewrite_type_applications(module, body, expanded,
                    sizeof(expanded), type->span, 0)) return 0;
            copy_text(module->types[t].body,
                      sizeof(module->types[t].body), expanded);
        }
    }
    for(int g = 0; g < module->global_count; g++) {
        ZirGlobal *global = &module->globals[g];
        if(!rewrite_type_applications(module, global->type, expanded,
                sizeof(expanded), global->span, 0)) return 0;
        if(strlen(expanded) >= sizeof(global->type)) return 0;
        copy_text(global->type, sizeof(global->type), expanded);
    }
    for(int f = 0; f < module->function_count; f++) {
        ZirFunction *fn = &module->functions[f];
        if(fn->from_ir || fn->is_template) continue;
        if(!rewrite_function_type_applications(module, fn)) return 0;
    }
    return 1;
}

/* Native interfaces define records by value. Put a field's local record
 * before its owner, including records created by nested type application. */
static int
order_local_types(ZirModule *module)
{
    int count = module->type_count;
    if(count == 0) return 1;
    for(int pass = 0; pass < count * count; pass++) {
        int moved = 0;
        for(int t = 0; t < count && !moved; t++) {
            const ZirType *owner = &module->types[t];
            if(owner->is_enum || owner->is_record_template || owner->is_procedure_type)
                continue;
            size_t offset = 0;
            ZirTypeField field;
            while(TypeNextField(owner, &offset, &field) == 1) {
                for(int dependency = t + 1; dependency < count; dependency++) {
                    if(strcmp(module->types[dependency].name, field.type) != 0)
                        continue;
                    ZirType needed = module->types[dependency];
                    memmove(&module->types[t + 1], &module->types[t],
                            (size_t)(dependency - t) * sizeof(needed));
                    module->types[t] = needed;
                    moved = 1;
                    break;
                }
                if(moved) break;
            }
        }
        if(!moved) return 1;
    }
    Diagnostic(module->span, "check.type_order",
               "record values contain a cyclic type dependency");
    return 0;
}

/* Type spellings are validated after parsing and after generic expansion so
 * source and saved IR cannot disagree about which declarations are Jai. */
int
JaiTypeSpelling(ZirSourceSpan span, const char *type)
{
    const char *start = skip_ws(type);
    int brackets = 0, parens = 0;
    for(const char *p = start; *p;) {
        if(*p == '"' || *p == '\'') {
            char quote = *p++;
            while(*p && *p != quote) {
                if(*p == '\\' && p[1]) p++;
                p++;
            }
            if(*p) p++;
            continue;
        }
        if(isalpha((unsigned char)*p) || *p == '_') {
            const char *word = p;
            while(isalnum((unsigned char)*p) || *p == '_') p++;
            if(!brackets && (size_t)(p - word) == 5 &&
               !strncmp(word, "const", 5)) {
                Diagnostic(span, "check.jai_syntax",
                           "const qualifier is not Jai syntax: %s", type);
                return 0;
            }
            if(!brackets && (size_t)(p - word) == 4 &&
               !strncmp(word, "char", 4)) {
                Diagnostic(span, "check.jai_syntax",
                           "non-Jai primitive type spelling: char");
                return 0;
            }
            continue;
        }
        if(*p == '[') brackets++;
        else if(*p == ']' && brackets) brackets--;
        else if(*p == '(') parens++;
        else if(*p == ')' && parens) parens--;
        else if(*p == '=' && !brackets && !parens) break;
        else if(*p == '*' && !brackets) {
            const char *previous = p;
            while(previous > start && isspace((unsigned char)previous[-1]))
                previous--;
            if(previous > start &&
               (isalnum((unsigned char)previous[-1]) ||
                previous[-1] == '_' || previous[-1] == ')')) {
                Diagnostic(span, "check.jai_syntax",
                           "C-style pointer type is not Jai syntax; use *Type: %s",
                           type);
                return 0;
            }
        }
        p++;
    }
    return 1;
}

static int
jai_parameter_types(ZirSourceSpan span, const char *args)
{
    char parameters[64][ZIR_TEXT_MAX];
    int count = *skip_ws(args) ?
        split_top_level(args, parameters[0], 64, sizeof(parameters[0])) : 0;
    for(int i = 0; i < count; i++) {
        char *colon = strchr(parameters[i], ':');
        if(colon != NULL && !JaiTypeSpelling(span, colon + 1))
            return 0;
    }
    return 1;
}

static int
jai_module_types(const ZirModule *module)
{
    for(int i = 0; i < module->define_count; i++) {
        const ZirDefine *definition = &module->defines[i];
        const char *value = skip_ws(definition->value);
        size_t length = strlen(value);
        while(length > 0 && isspace((unsigned char)value[length - 1]))
            length--;
        if(length > 0 && value[length - 1] == '*' &&
           !JaiTypeSpelling(definition->span, value)) return 0;
    }
    for(int i = 0; i < module->global_count; i++)
        if(!JaiTypeSpelling(module->globals[i].span,
                              module->globals[i].type)) return 0;
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *imp = &module->imports[i];
        if(imp->kind == ZIR_IMPORT_EXTERN &&
           (!jai_parameter_types(imp->span, imp->args) ||
            !JaiTypeSpelling(imp->span, imp->return_type))) return 0;
    }
    for(int i = 0; i < module->type_count; i++) {
        const ZirType *record = &module->types[i];
        if(record->is_procedure_type) {
            if(!jai_parameter_types(record->span, record->body) ||
               !JaiTypeSpelling(record->span, record->procedure_return_type)) return 0;
            continue;
        }
        if(record->is_enum) {
            if(!EnumMemberValue(record, NULL, NULL)) {
                Diagnostic(record->span, "check.enum",
                           "invalid enum value or value outside backing type: %s",
                           record->name);
                return 0;
            }
            continue;
        }
        size_t offset = 0;
        ZirTypeField field;
        int status;
        while((status = TypeNextField(record, &offset, &field)) == 1)
            if(!JaiTypeSpelling(record->span, field.type)) return 0;
        if(status < 0) continue; /* existing record diagnostics report this */
    }
    for(int i = 0; i < module->function_count; i++) {
        const ZirFunction *fn = &module->functions[i];
        if(!jai_parameter_types(fn->span, fn->args) ||
           !JaiTypeSpelling(fn->span, fn->return_type)) return 0;
        for(int s = 0; s < fn->stmt_count; s++)
            if(fn->stmts[s].kind == ZIR_STMT_DECL &&
               fn->stmts[s].type[0] &&
               !JaiTypeSpelling(fn->stmts[s].span,
                                   fn->stmts[s].type)) return 0;
    }
    return 1;
}

typedef struct PrivateFunctionName {
    ZirFunction *function;
    char internal[ZIR_NAME_MAX];
} PrivateFunctionName;

static int
name_conflicts(const ZirModule *module, const PrivateFunctionName *renames,
               int count, const char *name)
{
    for(int i = 0; i < module->function_count; i++)
        if(strcmp(module->functions[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->global_count; i++)
        if(strcmp(module->globals[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->define_count; i++)
        if(strcmp(module->defines[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->type_count; i++)
        if(strcmp(module->types[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->import_count; i++)
        if(strcmp(module->imports[i].name, name) == 0)
            return 1;
    for(int i = 0; i < count; i++)
        if(strcmp(renames[i].internal, name) == 0)
            return 1;
    return 0;
}

/* File-private declarations may share a source name in different loaded
 * files. Give colliding procedures distinct checked IR names after source
 * resolution, so native backends and the portable linker use one identity. */
static int
name_private_functions(ZirModule *module)
{
    PrivateFunctionName *renames = calloc((size_t)module->function_count + 1,
                                          sizeof(*renames));
    int count = 0;
    if(renames == NULL)
        return 0;
    for(int f = 0; f < module->function_count; f++) {
        ZirFunction *function = &module->functions[f];
        if(!function->is_file_private)
            continue;
        int collision = 0;
        for(int other = 0; other < module->function_count; other++) {
            const ZirFunction *candidate = &module->functions[other];
            if(other == f || strcmp(candidate->name, function->name) != 0)
                continue;
            if(strcmp(candidate->span.path, function->span.path) == 0) {
                Diagnostic(function->span, "check.file_scope",
                           "duplicate procedure in one file: %s",
                           function->name);
                free(renames);
                return 0;
            }
            collision = 1;
        }
        if(!collision)
            continue;
        if(function->exported) {
            Diagnostic(function->span, "check.file_scope",
                       "colliding #program_export procedure: %s",
                       function->name);
            free(renames);
            return 0;
        }
        renames[count].function = function;
        for(int serial = f; ; serial++) {
            snprintf(renames[count].internal,
                     sizeof(renames[count].internal),
                     "zir_file_function_%d", serial);
            if(!name_conflicts(module, renames, count,
                               renames[count].internal))
                break;
        }
        count++;
    }
    if(count == 0) {
        free(renames);
        return 1;
    }
    for(int f = 0; f < module->function_count; f++) {
        ZirFunction *caller = &module->functions[f];
        for(int x = 0; x < caller->expr_count; x++) {
            ZirExpr *expression = &caller->exprs[x];
            const ZirModule *owner = NULL;
            const ZirFunction *target = NULL;
            if(!((expression->kind == ZIR_EXPR_CALL &&
                  !expression->slot_type[0]) ||
                 (expression->kind == ZIR_EXPR_IDENT &&
                  expression->is_function_value)) ||
               !expression->name[0] ||
               ResolveFunctionAt(module, expression->name,
                   caller->span.path, &owner, &target) != 1 ||
               owner != module)
                continue;
            for(int i = 0; i < count; i++)
                if(target == renames[i].function) {
                    copy_text(expression->name, sizeof(expression->name),
                              renames[i].internal);
                    break;
                }
        }
        caller->from_ir = 1;
    }
    for(int i = 0; i < count; i++)
        copy_text(renames[i].function->name,
                  sizeof(renames[i].function->name),
                  renames[i].internal);
    free(renames);
    return 1;
}

typedef struct PrivateDefineName {
    ZirDefine *definition;
    char original[ZIR_NAME_MAX];
    char internal[ZIR_NAME_MAX];
} PrivateDefineName;

static int
rewrite_private_reference(char *source, size_t capacity,
                          ZirSourceSpan span, const char *original,
                          const char *internal)
{
    ZirLexer lexer;
    ZirToken previous = {0};
    ZirToken current, next;
    size_t current_end, next_end, copied = 0, written = 0;
    char *output = calloc(capacity, 1);
    if(output == NULL) return 0;
    LexerInit(&lexer, source, span.path);
    current = LexerNext(&lexer);
    current_end = lexer.pos;
    next = LexerNext(&lexer);
    next_end = lexer.pos;
    while(current.kind != ZIR_TOKEN_EOF) {
        if(current.kind == ZIR_TOKEN_IDENT &&
           !strcmp(current.text, original) &&
           strcmp(previous.text, ".") != 0 &&
           strcmp(next.text, ":") != 0) {
            size_t start = current_end - strlen(current.text);
            size_t prefix = start - copied;
            size_t replacement = strlen(internal);
            if(written + prefix + replacement >= capacity) goto too_long;
            memcpy(output + written, source + copied, prefix);
            written += prefix;
            memcpy(output + written, internal, replacement);
            written += replacement;
            copied = current_end;
        }
        previous = current;
        current = next;
        current_end = next_end;
        next = LexerNext(&lexer);
        next_end = lexer.pos;
    }
    size_t suffix = strlen(source + copied);
    if(written + suffix >= capacity) goto too_long;
    memcpy(output + written, source + copied, suffix + 1);
    memcpy(source, output, written + suffix + 1);
    free(output);
    return 1;
too_long:
    Diagnostic(span, "check.file_scope",
               "file-private reference exceeds text limit: %s", original);
    free(output);
    return 0;
}

static int
name_private_defines(ZirModule *module)
{
    PrivateDefineName *renames = calloc((size_t)module->define_count + 1,
                                        sizeof(*renames));
    int count = 0;
    if(renames == NULL) return 0;
    for(int d = 0; d < module->define_count; d++) {
        ZirDefine *definition = &module->defines[d];
        if(!definition->is_file_private) continue;
        int collision = 0;
        for(int other = 0; other < module->define_count; other++) {
            const ZirDefine *candidate = &module->defines[other];
            if(other == d || strcmp(candidate->name, definition->name))
                continue;
            if(!strcmp(candidate->span.path, definition->span.path)) {
                Diagnostic(definition->span, "check.file_scope",
                           "duplicate constant in one file: %s",
                           definition->name);
                free(renames);
                return 0;
            }
            collision = 1;
        }
        if(!collision) continue;
        PrivateDefineName *rename = &renames[count];
        rename->definition = definition;
        copy_text(rename->original, sizeof(rename->original),
                  definition->name);
        for(int serial = d; ; serial++) {
            int reserved = 0;
            snprintf(rename->internal, sizeof(rename->internal),
                     "zir_file_constant_%d", serial);
            for(int i = 0; i < count; i++)
                reserved |= !strcmp(renames[i].internal, rename->internal);
            if(!reserved && !name_conflicts(module, NULL, 0,
                                            rename->internal)) break;
        }
        count++;
    }
    for(int r = 0; r < count; r++) {
        const PrivateDefineName *rename = &renames[r];
        const char *path = rename->definition->span.path;
        for(int g = 0; g < module->global_count; g++) {
            ZirGlobal *global = &module->globals[g];
            if(strcmp(global->span.path, path)) continue;
            if(!rewrite_private_reference(global->type,
                                          sizeof(global->type), global->span,
                                          rename->original, rename->internal) ||
               !rewrite_private_reference(global->init,
                                          sizeof(global->init), global->span,
                                          rename->original, rename->internal))
                goto failed;
        }
        for(int d = 0; d < module->define_count; d++) {
            ZirDefine *definition = &module->defines[d];
            if(strcmp(definition->span.path, path)) continue;
            if(!rewrite_private_reference(definition->value,
                                          sizeof(definition->value),
                                          definition->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int t = 0; t < module->type_count; t++) {
            ZirType *type = &module->types[t];
            if(strcmp(type->span.path, path)) continue;
            if(!rewrite_private_reference(type->body, sizeof(type->body),
                                          type->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(type->template_args,
                                          sizeof(type->template_args),
                                          type->span, rename->original,
                                          rename->internal)) goto failed;
        }
    }
    for(int r = 0; r < count; r++)
        copy_text(renames[r].definition->name,
                  sizeof(renames[r].definition->name),
                  renames[r].internal);
    free(renames);
    return 1;
failed:
    free(renames);
    return 0;
}

typedef struct PrivateGlobalName {
    ZirGlobal *global;
    char original[ZIR_NAME_MAX];
    char internal[ZIR_NAME_MAX];
} PrivateGlobalName;

static int
name_private_globals(ZirModule *module)
{
    PrivateGlobalName *renames = calloc((size_t)module->global_count + 1,
                                        sizeof(*renames));
    int count = 0;
    if(renames == NULL) return 0;
    for(int g = 0; g < module->global_count; g++) {
        ZirGlobal *global = &module->globals[g];
        if(!global->is_file_private) continue;
        int collision = 0;
        for(int other = 0; other < module->global_count; other++) {
            const ZirGlobal *candidate = &module->globals[other];
            if(other == g || strcmp(candidate->name, global->name))
                continue;
            if(!strcmp(candidate->span.path, global->span.path)) {
                Diagnostic(global->span, "check.file_scope",
                           "duplicate global in one file: %s", global->name);
                free(renames);
                return 0;
            }
            collision = 1;
        }
        if(!collision) continue;
        PrivateGlobalName *rename = &renames[count];
        rename->global = global;
        copy_text(rename->original, sizeof(rename->original), global->name);
        for(int serial = g; ; serial++) {
            int reserved = 0;
            snprintf(rename->internal, sizeof(rename->internal),
                     "zir_file_global_%d", serial);
            for(int i = 0; i < count; i++)
                reserved |= !strcmp(renames[i].internal, rename->internal);
            if(!reserved && !name_conflicts(module, NULL, 0,
                                            rename->internal)) break;
        }
        count++;
    }
    for(int r = 0; r < count; r++) {
        const PrivateGlobalName *rename = &renames[r];
        const char *path = rename->global->span.path;
        for(int g = 0; g < module->global_count; g++) {
            ZirGlobal *global = &module->globals[g];
            if(strcmp(global->span.path, path)) continue;
            if(!rewrite_private_reference(global->init, sizeof(global->init),
                                          global->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int f = 0; f < module->function_count; f++) {
            ZirFunction *caller = &module->functions[f];
            if(strcmp(caller->span.path, path)) continue;
            for(int x = 0; x < caller->expr_count; x++) {
                ZirExpr *expression = &caller->exprs[x];
                if(expression->is_global_value &&
                   !strcmp(expression->name, rename->original))
                    copy_text(expression->name, sizeof(expression->name),
                              rename->internal);
            }
        }
    }
    for(int r = 0; r < count; r++)
        copy_text(renames[r].global->name, sizeof(renames[r].global->name),
                  renames[r].internal);
    if(count > 0)
        for(int f = 0; f < module->function_count; f++)
            module->functions[f].from_ir = 1;
    free(renames);
    return 1;
failed:
    free(renames);
    return 0;
}

typedef struct PrivateTypeName {
    ZirType *type;
    char original[ZIR_NAME_MAX];
    char internal[ZIR_NAME_MAX];
} PrivateTypeName;

static int
template_parameter_shadows(const ZirType *type, const char *name)
{
    ZirLexer lexer;
    ZirToken current, next;
    LexerInit(&lexer, type->template_params, type->span.path);
    current = LexerNext(&lexer);
    next = LexerNext(&lexer);
    while(current.kind != ZIR_TOKEN_EOF) {
        if(current.kind == ZIR_TOKEN_IDENT &&
           !strcmp(current.text, name) && !strcmp(next.text, ":"))
            return 1;
        current = next;
        next = LexerNext(&lexer);
    }
    return 0;
}

static int
name_private_types(ZirModule *module)
{
    PrivateTypeName *renames = calloc((size_t)module->type_count + 1,
                                      sizeof(*renames));
    int count = 0;
    if(renames == NULL) return 0;
    for(int t = 0; t < module->type_count; t++) {
        ZirType *type = &module->types[t];
        if(!type->is_file_private) continue;
        int collision = 0;
        for(int other = 0; other < module->type_count; other++) {
            const ZirType *candidate = &module->types[other];
            if(other == t || strcmp(candidate->name, type->name)) continue;
            if(!strcmp(candidate->span.path, type->span.path)) {
                Diagnostic(type->span, "check.file_scope",
                           "duplicate type in one file: %s", type->name);
                free(renames);
                return 0;
            }
            collision = 1;
        }
        if(!collision) continue;
        PrivateTypeName *rename = &renames[count];
        rename->type = type;
        copy_text(rename->original, sizeof(rename->original), type->name);
        for(int serial = t; ; serial++) {
            int reserved = 0;
            snprintf(rename->internal, sizeof(rename->internal),
                     "zir_file_type_%d", serial);
            for(int i = 0; i < count; i++)
                reserved |= !strcmp(renames[i].internal, rename->internal);
            if(!reserved && !name_conflicts(module, NULL, 0,
                                            rename->internal)) break;
        }
        count++;
    }
    for(int r = 0; r < count; r++) {
        const PrivateTypeName *rename = &renames[r];
        const char *path = rename->type->span.path;
        for(int t = 0; t < module->type_count; t++) {
            ZirType *type = &module->types[t];
            if(strcmp(type->span.path, path)) continue;
            if(!type->is_enum &&
               !template_parameter_shadows(type, rename->original) &&
               !rewrite_private_reference(type->body, sizeof(type->body),
                                          type->span, rename->original,
                                          rename->internal)) goto failed;
            if(!rewrite_private_reference(type->template_name,
                                          sizeof(type->template_name),
                                          type->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(type->template_args,
                                          sizeof(type->template_args),
                                          type->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(type->procedure_return_type,
                                          sizeof(type->procedure_return_type),
                                          type->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int g = 0; g < module->global_count; g++) {
            ZirGlobal *global = &module->globals[g];
            if(strcmp(global->span.path, path)) continue;
            if(!rewrite_private_reference(global->type, sizeof(global->type),
                                          global->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(global->init, sizeof(global->init),
                                          global->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int d = 0; d < module->define_count; d++) {
            ZirDefine *definition = &module->defines[d];
            if(strcmp(definition->span.path, path)) continue;
            if(!rewrite_private_reference(definition->value,
                                          sizeof(definition->value),
                                          definition->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int i = 0; i < module->import_count; i++) {
            ZirImport *import = &module->imports[i];
            if(strcmp(import->span.path, path)) continue;
            if(!rewrite_private_reference(import->signature,
                                          sizeof(import->signature),
                                          import->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(import->args, sizeof(import->args),
                                          import->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(import->return_type,
                                          sizeof(import->return_type),
                                          import->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int f = 0; f < module->function_count; f++) {
            ZirFunction *function = &module->functions[f];
            if(strcmp(function->span.path, path)) continue;
            if(!rewrite_private_reference(function->args,
                                          sizeof(function->args), function->span,
                                          rename->original, rename->internal) ||
               !rewrite_private_reference(function->return_type,
                                          sizeof(function->return_type),
                                          function->span, rename->original,
                                          rename->internal)) goto failed;
            for(int s = 0; s < function->stmt_count; s++) {
                ZirStmt *statement = &function->stmts[s];
                if(!rewrite_private_reference(statement->type,
                                              sizeof(statement->type),
                                              statement->span, rename->original,
                                              rename->internal)) goto failed;
            }
            for(int x = 0; x < function->expr_count; x++) {
                ZirExpr *expression = &function->exprs[x];
                if(!rewrite_private_reference(expression->type,
                                              sizeof(expression->type),
                                              expression->span,
                                              rename->original,
                                              rename->internal) ||
                   !rewrite_private_reference(expression->slot_type,
                                              sizeof(expression->slot_type),
                                              expression->span,
                                              rename->original,
                                              rename->internal)) goto failed;
                if(expression->kind == ZIR_EXPR_CAST ||
                   expression->kind == ZIR_EXPR_COMPOUND ||
                   expression->kind == ZIR_EXPR_SIZE_OF ||
                   (expression->kind == ZIR_EXPR_CALL &&
                    rename->type->is_record_template))
                    if(!rewrite_private_reference(expression->name,
                                                  sizeof(expression->name),
                                                  expression->span,
                                                  rename->original,
                                                  rename->internal)) goto failed;
            }
        }
    }
    for(int r = 0; r < count; r++)
        copy_text(renames[r].type->name, sizeof(renames[r].type->name),
                  renames[r].internal);
    if(count > 0)
        for(int f = 0; f < module->function_count; f++)
            module->functions[f].from_ir = 1;
    free(renames);
    return 1;
failed:
    free(renames);
    return 0;
}

static int
substitute_field(char *field, size_t capacity, const char *parameter,
                 const char *concrete)
{
    char *copy = malloc(capacity);
    if(copy == NULL) return 0;
    memcpy(copy, field, capacity);
    int ok = replace_template_type(field, capacity, copy,
                                   parameter, concrete);
    free(copy);
    return ok;
}

static int
instantiate_specializations(Checker *checker)
{
    for(int request_index = 0;
        request_index < checker->specialization_count; request_index++) {
        SpecializationRequest *request =
            &checker->specializations[request_index];
        ZirModule *template_owner = request->template_owner;
        ZirModule *owner = request->instance_owner;
        ZirFunction original = template_owner->functions[request->template_index];
        ZirFunction *instance = ModuleAddFunction(owner, request->name,
            original.args, original.return_type, 0, original.span);
        if(instance == NULL) return 0;
        *instance = original;
        copy_text(instance->name, sizeof(instance->name), request->name);
        copy_text(instance->specialization_type,
                  sizeof(instance->specialization_type), request->type);
        instance->is_template = 0;
        instance->is_specialization = 1;
        instance->is_public = 1;
        instance->is_file_private = 0;
        instance->exported = 0;
        if(owner != template_owner)
            instance->span = request->call_span;
        instance->checked = 0;
        instance->from_ir = 1;
        instance->uses_host = 0;
        instance->stmts = NULL;
        instance->exprs = NULL;
        instance->stmt_cap = instance->stmt_count;
        instance->expr_cap = instance->expr_count;
        if(instance->stmt_count > 0) {
            instance->stmts = malloc((size_t)instance->stmt_count *
                                     sizeof(*instance->stmts));
            if(instance->stmts == NULL) return 0;
            memcpy(instance->stmts, original.stmts,
                   (size_t)instance->stmt_count * sizeof(*instance->stmts));
        }
        if(instance->expr_count > 0) {
            instance->exprs = malloc((size_t)instance->expr_count *
                                     sizeof(*instance->exprs));
            if(instance->exprs == NULL) return 0;
            memcpy(instance->exprs, original.exprs,
                   (size_t)instance->expr_count * sizeof(*instance->exprs));
        }
        const char *parameter = original.template_param;
        const char *concrete = request->type;
        /* A second check of saved IR must see the same array signature. */
        if(!substitute_field(instance->args, sizeof(instance->args),
                             parameter, concrete) ||
           !substitute_field(instance->default_args,
                             sizeof(instance->default_args),
                             parameter, concrete) ||
           !substitute_field(instance->return_type,
                             sizeof(instance->return_type),
                             parameter, concrete) ||
           !normalize_function_arrays(owner, instance)) return 0;
        for(int s = 0; s < instance->stmt_count; s++)
            if(!substitute_field(instance->stmts[s].type,
                                 sizeof(instance->stmts[s].type),
                                 parameter, concrete)) return 0;
        for(int x = 0; x < instance->expr_count; x++) {
            ZirExpr *expression = &instance->exprs[x];
            if(!substitute_field(expression->type,
                                 sizeof(expression->type),
                                 parameter, concrete) ||
               !substitute_field(expression->slot_type,
                                 sizeof(expression->slot_type),
                                 parameter, concrete)) return 0;
            if(expression->kind == ZIR_EXPR_CAST ||
               expression->kind == ZIR_EXPR_COMPOUND ||
               expression->kind == ZIR_EXPR_SIZE_OF ||
               expression->kind == ZIR_EXPR_CALL)
                if(!substitute_field(expression->name,
                                     sizeof(expression->name),
                                     parameter, concrete)) return 0;
            if(owner != template_owner &&
               expression->kind == ZIR_EXPR_CALL &&
               strchr(expression->name, '.') == NULL) {
                const ZirModule *callee_owner = NULL;
                const ZirFunction *callee = NULL;
                if(ResolveFunctionAt(template_owner, expression->name,
                                     original.span.path,
                                     &callee_owner, &callee) == 1 &&
                   callee_owner == template_owner && callee != NULL) {
                    for(int import_index = 0;
                        import_index < owner->import_count; import_index++) {
                        const ZirImport *import = &owner->imports[import_index];
                        if(import->kind != ZIR_IMPORT_MODULE ||
                           import->resolved_module != template_owner ||
                           !in_lookup_file(owner, import->is_file_private,
                                           import->span)) continue;
                        char qualified[ZIR_NAME_MAX];
                        int written = snprintf(qualified, sizeof(qualified),
                            "%s.%s", import->name, expression->name);
                        if(written < 0 ||
                           (size_t)written >= sizeof(qualified)) return 0;
                        copy_text(expression->name,
                                  sizeof(expression->name), qualified);
                        break;
                    }
                }
            }
        }
    }
    checker->specialization_count = 0;
    return 1;
}

int
CheckPrograms(ZirProgram **programs, int count)
{
    Checker c = {0};
    c.programs = programs; c.program_count = count;
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
                if(type->is_procedure_type || type->is_record_template ||
                   type->is_extern) continue;
                for(int target = ZIR_C; target <= ZIR_GO; target++) {
                    char mapped[ZIR_NAME_MAX * 2];
                    TargetBindingName(NULL, (ZirTarget)target, type->name,
                                      mapped, sizeof(mapped));
                    if(strcmp(mapped, type->name))
                        type->native_name_mangled = 1;
                }
            }
        }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int g = 0; g < module->global_count; g++) {
                ZirGlobal *global = &module->globals[g];
                for(int q = p; q < count; q++)
                    for(int n = q == p ? m : 0;
                        n < programs[q]->module_count; n++) {
                        ZirModule *other_module = &programs[q]->modules[n];
                        for(int h = q == p && n == m ? g + 1 : 0;
                            h < other_module->global_count; h++) {
                            ZirGlobal *other = &other_module->globals[h];
                            if(!NativeGlobalNameConflict(module, global,
                                                         other_module, other))
                                continue;
                            global->native_name_collision = 1;
                            other->native_name_collision = 1;
                        }
                    }
            }
        }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int d = 0; d < module->define_count; d++) {
                ZirDefine *define = &module->defines[d];
                for(int q = p; q < count; q++)
                    for(int n = q == p ? m : 0;
                        n < programs[q]->module_count; n++) {
                        ZirModule *other_module = &programs[q]->modules[n];
                        for(int h = q == p && n == m ? d + 1 : 0;
                            h < other_module->define_count; h++) {
                            ZirDefine *other = &other_module->defines[h];
                            if(!NativeDefineNameConflict(module, define,
                                                         other_module, other))
                                continue;
                            define->native_name_collision = 1;
                            other->native_name_collision = 1;
                        }
                    }
            }
        }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int g = 0; g < module->global_count; g++) {
                ZirGlobal *global = &module->globals[g];
                for(int q = 0; q < count; q++)
                    for(int n = 0; n < programs[q]->module_count; n++) {
                        ZirModule *other_module = &programs[q]->modules[n];
                        for(int d = 0; d < other_module->define_count; d++) {
                            ZirDefine *define = &other_module->defines[d];
                            if(!NativeValueNameConflict(module, global,
                                                        other_module, define))
                                continue;
                            global->native_name_collision = 1;
                            define->native_name_collision = 1;
                        }
                    }
            }
        }
    {
        const ZirProgram *native_programs[count > 0 ? count : 1];
        for(int p = 0; p < count; p++)
            native_programs[p] = programs[p];
        for(int p = 0; p < count; p++)
            for(int m = 0; m < programs[p]->module_count; m++) {
                ZirModule *function_module = &programs[p]->modules[m];
                for(int f = 0; f < function_module->function_count; f++) {
                    ZirFunction *function = &function_module->functions[f];
                    if(function->is_extern || function->is_template) continue;
                    for(int q = 0; q < count; q++)
                        for(int n = 0; n < programs[q]->module_count; n++) {
                            ZirModule *module = &programs[q]->modules[n];
                            for(int g = 0; g < module->global_count; g++) {
                                ZirGlobal *global = &module->globals[g];
                                if(NativeFunctionValueNameConflict(
                                        native_programs, count, function_module,
                                        function, module, global->name, 1))
                                    global->native_name_collision = 1;
                            }
                            for(int d = 0; d < module->define_count; d++) {
                                ZirDefine *define = &module->defines[d];
                                if(NativeFunctionValueNameConflict(
                                        native_programs, count, function_module,
                                        function, module, define->name, 0))
                                    define->native_name_collision = 1;
                            }
                        }
                }
            }
    }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int t = 0; t < module->type_count; t++) {
                ZirType *type = &module->types[t];
                if(type->is_procedure_type || type->is_record_template ||
                   type->is_extern) continue;
                for(int q = p; q < count; q++)
                    for(int n = q == p ? m : 0;
                        n < programs[q]->module_count; n++) {
                        ZirModule *other_module = &programs[q]->modules[n];
                        for(int u = q == p && n == m ? t + 1 : 0;
                            u < other_module->type_count; u++) {
                            ZirType *other = &other_module->types[u];
                            if(other->is_procedure_type ||
                               other->is_record_template || other->is_extern ||
                               strcmp(type->name, other->name)) continue;
                            type->native_name_mangled = 1;
                            other->native_name_mangled = 1;
                        }
                    }
            }
        }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            char saved_path[ZIR_PATH_MAX];
            copy_text(saved_path, sizeof(saved_path), module->lookup_path);
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
                    Checker scope = {0};
                    scope.module = module;
                    activate_using_filtered(&scope, using->path,
                                            using->filter, using->span);
                    free(scope.bindings);
                    if(scope.errors || scope.failed)
                        return 0;
                }
            }
            copy_text(module->lookup_path, sizeof(module->lookup_path),
                      saved_path);
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
                              sizeof(module->lookup_path), saved_path);
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
                LexerInit(&lexer, definition->value, definition->span.path);
                for(;;) {
                    ZirToken token = LexerNext(&lexer);
                    if(token.kind == ZIR_TOKEN_EOF) break;
                    if(token.kind == ZIR_TOKEN_DIRECTIVE &&
                       strcmp(token.text, "#compile_time") == 0) {
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
                        char literal[ZIR_TEXT_MAX];
                        if(!EvaluateCompileLiteral(module, expression,
                                                   definition->span, 1, literal,
                                                   sizeof(literal), NULL))
                            continue;
                        copy_text(definition->value,
                                  sizeof(definition->value), literal);
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
                    char literal[ZIR_TEXT_MAX];
                    if(!EvaluateCompileLiteral(module,
                            assertion->condition, assertion->span, 0,
                            literal, sizeof(literal), NULL) ||
                       (strcmp(literal, "0") && strcmp(literal, "1"))) {
                        Diagnostic(assertion->span, "check.assert",
                                   "#assert requires a compile-time constant condition");
                        return 0;
                    }
                    value = literal[0] == '1';
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
                ZirFunction raw = {0};
                int raw_root = ParseExpr(&raw, module, global->init,
                                         global->span);
                int literal_initializer = raw_root >= 0 &&
                    raw.exprs[raw_root].kind == ZIR_EXPR_COMPOUND;
                char constant_name[ZIR_NAME_MAX] = "";
                if(raw_root >= 0 &&
                   raw.exprs[raw_root].kind == ZIR_EXPR_IDENT)
                    copy_text(constant_name, sizeof(constant_name),
                              raw.exprs[raw_root].name);
                else if(raw_root >= 0 &&
                        raw.exprs[raw_root].kind == ZIR_EXPR_MEMBER &&
                        raw.exprs[raw_root].left >= 0 &&
                        raw.exprs[raw.exprs[raw_root].left].kind ==
                            ZIR_EXPR_IDENT) {
                    const ZirExpr *base =
                        &raw.exprs[raw.exprs[raw_root].left];
                    int written = snprintf(constant_name,
                        sizeof(constant_name), "%s.%s", base->name,
                        raw.exprs[raw_root].name);
                    if(written < 0 ||
                       (size_t)written >= sizeof(constant_name))
                        constant_name[0] = '\0';
                }
                if(constant_name[0]) {
                    CompoundConstant compound = {0};
                    if(bound_compound_constant(module, constant_name, 0,
                            &compound) == 1) {
                        char lowered[ZIR_TEXT_MAX];
                        if(!lower_compound_global(module, global, &compound,
                                                  lowered,
                                                  sizeof(lowered))) {
                            Diagnostic(global->span, "check.constant_type",
                                       "aggregate constant type is unavailable or mismatched: %s",
                                       constant_name);
                            free(raw.exprs);
                            return 0;
                        }
                        copy_text(global->init, sizeof(global->init),
                                  lowered);
                        literal_initializer = 1;
                    }
                }
                free(raw.exprs);
                char folded[ZIR_TEXT_MAX];
                const ZirModule *folded_owner = NULL;
                if(!literal_initializer && strcmp(global->type, "bool") &&
                   (EvaluateCompileLiteral(module, global->init,
                        global->span, 0, folded, sizeof(folded),
                        &folded_owner) ||
                    evaluate_global_startup_literal(module, g, global->init,
                        global->span, folded, sizeof(folded),
                        &folded_owner))) {
                    if(folded_owner != NULL) {
                        CompoundConstant compound = {0};
                        copy_text(compound.literal,
                                  sizeof(compound.literal), folded);
                        compound.owner = folded_owner;
                        compound_qualifier_for_global(&compound,
                                                      global->type);
                        if(!lower_compound_global(module, global, &compound,
                                                  folded, sizeof(folded))) {
                            Diagnostic(global->span, "check.constant_type",
                                       "aggregate initializer type is unavailable or mismatched: %s",
                                       global->init);
                            return 0;
                        }
                    }
                    copy_text(global->init, sizeof(global->init), folded);
                }
                if(!check_file_private_expression(module, global->init,
                                                  global->span))
                    return 0;
                ZirFunction expression = {0};
                int root = ParseExprTyped(&expression, module,
                                          global->init, global->span,
                                          global->type);
                int valid = root >= 0 &&
                    expression.exprs[root].kind != ZIR_EXPR_UNKNOWN;
                if(valid && !check_file_scope_enum_names(module, &expression,
                                                         global->span)) {
                    free(expression.exprs);
                    return 0;
                }
                if(valid) {
                    Checker initializer = {0};
                    initializer.module = module;
                    initializer.fn = &expression;
                    initializer.programs = programs;
                    initializer.program_count = count;
                    copy_text(initializer.expected_type,
                              sizeof(initializer.expected_type),
                              global->type);
                    if(!reserve_compound_constants(module, &expression)) {
                        Diagnostic(global->span, "check.global",
                                   "global initializer expression is too large");
                        free(expression.exprs);
                        return 0;
                    }
                    const char *actual = expression_type(&initializer, root);
                    if(initializer.errors == 0) {
                        if(!actual[0])
                            error(&initializer, global->span,
                                  "cannot infer initializer type", global->name);
                        else if(!compatible_checked(&initializer,
                                                    global->type, actual))
                            error(&initializer, global->span,
                                  "initializer type mismatch", global->name);
                    }
                    int typed_valid = initializer.errors == 0 &&
                                      !initializer.failed;
                    for(int i = 0; i < initializer.restore_count; i++)
                        free(initializer.restores[i].states);
                    free(initializer.bindings);
                    free(initializer.specializations);
                    if(!typed_valid) {
                        if(initializer.errors == 0)
                            Diagnostic(global->span, "check.global",
                                       "cannot check file-scope initializer: %s",
                                       global->name);
                        free(expression.exprs);
                        return 0;
                    }
                }
                int runtime_initializer = 0;
                if(valid)
                    for(int e = 0; e < expression.expr_count; e++) {
                        const ZirExpr *node = &expression.exprs[e];
                        const ZirModule *owner = NULL;
                        const ZirGlobal *referenced = NULL;
                        if(node->kind == ZIR_EXPR_CALL ||
                           (node->kind == ZIR_EXPR_IDENT &&
                            ResolveGlobalAt(module, node->name,
                                            global->span.path, &owner,
                                            &referenced) == 1)) {
                            runtime_initializer = 1;
                            break;
                        }
                    }
                free(expression.exprs);
                if(!valid) {
                    Diagnostic(global->span, "check.global",
                               "invalid file-scope initializer: %s",
                               global->init);
                    return 0;
                }
                if(runtime_initializer) {
                    char name[ZIR_NAME_MAX], assignment[ZIR_TEXT_MAX];
                    int serial = 0, collision;
                    do {
                        snprintf(name, sizeof(name), "__global_init_%d_%d",
                                 g, serial++);
                        collision = 0;
                        for(int f = 0; f < module->function_count; f++)
                            collision |= strcmp(module->functions[f].name,
                                                name) == 0;
                    } while(collision);
                    int written = snprintf(assignment, sizeof(assignment),
                                           "%s = %s", global->name,
                                           global->init);
                    if(written < 0 || (size_t)written >= sizeof(assignment)) {
                        Diagnostic(global->span, "check.global",
                                   "global initializer expression is too large");
                        return 0;
                    }
                    ZirFunction *startup = ModuleAddFunction(module, name,
                        "", "void", 0, global->span);
                    if(startup == NULL ||
                       FunctionAddStmt(startup, ZIR_STMT_ASSIGN, assignment,
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
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            if(!normalize_type_applications(&programs[p]->modules[m]))
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
                    const ZirType *generic = FindType(module,
                        type->template_name, NULL);
                    if(generic == NULL || !generic->is_record_template ||
                       !InstantiateGenericRecord(type, generic)) {
                        Diagnostic(type->span, "check.specialize",
                                   "invalid generic type specialization: %s",
                                   type->name);
                        return 0;
                    }
                    type = &module->types[t];
                    if(type->body[0]) {
                        char body[sizeof(type->body)];
                        char expanded[sizeof(type->body)];
                        copy_text(body, sizeof(body), type->body);
                        if(!rewrite_type_applications(module, body, expanded,
                                sizeof(expanded), type->span, 0))
                            return 0;
                        copy_text(module->types[t].body,
                                  sizeof(module->types[t].body), expanded);
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
                ZirFunction expression = {0};
                int root;
                int valid;
                /* Array type aliases name a storage shape, not a value. */
                if(value[0] == '[')
                    continue;
                root = ParseExprNoDefaults(&expression, module,
                                           definition->value,
                                           definition->span);
                valid = root >= 0 &&
                    expression.exprs[root].kind != ZIR_EXPR_UNKNOWN;
                free(expression.exprs);
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
                char saved_path[ZIR_PATH_MAX];
                copy_text(saved_path, sizeof(saved_path),
                          module->lookup_path);
                select_lookup_file(module, module->functions[f].span);
                int normalized = normalize_function_arrays(
                    module, &module->functions[f]);
                copy_text(module->lookup_path, sizeof(module->lookup_path),
                          saved_path);
                if(!normalized)
                    return 0;
            }
        }
    }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            if(!check_type_declarations(&programs[p]->modules[m]) ||
               !normalize_record_arrays(&programs[p]->modules[m]))
                return 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            char saved_path[ZIR_PATH_MAX];
            copy_text(saved_path, sizeof(saved_path), module->lookup_path);
            for(int g = 0; g < module->global_count; g++) {
                ZirGlobal *global = &module->globals[g];
                select_lookup_file(module, global->span);
                normalize_array(module, global->type, sizeof(global->type));
            }
            copy_text(module->lookup_path, sizeof(module->lookup_path),
                      saved_path);
        }
    for(int p = 0; p < count; p++) for(int m = 0; m < programs[p]->module_count; m++) {
        c.module = &programs[p]->modules[m];
        for(int i = 0; i < c.module->global_count; i++) {
            select_lookup_file(c.module, c.module->globals[i].span);
            const char *type = c.module->globals[i].type;
            ValidatedRecords checked = {0};
            const char *error = storage_type_error(c.module, type, NULL, 0,
                                                   &checked, NULL, 0);
            free(checked.items);
            if(error != NULL) {
                ZirSourceSpan span = c.module->globals[i].span;
                Diagnostic(span, "check.storage", "%s: %s", error, type);
                free(c.bindings);
                return 0;
            }
        }
        for(int f = 0; f < c.module->function_count; f++) {
            if(c.module->functions[f].is_template) {
                if(!check_template_declaration(&c,
                        &c.module->functions[f])) {
                    free(c.bindings);
                    free(c.specializations);
                    return 0;
                }
                continue;
            }
            if(!check_function(&c, &c.module->functions[f])) {
                free(c.bindings);
                free(c.specializations);
                return 0;
            }

        }
        c.module->lookup_path[0] = '\0';
    }
    for(;;) {
        int pending = c.specialization_count;
        if(pending == 0) break;
        if(!instantiate_specializations(&c)) {
            free(c.bindings);
            free(c.specializations);
            return 0;
        }
        for(int p = 0; p < count; p++)
            for(int m = 0; m < programs[p]->module_count; m++) {
                c.module = &programs[p]->modules[m];
                for(int f = 0; f < c.module->function_count; f++) {
                    ZirFunction *instance = &c.module->functions[f];
                    if(!instance->is_specialization || instance->checked)
                        continue;
                    if(!check_function(&c, instance)) {
                        free(c.bindings);
                        free(c.specializations);
                        return 0;
                    }
                }
                c.module->lookup_path[0] = '\0';
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
    free(c.bindings);
    free(c.specializations);
    if(c.failed || c.errors != 0 || !CheckSliceLifetimes(programs, count))
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
