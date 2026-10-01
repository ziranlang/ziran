/*
 * zir_py_lower.c - checked ZIR to Python backend.
 *
 * Python has no fixed-width integers, value records, or raw pointers, so
 * the generated code keeps each Ziran rule explicitly:
 *
 * - Integer arithmetic wraps at its type's width through _s32, _u8, and
 *   friends; division truncates toward zero; shifts check their count.
 * - Records are classes and fixed arrays are lists. Ziran copies them by
 *   value, so a value taken from a place is copied, and an assignment to a
 *   place that a pointer or view may reach overwrites it in place.
 * - A pointer is a ZiranPointer to a list item, an object attribute, or an
 *   object. A local whose address is taken lives in a one-item list.
 * - Strings are immutable bytes; a view of mutable bytes is a snapshot.
 */
#include "zir_py_lower.h"
#include "zir_py_runtime.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"
#include "zir_text.h"
#include "zir_check.h"
#include "zir_parse.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    PY_VOID = 1,
    PY_BOOL,
    PY_INT,
    PY_FLOAT,
    PY_STRING,
    PY_POINTER,
    PY_NULL,
    PY_PROCEDURE,
    PY_SLICE,
    PY_ARRAY,
    PY_VEC,
    PY_RECORD,
    PY_UNION
};

/* A checked type as the Python target sees it. */
typedef struct PyType {
    int kind;
    int bits;          /* integer and float width */
    int is_signed;
    int capacity;      /* fixed array length */
    char element[ZIR_NAME_MAX]; /* array, slice, Vec, or pointer element */
    const ZirModule *owner;     /* module that declares a named type */
    const ZirType *declared;
} PyType;

typedef struct PyLocal {
    char source[ZIR_NAME_MAX];
    char python[ZIR_NAME_MAX];
    int depth;
    int boxed;
} PyLocal;

typedef struct PyNames {
    char (*items)[ZIR_NAME_MAX];
    int count;
    int capacity;
} PyNames;

typedef struct PyEmitter {
    FILE *output;
    const ZirProgram *const *programs;
    int program_count;
    const ZirModule *module;
    const ZirFunction *function;
    PyLocal *locals;
    int local_count;
    int local_capacity;
    PyNames module_names;   /* every top-level Python name */
    PyNames boxed;          /* source locals whose address is taken */
    PyNames in_place;       /* source locals a pointer or view may reach */
    PyNames counters;       /* Python names of loop counters that never go below zero */
    int depth;
    int indent;
    long lines;
    int loops[128];         /* enclosing loop ids, innermost last */
    int loop_count;
    int native_loops[128];
    int native_loop_count;
    int temporaries;
    int uses_host;
} PyEmitter;

static const char *const py_reserved[] = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield", "match", "case", "type",
    /* Builtins and modules the generated code names. */
    "abs", "bool", "bytes", "bytearray", "enumerate", "float", "getattr",
    "int", "isinstance", "len", "list", "print", "range", "setattr", "str",
    "super", "tuple", "object", "id", "hash", "max", "min", "repr", "iter",
    "zip", "format", "self", "sys", "math", "struct", "ctypes", "decimal",
    "host", NULL
};

static void *py_allocate(size_t size)
{
    void *memory = calloc(1, size);
    if(memory == NULL) {
        fprintf(stderr, "zi2py: out of memory\n");
        exit(1);
    }
    return memory;
}

static char *py_format(const char *format, ...)
{
    va_list arguments;
    char *text = NULL;
    va_start(arguments, format);
    if(vasprintf(&text, format, arguments) < 0) {
        fprintf(stderr, "zi2py: out of memory\n");
        exit(1);
    }
    va_end(arguments);
    return text;
}

static char *py_copy_string(const char *text)
{
    return py_format("%s", text);
}

static int identifier_character(int character)
{
    return isalnum((unsigned char)character) || character == '_';
}

/* A Python identifier for a Ziran name. Names that begin with an
 * underscore are left to the runtime, and reserved words get a trailing
 * underscore, as Python code spells class_ and from_. */
static void py_identifier(const char *source, char *output, size_t size)
{
    size_t used = 0;
    if(source == NULL || *source == '\0') {
        snprintf(output, size, "ziran_empty");
        return;
    }
    if(!isalpha((unsigned char)*source)) {
        used = (size_t)snprintf(output, size, "ziran%s", *source == '_' ? "" : "_");
        if(used >= size)
            used = size - 1;
    }
    for(const unsigned char *p = (const unsigned char *)source;
        *p && used + 1 < size; p++)
        output[used++] = identifier_character(*p) ? (char)*p : '_';
    output[used] = '\0';
    for(int index = 0; py_reserved[index] != NULL; index++)
        if(strcmp(output, py_reserved[index]) == 0 && used + 2 < size) {
            output[used++] = '_';
            output[used] = '\0';
            break;
        }
}

static int names_has(const PyNames *names, const char *name)
{
    for(int index = 0; index < names->count; index++)
        if(strcmp(names->items[index], name) == 0)
            return 1;
    return 0;
}

static void names_add(PyNames *names, const char *name)
{
    if(names_has(names, name))
        return;
    if(names->count == names->capacity) {
        int capacity = names->capacity ? names->capacity * 2 : 64;
        void *items = realloc(names->items, (size_t)capacity * sizeof(*names->items));
        if(items == NULL) {
            fprintf(stderr, "zi2py: out of memory\n");
            exit(1);
        }
        names->items = items;
        names->capacity = capacity;
    }
    snprintf(names->items[names->count++], ZIR_NAME_MAX, "%s", name);
}

static void write_line(PyEmitter *emitter, const char *format, ...)
{
    va_list arguments;
    for(int index = 0; index < emitter->indent; index++)
        fputs("    ", emitter->output);
    va_start(arguments, format);
    vfprintf(emitter->output, format, arguments);
    va_end(arguments);
    fputc('\n', emitter->output);
    emitter->lines++;
}

/* Parenthesized text without its one enclosing pair, for a whole statement. */
static char *py_bare(const char *text)
{
    size_t length = strlen(text);
    int depth = 0, whole = length >= 2 && text[0] == '(' && text[length - 1] == ')';
    char quote = 0;
    for(size_t index = 0; whole && index < length; index++) {
        char c = text[index];
        if(quote) {
            if(c == '\\') index++;
            else if(c == quote) quote = 0;
            continue;
        }
        if(c == '"' || c == '\'') quote = c;
        else if(c == '(' || c == '[') depth++;
        else if((c == ')' || c == ']') && --depth == 0 && index + 1 < length) whole = 0;
    }
    if(whole)
        return py_format("%.*s", (int)(length - 2), text + 1);
    return py_copy_string(text);
}

/* ----- types ----- */

static int py_integer_width(const char *type, int *bits, int *is_signed)
{
    static const struct { const char *name; int bits, is_signed; } integers[] = {
        {"s8", 8, 1}, {"s16", 16, 1}, {"s32", 32, 1}, {"s64", 64, 1},
        {"u8", 8, 0}, {"u16", 16, 0}, {"u32", 32, 0}, {"u64", 64, 0},
        {"isize", 64, 1}, {"usize", 64, 0}, {"integer", 64, 1}, {"int", 64, 1},
        {"char", 8, 0}, {NULL, 0, 0}
    };
    for(int index = 0; integers[index].name != NULL; index++)
        if(strcmp(type, integers[index].name) == 0) {
            *bits = integers[index].bits;
            *is_signed = integers[index].is_signed;
            return 1;
        }
    return 0;
}

static int py_classify(const ZirModule *scope, const char *type, PyType *out)
{
    const ZirModule *owner = NULL;
    const ZirType *declared;
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    memset(out, 0, sizeof(*out));
    if(type == NULL || *type == '\0' || strcmp(type, "void") == 0) {
        out->kind = PY_VOID;
        return 1;
    }
    if(strcmp(type, "bool") == 0) {
        out->kind = PY_BOOL;
        return 1;
    }
    if(py_integer_width(type, &out->bits, &out->is_signed)) {
        out->kind = PY_INT;
        return 1;
    }
    if(strcmp(type, "float32") == 0 || strcmp(type, "float64") == 0 ||
       strcmp(type, "real") == 0) {
        out->kind = PY_FLOAT;
        out->bits = strcmp(type, "float32") == 0 ? 32 : 64;
        return 1;
    }
    if(strcmp(type, "string") == 0) {
        out->kind = PY_STRING;
        return 1;
    }
    if(strcmp(type, "null") == 0) {
        out->kind = PY_NULL;
        return 1;
    }
    if(type[0] == '*' && type[1] != '\0') {
        out->kind = PY_POINTER;
        snprintf(out->element, sizeof(out->element), "%s", type + 1);
        return 1;
    }
    if(SliceElementType(type, element, sizeof(element))) {
        out->kind = PY_SLICE;
        snprintf(out->element, sizeof(out->element), "%s", element);
        return 1;
    }
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        out->kind = PY_ARRAY;
        out->capacity = capacity;
        snprintf(out->element, sizeof(out->element), "%s", element);
        return 1;
    }
    declared = scope != NULL ? FindType(scope, type, &owner) : NULL;
    if(declared == NULL)
        return 0;
    out->owner = owner != NULL ? owner : scope;
    out->declared = declared;
    if(declared->is_enum) {
        const char *backing = declared->enum_backing[0] ? declared->enum_backing : "s64";
        if(!py_integer_width(backing, &out->bits, &out->is_signed))
            return 0;
        out->kind = PY_INT;
        return 1;
    }
    if(declared->is_procedure_type) {
        out->kind = PY_PROCEDURE;
        return 1;
    }
    if(declared->is_owned_vec || VecElementType(scope, type, element, sizeof(element))) {
        ZirTypeField field;
        size_t offset = 0;
        out->kind = PY_VEC;
        if(TypeNextField(declared, &offset, &field) == 1 && field.type[0] == '*')
            snprintf(out->element, sizeof(out->element), "%s", field.type + 1);
        else if(VecElementType(scope, type, element, sizeof(element)))
            snprintf(out->element, sizeof(out->element), "%s", element);
        return 1;
    }
    if(declared->is_extern) {
        /* A host-owned record crosses only as an opaque handle. */
        out->kind = PY_POINTER;
        snprintf(out->element, sizeof(out->element), "void");
        return 1;
    }
    if(declared->is_record_template || declared->is_type_instance)
        return 0;
    out->kind = declared->is_union ? PY_UNION : PY_RECORD;
    return 1;
}

static int py_aggregate(int kind)
{
    return kind == PY_RECORD || kind == PY_UNION || kind == PY_ARRAY || kind == PY_VEC;
}

static void py_require_type(const ZirModule *scope, ZirSourceSpan span,
                            const char *type, PyType *out)
{
    if(!py_classify(scope, type, out)) {
        Diagnostic(span, "zir_py.type", "the Python target cannot lower this type: %s", type);
        exit(1);
    }
}

/* A class name. A record the checker made from a type call, like
 * Option(s32), has only a hashed name; Option records read as Option_s32. */
static void py_class_name(const PyType *info, char *output, size_t size)
{
    char native[ZIR_NAME_MAX];
    ZirTypeField fields[3];
    size_t offset = 0;
    if(strncmp(info->declared->name, "__type_", 7) == 0 &&
       TypeNextField(info->declared, &offset, &fields[0]) == 1 &&
       TypeNextField(info->declared, &offset, &fields[1]) == 1 &&
       TypeNextField(info->declared, &offset, &fields[2]) == 0 &&
       !strcmp(fields[0].name, "has_value") && !strcmp(fields[1].name, "value")) {
        snprintf(native, sizeof(native), "Option_%s", fields[1].type);
        for(char *c = native; *c; c++)
            if(!identifier_character(*c))
                *c = '_';
    } else if(strncmp(info->declared->name, "__type_", 7) == 0)
        snprintf(native, sizeof(native), "Type_%s", info->declared->name + 7);
    else
        NativeTypeName(info->owner, info->declared, native, sizeof(native));
    py_identifier(native, output, size);
}

static const char *py_wrap_name(int bits, int is_signed)
{
    switch(bits) {
    case 8: return is_signed ? "_s8" : "_u8";
    case 16: return is_signed ? "_s16" : "_u16";
    case 32: return is_signed ? "_s32" : "_u32";
    default: return is_signed ? "_s64" : "_u64";
    }
}

/* ----- names ----- */

/* Record classes define copy and assign, so fields by those names move. */
static void py_field_name(const char *source, char *output, size_t size)
{
    py_identifier(source, output, size);
    if((!strcmp(output, "copy") || !strcmp(output, "assign")) && strlen(output) + 2 < size)
        strcat(output, "_");
}

static void function_symbol(PyEmitter *emitter, const ZirModule *module,
                            const ZirFunction *function, char *output, size_t size)
{
    char native[ZIR_NAME_MAX * 2];
    if(function->export_symbol[0]) {
        py_identifier(function->export_symbol, output, size);
        return;
    }
    if(function->exported && strcmp(function->name, "main") != 0) {
        py_identifier(function->name, output, size);
        return;
    }
    NativeGoFunctionName(emitter->programs, emitter->program_count, module,
                         function, native, sizeof(native));
    py_identifier(native, output, size);
}

static void module_guard(PyEmitter *emitter, const ZirModule *module,
                         char *output, size_t size)
{
    char file_stem[1024];
    char guard[1024];
    NativeGoModuleIdentity(emitter->programs, emitter->program_count, module,
                           file_stem, sizeof(file_stem), guard, sizeof(guard));
    py_identifier(guard, output, size);
}

static void global_symbol(PyEmitter *emitter, const ZirModule *module,
                          const ZirGlobal *global, char *output, size_t size)
{
    char guard[ZIR_NAME_MAX];
    char name[ZIR_NAME_MAX];
    char joined[ZIR_NAME_MAX * 2 + 2];
    module_guard(emitter, module, guard, sizeof(guard));
    snprintf(name, sizeof(name), "%s", global->name);
    for(char *c = name; *c; c++)
        if(!identifier_character(*c)) *c = '_';
    snprintf(joined, sizeof(joined), "%s_%s", guard, name);
    py_identifier(joined, output, size);
}

static void foreign_symbol(PyEmitter *emitter, const ZirModule *module,
                           const ZirImport *import, char *output, size_t size)
{
    char guard[ZIR_NAME_MAX];
    char joined[ZIR_NAME_MAX * 2 + 2];
    module_guard(emitter, module, guard, sizeof(guard));
    snprintf(joined, sizeof(joined), "%s_%s", guard, import->name);
    py_identifier(joined, output, size);
}

/* ----- locals ----- */

static PyLocal *find_local(PyEmitter *emitter, const char *source)
{
    for(int index = emitter->local_count - 1; index >= 0; index--)
        if(strcmp(emitter->locals[index].source, source) == 0)
            return &emitter->locals[index];
    return NULL;
}

/* A Python name unused by live locals and every top-level name. */
static PyLocal *declare_local(PyEmitter *emitter, const char *source)
{
    char base[ZIR_NAME_MAX];
    char candidate[ZIR_NAME_MAX];
    PyLocal *local;
    py_identifier(source, base, sizeof(base));
    snprintf(candidate, sizeof(candidate), "%s", base);
    for(int serial = 2;; serial++) {
        int taken = names_has(&emitter->module_names, candidate);
        for(int index = 0; index < emitter->local_count && !taken; index++)
            taken = strcmp(emitter->locals[index].python, candidate) == 0;
        if(!taken)
            break;
        snprintf(candidate, sizeof(candidate), "%s_%d", base, serial);
    }
    if(emitter->local_count == emitter->local_capacity) {
        int capacity = emitter->local_capacity ? emitter->local_capacity * 2 : 64;
        PyLocal *locals = realloc(emitter->locals, (size_t)capacity * sizeof(*locals));
        if(locals == NULL) {
            fprintf(stderr, "zi2py: out of memory\n");
            exit(1);
        }
        emitter->locals = locals;
        emitter->local_capacity = capacity;
    }
    local = &emitter->locals[emitter->local_count++];
    memset(local, 0, sizeof(*local));
    snprintf(local->source, sizeof(local->source), "%s", source);
    snprintf(local->python, sizeof(local->python), "%s", candidate);
    local->depth = emitter->depth;
    local->boxed = names_has(&emitter->boxed, source);
    return local;
}

static void leave_scope(PyEmitter *emitter)
{
    while(emitter->local_count > 0 &&
          emitter->locals[emitter->local_count - 1].depth >= emitter->depth)
        emitter->local_count--;
    emitter->depth--;
}

static int global_reference(PyEmitter *emitter, const char *name,
                            char *output, size_t size, const ZirGlobal **found)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    if(find_local(emitter, name) != NULL ||
       ResolveGlobalAt(emitter->module, name, SpanPath(emitter->function->span),
                       &owner, &global) != 1 || owner == NULL)
        return 0;
    global_symbol(emitter, owner, global, output, size);
    if(found != NULL)
        *found = global;
    return 1;
}

static const ZirImport *py_foreign_import(const ZirModule *module, const char *name)
{
    for(int index = 0; index < module->import_count; index++) {
        const ZirImport *import = &module->imports[index];
        if(import->kind == ZIR_IMPORT_EXTERN &&
           (import->extern_kind == ZIR_EXTERN_C || import->extern_kind == ZIR_EXTERN_HOST ||
            import->extern_kind == ZIR_EXTERN_PY) &&
           strcmp(import->name, name) == 0)
            return import;
    }
    return NULL;
}

/* ----- values ----- */

static char *emit_expression(PyEmitter *emitter, int index);
static char *emit_value(PyEmitter *emitter, int index, const char *type);
static char *py_zero(PyEmitter *emitter, const ZirModule *scope, const char *type);

static const ZirExpr *expression_at(PyEmitter *emitter, int index)
{
    if(index < 0 || index >= emitter->function->expr_count)
        return NULL;
    return &emitter->function->exprs[index];
}

static void unsupported_expression(const ZirExpr *expression)
{
    Diagnostic(expression->span, "zir_py.expression",
            "unsupported expression in the Python target: %s",
            expression->text && expression->text[0] ? expression->text :
                ExprKindName(expression->kind));
    exit(1);
}

static int py_integer_literal(const char *text, long long *value)
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
static char *py_wrapped_literal(unsigned long long bits, int width, int is_signed)
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
static int text_integer(const char *text, long long *value)
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
static char *py_wrap(const char *text, int bits, int is_signed)
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

static char *py_round_float32_literal(double value)
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
static const char *py_pointer_ctype(PyEmitter *emitter, const char *type);

static char *py_convert(PyEmitter *emitter, const char *text, const char *from,
                        const char *to)
{
    PyType source, target;
    if(strcmp(from, to) == 0 ||
       !py_classify(emitter->module, from, &source) ||
       !py_classify(emitter->module, to, &target))
        return py_copy_string(text);
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

static int expression_is_place(const ZirExpr *expression)
{
    return expression->kind == ZIR_EXPR_IDENT || expression->kind == ZIR_EXPR_MEMBER ||
           expression->kind == ZIR_EXPR_POINTER_MEMBER || expression->kind == ZIR_EXPR_INDEX ||
           expression->kind == ZIR_EXPR_CONDITIONAL ||
           (expression->kind == ZIR_EXPR_UNARY && strcmp(expression->op, "*") == 0);
}

/* A copy of an aggregate value, so the copy and its source stay apart. */
static char *py_copy_text(PyEmitter *emitter, const ZirModule *scope,
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
static void py_assign_in_place(PyEmitter *emitter, const ZirModule *scope,
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

static char *py_zero(PyEmitter *emitter, const ZirModule *scope, const char *type)
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
static char *py_member_path(const char *path)
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

static int is_nonnegative_literal(const char *text)
{
    long long value;
    return text_integer(text, &value) && value >= 0;
}

/* An index into a list or bytes, checked to be non-negative: Python
 * would otherwise count a negative index from the end. */
static char *py_index(PyEmitter *emitter, const char *text)
{
    if(is_nonnegative_literal(text) || names_has(&emitter->counters, text))
        return py_copy_string(text);
    return py_format("_index(%s)", text);
}

static char *emit_place(PyEmitter *emitter, int index);

static const char *base_type_of(PyEmitter *emitter, const ZirExpr *expression)
{
    const ZirExpr *base = expression_at(emitter, expression->left);
    return base != NULL ? base->type : "";
}

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
static char *emit_place(PyEmitter *emitter, int index)
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

static char *py_string_literal(const unsigned char *bytes, size_t length)
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

static int expression_calls(const ZirFunction *function, int index)
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
static const char *py_ctype(PyEmitter *emitter, const char *type)
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
static const char *py_pointer_ctype(PyEmitter *emitter, const char *type)
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
static int foreign_parameters(const ZirImport *import, char types[][ZIR_NAME_MAX], int maximum)
{
    /* On the heap: this frame is inlined into callers that are inlined too. */
    char *parts = py_allocate(32 * 256);
    int count = *import->args ? split_top_level(import->args, parts, 32, 256) : 0;
    int used = 0;
    for(int index = 0; index < count && used < maximum; index++) {
        char *part = parts + index * 256;
        char *colon = strchr(part, ':');
        char *type = colon != NULL ? colon + 1 : part;
        while(*type == ' ' || *type == '\t')
            type++;
        size_t length = strlen(type);
        while(length > 0 && isspace((unsigned char)type[length - 1]))
            type[--length] = '\0';
        if(strncmp(type, "..", 2) == 0)
            break;
        snprintf(types[used++], ZIR_NAME_MAX, "%s", type);
    }
    free(parts);
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
static void py_module_symbol(const char *path, char *output, size_t size)
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
static int function_parameter_names(const ZirFunction *function,
                                    char names[][ZIR_NAME_MAX],
                                    char types[][ZIR_NAME_MAX], int maximum)
{
    char *parts = py_allocate(64 * 512);
    int count = *FunctionArgs(function) ? split_top_level(FunctionArgs(function), parts, 64, 512) : 0;
    int used = 0;
    for(int index = 0; index < count && used < maximum; index++) {
        char *part = parts + index * 512;
        char *colon = strchr(part, ':');
        const char *start = part;
        size_t length;
        if(colon == NULL)
            continue;
        while(*start == ' ' || *start == '\t' || *start == '\n')
            start++;
        length = (size_t)(colon - start);
        while(length > 0 && isspace((unsigned char)start[length - 1]))
            length--;
        if(strncmp(start, "using ", 6) == 0) {
            start += 6;
            length -= 6;
        }
        snprintf(names[used], ZIR_NAME_MAX, "%.*s", (int)length, start);
        {
            char *type = colon + 1;
            while(*type == ' ' || *type == '\t')
                type++;
            size_t type_length = strlen(type);
            while(type_length > 0 && isspace((unsigned char)type[type_length - 1]))
                type_length--;
            char *equals = memchr(type, '=', type_length);
            if(equals != NULL) {
                type_length = (size_t)(equals - type);
                while(type_length > 0 && isspace((unsigned char)type[type_length - 1]))
                    type_length--;
            }
            snprintf(types[used], ZIR_NAME_MAX, "%.*s", (int)type_length, type);
        }
        used++;
    }
    free(parts);
    return used;
}

static const char *place_base(const ZirFunction *function, int index, int *through_member);

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

static char *emit_expression(PyEmitter *emitter, int index)
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
static char *emit_value(PyEmitter *emitter, int index, const char *type)
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

static int block_end(const ZirFunction *function, int begin, int end)
{
    int depth = 1;
    for(int index = begin + 1; index < end; index++) {
        ZirStmtKind kind = function->stmts[index].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE || kind == ZIR_STMT_BLOCK_OPEN)
            depth++;
        else if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0)
            return index;
    }
    return end;
}

static void emit_sequence(PyEmitter *emitter, int begin, int end);

/* A block body, with pass when it writes nothing. */
static void emit_body(PyEmitter *emitter, int begin, int end)
{
    long before = emitter->lines;
    emitter->indent++;
    emitter->depth++;
    emit_sequence(emitter, begin, end);
    leave_scope(emitter);
    if(emitter->lines == before)
        write_line(emitter, "pass");
    emitter->indent--;
}

/* An if condition as an operand of and. */
static char *emit_condition_operand(PyEmitter *emitter, int index)
{
    const ZirExpr *condition = expression_at(emitter, index);
    char *text = emit_expression(emitter, index);
    if(condition != NULL && condition->kind == ZIR_EXPR_BINARY && !strcmp(condition->op, "||"))
        return text;
    char *bare = py_bare(text);
    free(text);
    return bare;
}

static int has_else(PyEmitter *emitter, int close, int end)
{
    return close + 1 < end && emitter->function->stmts[close + 1].kind == ZIR_STMT_IF &&
           emitter->function->stmts[close + 1].is_else;
}

static int emit_if(PyEmitter *emitter, int index, int end, int is_elif)
{
    const ZirStmt *statement = &emitter->function->stmts[index];
    int close = block_end(emitter->function, index, end);
    int body = index + 1;
    char *condition = emit_condition_operand(emitter, statement->expr_root);
    /* if a { if b { ... } } with no else on either is if a and b: Python
     * allows only 100 nested blocks, and the flat form reads better. */
    while(!has_else(emitter, close, end) && body < close &&
          emitter->function->stmts[body].kind == ZIR_STMT_IF &&
          !emitter->function->stmts[body].is_else &&
          block_end(emitter->function, body, close) == close - 1) {
        char *inner = emit_condition_operand(emitter, emitter->function->stmts[body].expr_root);
        char *joined = py_format("%s and %s", condition, inner);
        free(condition);
        free(inner);
        condition = joined;
        body++;
        close--;
    }
    if(body == index + 1) {
        char *bare = py_bare(condition);
        free(condition);
        condition = bare;
    }
    write_line(emitter, "%s %s:", is_elif ? "elif" : "if", condition);
    free(condition);
    emit_body(emitter, body, close);
    /* The closes of merged inner ifs come before the outer one. */
    close = block_end(emitter->function, index, end);
    if(close + 1 < end && emitter->function->stmts[close + 1].kind == ZIR_STMT_IF &&
       emitter->function->stmts[close + 1].is_else) {
        int next = close + 1;
        if(emitter->function->stmts[next].expr_root >= 0)
            close = emit_if(emitter, next, end, 1);
        else {
            close = block_end(emitter->function, next, end);
            write_line(emitter, "else:");
            emit_body(emitter, next + 1, close);
        }
    }
    return close;
}

static int loop_is_native(PyEmitter *emitter, int loop_id)
{
    for(int index = 0; index < emitter->native_loop_count; index++)
        if(emitter->native_loops[index] == loop_id)
            return 1;
    return 0;
}

static int innermost_loop(PyEmitter *emitter)
{
    return emitter->loop_count > 0 ? emitter->loops[emitter->loop_count - 1] : 0;
}

/* The far jumps a loop body makes: to loops outside it. */
static void loop_exits(PyEmitter *emitter, int open, int close, int loop_id,
                       int *breaks_parent, int *continues_parent, int *farther, int parent)
{
    const ZirFunction *function = emitter->function;
    *breaks_parent = *continues_parent = *farther = 0;
    for(int index = open + 1; index < close; index++) {
        const ZirStmt *statement = &function->stmts[index];
        int inside = 0;
        if((statement->kind != ZIR_STMT_BREAK && statement->kind != ZIR_STMT_CONTINUE) ||
           !statement->target_id || statement->target_id == loop_id)
            continue;
        for(int scan = open + 1; scan < close; scan++)
            if(function->stmts[scan].kind == ZIR_STMT_WHILE &&
               function->stmts[scan].loop_id == statement->target_id)
                inside = 1;
        if(inside)
            continue;
        if(statement->target_id == parent) {
            if(statement->kind == ZIR_STMT_BREAK)
                *breaks_parent = 1;
            else
                *continues_parent = 1;
        } else
            *farther = 1;
    }
}

static void emit_loop_body(PyEmitter *emitter, int loop_id, int begin, int end)
{
    emitter->loops[emitter->loop_count++] = loop_id;
    emit_body(emitter, begin, end);
    emitter->loop_count--;
}

/* After an inner loop, a jump it made to an outer loop goes on. */
static void emit_loop_exits(PyEmitter *emitter, int open, int close, int loop_id)
{
    int breaks_parent, continues_parent, farther;
    int parent = innermost_loop(emitter);
    if(emitter->loop_count == 0)
        return;
    loop_exits(emitter, open, close, loop_id, &breaks_parent, &continues_parent,
               &farther, parent);
    if(breaks_parent) {
        write_line(emitter, "if ziran_jump == -%d:", parent);
        emitter->indent++;
        write_line(emitter, "ziran_jump = 0");
        write_line(emitter, "break");
        emitter->indent--;
    }
    if(continues_parent) {
        write_line(emitter, "if ziran_jump == %d:", parent);
        emitter->indent++;
        write_line(emitter, "ziran_jump = 0");
        write_line(emitter, "continue");
        emitter->indent--;
    }
    if(farther) {
        write_line(emitter, "if ziran_jump:");
        emitter->indent++;
        write_line(emitter, "break");
        emitter->indent--;
    }
}

/* A lowered counting loop, { i: s64 = 0; while i < n { ...; i += 1 } },
 * is for i in range(0, n) in Python. The range steps, so the marked step
 * statements go. */
static int emit_native_for(PyEmitter *emitter, int open, int close)
{
    const ZirFunction *function = emitter->function;
    const ZirStmt *counter, *loop;
    const ZirExpr *condition;
    int loop_close, reverse = 0;
    char *start, *bound, *range;
    PyLocal *local;
    if(open + 2 >= close || emitter->native_loop_count >= 128)
        return 0;
    counter = &function->stmts[open + 1];
    loop = &function->stmts[open + 2];
    if(counter->kind != ZIR_STMT_DECL || counter->expr_root < 0 ||
       loop->kind != ZIR_STMT_WHILE || !loop->for_form || loop->is_parallel ||
       loop->expr_root < 0 || names_has(&emitter->boxed, counter->name))
        return 0;
    loop_close = block_end(function, open + 2, close);
    if(loop_close + 1 != close)
        return 0;
    condition = &function->exprs[loop->expr_root];
    if(condition->kind != ZIR_EXPR_BINARY ||
       function->exprs[condition->left].kind != ZIR_EXPR_IDENT ||
       strcmp(function->exprs[condition->left].name, counter->name) ||
       (strcmp(condition->op, "<=") && strcmp(condition->op, "<") &&
        strcmp(condition->op, ">=")))
        return 0;
    for(int s = open + 3; s < loop_close; s++)
        if(function->stmts[s].for_step == loop->loop_id) {
            reverse = !strcmp(function->stmts[s].assignment_op, "-=");
            break;
        }
    if(reverse != !strcmp(condition->op, ">="))
        return 0;
    emitter->depth++;
    start = emit_value(emitter, counter->expr_root, counter->type);
    bound = emit_value(emitter, condition->right, counter->type);
    {
        char *bare_start = py_bare(start), *bare_bound = py_bare(bound);
        free(start);
        free(bound);
        start = bare_start;
        bound = bare_bound;
    }
    local = declare_local(emitter, counter->name);
    if(reverse) {
        long long value;
        char *stop = text_integer(bound, &value) ? py_format("%lld", value - 1) :
                                                   py_format("%s - 1", bound);
        range = py_format("range(%s, %s, -1)", start, stop);
        free(stop);
    } else if(!strcmp(condition->op, "<=")) {
        long long value;
        char *stop = text_integer(bound, &value) ? py_format("%lld", value + 1) :
                                                   py_format("%s + 1", bound);
        if(!strcmp(start, "0"))
            range = py_format("range(%s)", stop);
        else
            range = py_format("range(%s, %s)", start, stop);
        free(stop);
    } else if(!strcmp(start, "0"))
        range = py_format("range(%s)", bound);
    else
        range = py_format("range(%s, %s)", start, bound);
    write_line(emitter, "for %s in %s:", local->python, range);
    if(!reverse && (is_nonnegative_literal(start) ||
                    names_has(&emitter->counters, start)))
        names_add(&emitter->counters, local->python);
    free(start);
    free(bound);
    free(range);
    emitter->native_loops[emitter->native_loop_count++] = loop->loop_id;
    emit_loop_body(emitter, loop->loop_id, open + 3, loop_close);
    emitter->native_loop_count--;
    leave_scope(emitter);
    emit_loop_exits(emitter, open + 2, loop_close, loop->loop_id);
    return 1;
}

/* A place the assignment writes, with its index read once. */
static char *emit_assignment_place(PyEmitter *emitter, int index, int compound)
{
    const ZirExpr *expression = expression_at(emitter, index);
    if(compound && expression != NULL && expression->kind == ZIR_EXPR_INDEX &&
       expression_calls(emitter->function, expression->right)) {
        /* x[f()] += v evaluates f once. */
        char temporary[64];
        PyType base;
        char *object = emit_place(emitter, expression->left);
        char *value = emit_expression(emitter, expression->right);
        char *bare = py_bare(value);
        snprintf(temporary, sizeof(temporary), "ziran_index_%d", ++emitter->temporaries);
        write_line(emitter, "%s = %s", temporary, bare);
        free(value);
        free(bare);
        py_classify(emitter->module, base_type_of(emitter, expression), &base);
        char *result = base.kind == PY_ARRAY || base.kind == PY_STRING ?
            py_format("%s[_index(%s)]", object, temporary) :
            py_format("%s[%s]", object, temporary);
        free(object);
        return result;
    }
    return emit_place(emitter, index);
}

static int local_in_place(PyEmitter *emitter, const char *name)
{
    return names_has(&emitter->in_place, name);
}

static void emit_assignment(PyEmitter *emitter, const ZirStmt *statement)
{
    const ZirExpr *target = expression_at(emitter, statement->lhs_root);
    PyType info;
    char *place, *value;
    if(target == NULL) {
        Diagnostic(statement->span, "zir_py.statement", "assignment has no destination");
        exit(1);
    }
    py_classify(emitter->module, target->type, &info);
    if(strcmp(statement->assignment_op, "=") == 0) {
        place = emit_assignment_place(emitter, statement->lhs_root, 0);
        value = emit_value(emitter, statement->expr_root, target->type);
        if(py_aggregate(info.kind) &&
           (target->kind != ZIR_EXPR_IDENT || find_local(emitter, target->name) == NULL ||
            local_in_place(emitter, target->name))) {
            /* Overwrite the place itself: views and pointers see it. */
            char *source = emit_expression(emitter, statement->expr_root);
            py_assign_in_place(emitter, emitter->module, target->type, place, source);
            free(source);
        } else {
            char *bare = py_bare(value);
            write_line(emitter, "%s = %s", place, bare);
            free(bare);
        }
        free(place);
        free(value);
        return;
    }
    {
        char operation[4];
        snprintf(operation, sizeof(operation), "%s", statement->assignment_op);
        operation[strlen(operation) - 1] = '\0';
        place = emit_assignment_place(emitter, statement->lhs_root, 1);
        value = emit_value(emitter, statement->expr_root,
                           !strcmp(operation, "<<") || !strcmp(operation, ">>") ?
                           expression_at(emitter, statement->expr_root)->type : target->type);
        char *bare = py_bare(value);
        char *combined;
        if(info.kind == PY_INT) {
            if(!strcmp(operation, "+") || !strcmp(operation, "-") || !strcmp(operation, "*")) {
                char *sum = py_format("%s %s %s", place, operation, value);
                combined = py_wrap(sum, info.bits, info.is_signed);
                free(sum);
            } else if((!strcmp(operation, "/") || !strcmp(operation, "%")) && !info.is_signed) {
                write_line(emitter, "%s %s= %s", place, !strcmp(operation, "/") ? "//" : "%",
                           bare);
                free(place);
                free(value);
                free(bare);
                return;
            } else if(!strcmp(operation, "/")) {
                char *quotient = py_format("_div(%s, %s)", place, bare);
                combined = info.is_signed ? py_wrap(quotient, info.bits, 1) :
                                            py_copy_string(quotient);
                free(quotient);
            } else if(!strcmp(operation, "%"))
                combined = py_format("_rem(%s, %s)", place, bare);
            else if(!strcmp(operation, "<<")) {
                char *shifted = py_format("_shl(%s, %s, %d)", place, bare, info.bits);
                combined = py_wrap(shifted, info.bits, info.is_signed);
                free(shifted);
            } else if(!strcmp(operation, ">>"))
                combined = py_format("_shr(%s, %s, %d)", place, bare, info.bits);
            else if(!strcmp(operation, "&") || !strcmp(operation, "|") ||
                    !strcmp(operation, "^")) {
                write_line(emitter, "%s %s= %s", place, operation, bare);
                free(place);
                free(value);
                free(bare);
                return;
            } else {
                Diagnostic(statement->span, "zir_py.assignment",
                        "unsupported assignment operation in the Python target: %s",
                        statement->assignment_op);
                exit(1);
                combined = NULL;
            }
        } else if(info.kind == PY_FLOAT) {
            if(!strcmp(operation, "/"))
                combined = py_format("_fdiv(%s, %s)", place, bare);
            else if(!strcmp(operation, "%"))
                combined = py_format("_frem(%s, %s)", place, bare);
            else if(info.bits == 64) {
                write_line(emitter, "%s %s= %s", place, operation, bare);
                free(place);
                free(value);
                free(bare);
                return;
            } else
                combined = py_format("%s %s %s", place, operation, value);
            if(info.bits == 32) {
                char *rounded = py_format("_f32(%s)", combined);
                free(combined);
                combined = rounded;
            }
        } else if(info.kind == PY_BOOL) {
            write_line(emitter, "%s = %s %s %s", place, place,
                       !strcmp(operation, "^") ? "!=" : operation, value);
            free(place);
            free(value);
            free(bare);
            return;
        } else {
            Diagnostic(statement->span, "zir_py.assignment",
                    "unsupported assignment operation in the Python target: %s",
                    statement->assignment_op);
            exit(1);
            combined = NULL;
        }
        char *bare_combined = py_bare(combined);
        write_line(emitter, "%s = %s", place, bare_combined);
        free(bare_combined);
        free(combined);
        free(bare);
    }
    free(place);
    free(value);
}

static void emit_print(PyEmitter *emitter, const ZirExpr *expression)
{
    PrintPiece *pieces = py_allocate(PRINT_PIECES_MAX * sizeof(*pieces));
    unsigned char *bytes = py_allocate(ZIR_TEXT_MAX);
    const ZirFunction *function = emitter->function;
    int first = expression->first_child, count, argument = -1, arguments = 0;
    char *format = py_copy_string("");
    char *values = py_copy_string("");
    if(first < 0 || (count = PrintFormatPieces(function->exprs[first].text, pieces,
                                               PRINT_PIECES_MAX)) < 0)
        unsupported_expression(expression);
    argument = function->exprs[first].next_sibling;
    for(int piece = 0; piece < count; piece++) {
        size_t length;
        const unsigned char *text = bytes;
        char *literal, *joined;
        if(!pieces[piece].is_argument) {
            if(!DecodeStringLiteral(pieces[piece].literal, bytes, ZIR_TEXT_MAX, &length))
                unsupported_expression(expression);
        } else {
            const ZirExpr *value;
            PyType info;
            char *text_value, *bare, *spelled;
            const char *placeholder = "%s";
            if(argument < 0)
                break;
            value = &function->exprs[argument];
            if(value->kind == ZIR_EXPR_STRING &&
               DecodeStringLiteral(value->text, bytes, ZIR_TEXT_MAX, &length)) {
                argument = value->next_sibling;
                goto literal_text;
            }
            py_classify(emitter->module, value->type, &info);
            text_value = emit_expression(emitter, argument);
            bare = py_bare(text_value);
            if(info.kind == PY_INT) {
                placeholder = "%d";
                spelled = py_copy_string(bare);
            } else if(info.kind == PY_BOOL)
                spelled = py_format("b\"true\" if %s else b\"false\"", bare);
            else if(info.kind == PY_FLOAT)
                spelled = info.bits == 32 ? py_format("_float_text(%s, True)", bare) :
                                            py_format("_float_text(%s)", bare);
            else
                spelled = py_copy_string(bare);
            joined = py_format("%s%s", format, placeholder);
            free(format);
            format = joined;
            joined = py_format("%s%s%s", values, arguments ? ", " : "", spelled);
            free(values);
            values = joined;
            arguments++;
            free(text_value);
            free(bare);
            free(spelled);
            argument = value->next_sibling;
            continue;
        }
literal_text:
        {
            /* Percent signs in the text are doubled for % formatting. */
            unsigned char *escaped = py_allocate(length * 2 + 1);
            size_t used = 0;
            for(size_t index = 0; index < length; index++) {
                escaped[used++] = text[index];
                if(text[index] == '%')
                    escaped[used++] = '%';
            }
            literal = py_string_literal(escaped, used);
            free(escaped);
            /* Drop b" and the closing quote to join the text. */
            literal[strlen(literal) - 1] = '\0';
            joined = py_format("%s%s", format, literal + 2);
            free(format);
            format = joined;
            free(literal);
        }
    }
    if(arguments == 0) {
        /* Without values the text needs no % formatting. */
        char *plain = py_allocate(strlen(format) + 1);
        size_t used = 0;
        for(const char *c = format; *c; c++) {
            if(c[0] == '%' && c[1] == '%')
                c++;
            plain[used++] = *c;
        }
        plain[used] = '\0';
        write_line(emitter, "sys.stdout.buffer.write(b\"%s\")", plain);
        free(plain);
    } else if(arguments == 1)
        write_line(emitter, "sys.stdout.buffer.write(b\"%s\" %% %s)", format,
                   strchr(values, ' ') != NULL && strncmp(values, "_float_text", 11) ?
                       py_format("(%s)", values) : values);
    else
        write_line(emitter, "sys.stdout.buffer.write(b\"%s\" %% (%s))", format, values);
    free(format);
    free(values);
    free(pieces);
    free(bytes);
}

static void emit_sequence(PyEmitter *emitter, int begin, int end)
{
    for(int index = begin; index < end; index++) {
        const ZirStmt *statement = &emitter->function->stmts[index];
        if(statement->kind == ZIR_STMT_ASSIGN && statement->for_step &&
           loop_is_native(emitter, statement->for_step))
            continue;
        /* Comments carry source locations without changing generated code
         * or the emitter's statement/empty-block accounting. */
        char *source = py_string_literal((const unsigned char *)SpanPath(statement->span),
                                        strlen(SpanPath(statement->span)));
        for(int indent = 0; indent < emitter->indent; indent++) fputs("    ", emitter->output);
        fprintf(emitter->output, "# ziran-source (%s, %d)\n", source, statement->span.line);
        free(source);
        switch(statement->kind) {
        case ZIR_STMT_DECL: {
            PyType info;
            char *value;
            PyLocal *local;
            py_require_type(emitter->module, statement->span, statement->type, &info);
            /* The value is lowered before the name is bound, so an
             * initializer can read a shadowed outer binding. */
            if(statement->expr_root >= 0)
                value = emit_value(emitter, statement->expr_root, statement->type);
            else
                value = py_zero(emitter, emitter->module, statement->type);
            local = declare_local(emitter, statement->name);
            {
                char *bare = py_bare(value);
                if(local->boxed && !py_aggregate(info.kind))
                    write_line(emitter, "%s = [%s]", local->python, bare);
                else {
                    local->boxed = 0;
                    write_line(emitter, "%s = %s", local->python, bare);
                }
                free(bare);
            }
            free(value);
            break;
        }
        case ZIR_STMT_ASSIGN:
            emit_assignment(emitter, statement);
            break;
        case ZIR_STMT_EXPR:
        case ZIR_STMT_UNUSED: {
            const ZirExpr *expression = expression_at(emitter, statement->expr_root);
            char *value, *bare;
            if(expression == NULL)
                break;
            if(expression->kind == ZIR_EXPR_CALL && !strcmp(expression->name, "print")) {
                emit_print(emitter, expression);
                break;
            }
            value = emit_expression(emitter, statement->expr_root);
            bare = py_bare(value);
            write_line(emitter, "%s", bare);
            free(value);
            free(bare);
            break;
        }
        case ZIR_STMT_IF:
            index = emit_if(emitter, index, end, 0);
            break;
        case ZIR_STMT_WHILE: {
            int close = block_end(emitter->function, index, end);
            char *condition = emit_expression(emitter, statement->expr_root);
            char *bare = py_bare(condition);
            if(statement->is_parallel || statement->is_gpu)
                write_line(emitter, "# ziran: #parallel region runs serially");
            write_line(emitter, "while %s:", bare);
            free(condition);
            free(bare);
            emit_loop_body(emitter, statement->loop_id, index + 1, close);
            emit_loop_exits(emitter, index, close, statement->loop_id);
            index = close;
            break;
        }
        case ZIR_STMT_BLOCK_OPEN: {
            int close = block_end(emitter->function, index, end);
            if(emit_native_for(emitter, index, close)) {
                index = close;
                break;
            }
            emitter->depth++;
            emit_sequence(emitter, index + 1, close);
            leave_scope(emitter);
            index = close;
            break;
        }
        case ZIR_STMT_BLOCK_CLOSE:
            break;
        case ZIR_STMT_RETURN:
            if(statement->expr_root >= 0) {
                const ZirExpr *expression = expression_at(emitter, statement->expr_root);
                char *value;
                PyLocal *local = expression->kind == ZIR_EXPR_IDENT ?
                    find_local(emitter, expression->name) : NULL;
                /* A local dies here, so it needs no copy. */
                if(local != NULL && !local->boxed)
                    value = emit_expression(emitter, statement->expr_root);
                else
                    value = emit_value(emitter, statement->expr_root,
                                       emitter->function->return_type);
                if(local != NULL && !local->boxed && expression->type[0] &&
                   strcmp(expression->type, emitter->function->return_type) != 0) {
                    char *converted = py_convert(emitter, value, expression->type,
                                                 emitter->function->return_type);
                    free(value);
                    value = converted;
                }
                char *bare = py_bare(value);
                write_line(emitter, "return %s", bare);
                free(value);
                free(bare);
            } else
                write_line(emitter, "return");
            break;
        case ZIR_STMT_BREAK:
        case ZIR_STMT_CONTINUE: {
            int is_break = statement->kind == ZIR_STMT_BREAK;
            if(statement->target_id && statement->target_id != innermost_loop(emitter)) {
                write_line(emitter, "ziran_jump = %s%d", is_break ? "-" : "",
                           statement->target_id);
                write_line(emitter, "break");
            } else
                write_line(emitter, is_break ? "break" : "continue");
            break;
        }
        case ZIR_STMT_UNREACHABLE:
            write_line(emitter, "raise AssertionError(\"unreachable\")");
            break;
        default:
            Diagnostic(statement->span, "zir_py.statement",
                    "unsupported statement in the Python target: %s",
                    statement->text && statement->text[0] ? statement->text :
                        StmtKindName(statement->kind));
            exit(1);
        }
    }
}

/* ----- functions ----- */

static const char *place_base(const ZirFunction *function, int index, int *through_member)
{
    *through_member = 0;
    while(index >= 0 && index < function->expr_count) {
        const ZirExpr *expression = &function->exprs[index];
        if(expression->kind == ZIR_EXPR_IDENT)
            return expression->name;
        if(expression->kind != ZIR_EXPR_MEMBER && expression->kind != ZIR_EXPR_INDEX &&
           expression->kind != ZIR_EXPR_SLICE)
            return NULL;
        *through_member = 1;
        index = expression->left;
    }
    return NULL;
}

/* Which locals live in a box, and which aggregates are overwritten in
 * place because a pointer or view may reach them. */
static void scan_function(PyEmitter *emitter, const ZirFunction *function)
{
    emitter->boxed.count = 0;
    emitter->in_place.count = 0;
    for(int index = 0; index < function->expr_count; index++) {
        const ZirExpr *expression = &function->exprs[index];
        const char *base;
        int through_member;
        if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "&")) {
            base = place_base(function, expression->right, &through_member);
            if(base != NULL) {
                if(!through_member)
                    names_add(&emitter->boxed, base);
                names_add(&emitter->in_place, base);
            }
        } else if(expression->kind == ZIR_EXPR_SLICE ||
                  (expression->kind == ZIR_EXPR_MEMBER && !strcmp(expression->name, "data"))) {
            base = place_base(function, expression->left, &through_member);
            if(base != NULL)
                names_add(&emitter->in_place, base);
        }
    }
}

static void emit_global_declarations(PyEmitter *emitter, const ZirFunction *function)
{
    PyNames rebound = {0};
    for(int index = 0; index < function->stmt_count; index++) {
        const ZirStmt *statement = &function->stmts[index];
        const ZirExpr *target;
        char name[ZIR_NAME_MAX * 2];
        PyType info;
        int declared = 0;
        if(statement->kind != ZIR_STMT_ASSIGN || statement->lhs_root < 0)
            continue;
        target = &function->exprs[statement->lhs_root];
        if(target->kind != ZIR_EXPR_IDENT)
            continue;
        for(int scan = 0; scan < function->stmt_count && !declared; scan++)
            declared = function->stmts[scan].kind == ZIR_STMT_DECL &&
                       !strcmp(function->stmts[scan].name, target->name);
        if(declared || find_local(emitter, target->name) != NULL)
            continue;
        if(!global_reference(emitter, target->name, name, sizeof(name), NULL))
            continue;
        if(py_classify(emitter->module, target->type, &info) && py_aggregate(info.kind))
            continue;
        names_add(&rebound, name);
    }
    if(rebound.count > 0) {
        char *line = py_copy_string("global ");
        for(int index = 0; index < rebound.count; index++) {
            char *joined = py_format("%s%s%s", line, index ? ", " : "", rebound.items[index]);
            free(line);
            line = joined;
        }
        write_line(emitter, "%s", line);
        free(line);
    }
    free(rebound.items);
}

static int function_uses_jump(const ZirFunction *function)
{
    for(int index = 0; index < function->stmt_count; index++) {
        const ZirStmt *statement = &function->stmts[index];
        if((statement->kind == ZIR_STMT_BREAK || statement->kind == ZIR_STMT_CONTINUE) &&
           statement->target_id) {
            /* A jump to the loop right around it needs no flag. */
            int depth = 0;
            for(int scan = index - 1; scan >= 0; scan--) {
                ZirStmtKind kind = function->stmts[scan].kind;
                if(kind == ZIR_STMT_BLOCK_CLOSE)
                    depth++;
                else if(kind == ZIR_STMT_IF || kind == ZIR_STMT_BLOCK_OPEN ||
                        kind == ZIR_STMT_WHILE) {
                    if(depth > 0)
                        depth--;
                    else if(kind == ZIR_STMT_WHILE) {
                        if(function->stmts[scan].loop_id != statement->target_id)
                            return 1;
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

static void lower_function(PyEmitter *emitter, const ZirModule *module,
                           const ZirFunction *function)
{
    char symbol[ZIR_NAME_MAX * 2];
    char (*names)[ZIR_NAME_MAX];
    char (*types)[ZIR_NAME_MAX];
    char *parameters;
    int count;
    long before;
    if(function->is_extern || function->is_template)
        return;
    names = py_allocate(64 * ZIR_NAME_MAX);
    types = py_allocate(64 * ZIR_NAME_MAX);
    parameters = py_copy_string("");
    emitter->module = module;
    emitter->function = function;
    emitter->local_count = 0;
    emitter->depth = 0;
    emitter->loop_count = 0;
    emitter->native_loop_count = 0;
    emitter->temporaries = 0;
    emitter->counters.count = 0;
    scan_function(emitter, function);
    function_symbol(emitter, module, function, symbol, sizeof(symbol));
    count = function_parameter_names(function, names, types, 64);
    for(int index = 0; index < count; index++) {
        PyType info;
        PyLocal *local;
        char *joined;
        py_require_type(module, function->span, types[index], &info);
        local = declare_local(emitter, names[index]);
        joined = py_format("%s%s%s", parameters, index ? ", " : "", local->python);
        free(parameters);
        parameters = joined;
    }
    fprintf(emitter->output, "def %s(%s):\n", symbol, parameters);
    free(parameters);
    emitter->indent = 1;
    before = emitter->lines;
    emitter->depth = 1;
    emit_global_declarations(emitter, function);
    for(int index = 0; index < emitter->local_count; index++) {
        PyLocal *local = &emitter->locals[index];
        PyType info;
        py_classify(module, types[index], &info);
        if(local->boxed && !py_aggregate(info.kind))
            write_line(emitter, "%s = [%s]", local->python, local->python);
        else
            local->boxed = 0;
    }
    if(function_uses_jump(function))
        write_line(emitter, "ziran_jump = 0");
    emit_sequence(emitter, 0, function->stmt_count);
    if(emitter->lines == before)
        write_line(emitter, "pass");
    emitter->indent = 0;
    fputs("\n\n", emitter->output);
    free(names);
    free(types);
}

/* ----- types ----- */

static const char *py_struct_format(PyEmitter *emitter, const ZirModule *scope,
                                    const char *type)
{
    PyType info;
    (void)emitter;
    if(!py_classify(scope, type, &info))
        return NULL;
    if(info.kind == PY_BOOL)
        return "?";
    if(info.kind == PY_FLOAT)
        return info.bits == 32 ? "f" : "d";
    if(info.kind != PY_INT)
        return NULL;
    switch(info.bits) {
    case 8: return info.is_signed ? "b" : "B";
    case 16: return info.is_signed ? "h" : "H";
    case 32: return info.is_signed ? "i" : "I";
    default: return info.is_signed ? "q" : "Q";
    }
}

static void emit_union_class(PyEmitter *emitter, FILE *output, const ZirModule *module,
                             const ZirType *record, const char *name)
{
    size_t size = 0, alignment = 0;
    size_t offset = 0;
    ZirTypeField field;
    if(!TypeLayout(module, record->name, &size, &alignment)) {
        Diagnostic(record->span, "zir_py.union", "cannot lay out union %s", record->name);
        exit(1);
    }
    fprintf(output, "class %s:\n", name);
    fputs("    \"\"\"A union: every field reads the same bytes.\"\"\"\n\n", output);
    fputs("    __slots__ = (\"_bytes\",)\n\n", output);
    fputs("    def __init__(self, **fields):\n", output);
    fprintf(output, "        self._bytes = bytearray(%zu)\n", size);
    fputs("        for field, value in fields.items():\n", output);
    fputs("            setattr(self, field, value)\n\n", output);
    while(TypeNextField(record, &offset, &field) == 1) {
        char field_name[ZIR_NAME_MAX];
        const char *format = py_struct_format(emitter, module, field.type);
        PyType array;
        if(format == NULL && py_classify(module, field.type, &array) &&
           array.kind == PY_ARRAY &&
           (format = py_struct_format(emitter, module, array.element)) != NULL) {
            py_field_name(field.name, field_name, sizeof(field_name));
            fprintf(output, "    @property\n    def %s(self):\n", field_name);
            fprintf(output, "        return ZiranUnionArray(self._bytes, \"%s\", %d)\n\n",
                    format, array.capacity);
            fprintf(output, "    @%s.setter\n    def %s(self, value):\n", field_name, field_name);
            fprintf(output, "        ZiranUnionArray(self._bytes, \"%s\", %d)[:] = value\n\n",
                    format, array.capacity);
            continue;
        }
        if(format == NULL) {
            Diagnostic(record->span, "zir_py.union",
                    "the Python target lowers only scalar union fields: %s", field.name);
            exit(1);
        }
        py_field_name(field.name, field_name, sizeof(field_name));
        fprintf(output, "    @property\n    def %s(self):\n", field_name);
        fprintf(output, "        return struct.unpack_from(\"=%s\", self._bytes)[0]\n\n", format);
        fprintf(output, "    @%s.setter\n    def %s(self, value):\n", field_name, field_name);
        fprintf(output, "        struct.pack_into(\"=%s\", self._bytes, 0, value)\n\n", format);
    }
    fprintf(output, "    def copy(self):\n        result = %s()\n", name);
    fputs("        result._bytes[:] = self._bytes\n        return result\n\n", output);
    fputs("    def assign(self, other):\n        self._bytes[:] = other._bytes\n\n\n", output);
}

static void emit_record_class(PyEmitter *emitter, FILE *output, const ZirModule *module,
                              const ZirType *record, const char *name)
{
    size_t offset = 0;
    ZirTypeField field;
    char *slots = py_copy_string("");
    char *parameters = py_copy_string("");
    int count = 0;
    while(TypeNextField(record, &offset, &field) == 1) {
        char field_name[ZIR_NAME_MAX];
        PyType info;
        char *joined;
        py_require_type(module, record->span, field.type, &info);
        py_field_name(field.name, field_name, sizeof(field_name));
        joined = py_format("%s%s\"%s\"", slots, count ? ", " : "", field_name);
        free(slots);
        slots = joined;
        joined = py_format("%s, %s", parameters, field_name);
        free(parameters);
        parameters = joined;
        count++;
    }
    fprintf(output, "class %s:\n", name);
    fprintf(output, "    __slots__ = (%s%s)\n\n", slots, count == 1 ? "," : "");
    fprintf(output, "    def __init__(self%s):\n", parameters);
    if(count == 0)
        fputs("        pass\n", output);
    offset = 0;
    while(TypeNextField(record, &offset, &field) == 1) {
        char field_name[ZIR_NAME_MAX];
        py_field_name(field.name, field_name, sizeof(field_name));
        fprintf(output, "        self.%s = %s\n", field_name, field_name);
    }
    fprintf(output, "\n    def copy(self):\n        return %s(", name);
    offset = 0;
    count = 0;
    while(TypeNextField(record, &offset, &field) == 1) {
        char field_name[ZIR_NAME_MAX];
        char *access, *copied;
        py_field_name(field.name, field_name, sizeof(field_name));
        access = py_format("self.%s", field_name);
        copied = py_copy_text(emitter, module, field.type, access, 0);
        fprintf(output, "%s%s", count ? ", " : "", copied);
        free(access);
        free(copied);
        count++;
    }
    fputs(")\n\n    def assign(self, other):\n", output);
    if(count == 0)
        fputs("        pass\n", output);
    offset = 0;
    while(TypeNextField(record, &offset, &field) == 1) {
        char field_name[ZIR_NAME_MAX];
        PyType info, element;
        py_field_name(field.name, field_name, sizeof(field_name));
        py_classify(module, field.type, &info);
        if(info.kind == PY_ARRAY) {
            if(py_classify(module, info.element, &element) && py_aggregate(element.kind))
                fprintf(output, "        _assign_items(self.%s, other.%s)\n", field_name,
                        field_name);
            else
                fprintf(output, "        self.%s[:] = other.%s\n", field_name, field_name);
        } else if(py_aggregate(info.kind))
            fprintf(output, "        self.%s.assign(other.%s)\n", field_name, field_name);
        else
            fprintf(output, "        self.%s = other.%s\n", field_name, field_name);
    }
    fputs("\n\n", output);
    free(slots);
    free(parameters);
}

static void emit_classes(PyEmitter *emitter, FILE *output)
{
    for(int program_index = 0; program_index < emitter->program_count; program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++) {
            const ZirModule *module = &program->modules[module_index];
            emitter->module = module;
            for(int type_index = 0; type_index < module->type_count; type_index++) {
                const ZirType *record = &module->types[type_index];
                PyType info;
                char name[ZIR_NAME_MAX];
                if(record->is_record_template)
                    continue;
                if(!py_classify(module, record->name, &info)) {
                    Diagnostic(record->span, "zir_py.type",
                            "the Python target cannot lower this type: %s", record->name);
                    exit(1);
                }
                if(info.kind != PY_RECORD && info.kind != PY_UNION)
                    continue;
                if(info.declared != record)
                    continue;
                py_class_name(&info, name, sizeof(name));
                if(info.kind == PY_UNION)
                    emit_union_class(emitter, output, module, record, name);
                else
                    emit_record_class(emitter, output, module, record, name);
            }
        }
    }
}

/* Shared libraries named in LDLIBS (-lNAME) when the program was built. */
static const char *const *py_linked_libraries;
static int py_linked_library_count;

void py_set_linked_libraries(const char *const *names, int count)
{
    py_linked_libraries = names;
    py_linked_library_count = count;
}

/* Buffers emit_foreign_bindings keeps on the heap: py_lower inlines it, and
 * some compilers add every inlined frame together past FRAMEFLAGS. */
typedef struct PyForeignBuffers {
    char symbol[ZIR_NAME_MAX * 2];
    char parameters[32][ZIR_NAME_MAX];
    char library[ZIR_PATH_MAX];
} PyForeignBuffers;

static void emit_foreign_bindings(PyEmitter *emitter, FILE *output)
{
    int any = 0, linked = 0;
    char *python_modules = py_copy_string("");
    PyForeignBuffers *buffers = py_allocate(sizeof(*buffers));
    char *symbol = buffers->symbol;
    char (*parameters)[ZIR_NAME_MAX] = buffers->parameters;
    char *library = buffers->library;
    for(int program_index = 0; program_index < emitter->program_count; program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++) {
            const ZirModule *module = &program->modules[module_index];
            emitter->module = module;
            for(int import_index = 0; import_index < module->import_count; import_index++) {
                const ZirImport *import = &module->imports[import_index];
                const char *dot, *link, *returned;
                char *argtypes;
                int count;
                if(import->kind != ZIR_IMPORT_EXTERN)
                    continue;
                if(import->extern_kind == ZIR_EXTERN_HOST)
                    continue;
                if(import->extern_kind == ZIR_EXTERN_PY) {
                    char python_module[ZIR_PATH_MAX], module_symbol[ZIR_PATH_MAX * 2];
                    char *line;
                    if(!PyForeignCallParts(import->target, python_module, sizeof(python_module),
                                           NULL, 0, NULL, 0)) {
                        Diagnostic(import->span, "zir_py.import",
                                   "invalid Python foreign target: %s", import->target);
                        exit(1);
                    }
                    py_module_symbol(python_module, module_symbol, sizeof(module_symbol));
                    line = py_format("%s = _py_module(\"%s\")\n", module_symbol, python_module);
                    if(strstr(python_modules, line) == NULL) {
                        char *joined = py_format("%s%s", python_modules, line);
                        fputs(line, output);
                        free(python_modules);
                        python_modules = joined;
                    }
                    free(line);
                    any = 1;
                    continue;
                }
                if(import->extern_kind != ZIR_EXTERN_C) {
                    Diagnostic(import->span, "zir_py.import",
                            "the Python target runs C and py: foreign imports, not Go: %s",
                            import->name);
                    exit(1);
                }
                /* C foreign symbols resolve in the process, as a C build links
                 * them; libraries named in LDLIBS join it first. */
                if(!linked)
                    for(int index = 0; index < py_linked_library_count; index++)
                        fprintf(output, "_link_library(\"%s\")\n", py_linked_libraries[index]);
                count = foreign_parameters(import, parameters, 32);
                foreign_symbol(emitter, module, import, symbol, sizeof(buffers->symbol));
                snprintf(library, sizeof(buffers->library), "%s", import->target);
                dot = strrchr(library, '.');
                if(dot != NULL)
                    library[dot - library] = '\0';
                link = import->extern_symbol[0] ? import->extern_symbol : import->name;
                argtypes = py_copy_string("");
                for(int index = 0; index < count; index++) {
                    const char *ctype = py_pointer_ctype(emitter, parameters[index]);
                    char *pointer = NULL;
                    char *joined;
                    if(ctype != NULL)
                        pointer = py_format("ctypes.POINTER(%s)", ctype);
                    else if((ctype = py_ctype(emitter, parameters[index])) == NULL) {
                        Diagnostic(import->span, "zir_py.import",
                                "unsupported foreign parameter type in %s", import->name);
                        exit(1);
                    }
                    joined = py_format("%s%s%s", argtypes, index ? ", " : "",
                                       pointer != NULL ? pointer : ctype);
                    free(argtypes);
                    free(pointer);
                    argtypes = joined;
                }
                returned = py_ctype(emitter, import->return_type[0] ? import->return_type : "void");
                if(returned == NULL) {
                    Diagnostic(import->span, "zir_py.import",
                            "unsupported foreign return type: %s", import->name);
                    exit(1);
                }
                fprintf(output, "%s = _foreign(_library(\"%s\"), \"%s\", [%s], %s)\n",
                        symbol, library, link, argtypes, returned);
                free(argtypes);
                any = linked = 1;
            }
        }
    }
    free(buffers);
    free(python_modules);
    if(any)
        fputs("\n\n", output);
}

/* ----- globals ----- */

static void fill_literal_types(PyEmitter *emitter, ZirFunction *literal, int index,
                               const char *type, int depth)
{
    char element[ZIR_NAME_MAX];
    PyType info;
    int position = 0;
    if(index < 0 || index >= literal->expr_count || depth > 32)
        return;
    ZirExpr *node = &literal->exprs[index];
    if(node->type[0] == '\0' || !strcmp(node->type, "integer") ||
       !strcmp(node->type, "real") || node->kind == ZIR_EXPR_COMPOUND)
        node->type = KeepNameFormat("%s", type);
    if(node->kind != ZIR_EXPR_COMPOUND)
        return;
    int array = ArrayElementType(type, element, sizeof(element), NULL);
    if(!array && (!py_classify(emitter->module, type, &info) || info.kind != PY_RECORD))
        return;
    for(int child = node->first_child; child >= 0;
        child = literal->exprs[child].next_sibling, position++) {
        ZirExpr *entry = &literal->exprs[child];
        char field_type[ZIR_NAME_MAX] = "";
        if(array)
            snprintf(field_type, sizeof(field_type), "%s", element);
        else {
            size_t offset = 0;
            int ordinal = 0;
            ZirTypeField field;
            while(TypeNextField(info.declared, &offset, &field) == 1) {
                if(entry->name[0] ? !strcmp(field.name, entry->name) : ordinal == position) {
                    snprintf(field_type, sizeof(field_type), "%s", field.type);
                    if(!entry->name[0])
                        entry->name = KeepNameFormat("%s", field.name);
                    break;
                }
                ordinal++;
            }
            if(field_type[0] && info.owner != emitter->module &&
               FindType(emitter->module, field_type, NULL) == NULL)
                for(int import = 0; import < emitter->module->import_count; import++)
                    if(emitter->module->imports[import].resolved_module == info.owner &&
                       emitter->module->imports[import].name[0]) {
                        char spelled[ZIR_NAME_MAX];
                        snprintf(spelled, sizeof(spelled), "%s.%s",
                                 emitter->module->imports[import].name, field_type);
                        snprintf(field_type, sizeof(field_type), "%s", spelled);
                        break;
                    }
        }
        if(!field_type[0])
            continue;
        if(entry->kind == ZIR_EXPR_FIELD_INIT) {
            entry->type = KeepNameFormat("%s", field_type);
            fill_literal_types(emitter, literal, entry->right, field_type, depth + 1);
        } else
            fill_literal_types(emitter, literal, child, field_type, depth + 1);
    }
}

static char *global_initializer(PyEmitter *emitter, const ZirGlobal *global)
{
    PyType info;
    py_classify(emitter->module, global->type, &info);
    if(global->init[0] != '\0' && info.kind == PY_STRING) {
        unsigned char *bytes = py_allocate(ZIR_TEXT_MAX);
        size_t length;
        if(DecodeStringLiteral(global->init, bytes, ZIR_TEXT_MAX, &length)) {
            char *result = py_string_literal(bytes, length);
            free(bytes);
            return result;
        }
        free(bytes);
    }
    if(global->init[0] != '\0' && info.kind == PY_INT && info.declared != NULL &&
       info.declared->is_enum) {
        const char *member = strrchr(global->init, '.');
        int64_t value;
        if(member != NULL && EnumMemberValue(info.declared, member + 1, &value))
            return py_format("%lld", (long long)value);
    }
    if(global->init[0] != '\0' && strchr(global->init, '(') == NULL) {
        long long integer;
        char *end;
        double real;
        if(info.kind == PY_BOOL && (!strcmp(global->init, "true") ||
                                    !strcmp(global->init, "false")))
            return py_copy_string(!strcmp(global->init, "true") ? "True" : "False");
        if(info.kind == PY_INT && py_integer_literal(global->init, &integer)) {
            if(!info.is_signed)
                return py_format("%llu", (unsigned long long)integer &
                                 (info.bits >= 64 ? ~0ULL : ((1ULL << info.bits) - 1)));
            return py_format("%lld", integer);
        }
        if(info.kind == PY_FLOAT) {
            real = strtod(global->init, &end);
            if(*end == '\0') {
                if(info.bits == 32)
                    return py_round_float32_literal(real);
                if(strpbrk(global->init, ".eEn") == NULL)
                    return py_format("%s.0", global->init);
                return py_format("%.17g", real);
            }
        }
        if((info.kind == PY_POINTER || info.kind == PY_PROCEDURE) &&
           !strcmp(global->init, "null"))
            return py_copy_string("None");
    }
    if(global->init[0] != '\0' && (info.kind == PY_RECORD || info.kind == PY_ARRAY)) {
        ZirFunction *probe = py_allocate(sizeof(*probe));
        int root = ParseExprTyped(probe, emitter->module, global->init, global->span,
                                  global->type);
        int constant = root >= 0;
        for(int index = 0; index < probe->expr_count && constant; index++)
            constant = probe->exprs[index].kind != ZIR_EXPR_IDENT &&
                       probe->exprs[index].kind != ZIR_EXPR_CALL;
        if(constant && probe->exprs[root].kind == ZIR_EXPR_COMPOUND) {
            const ZirFunction *saved = emitter->function;
            char *result;
            fill_literal_types(emitter, probe, root, global->type, 0);
            emitter->function = probe;
            result = emit_value(emitter, root, global->type);
            emitter->function = saved;
            free(probe->exprs);
            free(probe);
            return result;
        }
        free(probe->exprs);
        free(probe);
    }
    return py_zero(emitter, emitter->module, global->type);
}

/* Globals are lowered outside any function. A ZirFunction is 9 KB, too big
 * for py_lower's frame once emit_globals is inlined into it. */
static const ZirFunction py_no_function;

static void emit_globals(PyEmitter *emitter, FILE *output)
{
    int any = 0;
    for(int program_index = 0; program_index < emitter->program_count; program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++) {
            const ZirModule *module = &program->modules[module_index];
            emitter->module = module;
            emitter->function = &py_no_function;
            for(int global_index = 0; global_index < module->global_count; global_index++) {
                const ZirGlobal *global = &module->globals[global_index];
                char symbol[ZIR_NAME_MAX * 2];
                PyType info;
                char *value, *bare;
                py_require_type(module, global->span, global->type, &info);
                global_symbol(emitter, module, global, symbol, sizeof(symbol));
                value = global_initializer(emitter, global);
                bare = py_bare(value);
                fprintf(output, "%s = %s\n", symbol, bare);
                free(value);
                free(bare);
                any = 1;
            }
        }
    }
    if(any)
        fputs("\n", output);
}

typedef struct PyModuleVisit {
    const ZirModule *module;
    int state;
} PyModuleVisit;

typedef struct PyModuleVisits {
    PyModuleVisit *items;
    size_t count;
    size_t capacity;
} PyModuleVisits;

static PyModuleVisit *module_visit(PyModuleVisits *visits, const ZirModule *module)
{
    for(size_t index = 0; index < visits->count; index++)
        if(visits->items[index].module == module)
            return &visits->items[index];
    if(visits->count == visits->capacity) {
        size_t capacity = visits->capacity ? visits->capacity * 2 : 16;
        PyModuleVisit *items = realloc(visits->items, capacity * sizeof(*items));
        if(items == NULL) {
            fprintf(stderr, "zi2py: out of memory\n");
            exit(1);
        }
        visits->items = items;
        visits->capacity = capacity;
    }
    visits->items[visits->count].module = module;
    visits->items[visits->count].state = 0;
    return &visits->items[visits->count++];
}

static void visit_startup_module(PyModuleVisits *visits, PyModuleVisits *ordered,
                                 const ZirModule *module)
{
    PyModuleVisit *visit = module_visit(visits, module);
    if(visit->state == 1) {
        Diagnostic(module->span, "zir_py.startup", "cyclic module startup is unsupported");
        exit(1);
    }
    if(visit->state == 2)
        return;
    visit->state = 1;
    /* As in C and Go, only dependencies with something to set up take part:
     * an import cycle through modules without globals orders nothing. */
    for(int index = 0; index < module->import_count; index++)
        if(module->imports[index].resolved_module != NULL &&
           ModuleNeedsStartup(module->imports[index].resolved_module))
            visit_startup_module(visits, ordered, module->imports[index].resolved_module);
    visit = module_visit(visits, module);
    visit->state = 2;
    for(int index = 0; index < module->function_count; index++)
        if(module->functions[index].is_global_initializer) {
            int present = 0;
            for(size_t scan = 0; scan < ordered->count; scan++)
                present |= ordered->items[scan].module == module;
            if(!present)
                module_visit(ordered, module);
            break;
        }
}

static void emit_startup(PyEmitter *emitter, FILE *output)
{
    PyModuleVisits visits = {0}, ordered = {0};
    for(int program_index = 0; program_index < emitter->program_count; program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++)
            if(ModuleNeedsStartup(&program->modules[module_index]))
                visit_startup_module(&visits, &ordered, &program->modules[module_index]);
    }
    for(size_t index = 0; index < ordered.count; index++) {
        const ZirModule *module = ordered.items[index].module;
        for(int function_index = 0; function_index < module->function_count; function_index++) {
            const ZirFunction *function = &module->functions[function_index];
            char symbol[ZIR_NAME_MAX * 2];
            if(!function->is_global_initializer)
                continue;
            function_symbol(emitter, module, function, symbol, sizeof(symbol));
            fprintf(output, "%s()\n", symbol);
        }
    }
    if(ordered.count > 0)
        fputs("\n", output);
    free(visits.items);
    free(ordered.items);
}

/* ----- output ----- */

static int text_names(const char *text, size_t length, const char *name)
{
    size_t size = strlen(name);
    for(size_t index = 0; index + size <= length; index++) {
        if(memcmp(text + index, name, size) != 0)
            continue;
        if(index > 0 && (identifier_character(text[index - 1]) || text[index - 1] == '.'))
            continue;
        if(index + size < length && identifier_character(text[index + size]))
            continue;
        return 1;
    }
    return 0;
}

static int text_mentions(const char *text, size_t length, const char *needle)
{
    size_t size = strlen(needle);
    for(size_t index = 0; index + size <= length; index++)
        if(memcmp(text + index, needle, size) == 0)
            return 1;
    return 0;
}

/* The runtime items the program names, directly or through another kept
 * item, in runtime order. */
static void write_runtime(FILE *output, const char *body, size_t body_size,
                          int *kept, int *module_needed)
{
    int changed = 1, count = 0;
    while(py_runtime_items[count].name != NULL)
        count++;
    for(int index = 0; index < count; index++)
        kept[index] = text_names(body, body_size, py_runtime_items[index].name);
    while(changed) {
        changed = 0;
        for(int index = 0; index < count; index++) {
            if(kept[index])
                continue;
            for(int other = 0; other < count && !kept[index]; other++)
                if(kept[other] && other != index &&
                   text_names(py_runtime_items[other].text,
                              strlen(py_runtime_items[other].text),
                              py_runtime_items[index].name))
                    kept[index] = changed = 1;
        }
    }
    (void)module_needed;
    (void)output;
}

static void write_imports(FILE *output, const char *body, size_t body_size, const int *kept)
{
    static const char *const modules[] = {
        "ctypes", "ctypes.util", "decimal", "importlib", "math", "struct", "sys", NULL
    };
    int any = 0;
    for(int module = 0; modules[module] != NULL; module++) {
        char prefix[64];
        int needed = 0;
        snprintf(prefix, sizeof(prefix), "%s.", modules[module]);
        for(int index = 0; py_runtime_items[index].name != NULL && !needed; index++) {
            const char *imports = py_runtime_items[index].imports;
            if(!kept[index])
                continue;
            for(const char *at = imports; (at = strstr(at, modules[module])) != NULL;
                at += strlen(modules[module])) {
                size_t length = strlen(modules[module]);
                if((at == imports || at[-1] == ' ') && (at[length] == '\0' || at[length] == ' '))
                    needed = 1;
            }
        }
        if(!needed && strcmp(modules[module], "ctypes.util") != 0)
            needed = text_names(body, body_size, modules[module]) &&
                     text_mentions(body, body_size, prefix);
        if(needed) {
            fprintf(output, "import %s\n", modules[module]);
            any = 1;
        }
    }
    if(any)
        fputs("\n", output);
}

static void collect_module_names(PyEmitter *emitter)
{
    char name[ZIR_NAME_MAX * 2];
    for(int index = 0; py_runtime_items[index].name != NULL; index++)
        names_add(&emitter->module_names, py_runtime_items[index].name);
    names_add(&emitter->module_names, "ziran_jump");
    for(int program_index = 0; program_index < emitter->program_count; program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++) {
            const ZirModule *module = &program->modules[module_index];
            for(int index = 0; index < module->function_count; index++) {
                function_symbol(emitter, module, &module->functions[index], name, sizeof(name));
                names_add(&emitter->module_names, name);
            }
            for(int index = 0; index < module->global_count; index++) {
                global_symbol(emitter, module, &module->globals[index], name, sizeof(name));
                names_add(&emitter->module_names, name);
            }
            for(int index = 0; index < module->import_count; index++)
                if(module->imports[index].kind == ZIR_IMPORT_EXTERN) {
                    foreign_symbol(emitter, module, &module->imports[index], name, sizeof(name));
                    names_add(&emitter->module_names, name);
                }
            for(int index = 0; index < module->type_count; index++) {
                PyType info;
                if(module->types[index].is_record_template ||
                   !py_classify(module, module->types[index].name, &info) ||
                   (info.kind != PY_RECORD && info.kind != PY_UNION))
                    continue;
                py_class_name(&info, name, sizeof(name));
                names_add(&emitter->module_names, name);
            }
        }
    }
}

static void make_directories(const char *path)
{
    char partial[ZIR_PATH_MAX * 2];
    snprintf(partial, sizeof(partial), "%s", path);
    for(size_t index = 1; partial[index]; index++) {
        if(partial[index] != '/')
            continue;
        partial[index] = '\0';
        mkdir(partial, 0755);
        partial[index] = '/';
    }
    mkdir(partial, 0755);
}

static int integer_type(const char *type)
{
    int bits, is_signed;
    return py_integer_width(type, &bits, &is_signed);
}

int py_lower(const ZirProgram *const *programs, int program_count,
             const char *output_directory, const char *entry_module,
             const char *entry_function, int executable)
{
    PyEmitter *emitter = py_allocate(sizeof(*emitter));
    const ZirModule *entry_owner = NULL;
    const ZirFunction *entry = NULL;
    char *body_text = NULL;
    size_t body_size = 0;
    FILE *body, *output;
    char path[ZIR_PATH_MAX * 2];
    char temporary[ZIR_PATH_MAX * 2 + 64];
    int *kept;
    int runtime_count = 0;
    if(programs == NULL || program_count <= 0 || output_directory == NULL ||
       output_directory[0] == '\0') {
        Diagnostic(Span("<command>", 1, 1), "zir_py.input",
                   "no checked programs were supplied");
        return 1;
    }
    emitter->programs = programs;
    emitter->program_count = program_count;
    if(executable) {
        for(int program_index = 0; program_index < program_count; program_index++)
            for(int module_index = 0; module_index < programs[program_index]->module_count;
                module_index++) {
                const ZirModule *module = &programs[program_index]->modules[module_index];
                if(strcmp(module->name, entry_module) != 0)
                    continue;
                for(int function_index = 0; function_index < module->function_count;
                    function_index++)
                    if(strcmp(module->functions[function_index].name, entry_function) == 0) {
                        entry_owner = module;
                        entry = &module->functions[function_index];
                    }
            }
        if(entry == NULL) {
            Diagnostic(Span("<command>", 1, 1), "zir_py.entry",
                       "executable entry is missing: %s:%s", entry_module, entry_function);
            return 1;
        }
        if(FunctionArgs(entry)[0] || (strcmp(entry->return_type, "void") != 0 &&
                              !integer_type(entry->return_type) &&
                              strcmp(entry->return_type, "bool") != 0)) {
            Diagnostic(entry->span, "zir_py.entry",
                       "executable entry must take no arguments and return void, bool, or an integer");
            return 1;
        }
    }
    collect_module_names(emitter);
    body = open_memstream(&body_text, &body_size);
    if(body == NULL) {
        Diagnostic(Span(output_directory, 1, 1), "zir_py.output", "cannot buffer Python output");
        return 1;
    }
    emitter->output = body;
    emit_classes(emitter, body);
    emit_foreign_bindings(emitter, body);
    for(int program_index = 0; program_index < program_count; program_index++) {
        const ZirProgram *program = programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++) {
            const ZirModule *module = &program->modules[module_index];
            for(int function_index = 0; function_index < module->function_count;
                function_index++)
                lower_function(emitter, module, &module->functions[function_index]);
        }
    }
    if(emitter->uses_host)
        fputs("# The embedding program sets host to an object with the host functions.\n"
              "host = None\n\n", body);
    emit_globals(emitter, body);
    emit_startup(emitter, body);
    if(executable) {
        char symbol[ZIR_NAME_MAX * 2];
        function_symbol(emitter, entry_owner, entry, symbol, sizeof(symbol));
        fputs("if __name__ == \"__main__\":\n", body);
        if(strcmp(entry->return_type, "void") == 0)
            fprintf(body, "    _ziran_entry(%s)\n", symbol);
        else if(strcmp(entry->return_type, "bool") == 0)
            fprintf(body, "    sys.exit(1 if _ziran_entry(%s) else 0)\n", symbol);
        else
            fprintf(body, "    sys.exit(_ziran_entry(%s) & 0xFF)\n", symbol);
    }
    if(fclose(body) != 0) {
        Diagnostic(Span(output_directory, 1, 1), "zir_py.output", "cannot buffer Python output");
        return 1;
    }
    /* Trailing blank lines end the file at one newline. */
    while(body_size > 1 && body_text[body_size - 1] == '\n' && body_text[body_size - 2] == '\n')
        body_size--;
    while(py_runtime_items[runtime_count].name != NULL)
        runtime_count++;
    kept = py_allocate(sizeof(*kept) * (size_t)(runtime_count + 1));
    write_runtime(NULL, body_text, body_size, kept, NULL);
    make_directories(output_directory);
    snprintf(path, sizeof(path), "%s/%s", output_directory,
             executable ? "__main__.py" : "__init__.py");
    output = GeneratedOutputOpen(path, temporary, sizeof(temporary));
    if(output == NULL) {
        Diagnostic(Span(output_directory, 1, 1), "zir_py.output",
                   "cannot create Python output: %s", path);
        return 1;
    }
    if(program_count == 1 && programs[0]->module_count == 1)
        fprintf(output, "# Code generated by zi2py from %s. DO NOT EDIT.\n\n",
                programs[0]->modules[0].source_path);
    else
        fprintf(output, "# Code generated by zi2py from %s. DO NOT EDIT.\n\n",
                entry_owner != NULL ? entry_owner->source_path :
                                      programs[0]->modules[0].source_path);
    write_imports(output, body_text, body_size, kept);
    for(int index = 0; index < runtime_count; index++)
        if(kept[index]) {
            fputs(py_runtime_items[index].text, output);
            fputs("\n\n", output);
        }
    fwrite(body_text, 1, body_size, output);
    free(kept);
    free(body_text);
    if(fclose(output) != 0 || GeneratedOutputReplace(temporary, path) != 0) {
        Diagnostic(Span(output_directory, 1, 1), "zir_py.output",
                   "cannot finish Python output");
        return 1;
    }
    return 0;
}
