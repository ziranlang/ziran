#include "zir_vm_internal.h"

const ZirImport *
host_import(const ZirModule *module, const char *name)
{
    for(int i = 0; i < module->import_count; i++)
        if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
           strcmp(module->imports[i].name, name) == 0)
            return &module->imports[i];
    return NULL;
}

const ZirFunction *
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

int
same_bound_type(const ZirModule *caller, const ZirModule *provider,
                const char *type)
{
    const ZirModule *left_owner = NULL, *right_owner = NULL;
    const ZirType *left = FindType(caller, type, &left_owner);
    const ZirType *right = FindType(provider, type, &right_owner);
    return left == right ||
           same_type_application(left_owner, left, right_owner, right);
}

int
same_record_type(const ZirModule *owner, const ZirType *type, const Record *record)
{
    return type != NULL && record != NULL &&
           (type == record->type ||
            same_type_application(owner, type, record->owner, record->type));
}

ValueKind
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

int
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

int
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

int
portable_type(const ZirModule *module, const char *type)
{
    return portable_type_at(module, type, 0, NULL);
}

/* Host values have a declared, recursive record or array shape. A slice
 * parameter is copied into host values and copied back after the synchronous
 * call. Fixed arrays are copied by value. */
int
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
    if(record == NULL || record->is_extern || record->is_procedure_type || record->is_map)
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

Value
int_value(int64_t integer)
{
    Value value = {.kind = VALUE_INT, .integer = integer,
                   .bits = (uint64_t)integer};
    return value;
}

Value
uint_value(uint64_t bits)
{
    Value value = {.kind = VALUE_INT, .bits = bits, .unsigned64 = 1};
    return value;
}

Value
real_value(double real)
{
    Value value = {.kind = VALUE_REAL, .real = real};
    return value;
}

Value
string_value(const unsigned char *data, size_t length)
{
    Value value = {.kind = VALUE_STRING, .data = data, .length = length};
    return value;
}

Value
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

Value
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

Value
enum_value(const ZirType *type, int64_t integer)
{
    Value value = {.kind = VALUE_ENUM, .integer = integer,
                   .bits = (uint64_t)(int64_t)integer,
                   .enumeration = type};
    return value;
}

uint64_t
integer_bits(Value value)
{
    return value.unsigned64 ? value.bits : (uint64_t)value.integer;
}

int64_t
signed64(uint64_t bits)
{
    return bits <= INT64_MAX ? (int64_t)bits :
           -1 - (int64_t)(UINT64_MAX - bits);
}

double
as_real(Value value)
{
    if(value.kind == VALUE_REAL)
        return value.real;
    return value.unsigned64 ? (double)value.bits : (double)value.integer;
}

int
truthy(Value value)
{
    return value.kind == VALUE_REAL ? value.real != 0.0 :
           integer_bits(value) != 0;
}

static const VmLayout *
parse_layout(VmLayout *layout, const ZirType *type)
{
    ZirTypeField field;
    size_t offset = 0;
    int count = 0, status;
    layout->type = type;
    layout->count = -1;
    layout->fields = NULL;
    while((status = TypeNextField(type, &offset, &field)) == 1)
        count++;
    if(status < 0 || count > VM_MAX_FIELDS)
        return layout;
    layout->fields = calloc(count ? (size_t)count : 1, sizeof(*layout->fields));
    if(layout->fields == NULL)
        return layout;
    offset = 0;
    for(int i = 0; i < count; i++) {
        if(TypeNextField(type, &offset, &field) != 1) {
            free(layout->fields);
            layout->fields = NULL;
            return layout;
        }
        layout->fields[i].name = KeepText(field.name);
        layout->fields[i].type = KeepText(field.type);
    }
    layout->count = count;
    return layout;
}

static size_t
layout_slot(const VmLayout *layouts, size_t slots, const ZirType *type)
{
    size_t slot = ((uintptr_t)type >> 4) * 0x9e3779b97f4a7c15u & (slots - 1);
    while(layouts[slot].type != NULL && layouts[slot].type != type)
        slot = (slot + 1) & (slots - 1);
    return slot;
}

/* Record allocation happens for every record value the VM builds, so the
 * field list is parsed from the type text once and reused. */
const VmLayout *
record_layout(Vm *vm, const ZirType *type)
{
    if(type == NULL)
        return NULL;
    if(vm->layout_slots != 0) {
        size_t slot = layout_slot(vm->layouts, vm->layout_slots, type);
        if(vm->layouts[slot].type == type)
            return &vm->layouts[slot];
    }
    if((vm->layout_count + 1) * 2 > vm->layout_slots) {
        size_t slots = vm->layout_slots ? vm->layout_slots * 2 : 64;
        VmLayout *layouts = calloc(slots, sizeof(*layouts));
        if(layouts == NULL)
            return NULL;
        for(size_t i = 0; i < vm->layout_slots; i++)
            if(vm->layouts[i].type != NULL)
                layouts[layout_slot(layouts, slots, vm->layouts[i].type)] =
                    vm->layouts[i];
        free(vm->layouts);
        vm->layouts = layouts;
        vm->layout_slots = slots;
    }
    size_t slot = layout_slot(vm->layouts, vm->layout_slots, type);
    vm->layout_count++;
    return parse_layout(&vm->layouts[slot], type);
}

void
free_layouts(Vm *vm)
{
    for(size_t i = 0; i < vm->layout_slots; i++)
        free(vm->layouts[i].fields);
    free(vm->layouts);
    vm->layouts = NULL;
    vm->layout_slots = vm->layout_count = 0;
}

Record *
allocate_record(Vm *vm, const ZirModule *owner,
                const ZirType *type, int count)
{
    size_t bytes = sizeof(Record) + (size_t)count * sizeof(RecordField);
    if(count < 0 || count > VM_MAX_FIELDS ||
       bytes > VM_MAX_RECORD_BYTES - vm->record_bytes) {
        if(!vm->failed && count >= 0 && count <= VM_MAX_FIELDS)
            Diagnostic(Span("<bundle>", 1, 1), "zib.memory",
                       "records exceed the portable runner's %d MB budget",
                       VM_MAX_RECORD_BYTES / (1024 * 1024));
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

/* Whether a value of this type can reach VM records or arrays. Scalars,
 * strings, raw pointers (opaque handles), enums, and procedure values
 * cannot, so arrays of them need no element walk when marking or copying. */
static int
type_holds_references(const ZirModule *module, const char *type)
{
    if(value_kind(type) != VALUE_INVALID)
        return 0;
    const ZirType *found = module != NULL ? FindType(module, type, NULL) : NULL;
    return found == NULL || !(found->is_enum || found->is_procedure_type);
}

Array *
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
        if(fail_hard && !vm->failed)
            Diagnostic(Span("<bundle>", 1, 1), "zib.memory",
                       "arrays exceed the portable runner's %d MB budget",
                       VM_MAX_ARRAY_BYTES / (1024 * 1024));
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
    array->holds_references = type_holds_references(owner, element);
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
        if(!value.array->holds_references)
            memcpy(copy->elements, value.array->elements,
                   (size_t)copy->length * sizeof(Value));
        else
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

Value
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
        if(!array->holds_references && capacity > 0) {
            /* Scalar defaults allocate nothing, so one serves every slot. */
            Value zero = default_value(vm, module, element, depth + 1);
            for(int i = 0; i < capacity; i++)
                array->elements[i] = zero;
        } else
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
       record_type->is_extern || record_type->is_map || depth >= VM_MAX_DEPTH) {
        vm->failed = 1;
        return int_value(0);
    }
    const VmLayout *layout = record_layout(vm, record_type);
    if(layout == NULL || layout->count < 0) {
        vm->failed = 1;
        return int_value(0);
    }
    /* Nested defaults can grow the layout table; its field arrays stay put. */
    int count = layout->count;
    const VmField *fields = layout->fields;
    Record *record = allocate_record(vm, owner, record_type, count);
    if(record == NULL)
        return int_value(0);
    for(int i = 0; i < count && !vm->failed; i++) {
        record->fields[i].field = fields[i];
        if(i == 0 && VecElementType(module, type, NULL, 0))
            record->fields[i].value = (Value){.kind = VALUE_ARRAY};
        else
            record->fields[i].value = default_value(vm, owner,
                record->fields[i].field.type, depth + 1);
    }
    return (Value){.kind = VALUE_RECORD, .record = record};
}

Value
coerce(Vm *vm, const ZirModule *module, Value value, const char *type)
{
    ValueKind target = value_kind(type);
    /* A VM pointer stays a pointer in any pointer-typed storage. */
    if(value.kind == VALUE_POINTER && type[0] == '*')
        return value;
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
        const ZirModule *owner = NULL;
        const ZirType *record = FindType(module, type, &owner);
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
           !record->is_extern && !record->is_map && value.kind == VALUE_RECORD &&
           same_record_type(owner, record, value.record))
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
Value
coerce_expression(Vm *vm, const ZirModule *module,
                  Value value, const char *type)
{
    if(value.kind == VALUE_RECORD && value.record != NULL) {
        const ZirModule *owner = NULL;
        const ZirType *record = FindType(module, type, &owner);
        if(record != NULL && !record->is_enum && !record->is_procedure_type &&
           !record->is_extern && !record->is_map && same_record_type(owner, record, value.record))
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
void
retire_value(Vm *vm, Value value, int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return;
    if(value.kind == VALUE_RECORD && value.record != NULL &&
       !value.record->retired && !value.record->address_taken) {
        value.record->retired = 1;
        if(vm->retire_floor == 0 ||
           value.record->allocation < vm->retire_floor)
            vm->retire_floor = value.record->allocation;
        for(int i = 0; i < value.record->field_count; i++)
            retire_value(vm, value.record->fields[i].value, depth + 1);
    } else if(value.kind == VALUE_ARRAY && value.array != NULL &&
              !value.array->retired && !value.array->address_taken) {
        value.array->retired = 1;
        if(vm->retire_floor == 0 ||
           value.array->allocation < vm->retire_floor)
            vm->retire_floor = value.array->allocation;
        for(int i = 0; i < value.array->length; i++)
            retire_value(vm, value.array->elements[i], depth + 1);
    }
}

int
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

void
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

int
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
void
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
