#include "zir_vm_internal.h"

/* Set when the last parse_parameters or verify_sequence failed on one of
 * the VM's fixed limits, so the diagnostic can name the limit. */
static _Thread_local int parameters_exceeded;
/* How many bindings the function being verified has room for. */
static _Thread_local int binding_capacity;

/* The most locals FUNCTION can hold at once with PARAMETERS parameters:
 * every declaration may be live together. */
int
function_local_bound(const ZirFunction *function, int parameters)
{
    int bound = parameters;
    for(int s = 0; s < function->stmt_count; s++)
        if(function->stmts[s].kind == ZIR_STMT_DECL)
            bound++;
    return bound;
}

int
parse_parameters(const ZirModule *module, const ZirFunction *function,
                 Parameter *parameters)
{
    const char *cursor = function->args;
    int count = 0;
    parameters_exceeded = 0;
    while(*cursor != 0) {
        const char *start;
        size_t length;
        while(*cursor != '\0' && isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor == 0)
            break;
        if(count >= VM_MAX_PARAMS) {
            parameters_exceeded = 1;
            return -1;
        }
        start = cursor;
        while((*cursor >= 'a' && *cursor <= 'z') ||
              (*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= '0' && *cursor <= '9') || *cursor == '_')
            cursor++;
        length = (size_t)(cursor - start);
        if(length == 0 || length >= ZIR_NAME_MAX ||
           (start[0] >= '0' && start[0] <= '9'))
            return -1;
        memcpy(parameters[count].name, start, length);
        parameters[count].name[length] = 0;
        while(*cursor != '\0' && isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor++ != ':')
            return -1;
        while(*cursor != '\0' && isspace((unsigned char)*cursor))
            cursor++;
        start = cursor;
        if(*cursor == '[') {
            cursor++;
            while(isalnum((unsigned char)*cursor) || *cursor == '_')
                cursor++;
            if(*cursor++ != ']')
                return -1;
        }
        /* Pointer parameters name an opaque host handle in bundles. */
        while(*cursor == '*')
            cursor++;
        while((*cursor >= 'a' && *cursor <= 'z') ||
              (*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= '0' && *cursor <= '9') ||
              *cursor == '_' || *cursor == '.')
            cursor++;
        length = (size_t)(cursor - start);
        if(length == 0 || length >= ZIR_NAME_MAX)
            return -1;
        memcpy(parameters[count].type, start, length);
        parameters[count].type[length] = 0;
        if(!portable_type(module, parameters[count].type) ||
           strcmp(parameters[count].type, "void") == 0)
            return -1;
        count++;
        while(*cursor != '\0' && isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor == 0)
            break;
        if(*cursor++ != ',')
            return -1;
        if(*cursor == 0)
            return -1;
    }
    return count;
}
/* Buffers parse_import_parameters keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ParseImportParametersBuffers {
    ZirFunction signature;
} ParseImportParametersBuffers;

static int parse_import_parameters(const ZirModule *module, const ZirImport *import,
                        Parameter *parameters);

static int
parse_import_parameters_with_buffers(const ZirModule *module, const ZirImport *import,
                        Parameter *parameters, ParseImportParametersBuffers *buffers)
{
    memset(&buffers->signature, 0, sizeof(buffers->signature));
    copy_text(buffers->signature.args, sizeof(buffers->signature.args), import->args);
    return parse_parameters(module, &buffers->signature, parameters);
}

static int
parse_import_parameters(const ZirModule *module, const ZirImport *import,
                        Parameter *parameters)
{
    static _Thread_local ParseImportParametersBuffers *spares[16];
    static _Thread_local int spare_count;
    ParseImportParametersBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = parse_import_parameters_with_buffers(module, import, parameters, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static int
binding_index(const Parameter *bindings, int count, const char *name)
{
    for(int i = count - 1; i >= 0; i--)
        if(strcmp(bindings[i].name, name) == 0)
            return i;
    return -1;
}

static const ZirGlobal *
find_global_declaration(const ZirModule *module, const char *name,
                        const char *source_path)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    return ResolveGlobalAt(module, name, source_path, &owner, &global) == 1 ?
        global : NULL;
}

static const char *
assignment_root(const ZirFunction *function, int index)
{
    for(int depth = 0; depth < VM_MAX_DEPTH; depth++) {
        if(index < 0 || index >= function->expr_count)
            return NULL;
        const ZirExpr *expression = &function->exprs[index];
        if(expression->kind == ZIR_EXPR_IDENT)
            return expression->name;
        /* A write through a pointer is rooted at the pointer binding. */
        if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "*")) {
            index = expression->right;
            continue;
        }
        if(expression->kind != ZIR_EXPR_MEMBER &&
           expression->kind != ZIR_EXPR_POINTER_MEMBER &&
           expression->kind != ZIR_EXPR_INDEX)
            return NULL;
        index = expression->left;
    }
    return NULL;
}

/* Whether expression INDEX is a host call's result, directly or through a
 * binding some statement sets from one. Host pointers are opaque handles:
 * the portable runner cannot read through them. */
static int
host_pointer(const ZirModule *module, const ZirFunction *function, int index)
{
    if(index < 0 || index >= function->expr_count)
        return 0;
    const ZirExpr *expression = &function->exprs[index];
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    if(expression->kind == ZIR_EXPR_CALL)
        return host_import(module, expression->name) != NULL ||
               (ResolveFunction(module, expression->name, &owner, &callee) == 1 &&
                callee != NULL && callee->is_extern);
    if(expression->kind != ZIR_EXPR_IDENT)
        return 0;
    for(int s = 0; s < function->stmt_count; s++) {
        const ZirStmt *statement = &function->stmts[s];
        const char *bound = statement->kind == ZIR_STMT_DECL ? statement->name :
            statement->kind == ZIR_STMT_ASSIGN ?
            assignment_root(function, statement->lhs_root) : NULL;
        if(bound != NULL && !strcmp(bound, expression->name) &&
           statement->expr_root >= 0 &&
           function->exprs[statement->expr_root].kind == ZIR_EXPR_CALL &&
           host_pointer(module, function, statement->expr_root))
            return 1;
    }
    return 0;
}

static int
binary_operator(const char *op)
{
    static const char *const supported[] = {
        "+", "-", "*", "/", "%", "==", "!=", "<", "<=", ">", ">=",
        "&&", "||", "&", "|", "^", "<<", ">>", NULL
    };
    for(int i = 0; supported[i] != NULL; i++)
        if(strcmp(op, supported[i]) == 0)
            return 1;
    return 0;
}

static int
integer_type(const char *type)
{
    return value_kind(type) == VALUE_INT &&
           strcmp(type, "bool") != 0;
}

int
bitwise_operator(const char *op)
{
    return strcmp(op, "&") == 0 || strcmp(op, "|") == 0 ||
           strcmp(op, "^") == 0 || strcmp(op, "<<") == 0 ||
           strcmp(op, ">>") == 0;
}

const char *
assignment_binary_operator(const char *op)
{
    if(strcmp(op, "+=") == 0)
        return "+";
    if(strcmp(op, "-=") == 0)
        return "-";
    if(strcmp(op, "*=") == 0)
        return "*";
    if(strcmp(op, "/=") == 0)
        return "/";
    if(strcmp(op, "%=") == 0)
        return "%";
    if(strcmp(op, "&=") == 0)
        return "&";
    if(strcmp(op, "|=") == 0)
        return "|";
    if(strcmp(op, "^=") == 0)
        return "^";
    if(strcmp(op, "<<=") == 0)
        return "<<";
    if(strcmp(op, ">>=") == 0)
        return ">>";
    return NULL;
}

static int
same_verified_type(const ZirModule *declaration_module,
                   const char *declared, const ZirModule *use_module,
                   const char *checked)
{
    if(strcmp(declared, checked) == 0)
        return 1;
    const char *scalar = ScalarType(declared);
    if(*scalar && strcmp(scalar, checked) == 0)
        return 1;
    char declared_element[ZIR_NAME_MAX], checked_element[ZIR_NAME_MAX];
    int declared_count, checked_count;
    if(ArrayElementType(declared, declared_element,
                        sizeof(declared_element), &declared_count) &&
       ArrayElementType(checked, checked_element,
                        sizeof(checked_element), &checked_count))
        return declared_count == checked_count &&
               same_verified_type(declaration_module, declared_element,
                                  use_module, checked_element);
    const ZirModule *source_owner = NULL, *resolved_owner = NULL;
    const ZirType *source = FindType(declaration_module, declared, &source_owner);
    const ZirType *resolved = FindType(use_module, checked, &resolved_owner);
    return source != NULL &&
           (source == resolved ||
            same_type_application(source_owner, source, resolved_owner, resolved));
}
/* Buffers verify_expression keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct VerifyExpressionBuffers {
    Parameter parameters[VM_MAX_PARAMS];
    unsigned char bytes[ZIR_TEXT_MAX];
    ZirFunction signature;
    Parameter actual[VM_MAX_PARAMS];
    Parameter expected[VM_MAX_PARAMS];
    unsigned char seen[VM_MAX_FIELDS];
} VerifyExpressionBuffers;

static int verify_expression(const ZirModule *module, const ZirFunction *function,
                  const Parameter *bindings, int binding_count,
                  int index, int depth);

static int
verify_expression_with_buffers(const ZirModule *module, const ZirFunction *function,
                  const Parameter *bindings, int binding_count,
                  int index, int depth, VerifyExpressionBuffers *buffers)
{
    const ZirExpr *expression;
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    int children = 0;
    if(index < 0 || index >= function->expr_count || depth >= VM_MAX_DEPTH)
        return 0;
    expression = &function->exprs[index];
    if(!portable_type(module, expression->type))
        return 0;
    switch(expression->kind) {
    case ZIR_EXPR_SIZE_OF: {
        size_t size, alignment;
        if(expression->left != -1 || expression->right != -1 ||
           expression->third != -1 || expression->first_child != -1)
            return 0;
        if(!TypeLayout(module, expression->name, &size, &alignment) ||
           size > INT64_MAX) {
            Diagnostic(expression->span, "zib.size_of",
                       "cannot evaluate portable size_of(%s)", expression->name);
            return 0;
        }
        return 1;
    }
    case ZIR_EXPR_INT: {
        char *end;
        errno = 0;
        if(expression->text[0] == '-') {
            (void)strtoll(expression->text, &end, 0);
            return errno == 0 && end != expression->text && *end == 0;
        }
        (void)strtoull(expression->text, &end, 0);
        return errno == 0 && end != expression->text && *end == 0;
    }
    case ZIR_EXPR_FLOAT: {
        char *end;
        double number;
        errno = 0;
        number = strtod(expression->text, &end);
        return errno == 0 && end != expression->text && *end == 0 &&
               isfinite(number);
    }
    case ZIR_EXPR_STRING: {
        size_t length;
        return strcmp(expression->type, "string") == 0 &&
               DecodeStringLiteral(expression->text, buffers->bytes, sizeof(buffers->bytes), &length);
    }
    case ZIR_EXPR_COMPILE_TIME:
        return strcmp(expression->type, "bool") == 0 &&
               strcmp(expression->text, "#compile_time") == 0 &&
               expression->left == -1 && expression->right == -1 &&
               expression->third == -1 && expression->first_child == -1;
    case ZIR_EXPR_IDENT: {
        if(expression->is_function_value) {
            const ZirType *slot = FindType(module, expression->type, NULL);
            memset(&buffers->signature, 0, sizeof(buffers->signature));
            int actual_count, expected_count;
            if(slot == NULL || !slot->is_procedure_type ||
               ResolveFunction(module, expression->name,
                               &owner, &callee) != 1 ||
               callee == NULL || callee->is_extern ||
               strlen(slot->body) >= sizeof(buffers->signature.args) ||
               strcmp(callee->return_type, slot->procedure_return_type) != 0)
                return 0;
            copy_text(buffers->signature.args, sizeof(buffers->signature.args), slot->body);
            actual_count = parse_parameters(owner, callee, buffers->actual);
            expected_count = parse_parameters(module, &buffers->signature, buffers->expected);
            if(actual_count < 0 || actual_count != expected_count)
                return 0;
            for(int i = 0; i < actual_count; i++) {
                const ZirType *actual_type = FindType(owner, buffers->actual[i].type, NULL);
                const ZirType *expected_type = FindType(module, buffers->expected[i].type, NULL);
                if(actual_type != NULL || expected_type != NULL) {
                    if(actual_type != expected_type)
                        return 0;
                } else if(strcmp(buffers->actual[i].type, buffers->expected[i].type) != 0)
                    return 0;
            }
            return 1;
        }
        if(strcmp(expression->name, "true") == 0 ||
           strcmp(expression->name, "false") == 0 ||
           strcmp(expression->name, "null") == 0 ||
           binding_index(bindings, binding_count, expression->name) >= 0 ||
           find_global_declaration(module, expression->name,
                                   SpanPath(expression->span)) != NULL)
            return 1;
        return 0;
    }
    case ZIR_EXPR_UNARY:
        if(strcmp(expression->op, "&") == 0) {
            /* *place points at storage the portable runner owns. */
            const ZirExpr *place = expression->right >= 0 &&
                expression->right < function->expr_count ?
                &function->exprs[expression->right] : NULL;
            return place != NULL &&
                   (place->kind == ZIR_EXPR_IDENT || place->kind == ZIR_EXPR_MEMBER ||
                    place->kind == ZIR_EXPR_POINTER_MEMBER ||
                    place->kind == ZIR_EXPR_INDEX ||
                    (place->kind == ZIR_EXPR_UNARY && !strcmp(place->op, "*"))) &&
                   verify_expression(module, function, bindings, binding_count,
                                     expression->right, depth + 1);
        }
        if(strcmp(expression->op, "*") == 0)
            return expression->right >= 0 &&
                   expression->right < function->expr_count &&
                   function->exprs[expression->right].type[0] == '*' &&
                   !host_pointer(module, function, expression->right) &&
                   verify_expression(module, function, bindings, binding_count,
                                     expression->right, depth + 1);
        return (strcmp(expression->op, "+") == 0 ||
                strcmp(expression->op, "-") == 0 ||
                strcmp(expression->op, "!") == 0 ||
                strcmp(expression->op, "~") == 0) &&
               verify_expression(module, function, bindings, binding_count,
                                 expression->right, depth + 1) &&
               (strcmp(expression->op, "~") == 0 ?
                integer_type(function->exprs[expression->right].type) :
                strcmp(expression->op, "!") == 0 ?
                strcmp(function->exprs[expression->right].type, "bool") == 0 :
                value_kind(function->exprs[expression->right].type) == VALUE_INT ||
                value_kind(function->exprs[expression->right].type) == VALUE_REAL);
    case ZIR_EXPR_BINARY: {
        if(!binary_operator(expression->op) ||
           !verify_expression(module, function, bindings, binding_count,
                              expression->left, depth + 1) ||
           !verify_expression(module, function, bindings, binding_count,
                              expression->right, depth + 1))
            return 0;
        const char *left_type = function->exprs[expression->left].type;
        const char *right_type = function->exprs[expression->right].type;
        if(strcmp(left_type, "string") == 0 ||
           strcmp(right_type, "string") == 0)
            return strcmp(left_type, "string") == 0 &&
                   strcmp(right_type, "string") == 0 &&
                   (strcmp(expression->op, "==") == 0 ||
                    strcmp(expression->op, "!=") == 0);
        const ZirType *left_enum = FindType(module, left_type, NULL);
        const ZirType *right_enum = FindType(module, right_type, NULL);
        if((left_enum != NULL && left_enum->is_enum_flags) ||
           (right_enum != NULL && right_enum->is_enum_flags)) {
            const ZirType *flags = left_enum != NULL && left_enum->is_enum_flags ?
                left_enum : right_enum;
            if((left_enum != NULL && left_enum->is_enum_flags &&
                right_enum != NULL && right_enum->is_enum_flags &&
                left_enum != right_enum) ||
               (left_enum != flags && !integer_type(left_type)) ||
               (right_enum != flags && !integer_type(right_type)))
                return 0;
            return strcmp(expression->op, "==") == 0 ||
                   strcmp(expression->op, "!=") == 0 ||
                   strcmp(expression->op, "&") == 0 ||
                   strcmp(expression->op, "|") == 0 ||
                   strcmp(expression->op, "^") == 0 ||
                   strcmp(expression->op, "+") == 0 ||
                   strcmp(expression->op, "-") == 0;
        }
        if(left_enum != NULL && left_enum->is_enum)
            return left_enum == right_enum &&
                   (strcmp(expression->op, "==") == 0 ||
                    strcmp(expression->op, "!=") == 0);
        if(bitwise_operator(expression->op))
            return integer_type(left_type) && integer_type(right_type);
        return scalar_type(left_type) && scalar_type(right_type);
    }
    case ZIR_EXPR_CAST: {
        /* A pointer cast would reinterpret storage; the portable runner only
         * follows pointers at the type they were taken as. */
        if(expression->name[0] == '*' ||
           (expression->right >= 0 && expression->right < function->expr_count &&
            function->exprs[expression->right].type[0] == '*'))
            return 0;
        const ZirType *destination = FindType(module, expression->name, NULL);
        if(!scalar_type(expression->name) &&
           (destination == NULL || !destination->is_enum))
            return 0;
        if(!verify_expression(module, function, bindings, binding_count,
                              expression->right, depth + 1))
            return 0;
        const char *source_type = function->exprs[expression->right].type;
        const ZirType *source = FindType(module, source_type, NULL);
        if(strcmp(expression->name, "string") == 0 ||
           strcmp(source_type, "string") == 0)
            return strcmp(expression->name, "string") == 0 &&
                   strcmp(source_type, "string") == 0;
        return scalar_type(source_type) ||
               (source != NULL && source->is_enum);
    }
    case ZIR_EXPR_COMPOUND: {
        char element[ZIR_NAME_MAX];
        int capacity;
        if(ArrayElementType(expression->name, element,
                            sizeof(element), &capacity)) {
            int position = 0;
            if(capacity < 0 ||
               strcmp(expression->type, expression->name) != 0)
                return 0;
            for(int child = expression->first_child; child >= 0;
                child = function->exprs[child].next_sibling) {
                if(child >= function->expr_count || ++position > capacity)
                    return 0;
                const ZirExpr *item = &function->exprs[child];
                if(item->kind != ZIR_EXPR_FIELD_INIT ||
                   strcmp(item->op, "=") == 0 ||
                   !verify_expression(module, function, bindings,
                                      binding_count, item->right, depth + 1))
                    return 0;
            }
            return 1;
        }
        const ZirModule *record_owner = NULL;
        const ZirType *record = FindType(module, expression->name,
                                         &record_owner);
        memset(buffers->seen, 0, sizeof(buffers->seen));
        int children = 0;
        if(record == NULL || record->is_enum || record->is_procedure_type ||
           strcmp(expression->type, expression->name) != 0)
            return 0;
        for(int child = expression->first_child; child >= 0;
            child = function->exprs[child].next_sibling) {
            if(child >= function->expr_count || ++children > VM_MAX_FIELDS)
                return 0;
            const ZirExpr *initializer = &function->exprs[child];
            if(initializer->kind != ZIR_EXPR_FIELD_INIT ||
               !verify_expression(module, function, bindings, binding_count,
                                  initializer->right, depth + 1))
                return 0;
            size_t offset = 0;
            ZirTypeField field;
            int field_index = 0;
            int found = 0;
            while(TypeNextField(record, &offset, &field) == 1) {
                if(strcmp(field.name, initializer->name) == 0) {
                    if(buffers->seen[field_index] ||
                       !same_verified_type(record_owner, field.type,
                                           module, initializer->type))
                        return 0;
                    buffers->seen[field_index] = 1;
                    found = 1;
                    break;
                }
                field_index++;
            }
            if(!found)
                return 0;
        }
        return 1;
    }
    case ZIR_EXPR_POINTER_MEMBER:
        return expression->left >= 0 && expression->left < function->expr_count &&
               function->exprs[expression->left].type[0] == '*' &&
               !host_pointer(module, function, expression->left) &&
               verify_expression(module, function, bindings, binding_count,
                                 expression->left, depth + 1);
    case ZIR_EXPR_MEMBER: {
        if(expression->left < 0 || expression->left >= function->expr_count)
            return 0;
        const ZirExpr *base = &function->exprs[expression->left];
        char element[ZIR_NAME_MAX];
        int capacity;
        if(ArrayElementType(base->type, element, sizeof(element), &capacity) &&
           capacity == 0 && !strcmp(expression->name, "data")) {
            char pointer[ZIR_NAME_MAX];
            int written = snprintf(pointer, sizeof(pointer), "*%s", element);
            return written > 0 && (size_t)written < sizeof(pointer) &&
                   !strcmp(expression->type, pointer) &&
                   verify_expression(module, function, bindings, binding_count,
                                     expression->left, depth + 1);
        }
        if(strcmp(base->type, "string") == 0 ||
           SliceElementType(base->type, NULL, 0) ||
           ArrayElementType(base->type, NULL, 0, NULL)) {
            return strcmp(expression->name, "count") == 0 &&
                   strcmp(expression->type, "s64") == 0 &&
                   verify_expression(module, function, bindings, binding_count,
                                     expression->left, depth + 1);
        }
        const ZirModule *owner = NULL;
        const ZirType *record = FindType(module, base->type, &owner);
        char field_type[ZIR_NAME_MAX];
        if(record == NULL || record->is_enum || record->is_procedure_type ||
           !verify_expression(module, function, bindings, binding_count,
                              expression->left, depth + 1))
            return 0;
        if(!RecordFieldPathType(owner, record, expression->name,
                                field_type, sizeof(field_type)))
            return 0;
        return same_verified_type(owner, field_type, module,
                                  expression->type);
    }
    case ZIR_EXPR_SLICE: {
        char element[ZIR_NAME_MAX];
        char expected[ZIR_NAME_MAX];
        int capacity;
        if(expression->left < 0 || expression->left >= function->expr_count ||
           !verify_expression(module, function, bindings, binding_count,
                              expression->left, depth + 1))
            return 0;
        const char *base = function->exprs[expression->left].type;
        if(strcmp(base, "string") == 0) {
            if(strcmp(expression->type, "string") != 0) return 0;
        } else {
        if(!ArrayElementType(base, element, sizeof(element), &capacity) &&
           !SliceElementType(base, element, sizeof(element)))
            return 0;
        int written = snprintf(expected, sizeof(expected), "[]%s", element);
        if(written < 0 || (size_t)written >= sizeof(expected) ||
           strcmp(expression->type, expected) != 0)
            return 0;
        }
        if(expression->right >= 0 &&
           (!integer_type(function->exprs[expression->right].type) ||
            !verify_expression(module, function, bindings, binding_count,
                               expression->right, depth + 1)))
            return 0;
        if(expression->third >= 0 &&
           (!integer_type(function->exprs[expression->third].type) ||
            !verify_expression(module, function, bindings, binding_count,
                               expression->third, depth + 1)))
            return 0;
        return 1;
    }
    case ZIR_EXPR_INDEX: {
        if(expression->left < 0 || expression->left >= function->expr_count ||
           expression->right < 0 || expression->right >= function->expr_count ||
           !integer_type(function->exprs[expression->right].type) ||
           !verify_expression(module, function, bindings, binding_count,
                              expression->left, depth + 1) ||
           !verify_expression(module, function, bindings, binding_count,
                              expression->right, depth + 1))
            return 0;
        const char *base = function->exprs[expression->left].type;
        if(strcmp(base, "string") == 0)
            return strcmp(expression->type, "u8") == 0;
        char element[ZIR_NAME_MAX];
        int capacity;
        int array = ArrayElementType(base, element, sizeof(element), &capacity);
        int slice = !array && SliceElementType(base, element, sizeof(element));
        int vec = !array && !slice &&
                  VecElementType(module, base, element, sizeof(element));
        return (slice || vec || (array && capacity >= 0)) &&
               (strcmp(expression->type, element) == 0 ||
                (ScalarType(element)[0] != 0 &&
                 strcmp(expression->type, ScalarType(element)) == 0));
    }
    case ZIR_EXPR_CONDITIONAL:
        return verify_expression(module, function, bindings, binding_count,
                                 expression->left, depth + 1) &&
               strcmp(function->exprs[expression->left].type, "bool") == 0 &&
               verify_expression(module, function, bindings, binding_count,
                                 expression->right, depth + 1) &&
               verify_expression(module, function, bindings, binding_count,
                                 expression->third, depth + 1);
    case ZIR_EXPR_CALL:
        if(expression->name[0] == 0)
            return 0;
        if(!strcmp(expression->name, "TextView")) {
            int first = expression->first_child;
            return first >= 0 &&
                   function->exprs[first].next_sibling < 0 &&
                   strcmp(function->exprs[first].type, "[]u8") == 0 &&
                   strcmp(expression->type, "string") == 0 &&
                   verify_expression(module, function, bindings,
                                     binding_count, first, depth + 1);
        }
        if(!strcmp(expression->name, "zi_new")) {
            /* New(T) of a type the runner can hold. */
            const char *target = expression->type[0] == '*' ?
                skip_ws(expression->type + 1) : "";
            return expression->first_child < 0 && target[0] &&
                   (*ScalarType(target) || FindType(module, target, NULL) != NULL ||
                    ArrayElementType(target, NULL, 0, NULL));
        }
        if(!strcmp(expression->name, "zi_free")) {
            int first = expression->first_child;
            return first >= 0 && function->exprs[first].next_sibling < 0 &&
                   function->exprs[first].type[0] == '*' &&
                   verify_expression(module, function, bindings,
                                     binding_count, first, depth + 1);
        }
        if(!strcmp(expression->name, "print")) {
            PrintPiece *pieces = calloc(PRINT_PIECES_MAX, sizeof(*pieces));
            int first = expression->first_child, count, placeholders = 0;
            int arguments = 0, valid;
            valid = pieces != NULL && first >= 0 &&
                    function->exprs[first].kind == ZIR_EXPR_STRING &&
                    strcmp(expression->type, "void") == 0 &&
                    (count = PrintFormatPieces(function->exprs[first].text,
                                               pieces, PRINT_PIECES_MAX)) >= 0;
            for(int i = 0; valid && i < count; i++)
                placeholders += pieces[i].is_argument;
            free(pieces);
            for(int child = first; valid && child >= 0;
                child = function->exprs[child].next_sibling) {
                const char *type = ScalarType(function->exprs[child].type);
                if(child != first) {
                    arguments++;
                    valid = type[0] != '\0' && strcmp(type, "void") != 0;
                }
                valid = valid && verify_expression(module, function, bindings,
                                                   binding_count, child,
                                                   depth + 1);
            }
            return valid && placeholders == arguments;
        }
        if(!strcmp(expression->name, "VecPush") ||
           !strcmp(expression->name, "VecClear") ||
           !strcmp(expression->name, "VecFree") ||
           !strcmp(expression->name, "VecSwap") ||
           !strcmp(expression->name, "VecPop") ||
           !strcmp(expression->name, "VecGet") ||
           !strcmp(expression->name, "VecClone") ||
           !strcmp(expression->name, "VecSlice") ||
           !strcmp(expression->name, "BuilderAppend") ||
           !strcmp(expression->name, "BuilderFinish")) {
            int first = expression->first_child;
            int second = first >= 0 ?
                function->exprs[first].next_sibling : -1;
            int push = !strcmp(expression->name, "VecPush");
            int swap = !strcmp(expression->name, "VecSwap");
            int pop = !strcmp(expression->name, "VecPop");
            int get = !strcmp(expression->name, "VecGet");
            int clone = !strcmp(expression->name, "VecClone");
            int view = !strcmp(expression->name, "VecSlice");
            int text_append = !strcmp(expression->name, "BuilderAppend");
            int text_finish = !strcmp(expression->name, "BuilderFinish");
            char element[ZIR_NAME_MAX];
            if(first < 0 || assignment_root(function, first) == NULL ||
               !VecElementType(module, function->exprs[first].type,
                               element, sizeof(element)) ||
               !verify_expression(module, function, bindings, binding_count,
                                  first, depth + 1))
                return 0;
            if(clone)
                return second >= 0 &&
                       function->exprs[second].next_sibling < 0 &&
                       assignment_root(function, second) != NULL &&
                       strcmp(function->exprs[first].type,
                              function->exprs[second].type) == 0 &&
                       verify_expression(module, function, bindings,
                                         binding_count, second, depth + 1) &&
                       strcmp(expression->type, "bool") == 0;
            if(view) {
                int third = second >= 0 ?
                    function->exprs[second].next_sibling : -1;
                char expected[ZIR_NAME_MAX];
                int written;
                if(third < 0 || function->exprs[third].next_sibling >= 0 ||
                   !integer_type(function->exprs[second].type) ||
                   !integer_type(function->exprs[third].type) ||
                   !verify_expression(module, function, bindings,
                                      binding_count, second, depth + 1) ||
                   !verify_expression(module, function, bindings,
                                      binding_count, third, depth + 1))
                    return 0;
                written = snprintf(expected, sizeof(expected), "[]%s",
                                   element);
                return written > 0 &&
                       (size_t)written < sizeof(expected) &&
                       strcmp(expression->type, expected) == 0;
            }
            if(pop || get) {
                const ZirModule *owner = NULL;
                const ZirType *record_type = FindType(module,
                    expression->type, &owner);
                size_t offset = 0;
                ZirTypeField field;
                if(record_type == NULL || record_type->is_enum ||
                   record_type->is_record_template ||
                   record_type->is_procedure_type)
                    return 0;
                if(get && (second < 0 ||
                           function->exprs[second].next_sibling >= 0 ||
                           !integer_type(function->exprs[second].type) ||
                           !verify_expression(module, function, bindings,
                                              binding_count, second,
                                              depth + 1)))
                    return 0;
                if(!get && second >= 0)
                    return 0;
                return TypeNextField(record_type, &offset, &field) == 1 &&
                       strcmp(field.name, "has_value") == 0 &&
                       strcmp(field.type, "bool") == 0 &&
                       TypeNextField(record_type, &offset, &field) == 1 &&
                       strcmp(field.name, "value") == 0 &&
                       strcmp(field.type, element) == 0 &&
                       TypeNextField(record_type, &offset, &field) == 0;
            }
            if(text_append || text_finish) {
                if(strcmp(element, "u8") != 0)
                    return 0;
                if(text_finish)
                    return second < 0 &&
                           strcmp(expression->type, "string") == 0;
                return second >= 0 &&
                       function->exprs[second].next_sibling < 0 &&
                       strcmp(function->exprs[second].type, "string") == 0 &&
                       strcmp(expression->type, "bool") == 0 &&
                       verify_expression(module, function, bindings,
                                         binding_count, second, depth + 1);
            }
            if(strcmp(expression->type, push ? "bool" : "void"))
                return 0;
            if(swap)
                return second >= 0 &&
                       function->exprs[second].next_sibling < 0 &&
                       assignment_root(function, second) != NULL &&
                       !strcmp(function->exprs[first].type,
                               function->exprs[second].type) &&
                       verify_expression(module, function, bindings,
                                         binding_count, second, depth + 1);
            if(!push) return second < 0;
            return second >= 0 &&
                   function->exprs[second].next_sibling < 0 &&
                   verify_expression(module, function, bindings, binding_count,
                                     second, depth + 1) &&
                   (strcmp(function->exprs[second].type, element) == 0 ||
                    strcmp(ScalarType(element),
                           function->exprs[second].type) == 0);
        }
        if(expression->slot_type[0]) {
            int index = binding_index(bindings, binding_count,
                                      expression->name);
            const ZirType *slot = FindType(module, expression->slot_type,
                                           NULL);
            memset(&buffers->signature, 0, sizeof(buffers->signature));
            if(index < 0 || slot == NULL || !slot->is_procedure_type ||
               strcmp(bindings[index].type, expression->slot_type) != 0 ||
               strlen(slot->body) >= sizeof(buffers->signature.args) ||
               strcmp(expression->type, slot->procedure_return_type) != 0)
                return 0;
            copy_text(buffers->signature.args, sizeof(buffers->signature.args), slot->body);
            int expected = parse_parameters(module, &buffers->signature, buffers->parameters);
            if(expected < 0)
                return 0;
            unsigned used = 0;
            for(int child = expression->first_child; child >= 0;
                child = function->exprs[child].next_sibling) {
                int position = function->exprs[child].argument_index;
                if(position < 0 || position >= expected ||
                   (used & (1u << position)) ||
                   !verify_expression(module, function, bindings,
                                      binding_count, child, depth + 1))
                    return 0;
                used |= 1u << position;
                children++;
            }
            return children == expected &&
                   used == ((1u << expected) - 1u);
        }
        int resolved = ResolveFunction(module, expression->name,
                                       &owner, &callee);
        const ZirImport *external = resolved == 0 ?
            host_import(module, expression->name) : NULL;
        if((resolved != 1 || callee == NULL) &&
           (external == NULL || external->extern_kind != ZIR_EXTERN_HOST))
            return 0;
        unsigned used = 0;
        for(int child = expression->first_child; child >= 0;
            child = function->exprs[child].next_sibling) {
            int position = function->exprs[child].argument_index;
            if(position < 0 || position >= VM_MAX_PARAMS ||
               (used & (1u << position)) || ++children > VM_MAX_PARAMS ||
               !verify_expression(module, function, bindings, binding_count,
                                  child, depth + 1))
                return 0;
            used |= 1u << position;
        }
        int expected = external != NULL ?
            parse_import_parameters(module, external, buffers->parameters) :
            parse_parameters(owner, callee, buffers->parameters);
        return expected >= 0 && expected == children &&
               used == ((1u << expected) - 1u);
    default:
        return 0;
    }
}

static int
verify_expression(const ZirModule *module, const ZirFunction *function,
                  const Parameter *bindings, int binding_count,
                  int index, int depth)
{
    static _Thread_local VerifyExpressionBuffers *spares[16];
    static _Thread_local int spare_count;
    VerifyExpressionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = verify_expression_with_buffers(module, function, bindings, binding_count, index, depth, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

const ZirFunction *
find_entry(const ZirProgram *program, const char *module_name,
           const char *function_name, const ZirModule **module_out)
{
    const ZirFunction *entry = NULL;
    *module_out = NULL;
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        if(strcmp(module->name, module_name) != 0)
            continue;
        if(*module_out != NULL)
            return NULL;
        *module_out = module;
        for(int f = 0; f < module->function_count; f++)
            if(strcmp(module->functions[f].name, function_name) == 0) {
                if(entry != NULL)
                    return NULL;
                entry = &module->functions[f];
            }
    }
    return entry;
}

int
is_else_branch(const ZirStmt *statement)
{
    return statement->kind == ZIR_STMT_IF && statement->is_else;
}

int
statement_close(const ZirFunction *function, int begin, int end)
{
    int depth = 1;
    for(int i = begin + 1; i < end; i++) {
        ZirStmtKind kind = function->stmts[i].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE ||
           kind == ZIR_STMT_BLOCK_OPEN)
            depth++;
        else if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0)
            return i;
    }
    return -1;
}

static int
verify_sequence(const ZirModule *module, const ZirFunction *function,
                int begin, int end, Parameter *bindings, int binding_count,
                int loop_depth, int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return 0;
    for(int i = begin; i < end; i++) {
        const ZirStmt *statement = &function->stmts[i];
        int close;
        switch(statement->kind) {
        case ZIR_STMT_DECL:
            if(binding_count >= binding_capacity)
                return 0;
            if(!portable_type(module, statement->type) ||
               strcmp(statement->type, "void") == 0 ||
               statement->name[0] == 0 ||
               (statement->expr_root >= 0 &&
                !verify_expression(module, function, bindings, binding_count,
                                   statement->expr_root, 0)))
                return 0;
            copy_text(bindings[binding_count].name,
                      sizeof(bindings[binding_count].name), statement->name);
            copy_text(bindings[binding_count].type,
                      sizeof(bindings[binding_count].type), statement->type);
            binding_count++;
            break;
        case ZIR_STMT_ASSIGN:
            if(statement->lhs_root < 0 ||
               assignment_root(function, statement->lhs_root) == NULL ||
               (binding_index(bindings, binding_count,
                              assignment_root(function, statement->lhs_root)) < 0 &&
                find_global_declaration(module,
                    assignment_root(function, statement->lhs_root),
                    SpanPath(function->exprs[statement->lhs_root].span)) == NULL) ||
               statement->expr_root < 0 ||
               !verify_expression(module, function, bindings, binding_count,
                                  statement->lhs_root, 0) ||
               !verify_expression(module, function, bindings, binding_count,
                                  statement->expr_root, 0))
                return 0;
            if(strcmp(statement->assignment_op, "=") != 0) {
                const char *operation = assignment_binary_operator(
                    statement->assignment_op);
                const char *destination =
                    function->exprs[statement->lhs_root].type;
                const char *source =
                    function->exprs[statement->expr_root].type;
                const ZirType *flags = FindType(module, destination, NULL);
                int flag_assignment = flags != NULL && flags->is_enum_flags &&
                    (strcmp(source, destination) == 0 || integer_type(source)) &&
                    (strcmp(operation ? operation : "", "&") == 0 ||
                     strcmp(operation ? operation : "", "|") == 0 ||
                     strcmp(operation ? operation : "", "^") == 0 ||
                     strcmp(operation ? operation : "", "+") == 0 ||
                     strcmp(operation ? operation : "", "-") == 0);
                if(!flag_assignment && (operation == NULL || !scalar_type(destination) ||
                   !scalar_type(source) ||
                   strcmp(destination, "string") == 0 ||
                   strcmp(source, "string") == 0 ||
                   strcmp(destination, "bool") == 0 ||
                   strcmp(destination, "void") == 0 ||
                   strcmp(source, "bool") == 0 ||
                   strcmp(source, "void") == 0 ||
                   ((strcmp(operation, "%") == 0 ||
                     bitwise_operator(operation)) &&
                    (!integer_type(destination) ||
                     !integer_type(source)))))
                    return 0;
            }
            break;
        case ZIR_STMT_RETURN:
            if((statement->expr_root < 0) !=
               (strcmp(function->return_type, "void") == 0))
                return 0;
            if(statement->expr_root >= 0 &&
               !verify_expression(module, function, bindings, binding_count,
                                  statement->expr_root, 0))
                return 0;
            break;
        case ZIR_STMT_EXPR:
        case ZIR_STMT_UNUSED:
            if(statement->expr_root < 0 ||
               !verify_expression(module, function, bindings, binding_count,
                                  statement->expr_root, 0))
                return 0;
            break;
        case ZIR_STMT_IF: {
            int branch = i;
            int saw_else = 0;
            if(is_else_branch(statement))
                return 0;
            do {
                const ZirStmt *head = &function->stmts[branch];
                close = statement_close(function, branch, end);
                if(close < 0 || (saw_else && branch != i) ||
                   (head->expr_root < 0 && !is_else_branch(head)))
                    return 0;
                if(head->expr_root < 0)
                    saw_else = 1;
                else if(!verify_expression(module, function, bindings,
                                           binding_count, head->expr_root, 0) ||
                        strcmp(function->exprs[head->expr_root].type,
                               "bool") != 0)
                    return 0;
                if(!verify_sequence(module, function, branch + 1, close,
                                    bindings, binding_count, loop_depth,
                                    depth + 1))
                    return 0;
                branch = close + 1;
            } while(branch < end &&
                    is_else_branch(&function->stmts[branch]));
            i = branch - 1;
            break;
        }
        case ZIR_STMT_WHILE:
            close = statement_close(function, i, end);
            if(close < 0 || statement->expr_root < 0 ||
               !verify_expression(module, function, bindings, binding_count,
                                  statement->expr_root, 0) ||
               strcmp(function->exprs[statement->expr_root].type,
                      "bool") != 0 ||
               !verify_sequence(module, function, i + 1, close, bindings,
                                binding_count, loop_depth + 1, depth + 1))
                return 0;
            i = close;
            break;
        case ZIR_STMT_BLOCK_OPEN:
            close = statement_close(function, i, end);
            if(close < 0 || !verify_sequence(module, function, i + 1, close,
                                             bindings, binding_count,
                                             loop_depth, depth + 1))
                return 0;
            i = close;
            break;
        case ZIR_STMT_BREAK:
        case ZIR_STMT_CONTINUE:
            if(loop_depth == 0)
                return 0;
            break;
        case ZIR_STMT_UNREACHABLE:
            break;
        default:
            return 0;
        }
    }
    return 1;
}

/* Buffers verify_global_aggregate keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct VerifyGlobalAggregateBuffers {
    ZirFunction probe;
} VerifyGlobalAggregateBuffers;

static int verify_global_aggregate(const ZirModule *module, const ZirGlobal *global);

static int
verify_global_aggregate_with_buffers(const ZirModule *module, const ZirGlobal *global, VerifyGlobalAggregateBuffers *buffers)
{
    memset(&buffers->probe, 0, sizeof(buffers->probe));
    Vm scratch = {0};
    Value target = default_value(&scratch, module, global->type, 0);
    int root = !scratch.failed ? ParseExprTyped(&buffers->probe, module, global->init,
                                                global->span, global->type) : -1;
    int valid = root >= 0 && !scratch.failed &&
        fold_global_element(&scratch, module, &buffers->probe, root, &target,
                            global->type, global->span) && !scratch.failed;
    free(buffers->probe.exprs);
    free_records(&scratch);
    free_arrays(&scratch);
    free_layouts(&scratch);
    free_strings(&scratch);
    return valid;
}

/* Check aggregate literals with the same evaluator used during instance
 * creation, so nested runtime expressions fail before a bundle is written. */
static int
verify_global_aggregate(const ZirModule *module, const ZirGlobal *global)
{
    static _Thread_local VerifyGlobalAggregateBuffers *spares[16];
    static _Thread_local int spare_count;
    VerifyGlobalAggregateBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = verify_global_aggregate_with_buffers(module, global, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers VmVerify keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct VmVerifyBuffers {
    Parameter bindings[VM_MAX_PARAMS];
    Parameter expected[VM_MAX_PARAMS];
    Parameter actual[VM_MAX_PARAMS];
    Parameter parameters[VM_MAX_PARAMS];
    char literal[ZIR_TEXT_MAX];
} VmVerifyBuffers;

int VmVerify(const ZirProgram *program, const char *entry_module,
            const char *entry_function);

static int
VmVerify_with_buffers(const ZirProgram *program, const char *entry_module,
            const char *entry_function, VmVerifyBuffers *buffers)
{
    const ZirModule *entry_owner;
    const ZirFunction *entry;
    if(program == NULL || entry_module == NULL || entry_function == NULL)
        return 0;
    entry = find_entry(program, entry_module, entry_function, &entry_owner);
    if(entry == NULL || entry->is_extern ||
       parse_parameters(entry_owner, entry, buffers->bindings) != 0 ||
       (strcmp(entry->return_type, "s32") != 0 &&
        strcmp(entry->return_type, "s64") != 0 &&
        strcmp(entry->return_type, "bool") != 0 &&
        strcmp(entry->return_type, "void") != 0)) {
        Diagnostic(Span("<bundle>", 1, 1), "zib.entry",
                      "entry must be one unique zero-argument integer, bool, or void Ziran function");
        return 0;
    }
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        for(int other = 0; other < m; other++)
            if(strcmp(program->modules[other].name, module->name) == 0) {
                Diagnostic(module->span, "zib.module",
                              "duplicate module identity: %s", module->name);
                return 0;
            }
        for(int t = 0; t < module->type_count; t++)
            if(module->types[t].is_union &&
               !portable_union(module, &module->types[t])) {
                Diagnostic(module->types[t].span, "zib.union",
                           "portable unions support scalar fields only");
                return 0;
            }
        for(int i = 0; i < module->import_count; i++) {
            if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
               module->imports[i].extern_kind == ZIR_EXTERN_HOST) {
                const ZirImport *import = &module->imports[i];
                if(strncmp(import->target, "ziran:", 6) == 0) {
                    const ZirModule *provider_module = NULL;
                    const ZirFunction *provider = bound_provider(program,
                        import, &provider_module);
                    int expected_count = parse_import_parameters(module,
                        import, buffers->expected);
                    int actual_count = provider != NULL ?
                        parse_parameters(provider_module, provider, buffers->actual) : -1;
                    int valid = provider != NULL &&
                        strcmp(import->return_type, provider->return_type) == 0 &&
                        expected_count >= 0 && expected_count == actual_count &&
                        host_type_at(module, import->return_type, 0, 0) &&
                        same_bound_type(module, provider_module,
                                        import->return_type);
                    for(int p = 0; valid && p < expected_count; p++)
                        valid = strcmp(buffers->expected[p].type, buffers->actual[p].type) == 0 &&
                            host_type_at(module, buffers->expected[p].type, 0, 1) &&
                            strcmp(buffers->expected[p].type, "void") != 0 &&
                            same_bound_type(module, provider_module,
                                            buffers->expected[p].type);
                    if(!valid) {
                        Diagnostic(import->span, "zib.bind",
                                   "bound Ziran host provider has a missing or mismatched signature: %s:%s",
                                   module->name, import->name);
                        return 0;
                    }
                    continue;
                }
                int count = parse_import_parameters(module,
                    &module->imports[i], buffers->parameters);
                int host_signature =
                    host_type_at(module, module->imports[i].return_type, 0, 0) &&
                    count >= 0;
                for(int p = 0; host_signature && p < count; p++)
                    host_signature = host_type_at(module, buffers->parameters[p].type, 0, 1) &&
                        strcmp(buffers->parameters[p].type, "void") != 0;
                if(host_signature)
                    continue;
            }
            if((module->imports[i].kind != ZIR_IMPORT_OPEN &&
                module->imports[i].kind != ZIR_IMPORT_MODULE) ||
               module->imports[i].resolved_module == NULL) {
                Diagnostic(module->imports[i].span, "zib.capability",
                              "portable execution requires an included Ziran module or supported host capability");
                return 0;
            }
        }
        for(int g = 0; g < module->global_count; g++) {
            const ZirGlobal *global = &module->globals[g];
            if(!portable_type(module, global->type)) {
                Diagnostic(global->span, "zib.global",
                           "portable globals need a value type");
                return 0;
            }
            if(global->init[0]) {
                long value = 0;
                long folded = 0;
                const char *init = skip_ws(global->init);
                int literal_ok =
                    EvaluateCompileExpression(module, global->init,
                                              global->span, 0, &folded);
                if(!literal_ok && !strcmp(global->type, "string"))
                    literal_ok = init[0] == '"';
                if(!literal_ok &&
                   (!strcmp(global->type, "float32") ||
                    !strcmp(global->type, "float64"))) {
                    char *end = NULL;
                    strtod(init, &end);
                    literal_ok = end != init && *skip_ws(end) == '\0';
                }
                if(!literal_ok && !strcmp(global->type, "bool"))
                    literal_ok = !strcmp(init, "true") ||
                                 !strcmp(init, "false");
                if(!literal_ok) {
                    const ZirType *record = FindType(module, global->type,
                                                     NULL);
                    if(record != NULL && !record->is_enum &&
                       !record->is_procedure_type && !record->is_union &&
                       !SliceElementType(global->type, NULL, 0) &&
                       !ArrayElementType(global->type, NULL, 0, NULL))
                        literal_ok = verify_global_aggregate(module, global);
                }
                if(!literal_ok &&
                   ArrayElementType(global->type, NULL, 0, NULL))
                    literal_ok = verify_global_aggregate(module, global);
                if(!literal_ok ||
                   SliceElementType(global->type, NULL, 0)) {
                    Diagnostic(global->span, "zib.global",
                               "portable global initializers need a scalar, string, record, or array literal value");
                    return 0;
                }
            }
        }
        for(int f = 0; f < module->function_count; f++) {
            const ZirFunction *function = &module->functions[f];
            binding_capacity = function_local_bound(function, VM_MAX_PARAMS);
            Parameter *bindings = AllocateOrExit((size_t)binding_capacity * sizeof(*bindings));
            int binding_count = parse_parameters(module, function, bindings);
            if(binding_count < 0 && parameters_exceeded) {
                free(bindings);
                Diagnostic(function->span, "zib.function",
                           "portable functions take at most %d parameters: %s",
                           VM_MAX_PARAMS, function->name);
                return 0;
            }
            if(!function->checked || function->is_extern ||
               !portable_type(module, function->return_type) ||
               binding_count < 0) {
                free(bindings);
                Diagnostic(function->span, "zib.function",
                              "function is outside the portable subset: %s",
                              function->name);
                return 0;
            }
            int verified = verify_sequence(module, function, 0, function->stmt_count,
                                           bindings, binding_count, 0, 0);
            free(bindings);
            if(!verified) {
                Diagnostic(function->span, "zib.statement",
                           "statement is outside the portable subset in %s",
                           function->name);
                return 0;
            }
            if(strcmp(function->return_type, "void") != 0 &&
               !FunctionReturnsOnEveryPath(function)) {
                Diagnostic(function->span, "zib.return",
                           "portable functions must return on every path: %s",
                           function->name);
                return 0;
            }
        }
    }
    return 1;
}

int
VmVerify(const ZirProgram *program, const char *entry_module,
            const char *entry_function)
{
    static _Thread_local VmVerifyBuffers *spares[16];
    static _Thread_local int spare_count;
    VmVerifyBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = VmVerify_with_buffers(program, entry_module, entry_function, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
