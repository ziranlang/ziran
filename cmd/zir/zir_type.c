#include "zir_memory.h"
#include "zir.h"
#include "zir_thread.h"
#include "compiler_type.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

/* IR storage and C strings stay at this boundary. All spelling, field,
 * container, and foreign-symbol rules come from cmd/compiler_type.zi. */
static String type_text(const char *source)
{
    return source == NULL ? (String){0} : StringView(source, strlen(source));
}

static int copy_part(char *output, size_t capacity, String part)
{
    if(output == NULL) return 1;
    if(part.length < 0 || (uint64_t)part.length >= capacity) return 0;
    if(part.length > 0) memcpy(output, part.data, (size_t)part.length);
    output[part.length] = '\0';
    return 1;
}

/* Immutable parsed field snapshots are shared by identical bodies. A cursor
 * identifies its snapshot and field index; subsequent steps neither scan nor
 * hash the source again. Rewritten bodies get a new snapshot on the next walk.
 * KeepText owns source identity, so copied/reallocated ZirTypes are safe. */
typedef struct RecordFields {
    const char *body;
    int is_union, terminal, primitive_fields;
    size_t id, count, capacity;
    ZirTypeField *items;
    size_t field_slot_count;
    int *field_slots;
} RecordFields;
typedef struct FieldSnapshots {
    RecordFields **slots, **ids;
    size_t slots_count, count, capacity;
} FieldSnapshots;
static void free_record_fields(void *value);
static FieldSnapshots *context_fields(CompilerThread *thread)
{
    return CompilerThreadStorage(thread, COMPILER_FIELDS, sizeof(FieldSnapshots), free_record_fields);
}
static atomic_flag record_fields_lock = ATOMIC_FLAG_INIT;
enum { FIELD_CURSOR_BITS = 12, FIELD_CURSOR_MASK = (1 << FIELD_CURSOR_BITS) - 1 };

static size_t field_slot(RecordFields **slots, size_t count, const char *body, int is_union)
{
    size_t at = (((uintptr_t)body >> 3) * 31 + (unsigned)is_union) & (count - 1);
    while(slots[at] != NULL && (slots[at]->body != body || slots[at]->is_union != is_union))
        at = (at + 1) & (count - 1);
    return at;
}

static void free_record_fields(void *value)
{
    FieldSnapshots *fields = value;
    for(size_t i = 0; i < fields->count; i++) {
        free(fields->ids[i]->items);
        free(fields->ids[i]->field_slots);
        free(fields->ids[i]);
    }
    free(fields->ids);
    free(fields->slots);
}

static size_t
named_field_slot(const RecordFields *fields, const char *name)
{
    size_t hash = 2166136261u;
    for(const unsigned char *at = (const unsigned char *)name; *at; at++)
        hash = (hash ^ *at) * 16777619u;
    size_t slot = hash & (fields->field_slot_count - 1);
    while(fields->field_slots[slot] &&
          strcmp(fields->items[fields->field_slots[slot] - 1].name, name))
        slot = (slot + 1) & (fields->field_slot_count - 1);
    return slot;
}

static RecordFields *fields_of(CompilerThread *thread, const ZirType *record)
{
    size_t length = strnlen(record->body, sizeof(record->body));
    if(length == sizeof(record->body)) return NULL;
    const char *body = KeepText(record->body);
    /* The context cannot change here; look its storage up once. */
    FieldSnapshots *snapshots = context_fields(thread);
    int is_union = record->is_union != 0;
    if(snapshots->slots_count == 0 || snapshots->count * 2 >= snapshots->slots_count) {
        size_t count = snapshots->slots_count ? snapshots->slots_count * 2 : 128;
        RecordFields **slots = AllocateOrExit(count * sizeof(*slots));
        memset(slots, 0, count * sizeof(*slots));
        for(size_t i = 0; i < snapshots->count; i++) {
            RecordFields *entry = snapshots->ids[i];
            slots[field_slot(slots, count, entry->body, entry->is_union)] = entry;
        }
        free(snapshots->slots);
        snapshots->slots = slots;
        snapshots->slots_count = count;
    }
    size_t slot = field_slot(snapshots->slots, snapshots->slots_count, body, is_union);
    if(snapshots->slots[slot] != NULL) {
        ProfileCount("record_fields_reuse");
        return snapshots->slots[slot];
    }
    if(snapshots->count >= (SIZE_MAX >> FIELD_CURSOR_BITS)) return NULL;
    if(snapshots->count == snapshots->capacity) {
        size_t capacity = snapshots->capacity ? snapshots->capacity * 2 : 64;
        RecordFields **ids = AllocateOrExit(capacity * sizeof(*ids));
        if(snapshots->count) memcpy(ids, snapshots->ids, snapshots->count * sizeof(*ids));
        free(snapshots->ids);
        snapshots->ids = ids;
        snapshots->capacity = capacity;
    }
    RecordFields *result = AllocateOrExit(sizeof(*result));
    memset(result, 0, sizeof(*result));
    result->body = body;
    result->is_union = is_union;
    result->primitive_fields = 1;
    result->id = snapshots->count + 1;
    size_t offset = 0;
    unsigned char tags[sizeof(((ZirTypeField *)0)->go_tag)];
    for(;;) {
        ProfileCount("record_field_parse");
        RecordField parsed = compiler_type_NextField(StringView(body, length), (int64_t)offset,
            is_union, ZIR_NAME_MAX, ZIR_NAME_MAX, (Slice){tags, sizeof(tags)});
        if(parsed.status != 1) { result->terminal = parsed.status; break; }
        if(parsed.next <= (int64_t)offset || result->count == FIELD_CURSOR_MASK) {
            result->terminal = -1;
            break;
        }
        offset = (size_t)parsed.next;
        if(result->count == result->capacity) {
            size_t capacity = result->capacity ? result->capacity * 2 : 8;
            ZirTypeField *items = AllocateOrExit(capacity * sizeof(*items));
            if(result->count) memcpy(items, result->items, result->count * sizeof(*items));
            free(result->items);
            result->items = items;
            result->capacity = capacity;
        }
        ZirTypeField *field = &result->items[result->count++];
        memset(field, 0, sizeof(*field));
        copy_part(field->name, sizeof(field->name), parsed.name);
        copy_part(field->type, sizeof(field->type), parsed.type);
        copy_part(field->go_tag, sizeof(field->go_tag), parsed.tag);
        copy_part(field->c_type, sizeof(field->c_type), parsed.c_type);
        field->is_using = parsed.is_using;
        if(field->type[0] != '*' && !compiler_type_BuiltinValueName(type_text(field->type)))
            result->primitive_fields = 0;
    }
    snapshots->ids[snapshots->count++] = result;
    if(result->count > 0) {
        size_t slots = 8;
        while(slots < result->count * 2) slots *= 2;
        result->field_slots = AllocateOrExit(slots * sizeof(*result->field_slots));
        memset(result->field_slots, 0, slots * sizeof(*result->field_slots));
        result->field_slot_count = slots;
        for(size_t i = 0; i < result->count; i++) {
            size_t at = named_field_slot(result, result->items[i].name);
            /* The existing field walk resolves the first matching field.
             * Declaration checking separately rejects duplicate names. */
            if(!result->field_slots[at]) result->field_slots[at] = (int)i + 1;
        }
    }
    snapshots->slots[slot] = result;
    return result;
}

/* The next field of a walk, borrowed from its immutable snapshot, or NULL with
 * the walk's status: 0 after the last field, -1 for a malformed record. */
static const ZirTypeField *
next_field_persistent(CompilerThread *thread, const ZirType *record, size_t *cursor, int *status)
{
    *status = -1;
    if(record->is_enum || record->is_procedure_type) return NULL;
    RecordFields *fields;
    size_t index = 0;
    while(atomic_flag_test_and_set_explicit(&record_fields_lock, memory_order_acquire)) {}
    if(*cursor == 0) fields = fields_of(thread, record);
    else {
        FieldSnapshots *snapshots = context_fields(thread);
        size_t id = *cursor >> FIELD_CURSOR_BITS;
        fields = id > 0 && id <= snapshots->count ? snapshots->ids[id - 1] : NULL;
        index = *cursor & FIELD_CURSOR_MASK;
        ProfileCount("record_fields_reuse");
    }
    /* Snapshots never change after publication. Only their lookup tables can
     * move, so field copying and cursor iteration need no retained lock. */
    atomic_flag_clear_explicit(&record_fields_lock, memory_order_release);
    if(fields == NULL || fields->is_union != (record->is_union != 0) || index > fields->count)
        return NULL;
    if(index == fields->count) {
        *status = fields->terminal;
        return NULL;
    }
    *cursor = (fields->id << FIELD_CURSOR_BITS) | (index + 1);
    *status = 1;
    return &fields->items[index];
}

static int
TypeFindField_persistent(CompilerThread *thread, const ZirType *record, const char *name,
                         ZirTypeField *field)
{
    if(record == NULL || name == NULL || record->is_enum || record->is_procedure_type) {
        memset(field, 0, sizeof(*field));
        return -1;
    }
    while(atomic_flag_test_and_set_explicit(&record_fields_lock, memory_order_acquire)) {}
    RecordFields *fields = fields_of(thread, record);
    atomic_flag_clear_explicit(&record_fields_lock, memory_order_release);
    /* Both the parsed fields and their name index are immutable. A rewrite
     * or relocation of a record is resolved by its interned body, just as
     * a fresh TypeNextField cursor is. */
    if(fields != NULL && fields->field_slot_count) {
        int index = fields->field_slots[named_field_slot(fields, name)];
        if(index) {
            *field = fields->items[index - 1];
            ProfileCount("record_field_lookup");
            return 1;
        }
    }
    memset(field, 0, sizeof(*field));
    return fields == NULL || fields->terminal < 0 ? -1 : 0;
}

static int
TypeHasOnlyPrimitiveFields_persistent(CompilerThread *thread, const ZirType *record)
{
    if(record == NULL || record->is_enum || record->is_procedure_type) return 0;
    while(atomic_flag_test_and_set_explicit(&record_fields_lock, memory_order_acquire)) {}
    RecordFields *fields = fields_of(thread, record);
    atomic_flag_clear_explicit(&record_fields_lock, memory_order_release);
    /* This fact depends on source spelling only. Named types, arrays and
     * generic applications still require resolution in their owning scope. */
    return fields != NULL && fields->terminal == 0 && fields->count > 0 && fields->primitive_fields;
}

int SliceElementType(const char *type, char *element, size_t element_size)
{
    /* Only "[]T" is a slice; the VM asks this of every type it meets. */
    if(type == NULL || type[0] != '[' || type[1] != ']') return 0;
    ElementType result = compiler_type_SliceElement(type_text(type));
    return result.valid && copy_part(element, element_size, result.element);
}

size_t ScalarByteWidth(const char *type)
{
    return (size_t)compiler_type_ScalarByteWidth(type_text(type));
}

int UnionScalarFields(const ZirModule *module, const ZirType *record)
{
    if(record == NULL || !record->is_union) return 0;
    size_t cursor = 0;
    ZirTypeField field;
    int status;
    while((status = TypeNextField(record, &cursor, &field)) == 1) {
        const ZirType *enumeration = FindType(module, field.type, NULL);
        const char *backing = enumeration != NULL && enumeration->is_enum ?
            enumeration->enum_backing : field.type;
        if(ScalarByteWidth(backing) == 0) return 0;
    }
    return status == 0;
}

int ArrayTypeAlias(const char *value)
{
    return value != NULL && value[0] == '[' && strstr(value, ".[") == NULL;
}

int ArrayElementType(const char *type, char *element, size_t element_size, int *capacity)
{
    /* Only "[N]T" is an array; the VM asks this of every type it meets. */
    if(type == NULL || type[0] != '[') return 0;
    ElementType result = compiler_type_ArrayElement(type_text(type));
    if(!result.valid || !copy_part(element, element_size, result.element)) return 0;
    if(capacity != NULL) *capacity = (int)result.capacity;
    return 1;
}

/* The vector types compiler_type.zi's VectorType accepts, which every
 * backend spells the same way. The VM asks of each vector value it
 * coerces, so this reads a table rather than parsing the spelling. */
static const struct VectorSpelling {
    const char *name, *lane;
    int lanes, lane_bits;
} vector_types[] = {
    {"u8x16", "u8", 16, 8}, {"s8x16", "s8", 16, 8}, {"u16x8", "u16", 8, 16},
    {"s16x8", "s16", 8, 16}, {"u32x4", "u32", 4, 32}, {"s32x4", "s32", 4, 32},
    {"u64x2", "u64", 2, 64}, {"s64x2", "s64", 2, 64}, {"f32x4", "float32", 4, 32},
    {"f64x2", "float64", 2, 64}, {"u8x32", "u8", 32, 8}, {"s8x32", "s8", 32, 8},
    {"u16x16", "u16", 16, 16}, {"s16x16", "s16", 16, 16}, {"u32x8", "u32", 8, 32},
    {"s32x8", "s32", 8, 32}, {"u64x4", "u64", 4, 64}, {"s64x4", "s64", 4, 64},
    {"f32x8", "float32", 8, 32}, {"f64x4", "float64", 4, 64},
    /* Native vectors, with one block's lanes. */
    {"u8xN", "u8", 16, 8}, {"s8xN", "s8", 16, 8}, {"u16xN", "u16", 8, 16},
    {"s16xN", "s16", 8, 16}, {"u32xN", "u32", 4, 32}, {"s32xN", "s32", 4, 32},
    {"u64xN", "u64", 2, 64}, {"s64xN", "s64", 2, 64}, {"f32xN", "float32", 4, 32},
    {"f64xN", "float64", 2, 64}
};
enum { FIXED_VECTOR_TYPES = 20 };

int VectorType(const char *type, ZirVector *vector)
{
    if(type == NULL || (type[0] != 'u' && type[0] != 's' && type[0] != 'f'))
        return 0;
    for(size_t i = 0; i < sizeof(vector_types) / sizeof(*vector_types); i++) {
        const struct VectorSpelling *spelling = &vector_types[i];
        if(spelling->name[1] != type[1] || strcmp(type, spelling->name))
            continue;
        int native = i >= FIXED_VECTOR_TYPES;
        if(vector != NULL)
            *vector = (ZirVector){.name = spelling->name, .lane = spelling->lane,
                .lanes = spelling->lanes, .lane_bits = spelling->lane_bits,
                .bytes = spelling->lanes * spelling->lane_bits / 8,
                .is_signed = spelling->lane[0] == 's', .is_float = spelling->lane[0] == 'f',
                .is_native = native,
                .block = native ? vector_types[i - FIXED_VECTOR_TYPES].name : spelling->name};
        return 1;
    }
    return 0;
}

const char *VectorTypeName(const char *lane, int lanes)
{
    for(size_t i = 0; lane != NULL && i < FIXED_VECTOR_TYPES; i++)
        if(vector_types[i].lanes == lanes && !strcmp(vector_types[i].lane, lane))
            return vector_types[i].name;
    return NULL;
}

const char *NativeVectorName(const char *lane)
{
    for(size_t i = FIXED_VECTOR_TYPES; lane != NULL && i < sizeof(vector_types) / sizeof(*vector_types); i++)
        if(!strcmp(vector_types[i].lane, lane))
            return vector_types[i].name;
    return NULL;
}

int HoldsNativeVector(const char *type)
{
    if(type == NULL || VectorType(type, NULL)) return 0;
    for(const char *at = strstr(type, "xN"); at != NULL; at = strstr(at + 2, "xN")) {
        if(isalnum((unsigned char)at[2]) || at[2] == '_') continue;
        const char *start = at;
        while(start > type && (isalnum((unsigned char)start[-1]) || start[-1] == '_')) start--;
        char name[8];
        size_t length = (size_t)(at + 2 - start);
        if(length < sizeof(name)) {
            memcpy(name, start, length);
            name[length] = '\0';
            if(VectorType(name, NULL)) return 1;
        }
    }
    return 0;
}

int BuiltinTypeName(const char *name)
{
    return compiler_type_BuiltinName(type_text(name));
}

int MapPrimitiveName(const char *name)
{
    return compiler_type_MapPrimitive(type_text(name));
}

static const char *const bit_builtins[] = {
    NULL, "zi_clz", "zi_ctz", "zi_popcount", "zi_bswap"
};

int BitBuiltin(const char *checked_name)
{
    if(checked_name == NULL || strncmp(checked_name, "zi_", 3))
        return ZIR_BIT_NONE;
    for(int i = ZIR_BIT_CLZ; i <= ZIR_BIT_BSWAP; i++)
        if(!strcmp(checked_name, bit_builtins[i])) return i;
    return ZIR_BIT_NONE;
}

const char *BitBuiltinName(const char *source_name)
{
    for(int i = ZIR_BIT_CLZ; i <= ZIR_BIT_BSWAP; i++)
        if(source_name != NULL && !strcmp(source_name, bit_builtins[i] + 3))
            return bit_builtins[i];
    return NULL;
}

int GoForeignCallParts(const char *target, char *package, size_t package_size,
                      char *receiver, size_t receiver_size, char *symbol, size_t symbol_size)
{
    ForeignCall result = compiler_type_GoCall(type_text(target), ZIR_NAME_MAX);
    if(result.kind == 0 || !copy_part(package, package_size, result.module) ||
       !copy_part(receiver, receiver_size, result.receiver) ||
       !copy_part(symbol, symbol_size, result.symbol)) return 0;
    return result.kind;
}

int GoForeignTargetValid(const char *target)
{
    return compiler_type_GoTypeTarget(type_text(target));
}

int GoCHeader(const char *package, char *header, size_t header_size)
{
    if(strncmp(package, "C/", 2) || !package[2]) return 0;
    const char *path = package + 2;
    const char *segment = path;
    for(const char *p = path; ; p++) {
        if(*p == '/' || *p == '\0') {
            size_t length = (size_t)(p - segment);
            if(length == 0 || (length == 1 && segment[0] == '.') ||
               (length == 2 && segment[0] == '.' && segment[1] == '.')) return 0;
            if(!*p) break;
            segment = p + 1;
        } else if(!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                    (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '.')) {
            return 0;
        }
    }
    return copy_part(header, header_size, type_text(path));
}

int TypeNextFieldShared(const ZirType *record, size_t *cursor, const ZirTypeField **field)
{
    CompilerThread *thread = &CompilerThreadState;
    CompilerScope scope = CompilerThreadEnter(thread, record ? record->span.context : NULL);
    int status;
    *field = next_field_persistent(thread, record, cursor, &status);
    CompilerThreadLeave(thread, scope);
    return status;
}

int TypeNextField(const ZirType *record, size_t *cursor, ZirTypeField *field)
{
    const ZirTypeField *shared;
    int status = TypeNextFieldShared(record, cursor, &shared);
    if(shared) *field = *shared;
    else memset(field, 0, sizeof(*field));
    return status;
}

int TypeFindField(const ZirType *record, const char *name, ZirTypeField *field)
{
    CompilerThread *thread = &CompilerThreadState;
    CompilerScope scope = CompilerThreadEnter(thread, record ? record->span.context : NULL);
    int result = TypeFindField_persistent(thread, record, name, field);
    CompilerThreadLeave(thread, scope);
    return result;
}

int TypeHasOnlyPrimitiveFields(const ZirType *record)
{
    CompilerThread *thread = &CompilerThreadState;
    CompilerScope scope = CompilerThreadEnter(thread, record ? record->span.context : NULL);
    int result = TypeHasOnlyPrimitiveFields_persistent(thread, record);
    CompilerThreadLeave(thread, scope);
    return result;
}

int
ArrayValueType(const char *type)
{
    char element[ZIR_NAME_MAX];
    /* Host char buffers retain their explicit C-string interop convention. */
    return ArrayElementType(type, element, sizeof(element), NULL) &&
           strcmp(element, "char") != 0 && strcmp(element, "const char") != 0;
}
