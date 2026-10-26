/* Host capability trampolines for natively compiled bundles.
 *
 * A bundle compiled to native code calls each host capability as the plain C
 * function its module header declares. The definitions written here pass
 * those C arguments to the host as the same VmHostValue shapes the portable
 * VM builds: the declared type texts, record fields in declaration order with
 * their names, fixed arrays, and slices whose elements the host may change.
 * Results are checked like the VM checks them and converted back into the
 * declared C type. A type this cannot carry is an error when the bundle
 * compiles, never a silent fallback. */
#include "zir_zx_host.h"
#include "zir_check.h"
#include "zir_emit.h"
#include "zir_native.h"
#include "zir_stream.h"
#include "ziran_host.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Generated code sees host values under its own names, so a bundle module
 * whose header guard or types match ziran_host.h cannot collide with them.
 * The layout is VmHostValue's, checked below. */
#define ZX_VALUE_DECLARATIONS \
    typedef struct zx_field zx_field; \
    typedef struct zx_value { \
        int kind; \
        const char *type; \
        int64_t integer; \
        uint64_t bits; \
        double real; \
        const unsigned char *data; \
        size_t length; \
        const zx_field *fields; \
        size_t field_count; \
        struct zx_value *elements; \
        void *pointer; \
    } zx_value; \
    struct zx_field { \
        const char *name; \
        zx_value value; \
    };
ZX_VALUE_DECLARATIONS
#define ZX_SAME_FIELD(field) (offsetof(zx_value, field) == offsetof(VmHostValue, field))
_Static_assert(sizeof(zx_value) == sizeof(VmHostValue) && sizeof(zx_field) == sizeof(VmHostField) &&
               sizeof(VmHostValueKind) == sizeof(int) && ZX_SAME_FIELD(type) &&
               ZX_SAME_FIELD(integer) && ZX_SAME_FIELD(bits) && ZX_SAME_FIELD(real) &&
               ZX_SAME_FIELD(data) && ZX_SAME_FIELD(length) && ZX_SAME_FIELD(fields) &&
               ZX_SAME_FIELD(field_count) && ZX_SAME_FIELD(elements) && ZX_SAME_FIELD(pointer) &&
               offsetof(zx_field, value) == offsetof(VmHostField, value),
               "zx_value must keep VmHostValue's layout");
_Static_assert(VM_HOST_VOID == 0 && VM_HOST_INTEGER == 1 && VM_HOST_UNSIGNED == 2 &&
               VM_HOST_REAL == 3 && VM_HOST_STRING == 4 && VM_HOST_RECORD == 5 &&
               VM_HOST_SLICE == 6 && VM_HOST_POINTER == 7 && VM_HOST_ARRAY == 8,
               "ZX_ kinds must match VmHostValueKind");
#define ZX_TEXT(...) #__VA_ARGS__
#define ZX_EXPANDED_TEXT(...) ZX_TEXT(__VA_ARGS__)

const char ZxNativeServicesHeader[] =
    "#ifndef ZX_NATIVE_SERVICES_H\n"
    "#define ZX_NATIVE_SERVICES_H\n"
    "#include <stddef.h>\n"
    "#include <stdint.h>\n"
    ZX_EXPANDED_TEXT(ZX_VALUE_DECLARATIONS) "\n"
    "enum { ZX_VOID, ZX_INTEGER, ZX_UNSIGNED, ZX_REAL, ZX_STRING, ZX_RECORD, ZX_SLICE, ZX_POINTER, ZX_ARRAY };\n"
    "void ZxNativeHost(int capability, zx_value *args, int count, zx_value *result);\n"
    "void ZxNativeScratchReset(void);\n"
    "void *ZxNativeScratch(size_t size);\n"
    "void *ZxNativeKeep(size_t size);\n"
    "void ZxNativeString(const zx_value *value, const char **data, size_t *length);\n"
    "void ZxNativeExpect(const zx_value *value, int kind, const char *type);\n"
    "void ZxNativeExpectLength(const zx_value *value, size_t length);\n"
    "void ZxNativeExpectFields(const zx_value *value, size_t count);\n"
    "void ZxNativeExpectSlice(const zx_value *value, size_t element_size);\n"
    "void ZxNativeDistinct(const void *a, size_t a_bytes, const void *b, size_t b_bytes);\n"
    "int ZxNativeMatchByName(void);\n"
    "const zx_value *ZxNativeField(const zx_value *value, const char *name);\n"
    "const zx_value *ZxNativeFieldAt(const zx_value *value, size_t index, const char *name);\n"
    "#endif\n";

typedef enum HostShape {
    SHAPE_SCALAR,
    SHAPE_STRING,
    SHAPE_RECORD,
    SHAPE_ARRAY,
    SHAPE_SLICE,
    SHAPE_VOID
} HostShape;

/* A type that needs its own marshalling functions: scalars, strings and
 * records. Fixed arrays and slices are written inline around them. */
typedef struct HostType {
    const ZirModule *module;
    char text[ZIR_NAME_MAX];
} HostType;

typedef struct HostGen {
    FILE *out;
    HostType *types;
    int count;
    int capacity;
    int failed;
    char *error;
    size_t error_size;
} HostGen;

static void
gen_fail(HostGen *gen, const char *format, ...)
{
    if(gen->failed)
        return;
    gen->failed = 1;
    va_list args;
    va_start(args, format);
    vsnprintf(gen->error, gen->error_size, format, args);
    va_end(args);
}

static void
copy_trimmed(char *out, size_t size, const char *begin, const char *end)
{
    while(begin < end && (*begin == ' ' || *begin == '\t' || *begin == '\n' || *begin == '\r'))
        begin++;
    while(end > begin && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'))
        end--;
    size_t length = (size_t)(end - begin);
    if(length >= size)
        length = size - 1;
    memcpy(out, begin, length);
    out[length] = '\0';
}

/* The record or enum a named type declares, or NULL for builtin types. */
static const ZirType *
declared_type(const ZirModule *module, const char *type, const ZirModule **owner)
{
    *owner = module;
    if(ScalarType(type)[0] != '\0' || type[0] == '*')
        return NULL;
    return FindType(module, type, owner);
}

static HostShape
host_shape(HostGen *gen, const ZirModule *module, const char *type)
{
    if(type[0] == '\0' || strcmp(type, "void") == 0)
        return SHAPE_VOID;
    if(ArrayElementType(type, NULL, 0, NULL))
        return SHAPE_ARRAY;
    if(SliceElementType(type, NULL, 0))
        return SHAPE_SLICE;
    if(strcmp(type, "string") == 0)
        return SHAPE_STRING;
    if(type[0] == '*' || ScalarType(type)[0] != '\0')
        return SHAPE_SCALAR;
    const ZirModule *owner;
    const ZirType *declared = declared_type(module, type, &owner);
    if(declared == NULL) {
        gen_fail(gen, "host capability type %s is not declared", type);
        return SHAPE_VOID;
    }
    if(declared->is_enum)
        return SHAPE_SCALAR;
    if(declared->is_extern || declared->is_union || declared->is_procedure_type ||
       declared->is_record_template || declared->is_owned_vec || declared->is_map) {
        gen_fail(gen, "host capabilities cannot pass %s", type);
        return SHAPE_VOID;
    }
    return SHAPE_RECORD;
}

/* The zx_value kind the VM gives a scalar of this declared type. */
static const char *
scalar_kind(const char *type)
{
    if(strcmp(type, "float32") == 0 || strcmp(type, "float64") == 0)
        return "ZX_REAL";
    if(type[0] == '*')
        return "ZX_POINTER";
    if(type[0] == 'u')
        return "ZX_UNSIGNED";
    return "ZX_INTEGER";
}

/* The C spelling of a non-array type, as C lowering writes it. */
static int
c_spelling(HostGen *gen, const ZirModule *module, const char *type, char *out,
           size_t size)
{
    if(SliceElementType(type, NULL, 0)) {
        snprintf(out, size, "Slice");
        return 1;
    }
    if(type[0] == '*') {
        char base[ZIR_NAME_MAX * 2];
        if(!c_spelling(gen, module, type + 1, base, sizeof(base)))
            return 0;
        snprintf(out, size, "%s*", base);
        return 1;
    }
    if(ScalarType(type)[0] != '\0') {
        const char *native = TargetType(type, ZIR_C);
        if(native == NULL) {
            gen_fail(gen, "no C type for %s", type);
            return 0;
        }
        snprintf(out, size, "%s", native);
        return 1;
    }
    const ZirModule *owner;
    const ZirType *declared = declared_type(module, type, &owner);
    if(declared == NULL) {
        gen_fail(gen, "host capability type %s is not declared", type);
        return 0;
    }
    NativeTypeName(owner, declared, out, size);
    return out[0] != '\0';
}

static int
type_id(HostGen *gen, const ZirModule *module, const char *type)
{
    for(int i = 0; i < gen->count; i++)
        if(gen->types[i].module == module && strcmp(gen->types[i].text, type) == 0)
            return i;
    if(gen->count == gen->capacity) {
        int capacity = gen->capacity ? gen->capacity * 2 : 32;
        HostType *grown = realloc(gen->types, (size_t)capacity * sizeof(*grown));
        if(grown == NULL) {
            gen_fail(gen, "out of memory");
            return -1;
        }
        gen->types = grown;
        gen->capacity = capacity;
    }
    gen->types[gen->count].module = module;
    snprintf(gen->types[gen->count].text, sizeof(gen->types[0].text), "%s", type);
    return gen->count++;
}

static void
string_literal(FILE *out, const char *text)
{
    fputc('"', out);
    for(const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if(*p == '"' || *p == '\\') fprintf(out, "\\%c", *p);
        else if(*p < 32 || *p >= 127) fprintf(out, "\\%03o", *p);
        else fputc(*p, out);
    }
    fputc('"', out);
}

/* A fixed array's innermost element type in C and its dimensions, so
 * "[12][4]u8" becomes "uint8_t" and "[12][4]". */
static int
array_base(HostGen *gen, const ZirModule *module, const char *type, char *base,
           size_t base_size, char *dims, size_t dims_size)
{
    char element[ZIR_NAME_MAX], inner[ZIR_NAME_MAX];
    int capacity = 0;
    size_t used = 0;
    dims[0] = '\0';
    snprintf(inner, sizeof(inner), "%s", type);
    while(ArrayElementType(inner, element, sizeof(element), &capacity)) {
        used += (size_t)snprintf(dims + used, used < dims_size ? dims_size - used : 0,
                                 "[%d]", capacity);
        snprintf(inner, sizeof(inner), "%s", element);
    }
    return c_spelling(gen, module, inner, base, base_size);
}

/* For a fixed array or slice: its element type, the C type of a pointer to
 * one element, and the element's size. Elements may be fixed arrays too. */
static int
collection_element(HostGen *gen, const ZirModule *module, const char *type,
                   HostShape shape, char *element, char *element_pointer,
                   char *element_size, int *capacity)
{
    *capacity = 0;
    if(shape == SHAPE_ARRAY)
        ArrayElementType(type, element, ZIR_NAME_MAX, capacity);
    else
        SliceElementType(type, element, ZIR_NAME_MAX);
    char base[ZIR_NAME_MAX * 2], dims[ZIR_NAME_MAX];
    if(ArrayElementType(element, NULL, 0, NULL)) {
        if(!array_base(gen, module, element, base, sizeof(base), dims, sizeof(dims)))
            return 0;
        snprintf(element_pointer, ZIR_NAME_MAX * 3, "%s (*)%s", base, dims);
        snprintf(element_size, ZIR_NAME_MAX * 3, "sizeof(%s%s)", base, dims);
        return 1;
    }
    if(!c_spelling(gen, module, element, base, sizeof(base)))
        return 0;
    snprintf(element_pointer, ZIR_NAME_MAX * 3, "%s *", base);
    snprintf(element_size, ZIR_NAME_MAX * 3, "sizeof(%s)", base);
    return 1;
}

/* Write `dest` (a zx_value *) from the C lvalue `source`. */
static void
emit_put(HostGen *gen, const ZirModule *module, const char *type,
         const char *dest, const char *source, int depth)
{
    FILE *out = gen->out;
    HostShape shape = host_shape(gen, module, type);
    if(gen->failed)
        return;
    if(shape == SHAPE_ARRAY || shape == SHAPE_SLICE) {
        char element[ZIR_NAME_MAX], element_pointer[ZIR_NAME_MAX * 3], element_size[ZIR_NAME_MAX * 3];
        int capacity;
        if(!collection_element(gen, module, type, shape, element, element_pointer,
                               element_size, &capacity))
            return;
        int pad = depth * 4;
        /* Like the VM, a zero-length array crosses with no elements. */
        if(shape == SHAPE_ARRAY && capacity == 0) {
            fprintf(out, "%*s(%s)->kind = ZX_ARRAY;\n%*s(%s)->type = ", pad, "", dest, pad, "", dest);
            string_literal(out, type);
            fputs(";\n", out);
            return;
        }
        fprintf(out, "%*s{\n", pad, "");
        if(shape == SHAPE_ARRAY)
            fprintf(out, "%*s    size_t n%d = %d;\n", pad, "", depth, capacity);
        else
            fprintf(out, "%*s    Slice s%d = %s;\n%*s    size_t n%d = (size_t)s%d.length;\n",
                    pad, "", depth, source, pad, "", depth, depth);
        fprintf(out, "%*s    zx_value *e%d = (zx_value *)ZxNativeScratch((n%d ? n%d : 1) * sizeof(zx_value));\n",
                pad, "", depth, depth, depth);
        fprintf(out, "%*s    (%s)->kind = %s;\n%*s    (%s)->type = ", pad, "", dest,
                shape == SHAPE_ARRAY ? "ZX_ARRAY" : "ZX_SLICE", pad, "", dest);
        string_literal(out, type);
        fprintf(out, ";\n%*s    (%s)->elements = e%d;\n%*s    (%s)->length = n%d;\n",
                pad, "", dest, depth, pad, "", dest, depth);
        fprintf(out, "%*s    for(size_t i%d = 0; i%d < n%d; i%d++) {\n", pad, "",
                depth, depth, depth, depth);
        char element_dest[64], element_source[ZIR_NAME_MAX * 4];
        snprintf(element_dest, sizeof(element_dest), "&e%d[i%d]", depth, depth);
        if(shape == SHAPE_ARRAY)
            snprintf(element_source, sizeof(element_source), "(%s)[i%d]", source, depth);
        else
            snprintf(element_source, sizeof(element_source), "((%s)s%d.data)[i%d]",
                     element_pointer, depth, depth);
        emit_put(gen, module, element, element_dest, element_source, depth + 2);
        fprintf(out, "%*s    }\n%*s}\n", pad, "", pad, "");
        return;
    }
    if(shape == SHAPE_VOID) {
        gen_fail(gen, "host capability argument has no type");
        return;
    }
    int id = type_id(gen, module, type);
    if(id >= 0)
        fprintf(out, "%*szx_put_%d(%s, &(%s));\n", depth * 4, "", id, dest, source);
}

/* Read the C lvalue `target` from `source` (a const zx_value *). With
 * copy_back, slice elements are written into the argument's own storage,
 * whose length the host must keep. */
static void
emit_get(HostGen *gen, const ZirModule *module, const char *type,
         const char *source, const char *target, int depth, int copy_back)
{
    FILE *out = gen->out;
    HostShape shape = host_shape(gen, module, type);
    if(gen->failed)
        return;
    if(shape == SHAPE_ARRAY || shape == SHAPE_SLICE) {
        char element[ZIR_NAME_MAX], element_pointer[ZIR_NAME_MAX * 3], element_size[ZIR_NAME_MAX * 3];
        int capacity;
        if(!collection_element(gen, module, type, shape, element, element_pointer,
                               element_size, &capacity))
            return;
        int pad = depth * 4;
        fprintf(out, "%*s{\n%*s    const zx_value *v%d = %s;\n", pad, "", pad, "", depth, source);
        fprintf(out, "%*s    ZxNativeExpect(v%d, %s, ", pad, "", depth,
                shape == SHAPE_ARRAY ? "ZX_ARRAY" : "ZX_SLICE");
        string_literal(out, type);
        fputs(");\n", out);
        if(shape == SHAPE_ARRAY)
            fprintf(out, "%*s    ZxNativeExpectLength(v%d, %d);\n", pad, "", depth, capacity);
        else if(copy_back)
            fprintf(out, "%*s    ZxNativeExpectLength(v%d, (size_t)(%s).length);\n",
                    pad, "", depth, target);
        else
            fprintf(out, "%*s    ZxNativeExpectSlice(v%d, %s);\n"
                         "%*s    (%s).data = ZxNativeKeep((v%d->length ? v%d->length : 1) * %s);\n"
                         "%*s    (%s).length = (int64_t)v%d->length;\n",
                    pad, "", depth, element_size, pad, "", target, depth, depth, element_size,
                    pad, "", target, depth);
        fprintf(out, "%*s    for(size_t i%d = 0; i%d < v%d->length; i%d++) {\n", pad, "",
                depth, depth, depth, depth);
        char element_source[64], element_target[ZIR_NAME_MAX * 4];
        snprintf(element_source, sizeof(element_source), "&v%d->elements[i%d]", depth, depth);
        if(shape == SHAPE_ARRAY)
            snprintf(element_target, sizeof(element_target), "(%s)[i%d]", target, depth);
        else
            snprintf(element_target, sizeof(element_target), "((%s)(%s).data)[i%d]",
                     element_pointer, target, depth);
        emit_get(gen, module, element, element_source, element_target, depth + 2, 0);
        fprintf(out, "%*s    }\n%*s}\n", pad, "", pad, "");
        return;
    }
    if(shape == SHAPE_VOID) {
        gen_fail(gen, "host capability result has no type");
        return;
    }
    int id = type_id(gen, module, type);
    if(id >= 0)
        fprintf(out, "%*szx_get_%d(%s, &(%s));\n", depth * 4, "", id, source, target);
}

static void
emit_scalar_functions(HostGen *gen, int id, const char *type, const char *c_type,
                      int is_string)
{
    FILE *out = gen->out;
    const char *kind = is_string ? "ZX_STRING" : scalar_kind(type);
    fprintf(out, "static void\nzx_put_%d(zx_value *out, const %s *value)\n{\n"
                 "    out->kind = %s;\n    out->type = ", id, c_type, kind);
    string_literal(out, type);
    fputs(";\n", out);
    if(is_string)
        fputs("    out->data = (const unsigned char *)value->data;\n"
              "    out->length = value->length;\n", out);
    else if(strcmp(kind, "ZX_REAL") == 0)
        fputs("    out->real = (double)*value;\n", out);
    else if(strcmp(kind, "ZX_POINTER") == 0)
        fputs("    out->pointer = (void *)*value;\n", out);
    else if(strcmp(kind, "ZX_UNSIGNED") == 0)
        fputs("    out->bits = (uint64_t)*value;\n", out);
    else
        fputs("    out->integer = (int64_t)*value;\n    out->bits = (uint64_t)out->integer;\n", out);
    fprintf(out, "}\n\nstatic void\nzx_get_%d(const zx_value *in, %s *value)\n{\n"
                 "    ZxNativeExpect(in, %s, ", id, c_type, kind);
    string_literal(out, type);
    fputs(");\n", out);
    if(is_string)
        fputs("    ZxNativeString(in, &value->data, &value->length);\n", out);
    else if(strcmp(kind, "ZX_REAL") == 0)
        fprintf(out, "    *value = (%s)in->real;\n", c_type);
    else if(strcmp(kind, "ZX_POINTER") == 0)
        fprintf(out, "    *value = (%s)in->pointer;\n", c_type);
    else if(strcmp(type, "bool") == 0)
        fputs("    *value = in->integer != 0;\n", out);
    else if(strcmp(kind, "ZX_UNSIGNED") == 0)
        fprintf(out, "    *value = (%s)in->bits;\n", c_type);
    else
        fprintf(out, "    *value = (%s)in->integer;\n", c_type);
    fputs("}\n\n", out);
}

static void
emit_record_functions(HostGen *gen, int id, const ZirModule *owner,
                      const ZirType *record, const char *type, const char *c_type)
{
    FILE *out = gen->out;
    size_t cursor = 0;
    ZirTypeField field;
    int count = 0;
    int status;
    while((status = TypeNextField(record, &cursor, &field)) == 1)
        count++;
    if(status < 0) {
        gen_fail(gen, "record %s has malformed fields", type);
        return;
    }
    fprintf(out, "static void\nzx_put_%d(zx_value *out, const %s *value)\n{\n"
                 "    zx_field *fields = (zx_field *)ZxNativeScratch(%d * sizeof(zx_field));\n"
                 "    out->kind = ZX_RECORD;\n    out->type = ",
            id, c_type, count ? count : 1);
    string_literal(out, type);
    fprintf(out, ";\n    out->fields = fields;\n    out->field_count = %d;\n", count);
    cursor = 0;
    for(int i = 0; i < count && TypeNextField(record, &cursor, &field) == 1; i++) {
        char member[ZIR_NAME_MAX * 2], dest[64], source[ZIR_NAME_MAX * 3];
        TargetFieldName(record, ZIR_C, field.name, member, sizeof(member));
        fprintf(out, "    fields[%d].name = ", i);
        string_literal(out, field.name);
        fputs(";\n", out);
        snprintf(dest, sizeof(dest), "&fields[%d].value", i);
        snprintf(source, sizeof(source), "value->%s", member);
        emit_put(gen, owner, field.type, dest, source, 1);
    }
    fprintf(out, "}\n\nstatic void\nzx_get_%d(const zx_value *in, %s *value)\n{\n"
                 "    ZxNativeExpect(in, ZX_RECORD, ", id, c_type);
    string_literal(out, type);
    fprintf(out, ");\n    memset(value, 0, sizeof(*value));\n"
                 "    int by_name = ZxNativeMatchByName();\n"
                 "    if(!by_name)\n        ZxNativeExpectFields(in, %d);\n", count);
    cursor = 0;
    for(int i = 0; i < count && TypeNextField(record, &cursor, &field) == 1; i++) {
        char member[ZIR_NAME_MAX * 2], target[ZIR_NAME_MAX * 3];
        TargetFieldName(record, ZIR_C, field.name, member, sizeof(member));
        fputs("    {\n        const zx_value *f = by_name ? ZxNativeField(in, ", out);
        string_literal(out, field.name);
        fprintf(out, ") : ZxNativeFieldAt(in, %d, ", i);
        string_literal(out, field.name);
        fputs(");\n        if(f != NULL) {\n", out);
        snprintf(target, sizeof(target), "value->%s", member);
        emit_get(gen, owner, field.type, "f", target, 3, 0);
        fputs("        }\n    }\n", out);
    }
    fputs("}\n\n", out);
}

/* Writing one type's functions can register the types of its fields, so
 * this runs until every registered type has functions. */
static void
emit_type_functions(HostGen *gen, FILE *declarations)
{
    for(int id = 0; id < gen->count && !gen->failed; id++) {
        HostType type = gen->types[id];
        char c_type[ZIR_NAME_MAX * 2];
        if(!c_spelling(gen, type.module, type.text, c_type, sizeof(c_type)))
            return;
        fprintf(declarations, "static void zx_put_%d(zx_value *, const %s *);\n"
                              "static void zx_get_%d(const zx_value *, %s *);\n",
                id, c_type, id, c_type);
        HostShape shape = host_shape(gen, type.module, type.text);
        if(shape == SHAPE_RECORD) {
            const ZirModule *owner;
            const ZirType *record = declared_type(type.module, type.text, &owner);
            emit_record_functions(gen, id, owner, record, type.text, c_type);
        } else {
            emit_scalar_functions(gen, id, type.text, c_type, shape == SHAPE_STRING);
        }
    }
}

enum { HOST_MAX_PARAMETERS = 64 };

/* Split checked "name: Type, other: Type" parameters at top-level commas. */
static int
parameter_types(const char *args, char types[][ZIR_NAME_MAX], int limit)
{
    int count = 0;
    int depth = 0;
    const char *start = args;
    for(const char *p = args;; p++) {
        if(*p == '(' || *p == '[' || *p == '{') depth++;
        else if(*p == ')' || *p == ']' || *p == '}') depth--;
        if((*p == ',' && depth == 0) || *p == '\0') {
            const char *colon = start;
            while(colon < p && *colon != ':')
                colon++;
            if(colon < p) {
                if(count < limit)
                    copy_trimmed(types[count], ZIR_NAME_MAX, colon + 1, p);
                count++;
            }
            if(*p == '\0')
                break;
            start = p + 1;
        }
    }
    return count;
}

static const ZirImport *
capability_import(const ZirProgram *program, const char *module_name,
                  const char *function, const ZirModule **owner)
{
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        if(strcmp(module->name, module_name) != 0)
            continue;
        for(int i = 0; i < module->import_count; i++)
            if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
               strcmp(module->imports[i].name, function) == 0) {
                *owner = module;
                return &module->imports[i];
            }
    }
    return NULL;
}

static void
emit_trampoline(HostGen *gen, const ZirModule *module, const ZirImport *import,
                const char *symbol, int capability)
{
    FILE *out = gen->out;
    char (*types)[ZIR_NAME_MAX] = calloc(HOST_MAX_PARAMETERS, sizeof(*types));
    if(types == NULL) {
        gen_fail(gen, "out of memory");
        return;
    }
    const char *result = import->return_type[0] ? import->return_type : "void";
    int count = parameter_types(import->args, types, HOST_MAX_PARAMETERS);
    HostShape result_shape = host_shape(gen, module, result);
    char result_c[ZIR_NAME_MAX * 2], out_base[ZIR_NAME_MAX * 2], out_dims[ZIR_NAME_MAX];
    if(count > HOST_MAX_PARAMETERS)
        gen_fail(gen, "%s takes more than %d parameters", symbol, HOST_MAX_PARAMETERS);
    /* C lowering returns a fixed array through a hidden first parameter. */
    if(result_shape == SHAPE_ARRAY) {
        array_base(gen, module, result, out_base, sizeof(out_base), out_dims, sizeof(out_dims));
        snprintf(result_c, sizeof(result_c), "void");
    } else if(!gen->failed) {
        c_spelling(gen, module, result, result_c, sizeof(result_c));
    }
    if(gen->failed) {
        free(types);
        return;
    }
    fprintf(out, "%s\n%s(", result_c, symbol);
    int written = 0;
    if(result_shape == SHAPE_ARRAY) {
        fprintf(out, "%s zx_out%s", out_base, out_dims);
        written = 1;
    }
    for(int i = 0; i < count && !gen->failed; i++) {
        char c_type[ZIR_NAME_MAX * 2], dims[ZIR_NAME_MAX];
        if(written++ > 0) fputs(", ", out);
        if(ArrayElementType(types[i], NULL, 0, NULL)) {
            if(array_base(gen, module, types[i], c_type, sizeof(c_type), dims, sizeof(dims)))
                fprintf(out, "%s p%d%s", c_type, i, dims);
        } else if(c_spelling(gen, module, types[i], c_type, sizeof(c_type))) {
            fprintf(out, "%s p%d", c_type, i);
        }
    }
    if(written == 0)
        fputs("void", out);
    fprintf(out, ")\n{\n    zx_value zx_args[%d];\n    zx_value zx_result;\n"
                 "    ZxNativeScratchReset();\n"
                 "    memset(zx_args, 0, sizeof(zx_args));\n"
                 "    memset(&zx_result, 0, sizeof(zx_result));\n"
                 "    zx_result.type = ",
            count ? count : 1);
    string_literal(out, result);
    fputs(";\n", out);
    /* Like the VM, refuse overlapping mutable slices: the host could see
     * one element through both and the copy-back order would decide it. */
    for(int i = 0; i < count; i++) {
        if(host_shape(gen, module, types[i]) != SHAPE_SLICE)
            continue;
        for(int j = i + 1; j < count; j++) {
            if(host_shape(gen, module, types[j]) != SHAPE_SLICE)
                continue;
            char element[ZIR_NAME_MAX], pointer[ZIR_NAME_MAX * 3];
            char a_size[ZIR_NAME_MAX * 3], b_size[ZIR_NAME_MAX * 3];
            int capacity;
            if(!collection_element(gen, module, types[i], SHAPE_SLICE, element, pointer,
                                   a_size, &capacity) ||
               !collection_element(gen, module, types[j], SHAPE_SLICE, element, pointer,
                                   b_size, &capacity))
                break;
            fprintf(out, "    ZxNativeDistinct(p%d.data, (size_t)p%d.length * %s, "
                         "p%d.data, (size_t)p%d.length * %s);\n",
                    i, i, a_size, j, j, b_size);
        }
    }
    for(int i = 0; i < count; i++) {
        char dest[32], source[32];
        snprintf(dest, sizeof(dest), "&zx_args[%d]", i);
        snprintf(source, sizeof(source), "p%d", i);
        emit_put(gen, module, types[i], dest, source, 1);
    }
    fprintf(out, "    ZxNativeHost(%d, zx_args, %d, &zx_result);\n", capability, count);
    for(int i = 0; i < count; i++) {
        if(host_shape(gen, module, types[i]) != SHAPE_SLICE)
            continue;
        char source[32], target[32];
        snprintf(source, sizeof(source), "&zx_args[%d]", i);
        snprintf(target, sizeof(target), "p%d", i);
        emit_get(gen, module, types[i], source, target, 1, 1);
    }
    if(result_shape == SHAPE_ARRAY) {
        emit_get(gen, module, result, "&zx_result", "zx_out", 1, 0);
    } else if(result_shape != SHAPE_VOID) {
        fprintf(out, "    %s zx_returned;\n    memset(&zx_returned, 0, sizeof(zx_returned));\n", result_c);
        emit_get(gen, module, result, "&zx_result", "zx_returned", 1, 0);
        fputs("    return zx_returned;\n", out);
    }
    fputs("}\n\n", out);
    free(types);
}

static void
header_name(const ZirModule *module, char *out, size_t size)
{
    const char *path = module->source_path;
    size_t length = strlen(path);
    if(length > 3 && strcmp(path + length - 3, ".zi") == 0)
        length -= 3;
    snprintf(out, size, "%.*s.h", (int)length, path);
}

int
ZxWriteHostTrampolines(const ZirProgram *program, ZxCapabilityName *capabilities,
                       size_t capability_count, int allow_pruned, FILE *out, char *error, size_t error_size)
{
    HostGen gen = {.error = error, .error_size = error_size};
    unsigned char *body = NULL, *declarations = NULL, *includes = NULL;
    size_t body_size = 0, declarations_size = 0, includes_size = 0;
    FILE *body_out = ZirWriteMemory(&body, &body_size);
    FILE *declarations_out = ZirWriteMemory(&declarations, &declarations_size);
    FILE *includes_out = ZirWriteMemory(&includes, &includes_size);
    const ZirModule **modules = calloc(capability_count ? capability_count : 1,
                                       sizeof(*modules));
    const ZirImport **imports = calloc(capability_count ? capability_count : 1,
                                       sizeof(*imports));
    const ZirModule **included = calloc(capability_count ? capability_count : 1,
                                        sizeof(*included));
    size_t included_count = 0;
    if(body_out == NULL || declarations_out == NULL || includes_out == NULL ||
       modules == NULL || imports == NULL || included == NULL)
        gen_fail(&gen, "out of memory");
    gen.out = body_out;
    for(size_t i = 0; i < capability_count && !gen.failed; i++) {
        imports[i] = capability_import(program, capabilities[i].module,
                                       capabilities[i].function, &modules[i]);
        if(imports[i] == NULL) {
            if(allow_pruned) continue;
            gen_fail(&gen, "capability %s:%s has no declaration", capabilities[i].module,
                     capabilities[i].function);
            break;
        }
        const ZirImport *import = imports[i];
        snprintf(capabilities[i].symbol, sizeof(capabilities[i].symbol), "%s",
                 import->extern_kind == ZIR_EXTERN_HOST &&
                 strncmp(import->target, "ziran:", 6) == 0 && import->extern_symbol[0] != '\0' ?
                 import->extern_symbol : import->name);
        capabilities[i].renamed = 0;
        int seen = 0;
        for(size_t m = 0; m < included_count; m++)
            seen |= included[m] == modules[i];
        if(!seen) {
            char header[ZIR_PATH_MAX];
            header_name(modules[i], header, sizeof(header));
            fprintf(includes_out, "#include \"%s\"\n", header);
            included[included_count++] = modules[i];
        }
    }
    for(size_t i = 0; i < capability_count && !gen.failed; i++)
        for(size_t j = 0; j < capability_count; j++)
            if(imports[i] != NULL && imports[j] != NULL && j != i && modules[j] != modules[i] &&
               strcmp(capabilities[i].symbol, capabilities[j].symbol) == 0)
                capabilities[i].renamed = 1;
    for(size_t i = 0; i < capability_count && !gen.failed; i++) {
        if(imports[i] == NULL) continue;
        /* The plain symbol serves code outside the declaring modules, bound
         * like the first declaration. */
        int first = 1;
        for(size_t j = 0; j < i; j++)
            first &= imports[j] == NULL || strcmp(capabilities[i].symbol, capabilities[j].symbol) != 0;
        if(first)
            emit_trampoline(&gen, modules[i], imports[i], capabilities[i].symbol, (int)i);
        if(capabilities[i].renamed) {
            char renamed[ZIR_NAME_MAX];
            snprintf(renamed, sizeof(renamed), "zx_capability_%zu", i);
            emit_trampoline(&gen, modules[i], imports[i], renamed, (int)i);
        }
    }
    if(!gen.failed)
        emit_type_functions(&gen, declarations_out);
    if(body_out) fclose(body_out);
    if(declarations_out) fclose(declarations_out);
    if(includes_out) fclose(includes_out);
    if(!gen.failed) {
        fputs("/* Host capability trampolines for natively compiled bundles. */\n"
              "#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n"
              "#include <string.h>\n", out);
        /* A stream nothing was written to leaves its buffer NULL. */
        fputs(includes ? (const char *)includes : "", out);
        fputs("#include \"zx_native_services.h\"\n\n", out);
        fputs(declarations ? (const char *)declarations : "", out);
        fputs("\n", out);
        fputs(body ? (const char *)body : "", out);
        if(ferror(out))
            gen_fail(&gen, "cannot write host trampolines");
    }
    free(body);
    free(declarations);
    free(includes);
    free(modules);
    free(imports);
    free(included);
    free(gen.types);
    return !gen.failed;
}
