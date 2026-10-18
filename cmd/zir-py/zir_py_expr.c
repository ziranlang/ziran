#include "zir_py_internal.h"

static char *emit_identifier(PyEmitter *emitter, int index, const ZirExpr *expression)
{
    char name[ZIR_NAME_MAX * 2];
    const ZirModule *owner = NULL;
    const ZirFunction *function = NULL;
    PyLocal *local;
    if(strcmp(expression->type, "null") == 0 || strcmp(expression->name, "null") == 0)
        return py_copy_string("None");
    if(strcmp(expression->name, "true") == 0 || strcmp(expression->name, "false") == 0)
        return py_copy_string(expression->name[0] == 't' ? "True" : "False");
    if(expression->is_this || strcmp(expression->name, "#this") == 0) {
        function_symbol(emitter, emitter->module, emitter->function, name, sizeof(name));
        return py_copy_string(name);
    }
    local = find_local(emitter, expression->name);
    if(local != NULL)
        return local->boxed ? py_format("%s[0]", local->python) :
                              py_copy_string(local->python);
    if(global_reference(emitter, expression->name, name, sizeof(name), NULL))
        return py_copy_string(name);
    if(ResolveFunctionAt(emitter->module, expression->name,
                         SpanPath(emitter->function->span), &owner, &function) == 1 &&
       owner != NULL && function != NULL) {
        const ZirImport *foreign;
        if(function->is_extern &&
           (foreign = py_foreign_import(owner, function->name)) != NULL)
            foreign_symbol(emitter, owner, foreign, name, sizeof(name));
        else
            function_symbol(emitter, owner, function, name, sizeof(name));
        return py_copy_string(name);
    }
    (void)index;
    py_identifier(expression->name, name, sizeof(name));
    return py_copy_string(name);
}

static char *emit_member(PyEmitter *emitter, const ZirExpr *expression, int as_place)
{
    const char *base_type = base_type_of(emitter, expression);
    PyType base;
    char *object, *path, *result;
    if(!py_classify(emitter->module, base_type, &base))
        unsupported_expression(expression);
    if(base.kind == PY_STRING && strcmp(expression->name, "count") == 0) {
        object = emit_expression(emitter, expression->left);
        result = py_format("len(%s)", object);
        free(object);
        return result;
    }
    if(base.kind == PY_STRING && strcmp(expression->name, "data") == 0) {
        object = emit_expression(emitter, expression->left);
        result = py_format("ZiranPointer(%s, 0)", object);
        free(object);
        return result;
    }
    if(base.kind == PY_ARRAY) {
        if(strcmp(expression->name, "count") == 0)
            return py_format("%d", base.capacity);
        if(strcmp(expression->name, "data") == 0) {
            if(base.capacity == 0)
                return py_copy_string("None");
            object = emit_place(emitter, expression->left);
            result = py_format("ZiranPointer(%s, 0)", object);
            free(object);
            return result;
        }
    }
    if(base.kind == PY_SLICE || base.kind == PY_VEC) {
        object = as_place ? emit_place(emitter, expression->left) :
                            emit_expression(emitter, expression->left);
        result = py_format("%s.%s", object, expression->name);
        free(object);
        return result;
    }
    if(base.kind == PY_POINTER) {
        object = emit_expression(emitter, expression->left);
        path = py_member_path(expression->name);
        result = py_format("%s.value.%s", object, path);
        free(object);
        free(path);
        return result;
    }
    if(base.kind != PY_RECORD && base.kind != PY_UNION)
        unsupported_expression(expression);
    object = emit_place(emitter, expression->left);
    path = py_member_path(expression->name);
    result = py_format("%s.%s", object, path);
    free(object);
    free(path);
    return result;
}

static char *emit_index(PyEmitter *emitter, const ZirExpr *expression)
{
    const ZirExpr *base_expression = expression_at(emitter, expression->left);
    PyType base;
    char *object, *index, *result;
    if(base_expression == NULL ||
       !py_classify(emitter->module, base_expression->type, &base))
        unsupported_expression(expression);
    if(base_expression->kind == ZIR_EXPR_COMPOUND || base_expression->kind == ZIR_EXPR_CALL ||
       base_expression->kind == ZIR_EXPR_CONDITIONAL || base_expression->kind == ZIR_EXPR_STRING)
        object = emit_expression(emitter, expression->left);
    else
        object = emit_place(emitter, expression->left);
    index = emit_expression(emitter, expression->right);
    {
        char *bare = py_bare(index);
        free(index);
        index = bare;
    }
    if(base.kind == PY_STRING || base.kind == PY_ARRAY) {
        char *checked = py_index(emitter, index);
        result = py_format("%s[%s]", object, checked);
        free(checked);
    } else if(base.kind == PY_SLICE || base.kind == PY_VEC || base.kind == PY_POINTER)
        result = py_format("%s[%s]", object, index);
    else {
        unsupported_expression(expression);
        result = NULL;
    }
    free(object);
    free(index);
    return result;
}

/* A place: the text a store writes through. */
char *emit_place(PyEmitter *emitter, int index)
{
    const ZirExpr *expression = expression_at(emitter, index);
    if(expression == NULL)
        return py_copy_string("None");
    switch(expression->kind) {
    case ZIR_EXPR_IDENT:
        return emit_identifier(emitter, index, expression);
    case ZIR_EXPR_MEMBER:
    case ZIR_EXPR_POINTER_MEMBER:
        return emit_member(emitter, expression, 1);
    case ZIR_EXPR_INDEX:
        return emit_index(emitter, expression);
    case ZIR_EXPR_UNARY:
        if(strcmp(expression->op, "*") == 0) {
            char *pointer = emit_expression(emitter, expression->right);
            char *result = py_format("%s.value", pointer);
            free(pointer);
            return result;
        }
        break;
    default:
        break;
    }
    return emit_expression(emitter, index);
}

/* The address of a place. */
static char *emit_address(PyEmitter *emitter, const ZirExpr *address)
{
    const ZirExpr *target = expression_at(emitter, address->right);
    PyType info;
    char *result;
    if(target == NULL)
        unsupported_expression(address);
    py_classify(emitter->module, target->type, &info);
    if(target->kind == ZIR_EXPR_IDENT) {
        PyLocal *local = find_local(emitter, target->name);
        char name[ZIR_NAME_MAX * 2];
        if(local != NULL && local->boxed)
            return py_format("ZiranPointer(%s, 0)", local->python);
        if(local != NULL)
            return py_format("ZiranPointer(%s)", local->python);
        if(global_reference(emitter, target->name, name, sizeof(name), NULL)) {
            if(py_aggregate(info.kind))
                return py_format("ZiranPointer(%s)", name);
            return py_format("ZiranPointer(sys.modules[__name__], \"%s\")", name);
        }
        unsupported_expression(address);
    }
    if(target->kind == ZIR_EXPR_INDEX) {
        const ZirExpr *base_expression = expression_at(emitter, target->left);
        PyType base;
        char *object, *index;
        py_classify(emitter->module, base_expression->type, &base);
        object = emit_place(emitter, target->left);
        index = emit_expression(emitter, target->right);
        if(base.kind == PY_ARRAY || base.kind == PY_STRING)
            result = py_format("ZiranPointer(%s, %s)", object, index);
        else if(base.kind == PY_VEC)
            result = py_format("ZiranPointer(%s.items, %s)", object, index);
        else if(base.kind == PY_SLICE)
            result = py_format("ZiranPointer(%s.base, %s.low + %s)", object, object, index);
        else if(base.kind == PY_POINTER)
            result = py_format("ZiranPointer(%s.base, %s.key + %s)", object, object, index);
        else {
            unsupported_expression(address);
            result = NULL;
        }
        free(object);
        free(index);
        return result;
    }
    if(target->kind == ZIR_EXPR_MEMBER || target->kind == ZIR_EXPR_POINTER_MEMBER) {
        const char *path = target->name;
        const char *last = strrchr(path, '.');
        char field[ZIR_NAME_MAX];
        char *place = emit_place(emitter, target->left);
        PyType base;
        py_classify(emitter->module, base_type_of(emitter, target), &base);
        if(base.kind == PY_POINTER) {
            char *owner = py_format("%s.value", place);
            free(place);
            place = owner;
        }
        if(last != NULL) {
            char *prefix = py_format("%.*s", (int)(last - path), path);
            char *inner = py_member_path(prefix);
            char *joined = py_format("%s.%s", place, inner);
            free(prefix);
            free(inner);
            free(place);
            place = joined;
            path = last + 1;
        }
        py_field_name(path, field, sizeof(field));
        if(py_aggregate(info.kind))
            result = py_format("ZiranPointer(%s.%s)", place, field);
        else
            result = py_format("ZiranPointer(%s, \"%s\")", place, field);
        free(place);
        return result;
    }
    if(target->kind == ZIR_EXPR_UNARY && strcmp(target->op, "*") == 0)
        return emit_expression(emitter, target->right);
    unsupported_expression(address);
    return NULL;
}

char *py_string_literal(const unsigned char *bytes, size_t length)
{
    size_t capacity = length * 4 + 4;
    char *output = py_allocate(capacity);
    size_t used = 0;
    output[used++] = 'b';
    output[used++] = '"';
    for(size_t index = 0; index < length; index++) {
        unsigned char byte = bytes[index];
        if(byte == '"' || byte == '\\') {
            output[used++] = '\\';
            output[used++] = (char)byte;
        } else if(byte == '\n') {
            output[used++] = '\\';
            output[used++] = 'n';
        } else if(byte == '\t') {
            output[used++] = '\\';
            output[used++] = 't';
        } else if(byte == '\r') {
            output[used++] = '\\';
            output[used++] = 'r';
        } else if(byte < 0x20 || byte >= 0x7f)
            used += (size_t)snprintf(output + used, capacity - used, "\\x%02x", byte);
        else
            output[used++] = (char)byte;
    }
    output[used++] = '"';
    output[used] = '\0';
    return output;
}

static char *emit_string(const ZirExpr *expression)
{
    unsigned char *bytes = py_allocate(ZIR_TEXT_MAX * 4);
    size_t length = 0;
    char *result;
    if(!DecodeStringLiteral(expression->text, bytes, ZIR_TEXT_MAX * 4, &length)) {
        free(bytes);
        unsupported_expression(expression);
    }
    result = py_string_literal(bytes, length);
    free(bytes);
    return result;
}

static char *emit_integer(const ZirExpr *expression)
{
    long long value;
    if(py_integer_literal(expression->text, &value)) {
        PyType info;
        int bits = 64, is_signed = 1;
        if(py_integer_width(expression->type, &bits, &is_signed) && !is_signed)
            return py_format("%llu", (unsigned long long)value &
                             (bits >= 64 ? ~0ULL : ((1ULL << bits) - 1)));
        (void)info;
        return value < 0 ? py_format("(%lld)", value) : py_format("%lld", value);
    }
    if(expression->text[0] == '\'' ) {
        unsigned char bytes[16];
        size_t length = 0;
        if(DecodeStringLiteral(expression->text, bytes, sizeof(bytes), &length) && length == 1)
            return py_format("%d", bytes[0]);
    }
    return py_copy_string(expression->text);
}

static char *emit_float(PyEmitter *emitter, const ZirExpr *expression)
{
    char *end;
    double value = strtod(expression->text, &end);
    (void)emitter;
    if(*expression->text && *end == '\0') {
        if(strcmp(expression->type, "float32") == 0)
            return py_round_float32_literal(value);
        if(strpbrk(expression->text, ".eEn") == NULL)
            return py_format("%s.0", expression->text);
        if(expression->text[0] == '.')
            return py_format("0%s", expression->text);
        if(expression->text[strlen(expression->text) - 1] == '.')
            return py_format("%s0", expression->text);
        return py_copy_string(expression->text);
    }
    return py_copy_string(expression->text);
}

int expression_calls(const ZirFunction *function, int index)
{
    if(index < 0 || index >= function->expr_count)
        return 0;
    const ZirExpr *expression = &function->exprs[index];
    if(expression->kind == ZIR_EXPR_CALL)
        return 1;
    for(int child = expression->first_child; child >= 0;
        child = function->exprs[child].next_sibling)
        if(expression_calls(function, child))
            return 1;
    return expression_calls(function, expression->left) ||
           expression_calls(function, expression->right) ||
           expression_calls(function, expression->third);
}

static int child_count(PyEmitter *emitter, const ZirExpr *expression, int *children, int maximum)
{
    int count = 0;
    for(int child = expression->first_child; child >= 0 && count < maximum;
        child = emitter->function->exprs[child].next_sibling)
        children[count++] = child;
    return count;
}

/* The zero value and copy function a Vec needs for its element. */
static void vec_element_helpers(PyEmitter *emitter, const PyType *vec, char **zero, char **copy)
{
    PyType element;
    *zero = py_zero(emitter, emitter->module, vec->element);
    *copy = NULL;
    if(py_classify(emitter->module, vec->element, &element) && py_aggregate(element.kind)) {
        if(element.kind == PY_RECORD || element.kind == PY_UNION) {
            char name[ZIR_NAME_MAX];
            py_class_name(&element, name, sizeof(name));
            *copy = py_format("%s.copy", name);
        } else {
            char *body = py_copy_text(emitter, emitter->module, vec->element, "item", 0);
            *copy = py_format("lambda item: %s", body);
            free(body);
        }
    }
}

static char *emit_vec_call(PyEmitter *emitter, const ZirExpr *expression)
{
    int children[4];
    int count = child_count(emitter, expression, children, 4);
    const ZirExpr *first = count > 0 ? &emitter->function->exprs[children[0]] : NULL;
    PyType vec;
    char *vector, *result = NULL;
    const char *name = expression->name;
    if(first == NULL || !py_classify(emitter->module, first->type, &vec) || vec.kind != PY_VEC)
        unsupported_expression(expression);
    vector = emit_place(emitter, children[0]);
    if(!strcmp(name, "VecFree") && count == 1)
        result = py_format("_vec_free(%s)", vector);
    else if(!strcmp(name, "VecPush") && count == 2) {
        char *value = emit_value(emitter, children[1], vec.element);
        result = py_format("_vec_push(%s, %s)", vector, value);
        free(value);
    } else if((!strcmp(name, "VecPop") && count == 1) ||
              (!strcmp(name, "VecGet") && count == 2)) {
        PyType option;
        char class_name[ZIR_NAME_MAX];
        char *zero, *copy;
        if(!py_classify(emitter->module, expression->type, &option) || option.kind != PY_RECORD)
            unsupported_expression(expression);
        py_class_name(&option, class_name, sizeof(class_name));
        vec_element_helpers(emitter, &vec, &zero, &copy);
        if(!strcmp(name, "VecPop"))
            result = py_format("_vec_pop(%s, %s, %s)", vector, class_name, zero);
        else {
            char *index = emit_expression(emitter, children[1]);
            char *bare = py_bare(index);
            if(copy != NULL)
                result = py_format("_vec_get(%s, %s, %s, %s, %s)", vector, bare,
                                   class_name, zero, copy);
            else
                result = py_format("_vec_get(%s, %s, %s, %s)", vector, bare, class_name, zero);
            free(index);
            free(bare);
        }
        free(zero);
        free(copy);
    } else if(!strcmp(name, "VecClone") && count == 2) {
        char *source = emit_place(emitter, children[1]);
        char *zero, *copy;
        vec_element_helpers(emitter, &vec, &zero, &copy);
        if(copy != NULL)
            result = py_format("_vec_clone(%s, %s, %s)", vector, source, copy);
        else
            result = py_format("_vec_clone(%s, %s)", vector, source);
        free(zero);
        free(copy);
        free(source);
    } else if(!strcmp(name, "VecSlice") && count == 3) {
        char *low = emit_expression(emitter, children[1]);
        char *high = emit_expression(emitter, children[2]);
        char *bare_low = py_bare(low), *bare_high = py_bare(high);
        result = py_format("_vec_slice(%s, %s, %s)", vector, bare_low, bare_high);
        free(low);
        free(high);
        free(bare_low);
        free(bare_high);
    } else if(!strcmp(name, "VecClear") && count == 1)
        result = py_format("_vec_clear(%s)", vector);
    else if(!strcmp(name, "VecSwap") && count == 2) {
        char *other = emit_place(emitter, children[1]);
        result = py_format("_vec_swap(%s, %s)", vector, other);
        free(other);
    } else if(!strcmp(name, "BuilderAppend") && count == 2) {
        char *text = emit_expression(emitter, children[1]);
        char *bare = py_bare(text);
        result = py_format("_builder_append(%s, %s)", vector, bare);
        free(text);
        free(bare);
    } else if(!strcmp(name, "BuilderFinish") && count == 1)
        result = py_format("_builder_finish(%s)", vector);
    else
        unsupported_expression(expression);
    free(vector);
    return result;
}

static int is_vec_builtin(const char *name)
{
    static const char *const names[] = {
        "VecPush", "VecPop", "VecGet", "VecFree", "VecClone", "VecSlice",
        "VecClear", "VecSwap", "BuilderAppend", "BuilderFinish", NULL
    };
    for(int index = 0; names[index] != NULL; index++)
        if(strcmp(name, names[index]) == 0)
            return 1;
    return 0;
}

/* The ctypes type for a foreign parameter or result, or NULL. */
const char *py_ctype(PyEmitter *emitter, const char *type)
{
    PyType info;
    if(!py_classify(emitter->module, type, &info))
        return NULL;
    switch(info.kind) {
    case PY_VOID: return "None";
    case PY_BOOL: return "ctypes.c_bool";
    case PY_INT:
        switch(info.bits) {
        case 8: return info.is_signed ? "ctypes.c_int8" : "ctypes.c_uint8";
        case 16: return info.is_signed ? "ctypes.c_int16" : "ctypes.c_uint16";
        case 32: return info.is_signed ? "ctypes.c_int32" : "ctypes.c_uint32";
        default: return info.is_signed ? "ctypes.c_int64" : "ctypes.c_uint64";
        }
    case PY_FLOAT: return info.bits == 32 ? "ctypes.c_float" : "ctypes.c_double";
    case PY_STRING: return "ZiranCString";
    case PY_POINTER:
    case PY_PROCEDURE:
    case PY_NULL:
        return "ctypes.c_void_p";
    default:
        return NULL;
    }
}

/* A pointer parameter to scalar items passes a C copy of them. */
const char *py_pointer_ctype(PyEmitter *emitter, const char *type)
{
    PyType info, element;
    if(!py_classify(emitter->module, type, &info) || info.kind != PY_POINTER ||
       !py_classify(emitter->module, info.element, &element))
        return NULL;
    if(element.kind == PY_POINTER) return "ctypes.c_void_p";
    if(element.kind == PY_VOID) return "ctypes.c_uint8";
    if(element.kind != PY_INT && element.kind != PY_FLOAT && element.kind != PY_BOOL)
        return NULL;
    return py_ctype(emitter, info.element);
}

/* The parameter types of a foreign import, in order. */
int foreign_parameters(const ZirImport *import, char types[][ZIR_NAME_MAX], int maximum)
{
    const ZirParameters *parameters = ParametersOf(import->args);
    int used = 0;
    for(int index = 0; index < parameters->count && used < maximum; index++) {
        const ZirParameter *parameter = &parameters->items[index];
        const char *type = parameter->value_type ? parameter->value_type : parameter->text;
        if(!strncmp(type, "..", 2)) break;
        copy_text(types[used++], ZIR_NAME_MAX, type);
    }
    return used;
}

static char *join_arguments(char **arguments, int count)
{
    char *result = py_copy_string("");
    for(int index = 0; index < count; index++) {
        char *joined = py_format("%s%s%s", result, index ? ", " : "", arguments[index]);
        free(result);
        free(arguments[index]);
        result = joined;
    }
    return result;
}

/* A host capability bound with --bind to an exported Ziran function. */
static int bound_provider(PyEmitter *emitter, const ZirImport *import,
                          const ZirModule **owner, const ZirFunction **function)
{
    if(strncmp(import->target, "ziran:", 6) != 0)
        return 0;
    for(int program_index = 0; program_index < emitter->program_count; program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++) {
            const ZirModule *module = &program->modules[module_index];
            if(strcmp(module->name, import->target + 6) != 0)
                continue;
            for(int index = 0; index < module->function_count; index++)
                if(strcmp(module->functions[index].name, import->extern_symbol) == 0 &&
                   !module->functions[index].is_extern) {
                    *owner = module;
                    *function = &module->functions[index];
                    return 1;
                }
        }
    }
    return 0;
}

/* ----- Python natives: #system_library "py:module" ----- */

/* object.name, or getattr for a name Python reserves as a keyword. */
static char *py_attribute(const char *object, const char *name)
{
    static const char *const reserved[] = {
        "False", "None", "True", "and", "as", "assert", "async", "await", "break",
        "class", "continue", "def", "del", "elif", "else", "except", "finally",
        "for", "from", "global", "if", "import", "in", "is", "lambda", "nonlocal",
        "not", "or", "pass", "raise", "return", "try", "while", "with", "yield", NULL
    };
    for(int index = 0; reserved[index] != NULL; index++)
        if(strcmp(reserved[index], name) == 0)
            return py_format("getattr(%s, \"%s\")", object, name);
    return py_format("%s.%s", object, name);
}

/* The one variable holding an imported Python module: dots become "_"
 * after "_" doubles, so distinct module paths keep distinct names. */
void py_module_symbol(const char *path, char *output, size_t size)
{
    size_t used = 0;
    const char *prefix = "_py_";
    for(const char *p = prefix; *p && used + 1 < size; p++)
        output[used++] = *p;
    for(const char *p = path; *p && used + 2 < size; p++) {
        if(*p == '_')
            output[used++] = '_';
        output[used++] = *p == '.' ? '_' : *p;
    }
    output[used] = '\0';
}

/* object.a.b for the dotted attribute path a.b. */
static char *py_attribute_path(const char *object, const char *path)
{
    char *result = py_copy_string(object);
    char part[ZIR_NAME_MAX];
    size_t used = 0;
    for(const char *p = path;; p++) {
        if(*p == '.' || *p == '\0') {
            char *next;
            part[used] = '\0';
            next = py_attribute(result, part);
            free(result);
            result = next;
            used = 0;
            if(*p == '\0')
                return result;
        } else if(used + 1 < sizeof(part))
            part[used++] = *p;
    }
}

/* How a Python value becomes a Ziran value of type; NULL keeps it. */
static char *py_from_python(const ZirModule *scope, const char *type)
{
    PyType info;
    if(!py_classify(scope, type, &info))
        return NULL;
    switch(info.kind) {
    case PY_STRING: return py_copy_string("_py_text");
    case PY_SLICE: {
        if(!strcmp(info.element, "u8")) return py_copy_string("_py_u8");
        char *element = py_from_python(scope, info.element);
        char *result = py_format("(lambda value: _py_slice(value, %s))",
                                element != NULL ? element : "None");
        free(element);
        return result;
    }
    case PY_BOOL: return py_copy_string("bool");
    case PY_FLOAT: return py_copy_string("float");
    case PY_INT:
        return py_format("(lambda value: _%c%d(int(value)))",
                         info.is_signed ? 's' : 'u', info.bits);
    default: return NULL;
    }
}

/* expression, a Python value, as a Ziran value of type. */
static char *py_converted_from_python(const ZirModule *scope, const char *type,
                                      const char *expression)
{
    PyType info;
    if(py_classify(scope, type, &info) && info.kind == PY_INT)
        return py_format("_%c%d(int(%s))", info.is_signed ? 's' : 'u', info.bits, expression);
    {
        char *convert = py_from_python(scope, type);
        char *result = convert != NULL ? py_format("%s(%s)", convert, expression) :
            py_copy_string(expression);
        free(convert);
        return result;
    }
}

/* How a Ziran value of type becomes a Python value; NULL keeps it. */
static char *py_to_python(const ZirModule *scope, const char *type)
{
    PyType info;
    if(!py_classify(scope, type, &info))
        return NULL;
    if(info.kind == PY_STRING)
        return py_copy_string("_py_str");
    if(info.kind == PY_SLICE) {
        if(!strcmp(info.element, "u8")) return py_copy_string("_py_bytes");
        char *element = py_to_python(scope, info.element);
        char *result = py_format("(lambda value: _py_list(value, %s))",
                                element != NULL ? element : "None");
        free(element);
        return result;
    }
    return NULL;
}

/* A Ziran procedure passed to Python converts what Python calls it with. */
static char *py_callback(const ZirModule *scope, const ZirType *procedure, const char *value)
{
    char *converters = py_copy_string("");
    char *returned, *result;
    int count = 0;
    if(*procedure->body) {
        char *parts = py_allocate(64 * 512);
        count = split_top_level(procedure->body, parts, 64, 512);
        for(int part = 0; part < count; part++) {
            char *colon = strchr(parts + part * 512, ':');
            char *type = colon != NULL ? colon + 1 : parts + part * 512;
            char *convert, *joined;
            size_t length;
            while(*type == ' ')
                type++;
            length = strlen(type);
            while(length > 0 && isspace((unsigned char)type[length - 1]))
                type[--length] = '\0';
            convert = py_from_python(scope, type);
            joined = py_format("%s%s%s", converters, part ? ", " : "",
                               convert != NULL ? convert : "None");
            free(convert);
            free(converters);
            converters = joined;
        }
        free(parts);
    }
    returned = py_to_python(scope, procedure->procedure_return_type);
    result = py_format("_py_callback(%s, (%s%s), %s)", value, converters,
                       count == 1 ? "," : "", returned != NULL ? returned : "None");
    free(returned);
    free(converters);
    return result;
}

/* A call to a Python native: a module function, a method on the first
 * argument, or with #py_field an attribute read or write. Arguments and the
 * result convert by their declared Ziran types; #py_results catches the
 * exception into the result record. */
static char *emit_python_call(PyEmitter *emitter, const ZirExpr *expression,
                              const ZirModule *owner, const ZirImport *foreign)
{
    char module_symbol[ZIR_PATH_MAX * 2];
    char python_module[ZIR_PATH_MAX], receiver[ZIR_NAME_MAX], name[ZIR_NAME_MAX];
    char parameters[32][ZIR_NAME_MAX];
    int count = foreign_parameters(foreign, parameters, 32);
    int children[64];
    int child_total = child_count(emitter, expression, children, 64);
    char *arguments[64];
    char *call, *joined, *result;
    const ZirModule *scope = owner != NULL ? owner : emitter->module;
    int parts = PyForeignCallParts(foreign->target, python_module, sizeof(python_module),
                                   receiver, sizeof(receiver), name, sizeof(name));
    int named = 0;
    for(int index = 0; index < child_total; index++)
        named |= emitter->function->exprs[children[index]].argument_name[0] != '\0';
    for(int index = 0; index < child_total; index++) {
        const ZirExpr *argument = &emitter->function->exprs[children[index]];
        int parameter = argument->argument_index >= 0 ? argument->argument_index : index;
        const char *type = parameter < count ? parameters[parameter] :
            emitter->function->exprs[children[index]].type;
        char *raw = emit_value(emitter, children[index], type);
        char *value = py_bare(raw);
        const ZirModule *type_owner = NULL;
        const ZirType *declared = FindType(scope, type, &type_owner);
        char *convert = py_to_python(scope, type);
        free(raw);
        if(declared != NULL && declared->is_procedure_type) {
            arguments[index] = py_callback(type_owner != NULL ? type_owner : scope,
                                           declared, value);
            free(value);
        } else if(convert != NULL) {
            arguments[index] = py_format("%s(%s)", convert, value);
            free(value);
        } else
            arguments[index] = value;
        free(convert);
        if(named) {
            char *item = py_format("(%d, %s%s%s, %s)", parameter,
                argument->argument_name[0] ? "\"" : "",
                argument->argument_name[0] ? argument->argument_name : "None",
                argument->argument_name[0] ? "\"" : "", arguments[index]);
            free(arguments[index]);
            arguments[index] = item;
        }
    }
    py_module_symbol(python_module, module_symbol, sizeof(module_symbol));
    if(named) {
        joined = join_arguments(arguments, child_total);
        for(int index = 0; index < child_total; index++) arguments[index] = NULL;
        call = py_format("_py_call(%s, \"%s\", %s, (%s%s), %s)", module_symbol, name,
                         parts == 2 ? "True" : "False", joined,
                         child_total == 1 ? "," : "", foreign->py_field ? "True" : "False");
        free(joined);
    } else {
        const char *object = parts == 2 ? arguments[0] : module_symbol;
        int first = parts == 2 ? 1 : 0;
        if(foreign->py_field && child_total > first) {
            /* Set the last attribute of the path on the object before it. */
            char *dot = strrchr(name, '.');
            char *owner_text;
            if(dot != NULL) {
                *dot = '\0';
                owner_text = py_attribute_path(object, name);
                call = py_format("setattr(%s, \"%s\", %s)", owner_text, dot + 1, arguments[first]);
                free(owner_text);
            } else
                call = py_format("setattr(%s, \"%s\", %s)", object, name, arguments[first]);
        } else if(foreign->py_field)
            call = py_attribute_path(object, name);
        else {
            char *function = py_attribute_path(object, name);
            /* join_arguments frees what it joins. */
            joined = join_arguments(arguments + first, child_total - first);
            for(int index = first; index < child_total; index++)
                arguments[index] = NULL;
            call = py_format("%s(%s)", function, joined);
            free(function);
            free(joined);
        }
    }
    for(int index = 0; index < child_total; index++)
        free(arguments[index]);
    if(foreign->py_results) {
        const ZirType *record = FindType(scope, foreign->return_type, NULL);
        ZirTypeField fields[2];
        char value_field[ZIR_NAME_MAX] = "", error_field[ZIR_NAME_MAX];
        char *zero = py_zero(emitter, scope, foreign->return_type);
        char *convert = NULL;
        size_t offset = 0;
        int total = 0, text;
        PyType error_info;
        while(record != NULL && total < 2 &&
              TypeNextField(record, &offset, &fields[total]) == 1)
            total++;
        py_field_name(fields[total - 1].name, error_field, sizeof(error_field));
        text = py_classify(scope, fields[total - 1].type, &error_info) &&
            error_info.kind == PY_STRING;
        if(total == 2) {
            py_field_name(fields[0].name, value_field, sizeof(value_field));
            convert = py_from_python(scope, fields[0].type);
        }
        result = py_format("_py_results(lambda: %s, %s, %s%s%s, %s, \"%s\", %s)", call, zero,
                           total == 2 ? "\"" : "", total == 2 ? value_field : "None",
                           total == 2 ? "\"" : "", convert != NULL ? convert : "None",
                           error_field, text ? "True" : "False");
        free(zero);
        free(convert);
        free(call);
        return result;
    }
    if(!strcmp(foreign->return_type, "void") || !foreign->return_type[0])
        return call;
    result = py_converted_from_python(scope, foreign->return_type, call);
    free(call);
    return result;
}

static char *emit_foreign_call(PyEmitter *emitter, const ZirExpr *expression,
                               const ZirModule *owner, const ZirImport *foreign)
{
    char symbol[ZIR_NAME_MAX * 2];
    char parameters[32][ZIR_NAME_MAX];
    int count = foreign_parameters(foreign, parameters, 32);
    int children[64];
    int child_total = child_count(emitter, expression, children, 64);
    char *arguments[64];
    char *pointers = py_copy_string("");
    char *joined, *result;
    if(foreign->extern_kind == ZIR_EXTERN_PY) {
        free(pointers);
        return emit_python_call(emitter, expression, owner, foreign);
    }
    if(foreign->extern_kind == ZIR_EXTERN_HOST) {
        const ZirModule *provider_owner = NULL;
        const ZirFunction *provider = NULL;
        if(bound_provider(emitter, foreign, &provider_owner, &provider)) {
            function_symbol(emitter, provider_owner, provider, symbol, sizeof(symbol));
            for(int index = 0; index < child_total; index++) {
                const char *type = index < count ? parameters[index] :
                    emitter->function->exprs[children[index]].type;
                char *value = emit_value(emitter, children[index], type);
                arguments[index] = py_bare(value);
                free(value);
            }
            joined = join_arguments(arguments, child_total);
            result = py_format("%s(%s)", symbol, joined);
            free(joined);
            free(pointers);
            return result;
        }
        emitter->uses_host = 1;
        py_identifier(foreign->name, symbol, sizeof(symbol));
        for(int index = 0; index < child_total; index++) {
            const char *type = index < count ? parameters[index] :
                emitter->function->exprs[children[index]].type;
            arguments[index] = emit_value(emitter, children[index], type);
        }
        joined = join_arguments(arguments, child_total);
        result = py_format("host.%s(%s)", symbol, joined);
        free(joined);
        free(pointers);
        return result;
    }
    foreign_symbol(emitter, owner, foreign, symbol, sizeof(symbol));
    for(int index = 0; index < child_total; index++) {
        const ZirExpr *argument = &emitter->function->exprs[children[index]];
        const char *type = index < count ? parameters[index] : argument->type;
        char *raw = emit_value(emitter, children[index], type);
        char *value = py_bare(raw);
        const char *pointer = index < count ? py_pointer_ctype(emitter, type) : NULL;
        PyType info;
        free(raw);
        py_classify(emitter->module, type, &info);
        if(pointer != NULL) {
            char *listed = py_format("%s%s(%d, %s)", pointers, *pointers ? ", " : "",
                                     index, pointer);
            free(pointers);
            pointers = listed;
            arguments[index] = value;
        } else if(info.kind == PY_STRING) {
            arguments[index] = py_format("_c_string(%s)", value);
            free(value);
        } else if(index >= count) {
            /* A variadic argument carries its own C type. */
            const char *ctype = py_ctype(emitter, argument->type);
            if(info.kind == PY_POINTER && py_pointer_ctype(emitter, argument->type) != NULL) {
                char *listed = py_format("%s%s(%d, %s)", pointers, *pointers ? ", " : "",
                                         index, py_pointer_ctype(emitter, argument->type));
                free(pointers);
                pointers = listed;
                arguments[index] = value;
            } else if(ctype != NULL && strcmp(ctype, "None") != 0) {
                arguments[index] = py_format("%s(%s)", ctype, value);
                free(value);
            } else
                arguments[index] = value;
        } else
            arguments[index] = value;
    }
    joined = join_arguments(arguments, child_total);
    if(*pointers)
        result = py_format("_c_call(%s, (%s%s), (%s%s))", symbol, joined,
                           child_total == 1 ? "," : "", pointers,
                           strchr(pointers, ')') == strrchr(pointers, ')') ? "," : "");
    else
        result = py_format("%s(%s)", symbol, joined);
    free(joined);
    free(pointers);
    {
        PyType returned;
        if(py_classify(emitter->module, foreign->return_type, &returned) &&
           returned.kind == PY_STRING) {
            char *text = py_format("_from_c_string(%s)", result);
            free(result);
            result = text;
        } else if(returned.kind == PY_POINTER && strcmp(returned.element, "void")) {
            const char *ctype = py_pointer_ctype(emitter, foreign->return_type);
            if(ctype != NULL) {
                char *pointer = py_format("_from_c_pointer(%s, %s)", result, ctype);
                free(result);
                result = pointer;
            }
        }
    }
    return result;
}

/* The parameter names of a function, in order. */
int function_parameter_names(const ZirFunction *function,
                                    char names[][ZIR_NAME_MAX],
                                    char types[][ZIR_NAME_MAX], int maximum)
{
    const ZirParameters *parameters = ParametersOf(FunctionArgs(function));
    int used = 0;
    for(int index = 0; index < parameters->count && used < maximum; index++) {
        const ZirParameter *parameter = &parameters->items[index];
        if(!parameter->binding_name || !parameter->value_type) continue;
        copy_text(names[used], ZIR_NAME_MAX, parameter->binding_name);
        copy_text(types[used], ZIR_NAME_MAX, parameter->value_type);
        used++;
    }
    return used;
}

const char *place_base(const ZirFunction *function, int index, int *through_member);

/* Whether a function only reads a parameter: it never writes through it,
 * takes its address or a view of it, hands it to a Vec operation, or
 * returns it. Such a parameter can share the caller's value. */
static int parameter_is_read_only(const ZirFunction *function, const char *name)
{
    int through_member;
    const char *base;
    for(int index = 0; index < function->stmt_count; index++) {
        const ZirStmt *statement = &function->stmts[index];
        if(statement->kind == ZIR_STMT_DECL && !strcmp(statement->name, name))
            return 0;
        if(statement->kind == ZIR_STMT_ASSIGN &&
           (base = place_base(function, statement->lhs_root, &through_member)) != NULL &&
           !strcmp(base, name))
            return 0;
        if(statement->kind == ZIR_STMT_RETURN && statement->expr_root >= 0 &&
           function->exprs[statement->expr_root].kind == ZIR_EXPR_IDENT &&
           !strcmp(function->exprs[statement->expr_root].name, name))
            return 0;
    }
    for(int index = 0; index < function->expr_count; index++) {
        const ZirExpr *expression = &function->exprs[index];
        int operand = -1;
        if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "&"))
            operand = expression->right;
        else if(expression->kind == ZIR_EXPR_SLICE ||
                (expression->kind == ZIR_EXPR_MEMBER && !strcmp(expression->name, "data")))
            operand = expression->left;
        else if(expression->kind == ZIR_EXPR_CALL && is_vec_builtin(expression->name))
            operand = expression->first_child;
        if(operand >= 0 && (base = place_base(function, operand, &through_member)) != NULL &&
           !strcmp(base, name))
            return 0;
    }
    return 1;
}

static char *emit_call(PyEmitter *emitter, int index, const ZirExpr *expression)
{
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    char symbol[ZIR_NAME_MAX * 2];
    int children[64];
    int count = child_count(emitter, expression, children, 64);
    char *arguments[64];
    char *joined, *result;
    const ZirImport *foreign;
    (void)index;
    if(!strcmp(expression->name, "zi_new") && expression->type[0] == '*') {
        char *zero = py_zero(emitter, emitter->module, expression->type + 1);
        result = py_format("_heap_new(%s)", zero);
        free(zero);
        return result;
    }
    if(!strcmp(expression->name, "zi_free") && count == 1) {
        char *pointer = emit_expression(emitter, children[0]);
        result = py_format("_heap_free(%s)", pointer);
        free(pointer);
        return result;
    }
    if(is_vec_builtin(expression->name))
        return emit_vec_call(emitter, expression);
    if(strcmp(expression->name, "TextView") == 0 && count == 1) {
        const ZirExpr *argument = &emitter->function->exprs[children[0]];
        PyType info;
        char *view = emit_expression(emitter, children[0]);
        py_classify(emitter->module, argument->type, &info);
        if(info.kind == PY_STRING)
            return view;
        result = py_format("_text_view(%s)", view);
        free(view);
        return result;
    }
    if(expression->slot_type[0]) {
        const ZirModule *slot_owner = NULL;
        const ZirType *slot = FindType(emitter->module, expression->slot_type, &slot_owner);
        char *callable;
        char (*slot_types)[ZIR_NAME_MAX] = py_allocate(64 * ZIR_NAME_MAX);
        int slot_count = 0;
        if(expression->left >= 0)
            callable = emit_expression(emitter, expression->left);
        else {
            ZirExpr probe = *expression;
            probe.kind = ZIR_EXPR_IDENT;
            probe.is_this = 0;
            callable = emit_identifier(emitter, -1, &probe);
        }
        if(slot != NULL && slot->is_procedure_type && *slot->body) {
            char *parts = py_allocate(64 * 512);
            slot_count = split_top_level(slot->body, parts, 64, 512);
            for(int part = 0; part < slot_count; part++) {
                char *colon = strchr(parts + part * 512, ':');
                char *type = colon != NULL ? colon + 1 : parts + part * 512;
                while(*type == ' ')
                    type++;
                snprintf(slot_types[part], ZIR_NAME_MAX, "%s", type);
                size_t length = strlen(slot_types[part]);
                while(length > 0 && isspace((unsigned char)slot_types[part][length - 1]))
                    slot_types[part][--length] = '\0';
            }
            free(parts);
        }
        /* Named arguments go to their checked parameter positions. */
        for(int position = 0; position < count; position++) {
            int chosen = children[position];
            for(int candidate = 0; candidate < count; candidate++)
                if(emitter->function->exprs[children[candidate]].argument_index == position)
                    chosen = children[candidate];
            arguments[position] = position < slot_count ?
                emit_value(emitter, chosen, slot_types[position]) :
                emit_value(emitter, chosen, emitter->function->exprs[chosen].type);
        }
        joined = join_arguments(arguments, count);
        result = py_format("%s(%s)", callable, joined);
        free(joined);
        free(callable);
        free(slot_types);
        return result;
    }
    if(strcmp(expression->name, "print") == 0)
        unsupported_expression(expression);
    if(ResolveFunctionAt(emitter->module, expression->name,
                         SpanPath(emitter->function->span), &owner, &callee) == 1 &&
       owner != NULL && callee != NULL && callee->is_extern &&
       (foreign = py_foreign_import(owner, callee->name)) != NULL)
        return emit_foreign_call(emitter, expression, owner, foreign);
    if((foreign = py_foreign_import(emitter->module, expression->name)) != NULL)
        return emit_foreign_call(emitter, expression, emitter->module, foreign);
    if(owner == NULL || callee == NULL || callee->is_template)
        unsupported_expression(expression);
    function_symbol(emitter, owner, callee, symbol, sizeof(symbol));
    {
        char (*names)[ZIR_NAME_MAX] = py_allocate(64 * ZIR_NAME_MAX);
        char (*types)[ZIR_NAME_MAX] = py_allocate(64 * ZIR_NAME_MAX);
        int parameters = function_parameter_names(callee, names, types, 64);
        int named = 0;
        for(int position = 0; position < count; position++) {
            const ZirExpr *argument = &emitter->function->exprs[children[position]];
            if(argument->argument_index >= 0 && argument->argument_index != position)
                named = 1;
        }
        /* Python evaluates keyword arguments in the order they are written,
         * so named arguments keep source order and reach their parameters. */
        for(int position = 0; position < count; position++) {
            const ZirExpr *argument = &emitter->function->exprs[children[position]];
            int parameter = argument->argument_index >= 0 ? argument->argument_index : position;
            const ZirModule *saved = emitter->module;
            const char *type = parameter < parameters ? types[parameter] : argument->type;
            char *value;
            /* A parameter type is spelled in the callee's module. */
            PyType check;
            if(!py_classify(emitter->module, type, &check))
                type = argument->type;
            py_classify(emitter->module, type, &check);
            int through_member;
            const char *base = place_base(emitter->function, children[position],
                                          &through_member);
            /* A local that no pointer or view reaches cannot change during
             * the call, so a read-only parameter can share it. */
            if(py_aggregate(check.kind) && parameter < parameters && base != NULL &&
               find_local(emitter, base) != NULL && !names_has(&emitter->in_place, base) &&
               parameter_is_read_only(callee, names[parameter]))
                value = emit_expression(emitter, children[position]);
            else
                value = emit_value(emitter, children[position], type);
            emitter->module = saved;
            if(named && parameter < parameters) {
                char name[ZIR_NAME_MAX];
                char *bare = py_bare(value);
                py_identifier(names[parameter], name, sizeof(name));
                arguments[position] = py_format("%s=%s", name, bare);
                free(bare);
                free(value);
            } else {
                arguments[position] = py_bare(value);
                free(value);
            }
        }
        free(names);
        free(types);
    }
    joined = join_arguments(arguments, count);
    result = py_format("%s(%s)", symbol, joined);
    free(joined);
    return result;
}

static int is_integer_kind(PyEmitter *emitter, const char *type, PyType *info)
{
    return py_classify(emitter->module, type, info) && info->kind == PY_INT;
}

static const char *py_operator(const char *op)
{
    if(!strcmp(op, "&&")) return "and";
    if(!strcmp(op, "||")) return "or";
    return op;
}

static int comparison_operator(const char *op)
{
    return !strcmp(op, "==") || !strcmp(op, "!=") || !strcmp(op, "<") ||
           !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=");
}

/* An operand of and/or: Python binds comparisons and not tighter, and a
 * chain of one operator reads without parentheses. */
static char *emit_logical_operand(PyEmitter *emitter, int index, const char *op)
{
    const ZirExpr *operand = expression_at(emitter, index);
    char *text = emit_expression(emitter, index);
    if(operand != NULL &&
       ((operand->kind == ZIR_EXPR_BINARY &&
         (comparison_operator(operand->op) || !strcmp(operand->op, op))) ||
        (operand->kind == ZIR_EXPR_UNARY && !strcmp(operand->op, "!")))) {
        char *bare = py_bare(text);
        free(text);
        return bare;
    }
    return text;
}

static char *emit_binary(PyEmitter *emitter, const ZirExpr *expression)
{
    const ZirExpr *left = expression_at(emitter, expression->left);
    const ZirExpr *right = expression_at(emitter, expression->right);
    const char *op = expression->op;
    PyType result_type, left_type, right_type;
    char *left_text, *right_text, *result;
    int comparison = !strcmp(op, "==") || !strcmp(op, "!=") || !strcmp(op, "<") ||
                     !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=");
    if(left == NULL || right == NULL)
        unsupported_expression(expression);
    py_classify(emitter->module, left->type, &left_type);
    py_classify(emitter->module, right->type, &right_type);
    if((!strcmp(op, "==") || !strcmp(op, "!=")) &&
       (left_type.kind == PY_NULL || right_type.kind == PY_NULL)) {
        int value = left_type.kind == PY_NULL ? expression->right : expression->left;
        char *text = emit_expression(emitter, value);
        result = py_format("(%s is %sNone)", text, !strcmp(op, "==") ? "" : "not ");
        free(text);
        return result;
    }
    if(comparison) {
        /* An untyped literal takes the other operand's integer type. */
        const char *left_as = left->type, *right_as = right->type;
        if(!strcmp(left->type, "integer") && right_type.kind == PY_INT)
            left_as = right->type;
        if(!strcmp(right->type, "integer") && left_type.kind == PY_INT)
            right_as = left->type;
        left_text = emit_value(emitter, expression->left, left_as);
        right_text = emit_value(emitter, expression->right, right_as);
        result = py_format("(%s %s %s)", left_text, op, right_text);
        free(left_text);
        free(right_text);
        return result;
    }
    if(!strcmp(op, "&&") || !strcmp(op, "||")) {
        left_text = emit_logical_operand(emitter, expression->left, op);
        right_text = emit_logical_operand(emitter, expression->right, op);
        result = py_format("(%s %s %s)", left_text, py_operator(op), right_text);
        free(left_text);
        free(right_text);
        return result;
    }
    if(!py_classify(emitter->module, expression->type, &result_type))
        unsupported_expression(expression);
    if(result_type.kind == PY_FLOAT) {
        left_text = emit_value(emitter, expression->left, expression->type);
        right_text = emit_value(emitter, expression->right, expression->type);
        if(!strcmp(op, "/"))
            result = py_format("_fdiv(%s, %s)", left_text, right_text);
        else if(!strcmp(op, "%"))
            result = py_format("_frem(%s, %s)", left_text, right_text);
        else
            result = py_format("(%s %s %s)", left_text, op, right_text);
        if(result_type.bits == 32) {
            char *rounded = py_format("_f32(%s)", py_bare(result));
            free(result);
            result = rounded;
        }
        free(left_text);
        free(right_text);
        return result;
    }
    if(result_type.kind == PY_INT) {
        const char *type = expression->type;
        int bits = result_type.bits, is_signed = result_type.is_signed;
        if(!strcmp(op, "<<") || !strcmp(op, ">>")) {
            char *count = emit_expression(emitter, expression->right);
            char *bare = py_bare(count);
            left_text = emit_value(emitter, expression->left, type);
            {
                char *inner = py_bare(left_text);
                free(left_text);
                left_text = inner;
            }
            if(!strcmp(op, "<<")) {
                char *shifted = py_format("_shl(%s, %s, %d)", left_text, bare, bits);
                result = py_wrap(shifted, bits, is_signed);
                free(shifted);
            } else
                result = py_format("_shr(%s, %s, %d)", left_text, bare, bits);
            free(count);
            free(bare);
            free(left_text);
            return result;
        }
        left_text = emit_value(emitter, expression->left, type);
        right_text = emit_value(emitter, expression->right, type);
        /* With operands that are never negative, C division is Python's. */
        int natural = !is_signed ||
            ((is_nonnegative_literal(left_text) || names_has(&emitter->counters, left_text)) &&
             is_nonnegative_literal(right_text));
        if(!strcmp(op, "/") && natural)
            result = py_format("(%s // %s)", left_text, right_text);
        else if(!strcmp(op, "%") && natural)
            result = py_format("(%s %% %s)", left_text, right_text);
        else if(!strcmp(op, "/")) {
            char *quotient = py_format("_div(%s, %s)", left_text, right_text);
            result = py_wrap(quotient, bits, is_signed);
            free(quotient);
        } else if(!strcmp(op, "%"))
            result = py_format("_rem(%s, %s)", left_text, right_text);
        else if(!strcmp(op, "+") || !strcmp(op, "-") || !strcmp(op, "*")) {
            long long a, b;
            if(text_integer(left_text, &a) && text_integer(right_text, &b)) {
                uint64_t folded;
                if(FoldIntegerOperation(op, strcmp(type, "integer") ? type : "s64",
                                        (uint64_t)a, (uint64_t)b, &folded))
                    result = py_wrapped_literal(folded, bits, is_signed);
                else {
                    char *sum = py_format("%s %s %s", left_text, op, right_text);
                    result = py_wrap(sum, bits, is_signed);
                    free(sum);
                }
            } else {
                char *sum = py_format("%s %s %s", left_text, op, right_text);
                result = py_wrap(sum, bits, is_signed);
                free(sum);
            }
        } else if(!strcmp(op, "&") || !strcmp(op, "|") || !strcmp(op, "^"))
            result = py_format("(%s %s %s)", left_text, op, right_text);
        else {
            unsupported_expression(expression);
            result = NULL;
        }
        free(left_text);
        free(right_text);
        return result;
    }
    if(result_type.kind == PY_BOOL &&
       (!strcmp(op, "&") || !strcmp(op, "|") || !strcmp(op, "^"))) {
        left_text = emit_expression(emitter, expression->left);
        right_text = emit_expression(emitter, expression->right);
        result = py_format("(%s %s %s)", left_text,
                           !strcmp(op, "^") ? "!=" : op, right_text);
        free(left_text);
        free(right_text);
        return result;
    }
    unsupported_expression(expression);
    return NULL;
}

static char *emit_unary(PyEmitter *emitter, const ZirExpr *expression)
{
    const char *op = expression->op;
    PyType info;
    char *operand, *result;
    if(!strcmp(op, "&"))
        return emit_address(emitter, expression);
    if(!strcmp(op, "*")) {
        operand = emit_expression(emitter, expression->right);
        result = py_format("%s.value", operand);
        free(operand);
        return result;
    }
    operand = emit_value(emitter, expression->right, expression->type);
    if(!strcmp(op, "!"))
        result = py_format("(not %s)", operand);
    else if(!strcmp(op, "+"))
        result = py_copy_string(operand);
    else if(!strcmp(op, "-")) {
        if(is_integer_kind(emitter, expression->type, &info)) {
            long long value;
            char *negated = text_integer(operand, &value) ? py_format("-%s", operand) :
                            py_format("-%s", operand);
            result = py_wrap(negated, info.bits, info.is_signed);
            if(text_integer(operand, &value) && result[0] == '-')
                result = py_format("(%s)", result);
            free(negated);
        } else if(py_classify(emitter->module, expression->type, &info) &&
                  info.kind == PY_FLOAT)
            result = py_format("(-%s)", operand);
        else
            result = py_format("(-%s)", operand);
    } else if(!strcmp(op, "~")) {
        if(is_integer_kind(emitter, expression->type, &info) && !info.is_signed) {
            char *inverted = py_format("~%s", operand);
            result = py_wrap(inverted, info.bits, 0);
            free(inverted);
        } else
            result = py_format("(~%s)", operand);
    } else {
        unsupported_expression(expression);
        result = NULL;
    }
    free(operand);
    return result;
}

static char *emit_slice(PyEmitter *emitter, const ZirExpr *expression)
{
    PyType base;
    char *object, *low, *high, *result;
    if(!py_classify(emitter->module, base_type_of(emitter, expression), &base))
        unsupported_expression(expression);
    object = emit_place(emitter, expression->left);
    /* A whole string or view is itself: both are immutable values. */
    if((base.kind == PY_STRING || base.kind == PY_SLICE) &&
       expression->right < 0 && expression->third < 0)
        return object;
    low = expression->right >= 0 ? emit_value(emitter, expression->right, "s64") :
                                   py_copy_string("0");
    /* A missing upper bound is the end, found without reading the base
     * twice. */
    if(expression->third >= 0)
        high = emit_value(emitter, expression->third, "s64");
    else if(base.kind == PY_ARRAY)
        high = py_format("%d", base.capacity);
    else
        high = NULL;
    {
        char *bare_low = py_bare(low), *bare_high = high ? py_bare(high) : NULL;
        free(low);
        free(high);
        low = bare_low;
        high = bare_high;
    }
    if(high == NULL && base.kind == PY_STRING)
        result = py_format("_text_slice(%s, %s)", object, low);
    else if(high == NULL && base.kind == PY_SLICE)
        result = py_format("%s.view(%s)", object, low);
    else if(high == NULL && base.kind == PY_VEC)
        result = py_format("_vec_slice(%s, %s)", object, low);
    else if(base.kind == PY_STRING)
        result = py_format("_text_slice(%s, %s, %s)", object, low, high);
    else if(base.kind == PY_ARRAY)
        result = py_format("_view(%s, %s, %s)", object, low, high);
    else if(base.kind == PY_SLICE)
        result = py_format("%s.view(%s, %s)", object, low, high);
    else if(base.kind == PY_VEC)
        result = py_format("_vec_slice(%s, %s, %s)", object, low, high);
    else {
        unsupported_expression(expression);
        result = NULL;
    }
    free(object);
    free(low);
    free(high);
    return result;
}

static char *emit_compound(PyEmitter *emitter, const ZirExpr *expression)
{
    PyType info;
    char *result;
    if(!py_classify(emitter->module, expression->type, &info))
        unsupported_expression(expression);
    if(info.kind == PY_ARRAY) {
        int count = 0;
        result = py_copy_string("[");
        for(int child = expression->first_child; child >= 0;
            child = emitter->function->exprs[child].next_sibling) {
            const ZirExpr *initializer = &emitter->function->exprs[child];
            int value_index = initializer->kind == ZIR_EXPR_FIELD_INIT ?
                initializer->right : child;
            char *value = emit_value(emitter, value_index, info.element);
            char *bare = py_bare(value);
            char *joined = py_format("%s%s%s", result, count ? ", " : "", bare);
            free(value);
            free(bare);
            free(result);
            result = joined;
            count++;
        }
        if(count < info.capacity) {
            char *zero = py_zero(emitter, emitter->module, info.element);
            for(; count < info.capacity; count++) {
                char *joined = py_format("%s%s%s", result, count ? ", " : "", zero);
                free(result);
                result = joined;
            }
            free(zero);
        }
        char *closed = py_format("%s]", result);
        free(result);
        return closed;
    }
    if(info.kind == PY_RECORD || info.kind == PY_UNION) {
        char name[ZIR_NAME_MAX];
        size_t offset = 0;
        ZirTypeField field;
        int first = 1;
        py_class_name(&info, name, sizeof(name));
        result = py_format("%s(", name);
        while(TypeNextField(info.declared, &offset, &field) == 1) {
            int given = -1;
            char field_name[ZIR_NAME_MAX];
            char *value = NULL;
            for(int child = expression->first_child; child >= 0;
                child = emitter->function->exprs[child].next_sibling)
                if(!strcmp(emitter->function->exprs[child].name, field.name))
                    given = child;
            if(given < 0 && info.kind == PY_UNION)
                continue;
            py_field_name(field.name, field_name, sizeof(field_name));
            if(given >= 0) {
                const ZirExpr *initializer = &emitter->function->exprs[given];
                int value_index = initializer->kind == ZIR_EXPR_FIELD_INIT ?
                    initializer->right : given;
                const ZirModule *saved = emitter->module;
                PyType field_type;
                const char *type = field.type;
                emitter->module = info.owner;
                if(!py_classify(emitter->module, type, &field_type))
                    type = emitter->function->exprs[value_index].type;
                emitter->module = saved;
                {
                    /* Spell the field type in the literal's module when the
                     * record comes from another one. */
                    PyType here;
                    if(!py_classify(emitter->module, type, &here))
                        type = emitter->function->exprs[value_index].type;
                }
                char *raw = emit_value(emitter, value_index, type);
                value = py_bare(raw);
                free(raw);
            } else
                value = py_zero(emitter, info.owner, field.type);
            char *joined = py_format("%s%s%s=%s", result, first ? "" : ", ", field_name, value);
            free(result);
            free(value);
            result = joined;
            first = 0;
        }
        char *closed = py_format("%s)", result);
        free(result);
        return closed;
    }
    unsupported_expression(expression);
    return NULL;
}

static char *emit_cast(PyEmitter *emitter, const ZirExpr *expression)
{
    const ZirExpr *operand = expression_at(emitter, expression->right);
    char *value, *result;
    if(!strcmp(expression->type, "null"))
        return py_copy_string("None");
    if(operand == NULL)
        unsupported_expression(expression);
    value = emit_expression(emitter, expression->right);
    {
        PyType target;
        if(py_classify(emitter->module, expression->type, &target) &&
           target.kind == PY_INT && operand->kind == ZIR_EXPR_INT) {
            long long literal;
            if(py_integer_literal(operand->text, &literal)) {
                free(value);
                result = py_wrapped_literal((unsigned long long)literal, target.bits,
                                            target.is_signed);
                if(result[0] == '-') {
                    char *wrapped = py_format("(%s)", result);
                    free(result);
                    result = wrapped;
                }
                return result;
            }
        }
    }
    result = py_convert(emitter, value, operand->type, expression->type);
    free(value);
    return result;
}

char *emit_expression(PyEmitter *emitter, int index)
{
    const ZirExpr *expression = expression_at(emitter, index);
    if(expression == NULL)
        return py_copy_string("None");
    switch(expression->kind) {
    case ZIR_EXPR_IDENT:
        return emit_identifier(emitter, index, expression);
    case ZIR_EXPR_INT:
        return emit_integer(expression);
    case ZIR_EXPR_FLOAT:
        return emit_float(emitter, expression);
    case ZIR_EXPR_STRING:
        return emit_string(expression);
    case ZIR_EXPR_CALL:
        return emit_call(emitter, index, expression);
    case ZIR_EXPR_MEMBER:
    case ZIR_EXPR_POINTER_MEMBER:
        return emit_member(emitter, expression, 0);
    case ZIR_EXPR_INDEX:
        return emit_index(emitter, expression);
    case ZIR_EXPR_SLICE:
        return emit_slice(emitter, expression);
    case ZIR_EXPR_COMPOUND:
        return emit_compound(emitter, expression);
    case ZIR_EXPR_BINARY:
        return emit_binary(emitter, expression);
    case ZIR_EXPR_UNARY:
        return emit_unary(emitter, expression);
    case ZIR_EXPR_CAST:
        return emit_cast(emitter, expression);
    case ZIR_EXPR_SIZE_OF: {
        size_t size, alignment;
        if(!TypeLayout(emitter->module, expression->name, &size, &alignment))
            unsupported_expression(expression);
        return py_format("%zu", size);
    }
    case ZIR_EXPR_COMPILE_TIME:
        return py_copy_string("False");
    case ZIR_EXPR_CONDITIONAL: {
        char *condition = emit_expression(emitter, expression->left);
        char *yes = emit_value(emitter, expression->right, expression->type);
        char *no = emit_value(emitter, expression->third, expression->type);
        char *result = py_format("(%s if %s else %s)", yes, condition, no);
        free(condition);
        free(yes);
        free(no);
        return result;
    }
    default:
        unsupported_expression(expression);
        return NULL;
    }
}

/* An expression as a value of type: converted like an implicit cast, and
 * copied when it names an aggregate place. */
char *emit_value(PyEmitter *emitter, int index, const char *type)
{
    const ZirExpr *expression = expression_at(emitter, index);
    char *text = emit_expression(emitter, index);
    PyType info;
    if(expression == NULL)
        return text;
    if(type != NULL && *type && py_classify(emitter->module, type, &info)) {
        if(py_aggregate(info.kind)) {
            if(expression_is_place(expression) && !expression->is_move) {
                char *copied = py_copy_text(emitter, emitter->module, type, text, 0);
                free(text);
                return copied;
            }
            return text;
        }
        if(expression->type[0] && strcmp(expression->type, type) != 0) {
            char *converted = py_convert(emitter, text, expression->type, type);
            free(text);
            return converted;
        }
    }
    return text;
}

/* ----- statements ----- */
