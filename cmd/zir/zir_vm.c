#include "zir_vm.h"
#include "zir_check.h"
#include "zir_parse.h"
#include "zir_expr.h"
#include "zir_diagnostic.h"
#include "zir_text.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { VM_MAX_PARAMS = 16, VM_MAX_LOCALS = 64, VM_MAX_GLOBALS = 4096,
       VM_MAX_DEPTH = 128,
       VM_MAX_STEPS = 1000000, VM_MAX_FIELDS = 1024,
       VM_MAX_RECORD_BYTES = 256 * 1024 * 1024,
       VM_MAX_ARRAY_BYTES = 256 * 1024 * 1024 };

typedef struct Parameter {
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
} Parameter;

typedef enum ValueKind {
    VALUE_INVALID,
    VALUE_VOID,
    VALUE_INT,
    VALUE_REAL,
    VALUE_STRING,
    VALUE_ENUM,
    VALUE_RECORD,
    VALUE_ARRAY,
    VALUE_SLICE,
    VALUE_SLOT
} ValueKind;

typedef struct Record Record;
typedef struct Array Array;
typedef struct Frame Frame;

typedef struct Value {
    ValueKind kind;
    int64_t integer;
    uint64_t bits;
    int unsigned64;
    double real;
    const unsigned char *data;
    size_t length;
    size_t offset;
    const ZirType *enumeration;
    Record *record;
    Array *array;
    const ZirType *slot_type;
    const ZirModule *slot_module;
    const ZirFunction *slot_function;
} Value;

typedef struct RecordField {
    ZirTypeField field;
    Value value;
} RecordField;

struct Record {
    Record *next;
    int retired;
    uint64_t pinned;
    uint64_t allocation;
    const ZirModule *owner;
    const ZirType *type;
    int field_count;
    RecordField fields[];
};

struct Array {
    Array *next;
    int retired;
    uint64_t pinned;
    uint64_t allocation;
    const ZirModule *owner;
    char element_type[ZIR_NAME_MAX];
    int length;
    Value elements[];
};

typedef struct StringLiteral {
    struct StringLiteral *next;
    const ZirExpr *expression;
    size_t length;
    unsigned char data[];
} StringLiteral;

typedef struct Local {
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    Value value;
} Local;

typedef struct GlobalSlot {
    const ZirModule *module;
    const ZirGlobal *declaration;
    Value value;
} GlobalSlot;

typedef struct Vm {
    const ZirProgram *program;
    int depth;
    int steps;
    int failed;
    size_t record_bytes;
    size_t array_bytes;
    size_t allocated_since_collection;
    uint64_t allocation;
    uint64_t pin_generation;
    uint64_t retire_floor;
    Record *records;
    Array *arrays;
    StringLiteral *strings;
    GlobalSlot *globals;
    int global_count;
    Frame *active_frame;
    VmHostCall host;
    void *host_context;
} Vm;

struct VmInstance {
    Vm vm;
    const ZirModule *module;
    const ZirFunction *entry;
};

struct Frame {
    Vm *vm;
    const ZirModule *module;
    const ZirFunction *function;
    Frame *caller;
    Local locals[VM_MAX_LOCALS];
    int local_count;
    int control_target;
    /* Set while a union member is the assignment destination. */
    Record *union_write_record;
    const char *union_write_type;
};

static const ZirImport *
host_import(const ZirModule *module, const char *name)
{
    for(int i = 0; i < module->import_count; i++)
        if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
           strcmp(module->imports[i].name, name) == 0)
            return &module->imports[i];
    return NULL;
}

static const ZirFunction *
bound_provider(const ZirProgram *program, const ZirImport *import,
               const ZirModule **owner)
{
    const ZirFunction *found = NULL;
    *owner = NULL;
    if(program == NULL || import == NULL || import->extern_kind != ZIR_EXTERN_HOST ||
       strncmp(import->target, "ziran:", 6) != 0 ||
       import->target[6] == '\0' || import->extern_symbol[0] == '\0')
        return NULL;
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *candidate = &program->modules[m];
        if(strcmp(candidate->name, import->target + 6) != 0)
            continue;
        for(int f = 0; f < candidate->function_count; f++) {
            const ZirFunction *function = &candidate->functions[f];
            if(strcmp(function->name, import->extern_symbol) != 0 ||
               !function->exported || function->is_extern)
                continue;
            if(found != NULL)
                return NULL;
            found = function;
            *owner = candidate;
        }
    }
    return found;
}

static int
same_bound_type(const ZirModule *caller, const ZirModule *provider,
                const char *type)
{
    const ZirType *left = FindType(caller, type, NULL);
    const ZirType *right = FindType(provider, type, NULL);
    return left == right;
}

typedef enum Flow {
    FLOW_NEXT,
    FLOW_RETURN,
    FLOW_BREAK,
    FLOW_CONTINUE,
    FLOW_ERROR
} Flow;

static ValueKind
value_kind(const char *type)
{
    if(strcmp(type, "void") == 0)
        return VALUE_VOID;
    /* Raw pointers are opaque host handles in portable bundles; null is the
     * empty handle. The checker still rejects dereferencing and arithmetic. */
    if(type[0] == '*' || strcmp(type, "null") == 0)
        return VALUE_INT;
    if(strcmp(type, "s8") == 0 || strcmp(type, "s16") == 0 ||
       strcmp(type, "s32") == 0 || strcmp(type, "s64") == 0 ||
       strcmp(type, "u8") == 0 || strcmp(type, "u16") == 0 ||
       strcmp(type, "u32") == 0 || strcmp(type, "u64") == 0 ||
       strcmp(type, "integer") == 0 || strcmp(type, "bool") == 0)
        return VALUE_INT;
    if(strcmp(type, "float32") == 0 || strcmp(type, "float64") == 0 ||
       strcmp(type, "real") == 0)
        return VALUE_REAL;
    if(strcmp(type, "string") == 0)
        return VALUE_STRING;
    return VALUE_INVALID;
}

static int
scalar_type(const char *type)
{
    return value_kind(type) != VALUE_INVALID;
}


/* Portable unions overlap scalar storage: every field reads and writes the
 * same bytes at offset zero. */
static size_t
vm_union_field_width(const ZirModule *module, const char *type)
{
    const ZirType *enumeration = FindType(module, type, NULL);
    const char *backing = type;
    if(enumeration != NULL && enumeration->is_enum)
        backing = enumeration->enum_backing;
    if(!strcmp(backing, "s8") || !strcmp(backing, "u8") ||
       !strcmp(backing, "bool"))
        return 1;
    if(!strcmp(backing, "s16") || !strcmp(backing, "u16"))
        return 2;
    if(!strcmp(backing, "s32") || !strcmp(backing, "u32") ||
       !strcmp(backing, "float32"))
        return 4;
    if(!strcmp(backing, "s64") || !strcmp(backing, "u64") ||
       !strcmp(backing, "float64") || !strcmp(backing, "integer"))
        return 8;
    return 0;
}

static int
portable_union(const ZirModule *module, const ZirType *record)
{
    size_t offset = 0;
    ZirTypeField field;
    int status;
    if(record == NULL || !record->is_union)
        return 0;
    while((status = TypeNextField(record, &offset, &field)) == 1)
        if(vm_union_field_width(module, field.type) == 0)
            return 0;
    return status == 0;
}

typedef struct VmTypePath {
    const ZirType *record;
    const struct VmTypePath *parent;
} VmTypePath;

static int
portable_type_at(const ZirModule *module, const char *type, int depth,
                 const VmTypePath *path)
{
    const ZirModule *owner = NULL;
    const ZirType *record;
    char element[ZIR_NAME_MAX];
    int capacity;
    size_t offset = 0;
    ZirTypeField field;
    int count = 0;
    int status;
    if(scalar_type(type))
        return 1;
    if(depth >= VM_MAX_DEPTH)
        return 0;
    if(SliceElementType(type, element, sizeof(element))) {
        const ZirType *element_type = FindType(module, element, NULL);
        /* A slice refers to its elements, so a record may hold a slice of
         * itself; the record's own fields are already being checked. */
        for(const VmTypePath *ancestor = path; element_type && ancestor;
            ancestor = ancestor->parent)
            if(ancestor->record == element_type)
                return 1;
        return element[0] != '[' &&
               (element_type == NULL || !element_type->is_procedure_type) &&
               portable_type_at(module, element, depth + 1, path);
    }
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        return capacity >= 0 &&
               (size_t)capacity <= VM_MAX_ARRAY_BYTES / sizeof(Value) &&
               portable_type_at(module, element, depth + 1, path);
    }
    record = FindType(module, type, &owner);
    if(record == NULL || record->is_extern)
        return 0;
    if(VecElementType(module, type, element, sizeof(element)))
        return portable_type_at(module, element, depth + 1, path) &&
               !VecElementType(module, element, NULL, 0);
    if(record->is_procedure_type) {
        if(record->is_c_call)
            return 0;
        const char *cursor = record->body;
        int parameters = 0;
        while(*cursor) {
            char parameter_type[ZIR_NAME_MAX];
            size_t length = 0;
            while(*cursor == ' ' || *cursor == '\t')
                cursor++;
            if(*cursor == 0)
                break;
            if(!isalpha((unsigned char)*cursor) && *cursor != '_')
                return 0;
            while(isalnum((unsigned char)*cursor) || *cursor == '_')
                cursor++;
            while(*cursor == ' ' || *cursor == '\t')
                cursor++;
            if(*cursor++ != ':')
                return 0;
            while(*cursor == ' ' || *cursor == '\t')
                cursor++;
            while(isalnum((unsigned char)*cursor) || *cursor == '_') {
                if(length + 1 >= sizeof(parameter_type))
                    return 0;
                parameter_type[length++] = *cursor++;
            }
            parameter_type[length] = 0;
            if(length == 0 || ++parameters > VM_MAX_PARAMS ||
               strcmp(parameter_type, "void") == 0 ||
               !portable_type_at(owner, parameter_type, depth + 1, path))
                return 0;
            while(*cursor == ' ' || *cursor == '\t')
                cursor++;
            if(*cursor == 0)
                break;
            if(*cursor++ != ',' || *cursor == 0)
                return 0;
        }
        return portable_type_at(owner, record->procedure_return_type, depth + 1, path);
    }
    if(record->is_enum)
        return EnumMemberValue(record, NULL, NULL);
    VmTypePath current = {record, path};
    while((status = TypeNextField(record, &offset, &field)) == 1) {
        if(++count > VM_MAX_FIELDS || strcmp(field.type, "void") == 0 ||
           !portable_type_at(owner, field.type, depth + 1, &current))
            return 0;
    }
    return status == 0;
}

static int
portable_type(const ZirModule *module, const char *type)
{
    return portable_type_at(module, type, 0, NULL);
}

/* Host values have a declared, recursive record or array shape. A slice
 * parameter is copied into host values and copied back after the synchronous
 * call. Fixed arrays are copied by value. */
static int
host_type_at(const ZirModule *module, const char *type, int depth,
             int slice_parameter)
{
    char element[ZIR_NAME_MAX];
    int capacity;
    if(depth >= VM_MAX_DEPTH)
        return 0;
    if(ArrayElementType(type, element, sizeof(element), &capacity))
        return capacity >= 0 &&
               (size_t)capacity <= VM_MAX_ARRAY_BYTES / sizeof(Value) &&
               host_type_at(module, element, depth + 1, 0);
    if(SliceElementType(type, element, sizeof(element)))
        return element[0] != '[' && element[0] != '\0' &&
               host_type_at(module, element, depth + 1, 0);
    if(value_kind(type) != VALUE_INVALID)
        return 1;
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL || record->is_extern || record->is_procedure_type)
        return 0;
    if(record->is_enum)
        return EnumMemberValue(record, NULL, NULL);
    size_t offset = 0;
    ZirTypeField field;
    int count = 0, status;
    while((status = TypeNextField(record, &offset, &field)) == 1)
        if(++count > VM_MAX_FIELDS || !strcmp(field.type, "void") ||
           !host_type_at(owner, field.type, depth + 1, 0))
            return 0;
    return status == 0;
}

static Value
int_value(int64_t integer)
{
    Value value = {.kind = VALUE_INT, .integer = integer,
                   .bits = (uint64_t)integer};
    return value;
}

static Value
uint_value(uint64_t bits)
{
    Value value = {.kind = VALUE_INT, .bits = bits, .unsigned64 = 1};
    return value;
}

static Value
real_value(double real)
{
    Value value = {.kind = VALUE_REAL, .real = real};
    return value;
}

static Value
string_value(const unsigned char *data, size_t length)
{
    Value value = {.kind = VALUE_STRING, .data = data, .length = length};
    return value;
}


static Value
literal_string(Vm *vm, const ZirExpr *expression)
{
    for(StringLiteral *item = vm->strings; item != NULL; item = item->next)
        if(item->expression == expression)
            return string_value(item->data, item->length);
    size_t capacity = strlen(expression->text);
    StringLiteral *item = malloc(sizeof(*item) + capacity + 1);
    if(item == NULL || !DecodeStringLiteral(expression->text, item->data,
                                      capacity + 1, &item->length)) {
        free(item);
        vm->failed = 1;
        return string_value((const unsigned char *)"", 0);
    }
    item->expression = expression;
    item->next = vm->strings;
    vm->strings = item;
    return string_value(item->data, item->length);
}

static Value
global_literal_string(Vm *vm, const char *source)
{
    size_t capacity = strlen(source);
    StringLiteral *item = malloc(sizeof(*item) + capacity + 1);
    if(item == NULL || !DecodeStringLiteral(source, item->data,
                                              capacity + 1, &item->length)) {
        free(item);
        vm->failed = 1;
        return string_value((const unsigned char *)"", 0);
    }
    item->expression = NULL;
    item->next = vm->strings;
    vm->strings = item;
    return string_value(item->data, item->length);
}

static Value
enum_value(const ZirType *type, int64_t integer)
{
    Value value = {.kind = VALUE_ENUM, .integer = integer,
                   .bits = (uint64_t)(int64_t)integer,
                   .enumeration = type};
    return value;
}

static uint64_t
integer_bits(Value value)
{
    return value.unsigned64 ? value.bits : (uint64_t)value.integer;
}

static int64_t
signed64(uint64_t bits)
{
    return bits <= INT64_MAX ? (int64_t)bits :
           -1 - (int64_t)(UINT64_MAX - bits);
}

static double
as_real(Value value)
{
    if(value.kind == VALUE_REAL)
        return value.real;
    return value.unsigned64 ? (double)value.bits : (double)value.integer;
}

static int
truthy(Value value)
{
    return value.kind == VALUE_REAL ? value.real != 0.0 :
           integer_bits(value) != 0;
}

static Record *
allocate_record(Vm *vm, const ZirModule *owner,
                const ZirType *type, int count)
{
    size_t bytes = sizeof(Record) + (size_t)count * sizeof(RecordField);
    if(count < 0 || count > VM_MAX_FIELDS ||
       bytes > VM_MAX_RECORD_BYTES - vm->record_bytes) {
        vm->failed = 1;
        return NULL;
    }
    Record *record = calloc(1, bytes);
    if(record == NULL) {
        vm->failed = 1;
        return NULL;
    }
    record->next = vm->records;
    record->allocation = ++vm->allocation;
    record->owner = owner;
    record->type = type;
    record->field_count = count;
    vm->records = record;
    vm->record_bytes += bytes;
    vm->allocated_since_collection += bytes;
    return record;
}

static Array *
allocate_array_try(Vm *vm, const ZirModule *owner, const char *element,
                   int length, int fail_hard)
{
    if(length < 0 || (size_t)length >
       (VM_MAX_ARRAY_BYTES - sizeof(Array)) / sizeof(Value)) {
        if(fail_hard) vm->failed = 1;
        return NULL;
    }
    size_t bytes = sizeof(Array) + (size_t)length * sizeof(Value);
    if(bytes > VM_MAX_ARRAY_BYTES - vm->array_bytes) {
        if(fail_hard) vm->failed = 1;
        return NULL;
    }
    Array *array = calloc(1, bytes);
    if(array == NULL) {
        if(fail_hard) vm->failed = 1;
        return NULL;
    }
    array->next = vm->arrays;
    array->allocation = ++vm->allocation;
    array->owner = owner;
    copy_text(array->element_type, sizeof(array->element_type), element);
    array->length = length;
    vm->arrays = array;
    vm->array_bytes += bytes;
    vm->allocated_since_collection += bytes;
    return array;
}

static Array *
allocate_array(Vm *vm, const ZirModule *owner, const char *element,
               int length)
{
    return allocate_array_try(vm, owner, element, length, 1);
}

static Value default_value(Vm *vm, const ZirModule *module,
                           const char *type, int depth);

static Value
clone_value(Vm *vm, Value value, int depth)
{
    if(vm->failed || (value.kind != VALUE_RECORD &&
                      value.kind != VALUE_ARRAY))
        return value;
    if(value.kind == VALUE_ARRAY) {
        /* A default-initialized Vec or cleared slice owns no storage; its
         * clone is equally empty. */
        if(value.array == NULL)
            return value;
        if(depth >= VM_MAX_DEPTH) {
            vm->failed = 1;
            return int_value(0);
        }
        Array *copy = allocate_array(vm, value.array->owner,
                                     value.array->element_type,
                                     value.array->length);
        if(copy == NULL)
            return int_value(0);
        for(int i = 0; i < copy->length && !vm->failed; i++)
            copy->elements[i] = clone_value(vm,
                value.array->elements[i], depth + 1);
        return (Value){.kind = VALUE_ARRAY, .array = copy};
    }
    if(value.record == NULL || depth >= VM_MAX_DEPTH) {
        vm->failed = 1;
        return int_value(0);
    }
    Record *copy = allocate_record(vm, value.record->owner,
                                   value.record->type,
                                   value.record->field_count);
    if(copy == NULL)
        return int_value(0);
    for(int i = 0; i < copy->field_count && !vm->failed; i++) {
        copy->fields[i].field = value.record->fields[i].field;
        copy->fields[i].value = clone_value(vm,
            value.record->fields[i].value, depth + 1);
    }
    return (Value){.kind = VALUE_RECORD, .record = copy};
}

static Value
default_value(Vm *vm, const ZirModule *module, const char *type, int depth)
{
    ValueKind kind = value_kind(type);
    char element[ZIR_NAME_MAX];
    int capacity;
    if(kind == VALUE_INT)
        return int_value(0);
    if(kind == VALUE_REAL)
        return real_value(0.0);
    if(kind == VALUE_STRING)
        return string_value((const unsigned char *)"", 0);
    if(SliceElementType(type, element, sizeof(element)))
        return (Value){.kind = VALUE_SLICE};
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        if(depth >= VM_MAX_DEPTH || capacity < 0) {
            vm->failed = 1;
            return int_value(0);
        }
        Array *array = allocate_array(vm, module, element, capacity);
        if(array == NULL)
            return int_value(0);
        for(int i = 0; i < capacity && !vm->failed; i++)
            array->elements[i] = default_value(vm, module, element,
                                               depth + 1);
        return (Value){.kind = VALUE_ARRAY, .array = array};
    }
    const ZirModule *owner = NULL;
    const ZirType *record_type = FindType(module, type, &owner);
    if(record_type != NULL && record_type->is_enum)
        return enum_value(record_type, 0);
    if(record_type != NULL && record_type->is_procedure_type)
        return (Value){.kind = VALUE_SLOT, .slot_type = record_type};
    if(kind != VALUE_INVALID || record_type == NULL ||
       record_type->is_procedure_type ||
       record_type->is_extern || depth >= VM_MAX_DEPTH) {
        vm->failed = 1;
        return int_value(0);
    }
    size_t offset = 0;
    ZirTypeField field;
    int count = 0;
    int status;
    while((status = TypeNextField(record_type, &offset, &field)) == 1)
        count++;
    if(status < 0 || count > VM_MAX_FIELDS) {
        vm->failed = 1;
        return int_value(0);
    }
    Record *record = allocate_record(vm, owner, record_type, count);
    if(record == NULL)
        return int_value(0);
    offset = 0;
    for(int i = 0; i < count && !vm->failed; i++) {
        if(TypeNextField(record_type, &offset, &record->fields[i].field) != 1) {
            vm->failed = 1;
            break;
        }
        if(i == 0 && VecElementType(module, type, NULL, 0))
            record->fields[i].value = (Value){.kind = VALUE_ARRAY};
        else
            record->fields[i].value = default_value(vm, owner,
                record->fields[i].field.type, depth + 1);
    }
    return (Value){.kind = VALUE_RECORD, .record = record};
}

static Value
coerce(Vm *vm, const ZirModule *module, Value value, const char *type)
{
    ValueKind target = value_kind(type);
    if(target == VALUE_VOID) {
        Value empty = {.kind = VALUE_VOID};
        return empty;
    }
    if(target == VALUE_STRING) {
        if(value.kind == VALUE_STRING)
            return value;
        vm->failed = 1;
        return string_value((const unsigned char *)"", 0);
    }
    if(target == VALUE_INVALID) {
        char element[ZIR_NAME_MAX];
        int capacity;
        if(SliceElementType(type, element, sizeof(element))) {
            if(value.kind == VALUE_SLICE &&
               (value.array == NULL ||
                strcmp(value.array->element_type, element) == 0))
                return value;
            vm->failed = 1;
            return int_value(0);
        }
        if(ArrayElementType(type, element, sizeof(element), &capacity)) {
            if(value.kind != VALUE_ARRAY || value.array == NULL ||
               capacity != value.array->length) {
                vm->failed = 1;
                return int_value(0);
            }
            Array *copy = allocate_array(vm, module, element, capacity);
            if(copy == NULL)
                return int_value(0);
            for(int i = 0; i < capacity && !vm->failed; i++)
                copy->elements[i] = coerce(vm, module,
                    value.array->elements[i], element);
            return (Value){.kind = VALUE_ARRAY, .array = copy};
        }
        const ZirType *record = FindType(module, type, NULL);
        if(record != NULL && record->is_procedure_type &&
           value.kind == VALUE_SLOT && value.slot_type == record)
            return value;
        if(record != NULL && record->is_enum &&
           value.kind != VALUE_RECORD && value.kind != VALUE_VOID &&
           value.kind != VALUE_INVALID) {
            value = coerce(vm, module, value, record->enum_backing);
            return enum_value(record, value.integer);
        }
        if(record != NULL && !record->is_enum && !record->is_procedure_type &&
           !record->is_extern && value.kind == VALUE_RECORD &&
           value.record != NULL && value.record->type == record)
            return VecElementType(module, type, NULL, 0) ?
                value : clone_value(vm, value, 0);
        vm->failed = 1;
        return int_value(0);
    }
    if(value.kind == VALUE_STRING || value.kind == VALUE_RECORD ||
       value.kind == VALUE_ARRAY || value.kind == VALUE_SLICE ||
       value.kind == VALUE_SLOT ||
       value.kind == VALUE_VOID ||
       value.kind == VALUE_INVALID) {
        vm->failed = 1;
        return int_value(0);
    }
    if(strcmp(type, "bool") == 0)
        return int_value(truthy(value));
    if(type[0] == '*')
        return uint_value(integer_bits(value));
    if(strcmp(type, "integer") == 0)
        return value;
    if(target == VALUE_REAL) {
        double number = as_real(value);
        if(strcmp(type, "float32") == 0)
            number = (float)number;
        return real_value(number);
    }
    if(value.kind == VALUE_REAL) {
        int unsigned_type = strcmp(type, "u32") == 0 ||
                            strcmp(type, "u16") == 0 ||
                            strcmp(type, "u8") == 0 ||
                            strcmp(type, "u64") == 0;
        double lower = unsigned_type ? 0.0 :
                       strcmp(type, "s64") == 0 ? -9223372036854775808.0 :
                       INT32_MIN;
        double upper = strcmp(type, "u64") == 0 ?
                       18446744073709551616.0 :
                       strcmp(type, "s64") == 0 ?
                       9223372036854775808.0 :
                       strcmp(type, "u32") == 0 ?
                       (double)UINT32_MAX + 1.0 :
                       strcmp(type, "u16") == 0 ? 65536.0 :
                       strcmp(type, "u8") == 0 ? 256.0 :
                       strcmp(type, "s16") == 0 ? 32768.0 :
                       strcmp(type, "s8") == 0 ? 128.0 :
                       (double)INT32_MAX + 1.0;
        if(strcmp(type, "s8") == 0) lower = -128.0;
        if(strcmp(type, "s16") == 0) lower = -32768.0;
        if(!isfinite(value.real) || value.real < lower ||
           value.real >= upper) {
            vm->failed = 1;
            return int_value(0);
        }
        if(strcmp(type, "u64") == 0)
            return uint_value((uint64_t)value.real);
        if(strcmp(type, "s64") == 0)
            return int_value((int64_t)value.real);
        if(strcmp(type, "u32") == 0)
            return int_value((uint32_t)value.real);
        if(strcmp(type, "u8") == 0)
            return int_value((uint8_t)value.real);
        if(strcmp(type, "u16") == 0)
            return int_value((uint16_t)value.real);
        if(strcmp(type, "s8") == 0)
            return int_value((int8_t)value.real);
        if(strcmp(type, "s16") == 0)
            return int_value((int16_t)value.real);
        return int_value((int32_t)value.real);
    }
    if(strcmp(type, "u64") == 0)
        return uint_value(integer_bits(value));
    if(strcmp(type, "s64") == 0)
        return int_value(signed64(integer_bits(value)));
    uint32_t bits = (uint32_t)integer_bits(value);
    if(strcmp(type, "u32") == 0)
        return int_value(bits);
    if(strcmp(type, "u8") == 0)
        return int_value((uint8_t)bits);
    if(strcmp(type, "u16") == 0)
        return int_value((uint16_t)bits);
    if(strcmp(type, "s8") == 0)
        return int_value((int8_t)bits);
    if(strcmp(type, "s16") == 0)
        return int_value((int16_t)bits);
    return int_value(bits <= INT32_MAX ? (int64_t)bits :
                     (int64_t)bits - 4294967296LL);
}

/* Expression values are read-only until a declaration, assignment, or call
 * parameter stores them. Those storage boundaries use coerce() and make the
 * required value copy. Borrowing matching records and arrays here avoids a
 * deep temporary copy for every member read and function argument. */
static Value
coerce_expression(Vm *vm, const ZirModule *module,
                  Value value, const char *type)
{
    if(value.kind == VALUE_RECORD && value.record != NULL) {
        const ZirType *record = FindType(module, type, NULL);
        if(record != NULL && !record->is_enum && !record->is_procedure_type &&
           !record->is_extern && value.record->type == record)
            return value;
    }
    if(value.kind == VALUE_ARRAY && value.array != NULL) {
        char element[ZIR_NAME_MAX];
        int capacity;
        if(ArrayElementType(type, element, sizeof(element), &capacity) &&
           capacity == value.array->length &&
           strcmp(element, value.array->element_type) == 0)
            return value;
    }
    return coerce(vm, module, value, type);
}

/* A storage assignment clones its replacement before it discards the old
 * value. Owned record and array trees can then be reclaimed immediately;
 * this bounds repeated updates of large value records. */
static void
retire_value(Vm *vm, Value value, int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return;
    if(value.kind == VALUE_RECORD && value.record != NULL &&
       !value.record->retired) {
        value.record->retired = 1;
        if(vm->retire_floor == 0 ||
           value.record->allocation < vm->retire_floor)
            vm->retire_floor = value.record->allocation;
        for(int i = 0; i < value.record->field_count; i++)
            retire_value(vm, value.record->fields[i].value, depth + 1);
    } else if(value.kind == VALUE_ARRAY && value.array != NULL &&
              !value.array->retired) {
        value.array->retired = 1;
        if(vm->retire_floor == 0 ||
           value.array->allocation < vm->retire_floor)
            vm->retire_floor = value.array->allocation;
        for(int i = 0; i < value.array->length; i++)
            retire_value(vm, value.array->elements[i], depth + 1);
    }
}

static void pin_value(Vm *vm, Value value, int depth);

static int
array_has_active_slice(Vm *vm, const Array *array)
{
    for(Frame *frame = vm->active_frame; frame != NULL;
        frame = frame->caller) {
        for(int i = 0; i < frame->local_count; i++) {
            Value value = frame->locals[i].value;
            if(value.kind == VALUE_SLICE && value.array == array)
                return 1;
        }
    }
    return 0;
}

static void
release_retired(Vm *vm)
{
    if(vm->retire_floor == 0)
        return;
    uint64_t floor = vm->retire_floor;
    uint64_t remaining = 0;
    vm->pin_generation++;
    for(Frame *frame = vm->active_frame; frame != NULL;
        frame = frame->caller) {
        for(int i = 0; i < frame->local_count; i++)
            if(frame->locals[i].value.kind == VALUE_SLICE)
                pin_value(vm, frame->locals[i].value, 0);
    }
    Record **record = &vm->records;
    while(*record != NULL && (*record)->allocation >= floor) {
        if((*record)->retired &&
           (*record)->pinned != vm->pin_generation) {
            Record *dead = *record;
            *record = dead->next;
            vm->record_bytes -= sizeof(Record) +
                (size_t)dead->field_count * sizeof(RecordField);
            free(dead);
        } else {
            if((*record)->retired &&
               (remaining == 0 || (*record)->allocation < remaining))
                remaining = (*record)->allocation;
            record = &(*record)->next;
        }
    }
    Array **array = &vm->arrays;
    while(*array != NULL && (*array)->allocation >= floor) {
        if((*array)->retired &&
           (*array)->pinned != vm->pin_generation) {
            Array *dead = *array;
            *array = dead->next;
            vm->array_bytes -= sizeof(Array) +
                (size_t)dead->length * sizeof(Value);
            free(dead);
        } else {
            if((*array)->retired &&
               (remaining == 0 || (*array)->allocation < remaining))
                remaining = (*array)->allocation;
            array = &(*array)->next;
        }
    }
    vm->retire_floor = remaining;
}

static int
vm_type_contains_vec(const ZirModule *module, const char *type, int depth)
{
    char element[ZIR_NAME_MAX];
    const ZirModule *owner = NULL;
    const ZirType *record = NULL;
    if(depth > 32 || module == NULL || type == NULL || !*type || *type == '*')
        return 0;
    if(VecElementType(module, type, NULL, 0))
        return 1;
    if(ArrayElementType(type, element, sizeof(element), NULL))
        return vm_type_contains_vec(module, element, depth + 1);
    record = FindType(module, type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template || record->is_extern)
        return 0;
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1)
        if(vm_type_contains_vec(owner ? owner : module,
                                field.type, depth + 1))
            return 1;
    return 0;
}

/* An owned Vec or aggregate owns its reachable record and array storage.
 * Release it when the binding leaves scope; borrowed slices in surviving
 * frames keep their backing pinned until those views leave scope as well. */
static void
drop_owned_locals(Frame *frame, int first)
{
    int retired = 0;
    for(int i = frame->local_count - 1; i >= first; i--) {
        Local *local = &frame->locals[i];
        if(vm_type_contains_vec(frame->module, local->type, 0) &&
           local->value.kind == VALUE_RECORD) {
            retire_value(frame->vm, local->value, 0);
            retired = 1;
        }
    }
    frame->local_count = first;
    if(retired)
        release_retired(frame->vm);
}

static int
parse_parameters(const ZirModule *module, const ZirFunction *function,
                 Parameter *parameters)
{
    const char *cursor = function->args;
    int count = 0;
    while(*cursor != 0) {
        const char *start;
        size_t length;
        while(*cursor != '\0' && isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor == 0)
            break;
        if(count >= VM_MAX_PARAMS)
            return -1;
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
              (*cursor >= '0' && *cursor <= '9') || *cursor == '_')
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

static int
parse_import_parameters(const ZirModule *module, const ZirImport *import,
                        Parameter *parameters)
{
    ZirFunction signature = {0};
    copy_text(signature.args, sizeof(signature.args), import->args);
    return parse_parameters(module, &signature, parameters);
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
        if(expression->kind != ZIR_EXPR_MEMBER &&
           expression->kind != ZIR_EXPR_INDEX)
            return NULL;
        index = expression->left;
    }
    return NULL;
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

static int
bitwise_operator(const char *op)
{
    return strcmp(op, "&") == 0 || strcmp(op, "|") == 0 ||
           strcmp(op, "^") == 0 || strcmp(op, "<<") == 0 ||
           strcmp(op, ">>") == 0;
}

static const char *
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
    const ZirType *source = FindType(declaration_module, declared, NULL);
    const ZirType *resolved = FindType(use_module, checked, NULL);
    return source != NULL && source == resolved;
}

static int
verify_expression(const ZirModule *module, const ZirFunction *function,
                  const Parameter *bindings, int binding_count,
                  int index, int depth)
{
    const ZirExpr *expression;
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    int children = 0;
    Parameter parameters[VM_MAX_PARAMS];
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
        unsigned char bytes[ZIR_TEXT_MAX];
        size_t length;
        return strcmp(expression->type, "string") == 0 &&
               DecodeStringLiteral(expression->text, bytes, sizeof(bytes), &length);
    }
    case ZIR_EXPR_COMPILE_TIME:
        return strcmp(expression->type, "bool") == 0 &&
               strcmp(expression->text, "#compile_time") == 0 &&
               expression->left == -1 && expression->right == -1 &&
               expression->third == -1 && expression->first_child == -1;
    case ZIR_EXPR_IDENT: {
        if(expression->is_function_value) {
            const ZirType *slot = FindType(module, expression->type, NULL);
            ZirFunction signature = {0};
            Parameter actual[VM_MAX_PARAMS], expected[VM_MAX_PARAMS];
            int actual_count, expected_count;
            if(slot == NULL || !slot->is_procedure_type ||
               ResolveFunction(module, expression->name,
                               &owner, &callee) != 1 ||
               callee == NULL || callee->is_extern ||
               strlen(slot->body) >= sizeof(signature.args) ||
               strcmp(callee->return_type, slot->procedure_return_type) != 0)
                return 0;
            copy_text(signature.args, sizeof(signature.args), slot->body);
            actual_count = parse_parameters(owner, callee, actual);
            expected_count = parse_parameters(module, &signature, expected);
            if(actual_count < 0 || actual_count != expected_count)
                return 0;
            for(int i = 0; i < actual_count; i++) {
                const ZirType *actual_type = FindType(owner, actual[i].type, NULL);
                const ZirType *expected_type = FindType(module, expected[i].type, NULL);
                if(actual_type != NULL || expected_type != NULL) {
                    if(actual_type != expected_type)
                        return 0;
                } else if(strcmp(actual[i].type, expected[i].type) != 0)
                    return 0;
            }
            return 1;
        }
        if(strcmp(expression->name, "true") == 0 ||
           strcmp(expression->name, "false") == 0 ||
           strcmp(expression->name, "null") == 0 ||
           binding_index(bindings, binding_count, expression->name) >= 0 ||
           find_global_declaration(module, expression->name,
                                   expression->span.path) != NULL)
            return 1;
        return 0;
    }
    case ZIR_EXPR_UNARY:
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
        unsigned char seen[VM_MAX_FIELDS] = {0};
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
                    if(seen[field_index] ||
                       !same_verified_type(record_owner, field.type,
                                           module, initializer->type))
                        return 0;
                    seen[field_index] = 1;
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
            ZirFunction signature = {0};
            if(index < 0 || slot == NULL || !slot->is_procedure_type ||
               strcmp(bindings[index].type, expression->slot_type) != 0 ||
               strlen(slot->body) >= sizeof(signature.args) ||
               strcmp(expression->type, slot->procedure_return_type) != 0)
                return 0;
            copy_text(signature.args, sizeof(signature.args), slot->body);
            int expected = parse_parameters(module, &signature, parameters);
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
            parse_import_parameters(module, external, parameters) :
            parse_parameters(owner, callee, parameters);
        return expected >= 0 && expected == children &&
               used == ((1u << expected) - 1u);
    default:
        return 0;
    }
}

static const ZirFunction *
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

static int
is_else_branch(const ZirStmt *statement)
{
    return statement->kind == ZIR_STMT_IF && statement->is_else;
}

static int
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
            if(!portable_type(module, statement->type) ||
               strcmp(statement->type, "void") == 0 ||
               statement->name[0] == 0 || binding_count >= VM_MAX_LOCALS ||
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
                    function->exprs[statement->lhs_root].span.path) == NULL) ||
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

static int
sequence_guarantees_return(const ZirFunction *function, int begin, int end,
                           int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return 0;
    for(int i = begin; i < end; i++) {
        const ZirStmt *statement = &function->stmts[i];
        int close;
        if(statement->kind == ZIR_STMT_RETURN)
            return 1;
        if(statement->kind == ZIR_STMT_UNREACHABLE)
            return 1;
        if(statement->kind == ZIR_STMT_IF &&
           !is_else_branch(statement)) {
            int branch = i;
            int all_return = 1;
            int has_else = 0;
            do {
                close = statement_close(function, branch, end);
                if(close < 0)
                    return 0;
                all_return &= sequence_guarantees_return(function,
                              branch + 1, close, depth + 1);
                has_else |= function->stmts[branch].expr_root < 0;
                branch = close + 1;
            } while(branch < end &&
                    is_else_branch(&function->stmts[branch]));
            if(all_return && has_else)
                return 1;
            i = branch - 1;
        } else if(statement->kind == ZIR_STMT_BLOCK_OPEN) {
            close = statement_close(function, i, end);
            if(close < 0)
                return 0;
            if(sequence_guarantees_return(function, i + 1, close, depth + 1))
                return 1;
            i = close;
        } else if(statement->kind == ZIR_STMT_WHILE) {
            close = statement_close(function, i, end);
            if(close < 0)
                return 0;
            i = close;
        } else if(statement->kind == ZIR_STMT_BREAK ||
                  statement->kind == ZIR_STMT_CONTINUE) {
            return 0;
        }
    }
    return 0;
}

static int
fold_global_element(Vm *vm, const ZirModule *module, const ZirFunction *probe,
                    int index, Value *target, const char *type,
                    ZirSourceSpan span);
static void free_records(Vm *vm);
static void free_arrays(Vm *vm);
static void free_strings(Vm *vm);

/* Check aggregate literals with the same evaluator used during instance
 * creation, so nested runtime expressions fail before a bundle is written. */
static int
verify_global_aggregate(const ZirModule *module, const ZirGlobal *global)
{
    ZirFunction probe = {0};
    Vm scratch = {0};
    Value target = default_value(&scratch, module, global->type, 0);
    int root = !scratch.failed ? ParseExprTyped(&probe, module, global->init,
                                                global->span, global->type) : -1;
    int valid = root >= 0 && !scratch.failed &&
        fold_global_element(&scratch, module, &probe, root, &target,
                            global->type, global->span) && !scratch.failed;
    free(probe.exprs);
    free_records(&scratch);
    free_arrays(&scratch);
    free_strings(&scratch);
    return valid;
}

int
VmVerify(const ZirProgram *program, const char *entry_module,
            const char *entry_function)
{
    const ZirModule *entry_owner;
    const ZirFunction *entry;
    Parameter bindings[VM_MAX_LOCALS];
    if(program == NULL || entry_module == NULL || entry_function == NULL)
        return 0;
    entry = find_entry(program, entry_module, entry_function, &entry_owner);
    if(entry == NULL || entry->is_extern ||
       parse_parameters(entry_owner, entry, bindings) != 0 ||
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
                    Parameter expected[VM_MAX_PARAMS], actual[VM_MAX_PARAMS];
                    int expected_count = parse_import_parameters(module,
                        import, expected);
                    int actual_count = provider != NULL ?
                        parse_parameters(provider_module, provider, actual) : -1;
                    int valid = provider != NULL &&
                        strcmp(import->return_type, provider->return_type) == 0 &&
                        expected_count >= 0 && expected_count == actual_count &&
                        host_type_at(module, import->return_type, 0, 0) &&
                        same_bound_type(module, provider_module,
                                        import->return_type);
                    for(int p = 0; valid && p < expected_count; p++)
                        valid = strcmp(expected[p].type, actual[p].type) == 0 &&
                            host_type_at(module, expected[p].type, 0, 1) &&
                            strcmp(expected[p].type, "void") != 0 &&
                            same_bound_type(module, provider_module,
                                            expected[p].type);
                    if(!valid) {
                        Diagnostic(import->span, "zib.bind",
                                   "bound Ziran host provider has a missing or mismatched signature: %s:%s",
                                   module->name, import->name);
                        return 0;
                    }
                    continue;
                }
                Parameter parameters[VM_MAX_PARAMS];
                int count = parse_import_parameters(module,
                    &module->imports[i], parameters);
                int host_signature =
                    host_type_at(module, module->imports[i].return_type, 0, 0) &&
                    count >= 0;
                for(int p = 0; host_signature && p < count; p++)
                    host_signature = host_type_at(module, parameters[p].type, 0, 1) &&
                        strcmp(parameters[p].type, "void") != 0;
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
                char literal[ZIR_TEXT_MAX];
                int64_t folded = 0;
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
            int binding_count = parse_parameters(module, function, bindings);
            if(!function->checked || function->is_extern ||
               !portable_type(module, function->return_type) ||
               binding_count < 0) {
                Diagnostic(function->span, "zib.function",
                              "function is outside the portable subset: %s",
                              function->name);
                return 0;
            }
            if(!verify_sequence(module, function, 0, function->stmt_count,
                                bindings, binding_count, 0, 0)) {
                Diagnostic(function->span, "zib.statement",
                           "statement is outside the portable subset in %s",
                           function->name);
                return 0;
            }
            if(strcmp(function->return_type, "void") != 0 &&
               !sequence_guarantees_return(function, 0,
                                           function->stmt_count, 0)) {
                Diagnostic(function->span, "zib.return",
                           "portable functions must return on every path: %s",
                           function->name);
                return 0;
            }
        }
    }
    return 1;
}

static Value run_function(Vm *vm, const ZirModule *module,
                          const ZirFunction *function, const Value *args,
                          int arg_count);
static Value eval(Frame *frame, int index, int depth);

static Local *
find_local(Frame *frame, const char *name)
{
    for(int i = frame->local_count - 1; i >= 0; i--)
        if(strcmp(frame->locals[i].name, name) == 0)
            return &frame->locals[i];
    return NULL;
}

static Value *
find_global_value(Frame *frame, const char *name)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *declaration = NULL;
    if(ResolveGlobalAt(frame->module, name, frame->function->span.path,
                       &owner, &declaration) != 1)
        return NULL;
    for(int i = 0; i < frame->vm->global_count; i++) {
        GlobalSlot *slot = &frame->vm->globals[i];
        if(slot->module == owner && slot->declaration == declaration)
            return &slot->value;
    }
    return NULL;
}

static Value *
record_field(Record *record, const char *name)
{
    if(record == NULL)
        return NULL;
    for(int i = 0; i < record->field_count; i++)
        if(strcmp(record->fields[i].field.name, name) == 0)
            return &record->fields[i].value;
    return NULL;
}

static Value *
record_field_path(Record *record, const char *path)
{
    const char *dot = strchr(path, '.');
    if(dot == NULL) return record_field(record, path);
    size_t length = (size_t)(dot - path);
    if(length == 0 || length >= ZIR_NAME_MAX) return NULL;
    char name[ZIR_NAME_MAX];
    memcpy(name, path, length);
    name[length] = '\0';
    Value *field = record_field(record, name);
    return field != NULL && field->kind == VALUE_RECORD ?
           record_field_path(field->record, dot + 1) : NULL;
}

/* Union storage keeps raw bits in the first field slot; every access
 * reinterprets those bytes through the named field's declared type. */
static Value
union_member_read(Vm *vm, Record *record, const char *field_type)
{
    const ZirType *enumeration = FindType(record->owner, field_type, NULL);
    const char *backing = field_type;
    uint64_t bits;
    Value result = int_value(0);
    if(record->field_count < 1) {
        vm->failed = 1;
        return result;
    }
    if(enumeration != NULL && enumeration->is_enum)
        backing = enumeration->enum_backing;
    bits = integer_bits(record->fields[0].value);
    if(!strcmp(backing, "bool"))
        return int_value(bits != 0);
    if(!strcmp(backing, "u8"))
        return uint_value(bits & UINT64_C(0xff));
    if(!strcmp(backing, "u16"))
        return uint_value(bits & UINT64_C(0xffff));
    if(!strcmp(backing, "u32"))
        return uint_value(bits & UINT64_C(0xffffffff));
    if(!strcmp(backing, "u64"))
        return uint_value(bits);
    if(!strcmp(backing, "s8"))
        return int_value((int64_t)(int8_t)(bits & UINT64_C(0xff)));
    if(!strcmp(backing, "s16"))
        return int_value((int64_t)(int16_t)(bits & UINT64_C(0xffff)));
    if(!strcmp(backing, "s32"))
        return int_value((int64_t)(int32_t)(bits & UINT64_C(0xffffffff)));
    if(!strcmp(backing, "s64"))
        return int_value(signed64(bits));
    if(!strcmp(backing, "float32")) {
        uint32_t narrow = (uint32_t)(bits & UINT64_C(0xffffffff));
        float number;
        memcpy(&number, &narrow, sizeof(number));
        return real_value(number);
    }
    if(!strcmp(backing, "float64")) {
        double number;
        memcpy(&number, &bits, sizeof(number));
        return real_value(number);
    }
    if(enumeration != NULL && enumeration->is_enum)
        return enum_value(enumeration, signed64(bits));
    vm->failed = 1;
    return result;
}

static int
union_member_write(Vm *vm, Record *record, const char *field_type, Value value)
{
    const ZirType *enumeration = FindType(record->owner, field_type, NULL);
    const char *backing = field_type;
    uint64_t bits = 0;
    if(record->field_count < 1)
        return 0;
    if(enumeration != NULL && enumeration->is_enum)
        backing = enumeration->enum_backing;
    if(!strcmp(backing, "float32")) {
        float number = (float)(value.kind == VALUE_REAL ? value.real :
                                as_real(value));
        uint32_t narrow;
        memcpy(&narrow, &number, sizeof(narrow));
        bits = narrow;
    } else if(!strcmp(backing, "float64")) {
        double number = as_real(value);
        memcpy(&bits, &number, sizeof(bits));
    } else if(!strcmp(backing, "bool"))
        bits = truthy(value) ? 1 : 0;
    else
        bits = integer_bits(value);
    record->fields[0].value = uint_value(bits);
    return 1;
}

static Value *
indexed_element(Value base, uint64_t index)
{
    if(base.kind == VALUE_RECORD && base.record != NULL &&
       VecElementType(base.record->owner, base.record->type->name,
                      NULL, 0)) {
        Value *data = record_field(base.record, "data");
        Value *count = record_field(base.record, "count");
        if(data != NULL && count != NULL &&
           count->kind == VALUE_INT && count->integer >= 0 &&
           index < (uint64_t)count->integer &&
           data->kind == VALUE_ARRAY && data->array != NULL &&
           index < (uint64_t)data->array->length)
            return &data->array->elements[index];
        return NULL;
    }
    if(base.kind == VALUE_ARRAY && base.array != NULL &&
       index < (uint64_t)base.array->length)
        return &base.array->elements[index];
    if(base.kind == VALUE_SLICE && base.array != NULL &&
       base.offset <= (size_t)base.array->length &&
       base.length <= (size_t)base.array->length - base.offset &&
       index < base.length)
        return &base.array->elements[base.offset + (size_t)index];
    return NULL;
}

static Value *
assignment_slot(Frame *frame, int index, int depth)
{
    if(index < 0 || index >= frame->function->expr_count ||
       depth >= VM_MAX_DEPTH)
        return NULL;
    const ZirExpr *expression = &frame->function->exprs[index];
    if(expression->kind == ZIR_EXPR_IDENT) {
        Local *local = find_local(frame, expression->name);
        return local != NULL ? &local->value :
               find_global_value(frame, expression->name);
    }
    if(expression->kind == ZIR_EXPR_INDEX) {
        Value *base = assignment_slot(frame, expression->left, depth + 1);
        Value index_value = eval(frame, expression->right, depth + 1);
        if(base == NULL || index_value.kind != VALUE_INT ||
           (!index_value.unsigned64 && index_value.integer < 0)) {
            frame->vm->failed = 1;
            return NULL;
        }
        Value *element = indexed_element(*base, integer_bits(index_value));
        if(element == NULL)
            frame->vm->failed = 1;
        return element;
    }
    if(expression->kind != ZIR_EXPR_MEMBER)
        return NULL;
    Value *base = assignment_slot(frame, expression->left, depth + 1);
    if(base == NULL || base->kind != VALUE_RECORD)
        return NULL;
    if(base->record->type != NULL && base->record->type->is_union) {
        /* Union writes land in the shared bit slot; the assignment layer
         * reinterprets through the declared field type. */
        frame->union_write_record = base->record;
        frame->union_write_type = expression->type;
        return &base->record->fields[0].value;
    }
    return record_field_path(base->record, expression->name);
}

static Value
binary_value(Vm *vm, const char *op, Value left, Value right,
             const char *left_type, const char *right_type)
{
    int real = left.kind == VALUE_REAL || right.kind == VALUE_REAL;
    int shift = strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0;
    /* Host handles live in the unsigned bits; compare them by address. */
    int unsigned64 = strcmp(left_type, "u64") == 0 || left_type[0] == '*' ||
                     strcmp(left_type, "null") == 0 ||
                     (!shift && (strcmp(right_type, "u64") == 0 ||
                                 right_type[0] == '*' ||
                                 strcmp(right_type, "null") == 0));
    int signed_wide = !unsigned64 &&
        (strcmp(left_type, "s64") == 0 ||
         (!shift && strcmp(right_type, "s64") == 0));
    uint64_t left_bits = integer_bits(left);
    uint64_t right_bits = integer_bits(right);
    double a = as_real(left), b = as_real(right);
    if(vm->failed || left.kind == VALUE_VOID || right.kind == VALUE_VOID)
        goto failed;
    if(left.kind == VALUE_STRING || right.kind == VALUE_STRING) {
        if(left.kind != VALUE_STRING || right.kind != VALUE_STRING)
            goto failed;
        int equal = left.length == right.length &&
                    (left.length == 0 ||
                     memcmp(left.data, right.data, left.length) == 0);
        if(strcmp(op, "==") == 0)
            return int_value(equal);
        if(strcmp(op, "!=") == 0)
            return int_value(!equal);
        goto failed;
    }
    if((left.kind == VALUE_ENUM && left.enumeration->is_enum_flags) ||
       (right.kind == VALUE_ENUM && right.enumeration->is_enum_flags)) {
        const ZirType *flags = left.kind == VALUE_ENUM ? left.enumeration :
                               right.enumeration;
        if((left.kind == VALUE_ENUM && left.enumeration != flags) ||
           (right.kind == VALUE_ENUM && right.enumeration != flags) ||
           (left.kind != VALUE_ENUM && left.kind != VALUE_INT) ||
           (right.kind != VALUE_ENUM && right.kind != VALUE_INT))
            goto failed;
        Value result = binary_value(vm, op, int_value(left.integer),
                                    int_value(right.integer),
                                    flags->enum_backing, flags->enum_backing);
        if(vm->failed || strcmp(op, "==") == 0 || strcmp(op, "!=") == 0)
            return result;
        result = coerce(vm, NULL, result, flags->enum_backing);
        return enum_value(flags, result.integer);
    }
    if(left.kind == VALUE_ENUM || right.kind == VALUE_ENUM) {
        if(left.kind != VALUE_ENUM || right.kind != VALUE_ENUM ||
           left.enumeration != right.enumeration)
            goto failed;
        if(strcmp(op, "==") == 0)
            return int_value(left.integer == right.integer);
        if(strcmp(op, "!=") == 0)
            return int_value(left.integer != right.integer);
        goto failed;
    }
    if(strcmp(op, "==") == 0)
        return int_value(real ? a == b :
                         unsigned64 ? left_bits == right_bits :
                         left.integer == right.integer);
    if(strcmp(op, "!=") == 0)
        return int_value(real ? a != b :
                         unsigned64 ? left_bits != right_bits :
                         left.integer != right.integer);
    if(strcmp(op, "<") == 0)
        return int_value(real ? a < b :
                         unsigned64 ? left_bits < right_bits :
                         left.integer < right.integer);
    if(strcmp(op, "<=") == 0)
        return int_value(real ? a <= b :
                         unsigned64 ? left_bits <= right_bits :
                         left.integer <= right.integer);
    if(strcmp(op, ">") == 0)
        return int_value(real ? a > b :
                         unsigned64 ? left_bits > right_bits :
                         left.integer > right.integer);
    if(strcmp(op, ">=") == 0)
        return int_value(real ? a >= b :
                         unsigned64 ? left_bits >= right_bits :
                         left.integer >= right.integer);
    if(real) {
        if(strcmp(op, "+") == 0) return real_value(a + b);
        if(strcmp(op, "-") == 0) return real_value(a - b);
        if(strcmp(op, "*") == 0) return real_value(a * b);
        if(strcmp(op, "/") == 0) return real_value(a / b);
        goto failed;
    }
    if(bitwise_operator(op)) {
        if(unsigned64 || signed_wide) {
            if(strcmp(op, "&") == 0)
                return unsigned64 ? uint_value(left_bits & right_bits) :
                                    int_value(signed64(left_bits & right_bits));
            if(strcmp(op, "|") == 0)
                return unsigned64 ? uint_value(left_bits | right_bits) :
                                    int_value(signed64(left_bits | right_bits));
            if(strcmp(op, "^") == 0)
                return unsigned64 ? uint_value(left_bits ^ right_bits) :
                                    int_value(signed64(left_bits ^ right_bits));
            if((!right.unsigned64 && right.integer < 0) || right_bits >= 64)
                goto failed;
            if(strcmp(op, "<<") == 0)
                return unsigned64 ? uint_value(left_bits << right_bits) :
                                    int_value(signed64(left_bits << right_bits));
            if(unsigned64)
                return uint_value(left_bits >> right_bits);
            uint64_t shifted = left_bits >> right_bits;
            if((left_bits & UINT64_C(0x8000000000000000)) != 0 &&
               right_bits > 0)
                shifted |= UINT64_MAX << (64 - right_bits);
            return int_value(signed64(shifted));
        }
        uint32_t a_bits = (uint32_t)left_bits;
        uint32_t b_bits = (uint32_t)right_bits;
        if(strcmp(op, "&") == 0)
            return int_value(a_bits & b_bits);
        if(strcmp(op, "|") == 0)
            return int_value(a_bits | b_bits);
        if(strcmp(op, "^") == 0)
            return int_value(a_bits ^ b_bits);
        unsigned width = !strcmp(left_type, "u8") ||
                         !strcmp(left_type, "s8") ? 8 :
                         !strcmp(left_type, "u16") ||
                         !strcmp(left_type, "s16") ? 16 : 32;
        if((!right.unsigned64 && right.integer < 0) || right_bits >= width)
            goto failed;
        unsigned amount = (unsigned)right_bits;
        uint32_t mask = width == 32 ? UINT32_MAX :
                        (UINT32_C(1) << width) - 1;
        a_bits &= mask;
        if(strcmp(op, "<<") == 0)
            return int_value((a_bits << amount) & mask);
        uint32_t shifted = a_bits >> amount;
        if(left_type[0] != 'u' &&
           (a_bits & (UINT32_C(1) << (width - 1))) != 0 && amount > 0)
            shifted |= mask ^ (mask >> amount);
        return int_value(shifted & mask);
    }
    if(unsigned64) {
        if(strcmp(op, "+") == 0) return uint_value(left_bits + right_bits);
        if(strcmp(op, "-") == 0) return uint_value(left_bits - right_bits);
        if(strcmp(op, "*") == 0) return uint_value(left_bits * right_bits);
        if(strcmp(op, "/") == 0 && right_bits != 0)
            return uint_value(left_bits / right_bits);
        if(strcmp(op, "%") == 0 && right_bits != 0)
            return uint_value(left_bits % right_bits);
        goto failed;
    }
    if(signed_wide) {
        if(strcmp(op, "+") == 0)
            return int_value(signed64(left_bits + right_bits));
        if(strcmp(op, "-") == 0)
            return int_value(signed64(left_bits - right_bits));
        if(strcmp(op, "*") == 0)
            return int_value(signed64(left_bits * right_bits));
        if(right.integer == 0)
            goto failed;
        if(left.integer == INT64_MIN && right.integer == -1)
            return int_value(strcmp(op, "/") == 0 ? INT64_MIN : 0);
        if(strcmp(op, "/") == 0)
            return int_value(left.integer / right.integer);
        if(strcmp(op, "%") == 0)
            return int_value(left.integer % right.integer);
        goto failed;
    }
    if(strcmp(op, "+") == 0) return int_value(left.integer + right.integer);
    if(strcmp(op, "-") == 0) return int_value(left.integer - right.integer);
    if(strcmp(op, "*") == 0) return int_value(left.integer * right.integer);
    if(strcmp(op, "/") == 0 && right.integer != 0)
        return int_value(left.integer / right.integer);
    if(strcmp(op, "%") == 0 && right.integer != 0)
        return int_value(left.integer % right.integer);
failed:
    vm->failed = 1;
    return int_value(0);
}

static Value
eval(Frame *frame, int index, int depth)
{
    const ZirExpr *expression;
    Value value = int_value(0), left, right;
    if(frame->vm->failed || index < 0 || index >= frame->function->expr_count ||
       depth >= VM_MAX_DEPTH) {
        frame->vm->failed = 1;
        return value;
    }
    expression = &frame->function->exprs[index];
    switch(expression->kind) {
    case ZIR_EXPR_SIZE_OF: {
        size_t size, alignment;
        if(!TypeLayout(frame->module, expression->name, &size, &alignment) ||
           size > INT64_MAX) {
            frame->vm->failed = 1;
            break;
        }
        value = int_value((int64_t)size);
        break;
    }
    case ZIR_EXPR_INT: {
        char *end;
        errno = 0;
        if(expression->text[0] == '-') {
            value = int_value(strtoll(expression->text, &end, 0));
        } else {
            unsigned long long bits = strtoull(expression->text, &end, 0);
            value = bits > INT64_MAX ? uint_value(bits) : int_value((int64_t)bits);
        }
        if(errno || end == expression->text || *end)
            frame->vm->failed = 1;
        else {
            const ZirType *enumeration = FindType(frame->module,
                                                  expression->type, NULL);
            if(enumeration != NULL && enumeration->is_enum)
                value = coerce(frame->vm, frame->module, value,
                               expression->type);
        }
        break;
    }
    case ZIR_EXPR_FLOAT: {
        char *end;
        errno = 0;
        value = real_value(strtod(expression->text, &end));
        if(errno || end == expression->text || *end || !isfinite(value.real))
            frame->vm->failed = 1;
        break;
    }
    case ZIR_EXPR_STRING:
        value = literal_string(frame->vm, expression);
        break;
    case ZIR_EXPR_COMPILE_TIME:
        return int_value(0);
    case ZIR_EXPR_IDENT: {
        if(expression->is_function_value) {
            const ZirModule *owner = NULL;
            const ZirFunction *function = NULL;
            const ZirType *slot = FindType(frame->module,
                                           expression->type, NULL);
            if(slot == NULL || !slot->is_procedure_type ||
               ResolveFunction(frame->module, expression->name,
                               &owner, &function) != 1 || function == NULL) {
                frame->vm->failed = 1;
                break;
            }
            value.kind = VALUE_SLOT;
            value.slot_type = slot;
            value.slot_module = owner;
            value.slot_function = function;
            break;
        }
        if(strcmp(expression->name, "true") == 0)
            return int_value(1);
        if(strcmp(expression->name, "false") == 0)
            return int_value(0);
        if(strcmp(expression->name, "null") == 0)
            return uint_value(0);
        Local *local = find_local(frame, expression->name);
        if(local != NULL) {
            Value stored = local->value;
            if(expression->is_move) {
                if(stored.kind != VALUE_RECORD ||
                   !vm_type_contains_vec(frame->module,
                                         expression->type, 0)) {
                    frame->vm->failed = 1;
                    break;
                }
                if(VecElementType(frame->module, expression->type, NULL, 0))
                    local->value = (Value){.kind = VALUE_INVALID};
                /* Aggregate storage is cloned at the destination boundary,
                 * while the checker makes the source unusable afterward.
                 * Retiring the same reachable tree twice is idempotent. */
            }
            return coerce_expression(frame->vm, frame->module,
                                     stored, expression->type);
        }
        Value *global = find_global_value(frame, expression->name);
        if(global != NULL)
            return coerce_expression(frame->vm, frame->module,
                                     *global, expression->type);
        frame->vm->failed = 1;
        break;
    }
    case ZIR_EXPR_UNARY:
        right = eval(frame, expression->right, depth + 1);
        if(frame->vm->failed)
            break;
        if(strcmp(expression->op, "-") == 0)
            value = right.kind == VALUE_REAL ? real_value(-right.real) :
                    right.unsigned64 ? uint_value(UINT64_C(0) - right.bits) :
                                       int_value(signed64(UINT64_C(0) -
                                                          integer_bits(right)));
        else if(strcmp(expression->op, "+") == 0)
            value = right;
        else if(strcmp(expression->op, "!") == 0)
            value = int_value(!truthy(right));
        else if(strcmp(expression->op, "~") == 0 && right.kind == VALUE_INT)
            value = right.unsigned64 ? uint_value(~right.bits) :
                                       int_value(~right.integer);
        else
            frame->vm->failed = 1;
        break;
    case ZIR_EXPR_BINARY:
        left = eval(frame, expression->left, depth + 1);
        if(frame->vm->failed)
            break;
        if(strcmp(expression->op, "&&") == 0 && !truthy(left))
            return int_value(0);
        if(strcmp(expression->op, "||") == 0 && truthy(left))
            return int_value(1);
        right = eval(frame, expression->right, depth + 1);
        if(strcmp(expression->op, "&&") == 0)
            value = int_value(truthy(left) && truthy(right));
        else if(strcmp(expression->op, "||") == 0)
            value = int_value(truthy(left) || truthy(right));
        else
            value = binary_value(frame->vm, expression->op, left, right,
                frame->function->exprs[expression->left].type,
                frame->function->exprs[expression->right].type);
        break;
    case ZIR_EXPR_CAST:
        value = coerce(frame->vm, frame->module,
                       eval(frame, expression->right, depth + 1),
                       expression->name);
        break;
    case ZIR_EXPR_COMPOUND: {
        value = default_value(frame->vm, frame->module,
                              expression->name, depth + 1);
        if(frame->vm->failed)
            break;
        if(value.kind == VALUE_ARRAY) {
            int position = 0;
            for(int child = expression->first_child; child >= 0;
                child = frame->function->exprs[child].next_sibling) {
                if(child >= frame->function->expr_count ||
                   position >= value.array->length) {
                    frame->vm->failed = 1;
                    break;
                }
                const ZirExpr *item = &frame->function->exprs[child];
                Value initialized = eval(frame, item->right, depth + 1);
                if(frame->vm->failed)
                    break;
                value.array->elements[position++] = coerce(frame->vm,
                    value.array->owner, initialized,
                    value.array->element_type);
            }
            break;
        }
        if(value.kind != VALUE_RECORD) {
            frame->vm->failed = 1;
            break;
        }
        int count = 0;
        for(int child = expression->first_child; child >= 0;
            child = frame->function->exprs[child].next_sibling) {
            if(child >= frame->function->expr_count ||
               ++count > VM_MAX_FIELDS) {
                frame->vm->failed = 1;
                break;
            }
            const ZirExpr *initializer = &frame->function->exprs[child];
            RecordField *field = NULL;
            for(int i = 0; i < value.record->field_count; i++) {
                if(strcmp(value.record->fields[i].field.name,
                          initializer->name) == 0) {
                    field = &value.record->fields[i];
                    break;
                }
            }
            if(field == NULL) {
                frame->vm->failed = 1;
                break;
            }
            Value initialized = eval(frame, initializer->right, depth + 1);
            if(frame->vm->failed)
                break;
            field->value = coerce(frame->vm, value.record->owner,
                                  initialized, field->field.type);
        }
        break;
    }
    case ZIR_EXPR_MEMBER: {
        if(expression->is_move) {
            Value *source = assignment_slot(frame, index, depth + 1);
            char element[ZIR_NAME_MAX];
            if(source == NULL ||
               !VecElementType(frame->module, expression->type,
                               element, sizeof(element))) {
                frame->vm->failed = 1;
                break;
            }
            left = *source;
            *source = (Value){.kind = VALUE_INVALID};
            return coerce_expression(frame->vm, frame->module, left,
                                     expression->type);
        }
        left = eval(frame, expression->left, depth + 1);
        if(left.kind == VALUE_STRING &&
           strcmp(expression->name, "count") == 0) {
            value = int_value((int64_t)left.length);
            break;
        }
        if(left.kind == VALUE_SLICE &&
           strcmp(expression->name, "count") == 0) {
            value = int_value((int64_t)left.length);
            break;
        }
        if(left.kind == VALUE_ARRAY && left.array != NULL &&
           strcmp(expression->name, "count") == 0) {
            value = int_value((int64_t)left.array->length);
            break;
        }
        if(left.kind == VALUE_ARRAY && left.array != NULL &&
           left.array->length == 0 &&
           strcmp(expression->name, "data") == 0) {
            value = int_value(0);
            break;
        }
        Value *field = left.kind == VALUE_RECORD ?
                       record_field_path(left.record, expression->name) : NULL;
        if(field == NULL)
            frame->vm->failed = 1;
        else if(left.record->type != NULL && left.record->type->is_union)
            value = union_member_read(frame->vm, left.record,
                                      expression->type);
        else
            value = *field;
        break;
    }
    case ZIR_EXPR_SLICE: {
        Value *stored = assignment_slot(frame, expression->left, depth + 1);
        left = stored != NULL ? *stored :
               eval(frame, expression->left, depth + 1);
        if(left.kind == VALUE_STRING) {
            Value low = expression->right >= 0 ?
                eval(frame, expression->right, depth + 1) : int_value(0);
            Value high = expression->third >= 0 ?
                eval(frame, expression->third, depth + 1) :
                int_value((int64_t)left.length);
            if(frame->vm->failed || low.kind != VALUE_INT ||
               high.kind != VALUE_INT ||
               (!low.unsigned64 && low.integer < 0) ||
               (!high.unsigned64 && high.integer < 0) ||
               integer_bits(low) > integer_bits(high) ||
               integer_bits(high) > left.length) {
                frame->vm->failed = 1;
                break;
            }
            size_t start = (size_t)integer_bits(low);
            value = string_value(left.data != NULL ? left.data + start : NULL,
                                 (size_t)(integer_bits(high) - integer_bits(low)));
            break;
        }
        Array *backing = NULL;
        size_t offset = 0;
        size_t length = 0;
        if(left.kind == VALUE_ARRAY && left.array != NULL) {
            backing = left.array;
            length = (size_t)backing->length;
            if(length == 0) backing = NULL;
        } else if(left.kind == VALUE_SLICE) {
            backing = left.array;
            offset = left.offset;
            length = left.length;
            if(backing != NULL &&
               (offset > (size_t)backing->length ||
                length > (size_t)backing->length - offset))
                frame->vm->failed = 1;
        } else {
            frame->vm->failed = 1;
        }
        Value low = expression->right >= 0 ?
            eval(frame, expression->right, depth + 1) : int_value(0);
        Value high = expression->third >= 0 ?
            eval(frame, expression->third, depth + 1) :
            int_value((int64_t)length);
        if(frame->vm->failed || low.kind != VALUE_INT ||
           high.kind != VALUE_INT ||
           (!low.unsigned64 && low.integer < 0) ||
           (!high.unsigned64 && high.integer < 0) ||
           integer_bits(low) > integer_bits(high) ||
           integer_bits(high) > length) {
            frame->vm->failed = 1;
            break;
        }
        value = (Value){.kind = VALUE_SLICE, .array = backing,
                        .offset = offset + (size_t)integer_bits(low),
                        .length = (size_t)(integer_bits(high) -
                                           integer_bits(low))};
        break;
    }
    case ZIR_EXPR_INDEX: {
        /* Indexing a stored array borrows its backing values. Copying the
         * entire array for each read would exhaust the VM allocation budget
         * in an ordinary loop. The result is coerced below as a value. */
        Value *stored = assignment_slot(frame, expression->left, depth + 1);
        left = stored != NULL &&
               (stored->kind == VALUE_ARRAY ||
                VecElementType(frame->module,
                    frame->function->exprs[expression->left].type,
                    NULL, 0)) ?
               *stored : eval(frame, expression->left, depth + 1);
        right = eval(frame, expression->right, depth + 1);
        if(right.kind != VALUE_INT ||
           (!right.unsigned64 && right.integer < 0)) {
            frame->vm->failed = 1;
            break;
        }
        if(left.kind == VALUE_STRING && integer_bits(right) < left.length)
            value = int_value(left.data[integer_bits(right)]);
        else {
            Value *element = indexed_element(left, integer_bits(right));
            if(element != NULL)
                value = *element;
            else
                frame->vm->failed = 1;
        }
        break;
    }
    case ZIR_EXPR_CONDITIONAL:
        left = eval(frame, expression->left, depth + 1);
        if(!frame->vm->failed)
            value = eval(frame, truthy(left) ? expression->right :
                          expression->third, depth + 1);
        break;
    case ZIR_EXPR_CALL: {
        if(!strcmp(expression->name, "TextView")) {
            Value bytes = eval(frame, expression->first_child, depth + 1);
            if(frame->vm->failed || bytes.kind != VALUE_SLICE ||
               (bytes.length > 0 && bytes.array == NULL)) {
                frame->vm->failed = 1;
                break;
            }
            StringLiteral *item = malloc(sizeof(*item) + bytes.length + 1);
            if(item == NULL) {
                frame->vm->failed = 1;
                break;
            }
            for(size_t i = 0; i < bytes.length; i++) {
                Value *part = indexed_element(bytes, i);
                if(part == NULL || part->kind != VALUE_INT) {
                    frame->vm->failed = 1;
                    free(item);
                    break;
                }
                item->data[i] = (unsigned char)integer_bits(*part);
            }
            if(frame->vm->failed)
                break;
            item->data[bytes.length] = 0;
            item->length = bytes.length;
            item->expression = NULL;
            item->next = frame->vm->strings;
            frame->vm->strings = item;
            value = string_value(item->data, item->length);
            break;
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
            int push = !strcmp(expression->name, "VecPush");
            Value *vec = assignment_slot(frame, first, depth + 1);
            char element[ZIR_NAME_MAX];
            if(vec == NULL || vec->kind != VALUE_RECORD ||
               !VecElementType(frame->module,
                   frame->function->exprs[first].type,
                   element, sizeof(element))) {
                frame->vm->failed = 1;
                break;
            }
            if(!strcmp(expression->name, "VecSwap")) {
                int second = frame->function->exprs[first].next_sibling;
                Value *other = assignment_slot(frame, second, depth + 1);
                if(other == NULL || other->kind != VALUE_RECORD ||
                   other->record->type != vec->record->type) {
                    frame->vm->failed = 1;
                    break;
                }
                Value saved = *vec;
                *vec = *other;
                *other = saved;
                value = (Value){.kind = VALUE_VOID};
                break;
            }
            if(!strcmp(expression->name, "VecClone")) {
                int second = frame->function->exprs[first].next_sibling;
                Value *source = assignment_slot(frame, second, depth + 1);
                Value *dest_data, *dest_count, *dest_capacity;
                Value *src_data, *src_count, *src_capacity;
                Array *copy = NULL;
                if(source == NULL || source->kind != VALUE_RECORD ||
                   source->record->type != vec->record->type) {
                    frame->vm->failed = 1;
                    break;
                }
                dest_data = record_field(vec->record, "data");
                dest_count = record_field(vec->record, "count");
                dest_capacity = record_field(vec->record, "capacity");
                src_data = record_field(source->record, "data");
                src_count = record_field(source->record, "count");
                src_capacity = record_field(source->record, "capacity");
                if(dest_data == NULL || dest_count == NULL ||
                   dest_capacity == NULL || src_data == NULL ||
                   src_count == NULL || src_capacity == NULL ||
                   src_count->kind != VALUE_INT || src_count->integer < 0 ||
                   src_capacity->kind != VALUE_INT ||
                   src_capacity->integer < src_count->integer ||
                   dest_count->kind != VALUE_INT) {
                    frame->vm->failed = 1;
                    break;
                }
                if(src_count->integer > 0) {
                    copy = allocate_array_try(frame->vm, frame->module,
                                              element, (int)src_count->integer,
                                              0);
                    if(copy == NULL) {
                        value = int_value(0);
                        break;
                    }
                    for(int i = 0; i < src_count->integer && !frame->vm->failed;
                        i++) {
                        Value *entry = src_data->kind == VALUE_ARRAY ?
                            &src_data->array->elements[i] : NULL;
                        if(entry == NULL) {
                            frame->vm->failed = 1;
                            break;
                        }
                        copy->elements[i] = coerce(frame->vm, frame->module,
                                                   *entry, element);
                    }
                    if(frame->vm->failed)
                        break;
                    if(dest_data->kind == VALUE_ARRAY && dest_data->array != NULL)
                        retire_value(frame->vm, *dest_data, 0);
                } else if(dest_data->kind == VALUE_ARRAY &&
                          dest_data->array != NULL) {
                    retire_value(frame->vm, *dest_data, 0);
                }
                *dest_data = copy != NULL ?
                    (Value){.kind = VALUE_ARRAY, .array = copy} :
                    (Value){.kind = VALUE_ARRAY};
                *dest_count = int_value(src_count->integer);
                *dest_capacity = int_value(src_count->integer);
                value = int_value(1);
                break;
            }
            if(!strcmp(expression->name, "VecSlice")) {
                int second = frame->function->exprs[first].next_sibling;
                int third = frame->function->exprs[second].next_sibling;
                Value low = eval(frame, second, depth + 1);
                Value high;
                Value *data = record_field(vec->record, "data");
                Value *count = record_field(vec->record, "count");
                int64_t from, to;
                if(frame->vm->failed || low.kind != VALUE_INT ||
                   (!low.unsigned64 && low.integer < 0)) {
                    frame->vm->failed = 1;
                    break;
                }
                high = eval(frame, third, depth + 1);
                if(frame->vm->failed || high.kind != VALUE_INT ||
                   (!high.unsigned64 && high.integer < 0)) {
                    frame->vm->failed = 1;
                    break;
                }
                from = (int64_t)integer_bits(low);
                to = (int64_t)integer_bits(high);
                if(data == NULL || count == NULL || count->kind != VALUE_INT ||
                   from > to || to > count->integer) {
                    frame->vm->failed = 1;
                    break;
                }
                if(to == from || data->kind != VALUE_ARRAY ||
                   data->array == NULL)
                    value = (Value){.kind = VALUE_SLICE};
                else
                    value = (Value){.kind = VALUE_SLICE,
                                    .array = data->array,
                                    .offset = (size_t)from,
                                    .length = (size_t)(to - from)};
                break;
            }
            if(!strcmp(expression->name, "VecPop") ||
               !strcmp(expression->name, "VecGet")) {
                int get = !strcmp(expression->name, "VecGet");
                const ZirModule *owner = NULL;
                const ZirType *record_type = FindType(frame->module,
                    expression->type, &owner);
                Value *data = record_field(vec->record, "data");
                Value *count = record_field(vec->record, "count");
                Value result;
                Value *has_value, *item;
                int64_t at = -1;
                if(record_type == NULL || data == NULL || count == NULL ||
                   count->kind != VALUE_INT || count->integer < 0) {
                    frame->vm->failed = 1;
                    break;
                }
                if(get) {
                    int second = frame->function->exprs[first].next_sibling;
                    Value index = eval(frame, second, depth + 1);
                    if(frame->vm->failed || index.kind != VALUE_INT ||
                       (!index.unsigned64 && index.integer < 0)) {
                        frame->vm->failed = 1;
                        break;
                    }
                    at = (int64_t)integer_bits(index);
                }
                result = default_value(frame->vm, owner, expression->type, 0);
                if(frame->vm->failed)
                    break;
                has_value = record_field(result.record, "has_value");
                item = record_field(result.record, "value");
                if(has_value == NULL || item == NULL) {
                    frame->vm->failed = 1;
                    break;
                }
                if(get ? (at >= 0 && at < count->integer)
                       : (count->integer > 0)) {
                    Value *source = data->kind == VALUE_ARRAY ?
                        &data->array->elements[get ? at :
                         count->integer - 1] : NULL;
                    if(source == NULL) {
                        frame->vm->failed = 1;
                        break;
                    }
                    if(get)
                        *item = coerce(frame->vm, frame->module, *source,
                                       element);
                    else {
                        *item = *source;
                        *source = (Value){0};
                        count->integer--;
                    }
                    if(frame->vm->failed)
                        break;
                    *has_value = int_value(1);
                }
                value = result;
                break;
            }
            if(!strcmp(expression->name, "BuilderAppend") ||
               !strcmp(expression->name, "BuilderFinish")) {
                int finish = !strcmp(expression->name, "BuilderFinish");
                Value *data = record_field(vec->record, "data");
                Value *count = record_field(vec->record, "count");
                Value *capacity = record_field(vec->record, "capacity");
                Value text;
                if(data == NULL || count == NULL || capacity == NULL ||
                   count->kind != VALUE_INT || capacity->kind != VALUE_INT ||
                   count->integer < 0 || capacity->integer < count->integer ||
                   capacity->integer > INT32_MAX) {
                    frame->vm->failed = 1;
                    break;
                }
                if(finish) {
                    StringLiteral *built = NULL;
                    if(count->integer > 0) {
                        built = malloc(sizeof(*built) +
                                       (size_t)count->integer);
                        if(built != NULL) {
                            for(int i = 0; i < count->integer; i++) {
                                Value *byte = data->kind == VALUE_ARRAY ?
                                    &data->array->elements[i] : NULL;
                                if(byte == NULL || byte->kind != VALUE_INT) {
                                    free(built);
                                    built = NULL;
                                    break;
                                }
                                built->data[i] =
                                    (unsigned char)integer_bits(*byte);
                            }
                        }
                        if(built == NULL) {
                            frame->vm->failed = 1;
                            break;
                        }
                        built->expression = NULL;
                        built->length = (size_t)count->integer;
                        built->next = frame->vm->strings;
                        frame->vm->strings = built;
                    }
                    value = built != NULL ?
                        string_value(built->data, built->length) :
                        string_value((const unsigned char *)"", 0);
                    if(data->kind == VALUE_ARRAY && count->integer > 0) {
                        for(int i = 0; i < count->integer; i++)
                            retire_value(frame->vm, data->array->elements[i], 0);
                    }
                    *data = (Value){.kind = VALUE_ARRAY};
                    *count = int_value(0);
                    *capacity = int_value(0);
                    break;
                }
                {
                    int second = frame->function->exprs[first].next_sibling;
                    int64_t length, position;
                    text = eval(frame, second, depth + 1);
                    if(frame->vm->failed || text.kind != VALUE_STRING) {
                        frame->vm->failed = 1;
                        break;
                    }
                    length = (int64_t)text.length;
                    if(count->integer > INT32_MAX - length) {
                        value = int_value(0);
                        break;
                    }
                    if(count->integer + length > capacity->integer) {
                        int64_t next_capacity = capacity->integer == 0 ? 8 :
                            capacity->integer;
                        while(next_capacity < count->integer + length) {
                            if(next_capacity > INT32_MAX / 2) {
                                next_capacity = count->integer + length;
                                break;
                            }
                            next_capacity *= 2;
                        }
                        if(next_capacity > INT32_MAX) {
                            value = int_value(0);
                            break;
                        }
                        Array *grown = allocate_array_try(frame->vm,
                            frame->module, "u8", (int)next_capacity, 0);
                        if(grown == NULL) {
                            value = int_value(0);
                            break;
                        }
                        if(data->array != NULL) {
                            for(int i = 0; i < count->integer; i++) {
                                grown->elements[i] = data->array->elements[i];
                                data->array->elements[i] = (Value){0};
                            }
                            retire_value(frame->vm, *data, 0);
                        }
                        *data = (Value){.kind = VALUE_ARRAY, .array = grown};
                        *capacity = int_value(next_capacity);
                    }
                    for(position = 0; position < length; position++) {
                        data->array->elements[count->integer + position] =
                            int_value(text.data[position]);
                    }
                    count->integer += length;
                    value = int_value(1);
                }
                break;
            }
            Value *data = record_field(vec->record, "data");
            Value *count = record_field(vec->record, "count");
            Value *capacity = record_field(vec->record, "capacity");
            if(data == NULL || count == NULL || capacity == NULL ||
               count->kind != VALUE_INT || capacity->kind != VALUE_INT ||
               count->integer < 0 || capacity->integer < count->integer ||
               capacity->integer > INT32_MAX) {
                frame->vm->failed = 1;
                break;
            }
            if(push) {
                int second = frame->function->exprs[first].next_sibling;
                Value item = eval(frame, second, depth + 1);
                if(frame->vm->failed) break;
                if(count->integer == capacity->integer) {
                    int next_capacity = capacity->integer == 0 ? 8 :
                        capacity->integer > INT32_MAX / 2 ? 0 :
                        (int)(capacity->integer * 2);
                    Array *grown = next_capacity > 0 ?
                        allocate_array_try(frame->vm, frame->module,
                                           element, next_capacity, 0) : NULL;
                    if(grown == NULL) {
                        value = int_value(0);
                        break;
                    }
                    if(data->array != NULL) {
                        for(int i = 0; i < count->integer; i++) {
                            grown->elements[i] = data->array->elements[i];
                            data->array->elements[i] = (Value){0};
                        }
                        retire_value(frame->vm, *data, 0);
                    }
                    *data = (Value){.kind = VALUE_ARRAY, .array = grown};
                    *capacity = int_value(next_capacity);
                }
                Value stored = coerce(frame->vm, frame->module, item, element);
                if(frame->vm->failed) break;
                data->array->elements[count->integer] = stored;
                count->integer++;
                value = int_value(1);
            } else {
                if(data->array != NULL) {
                    for(int i = 0; i < count->integer; i++) {
                        retire_value(frame->vm, data->array->elements[i], 0);
                        data->array->elements[i] = (Value){0};
                    }
                }
                *count = int_value(0);
                if(!strcmp(expression->name, "VecFree")) {
                    retire_value(frame->vm, *data, 0);
                    *data = (Value){.kind = VALUE_ARRAY};
                    *capacity = int_value(0);
                }
                value = (Value){.kind = VALUE_VOID};
            }
            break;
        }
        Value args[VM_MAX_PARAMS] = {0};
        int count = 0;
        unsigned used = 0;
        const ZirModule *owner = NULL;
        const ZirFunction *callee = NULL;
        const ZirImport *external = NULL;
        if(expression->slot_type[0]) {
            Local *binding = find_local(frame, expression->name);
            if(binding == NULL || binding->value.kind != VALUE_SLOT) {
                frame->vm->failed = 1;
                break;
            }
            owner = binding->value.slot_module;
            callee = binding->value.slot_function;
        } else {
            int resolved = ResolveFunction(frame->module, expression->name,
                                           &owner, &callee);
            external = resolved == 0 ?
                host_import(frame->module, expression->name) : NULL;
        }
        if((owner == NULL || callee == NULL) && external == NULL) {
            frame->vm->failed = 1;
            break;
        }
        for(int child = expression->first_child; child >= 0;
            child = frame->function->exprs[child].next_sibling) {
            int position = frame->function->exprs[child].argument_index;
            if(count >= VM_MAX_PARAMS || position < 0 ||
               position >= VM_MAX_PARAMS || (used & (1u << position))) {
                frame->vm->failed = 1;
                break;
            }
            args[position] = eval(frame, child, depth + 1);
            used |= 1u << position;
            count++;
        }
        if(!frame->vm->failed) {
            if(external != NULL) {
                ZirFunction signature = {0};
                copy_text(signature.name, sizeof(signature.name),
                          external->name);
                copy_text(signature.args, sizeof(signature.args),
                          external->args);
                copy_text(signature.return_type,
                          sizeof(signature.return_type),
                          external->return_type);
                signature.is_extern = 1;
                signature.span = external->span;
                value = run_function(frame->vm, frame->module,
                                     &signature, args, count);
            } else {
                value = run_function(frame->vm, owner, callee, args, count);
            }
        }
        break;
    }
    default:
        frame->vm->failed = 1;
        break;
    }
    return coerce_expression(frame->vm, frame->module, value,
                             expression->type);
}

static void collect_unreachable(Vm *vm);

static Flow
execute_sequence(Frame *frame, int begin, int end, int depth,
                 Value *result)
{
    Vm *vm = frame->vm;
    const ZirFunction *function = frame->function;
    if(depth >= VM_MAX_DEPTH)
        return FLOW_ERROR;
    for(int s = begin; s < end && !vm->failed; s++) {
        /* At a top-level statement boundary no expression temporaries are
         * live outside Frame.locals. Batch reclamation here instead of
         * walking growing globals after every helper call. */
        if(vm->depth == 1 &&
           vm->allocated_since_collection >= 4 * 1024 * 1024)
            collect_unreachable(vm);
        const ZirStmt *statement = &function->stmts[s];
        int close;
        int saved_locals;
        Flow flow;
        if(++vm->steps > VM_MAX_STEPS)
            return FLOW_ERROR;
        switch(statement->kind) {
        case ZIR_STMT_DECL: {
            Value value = statement->expr_root >= 0 ?
                eval(frame, statement->expr_root, 0) :
                default_value(vm, frame->module, statement->type, 0);
            if(vm->failed || frame->local_count >= VM_MAX_LOCALS)
                return FLOW_ERROR;
            Local *local = &frame->locals[frame->local_count++];
            copy_text(local->name, sizeof(local->name), statement->name);
            copy_text(local->type, sizeof(local->type), statement->type);
            local->value = coerce(vm, frame->module, value, statement->type);
            break;
        }
        case ZIR_STMT_ASSIGN: {
            Record *union_record = NULL;
            const char *union_type = NULL;
            Value *slot = assignment_slot(frame, statement->lhs_root, 0);
            if(frame->union_write_record != NULL) {
                union_record = frame->union_write_record;
                union_type = frame->union_write_type;
                frame->union_write_record = NULL;
                frame->union_write_type = NULL;
            }
            if(slot == NULL)
                return FLOW_ERROR;
            Value right = eval(frame, statement->expr_root, 0);
            if(vm->failed)
                return FLOW_ERROR;
            if(strcmp(statement->assignment_op, "=") != 0) {
                const char *operation = assignment_binary_operator(
                    statement->assignment_op);
                if(operation == NULL)
                    return FLOW_ERROR;
                Value current = union_record != NULL ?
                    union_member_read(vm, union_record, union_type) : *slot;
                right = binary_value(vm, operation, current, right,
                    function->exprs[statement->lhs_root].type,
                    function->exprs[statement->expr_root].type);
            }
            Value replacement = union_record != NULL ? right :
                coerce(vm, frame->module, right,
                       function->exprs[statement->lhs_root].type);
            if(vm->failed)
                return FLOW_ERROR;
            if(union_record != NULL) {
                if(!union_member_write(vm, union_record, union_type,
                                       replacement))
                    return FLOW_ERROR;
                break;
            }
            Value previous = *slot;
            if(previous.kind == VALUE_ARRAY && previous.array != NULL &&
               replacement.kind == VALUE_ARRAY && replacement.array != NULL &&
               previous.array->length == replacement.array->length &&
               array_has_active_slice(vm, previous.array)) {
                /* A live slice borrows the array's storage. Native array
                 * assignment updates that storage, so keep its identity and
                 * move the replacement elements into the existing slots. */
                for(int element = 0; element < previous.array->length; element++) {
                    Value old = previous.array->elements[element];
                    previous.array->elements[element] =
                        replacement.array->elements[element];
                    replacement.array->elements[element] = old;
                }
                retire_value(vm, replacement, 0);
                release_retired(vm);
                break;
            }
            *slot = replacement;
            if(previous.kind == VALUE_RECORD || previous.kind == VALUE_ARRAY) {
                retire_value(vm, previous, 0);
                release_retired(vm);
            }
            break;
        }
        case ZIR_STMT_RETURN:
            *result = statement->expr_root >= 0 ?
                eval(frame, statement->expr_root, 0) : int_value(0);
            return vm->failed ? FLOW_ERROR : FLOW_RETURN;
        case ZIR_STMT_UNREACHABLE:
            Diagnostic(statement->span, "vm.unreachable", "unreachable code executed");
            vm->failed = 1;
            return FLOW_ERROR;
        case ZIR_STMT_EXPR:
        case ZIR_STMT_UNUSED: {
            Value unused = eval(frame, statement->expr_root, 0);
            if(!vm->failed && statement->expr_root >= 0 &&
               function->exprs[statement->expr_root].kind == ZIR_EXPR_CALL &&
               VecElementType(frame->module,
                   function->exprs[statement->expr_root].type, NULL, 0)) {
                retire_value(vm, unused, 0);
                release_retired(vm);
            }
            break;
        }
        case ZIR_STMT_IF: {
            int branch = s;
            int executed = 0;
            do {
                const ZirStmt *head = &function->stmts[branch];
                close = statement_close(function, branch, end);
                if(close < 0)
                    return FLOW_ERROR;
                if(!executed && (head->expr_root < 0 ||
                                 truthy(eval(frame, head->expr_root, 0)))) {
                    if(vm->failed)
                        return FLOW_ERROR;
                    saved_locals = frame->local_count;
                    flow = execute_sequence(frame, branch + 1, close,
                                            depth + 1, result);
                    if(flow != FLOW_RETURN && flow != FLOW_ERROR)
                        drop_owned_locals(frame, saved_locals);
                    if(flow != FLOW_NEXT)
                        return flow;
                    executed = 1;
                }
                if(vm->failed)
                    return FLOW_ERROR;
                branch = close + 1;
            } while(branch < end &&
                    is_else_branch(&function->stmts[branch]));
            s = branch - 1;
            break;
        }
        case ZIR_STMT_WHILE:
            close = statement_close(function, s, end);
            if(close < 0)
                return FLOW_ERROR;
            saved_locals = frame->local_count;
            while(1) {
                if(++vm->steps > VM_MAX_STEPS)
                    return FLOW_ERROR;
                Value condition = eval(frame, statement->expr_root, 0);
                if(vm->failed)
                    return FLOW_ERROR;
                if(!truthy(condition))
                    break;
                flow = execute_sequence(frame, s + 1, close, depth + 1,
                                        result);
                if(flow != FLOW_RETURN && flow != FLOW_ERROR)
                    drop_owned_locals(frame, saved_locals);
                if(flow == FLOW_RETURN || flow == FLOW_ERROR)
                    return flow;
                if((flow == FLOW_BREAK || flow == FLOW_CONTINUE) &&
                   frame->control_target != 0 &&
                   frame->control_target != statement->loop_id)
                    return flow;
                if(flow == FLOW_BREAK) {
                    frame->control_target = 0;
                    break;
                }
                if(flow == FLOW_CONTINUE)
                    frame->control_target = 0;
            }
            s = close;
            break;
        case ZIR_STMT_BLOCK_OPEN:
            close = statement_close(function, s, end);
            if(close < 0)
                return FLOW_ERROR;
            saved_locals = frame->local_count;
            flow = execute_sequence(frame, s + 1, close, depth + 1, result);
            if(flow != FLOW_RETURN && flow != FLOW_ERROR)
                drop_owned_locals(frame, saved_locals);
            if(flow != FLOW_NEXT)
                return flow;
            s = close;
            break;
        case ZIR_STMT_BREAK:
            frame->control_target = statement->target_id;
            return FLOW_BREAK;
        case ZIR_STMT_CONTINUE:
            frame->control_target = statement->target_id;
            return FLOW_CONTINUE;
        default:
            return FLOW_ERROR;
        }
    }
    return vm->failed ? FLOW_ERROR : FLOW_NEXT;
}

static void
release_host_argument(VmHostValue *value, int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return;
    if(value->kind == VM_HOST_SLICE || value->kind == VM_HOST_ARRAY) {
        for(size_t i = 0; i < value->length && value->elements != NULL; i++)
            release_host_argument(&value->elements[i], depth + 1);
        free(value->elements);
        value->elements = NULL;
        return;
    }
    if(value->kind != VM_HOST_RECORD || value->fields == NULL)
        return;
    VmHostField *fields = (VmHostField *)value->fields;
    for(size_t i = 0; i < value->field_count; i++)
        release_host_argument(&fields[i].value, depth + 1);
    free(fields);
    value->fields = NULL;
}

static int
host_argument(const ZirModule *module, const char *type, Value value,
              VmHostValue *out, int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return 0;
    out->type = type;
    char element[ZIR_NAME_MAX];
    int capacity;
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        if(capacity < 0 || value.kind != VALUE_ARRAY || value.array == NULL ||
           value.array->length != capacity ||
           strcmp(value.array->element_type, element) != 0)
            return 0;
        out->kind = VM_HOST_ARRAY;
        out->length = (size_t)capacity;
        if(capacity == 0)
            return 1;
        out->elements = calloc((size_t)capacity, sizeof(*out->elements));
        if(out->elements == NULL)
            return 0;
        for(int i = 0; i < capacity; i++)
            if(!host_argument(module, value.array->element_type,
                              value.array->elements[i],
                              &out->elements[i], depth + 1))
                return 0;
        return 1;
    }
    if(SliceElementType(type, element, sizeof(element))) {
        if(value.kind != VALUE_SLICE ||
           (value.length > 0 && value.array == NULL) ||
           (value.array != NULL &&
            (strcmp(value.array->element_type, element) != 0 ||
             value.offset > (size_t)value.array->length ||
             value.length > (size_t)value.array->length - value.offset)))
            return 0;
        out->kind = VM_HOST_SLICE;
        out->length = value.length;
        out->elements = calloc(value.length ? value.length : 1,
                               sizeof(*out->elements));
        if(out->elements == NULL)
            return 0;
        for(size_t i = 0; i < value.length; i++)
            if(!host_argument(module, value.array->element_type,
                              value.array->elements[value.offset + i],
                              &out->elements[i], depth + 1))
                return 0;
        return 1;
    }
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record != NULL && !record->is_enum) {
        if(value.kind != VALUE_RECORD || value.record == NULL ||
           value.record->type != record ||
           value.record->field_count < 0 ||
           value.record->field_count > VM_MAX_FIELDS)
            return 0;
        int count = value.record->field_count;
        VmHostField *fields = calloc(count ? (size_t)count : 1,
                                     sizeof(*fields));
        if(fields == NULL)
            return 0;
        out->kind = VM_HOST_RECORD;
        out->fields = fields;
        out->field_count = (size_t)count;
        for(int i = 0; i < count; i++) {
            fields[i].name = value.record->fields[i].field.name;
            if(!host_argument(owner,
                              value.record->fields[i].field.type,
                              value.record->fields[i].value,
                              &fields[i].value, depth + 1))
                return 0;
        }
        return 1;
    }
    if(record != NULL && record->is_enum && value.kind != VALUE_ENUM)
        return 0;
    out->integer = value.integer;
    out->bits = value.bits;
    out->real = value.real;
    out->data = value.data;
    out->length = value.length;
    out->kind = value.kind == VALUE_REAL ? VM_HOST_REAL :
        value.kind == VALUE_STRING ? VM_HOST_STRING :
        type[0] == '*' ? VM_HOST_POINTER :
        type[0] == 'u' ? VM_HOST_UNSIGNED : VM_HOST_INTEGER;
    if(out->kind == VM_HOST_POINTER) {
        if(value.kind != VALUE_INT)
            return 0;
        out->pointer = (void *)(uintptr_t)integer_bits(value);
        return 1;
    }
    return value.kind == VALUE_INT || value.kind == VALUE_ENUM ||
           value.kind == VALUE_REAL || value.kind == VALUE_STRING;
}

static Value
host_return(Vm *vm, const ZirModule *module, const char *type,
            const VmHostValue *input, int depth)
{
    Value result = int_value(0);
    if(depth >= VM_MAX_DEPTH || input->type == NULL ||
       strcmp(input->type, type) != 0) {
        vm->failed = 1;
        return result;
    }
    char array_element[ZIR_NAME_MAX];
    int capacity;
    if(ArrayElementType(type, array_element, sizeof(array_element),
                        &capacity)) {
        if(capacity < 0 || input->kind != VM_HOST_ARRAY ||
           input->length != (size_t)capacity ||
           (capacity > 0 && input->elements == NULL) ||
           input->field_count != 0 || input->fields != NULL) {
            vm->failed = 1;
            return result;
        }
        Array *array = allocate_array_try(vm, module, array_element,
                                          capacity, 1);
        if(array == NULL)
            return result;
        for(int i = 0; i < capacity && !vm->failed; i++)
            array->elements[i] = host_return(vm, module, array_element,
                                              &input->elements[i], depth + 1);
        return (Value){.kind = VALUE_ARRAY, .array = array};
    }
    const ZirModule *owner = NULL;
    const ZirType *declared = FindType(module, type, &owner);
    if(declared != NULL && !declared->is_enum) {
        if(input->kind != VM_HOST_RECORD ||
           (input->field_count > 0 && input->fields == NULL) ||
           input->field_count > VM_MAX_FIELDS) {
            vm->failed = 1;
            return result;
        }
        size_t offset = 0;
        ZirTypeField field;
        int count = 0, status;
        while((status = TypeNextField(declared, &offset, &field)) == 1)
            count++;
        if(status < 0 || count != (int)input->field_count) {
            vm->failed = 1;
            return result;
        }
        Record *record = allocate_record(vm, owner, declared, count);
        if(record == NULL)
            return result;
        offset = 0;
        for(int i = 0; i < count && !vm->failed; i++) {
            if(TypeNextField(declared, &offset,
                             &record->fields[i].field) != 1 ||
               input->fields[i].name == NULL ||
               strcmp(input->fields[i].name,
                      record->fields[i].field.name) != 0) {
                vm->failed = 1;
                break;
            }
            record->fields[i].value = host_return(vm, owner,
                record->fields[i].field.type,
                &input->fields[i].value, depth + 1);
        }
        return (Value){.kind = VALUE_RECORD, .record = record};
    }
    ValueKind expected = value_kind(type);
    char slice_element[ZIR_NAME_MAX];
    if(SliceElementType(type, slice_element, sizeof(slice_element))) {
        /* A host-returned slice is copied into VM-owned storage; the caller
         * owns the elements from here on. */
        Array *owned;
        if(input->kind != VM_HOST_SLICE ||
           (input->length > 0 && input->elements == NULL) ||
           input->length >
               (VM_MAX_ARRAY_BYTES - sizeof(Array)) / sizeof(Value) ||
           input->field_count != 0 || input->fields != NULL) {
            vm->failed = 1;
            return result;
        }
        owned = allocate_array_try(vm, module, slice_element,
                                   (int)input->length, 1);
        if(owned == NULL)
            return result;
        for(size_t i = 0; i < input->length && !vm->failed; i++)
            owned->elements[i] = host_return(vm, module, slice_element,
                                             &input->elements[i], depth + 1);
        if(vm->failed)
            return result;
        return (Value){.kind = VALUE_SLICE, .array = owned,
                       .offset = 0, .length = input->length};
    }
    VmHostValueKind kind = expected == VALUE_VOID ? VM_HOST_VOID :
        expected == VALUE_REAL ? VM_HOST_REAL :
        expected == VALUE_STRING ? VM_HOST_STRING :
        type[0] == '*' ? VM_HOST_POINTER :
        type[0] == 'u' ? VM_HOST_UNSIGNED : VM_HOST_INTEGER;
    if((declared == NULL && expected == VALUE_INVALID) ||
       input->kind != kind || input->field_count != 0 ||
       input->fields != NULL ||
       (kind == VM_HOST_STRING && input->data == NULL &&
        input->length != 0)) {
        vm->failed = 1;
        return result;
    }
    if(kind == VM_HOST_REAL)
        result = real_value(input->real);
    else if(kind == VM_HOST_STRING)
        result = string_value(input->data, input->length);
    else if(kind == VM_HOST_POINTER)
        result = uint_value((uintptr_t)input->pointer);
    else if(kind == VM_HOST_UNSIGNED)
        result = uint_value(input->bits);
    else if(kind == VM_HOST_INTEGER)
        result = int_value(input->integer);
    else
        result.kind = VALUE_VOID;
    return coerce(vm, module, result, type);
}

static int
host_copy_back(Vm *vm, const ZirModule *module, const char *type,
               Value target, const VmHostValue *input)
{
    char element[ZIR_NAME_MAX];
    if(!SliceElementType(type, element, sizeof(element)))
        return 1;
    if(input->kind != VM_HOST_SLICE || input->type == NULL ||
       strcmp(input->type, type) != 0 || target.kind != VALUE_SLICE ||
       target.length != input->length ||
       (target.length > 0 &&
        (target.array == NULL || input->elements == NULL))) {
        vm->failed = 1;
        return 0;
    }
    if(target.length == 0)
        return 1;
    Value *updates = calloc(target.length, sizeof(*updates));
    if(updates == NULL) {
        vm->failed = 1;
        return 0;
    }
    for(size_t i = 0; i < target.length && !vm->failed; i++)
        updates[i] = host_return(vm, module, element,
                                 &input->elements[i], 1);
    if(!vm->failed) {
        for(size_t i = 0; i < target.length; i++) {
            Value *slot = &target.array->elements[target.offset + i];
            Value previous = *slot;
            *slot = updates[i];
            retire_value(vm, previous, 0);
        }
        release_retired(vm);
    }
    free(updates);
    return !vm->failed;
}

static const char *
assignment_root_name(const ZirFunction *function, int index)
{
    while(index >= 0 && index < function->expr_count) {
        const ZirExpr *expression = &function->exprs[index];
        if(expression->kind == ZIR_EXPR_IDENT)
            return expression->name;
        if(expression->kind != ZIR_EXPR_MEMBER &&
           expression->kind != ZIR_EXPR_INDEX)
            return NULL;
        index = expression->left;
    }
    return NULL;
}

static int
function_uses_slots(const ZirFunction *function)
{
    for(int i = 0; i < function->expr_count; i++) {
        if(function->exprs[i].is_function_value ||
           function->exprs[i].slot_type[0])
            return 1;
    }
    return 0;
}

/* A record or array parameter can share its caller's value only when the
 * callee cannot write through that parameter. Other storage paths still copy
 * values, so passing a parameter onward to a mutating function stays safe. */
static int
parameter_read_only(const ZirFunction *function, const char *name)
{
    if(function_uses_slots(function))
        return 0;
    for(int i = 0; i < function->stmt_count; i++) {
        const ZirStmt *statement = &function->stmts[i];
        if(statement->kind == ZIR_STMT_ASSIGN) {
            const char *root = assignment_root_name(function,
                                                    statement->lhs_root);
            if(root == NULL || strcmp(root, name) == 0)
                return 0;
        }
    }
    return 1;
}

/* Globals can receive values during a call. Keep every allocation reachable
 * from them when reclaiming completed call temporaries. */
static void
pin_value(Vm *vm, Value value, int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return;
    if(value.kind == VALUE_RECORD && value.record != NULL &&
       value.record->pinned != vm->pin_generation) {
        value.record->pinned = vm->pin_generation;
        for(int i = 0; i < value.record->field_count; i++)
            pin_value(vm, value.record->fields[i].value, depth + 1);
    } else if(value.kind == VALUE_ARRAY && value.array != NULL &&
              value.array->pinned != vm->pin_generation) {
        value.array->pinned = vm->pin_generation;
        for(int i = 0; i < value.array->length; i++)
            pin_value(vm, value.array->elements[i], depth + 1);
    } else if(value.kind == VALUE_SLICE && value.array != NULL) {
        pin_value(vm, (Value){.kind = VALUE_ARRAY, .array = value.array},
                  depth + 1);
    }
}

static void
pin_globals(Vm *vm)
{
    for(int i = 0; i < vm->global_count; i++)
        pin_value(vm, vm->globals[i].value, 0);
}

/* A callee may write through a slice borrowed from a caller. Its newly
 * allocated record elements then belong to that caller's array even though
 * their allocation sequence falls inside the completed call. */
static void
pin_active_frames(Vm *vm)
{
    for(Frame *frame = vm->active_frame; frame != NULL;
        frame = frame->caller)
        for(int i = 0; i < frame->local_count; i++)
            pin_value(vm, frame->locals[i].value, 0);
}

static void
collect_unreachable(Vm *vm)
{
    uint64_t remaining_retired = 0;
    vm->pin_generation++;
    pin_globals(vm);
    pin_active_frames(vm);
    Record **record = &vm->records;
    while(*record != NULL) {
        Record *current = *record;
        if(current->pinned != vm->pin_generation) {
            *record = current->next;
            vm->record_bytes -= sizeof(Record) +
                (size_t)current->field_count * sizeof(RecordField);
            free(current);
        } else {
            if(current->retired &&
               (remaining_retired == 0 ||
                current->allocation < remaining_retired))
                remaining_retired = current->allocation;
            record = &current->next;
        }
    }
    Array **array = &vm->arrays;
    while(*array != NULL) {
        Array *current = *array;
        if(current->pinned != vm->pin_generation) {
            *array = current->next;
            vm->array_bytes -= sizeof(Array) +
                (size_t)current->length * sizeof(Value);
            free(current);
        } else {
            if(current->retired &&
               (remaining_retired == 0 ||
                current->allocation < remaining_retired))
                remaining_retired = current->allocation;
            array = &current->next;
        }
    }
    vm->retire_floor = remaining_retired;
    vm->allocated_since_collection = 0;
}

/* Result coercion creates its own deep copy after before_result. Reclaim
 * earlier allocations from the completed call by allocation sequence, since
 * replacing a global can remove the record that was at the call's entry. */
static void
release_call_records(Vm *vm, uint64_t entry, uint64_t before_result)
{
    Record **record = &vm->records;
    while(*record != NULL) {
        Record *current = *record;
        if(current->allocation > entry &&
           current->allocation <= before_result &&
           current->pinned != vm->pin_generation) {
            *record = current->next;
            vm->record_bytes -= sizeof(Record) +
                (size_t)current->field_count * sizeof(RecordField);
            free(current);
        } else {
            if(current->allocation <= entry)
                break;
            record = &current->next;
        }
    }
}

static void
release_call_arrays(Vm *vm, uint64_t entry, uint64_t before_result)
{
    Array **array = &vm->arrays;
    while(*array != NULL) {
        Array *current = *array;
        if(current->allocation > entry &&
           current->allocation <= before_result &&
           current->pinned != vm->pin_generation) {
            *array = current->next;
            vm->array_bytes -= sizeof(Array) +
                (size_t)current->length * sizeof(Value);
            free(current);
        } else {
            if(current->allocation <= entry)
                break;
            array = &current->next;
        }
    }
}

static Value
run_function(Vm *vm, const ZirModule *module, const ZirFunction *function,
             const Value *args, int arg_count)
{
    Frame frame = {0};
    Parameter parameters[VM_MAX_PARAMS];
    int count = parse_parameters(module, function, parameters);
    Value result = int_value(0);
    uint64_t allocation_entry = vm->allocation;
    if(vm->failed || vm->depth >= VM_MAX_DEPTH || count != arg_count) {
        vm->failed = 1;
        return result;
    }
    if(function->is_extern) {
        const ZirImport *import = host_import(module, function->name);
        if(import != NULL && strncmp(import->target, "ziran:", 6) == 0) {
            const ZirModule *provider_module = NULL;
            const ZirFunction *provider = bound_provider(vm->program, import,
                &provider_module);
            if(provider == NULL) {
                vm->failed = 1;
                return result;
            }
            return run_function(vm, provider_module, provider, args, arg_count);
        }
        VmHostValue host_args[VM_MAX_PARAMS] = {0};
        VmHostValue host_result = {0};
        if(vm->host == NULL) {
            Diagnostic(function->span, "zib.capability",
                       "missing host capability: %s:%s",
                       module->name, function->name);
            vm->failed = 1;
            return result;
        }
        for(int i = 0; i < count && !vm->failed; i++) {
            if(args[i].kind != VALUE_SLICE || args[i].length == 0)
                continue;
            for(int j = i + 1; j < count; j++) {
                if(args[j].kind != VALUE_SLICE || args[j].length == 0 ||
                   args[i].array != args[j].array)
                    continue;
                if(args[i].offset < args[j].offset + args[j].length &&
                   args[j].offset < args[i].offset + args[i].length) {
                    Diagnostic(function->span, "zib.host_alias",
                               "overlapping mutable host slice arguments require separate storage");
                    vm->failed = 1;
                    break;
                }
            }
        }
        for(int i = 0; i < count; i++) {
            if(vm->failed)
                break;
            Value argument = coerce_expression(vm, module, args[i],
                                               parameters[i].type);
            if(vm->failed || !host_argument(module,
                                            parameters[i].type, argument,
                                            &host_args[i], 0)) {
                vm->failed = 1;
                break;
            }
        }
        host_result.type = function->return_type;
        if(!vm->failed && !vm->host(vm->host_context, module->name,
                                    function->name, host_args, count,
                                    &host_result))
            vm->failed = 1;
        for(int i = 0; i < count && !vm->failed; i++)
            host_copy_back(vm, module, parameters[i].type,
                           args[i], &host_args[i]);
        for(int i = 0; i < count; i++)
            release_host_argument(&host_args[i], 0);
        if(vm->failed)
            return result;
        return host_return(vm, module, function->return_type,
                           &host_result, 0);
    }
    vm->depth++;
    frame.vm = vm;
    frame.module = module;
    frame.function = function;
    frame.caller = vm->active_frame;
    vm->active_frame = &frame;
    for(int i = 0; i < count; i++) {
        copy_text(frame.locals[i].name, sizeof(frame.locals[i].name),
                  parameters[i].name);
        copy_text(frame.locals[i].type, sizeof(frame.locals[i].type),
                  parameters[i].type);
        frame.locals[i].value = !VecElementType(module, parameters[i].type,
                                                NULL, 0) &&
                                parameter_read_only(function,
                                                    parameters[i].name) ?
            coerce_expression(vm, module, args[i], parameters[i].type) :
            coerce(vm, module, args[i], parameters[i].type);
    }
    frame.local_count = count;
    Flow flow = execute_sequence(&frame, 0, function->stmt_count, 0, &result);
    if(flow == FLOW_ERROR || flow == FLOW_BREAK || flow == FLOW_CONTINUE ||
       (flow != FLOW_RETURN && strcmp(function->return_type, "void") != 0))
        vm->failed = 1;
    uint64_t allocation_before_result = vm->allocation;
    Value returned = coerce(vm, module, result, function->return_type);
    /* Nested return paths leave their locals in this frame. Materialize the
     * return value before releasing those bindings. */
    drop_owned_locals(&frame, 0);
    vm->active_frame = frame.caller;
    vm->depth--;
    /* Named Ziran procedure values carry no borrowed frame context. The
     * returned value has already been copied, so slot-using calls can drop
     * temporaries by the same reachability rule as direct calls. */
    /* Nested calls are collected at a top-level statement boundary. A call
     * with unusually large live storage still reclaims its own temporaries
     * before the VM's allocation limits are reached. */
    if(!vm->failed && (vm->depth == 0 ||
       vm->record_bytes > VM_MAX_RECORD_BYTES / 2 ||
       vm->array_bytes > VM_MAX_ARRAY_BYTES / 2)) {
        vm->pin_generation++;
        pin_globals(vm);
        pin_active_frames(vm);
        pin_value(vm, returned, 0);
        release_call_records(vm, allocation_entry, allocation_before_result);
        release_call_arrays(vm, allocation_entry, allocation_before_result);
    }
    return returned;
}

static void
free_records(Vm *vm)
{
    while(vm->records != NULL) {
        Record *next = vm->records->next;
        free(vm->records);
        vm->records = next;
    }
}

static void
free_arrays(Vm *vm)
{
    while(vm->arrays != NULL) {
        Array *next = vm->arrays->next;
        free(vm->arrays);
        vm->arrays = next;
    }
}

static void
free_strings(Vm *vm)
{
    while(vm->strings != NULL) {
        StringLiteral *next = vm->strings->next;
        free(vm->strings);
        vm->strings = next;
    }
}

/* Fold one parsed initializer expression into a global's storage slot.
 * Types come from the declared global, never the probe expression (parsed
 * standalone, so it carries no inferred types). Scalars, string literals,
 * records, and arrays recurse through the declared storage shape. */
static int
fold_global_element(Vm *vm, const ZirModule *module, const ZirFunction *probe,
                    int index, Value *target, const char *type,
                    ZirSourceSpan span)
{
    const ZirExpr *expr = &probe->exprs[index];
    if((target->kind == VALUE_ARRAY || target->kind == VALUE_RECORD) &&
       expr->kind != ZIR_EXPR_COMPOUND)
        return 0;
    if(expr->kind == ZIR_EXPR_COMPOUND && target->kind == VALUE_ARRAY) {
        char element[ZIR_NAME_MAX];
        int position = 0;
        if(!ArrayElementType(type, element, sizeof(element), NULL))
            return 0;
        for(int child = expr->first_child; child >= 0;
            child = probe->exprs[child].next_sibling) {
            const ZirExpr *entry = &probe->exprs[child];
            if(entry->right < 0 || position >= target->array->length ||
               !fold_global_element(vm, module, probe, entry->right,
                                    &target->array->elements[position],
                                    element, span))
                return 0;
            position++;
        }
        return 1;
    }
    if(expr->kind == ZIR_EXPR_COMPOUND && target->kind == VALUE_RECORD) {
        const ZirType *record = target->record->type;
        int matched = record != NULL;
        for(int child = expr->first_child; child >= 0 && matched;
            child = probe->exprs[child].next_sibling) {
            const ZirExpr *entry = &probe->exprs[child];
            char field_type[ZIR_NAME_MAX] = "";
            Value *field = entry->name[0] ?
                record_field(target->record, entry->name) : NULL;
            size_t offset = 0;
            ZirTypeField decl;
            if(field == NULL || entry->right < 0)
                return 0;
            while(TypeNextField(record, &offset, &decl) == 1)
                if(!strcmp(decl.name, entry->name)) {
                    copy_text(field_type, sizeof(field_type), decl.type);
                    break;
                }
            if(field_type[0] == '\0') return 0;
            matched = fold_global_element(vm, module, probe,
                                          entry->right, field, field_type,
                                          span);
        }
        return matched;
    }
    if(target->kind == VALUE_STRING) {
        *target = global_literal_string(vm, expr->text);
        return !vm->failed;
    }
    {
        int64_t folded = 0;
        if(EvaluateCompileExpression(module, expr->text, span, 0, &folded)) {
            *target = target->kind == VALUE_REAL ?
                real_value((double)folded) : int_value(folded);
            return 1;
        }
        char *end = NULL;
        double number = strtod(expr->text, &end);
        if(end != expr->text && *skip_ws(end) == '\0' &&
           isfinite(number) && target->kind == VALUE_REAL) {
            *target = real_value(number);
            return 1;
        }
    }
    return 0;
}

static int
initialize_module_startup(Vm *vm, const ZirProgram *program, int index,
                          unsigned char *state)
{
    const ZirModule *module = &program->modules[index];
    if(state[index] == 2) return 1;
    if(state[index] == 1) {
        Diagnostic(module->span, "zib.global",
                   "cyclic module startup dependency: %s", module->name);
        vm->failed = 1;
        return 0;
    }
    state[index] = 1;
    for(int i = 0; i < module->import_count; i++) {
        const ZirModule *dependency = module->imports[i].resolved_module;
        if(dependency == NULL) continue;
        for(int m = 0; m < program->module_count; m++)
            if(dependency == &program->modules[m] &&
               !initialize_module_startup(vm, program, m, state))
                return 0;
    }
    for(int f = 0; f < module->function_count; f++)
        if(module->functions[f].is_global_initializer) {
            run_function(vm, module, &module->functions[f], NULL, 0);
            if(vm->failed) return 0;
        }
    state[index] = 2;
    return 1;
}

static int
initialize_globals(Vm *vm, const ZirProgram *program)
{
    int count = 0;
    for(int m = 0; m < program->module_count; m++) {
        if(program->modules[m].global_count < 0 ||
           program->modules[m].global_count > VM_MAX_GLOBALS - count)
            return 0;
        count += program->modules[m].global_count;
    }
    if(count == 0)
        return 1;
    vm->globals = calloc((size_t)count, sizeof(*vm->globals));
    if(vm->globals == NULL)
        return 0;
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        for(int g = 0; g < module->global_count; g++) {
            GlobalSlot *slot = &vm->globals[vm->global_count++];
            slot->module = module;
            slot->declaration = &module->globals[g];
            slot->value = default_value(vm, module,
                                        slot->declaration->type, 0);
            if(!vm->failed && slot->declaration->init[0]) {
                const char *init = skip_ws(slot->declaration->init);
                if(slot->value.kind == VALUE_ARRAY ||
                   slot->value.kind == VALUE_RECORD) {
                    ZirFunction probe = {0};
                    int root = ParseExprTyped(&probe, module,
                                               slot->declaration->init,
                                               slot->declaration->span,
                                               slot->declaration->type);
                    int matched = root >= 0 &&
                        fold_global_element(vm, module, &probe, root,
                                            &slot->value,
                                            slot->declaration->type,
                                            slot->declaration->span);
                    free(probe.exprs);
                    if(!matched)
                        vm->failed = 1;
                } else if(!strcmp(slot->declaration->type, "string")) {
                    slot->value = global_literal_string(vm, init);
                } else {
                    int64_t folded = 0;
                    if(EvaluateCompileExpression(module, init,
                                                slot->declaration->span, 0,
                                                &folded)) {
                        Value coerced = coerce(vm, module,
                                               int_value(folded),
                                               slot->declaration->type);
                        if(!vm->failed)
                            slot->value = coerced;
                    } else if(!strcmp(slot->declaration->type,
                                      "float32") ||
                              !strcmp(slot->declaration->type,
                                      "float64")) {
                        char *end = NULL;
                        double number = strtod(init, &end);
                        if(end != init && *skip_ws(end) == '\0')
                            slot->value = real_value(number);
                        else
                            vm->failed = 1;
                    } else if(!strcmp(slot->declaration->type, "bool")) {
                        slot->value = int_value(
                            strcmp(init, "true") == 0);
                    } else
                        vm->failed = 1;
                }
                if(vm->failed)
                    return 0;
            }
            if(vm->failed)
                return 0;
        }
    }
    unsigned char *state = calloc((size_t)program->module_count, 1);
    if(state == NULL) return 0;
    int initialized = 1;
    for(int m = 0; m < program->module_count && initialized; m++)
        initialized = initialize_module_startup(vm, program, m, state);
    free(state);
    return initialized;
}

VmInstance *
VmInstanceOpen(const ZirProgram *program, const char *entry_module,
               const char *entry_function, VmHostCall host, void *context)
{
    if(!VmVerify(program, entry_module, entry_function))
        return NULL;
    VmInstance *instance = calloc(1, sizeof(*instance));
    if(instance == NULL)
        return NULL;
    instance->vm.program = program;
    instance->vm.host = host;
    instance->vm.host_context = context;
    instance->entry = find_entry(program, entry_module, entry_function,
                                 &instance->module);
    if(instance->entry == NULL ||
       !initialize_globals(&instance->vm, program)) {
        VmInstanceClose(instance);
        return NULL;
    }
    return instance;
}

int
VmInstanceRun(VmInstance *instance, long long *result, int *has_result)
{
    if(instance == NULL || result == NULL || has_result == NULL ||
       instance->vm.failed)
        return 0;
    Vm *vm = &instance->vm;
    vm->steps = 0;
    Value value = run_function(vm, instance->module, instance->entry,
                               NULL, 0);
    *result = value.integer;
    *has_result = strcmp(instance->entry->return_type, "void") != 0;
    if(vm->failed) {
        Diagnostic(instance->entry->span, "zib.runtime",
                      "portable execution failed");
        return 0;
    }
    return 1;
}

size_t
VmInstanceLiveValueBytes(const VmInstance *instance)
{
    return instance == NULL ? 0 :
        instance->vm.record_bytes + instance->vm.array_bytes;
}

void
VmInstanceClose(VmInstance *instance)
{
    if(instance == NULL)
        return;
    free_records(&instance->vm);
    free_arrays(&instance->vm);
    free_strings(&instance->vm);
    free(instance->vm.globals);
    free(instance);
}

int
VmRunWithHost(const ZirProgram *program, const char *entry_module,
              const char *entry_function, VmHostCall host, void *context,
              long long *result, int *has_result)
{
    VmInstance *instance = VmInstanceOpen(program, entry_module,
        entry_function, host, context);
    if(instance == NULL)
        return 0;
    int ok = VmInstanceRun(instance, result, has_result);
    VmInstanceClose(instance);
    return ok;
}

int
VmRun(const ZirProgram *program, const char *entry_module,
      const char *entry_function, long long *result, int *has_result)
{
    return VmRunWithHost(program, entry_module, entry_function,
                         NULL, NULL, result, has_result);
}
