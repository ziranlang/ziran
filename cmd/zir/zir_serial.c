#include "zir_serial.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_parse.h"
#include "zir_text.h"

#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef enum FieldKind {
    FIELD_STRING,
    FIELD_TEXT, /* const char * kept with KeepText; same encoding as a string */
    FIELD_NAME, /* const char * kept with KeepName; a string under ZIR_NAME_MAX */
    FIELD_INTEGER,
    FIELD_U64,
    FIELD_SPAN
} FieldKind;

typedef struct Field {
    size_t offset;
    size_t size;
    FieldKind kind;
} Field;

typedef struct Reader {
    FILE *in;
    const char *path;
    const char *problem;
    size_t budget;
} Reader;

#define STRING_FIELD(type, name) \
    {offsetof(type, name), sizeof(((type *)0)->name), FIELD_STRING}
#define TEXT_FIELD(type, name) \
    {offsetof(type, name), ZIR_TEXT_MAX, FIELD_TEXT}
#define NAME_FIELD(type, name) \
    {offsetof(type, name), ZIR_NAME_MAX, FIELD_NAME}
#define INTEGER_FIELD(type, name) \
    {offsetof(type, name), sizeof(((type *)0)->name), FIELD_INTEGER}
#define U64_FIELD(type, name) \
    {offsetof(type, name), sizeof(((type *)0)->name), FIELD_U64}
#define SPAN_FIELD(type, name) \
    {offsetof(type, name), sizeof(((type *)0)->name), FIELD_SPAN}
#define FIELD_COUNT(fields) (sizeof(fields) / sizeof((fields)[0]))
#define ZIR_FORMAT_VERSION 52u

static const Field import_fields[] = {
    INTEGER_FIELD(ZirImport, kind), INTEGER_FIELD(ZirImport, extern_kind),
    INTEGER_FIELD(ZirImport, is_public),
    INTEGER_FIELD(ZirImport, is_file_private), STRING_FIELD(ZirImport, name),
    STRING_FIELD(ZirImport, target), STRING_FIELD(ZirImport, extern_symbol),
    STRING_FIELD(ZirImport, signature), STRING_FIELD(ZirImport, args),
    STRING_FIELD(ZirImport, return_type), INTEGER_FIELD(ZirImport, must_use),
    INTEGER_FIELD(ZirImport, is_varargs),
    INTEGER_FIELD(ZirImport, go_results),
    INTEGER_FIELD(ZirImport, go_field),
    INTEGER_FIELD(ZirImport, go_defer),
    INTEGER_FIELD(ZirImport, is_using),
    INTEGER_FIELD(ZirImport, required),
    SPAN_FIELD(ZirImport, span)
};
static const Field statement_fields[] = {
    INTEGER_FIELD(ZirStmt, kind), TEXT_FIELD(ZirStmt, text),
    INTEGER_FIELD(ZirStmt, is_else), INTEGER_FIELD(ZirStmt, is_using),
    INTEGER_FIELD(ZirStmt, loop_id), INTEGER_FIELD(ZirStmt, target_id),
    INTEGER_FIELD(ZirStmt, for_form), INTEGER_FIELD(ZirStmt, for_step),
    INTEGER_FIELD(ZirStmt, expr_root),
    INTEGER_FIELD(ZirStmt, lhs_root), STRING_FIELD(ZirStmt, name),
    STRING_FIELD(ZirStmt, type), STRING_FIELD(ZirStmt, assignment_op),
    INTEGER_FIELD(ZirStmt, is_parallel), INTEGER_FIELD(ZirStmt, is_gpu),
    SPAN_FIELD(ZirStmt, span)
};
static const Field expression_fields[] = {
    INTEGER_FIELD(ZirExpr, kind), INTEGER_FIELD(ZirExpr, is_function_value),
    INTEGER_FIELD(ZirExpr, is_this),
    NAME_FIELD(ZirExpr, slot_type), TEXT_FIELD(ZirExpr, text),
    STRING_FIELD(ZirExpr, name), NAME_FIELD(ZirExpr, argument_name),
    INTEGER_FIELD(ZirExpr, argument_index), STRING_FIELD(ZirExpr, op),
    INTEGER_FIELD(ZirExpr, left), INTEGER_FIELD(ZirExpr, right),
    INTEGER_FIELD(ZirExpr, first_child), INTEGER_FIELD(ZirExpr, next_sibling),
    INTEGER_FIELD(ZirExpr, third), STRING_FIELD(ZirExpr, type),
    SPAN_FIELD(ZirExpr, span)
};
static const Field function_fields[] = {
    STRING_FIELD(ZirFunction, name), STRING_FIELD(ZirFunction, args),
    STRING_FIELD(ZirFunction, default_args),
    U64_FIELD(ZirFunction, using_parameters),
    STRING_FIELD(ZirFunction, return_type), INTEGER_FIELD(ZirFunction, must_use),
    INTEGER_FIELD(ZirFunction, is_conversion),
    STRING_FIELD(ZirFunction, effect_class),
    INTEGER_FIELD(ZirFunction, exported),
    STRING_FIELD(ZirFunction, export_symbol),
    INTEGER_FIELD(ZirFunction, is_extern), INTEGER_FIELD(ZirFunction, extern_kind),
    INTEGER_FIELD(ZirFunction, is_public),
    INTEGER_FIELD(ZirFunction, is_file_private),
    INTEGER_FIELD(ZirFunction, is_template),
    INTEGER_FIELD(ZirFunction, is_specialization),
    INTEGER_FIELD(ZirFunction, is_global_initializer),
    STRING_FIELD(ZirFunction, template_param),
    STRING_FIELD(ZirFunction, specialization_type),
    STRING_FIELD(ZirFunction, overload_name),
    INTEGER_FIELD(ZirFunction, checked),
    INTEGER_FIELD(ZirFunction, uses_host), STRING_FIELD(ZirFunction, extern_target),
    STRING_FIELD(ZirFunction, extern_symbol), SPAN_FIELD(ZirFunction, span)
};
static const Field global_fields[] = {
    STRING_FIELD(ZirGlobal, name), STRING_FIELD(ZirGlobal, type),
    STRING_FIELD(ZirGlobal, init), INTEGER_FIELD(ZirGlobal, is_static),
    INTEGER_FIELD(ZirGlobal, is_file_private), SPAN_FIELD(ZirGlobal, span)
};
static const Field define_fields[] = {
    STRING_FIELD(ZirDefine, name), STRING_FIELD(ZirDefine, value),
    INTEGER_FIELD(ZirDefine, is_public),
    INTEGER_FIELD(ZirDefine, is_file_private), SPAN_FIELD(ZirDefine, span)
};
static const Field assert_fields[] = {
    STRING_FIELD(ZirAssert, condition), STRING_FIELD(ZirAssert, message),
    SPAN_FIELD(ZirAssert, span)
};
static const Field law_fields[] = {
    STRING_FIELD(ZirLaw, name), STRING_FIELD(ZirLaw, kind),
    STRING_FIELD(ZirLaw, payload), SPAN_FIELD(ZirLaw, span),
    STRING_FIELD(ZirLaw, evidence.method), STRING_FIELD(ZirLaw, evidence.domain),
    U64_FIELD(ZirLaw, evidence.cases_checked),
    STRING_FIELD(ZirLaw, evidence.counterexample), INTEGER_FIELD(ZirLaw, evidence.waived)
};
static const Field proof_fields[] = {
    STRING_FIELD(ZirProof, name), STRING_FIELD(ZirProof, source), SPAN_FIELD(ZirProof, span)
};
static const Field proof_step_fields[] = {
    INTEGER_FIELD(ZirProofStep, kind), STRING_FIELD(ZirProofStep, target),
    INTEGER_FIELD(ZirProofStep, term_root), SPAN_FIELD(ZirProofStep, span)
};
static const Field law_waiver_fields[] = {
    STRING_FIELD(ZirLawWaiver, name), STRING_FIELD(ZirLawWaiver, reason),
    SPAN_FIELD(ZirLawWaiver, span)
};
static const Field type_fields[] = {
    STRING_FIELD(ZirType, name), STRING_FIELD(ZirType, body),
    STRING_FIELD(ZirType, template_params),
    STRING_FIELD(ZirType, template_name), STRING_FIELD(ZirType, template_args),
    STRING_FIELD(ZirType, foreign_target),
    INTEGER_FIELD(ZirType, is_synthetic_application),
    INTEGER_FIELD(ZirType, is_procedure_type),
    INTEGER_FIELD(ZirType, is_c_call),
    STRING_FIELD(ZirType, procedure_return_type),
    INTEGER_FIELD(ZirType, is_public),
    INTEGER_FIELD(ZirType, is_file_private),
    INTEGER_FIELD(ZirType, is_enum),
    INTEGER_FIELD(ZirType, is_union),
    INTEGER_FIELD(ZirType, is_enum_flags),
    INTEGER_FIELD(ZirType, is_enum_specified),
    STRING_FIELD(ZirType, enum_backing),
    INTEGER_FIELD(ZirType, is_record_template),
    INTEGER_FIELD(ZirType, is_owned_vec),
    INTEGER_FIELD(ZirType, is_map),
    INTEGER_FIELD(ZirType, is_extern),
    INTEGER_FIELD(ZirType, is_abi_incomplete), INTEGER_FIELD(ZirType, is_results),
    SPAN_FIELD(ZirType, span)
};

static int
enum_backing_valid(const char *backing)
{
    static const char *const allowed[] = {
        "s8", "u8", "s16", "u16", "s32", "u32", "s64", "u64"
    };
    for(size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++)
        if(strcmp(backing, allowed[i]) == 0)
            return 1;
    return 0;
}

static int
using_path_valid(const char *path, int allow_members)
{
    const unsigned char *cursor = (const unsigned char *)path;
    if(!isalpha(*cursor) && *cursor != '_') return 0;
    for(;;) {
        while(isalnum(*cursor) || *cursor == '_') cursor++;
        if(*cursor == '\0') return 1;
        if(!allow_members || *cursor++ != '.' ||
           (!isalpha(*cursor) && *cursor != '_')) return 0;
    }
}

static int
specified_enum_body_valid(const char *body)
{
    int members = 0;
    while(*body) {
        while(*body == ',' || isspace((unsigned char)*body)) body++;
        if(!*body) break;
        if(!isalpha((unsigned char)*body) && *body != '_') return 0;
        do body++;
        while(isalnum((unsigned char)*body) || *body == '_');
        while(*body == ' ' || *body == '\t') body++;
        if(*body++ != '=') return 0;
        while(*body && *body != '\n' && *body != ',') body++;
        members++;
    }
    return members > 0;
}

static int
default_signature_valid(const ZirFunction *function)
{
    if(!function->default_args[0]) return 1;
    char (*parts)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parts));
    if(parts == NULL) return 0;
    int count = split_top_level(function->default_args, parts[0], 64,
                                sizeof(parts[0]));
    char normalized[ZIR_TEXT_MAX] = "";
    size_t used = 0;
    int defaults = 0;
    int valid = count > 0;
    for(int i = 0; valid && i < count; i++) {
        char *assignment = top_level_assignment(parts[i]);
        if(assignment != NULL) {
            if(!*skip_ws(assignment + 1)) { valid = 0; break; }
            *assignment = '\0';
            trim_in_place(parts[i]);
            defaults++;
        }
        int written = snprintf(normalized + used, sizeof(normalized) - used,
                               "%s%s", i ? ", " : "", parts[i]);
        if(written < 0 || (size_t)written >= sizeof(normalized) - used) {
            valid = 0;
            break;
        }
        used += (size_t)written;
    }
    valid = valid && defaults > 0 &&
            strcmp(normalized, function->args) == 0;
    free(parts);
    return valid;
}
static const Field module_fields[] = {
    STRING_FIELD(ZirModule, name), STRING_FIELD(ZirModule, source_path),
    SPAN_FIELD(ZirModule, span)
};

static int
write_u32(FILE *out, uint32_t value)
{
    for(int i = 0; i < 4; i++) {
        if(fputc((int)(value & 255u), out) == EOF)
            return 0;
        value >>= 8;
    }
    return 1;
}

static int
read_u32(Reader *reader, uint32_t *value)
{
    uint32_t result = 0;
    for(int i = 0; i < 4; i++) {
        int byte = fgetc(reader->in);
        if(byte == EOF) {
            reader->problem = "truncated integer";
            return 0;
        }
        result |= (uint32_t)(unsigned char)byte << (8 * i);
    }
    *value = result;
    return 1;
}

static int
write_u64(FILE *out, uint64_t value)
{
    return write_u32(out, (uint32_t)value) &&
           write_u32(out, (uint32_t)(value >> 32));
}

static int
read_u64(Reader *reader, uint64_t *value)
{
    uint32_t low, high;
    if(!read_u32(reader, &low) || !read_u32(reader, &high)) return 0;
    *value = (uint64_t)low | ((uint64_t)high << 32);
    return 1;
}

static int
write_string(FILE *out, const char *value, size_t capacity)
{
    size_t length = strnlen(value, capacity);
    if(length == capacity || length > UINT32_MAX)
        return 0;
    return write_u32(out, (uint32_t)length) &&
           fwrite(value, 1, length, out) == length;
}

static int read_string_body(Reader *reader, char *value, uint32_t length);

static int
read_string(Reader *reader, char *value, size_t capacity)
{
    uint32_t length;
    if(!read_u32(reader, &length))
        return 0;
    if(length >= capacity) {
        reader->problem = "string exceeds field limit";
        return 0;
    }
    return read_string_body(reader, value, length);
}

/* The LENGTH bytes of a string whose length was just read. */
static int
read_string_body(Reader *reader, char *value, uint32_t length)
{
    if(fread(value, 1, length, reader->in) != length) {
        reader->problem = "truncated string";
        return 0;
    }
    if(memchr(value, '\0', length) != NULL) {
        reader->problem = "embedded NUL in string";
        return 0;
    }
    value[length] = '\0';
    return 1;
}

static int
write_span(FILE *out, const ZirSourceSpan *span)
{
    return write_string(out, SpanPath(*span), ZIR_PATH_MAX) &&
           write_u32(out, (uint32_t)span->line) &&
           write_u32(out, (uint32_t)span->column) &&
           write_u32(out, (uint32_t)span->end_line) &&
           write_u32(out, (uint32_t)span->end_column);
}

static int
read_span(Reader *reader, ZirSourceSpan *span)
{
    uint32_t numbers[4];
    char path[ZIR_PATH_MAX];
    if(!read_string(reader, path, sizeof(path)))
        return 0;
    span->file = SourceFile(path);
    for(int i = 0; i < 4; i++)
        if(!read_u32(reader, &numbers[i]))
            return 0;
    for(int i = 0; i < 4; i++) {
        if(numbers[i] > INT32_MAX) {
            reader->problem = "invalid source position";
            return 0;
        }
    }
    span->line = (int)numbers[0];
    span->column = (int)numbers[1];
    span->end_line = (int)numbers[2];
    span->end_column = (int)numbers[3];
    return 1;
}

static int
write_fields(FILE *out, const void *record, const Field *fields, size_t count)
{
    for(size_t i = 0; i < count; i++) {
        const char *value = (const char *)record + fields[i].offset;
        if(fields[i].kind == FIELD_STRING) {
            if(!write_string(out, value, fields[i].size))
                return 0;
        } else if(fields[i].kind == FIELD_NAME) {
            const char *name = *(const char *const *)value;
            if(!write_string(out, name ? name : "", fields[i].size))
                return 0;
        } else if(fields[i].kind == FIELD_TEXT) {
            /* Source text has no field limit: a long literal keeps it all. */
            const char *text = *(const char *const *)value;
            if(!write_string(out, text ? text : "", ZIR_KEPT_TEXT_MAX))
                return 0;
        } else if(fields[i].kind == FIELD_INTEGER) {
            if(!write_u32(out, (uint32_t)*(const int *)value))
                return 0;
        } else if(fields[i].kind == FIELD_U64) {
            if(!write_u64(out, *(const uint64_t *)value))
                return 0;
        } else if(!write_span(out, (const ZirSourceSpan *)value)) {
            return 0;
        }
    }
    return 1;
}

static int
read_fields(Reader *reader, void *record, const Field *fields, size_t count)
{
    for(size_t i = 0; i < count; i++) {
        char *value = (char *)record + fields[i].offset;
        if(fields[i].kind == FIELD_STRING) {
            if(!read_string(reader, value, fields[i].size))
                return 0;
        } else if(fields[i].kind == FIELD_NAME) {
            char name[ZIR_NAME_MAX];
            if(!read_string(reader, name, sizeof(name)))
                return 0;
            *(const char **)value = KeepName(name);
        } else if(fields[i].kind == FIELD_TEXT) {
            char small[ZIR_TEXT_MAX];
            uint32_t length;
            if(!read_u32(reader, &length))
                return 0;
            if(length >= ZIR_KEPT_TEXT_MAX) {
                reader->problem = "string exceeds field limit";
                return 0;
            }
            char *text = length < sizeof(small) ? small : malloc((size_t)length + 1);
            if(text == NULL || !read_string_body(reader, text, length)) {
                if(text != small) free(text);
                if(reader->problem == NULL) reader->problem = "out of memory reading text";
                return 0;
            }
            *(const char **)value = KeepText(text);
            if(text != small) free(text);
        } else if(fields[i].kind == FIELD_INTEGER) {
            uint32_t number;
            if(!read_u32(reader, &number))
                return 0;
            *(int *)value = (int)(int32_t)number;
        } else if(fields[i].kind == FIELD_U64) {
            if(!read_u64(reader, (uint64_t *)value))
                return 0;
        } else if(!read_span(reader, (ZirSourceSpan *)value)) {
            return 0;
        }
    }
    return 1;
}

static int
write_records(FILE *out, const void *records, int count, size_t size,
              const Field *fields, size_t field_count)
{
    if(count < 0 || (count > 0 && records == NULL) ||
       !write_u32(out, (uint32_t)count))
        return 0;
    for(int i = 0; i < count; i++)
        if(!write_fields(out, (const char *)records + (size_t)i * size,
                         fields, field_count))
            return 0;
    return 1;
}

static void *
read_records(Reader *reader, int *count, size_t size,
             const Field *fields, size_t field_count)
{
    uint32_t length;
    if(!read_u32(reader, &length))
        return NULL;
    if(length > 100000 || size == 0 || length > reader->budget / size) {
        reader->problem = "record count exceeds IR limit";
        return NULL;
    }
    reader->budget -= (size_t)length * size;
    *count = (int)length;
    if(length == 0)
        return NULL;
    void *records = calloc(length, size);
    if(records == NULL) {
        reader->problem = "out of memory reading IR";
        return NULL;
    }
    for(uint32_t i = 0; i < length; i++) {
        if(!read_fields(reader, (char *)records + (size_t)i * size,
                        fields, field_count)) {
            free(records);
            return NULL;
        }
    }
    return records;
}

/* Arrays whose count names are not formed by adding _count to the pointer. */
#define WRITE_ARRAY(out, owner, pointer, count, fields) \
    write_records((out), (owner)->pointer, (owner)->count, \
                  sizeof(*(owner)->pointer), (fields), FIELD_COUNT(fields))
#define READ_ARRAY(reader, owner, pointer, count, fields) do { \
    (owner)->pointer = read_records((reader), &(owner)->count, \
                                     sizeof(*(owner)->pointer), (fields), \
                                     FIELD_COUNT(fields)); \
    if((reader)->problem != NULL) return 0; \
} while(0)

static int
write_function(FILE *out, const ZirFunction *function)
{
    if(!write_fields(out, function, function_fields, FIELD_COUNT(function_fields)))
        return 0;
    return WRITE_ARRAY(out, function, stmts, stmt_count, statement_fields) &&
           WRITE_ARRAY(out, function, exprs, expr_count, expression_fields);
}

static int
read_function(Reader *reader, ZirFunction *function)
{
    if(!read_fields(reader, function, function_fields, FIELD_COUNT(function_fields)))
        return 0;
    READ_ARRAY(reader, function, stmts, stmt_count, statement_fields);
    READ_ARRAY(reader, function, exprs, expr_count, expression_fields);
    function->stmt_cap = function->stmt_count;
    function->expr_cap = function->expr_count;
    function->from_ir = 1;
    return 1;
}

static int
write_law_graphs(FILE *out, const ZirModule *module)
{
    for(int l = 0; l < module->law_count; l++) {
        const ZirFunction *claim = module->laws[l].claim;
        if(!write_u32(out, claim != NULL) || (claim && !write_function(out, claim))) return 0;
    }
    if(!write_u32(out, (uint32_t)module->proof_count)) return 0;
    for(int p = 0; p < module->proof_count; p++) {
        const ZirProof *proof = &module->proofs[p];
        if(!write_fields(out, proof, proof_fields, FIELD_COUNT(proof_fields)) ||
           !write_function(out, &proof->terms) ||
           !WRITE_ARRAY(out, proof, steps, step_count, proof_step_fields)) return 0;
    }
    return 1;
}

static int
read_law_graphs(Reader *reader, ZirModule *module)
{
    for(int l = 0; l < module->law_count; l++) {
        uint32_t has;
        if(!read_u32(reader, &has) || has > 1) return 0;
        if(has) {
            if(reader->budget < sizeof(ZirFunction)) return 0;
            reader->budget -= sizeof(ZirFunction);
            module->laws[l].claim = calloc(1, sizeof(ZirFunction));
            if(!module->laws[l].claim || !read_function(reader, module->laws[l].claim)) return 0;
        }
    }
    uint32_t count;
    if(!read_u32(reader, &count) || count > 100000 ||
       count > reader->budget / sizeof(ZirProof)) return 0;
    reader->budget -= (size_t)count * sizeof(ZirProof);
    module->proofs = calloc(count ? count : 1, sizeof(ZirProof));
    if(!module->proofs) return 0;
    module->proof_count = module->proof_cap = (int)count;
    for(uint32_t p = 0; p < count; p++) {
        ZirProof *proof = &module->proofs[p];
        if(!read_fields(reader, proof, proof_fields, FIELD_COUNT(proof_fields)) ||
           !read_function(reader, &proof->terms)) return 0;
        READ_ARRAY(reader, proof, steps, step_count, proof_step_fields);
    }
    return 1;
}

static int
write_module(FILE *out, const ZirModule *module)
{
    if(!write_fields(out, module, module_fields, FIELD_COUNT(module_fields)) ||
       !WRITE_ARRAY(out, module, globals, global_count, global_fields) ||
       !WRITE_ARRAY(out, module, defines, define_count, define_fields) ||
       !WRITE_ARRAY(out, module, asserts, assert_count, assert_fields) ||
       !WRITE_ARRAY(out, module, laws, law_count, law_fields) ||
       !write_law_graphs(out, module) ||
       !WRITE_ARRAY(out, module, law_waivers, law_waiver_count,
                    law_waiver_fields) ||
       !WRITE_ARRAY(out, module, types, type_count, type_fields) ||
       !WRITE_ARRAY(out, module, imports, import_count, import_fields) ||
       !write_u32(out, (uint32_t)module->function_count))
        return 0;
    for(int i = 0; i < module->function_count; i++)
        if(!write_function(out, &module->functions[i]))
            return 0;
    return 1;
}

static int
read_module(Reader *reader, ZirModule *module)
{
    uint32_t count;
    if(!read_fields(reader, module, module_fields, FIELD_COUNT(module_fields)))
        return 0;
    READ_ARRAY(reader, module, globals, global_count, global_fields);
    READ_ARRAY(reader, module, defines, define_count, define_fields);
    READ_ARRAY(reader, module, asserts, assert_count, assert_fields);
    READ_ARRAY(reader, module, laws, law_count, law_fields);
    if(!read_law_graphs(reader, module)) {
        reader->problem = "invalid law or proof graph";
        return 0;
    }
    READ_ARRAY(reader, module, law_waivers, law_waiver_count,
               law_waiver_fields);
    READ_ARRAY(reader, module, types, type_count, type_fields);
    READ_ARRAY(reader, module, imports, import_count, import_fields);
    if(!read_u32(reader, &count))
        return 0;
    if(count > 100000 || count > reader->budget / sizeof(ZirFunction)) {
        reader->problem = "function count exceeds IR limit";
        return 0;
    }
    reader->budget -= (size_t)count * sizeof(ZirFunction);
    module->function_count = module->function_cap = (int)count;
    module->functions = calloc(count ? count : 1, sizeof(ZirFunction));
    if(module->functions == NULL) {
        reader->problem = "out of memory reading functions";
        return 0;
    }
    for(uint32_t i = 0; i < count; i++)
        if(!read_function(reader, &module->functions[i]))
            return 0;
    module->global_cap = module->global_count;
    module->define_cap = module->define_count;
    module->assert_cap = module->assert_count;
    module->type_cap = module->type_count;
    module->import_cap = module->import_count;
    return 1;
}

static int
reference_valid(int index, int count)
{
    return index >= -1 && index < count;
}

static int
export_symbol_valid(const char *symbol)
{
    if(!symbol[0])
        return 1;
    if(!isalpha((unsigned char)symbol[0]) && symbol[0] != '_')
        return 0;
    for(const unsigned char *cursor = (const unsigned char *)symbol + 1;
        *cursor; cursor++)
        if(!isalnum(*cursor) && *cursor != '_')
            return 0;
    return 1;
}

static int
validate_program(const ZirProgram *program)
{
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        if(!module->name[0] || !module->source_path[0])
            return 0;
        for(int i = 0; i < module->import_count; i++)
            if(module->imports[i].kind < ZIR_IMPORT_OPEN ||
               module->imports[i].kind > ZIR_IMPORT_EXTERN ||
               (module->imports[i].go_results != 0 && module->imports[i].go_results != 1) ||
               (module->imports[i].go_field != 0 && module->imports[i].go_field != 1) ||
               (module->imports[i].go_defer != 0 && module->imports[i].go_defer != 1) ||
               (module->imports[i].go_defer &&
                (module->imports[i].kind != ZIR_IMPORT_EXTERN ||
                 module->imports[i].extern_kind != ZIR_EXTERN_GO ||
                 module->imports[i].go_results || module->imports[i].go_field ||
                 strncmp(module->imports[i].target, "go:", 3))) ||
               (module->imports[i].go_field &&
                (module->imports[i].kind != ZIR_IMPORT_EXTERN ||
                 module->imports[i].extern_kind != ZIR_EXTERN_GO ||
                 module->imports[i].go_results ||
                 strncmp(module->imports[i].target, "go:", 3))) ||
               (module->imports[i].go_results &&
                (module->imports[i].kind != ZIR_IMPORT_EXTERN ||
                 module->imports[i].extern_kind != ZIR_EXTERN_GO ||
                 strncmp(module->imports[i].target, "go:", 3))) ||
               strncmp(module->imports[i].signature, "c-header:", 9) == 0 ||
               (module->imports[i].must_use != 0 &&
                module->imports[i].must_use != 1) ||
               (module->imports[i].must_use &&
                (module->imports[i].kind != ZIR_IMPORT_EXTERN ||
                 !strcmp(module->imports[i].return_type, "void"))) ||
               (module->imports[i].is_file_private != 0 &&
                module->imports[i].is_file_private != 1) ||
               (module->imports[i].is_file_private &&
                module->imports[i].is_public))
                return 0;
        for(int g = 0; g < module->global_count; g++)
            if((module->globals[g].is_file_private != 0 &&
                module->globals[g].is_file_private != 1) ||
               (module->globals[g].is_file_private &&
                !module->globals[g].is_static))
                return 0;
        for(int d = 0; d < module->define_count; d++)
            if((module->defines[d].is_file_private != 0 &&
                module->defines[d].is_file_private != 1) ||
               (module->defines[d].is_file_private &&
                module->defines[d].is_public))
                return 0;
        for(int t = 0; t < module->type_count; t++) {
            const ZirType *type = &module->types[t];
            if((type->is_map != 0 && type->is_map != 1) ||
               (type->is_map &&
                (!MapTypeParts(module, type->name, NULL, 0, NULL, 0) ||
                 type->is_owned_vec || type->is_abi_incomplete || type->foreign_target[0])))
                return 0;
            if(type->foreign_target[0] &&
               (!GoForeignTargetValid(type->foreign_target) || !type->is_extern ||
                type->body[0] || type->is_enum || type->is_union ||
                type->is_procedure_type || type->is_record_template ||
                type->is_synthetic_application || type->is_owned_vec ||
                type->is_abi_incomplete))
                return 0;
            if((type->is_file_private != 0 && type->is_file_private != 1) ||
               (type->is_file_private && type->is_public) ||
               (type->is_procedure_type &&
                !type->procedure_return_type[0]) ||
               (type->is_c_call != 0 && type->is_c_call != 1) ||
               (type->is_c_call && !type->is_procedure_type) ||
               (!type->is_procedure_type &&
                type->procedure_return_type[0]) ||
               type->name[0] == '#' ||
               (type->is_union != 0 && type->is_union != 1) ||
               (type->is_union &&
                (type->is_enum || type->is_procedure_type || type->is_extern ||
                 type->is_owned_vec)) ||
               (type->is_enum_flags != 0 && type->is_enum_flags != 1) ||
               (type->is_enum_specified != 0 &&
                type->is_enum_specified != 1) ||
               (type->is_enum && !enum_backing_valid(type->enum_backing)) ||
               (!type->is_enum &&
                (type->is_enum_flags || type->is_enum_specified ||
                 type->enum_backing[0])) ||
               (type->is_enum_specified &&
                !specified_enum_body_valid(type->body)) ||
               (type->is_record_template != 0 &&
                type->is_record_template != 1) ||
               (type->is_owned_vec != 0 && type->is_owned_vec != 1) ||
               (type->is_owned_vec &&
                !VecElementType(module, type->name, NULL, 0)) ||
               (type->is_abi_incomplete != 0 &&
                type->is_abi_incomplete != 1) ||
               (type->is_abi_incomplete &&
                (type->is_enum || type->is_procedure_type ||
                 type->is_record_template || type->is_extern ||
                 type->is_owned_vec)) ||
               type->is_type_instance ||
               (type->is_synthetic_application != 0 &&
                type->is_synthetic_application != 1) ||
               (type->is_synthetic_application &&
                (type->is_record_template || type->is_enum ||
                 type->is_procedure_type || !type->template_name[0] ||
                 !type->template_args[0])) ||
               (!type->is_synthetic_application &&
                (type->template_name[0] || type->template_args[0])) ||
               (type->is_record_template &&
                (type->is_enum || type->is_procedure_type || type->is_extern ||
                 !type->template_params[0] || !type->body[0])) ||
               (!type->is_record_template &&
                type->template_params[0]))
                return 0;
        }
        for(int f = 0; f < module->function_count; f++) {
            const ZirFunction *function = &module->functions[f];
            if(!function->name[0] ||
               !default_signature_valid(function) ||
               !export_symbol_valid(function->export_symbol) ||
               (function->export_symbol[0] && !function->exported) ||
               (function->must_use != 0 && function->must_use != 1) ||
               (function->must_use &&
                !strcmp(function->return_type, "void")) ||
               (function->is_file_private != 0 &&
                function->is_file_private != 1) ||
               (function->is_file_private && function->is_public) ||
               (function->is_template != 0 && function->is_template != 1) ||
               (function->using_parameters && !function->is_template) ||
               (function->is_specialization != 0 &&
                function->is_specialization != 1) ||
               (function->is_global_initializer != 0 &&
                function->is_global_initializer != 1) ||
               (function->is_global_initializer &&
                (function->exported || function->is_public ||
                 function->is_extern || function->is_template ||
                 function->is_specialization ||
                 !function->is_file_private || function->args[0] ||
                 strcmp(function->return_type, "void") != 0 ||
                 function->stmt_count == 0)) ||
               (function->is_template &&
                (function->is_specialization || function->is_extern ||
                 function->exported || !function->template_param[0] ||
                 function->specialization_type[0])) ||
               (function->is_specialization &&
                (function->is_extern || !function->template_param[0] ||
                 !function->specialization_type[0] ||
                 strchr(function->specialization_type, '$') != NULL)) ||
               (!function->is_template && !function->is_specialization &&
                (function->template_param[0] ||
                 function->specialization_type[0])))
                return 0;
            for(int s = 0; s < function->stmt_count; s++) {
                const ZirStmt *statement = &function->stmts[s];
                if(statement->kind <= ZIR_STMT_UNKNOWN ||
                   statement->kind > ZIR_STMT_IF_CASE ||
                   statement->kind == ZIR_STMT_IF_CASE ||
                   (statement->is_else != 0 && statement->is_else != 1) ||
                   (statement->is_using != 0 && statement->is_using != 1) ||
                   (statement->is_using &&
                    (!function->is_template ||
                     (statement->kind != ZIR_STMT_DECL &&
                      statement->kind != ZIR_STMT_EXPR) ||
                     !using_path_valid(statement->name,
                                       statement->kind == ZIR_STMT_EXPR))) ||
                   (statement->is_else && statement->kind != ZIR_STMT_IF) ||
                   statement->loop_id < 0 || statement->target_id < 0 ||
                   (statement->loop_id && statement->kind != ZIR_STMT_WHILE) ||
                   statement->for_form < 0 || statement->for_form > 2 ||
                   (statement->for_form && !statement->loop_id) ||
                   statement->for_step < 0 ||
                   (statement->for_step && statement->kind != ZIR_STMT_ASSIGN) ||
                   (statement->target_id &&
                    statement->kind != ZIR_STMT_BREAK &&
                    statement->kind != ZIR_STMT_CONTINUE) ||
                   !reference_valid(statement->expr_root, function->expr_count) ||
                   !reference_valid(statement->lhs_root, function->expr_count))
                    return 0;
            }
            for(int e = 0; e < function->expr_count; e++) {
                const ZirExpr *expression = &function->exprs[e];
                if(expression->kind <= ZIR_EXPR_UNKNOWN ||
                   expression->kind > ZIR_EXPR_COMPILE_TIME ||
                   (expression->kind == ZIR_EXPR_COMPILE_TIME &&
                    (strcmp(expression->text, "#compile_time") != 0 ||
                     strcmp(expression->type, "bool") != 0 ||
                     expression->name[0] || expression->op[0] ||
                     expression->left != -1 || expression->right != -1 ||
                     expression->third != -1 ||
                     expression->first_child != -1)) ||
                   (expression->is_this != 0 && expression->is_this != 1) ||
                   (expression->is_this &&
                    expression->kind != ZIR_EXPR_IDENT &&
                    expression->kind != ZIR_EXPR_CALL) ||
                   !reference_valid(expression->left, function->expr_count) ||
                   !reference_valid(expression->right, function->expr_count) ||
                   !reference_valid(expression->first_child, function->expr_count) ||
                   !reference_valid(expression->next_sibling, function->expr_count) ||
                   !reference_valid(expression->third, function->expr_count) ||
                   expression->left >= e || expression->right >= e ||
                   expression->first_child >= e || expression->third >= e ||
                   expression->argument_index < -1 ||
                   expression->argument_index >= 64 ||
                   (expression->next_sibling >= 0 &&
                    expression->next_sibling <= e))
                    return 0;
                for(int child = expression->first_child; child >= 0;
                    child = function->exprs[child].next_sibling)
                    if(child >= e)
                        return 0;
            }
        }
    }
    return 1;
}

int
ProgramWrite(const ZirProgram *program, FILE *out)
{
    static const unsigned char magic[4] = {'Z', 'I', 'R', 0};
    if(program == NULL || out == NULL || !validate_program(program) ||
       fwrite(magic, 1, sizeof(magic), out) != sizeof(magic) ||
       !write_u32(out, ZIR_FORMAT_VERSION) ||
       !write_u32(out, (uint32_t)program->module_count))
        return 0;
    for(int i = 0; i < program->module_count; i++)
        if(!write_module(out, &program->modules[i]))
            return 0;
    return fflush(out) == 0;
}

ZirProgram *
ProgramRead(FILE *in, const char *path)
{
    unsigned char magic[4];
    uint32_t version, count;
    Reader reader = {in, path, NULL, 256u * 1024u * 1024u};
    ZirProgram *program = NULL;
    if(in == NULL || path == NULL)
        return NULL;
    if(fread(magic, 1, sizeof(magic), in) != sizeof(magic) ||
       memcmp(magic, "ZIR\0", sizeof(magic)) != 0) {
        reader.problem = "invalid ZIR signature";
        goto failed;
    }
    if(!read_u32(&reader, &version))
        goto failed;
    if(version != ZIR_FORMAT_VERSION) {
        reader.problem = "unsupported ZIR version";
        goto failed;
    }
    if(!read_u32(&reader, &count))
        goto failed;
    if(count == 0 || count > 100000 || count > reader.budget / sizeof(ZirModule)) {
        reader.problem = "invalid module count";
        goto failed;
    }
    reader.budget -= (size_t)count * sizeof(ZirModule);
    program = ProgramNew();
    if(program == NULL) {
        reader.problem = "out of memory reading IR";
        goto failed;
    }
    program->module_count = program->module_cap = (int)count;
    program->modules = calloc(count, sizeof(ZirModule));
    if(program->modules == NULL) {
        reader.problem = "out of memory reading modules";
        goto failed;
    }
    for(uint32_t i = 0; i < count; i++)
        if(!read_module(&reader, &program->modules[i]))
            goto failed;
    if(fgetc(in) != EOF || ferror(in)) {
        reader.problem = "trailing or unreadable ZIR data";
        goto failed;
    }
    if(!validate_program(program)) {
        reader.problem = "invalid checked IR structure";
        goto failed;
    }
    return program;
failed:
    Diagnostic(Span(path, 1, 1), "zir.invalid", "%s",
                  reader.problem ? reader.problem : "truncated ZIR data");
    ProgramFree(program);
    return NULL;
}

int
PathIsIR(const char *path)
{
    size_t length = path ? strlen(path) : 0;
    return length > 4 && strcmp(path + length - 4, ".zir") == 0;
}

ZirProgram *
ProgramLoad(const char *path, const char *root)
{
    ZirProgram *program;
    FILE *file;
    size_t length = path ? strlen(path) : 0;
    if(!PathIsIR(path) &&
       !(length > 3 && strcmp(path + length - 3, ".zi") == 0)) {
        Diagnostic(Span(path, 1, 1), "module.extension",
                   "module input must be .zi source or .zir IR");
        return NULL;
    }
    if(!PathIsIR(path))
        return parse_file(path, root);
    file = fopen(path, "rb");
    if(file == NULL) {
        Diagnostic(Span(path, 1, 1), "zir.input", "cannot open IR input");
        return NULL;
    }
    program = ProgramRead(file, path);
    fclose(file);
    return program;
}
/* Buffers CheckCanonicalPrograms keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CheckCanonicalProgramsBuffers {
    unsigned char left[8192];
    unsigned char right[8192];
} CheckCanonicalProgramsBuffers;

int CheckCanonicalPrograms(ZirProgram **programs, int count,
                       const char *const *input_paths);

static int
CheckCanonicalPrograms_with_buffers(ZirProgram **programs, int count,
                       const char *const *input_paths, CheckCanonicalProgramsBuffers *buffers)
{
    FILE *before = NULL;
    FILE *after = NULL;
    int saved_count = 0;
    int first_saved = -1;
    int valid = 0;
    int **original_counts = NULL;
    for(int i = 0; i < count; i++) {
        if(input_paths == NULL || PathIsIR(input_paths[i])) {
            if(first_saved < 0)
                first_saved = i;
            saved_count++;
        }
    }
    if(saved_count == 0)
        return CheckPrograms(programs, count);
    original_counts = calloc((size_t)count, sizeof(*original_counts));
    if(original_counts == NULL) goto failed;
    for(int i = 0; i < count; i++) {
        if(input_paths != NULL && !PathIsIR(input_paths[i])) continue;
        int modules = programs[i]->module_count;
        original_counts[i] = malloc((size_t)modules * sizeof(int));
        if(original_counts[i] == NULL) goto failed;
        for(int m = 0; m < modules; m++)
            original_counts[i][m] = programs[i]->modules[m].function_count;
    }
    /* Saved IR is an executable typed artifact. Require checked statement
     * and expression graphs before target emission. */
    before = tmpfile();
    after = tmpfile();
    if(before == NULL || after == NULL)
        goto failed;
    for(int i = 0; i < count; i++)
        if((input_paths == NULL || PathIsIR(input_paths[i])) &&
           !ProgramWrite(programs[i], before))
            goto failed;
    if(!CheckPrograms(programs, count))
        goto done;
    for(int i = 0; i < count; i++) {
        if(original_counts[i] == NULL) continue;
        int modules = programs[i]->module_count;
        int *checked_counts = malloc((size_t)modules * sizeof(int));
        if(checked_counts == NULL) goto failed;
        for(int m = 0; m < modules; m++) {
            checked_counts[m] = programs[i]->modules[m].function_count;
            programs[i]->modules[m].function_count = original_counts[i][m];
        }
        int written = ProgramWrite(programs[i], after);
        for(int m = 0; m < modules; m++)
            programs[i]->modules[m].function_count = checked_counts[m];
        free(checked_counts);
        if(!written) goto failed;
    }
    if(fseek(before, 0, SEEK_SET) || fseek(after, 0, SEEK_SET))
        goto failed;
    for(;;) {
        size_t left_count = fread(buffers->left, 1, sizeof(buffers->left), before);
        size_t right_count = fread(buffers->right, 1, sizeof(buffers->right), after);
        if(left_count != right_count ||
           memcmp(buffers->left, buffers->right, left_count) != 0) {
            Diagnostic(Span(input_paths != NULL ? input_paths[first_saved] : "<bundle>",
                            1, 1), "zir.noncanonical",
                       "saved IR does not match the checked program");
            goto done;
        }
        if(left_count == 0) {
            if(ferror(before) || ferror(after))
                goto failed;
            valid = 1;
            break;
        }
    }
    goto done;
failed:
    Diagnostic(Span(input_paths != NULL && first_saved >= 0 ?
                    input_paths[first_saved] : "<bundle>", 1, 1),
               "zir.validation", "cannot validate saved IR");
done:
    if(original_counts != NULL) {
        for(int i = 0; i < count; i++) free(original_counts[i]);
        free(original_counts);
    }
    if(before != NULL)
        fclose(before);
    if(after != NULL)
        fclose(after);
    return valid;
}

int
CheckCanonicalPrograms(ZirProgram **programs, int count,
                       const char *const *input_paths)
{
    static _Thread_local CheckCanonicalProgramsBuffers *spares[16];
    static _Thread_local int spare_count;
    CheckCanonicalProgramsBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = CheckCanonicalPrograms_with_buffers(programs, count, input_paths, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
