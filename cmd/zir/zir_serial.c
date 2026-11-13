#include "zir_scratch.h"
#include "zir_memory.h"
#include "zir_stream.h"
#include "zir_serial.h"
#include "zir_check.h"
#include "zir_native.h"
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
    FIELD_NAME, /* const char * kept with KeepName, or KeepParameters when the
                 * limit is ZIR_TEXT_MAX; a string under that limit */
    FIELD_INTEGER,
    FIELD_STATEMENT_KIND, /* high flag bit announces optional block metadata */
    FIELD_STATEMENT_NAME, /* shares storage with optional block annotation */
    FIELD_STATEMENT_TEXT, /* portable output normalizes native blocks to { */
    FIELD_FUNCTION_ASSEMBLY, /* procedure variants; hosts decide whether they run */
    FIELD_ASSEMBLY_TEXT,
    /* An expression's text: a slice of one of its function's statements,
     * found from the two spans, or the text itself when no statement has it. */
    FIELD_EXPRESSION_TEXT,
    FIELD_U64,
    FIELD_SPAN
} FieldKind;

typedef struct Field {
    size_t offset;
    size_t size;
    FieldKind kind;
} Field;

/* IR moves through a private block buffer. Every stdio call locks its
 * stream, and Android hosts read and rewrite each bundle's IR at launch, so
 * one call per field cost seconds on slow phones. */
enum { SERIAL_BLOCK = 64 * 1024 };

/* A string written once; later uses name its first-use index. A reader
 * keeps each name or text string once, however often it is used. */
typedef struct SerialString {
    const char *text;
    uint32_t length;
    const char *kept;
} SerialString;

/* The statement text an expression was last located in: its length and the
 * last line reached, so a long statement is measured and walked once. */
typedef struct StatementCursor {
    const char *text;
    size_t length;
    int base_line;
    int line;
    size_t at;
} StatementCursor;

typedef struct Reader {
    FILE *in;
    const char *path;
    const char *problem;
    size_t budget;
    unsigned char *block;
    size_t start, end;
    /* The last span's file: one function's spans name the same path. */
    int last_file;
    char last_path[ZIR_PATH_MAX];
    /* Strings read so far, in first-use order, and the previous span. */
    SerialString *strings;
    uint32_t string_count, string_capacity;
    int last_line;
    int has_span;
    /* The context selected for the whole read; spans and modules record it. */
    CompilerContext *context;
    /* The function whose expressions are being read, and the statement the
     * last expression's text came from. */
    const ZirFunction *function;
    int last_owner;
    int64_t last_bias;
    StatementCursor cursor;
} Reader;

typedef struct Writer {
    FILE *out;
    unsigned char *block;
    size_t used;
    size_t bytes;
    int portable, measure;
    /* Strings written so far, found through an open-addressed hash of
     * first-use indexes plus one; and the previous span. */
    SerialString *strings;
    uint32_t string_count, string_capacity;
    uint32_t *slots;
    uint32_t slot_count;
    /* Addresses written so far, hashed, each with its string's index plus
     * one: kept spellings recur by address, and the written program does not
     * change, so a known address needs no length scan, hash or comparison. */
    uintptr_t *addresses;
    uint32_t *address_strings;
    uint32_t address_count, address_slot_count;
    /* Bytes a reader will charge against ZIR_READ_BUDGET for what is
     * written so far, and why writing stopped, if it did. */
    size_t charged;
    const char *problem;
    int last_line;
    const char *last_path;
    int has_span;
    /* The function whose expressions are being written, the statement whose
     * tree holds each expression, or -1, and the last statement referenced. */
    const ZirFunction *function;
    const int *owners;
    int last_owner;
    int64_t last_bias;
    StatementCursor cursor;
} Writer;

static int
read_bytes(Reader *reader, void *output, size_t count)
{
    unsigned char *to = output;
    while(count > 0) {
        if(reader->start == reader->end) {
            if(count >= SERIAL_BLOCK)
                return fread(to, 1, count, reader->in) == count;
            reader->start = 0;
            reader->end = fread(reader->block, 1, SERIAL_BLOCK, reader->in);
            if(reader->end == 0)
                return 0;
        }
        size_t amount = reader->end - reader->start;
        if(amount > count)
            amount = count;
        memcpy(to, reader->block + reader->start, amount);
        reader->start += amount;
        to += amount;
        count -= amount;
    }
    return 1;
}

/* One byte from the block, refilling it as needed. */
static int
read_byte(Reader *reader, unsigned char *byte)
{
    if(reader->start == reader->end) {
        reader->start = 0;
        reader->end = fread(reader->block, 1, SERIAL_BLOCK, reader->in);
        if(reader->end == 0)
            return 0;
    }
    *byte = reader->block[reader->start++];
    return 1;
}

static int
flush_writer(Writer *writer)
{
    size_t used = writer->used;
    writer->used = 0;
    if(used > SIZE_MAX - writer->bytes ||
       (used != 0 && fwrite(writer->block, 1, used, writer->out) != used))
        return 0;
    writer->bytes += used;
    return 1;
}

static int
write_bytes(Writer *writer, const void *data, size_t count)
{
    if(writer->measure) {
        if(count > SIZE_MAX - writer->bytes) return 0;
        writer->bytes += count;
        return 1;
    }
    if(count > SERIAL_BLOCK - writer->used) {
        if(!flush_writer(writer))
            return 0;
        if(count >= SERIAL_BLOCK) {
            if(count > SIZE_MAX - writer->bytes ||
               fwrite(data, 1, count, writer->out) != count)
                return 0;
            writer->bytes += count;
            return 1;
        }
    }
    memcpy(writer->block + writer->used, data, count);
    writer->used += count;
    return 1;
}

#define STRING_FIELD(type, name) \
    {offsetof(type, name), sizeof(((type *)0)->name), FIELD_STRING}
#define TEXT_FIELD(type, name) \
    {offsetof(type, name), ZIR_TEXT_MAX, FIELD_TEXT}
#define NAME_FIELD(type, name) \
    {offsetof(type, name), ZIR_NAME_MAX, FIELD_NAME}
#define PARAMETERS_FIELD(type, name) \
    {offsetof(type, name), ZIR_TEXT_MAX, FIELD_NAME}
#define INTEGER_FIELD(type, name) \
    {offsetof(type, name), sizeof(((type *)0)->name), FIELD_INTEGER}
#define U64_FIELD(type, name) \
    {offsetof(type, name), sizeof(((type *)0)->name), FIELD_U64}
#define SPAN_FIELD(type, name) \
    {offsetof(type, name), sizeof(((type *)0)->name), FIELD_SPAN}
#define FIELD_COUNT(fields) (sizeof(fields) / sizeof((fields)[0]))
#define ZIR_FORMAT_VERSION 64u
/* Memory a reader may spend on one program's records and on the text its
 * strings stand for. Writers charge the same, so every program written can
 * be read back; this budget, not a count or length cap, bounds saved IR. */
#define ZIR_READ_BUDGET (256u * 1024u * 1024u)
#define ZIR_READ_BUDGET_PROBLEM "checked IR exceeds the 256 MiB IR read budget"
enum { ASSEMBLY_BLOCK_FLAG = 1u << 30 };

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
    INTEGER_FIELD(ZirImport, go_variadic),
    INTEGER_FIELD(ZirImport, reads_arguments),
    INTEGER_FIELD(ZirImport, reserved_1),
    INTEGER_FIELD(ZirImport, is_using),
    INTEGER_FIELD(ZirImport, required),
    SPAN_FIELD(ZirImport, span)
};
static const Field statement_fields[] = {
    {offsetof(ZirStmt, kind), sizeof(ZirStmtKind), FIELD_STATEMENT_KIND},
    {offsetof(ZirStmt, text), ZIR_TEXT_MAX, FIELD_STATEMENT_TEXT},
    {offsetof(ZirStmt, assembly_text), ZIR_TEXT_MAX, FIELD_ASSEMBLY_TEXT},
    INTEGER_FIELD(ZirStmt, is_else), INTEGER_FIELD(ZirStmt, is_using),
    INTEGER_FIELD(ZirStmt, loop_id), INTEGER_FIELD(ZirStmt, target_id),
    INTEGER_FIELD(ZirStmt, for_form), INTEGER_FIELD(ZirStmt, for_step),
    INTEGER_FIELD(ZirStmt, expr_root),
    INTEGER_FIELD(ZirStmt, lhs_root),
    {offsetof(ZirStmt, name), ZIR_NAME_MAX, FIELD_STATEMENT_NAME},
    NAME_FIELD(ZirStmt, type), STRING_FIELD(ZirStmt, assignment_op),
    INTEGER_FIELD(ZirStmt, is_parallel), INTEGER_FIELD(ZirStmt, is_gpu),
    SPAN_FIELD(ZirStmt, span)
};
static const Field expression_fields[] = {
    INTEGER_FIELD(ZirExpr, kind), INTEGER_FIELD(ZirExpr, is_function_value),
    INTEGER_FIELD(ZirExpr, is_this),
    NAME_FIELD(ZirExpr, slot_type),
    NAME_FIELD(ZirExpr, name), NAME_FIELD(ZirExpr, argument_name),
    INTEGER_FIELD(ZirExpr, argument_index), STRING_FIELD(ZirExpr, op),
    INTEGER_FIELD(ZirExpr, left), INTEGER_FIELD(ZirExpr, right),
    INTEGER_FIELD(ZirExpr, first_child), INTEGER_FIELD(ZirExpr, next_sibling),
    INTEGER_FIELD(ZirExpr, third), NAME_FIELD(ZirExpr, type),
    SPAN_FIELD(ZirExpr, span),
    /* After the span, which locates the text in its statement. */
    {offsetof(ZirExpr, text), ZIR_TEXT_MAX, FIELD_EXPRESSION_TEXT}
};
static const Field function_fields[] = {
    STRING_FIELD(ZirFunction, name), PARAMETERS_FIELD(ZirFunction, args_text),
    PARAMETERS_FIELD(ZirFunction, default_args_text),
    U64_FIELD(ZirFunction, using_parameters),
    STRING_FIELD(ZirFunction, return_type), INTEGER_FIELD(ZirFunction, must_use),
    INTEGER_FIELD(ZirFunction, is_conversion),
    STRING_FIELD(ZirFunction, go_method),
    INTEGER_FIELD(ZirFunction, go_method_results),
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
    STRING_FIELD(ZirFunction, extern_symbol), INTEGER_FIELD(ZirFunction, inlining),
    INTEGER_FIELD(ZirFunction, simd_target),
    SPAN_FIELD(ZirFunction, span),
    {offsetof(ZirFunction, assembly_text), ZIR_TEXT_MAX, FIELD_FUNCTION_ASSEMBLY}
};
static const Field global_fields[] = {
    STRING_FIELD(ZirGlobal, name), STRING_FIELD(ZirGlobal, type),
    TEXT_FIELD(ZirGlobal, init), INTEGER_FIELD(ZirGlobal, is_static),
    INTEGER_FIELD(ZirGlobal, is_file_private), SPAN_FIELD(ZirGlobal, span)
};
static const Field define_fields[] = {
    STRING_FIELD(ZirDefine, name), TEXT_FIELD(ZirDefine, value),
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
    INTEGER_FIELD(ZirType, reads_arguments),
    STRING_FIELD(ZirType, procedure_return_type),
    INTEGER_FIELD(ZirType, is_public),
    INTEGER_FIELD(ZirType, is_file_private),
    INTEGER_FIELD(ZirType, is_enum),
    INTEGER_FIELD(ZirType, is_union),
    INTEGER_FIELD(ZirType, is_go_anonymous),
    INTEGER_FIELD(ZirType, is_enum_flags),
    INTEGER_FIELD(ZirType, is_enum_specified),
    STRING_FIELD(ZirType, enum_backing), STRING_FIELD(ZirType, c_prefix),
    INTEGER_FIELD(ZirType, is_record_template),
    INTEGER_FIELD(ZirType, is_owned_vec),
    INTEGER_FIELD(ZirType, is_map),
    INTEGER_FIELD(ZirType, is_extern),
    INTEGER_FIELD(ZirType, is_abi_incomplete), INTEGER_FIELD(ZirType, is_results),
    SPAN_FIELD(ZirType, span)
};

/* An enum's `#c` constant prefix: empty or a C identifier. */
static int
c_prefix_valid(const char *prefix)
{
    if(isdigit((unsigned char)prefix[0])) return 0;
    for(; *prefix; prefix++)
        if(!isalnum((unsigned char)*prefix) && *prefix != '_') return 0;
    return 1;
}

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
    if(!FunctionDefaultArgs(function)[0]) return 1;
    char (*parts)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parts));
    if(parts == NULL) return 0;
    int count = split_top_level(FunctionDefaultArgs(function), parts[0], 64,
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
            strcmp(normalized, FunctionArgs(function)) == 0;
    free(parts);
    return valid;
}
static const Field module_fields[] = {
    STRING_FIELD(ZirModule, name), STRING_FIELD(ZirModule, source_path),
    SPAN_FIELD(ZirModule, span)
};

/* The signature's version stays four fixed bytes, so every reader can name
 * the version it was given. */
static int
write_version(Writer *out, uint32_t value)
{
    unsigned char bytes[4] = {
        (unsigned char)value, (unsigned char)(value >> 8),
        (unsigned char)(value >> 16), (unsigned char)(value >> 24)
    };
    return write_bytes(out, bytes, sizeof(bytes));
}

static int
read_version(Reader *reader, uint32_t *value)
{
    unsigned char bytes[4];
    if(!read_bytes(reader, bytes, sizeof(bytes))) {
        reader->problem = "truncated integer";
        return 0;
    }
    *value = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
             (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
    return 1;
}

/* Numbers are unsigned LEB128: seven bits a byte, low bits first. */
static int
write_u64(Writer *out, uint64_t value)
{
    unsigned char bytes[10];
    size_t used = 0;
    do {
        unsigned char byte = (unsigned char)(value & 0x7f);
        value >>= 7;
        bytes[used++] = (unsigned char)(byte | (value != 0 ? 0x80 : 0));
    } while(value != 0);
    return write_bytes(out, bytes, used);
}

/* Readers accept only the shortest encoding, so equal values have equal
 * bytes, and only values up to MAXIMUM. */
static int
read_number(Reader *reader, uint64_t *value, uint64_t maximum)
{
    uint64_t result = 0;
    for(int shift = 0; shift < 64; shift += 7) {
        unsigned char byte;
        if(!read_byte(reader, &byte)) {
            reader->problem = "truncated integer";
            return 0;
        }
        if(shift == 63 && byte > 1)
            break;
        result |= (uint64_t)(byte & 0x7f) << shift;
        if((byte & 0x80) == 0) {
            if(byte == 0 && shift > 0) {
                reader->problem = "noncanonical integer";
                return 0;
            }
            if(result > maximum) {
                reader->problem = "integer out of range";
                return 0;
            }
            *value = result;
            return 1;
        }
    }
    reader->problem = "integer out of range";
    return 0;
}

static int
read_u64(Reader *reader, uint64_t *value)
{
    return read_number(reader, value, UINT64_MAX);
}

static int
write_u32(Writer *out, uint32_t value)
{
    return write_u64(out, value);
}

static int
read_u32(Reader *reader, uint32_t *value)
{
    uint64_t number;
    if(!read_number(reader, &number, UINT32_MAX)) return 0;
    *value = (uint32_t)number;
    return 1;
}

/* Signed numbers zigzag, so -1, which marks absent references, is one byte. */
static int
write_s32(Writer *out, int32_t value)
{
    uint32_t bits = (uint32_t)value;
    return write_u32(out, (bits << 1) ^ (value < 0 ? UINT32_MAX : 0));
}

static int
read_s32(Reader *reader, int32_t *value)
{
    uint32_t bits;
    if(!read_u32(reader, &bits)) return 0;
    *value = (int32_t)((bits >> 1) ^ (0u - (bits & 1u)));
    return 1;
}

static uint32_t
string_hash(const char *text, size_t length)
{
    uint32_t hash = 2166136261u;
    for(size_t i = 0; i < length; i++)
        hash = (hash ^ (unsigned char)text[i]) * 16777619u;
    return hash;
}

/* Index plus one of an earlier equal string, or 0. */
static uint32_t
writer_string_slot(const Writer *out, const char *text, size_t length, uint32_t *slot)
{
    uint32_t mask = out->slot_count - 1;
    uint32_t at = string_hash(text, length) & mask;
    while(out->slots[at] != 0) {
        const SerialString *known = &out->strings[out->slots[at] - 1];
        if(known->length == length && memcmp(known->text, text, length) == 0)
            break;
        at = (at + 1) & mask;
    }
    *slot = at;
    return out->slots[at];
}

static int
writer_grow_strings(Writer *out)
{
    if(out->string_count == out->string_capacity) {
        uint32_t capacity = out->string_capacity ? out->string_capacity * 2 : 1024;
        SerialString *strings = realloc(out->strings, (size_t)capacity * sizeof(*strings));
        if(strings == NULL) return 0;
        out->strings = strings;
        out->string_capacity = capacity;
    }
    if((out->string_count + 1) * 2 <= out->slot_count) return 1;
    uint32_t count = out->slot_count ? out->slot_count * 2 : 2048;
    uint32_t *slots = calloc(count, sizeof(*slots));
    if(slots == NULL) return 0;
    free(out->slots);
    out->slots = slots;
    out->slot_count = count;
    for(uint32_t i = 0; i < out->string_count; i++) {
        uint32_t slot;
        writer_string_slot(out, out->strings[i].text, out->strings[i].length, &slot);
        out->slots[slot] = i + 1;
    }
    return 1;
}

static uint32_t
address_slot(const Writer *out, uintptr_t address)
{
    uint32_t mask = out->address_slot_count - 1;
    uint32_t at = (uint32_t)(((uint64_t)address * UINT64_C(0x9e3779b97f4a7c15)) >> 32) & mask;
    while(out->address_strings[at] != 0 && out->addresses[at] != address)
        at = (at + 1) & mask;
    return at;
}

/* Index plus one of the string last written from ADDRESS, or 0. */
static uint32_t
writer_address_string(const Writer *out, const char *address)
{
    if(out->address_slot_count == 0) return 0;
    return out->address_strings[address_slot(out, (uintptr_t)address)];
}

static int
writer_remember_address(Writer *out, const char *address, uint32_t string)
{
    if((out->address_count + 1) * 2 > out->address_slot_count) {
        uint32_t old_count = out->address_slot_count;
        uintptr_t *old_addresses = out->addresses;
        uint32_t *old_strings = out->address_strings;
        uint32_t count = old_count ? old_count * 2 : 4096;
        out->addresses = malloc((size_t)count * sizeof(*out->addresses));
        out->address_strings = calloc(count, sizeof(*out->address_strings));
        if(out->addresses == NULL || out->address_strings == NULL) {
            free(out->addresses);
            free(out->address_strings);
            out->addresses = old_addresses;
            out->address_strings = old_strings;
            return 0;
        }
        out->address_slot_count = count;
        for(uint32_t i = 0; i < old_count; i++)
            if(old_strings[i] != 0) {
                uint32_t at = address_slot(out, old_addresses[i]);
                out->addresses[at] = old_addresses[i];
                out->address_strings[at] = old_strings[i];
            }
        free(old_addresses);
        free(old_strings);
    }
    uint32_t at = address_slot(out, (uintptr_t)address);
    if(out->address_strings[at] == 0) out->address_count++;
    out->addresses[at] = (uintptr_t)address;
    out->address_strings[at] = string;
    return 1;
}

static void
writer_free_strings(Writer *out)
{
    free(out->strings);
    free(out->slots);
    free(out->addresses);
    free(out->address_strings);
}

/* Charges COUNT records of SIZE bytes exactly as the reader will. */
static int
writer_charge(Writer *out, size_t count, size_t size)
{
    if(size != 0 && count > (ZIR_READ_BUDGET - out->charged) / size) {
        out->problem = ZIR_READ_BUDGET_PROBLEM;
        return 0;
    }
    out->charged += count * size;
    return 1;
}

/* Empty is 0; a string seen before is its index times SCALE plus one; a
 * new string is its length times SCALE plus NEW_TAG, then its bytes. Names
 * and text use a scale of two; expression text keeps a third form, so its
 * scale is four. The written program stays alive and unchanged while it is
 * written, so the table borrows it. */
static int
write_tagged_string(Writer *out, const char *value, size_t capacity,
                    uint32_t scale, uint32_t new_tag)
{
    if(capacity == 0)
        return 0;
    if(*value == '\0')
        return write_u32(out, 0);
    uint32_t known = writer_address_string(out, value);
    if(known != 0) {
        size_t length = out->strings[known - 1].length;
        if(length >= capacity || !writer_charge(out, length, 1) ||
           known - 1 > (UINT32_MAX - 1) / scale)
            return 0;
        return write_u32(out, (known - 1) * scale + 1);
    }
    size_t length = strnlen(value, capacity);
    if(length == capacity)
        return 0;
    if(!writer_charge(out, length, 1) || length > (UINT32_MAX - new_tag) / scale)
        return 0;
    if(!writer_grow_strings(out))
        return 0;
    uint32_t slot;
    known = writer_string_slot(out, value, length, &slot);
    if(known == 0) {
        out->strings[out->string_count] = (SerialString){value, (uint32_t)length, NULL};
        out->slots[slot] = ++out->string_count;
        if(!write_u32(out, (uint32_t)length * scale + new_tag) || !write_bytes(out, value, length))
            return 0;
        known = out->string_count;
    } else if(known - 1 > (UINT32_MAX - 1) / scale ||
              !write_u32(out, (known - 1) * scale + 1)) {
        return 0;
    }
    return writer_remember_address(out, value, known);
}

static int
write_string(Writer *out, const char *value, size_t capacity)
{
    return write_tagged_string(out, value, capacity, 2, 0);
}

/* The table index plus one of a nonempty VALUE written before, or 0. */
static uint32_t
writer_known_string(const Writer *out, const char *value, size_t length)
{
    uint32_t known = writer_address_string(out, value);
    if(known != 0 || out->slot_count == 0) return known;
    uint32_t slot;
    return writer_string_slot(out, value, length, &slot);
}

/* Reads one string into the table and returns it through TEXT and LENGTH;
 * the table owns the bytes. */
/* A string after its tag: an earlier string's INDEX when REFERENCE is set,
 * otherwise SIZE new bytes. */
static int
read_tagged_string(Reader *reader, int reference, uint32_t value,
                   const char **text, uint32_t *length, uint32_t *index_out)
{
    *index_out = UINT32_MAX;
    /* Every string costs the bytes it stands for, so references to a long
     * string cannot expand a short input into an unbounded amount of text. */
    if(reference) {
        uint32_t index = value;
        if(index >= reader->string_count) {
            reader->problem = "invalid string reference";
            return 0;
        }
        if(reader->strings[index].length > reader->budget) {
            reader->problem = ZIR_READ_BUDGET_PROBLEM;
            return 0;
        }
        reader->budget -= reader->strings[index].length;
        *text = reader->strings[index].text;
        *length = reader->strings[index].length;
        *index_out = index;
        return 1;
    }
    uint32_t size = value;
    if(size > reader->budget) {
        reader->problem = ZIR_READ_BUDGET_PROBLEM;
        return 0;
    }
    reader->budget -= size;
    if(reader->string_count == reader->string_capacity) {
        uint32_t capacity = reader->string_capacity ? reader->string_capacity * 2 : 1024;
        SerialString *strings = realloc(reader->strings, (size_t)capacity * sizeof(*strings));
        if(strings == NULL) {
            reader->problem = "out of memory reading IR";
            return 0;
        }
        reader->strings = strings;
        reader->string_capacity = capacity;
    }
    char *bytes = malloc((size_t)size + 1);
    if(bytes == NULL) {
        reader->problem = "out of memory reading IR";
        return 0;
    }
    if(!read_bytes(reader, bytes, size)) {
        free(bytes);
        reader->problem = "truncated string";
        return 0;
    }
    if(memchr(bytes, '\0', size) != NULL) {
        free(bytes);
        reader->problem = "embedded NUL in string";
        return 0;
    }
    bytes[size] = '\0';
    *index_out = reader->string_count;
    reader->strings[reader->string_count++] = (SerialString){bytes, size, NULL};
    *text = bytes;
    *length = size;
    return 1;
}

static int
read_serial_string_at(Reader *reader, const char **text, uint32_t *length, uint32_t *index_out)
{
    uint32_t tag;
    *index_out = UINT32_MAX;
    if(!read_u32(reader, &tag))
        return 0;
    if(tag == 0) {
        *text = "";
        *length = 0;
        return 1;
    }
    return read_tagged_string(reader, (tag & 1) != 0, tag / 2, text, length, index_out);
}

static int
read_serial_string(Reader *reader, const char **text, uint32_t *length)
{
    uint32_t index;
    return read_serial_string_at(reader, text, length, &index);
}

/* KeepText of a string just read; each table entry is kept once. */
static const char *
reader_kept(Reader *reader, const char *text, uint32_t index)
{
    if(index == UINT32_MAX)
        return KeepText(text);
    SerialString *string = &reader->strings[index];
    if(string->kept == NULL)
        string->kept = KeepText(text);
    return string->kept;
}

static void
reader_free_strings(Reader *reader)
{
    for(uint32_t i = 0; i < reader->string_count; i++)
        free((char *)reader->strings[i].text);
    free(reader->strings);
}

static int
read_string(Reader *reader, char *value, size_t capacity)
{
    const char *text;
    uint32_t length;
    if(!read_serial_string(reader, &text, &length))
        return 0;
    if(length >= capacity) {
        reader->problem = "string exceeds field limit";
        return 0;
    }
    memcpy(value, text, (size_t)length + 1);
    return 1;
}

/* A span starts with its line's delta from the previous span, doubled, plus
 * one when it names a different file, whose path then follows. The end is a
 * delta from the start, so nearby positions take a byte or two. */
static int
write_span(Writer *out, const ZirSourceSpan *span)
{
    const char *path = SpanPath(*span);
    int new_file = !out->has_span || (path != out->last_path && strcmp(path, out->last_path) != 0);
    int64_t line_delta = (int64_t)span->line - out->last_line;
    uint64_t line_bits = line_delta < 0 ? ((uint64_t)-line_delta << 1) - 1 : (uint64_t)line_delta << 1;
    uint32_t end_line_bits = (uint32_t)span->end_line - (uint32_t)span->line;
    uint32_t end_column_bits = (uint32_t)span->end_column - (uint32_t)span->column;
    out->has_span = 1;
    out->last_path = path;
    out->last_line = span->line;
    return write_u64(out, line_bits * 2 + (uint64_t)new_file) &&
           (!new_file || write_string(out, path, ZIR_PATH_MAX)) &&
           write_u32(out, (uint32_t)span->column) &&
           write_s32(out, (int32_t)end_line_bits) &&
           write_s32(out, (int32_t)end_column_bits);
}

static int
read_span(Reader *reader, ZirSourceSpan *span)
{
    char path[ZIR_PATH_MAX];
    uint64_t line_bits;
    int32_t end_line_delta, end_column_delta;
    uint32_t column;
    if(!read_number(reader, &line_bits, UINT64_C(1) << 34))
        return 0;
    int new_file = (int)(line_bits & 1);
    line_bits >>= 1;
    int64_t line_delta = (line_bits & 1) ? -(int64_t)((line_bits + 1) >> 1) : (int64_t)(line_bits >> 1);
    if(!new_file && !reader->has_span) {
        reader->problem = "span without a file";
        return 0;
    }
    if(new_file) {
        if(!read_string(reader, path, sizeof(path)))
            return 0;
        if(reader->has_span && strcmp(path, reader->last_path) == 0) {
            reader->problem = "noncanonical span file";
            return 0;
        }
        reader->last_file = SourceFile(path);
        memcpy(reader->last_path, path, strlen(path) + 1);
    }
    reader->has_span = 1;
    span->context = reader->context;
    span->file = reader->last_file;
    if(!read_u32(reader, &column) || !read_s32(reader, &end_line_delta) ||
       !read_s32(reader, &end_column_delta))
        return 0;
    int64_t line = (int64_t)reader->last_line + line_delta;
    int64_t end_line = line + end_line_delta;
    int64_t end_column = (int64_t)column + end_column_delta;
    if(line < 0 || line > INT32_MAX || end_line < 0 || end_line > INT32_MAX ||
       column > INT32_MAX || end_column < 0 || end_column > INT32_MAX) {
        reader->problem = "invalid source position";
        return 0;
    }
    reader->last_line = (int)line;
    span->line = (int)line;
    span->column = (int)column;
    span->end_line = (int)end_line;
    span->end_column = (int)end_column;
    return 1;
}

/* Where LINE:COLUMN falls in TEXT, the saved text of a statement that starts
 * at BASE_LINE:BASE_COLUMN, or 0 when it falls outside. Columns count bytes
 * from 1, and later lines of TEXT start at column 1. Expression columns are
 * offset by their statement's header, so a saved bias corrects this. */
static size_t
cursor_length(StatementCursor *cursor, const char *text, int base_line)
{
    if(cursor->text != text || cursor->base_line != base_line) {
        cursor->text = text;
        cursor->length = strlen(text);
        cursor->base_line = base_line;
        cursor->line = base_line;
        cursor->at = 0;
    }
    return cursor->length;
}

static int64_t
source_offset(StatementCursor *cursor, const char *text, const ZirSourceSpan *statement,
              const ZirSourceSpan *span)
{
    size_t length = cursor_length(cursor, text, statement->line);
    if(span->line < statement->line) return 0;
    if(cursor->line > span->line) {
        cursor->line = statement->line;
        cursor->at = 0;
    }
    while(cursor->line < span->line) {
        const char *newline = memchr(text + cursor->at, '\n', length - cursor->at);
        if(newline == NULL) return 0;
        cursor->at = (size_t)(newline - text) + 1;
        cursor->line++;
    }
    size_t at = cursor->at;
    int64_t column = span->line == statement->line ? (int64_t)span->column - statement->column
                                                   : (int64_t)span->column - 1;
    if(column < 0 || (uint64_t)column > length - at) return 0;
    return (int64_t)at + column;
}

static const char *
saved_statement_text(const Writer *out, const ZirStmt *statement)
{
    if(out->portable && StatementHasAssembly(statement)) return "{";
    return statement->text ? statement->text : "";
}

/* Each expression's statement: the first whose expression trees reach it. */
static int *
expression_owners(const ZirFunction *function)
{
    int count = function->expr_count;
    if(count <= 0) return NULL;
    int *owners = malloc((size_t)count * sizeof(*owners));
    int *stack = malloc((size_t)count * sizeof(*stack));
    if(owners == NULL || stack == NULL) {
        free(owners);
        free(stack);
        return NULL;
    }
    for(int i = 0; i < count; i++) owners[i] = -1;
    for(int s = 0; s < function->stmt_count; s++) {
        int top = 0;
        int roots[2] = {function->stmts[s].expr_root, function->stmts[s].lhs_root};
        for(int r = 0; r < 2; r++)
            if(roots[r] >= 0 && roots[r] < count && owners[roots[r]] < 0) {
                owners[roots[r]] = s;
                stack[top++] = roots[r];
            }
        while(top > 0) {
            const ZirExpr *expression = &function->exprs[stack[--top]];
            int links[5] = {expression->left, expression->right, expression->first_child,
                            expression->next_sibling, expression->third};
            for(int l = 0; l < 5; l++)
                if(links[l] >= 0 && links[l] < count && owners[links[l]] < 0) {
                    owners[links[l]] = s;
                    stack[top++] = links[l];
                }
        }
    }
    free(stack);
    return owners;
}

static uint32_t
zigzag32(int64_t value)
{
    uint32_t bits = (uint32_t)(int32_t)value;
    return (bits << 1) ^ (value < 0 ? UINT32_MAX : 0);
}

static int32_t
unzigzag32(uint32_t bits)
{
    return (int32_t)((bits >> 1) ^ (0u - (bits & 1u)));
}

static size_t
number_size(uint64_t value)
{
    size_t size = 1;
    while(value >>= 7) size++;
    return size;
}

/* Writers look for an expression's text this many bytes either side of the
 * offset its span names; statement headers are shorter than this. */
enum { EXPRESSION_TEXT_WINDOW = 256 };

/* Expression text is 0 when empty, a string as write_tagged_string writes it
 * with scale four, or a slice of a statement's saved text: one tag of the
 * zigzag statement distance from the last slice times four plus three, then
 * the length times two plus one when the bias changes, then the zigzag change
 * in bias. The bias is the byte offset of the text in its statement less
 * the offset its span names. Writers choose the shorter form. */
static int
write_expression_text(Writer *out, const ZirExpr *expression)
{
    const char *text = expression->text ? expression->text : "";
    if(*text == '\0') return write_u32(out, 0);
    size_t size = strlen(text);
    const ZirFunction *function = out->function;
    int best_owner = -1;
    int64_t best_bias = 0;
    size_t best_cost = SIZE_MAX;
    if(function != NULL && size <= INT32_MAX) {
        int index = (int)(expression - function->exprs);
        int candidates[2] = {out->owners ? out->owners[index] : -1, out->last_owner};
        for(int c = 0; c < 2; c++) {
            int owner = candidates[c];
            if(owner < 0 || owner >= function->stmt_count || (c == 1 && owner == candidates[0])) continue;
            const ZirStmt *statement = &function->stmts[owner];
            const char *saved = saved_statement_text(out, statement);
            size_t saved_size = cursor_length(&out->cursor, saved, statement->span.line);
            if(saved_size > INT32_MAX || size > saved_size) continue;
            int64_t base = source_offset(&out->cursor, saved, &statement->span, &expression->span);
            int64_t low = base > EXPRESSION_TEXT_WINDOW ? base - EXPRESSION_TEXT_WINDOW : 0;
            int64_t high = base + EXPRESSION_TEXT_WINDOW;
            if(high > (int64_t)(saved_size - size)) high = (int64_t)(saved_size - size);
            /* The cheapest occurrence in the window; on a tie, the earlier. */
            for(int64_t at = low; at <= high; at++) {
                if(saved[at] != text[0] || memcmp(saved + at, text, size) != 0) continue;
                int64_t bias = at - base;
                size_t cost = number_size((uint64_t)zigzag32(owner - out->last_owner) * 4 + 3) +
                              number_size((uint64_t)size * 2 + (bias != out->last_bias)) +
                              (bias != out->last_bias ? number_size(zigzag32(bias - out->last_bias)) : 0);
                if(cost < best_cost) {
                    best_cost = cost;
                    best_owner = owner;
                    best_bias = bias;
                }
            }
        }
    }
    uint32_t known = writer_known_string(out, text, size);
    size_t explicit_cost = known != 0 ? number_size((uint64_t)(known - 1) * 4 + 1)
                                      : number_size((uint64_t)size * 4 + 2) + size;
    if(best_owner < 0 || explicit_cost <= best_cost)
        return write_tagged_string(out, text, SIZE_MAX, 4, 2);
    int changed = best_bias != out->last_bias;
    uint32_t distance = zigzag32(best_owner - out->last_owner);
    uint32_t change = zigzag32(best_bias - out->last_bias);
    if(distance > (UINT32_MAX - 3) / 4 || !writer_charge(out, size, 1) ||
       !write_u32(out, distance * 4 + 3) ||
       !write_u32(out, (uint32_t)size * 2 + (uint32_t)changed) ||
       (changed && !write_u32(out, change)))
        return 0;
    out->last_owner = best_owner;
    out->last_bias = best_bias;
    return 1;
}

static int
read_expression_text(Reader *reader, ZirExpr *expression)
{
    uint32_t tag;
    if(!read_u32(reader, &tag)) return 0;
    if(tag == 0 || (tag & 3) != 3) {
        const char *text = "";
        uint32_t length = 0, index = UINT32_MAX;
        if(tag != 0 && ((tag & 3) == 0 ||
           !read_tagged_string(reader, (tag & 3) == 1, tag / 4, &text, &length, &index))) {
            if(reader->problem == NULL) reader->problem = "invalid expression text";
            return 0;
        }
        if(tag != 0 && length == 0) {
            reader->problem = "invalid expression text";
            return 0;
        }
        expression->text = reader_kept(reader, text, index);
        return 1;
    }
    uint32_t measure, change = 0;
    if(!read_u32(reader, &measure) || ((measure & 1) && !read_u32(reader, &change)))
        return 0;
    int64_t owner = (int64_t)reader->last_owner + unzigzag32(tag / 4);
    int64_t bias = reader->last_bias + unzigzag32(change);
    uint32_t length = measure / 2;
    const ZirFunction *function = reader->function;
    const char *saved = function != NULL && owner >= 0 && owner < function->stmt_count
                            ? function->stmts[owner].text : NULL;
    size_t saved_size = saved ? cursor_length(&reader->cursor, saved, function->stmts[owner].span.line) : 0;
    int64_t at = saved ? source_offset(&reader->cursor, saved, &function->stmts[owner].span,
                                       &expression->span) + bias : -1;
    if(saved == NULL || length == 0 || ((measure & 1) && change == 0) ||
       at < 0 || (uint64_t)at > saved_size || length > saved_size - (size_t)at) {
        reader->problem = "invalid expression text reference";
        return 0;
    }
    if(length > reader->budget) {
        reader->problem = ZIR_READ_BUDGET_PROBLEM;
        return 0;
    }
    reader->budget -= length;
    char *copy = malloc((size_t)length + 1);
    if(copy == NULL) {
        reader->problem = "out of memory reading IR";
        return 0;
    }
    memcpy(copy, saved + at, length);
    copy[length] = '\0';
    expression->text = KeepText(copy);
    free(copy);
    reader->last_owner = (int)owner;
    reader->last_bias = bias;
    return 1;
}

static int
write_fields(Writer *out, const void *record, const Field *fields, size_t count)
{
    for(size_t i = 0; i < count; i++) {
        const char *value = (const char *)record + fields[i].offset;
        if(fields[i].kind == FIELD_STRING) {
            if(!write_string(out, value, fields[i].size))
                return 0;
        } else if(fields[i].kind == FIELD_NAME || fields[i].kind == FIELD_STATEMENT_NAME) {
            const char *name = *(const char *const *)value;
            if(fields[i].kind == FIELD_STATEMENT_NAME &&
               ((const ZirStmt *)record)->kind == ZIR_STMT_BLOCK_OPEN) name = "";
            if(!write_string(out, name ? name : "", fields[i].size))
                return 0;
        } else if(fields[i].kind == FIELD_TEXT || fields[i].kind == FIELD_ASSEMBLY_TEXT ||
                  fields[i].kind == FIELD_STATEMENT_TEXT || fields[i].kind == FIELD_FUNCTION_ASSEMBLY) {
            /* Source text has no field limit: a long literal keeps it all. */
            const char *text = *(const char *const *)value;
            if(fields[i].kind == FIELD_ASSEMBLY_TEXT &&
               (out->portable || !StatementHasAssembly(record)))
                continue;
            if(out->portable && fields[i].kind == FIELD_STATEMENT_TEXT && StatementHasAssembly(record)) text = "{";
            size_t limit = fields[i].kind == FIELD_ASSEMBLY_TEXT ? fields[i].size : SIZE_MAX;
            if(!write_string(out, text ? text : "", limit))
                return 0;
        } else if(fields[i].kind == FIELD_STATEMENT_KIND) {
            const ZirStmt *statement = record;
            uint32_t kind = (uint32_t)statement->kind;
            if(!out->portable && StatementHasAssembly(statement)) {
                kind |= ASSEMBLY_BLOCK_FLAG;
            }
            if(!write_u32(out, kind)) return 0;
        } else if(fields[i].kind == FIELD_EXPRESSION_TEXT) {
            if(!write_expression_text(out, record)) return 0;
        } else if(fields[i].kind == FIELD_INTEGER) {
            if(!write_s32(out, (int32_t)*(const int *)value))
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
    int assembly_block = 0;
    for(size_t i = 0; i < count; i++) {
        char *value = (char *)record + fields[i].offset;
        if(fields[i].kind == FIELD_STRING) {
            if(!read_string(reader, value, fields[i].size))
                return 0;
        } else if(fields[i].kind == FIELD_NAME || fields[i].kind == FIELD_STATEMENT_NAME) {
            const char *name;
            uint32_t length, index;
            if(!read_serial_string_at(reader, &name, &length, &index))
                return 0;
            if(length >= fields[i].size) {
                reader->problem = "string exceeds field limit";
                return 0;
            }
            if(fields[i].kind == FIELD_STATEMENT_NAME &&
               ((ZirStmt *)record)->kind == ZIR_STMT_BLOCK_OPEN) {
                if(*name) { reader->problem = "unexpected block binding name"; return 0; }
            } else *(const char **)value = reader_kept(reader, name, index);
        } else if(fields[i].kind == FIELD_TEXT || fields[i].kind == FIELD_ASSEMBLY_TEXT ||
                  fields[i].kind == FIELD_STATEMENT_TEXT || fields[i].kind == FIELD_FUNCTION_ASSEMBLY) {
            if(fields[i].kind == FIELD_ASSEMBLY_TEXT && !assembly_block) {
                *(const char **)value = "";
                continue;
            }
            const char *text;
            uint32_t length, index;
            if(!read_serial_string_at(reader, &text, &length, &index))
                return 0;
            if(fields[i].kind == FIELD_ASSEMBLY_TEXT &&
               (length == 0 || length >= fields[i].size)) {
                reader->problem = length == 0 ? "empty assembly block metadata" : "string exceeds field limit";
                return 0;
            }
            *(const char **)value = reader_kept(reader, text, index);
        } else if(fields[i].kind == FIELD_STATEMENT_KIND) {
            uint32_t kind;
            if(!read_u32(reader, &kind)) return 0;
            assembly_block = (kind & ASSEMBLY_BLOCK_FLAG) != 0;
            kind &= ~(uint32_t)ASSEMBLY_BLOCK_FLAG;
            if(assembly_block && kind != ZIR_STMT_BLOCK_OPEN) {
                reader->problem = "assembly metadata requires a block statement";
                return 0;
            }
            *(int *)value = (int)(int32_t)kind;
        } else if(fields[i].kind == FIELD_EXPRESSION_TEXT) {
            if(!read_expression_text(reader, record)) return 0;
        } else if(fields[i].kind == FIELD_INTEGER) {
            int32_t number;
            if(!read_s32(reader, &number))
                return 0;
            *(int *)value = (int)number;
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
write_records(Writer *out, const void *records, int count, size_t size,
              const Field *fields, size_t field_count)
{
    if(count < 0 || (count > 0 && records == NULL) ||
       !writer_charge(out, (size_t)count, size) ||
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
    /* The memory budget, not a fixed count, bounds records: a function
     * holding a long literal has one expression per element. */
    if(length > INT32_MAX || size == 0) {
        reader->problem = "record count exceeds IR limit";
        return NULL;
    }
    if(length > reader->budget / size) {
        reader->problem = ZIR_READ_BUDGET_PROBLEM;
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
write_function(Writer *out, const ZirFunction *function)
{
    if(!write_fields(out, function, function_fields, FIELD_COUNT(function_fields)) ||
       !WRITE_ARRAY(out, function, stmts, stmt_count, statement_fields))
        return 0;
    int *owners = expression_owners(function);
    if(function->expr_count > 0 && owners == NULL) {
        out->problem = "out of memory writing IR";
        return 0;
    }
    out->function = function;
    out->owners = owners;
    out->last_owner = 0;
    out->last_bias = 0;
    int ok = WRITE_ARRAY(out, function, exprs, expr_count, expression_fields);
    out->function = NULL;
    out->owners = NULL;
    free(owners);
    return ok;
}

static int
read_function(Reader *reader, ZirFunction *function)
{
    if(!read_fields(reader, function, function_fields, FIELD_COUNT(function_fields)))
        return 0;
    READ_ARRAY(reader, function, stmts, stmt_count, statement_fields);
    reader->function = function;
    reader->last_owner = 0;
    reader->last_bias = 0;
    READ_ARRAY(reader, function, exprs, expr_count, expression_fields);
    reader->function = NULL;
    function->stmt_cap = function->stmt_count;
    function->expr_cap = function->expr_count;
    function->from_ir = 1;
    return 1;
}

static int
write_law_graphs(Writer *out, const ZirModule *module)
{
    for(int l = 0; l < module->law_count; l++) {
        const ZirFunction *claim = module->laws[l].claim;
        if(!write_u32(out, claim != NULL) ||
           (claim && (!writer_charge(out, 1, sizeof(ZirFunction)) ||
                      !write_function(out, claim)))) return 0;
    }
    if(module->proof_count < 0 ||
       !writer_charge(out, (size_t)module->proof_count, sizeof(ZirProof)) ||
       !write_u32(out, (uint32_t)module->proof_count)) return 0;
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
    if(!read_u32(reader, &count) || count > INT32_MAX ||
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
write_module(Writer *out, const ZirModule *module)
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
       module->function_count < 0 ||
       !writer_charge(out, (size_t)module->function_count, sizeof(ZirFunction)) ||
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
    module->context = reader->context;
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
    if(count > INT32_MAX || count > reader->budget / sizeof(ZirFunction)) {
        reader->problem = count > INT32_MAX ? "function count exceeds IR limit" :
                          ZIR_READ_BUDGET_PROBLEM;
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
               (module->imports[i].go_variadic != 0 && module->imports[i].go_variadic != 1) ||
               (module->imports[i].reads_arguments != 0 &&
                (module->imports[i].reads_arguments != 1 ||
                 module->imports[i].kind != ZIR_IMPORT_EXTERN)) ||
               module->imports[i].reserved_1 != 0 ||
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
               (module->imports[i].go_variadic &&
                (module->imports[i].kind != ZIR_IMPORT_EXTERN ||
                 module->imports[i].extern_kind != ZIR_EXTERN_GO ||
                 module->imports[i].go_field || module->imports[i].is_varargs ||
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
            if((type->is_go_anonymous != 0 && type->is_go_anonymous != 1) ||
               (type->is_go_anonymous &&
                (type->is_enum || type->is_union || type->is_extern ||
                 type->is_procedure_type || type->is_abi_incomplete ||
                 type->is_map || type->is_owned_vec)))
                return 0;
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
               (type->reads_arguments != 0 && type->reads_arguments != 1) ||
               (type->reads_arguments && (!type->is_procedure_type || type->is_c_call)) ||
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
                 type->enum_backing[0] || type->c_prefix[0])) ||
               !c_prefix_valid(type->c_prefix) ||
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
               !export_symbol_valid(function->go_method) ||
               (function->go_method_results != 0 && function->go_method_results != 1) ||
               (function->go_method_results && !function->go_method[0]) ||
               (function->go_method[0] &&
                (function->is_extern || function->is_template ||
                 function->is_specialization || function->is_global_initializer)) ||
               (function->export_symbol[0] && !function->exported) ||
               (function->must_use != 0 && function->must_use != 1) ||
               function->inlining < ZIR_INLINE_NONE ||
               function->inlining > ZIR_INLINE_COLD ||
               (function->simd_target != 0 && function->simd_target != 1) ||
               (function->simd_target && function->is_extern) ||
               (function->inlining != ZIR_INLINE_NONE && function->is_extern) ||
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
                 !function->is_file_private || FunctionArgs(function)[0] ||
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

/* Names a limit that stopped writing; the caller still reports the failed
 * output. */
static int
report_write_problem(const ZirProgram *program, const Writer *out)
{
    if(out->problem != NULL && program != NULL && program->module_count > 0)
        Diagnostic(program->modules[0].span, "zir.output", "%s", out->problem);
    return 0;
}

static int
write_program(const ZirProgram *program, Writer *out, int validate)
{
    static const unsigned char magic[4] = {'Z', 'I', 'R', 0};
    if(program == NULL || out == NULL || (validate && !validate_program(program)) ||
       !write_bytes(out, magic, sizeof(magic)) ||
       !write_version(out, ZIR_FORMAT_VERSION) ||
       program->module_count < 0 ||
       !writer_charge(out, (size_t)program->module_count, sizeof(ZirModule)) ||
       !write_u32(out, (uint32_t)program->module_count))
        return report_write_problem(program, out);
    for(int i = 0; i < program->module_count; i++)
        if(!write_module(out, &program->modules[i]))
            return report_write_problem(program, out);
    return out->measure || (flush_writer(out) && fflush(out->out) == 0);
}

int
ProgramWrite(const ZirProgram *program, FILE *out)
{
    uint64_t started = ProfileStart();
    Writer writer = {.out = out, .block = malloc(SERIAL_BLOCK)};
    int result = writer.block != NULL && out != NULL &&
                 write_program(program, &writer, 1);
    writer_free_strings(&writer);
    free(writer.block);
    ProfileEnd("ir.write", started);
    return result;
}

int
ProgramMeasurePortable(const ZirProgram *program, size_t *size)
{
    if(size == NULL) return 0;
    *size = 0;
    Writer writer = {.portable = 1, .measure = 1};
    uint64_t started = ProfileStart();
    int result = write_program(program, &writer, 1);
    writer_free_strings(&writer);
    ProfileEnd("ir.measure.portable", started);
    if(result) *size = writer.bytes;
    return result;
}

int
ProgramWritePortable(const ZirProgram *program, FILE *out, size_t *size)
{
    if(size == NULL || out == NULL) return 0;
    *size = 0;
    Writer writer = {.out = out, .block = malloc(SERIAL_BLOCK), .portable = 1};
    /* Bundle writing has just measured and validated this immutable graph. */
    uint64_t started = ProfileStart();
    int result = writer.block != NULL && write_program(program, &writer, 0);
    ProfileEnd("ir.write", started);
    if(result) *size = writer.bytes;
    writer_free_strings(&writer);
    free(writer.block);
    return result;
}

static ZirProgram *
ProgramRead_in_context(FILE *in, const char *path)
{
    unsigned char magic[4];
    uint32_t version, count;
    Reader reader = {in, path, NULL, ZIR_READ_BUDGET, NULL, 0, 0};
    ZirProgram *program = NULL;
    if(in == NULL || path == NULL)
        return NULL;
    reader.context = CompilerContextCurrent();
    reader.block = malloc(SERIAL_BLOCK);
    if(reader.block == NULL) {
        reader.problem = "out of memory reading IR";
        goto failed;
    }
    if(!read_bytes(&reader, magic, sizeof(magic)) ||
       memcmp(magic, "ZIR\0", sizeof(magic)) != 0) {
        reader.problem = "invalid ZIR signature";
        goto failed;
    }
    if(!read_version(&reader, &version))
        goto failed;
    if(version != ZIR_FORMAT_VERSION) {
        reader.problem = "unsupported ZIR version";
        goto failed;
    }
    if(!read_u32(&reader, &count))
        goto failed;
    if(count == 0 || count > INT32_MAX || count > reader.budget / sizeof(ZirModule)) {
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
    if(reader.start != reader.end || fgetc(in) != EOF || ferror(in)) {
        reader.problem = "trailing or unreadable ZIR data";
        goto failed;
    }
    if(!validate_program(program)) {
        reader.problem = "invalid checked IR structure";
        goto failed;
    }
    reader_free_strings(&reader);
    free(reader.block);
    return program;
failed:
    Diagnostic(Span(path, 1, 1), "zir.invalid", "%s",
                  reader.problem ? reader.problem : "truncated ZIR data");
    reader_free_strings(&reader);
    free(reader.block);
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

typedef struct SavedModuleShape {
    int function_count;
    int type_count;
    char (*type_names)[ZIR_NAME_MAX];
} SavedModuleShape;

/* Checking a source consumer can append specializations and concrete result
 * types to a saved library. Compare every original declaration in its saved
 * order, including its newly checked fields, while excluding those additions.
 * Type ordering may move new dependencies before existing records. */
static int
write_original_checked_program(const ZirProgram *program,
                               const SavedModuleShape *shapes, FILE *out)
{
    ZirProgram original = *program;
    original.modules = calloc((size_t)program->module_count, sizeof(*original.modules));
    if(original.modules == NULL) return 0;
    int written = 0;
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *checked = &program->modules[m];
        ZirModule *module = &original.modules[m];
        *module = *checked;
        module->function_count = shapes[m].function_count;
        module->type_count = shapes[m].type_count;
        module->types = NULL;
        if(module->type_count > 0) {
            module->types = malloc((size_t)module->type_count * sizeof(*module->types));
            if(module->types == NULL) goto done;
        }
        for(int t = 0; t < module->type_count; t++) {
            const ZirType *type = NULL;
            for(int c = 0; c < checked->type_count; c++)
                if(!strcmp(checked->types[c].name, shapes[m].type_names[t])) {
                    type = &checked->types[c];
                    break;
                }
            if(type == NULL) goto done;
            module->types[t] = *type;
        }
    }
    SymbolLookupsChanged();
    written = ProgramWrite(&original, out);
done:
    for(int m = 0; m < program->module_count; m++) free(original.modules[m].types);
    free(original.modules);
    SymbolLookupsChanged();
    return written;
}

int CheckCanonicalPrograms(ZirProgram **programs, int count,
                       const char *const *input_paths);

static int
CheckCanonicalPrograms_with_buffers(ZirProgram **programs, int count,
                       const char *const *input_paths, CheckCanonicalProgramsBuffers *buffers)
{
    FILE *before = NULL;
    FILE *after = NULL;
    int saved_count = 0;
#if defined(ZIR_MEMORY_STREAMS)
    unsigned char *before_data = NULL, *after_data = NULL;
    size_t before_size = 0, after_size = 0;
#endif
    int first_saved = -1;
    int valid = 0;
    SavedModuleShape **original_counts = NULL;
    for(int i = 0; i < count; i++) {
        if(input_paths == NULL || PathIsIR(input_paths[i])) {
            if(first_saved < 0)
                first_saved = i;
            saved_count++;
        }
    }
    if(saved_count == 0)
        return CheckPrograms(programs, count) && PrepareNativeNames(programs, count);
    original_counts = calloc((size_t)count, sizeof(*original_counts));
    if(original_counts == NULL) goto failed;
    for(int i = 0; i < count; i++) {
        if(input_paths != NULL && !PathIsIR(input_paths[i])) continue;
        int modules = programs[i]->module_count;
        original_counts[i] = calloc((size_t)modules, sizeof(*original_counts[i]));
        if(original_counts[i] == NULL) goto failed;
        for(int m = 0; m < modules; m++) {
            const ZirModule *module = &programs[i]->modules[m];
            SavedModuleShape *shape = &original_counts[i][m];
            shape->function_count = module->function_count;
            shape->type_count = module->type_count;
            if(shape->type_count == 0) continue;
            shape->type_names = malloc((size_t)shape->type_count * sizeof(*shape->type_names));
            if(shape->type_names == NULL) goto failed;
            for(int t = 0; t < shape->type_count; t++)
                copy_text(shape->type_names[t], sizeof(shape->type_names[t]), module->types[t].name);
        }
    }
    /* Saved IR is an executable typed artifact. Require checked statement
     * and expression graphs before target emission. */
#if defined(ZIR_MEMORY_STREAMS)
    before = ZirWriteMemory(&before_data, &before_size);
    after = ZirWriteMemory(&after_data, &after_size);
#else
    before = tmpfile();
    after = tmpfile();
#endif
    if(before == NULL || after == NULL)
        goto failed;
    for(int i = 0; i < count; i++)
        if((input_paths == NULL || PathIsIR(input_paths[i])) &&
           !ProgramWrite(programs[i], before))
            goto failed;
    if(!CheckPrograms(programs, count) || !PrepareNativeNames(programs, count))
        goto done;
    for(int i = 0; i < count; i++) {
        if(original_counts[i] == NULL) continue;
        if(!write_original_checked_program(programs[i], original_counts[i], after))
            goto failed;
    }
#if defined(ZIR_MEMORY_STREAMS)
    int before_status = fclose(before);
    int after_status = fclose(after);
    before = NULL;
    after = NULL;
    if(before_status != 0 || after_status != 0)
        goto failed;
    if(before_size != after_size || memcmp(before_data, after_data, before_size) != 0) {
        Diagnostic(Span(input_paths != NULL ? input_paths[first_saved] : "<bundle>",
                        1, 1), "zir.noncanonical",
                   "saved IR does not match the checked program");
        goto done;
    }
    valid = 1;
#else
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
#endif
    goto done;
failed:
    Diagnostic(Span(input_paths != NULL && first_saved >= 0 ?
                    input_paths[first_saved] : "<bundle>", 1, 1),
               "zir.validation", "cannot validate saved IR");
done:
    if(original_counts != NULL) {
        for(int i = 0; i < count; i++) {
            if(original_counts[i] != NULL)
                for(int m = 0; m < programs[i]->module_count; m++)
                    free(original_counts[i][m].type_names);
            free(original_counts[i]);
        }
        free(original_counts);
    }
    if(before != NULL)
        fclose(before);
    if(after != NULL)
        fclose(after);
#if defined(ZIR_MEMORY_STREAMS)
    free(before_data);
    free(after_data);
#endif
    return valid;
}

COMPILER_SCRATCH_RETURN(static, int, CheckCanonicalPrograms_in_context,
    (ZirProgram **programs, int count,
                       const char *const *input_paths),
    CheckCanonicalProgramsBuffers, RECOVERABLE, free,
    CheckCanonicalPrograms_with_buffers(programs, count, input_paths, buffers))

int CheckCanonicalPrograms(ZirProgram **programs, int count, const char *const *input_paths)
{
    CompilerContext *previous = CompilerContextSelect(count > 0 && programs && programs[0] ? programs[0]->context : CompilerContextCurrent());
    int result = CheckCanonicalPrograms_in_context(programs, count, input_paths);
    CompilerContextSelect(previous);
    return result;
}

ZirProgram *ProgramRead(FILE *in, const char *path)
{
    CompilerMemory *memory = CompilerMemoryPause();
    CompilerContext *previous = CompilerContextCurrent();
    CompilerContext *owner = previous ? previous : CompilerContextOpen();
    if(!owner) { CompilerMemoryResume(memory); DiagnosticOutOfMemory(); return NULL; }
    CompilerContextSelect(owner);
    ZirProgram *program = ProgramRead_in_context(in, path);
    if(program && !program->context_retained) {
        CompilerContextRetain(owner);
        program->context_retained = 1;
    }
    if(program && !previous) program->owns_context = 1;
    CompilerContextSelect(previous);
    if(!program && !previous) CompilerContextClose(owner);
    CompilerMemoryResume(memory);
    return program;
}
