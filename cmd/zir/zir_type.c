#include "zir.h"
#include "compiler_type.h"
#include <string.h>
#include <stdlib.h>

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
    int is_union, terminal;
    size_t id, count, capacity;
    ZirTypeField *items;
} RecordFields;
static struct {
    RecordFields **slots, **ids;
    size_t slots_count, count, capacity;
} record_fields;
enum { FIELD_CURSOR_BITS = 12, FIELD_CURSOR_MASK = (1 << FIELD_CURSOR_BITS) - 1 };

static size_t field_slot(RecordFields **slots, size_t count, const char *body, int is_union)
{
    size_t at = (((uintptr_t)body >> 3) * 31 + (unsigned)is_union) & (count - 1);
    while(slots[at] != NULL && (slots[at]->body != body || slots[at]->is_union != is_union))
        at = (at + 1) & (count - 1);
    return at;
}

static void free_record_fields(void)
{
    for(size_t i = 0; i < record_fields.count; i++) {
        free(record_fields.ids[i]->items);
        free(record_fields.ids[i]);
    }
    free(record_fields.ids);
    free(record_fields.slots);
    memset(&record_fields, 0, sizeof(record_fields));
}

static RecordFields *fields_of(const ZirType *record)
{
    size_t length = strnlen(record->body, sizeof(record->body));
    if(length == sizeof(record->body)) return NULL;
    const char *body = KeepText(record->body);
    int is_union = record->is_union != 0;
    if(record_fields.slots_count == 0 || record_fields.count * 2 >= record_fields.slots_count) {
        size_t count = record_fields.slots_count ? record_fields.slots_count * 2 : 128;
        RecordFields **slots = AllocateOrExit(count * sizeof(*slots));
        memset(slots, 0, count * sizeof(*slots));
        for(size_t i = 0; i < record_fields.count; i++) {
            RecordFields *entry = record_fields.ids[i];
            slots[field_slot(slots, count, entry->body, entry->is_union)] = entry;
        }
        if(record_fields.slots_count == 0) atexit(free_record_fields);
        free(record_fields.slots);
        record_fields.slots = slots;
        record_fields.slots_count = count;
    }
    size_t slot = field_slot(record_fields.slots, record_fields.slots_count, body, is_union);
    if(record_fields.slots[slot] != NULL) {
        ProfileCount("record_fields_reuse");
        return record_fields.slots[slot];
    }
    if(record_fields.count >= (SIZE_MAX >> FIELD_CURSOR_BITS)) return NULL;
    if(record_fields.count == record_fields.capacity) {
        size_t capacity = record_fields.capacity ? record_fields.capacity * 2 : 64;
        RecordFields **ids = AllocateOrExit(capacity * sizeof(*ids));
        if(record_fields.count) memcpy(ids, record_fields.ids, record_fields.count * sizeof(*ids));
        free(record_fields.ids);
        record_fields.ids = ids;
        record_fields.capacity = capacity;
    }
    RecordFields *result = AllocateOrExit(sizeof(*result));
    memset(result, 0, sizeof(*result));
    result->body = body;
    result->is_union = is_union;
    result->id = record_fields.count + 1;
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
        field->is_using = parsed.is_using;
    }
    record_fields.ids[record_fields.count++] = result;
    record_fields.slots[slot] = result;
    return result;
}

int TypeNextField(const ZirType *record, size_t *cursor, ZirTypeField *field)
{
    memset(field, 0, sizeof(*field));
    if(record->is_enum || record->is_procedure_type) return -1;
    RecordFields *fields;
    size_t index = 0;
    if(*cursor == 0) fields = fields_of(record);
    else {
        size_t id = *cursor >> FIELD_CURSOR_BITS;
        if(id == 0 || id > record_fields.count) return -1;
        fields = record_fields.ids[id - 1];
        index = *cursor & FIELD_CURSOR_MASK;
        ProfileCount("record_fields_reuse");
    }
    if(fields == NULL || fields->is_union != (record->is_union != 0) || index > fields->count)
        return -1;
    if(index == fields->count) return fields->terminal;
    *field = fields->items[index];
    *cursor = (fields->id << FIELD_CURSOR_BITS) | (index + 1);
    return 1;
}

int SliceElementType(const char *type, char *element, size_t element_size)
{
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

int ArrayElementType(const char *type, char *element, size_t element_size, int *capacity)
{
    ElementType result = compiler_type_ArrayElement(type_text(type));
    if(!result.valid || !copy_part(element, element_size, result.element)) return 0;
    if(capacity != NULL) *capacity = (int)result.capacity;
    return 1;
}

int BuiltinTypeName(const char *name)
{
    return compiler_type_BuiltinName(type_text(name));
}

int MapPrimitiveName(const char *name)
{
    return compiler_type_MapPrimitive(type_text(name));
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

int PyForeignCallParts(const char *target, char *module, size_t module_size,
                      char *receiver, size_t receiver_size, char *symbol, size_t symbol_size)
{
    ForeignCall result = compiler_type_PythonCall(type_text(target));
    if(result.kind == 0 || !copy_part(module, module_size, result.module) ||
       !copy_part(receiver, receiver_size, result.receiver) ||
       !copy_part(symbol, symbol_size, result.symbol)) return 0;
    return result.kind;
}

int PyForeignTargetValid(const char *target)
{
    return compiler_type_PythonCall(type_text(target)).kind == 1;
}
