#include "zir_py_internal.h"

const ZirExpr *expression_at(PyEmitter *emitter, int index)
{
    if(index < 0 || index >= emitter->function->expr_count)
        return NULL;
    return &emitter->function->exprs[index];
}

void unsupported_expression(const ZirExpr *expression)
{
    DiagnosticTarget(expression->span, "zir_py.expression", "py", "expressions.py",
            "unsupported expression in the Python target: %s",
            expression->text && expression->text[0] ? expression->text :
                ExprKindName(expression->kind));
    exit(1);
}

int py_integer_literal(const char *text, long long *value)
{
    char *end;
    const char *digits = text;
    char cleaned[128];
    size_t used = 0;
    int negative = 0;
    if(text == NULL)
        return 0;
    while(*digits == ' ') digits++;
    if(*digits == '-') {
        negative = 1;
        digits++;
    }
    for(const char *c = digits; *c && used + 1 < sizeof(cleaned); c++)
        if(*c != '_')
            cleaned[used++] = *c;
    cleaned[used] = '\0';
    if(!isdigit((unsigned char)cleaned[0]))
        return 0;
    errno = 0;
    unsigned long long magnitude;
    if(cleaned[0] == '0' && (cleaned[1] == 'b' || cleaned[1] == 'B'))
        magnitude = strtoull(cleaned + 2, &end, 2);
    else if(cleaned[0] == '0' && (cleaned[1] == 'x' || cleaned[1] == 'X'))
        magnitude = strtoull(cleaned + 2, &end, 16);
    else
        magnitude = strtoull(cleaned, &end, 10);
    if(errno != 0 || *end != '\0')
        return 0;
    *value = negative ? (long long)(0 - magnitude) : (long long)magnitude;
    return 1;
}

/* An integer wrapped to a width, spelled in decimal. */
char *py_wrapped_literal(unsigned long long bits, int width, int is_signed)
{
    unsigned long long mask = width >= 64 ? ~0ULL : ((1ULL << width) - 1);
    bits &= mask;
    if(is_signed && width < 64 && (bits >> (width - 1)))
        return py_format("%lld", (long long)(bits | ~mask));
    if(is_signed && width >= 64)
        return py_format("%lld", (long long)bits);
    return py_format("%llu", bits);
}

/* A literal integer text, or NULL. */
int text_integer(const char *text, long long *value)
{
    const char *start = text;
    size_t length = strlen(text);
    char inner[128];
    if(length >= 2 && text[0] == '(' && text[length - 1] == ')') {
        if(length - 2 >= sizeof(inner))
            return 0;
        memcpy(inner, text + 1, length - 2);
        inner[length - 2] = '\0';
        start = inner;
    }
    if(*start == '-') {
        if(!isdigit((unsigned char)start[1]))
            return 0;
    } else if(!isdigit((unsigned char)*start))
        return 0;
    for(const char *c = start + 1; *c; c++)
        if(!isdigit((unsigned char)*c))
            return 0;
    errno = 0;
    char *end;
    long long result = strtoll(start, &end, 10);
    if(errno == ERANGE) {
        unsigned long long unsigned_result = strtoull(start, &end, 10);
        if(errno == ERANGE && *start != '-')
            return 0;
        result = (long long)unsigned_result;
    }
    *value = result;
    return 1;
}

static int text_unsigned_integer(const char *text, unsigned long long *value)
{
    if(!isdigit((unsigned char)text[0]))
        return 0;
    for(const char *c = text; *c; c++)
        if(!isdigit((unsigned char)*c))
            return 0;
    errno = 0;
    *value = strtoull(text, NULL, 10);
    return errno == 0;
}

/* Wrap an integer expression to a width; literals fold. */
char *py_wrap(const char *text, int bits, int is_signed)
{
    long long value;
    unsigned long long unsigned_value;
    if(text_unsigned_integer(text, &unsigned_value))
        return py_wrapped_literal(unsigned_value, bits, is_signed);
    if(text_integer(text, &value))
        return py_wrapped_literal((unsigned long long)value, bits, is_signed);
    return py_format("%s(%s)", py_wrap_name(bits, is_signed), text);
}

static int integer_range_fits(const PyType *from, const PyType *to)
{
    if(from->is_signed == to->is_signed)
        return from->bits <= to->bits;
    if(!from->is_signed && to->is_signed)
        return from->bits < to->bits;
    return 0;
}

char *py_round_float32_literal(double value)
{
    float single = (float)value;
    double rounded = (double)single;
    char text[64];
    if(rounded != rounded)
        return py_copy_string("math.nan");
    if(isinf(rounded))
        return py_copy_string(rounded < 0 ? "-math.inf" : "math.inf");
    for(int precision = 1; precision <= 17; precision++) {
        snprintf(text, sizeof(text), "%.*g", precision, rounded);
        if(strtod(text, NULL) == rounded)
            break;
    }
    if(strpbrk(text, ".en") == NULL)
        strcat(text, ".0");
    return py_copy_string(text);
}

/* Convert text of one checked scalar type to another, as a cast does. */
const char *py_pointer_ctype(PyEmitter *emitter, const char *type);

/* Strings and slices have a pointer/count native header. Keep casts to that
 * layout attached to the original place; record names and field names do not
 * determine the representation. */
static char *py_sequence_header(PyEmitter *emitter, const char *text,
                                const PyType *source, const PyType *target)
{
    PyType sequence, record, data, length;
    ZirTypeField fields[3];
    size_t offset = 0;
    char first[ZIR_NAME_MAX], second[ZIR_NAME_MAX], name[ZIR_NAME_MAX];
    if(source->kind != PY_POINTER || target->kind != PY_POINTER ||
       !py_classify(emitter->module, source->element, &sequence) ||
       (sequence.kind != PY_STRING && sequence.kind != PY_SLICE) ||
       !py_classify(emitter->module, target->element, &record) ||
       record.kind != PY_RECORD ||
       TypeNextField(record.declared, &offset, &fields[0]) != 1 ||
       TypeNextField(record.declared, &offset, &fields[1]) != 1 ||
       TypeNextField(record.declared, &offset, &fields[2]) != 0 ||
       !py_classify(record.owner, fields[0].type, &data) || data.kind != PY_POINTER ||
       strcmp(data.element, sequence.kind == PY_STRING ? "u8" : sequence.element) ||
       !py_classify(record.owner, fields[1].type, &length) || length.kind != PY_INT ||
       length.bits != 64 || length.is_signed != (sequence.kind == PY_SLICE))
        return NULL;
    py_field_name(fields[0].name, first, sizeof(first));
    py_field_name(fields[1].name, second, sizeof(second));
    py_class_name(&record, name, sizeof(name));
    return py_format("_sequence_header(%s, \"%s\", \"%s\", %s, %s)",
                     text, first, second, name, sequence.kind == PY_STRING ? "True" : "False");
}

char *py_convert(PyEmitter *emitter, const char *text, const char *from,
                        const char *to)
{
    PyType source, target;
    if(strcmp(from, to) == 0 ||
       !py_classify(emitter->module, from, &source) ||
       !py_classify(emitter->module, to, &target))
        return py_copy_string(text);
    {
        char *header = py_sequence_header(emitter, text, &source, &target);
        if(header != NULL) return header;
    }
    if(target.kind == PY_POINTER && strcmp(target.element, "void") &&
       (source.kind == PY_POINTER || source.kind == PY_INT)) {
        const char *ctype = py_pointer_ctype(emitter, to);
        if(ctype != NULL) return py_format("_from_c_pointer(%s, %s)", text, ctype);
    }
    if(target.kind == PY_INT) {
        if(source.kind == PY_INT) {
            if(integer_range_fits(&source, &target) && strcmp(from, "integer") != 0)
                return py_copy_string(text);
            {
                long long value;
                if(text_integer(text, &value) && strcmp(from, "integer") == 0) {
                    int fits = target.is_signed ?
                        (target.bits >= 64 || (value >= -(1LL << (target.bits - 1)) &&
                                               value < (1LL << (target.bits - 1)))) :
                        (value >= 0 && (target.bits >= 64 || value < (1LL << target.bits)));
                    if(fits)
                        return py_copy_string(text);
                }
            }
            return py_wrap(text, target.bits, target.is_signed);
        }
        if(source.kind == PY_BOOL)
            return py_format("int(%s)", text);
        if(source.kind == PY_FLOAT) {
            char *truncated = py_format("int(%s)", text);
            char *wrapped = py_wrap(truncated, target.bits, target.is_signed);
            free(truncated);
            return wrapped;
        }
        return py_copy_string(text);
    }
    if(target.kind == PY_FLOAT) {
        if(source.kind == PY_INT) {
            long long value;
            if(text_integer(text, &value)) {
                if(target.bits == 32)
                    return py_round_float32_literal((double)value);
                return py_format("%lld.0", value);
            }
            if(target.bits == 32)
                return py_format("_f32(%s)", text);
            return py_format("float(%s)", text);
        }
        if(source.kind == PY_FLOAT && target.bits == 32 && source.bits == 64) {
            char *end;
            double value = strtod(text, &end);
            if(*text && *end == '\0')
                return py_round_float32_literal(value);
            return py_format("_f32(%s)", text);
        }
        if(source.kind == PY_BOOL)
            return py_format("float(%s)", text);
        return py_copy_string(text);
    }
    if(target.kind == PY_BOOL && source.kind == PY_INT)
        return py_format("(%s != 0)", text);
    return py_copy_string(text);
}

int expression_is_place(const ZirExpr *expression)
{
    return expression->kind == ZIR_EXPR_IDENT || expression->kind == ZIR_EXPR_MEMBER ||
           expression->kind == ZIR_EXPR_POINTER_MEMBER || expression->kind == ZIR_EXPR_INDEX ||
           expression->kind == ZIR_EXPR_CONDITIONAL ||
           (expression->kind == ZIR_EXPR_UNARY && strcmp(expression->op, "*") == 0);
}

/* A copy of an aggregate value, so the copy and its source stay apart. */
char *py_copy_text(PyEmitter *emitter, const ZirModule *scope,
                          const char *type, const char *text, int depth)
{
    PyType info;
    if(!py_classify(scope, type, &info) || !py_aggregate(info.kind))
        return py_copy_string(text);
    if(info.kind != PY_ARRAY)
        return py_format("%s.copy()", text);
    {
        PyType element;
        char name[32];
        if(!py_classify(scope, info.element, &element) || !py_aggregate(element.kind))
            return py_format("%s.copy()", text);
        snprintf(name, sizeof(name), depth ? "item%d" : "item", depth + 1);
        char *inner = py_copy_text(emitter, scope, info.element, name, depth + 1);
        char *result = py_format("[%s for %s in %s]", inner, name, text);
        free(inner);
        return result;
    }
}

/* The statement that stores value into a place of an aggregate type in
 * place, so pointers and views of the place still see it. */
void py_assign_in_place(PyEmitter *emitter, const ZirModule *scope,
                               const char *type, const char *place, const char *value)
{
    PyType info, element;
    char *bare = py_bare(value);
    py_classify(scope, type, &info);
    if(info.kind == PY_ARRAY) {
        if(py_classify(scope, info.element, &element) && py_aggregate(element.kind))
            write_line(emitter, "_assign_items(%s, %s)", place, bare);
        else
            write_line(emitter, "%s[:] = %s", place, bare);
    } else
        write_line(emitter, "%s.assign(%s)", place, bare);
    free(bare);
}

/* ----- zero values ----- */

char *py_zero(PyEmitter *emitter, const ZirModule *scope, const char *type)
{
    PyType info;
    if(!py_classify(scope, type, &info))
        return py_copy_string("None");
    switch(info.kind) {
    case PY_BOOL: return py_copy_string("False");
    case PY_INT: return py_copy_string("0");
    case PY_FLOAT: return py_copy_string("0.0");
    case PY_STRING: return py_copy_string("b\"\"");
    case PY_SLICE: return py_copy_string("ZiranSlice((), 0, 0)");
    case PY_VEC: return py_copy_string("ZiranVec()");
    case PY_ARRAY: {
        PyType element;
        char *zero = py_zero(emitter, scope, info.element);
        char *result;
        if(info.capacity == 0)
            result = py_copy_string("[]");
        else if(py_classify(scope, info.element, &element) && py_aggregate(element.kind))
            result = py_format("[%s for _ in range(%d)]", zero, info.capacity);
        else
            result = py_format("[%s] * %d", zero, info.capacity);
        free(zero);
        return result;
    }
    case PY_RECORD: {
        char name[ZIR_NAME_MAX];
        size_t offset = 0, used;
        ZirTypeField field;
        char *result;
        py_class_name(&info, name, sizeof(name));
        result = py_format("%s(", name);
        used = strlen(result);
        int first = 1;
        while(TypeNextField(info.declared, &offset, &field) == 1) {
            char *zero = py_zero(emitter, info.owner, field.type);
            char *joined = py_format("%s%s%s", result, first ? "" : ", ", zero);
            free(result);
            free(zero);
            result = joined;
            first = 0;
        }
        (void)used;
        char *closed = py_format("%s)", result);
        free(result);
        return closed;
    }
    case PY_UNION: {
        char name[ZIR_NAME_MAX];
        py_class_name(&info, name, sizeof(name));
        return py_format("%s()", name);
    }
    default:
        return py_copy_string("None");
    }
}

/* ----- expressions ----- */

/* A field reached through using fields is a dotted path (middle.point.x);
 * each step is named in its own record. */
char *py_member_path(const char *path)
{
    char part[ZIR_NAME_MAX];
    char field[ZIR_NAME_MAX];
    char *result = py_copy_string("");
    while(*path) {
        const char *dot = strchr(path, '.');
        size_t length = dot ? (size_t)(dot - path) : strlen(path);
        snprintf(part, sizeof(part), "%.*s", (int)length, path);
        py_field_name(part, field, sizeof(field));
        char *joined = py_format("%s%s%s", result, *result ? "." : "", field);
        free(result);
        result = joined;
        if(dot == NULL)
            break;
        path = dot + 1;
    }
    return result;
}

int is_nonnegative_literal(const char *text)
{
    long long value;
    return text_integer(text, &value) && value >= 0;
}

/* An index into a list or bytes, checked to be non-negative: Python
 * would otherwise count a negative index from the end. */
char *py_index(PyEmitter *emitter, const char *text)
{
    if(is_nonnegative_literal(text) || names_has(&emitter->counters, text))
        return py_copy_string(text);
    return py_format("_index(%s)", text);
}

char *emit_place(PyEmitter *emitter, int index);

const char *base_type_of(PyEmitter *emitter, const ZirExpr *expression)
{
    const ZirExpr *base = expression_at(emitter, expression->left);
    return base != NULL ? base->type : "";
}
