#include "zir_vm_internal.h"
/* Buffers vm_print keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
struct VmPrintBuffers {
    Value values[PRINT_PIECES_MAX];
    unsigned char bytes[ZIR_TEXT_MAX];
};

static void vm_print(Frame *frame, const ZirExpr *expression, int depth);
static void free_scalar_plans(Vm *vm);

static void
vm_print_with_buffers(Frame *frame, const ZirExpr *expression, int depth, VmPrintBuffers *buffers)
{
    const ZirFunction *function = frame->function;
    PrintPiece *pieces = calloc(PRINT_PIECES_MAX, sizeof(*pieces));
    const char *types[PRINT_PIECES_MAX];
    int first = expression->first_child, count, argument = 0;
    if(pieces == NULL || first < 0 ||
       (count = PrintFormatPieces(function->exprs[first].text, pieces,
                                  PRINT_PIECES_MAX)) < 0) {
        free(pieces);
        frame->vm->failed = 1;
        return;
    }
    VmRoots roots = {frame->vm->evaluation_roots, buffers->values, 0};
    frame->vm->evaluation_roots = &roots;
    for(int child = function->exprs[first].next_sibling;
        child >= 0 && !frame->vm->failed && argument < PRINT_PIECES_MAX;
        child = function->exprs[child].next_sibling, argument++) {
        types[argument] = ScalarType(function->exprs[child].type);
        buffers->values[argument] = eval(frame, child, depth + 1);
        roots.count = argument + 1;
    }
    argument = 0;
    for(int i = 0; i < count && !frame->vm->failed; i++) {
        char text[64];
        size_t length;
        Value value;
        const char *type;
        if(!pieces[i].is_argument) {
            if(DecodeStringLiteral(pieces[i].literal, buffers->bytes, sizeof(buffers->bytes),
                                   &length))
                fwrite(buffers->bytes, 1, length, stdout);
            continue;
        }
        value = buffers->values[argument];
        type = types[argument++];
        if(!strcmp(type, "string")) {
            if(value.kind != VALUE_STRING) {
                frame->vm->failed = 1;
                break;
            }
            if(value.length > 0)
                fwrite(value.data, 1, value.length, stdout);
            continue;
        }
        if(!strcmp(type, "bool"))
            snprintf(text, sizeof(text), "%s", truthy(value) ? "true" : "false");
        else if(!strcmp(type, "float32") || !strcmp(type, "float64"))
            FormatPrintFloat(value.kind == VALUE_REAL ? value.real :
                             (double)value.integer,
                             !strcmp(type, "float32"), text, sizeof(text));
        else if(type[0] == 'u')
            snprintf(text, sizeof(text), "%llu",
                     (unsigned long long)integer_bits(value));
        else
            snprintf(text, sizeof(text), "%lld",
                     (long long)signed64(integer_bits(value)));
        fputs(text, stdout);
    }
    free(pieces);
    frame->vm->evaluation_roots = roots.previous;
}

/* Checked `print`: evaluate every argument left to right, then write literal
 * pieces and values to standard output with the native targets' spelling. */
static void
vm_print(Frame *frame, const ZirExpr *expression, int depth)
{
    Vm *vm = frame->vm;
    VmPrintBuffers *buffers = vm->print_buffer_count > 0 ? vm->print_buffers[--vm->print_buffer_count] :
        AllocateOrExit(sizeof(*buffers));
    vm_print_with_buffers(frame, expression, depth, buffers);
    if(vm->print_buffer_count < 16)
        vm->print_buffers[vm->print_buffer_count++] = buffers;
    else
        free(buffers);
}

void
free_evaluation_buffers(Vm *vm)
{
    free_scalar_plans(vm);
    while(vm->print_buffer_count > 0) {
        int slot = --vm->print_buffer_count;
        free(vm->print_buffers[slot]);
        vm->print_buffers[slot] = NULL;
    }
}

static Local *
find_local(Frame *frame, const char *name)
{
    /* Checked expressions, declarations and cached parameters share names. */
    for(int i = frame->local_count - 1; i >= 0; i--)
        if(frame->locals[i].name == name)
            return &frame->locals[i];
    return NULL;
}

static GlobalSlot *
find_global_slot(Frame *frame, const ZirExpr *expression)
{
    enum { SLOTS = 4096 };
    Vm *vm = frame->vm;
    size_t index = vm_expression_slot(expression, SLOTS / 2) * 2;
    if(vm->global_sites == NULL)
        vm->global_sites = calloc(SLOTS, sizeof(*vm->global_sites));
    if(vm->global_sites != NULL) {
        for(size_t way = 0; way < 2; way++) {
            VmGlobalSite cached = vm->global_sites[index + way];
            if(cached.expression != expression || cached.module != frame->module)
                continue;
            if(way != 0) {
                vm->global_sites[index + 1] = vm->global_sites[index];
                vm->global_sites[index] = cached;
            }
            if(vm->profile != NULL)
                profile_cache_access(vm, VM_CACHE_GLOBAL, VM_CACHE_HIT);
            return cached.slot;
        }
    }
    if(vm->profile != NULL)
        profile_cache_access(vm, VM_CACHE_GLOBAL,
            vm->global_sites != NULL && vm->global_sites[index + 1].expression != NULL ?
            VM_CACHE_COLLISION : VM_CACHE_MISS);
    const ZirModule *owner = NULL;
    const ZirGlobal *declaration = NULL;
    GlobalSlot *result = NULL;
    if(ResolveGlobalAt(frame->module, expression->name, SpanPath(frame->function->span),
                       &owner, &declaration) == 1) {
        for(int i = 0; i < frame->vm->global_count; i++) {
            GlobalSlot *slot = &frame->vm->globals[i];
            if(slot->module == owner && slot->declaration == declaration) {
                result = slot;
                break;
            }
        }
    }
    /* Resolution, including an absent global, cannot change in an instance.
     * Retain locations and metadata only; mutable contents are always read
     * from their original slot. Two ways protect reused colliding sites. */
    if(vm->global_sites != NULL) {
        vm->global_sites[index + 1] = vm->global_sites[index];
        vm->global_sites[index] = (VmGlobalSite){expression, frame->module, result};
    }
    return result;
}

static Value *
find_global_value(Frame *frame, const ZirExpr *expression)
{
    GlobalSlot *slot = find_global_slot(frame, expression);
    return slot != NULL ? &slot->value : NULL;
}

/* These scalar storage reads cannot change kind or numeric width. Default
 * u64 and opaque pointer fields may still hold a signed zero, so those
 * values retain coercion until their storage carries unsigned bits. */
static int
normalized_scalar_read(Value value, const char *stored_type, const char *type)
{
    if(stored_type != type)
        return 0;
    if(value.kind == VALUE_POINTER)
        return type[0] == '*';
    if(value.kind == VALUE_STRING || value.kind == VALUE_REAL)
        return value_kind(type) == value.kind;
    if(value.kind != VALUE_INT)
        return 0;
    if(value_kind(type) != VALUE_INT)
        return 0;
    if(type[0] == '*' ||
       (type[0] == 'u' && type[1] == '6' && type[2] == '4' && type[3] == '\0'))
        return value.unsigned64;
    return 1;
}

static int
normalized_field_read(const Value *field, const char *type)
{
    const RecordField *entry = (const RecordField *)
        ((const unsigned char *)field - offsetof(RecordField, value));
    return normalized_scalar_read(*field, entry->field.type, type);
}

/* A direct call's import resolution cannot change within an instance.
 * Procedure values still resolve their current binding on every call. */
static VmCallSite
resolve_call(Frame *frame, const ZirExpr *expression)
{
    enum { SLOTS = 4096 };
    Vm *vm = frame->vm;
    size_t slot = vm_expression_slot(expression, SLOTS / 2) * 2;
    if(vm->call_sites == NULL)
        vm->call_sites = calloc(SLOTS, sizeof(*vm->call_sites));
    if(vm->call_sites != NULL) {
        for(size_t way = 0; way < 2; way++) {
            VmCallSite cached = vm->call_sites[slot + way];
            if(cached.expression != expression || cached.module != frame->module)
                continue;
            if(way != 0) {
                vm->call_sites[slot + 1] = vm->call_sites[slot];
                vm->call_sites[slot] = cached;
            }
            if(vm->profile != NULL)
                profile_cache_access(vm, VM_CACHE_CALL, VM_CACHE_HIT);
            return cached;
        }
    }
    if(vm->profile != NULL)
        profile_cache_access(vm, VM_CACHE_CALL,
            vm->call_sites != NULL && vm->call_sites[slot + 1].expression != NULL ?
            VM_CACHE_COLLISION : VM_CACHE_MISS);
    VmCallSite result = {.expression = expression, .module = frame->module};
    if(ResolveFunction(frame->module, expression->name,
                       &result.owner, &result.callee) == 0)
        result.external = host_import(frame->module, expression->name);
    if(vm->call_sites != NULL) {
        vm->call_sites[slot + 1] = vm->call_sites[slot];
        vm->call_sites[slot] = result;
    }
    return result;
}

Value *
record_field(Record *record, const char *name)
{
    if(record == NULL)
        return NULL;
    size_t position = vm_field_hash(name) & (record->field_slot_count - 1);
    while(record->field_slots[position] >= 0) {
        int index = record->field_slots[position];
        if(record->fields[index].field.name == name ||
           strcmp(record->fields[index].field.name, name) == 0)
            return &record->fields[index].value;
        position = (position + 1) & (record->field_slot_count - 1);
    }
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
Value
union_member_read(Vm *vm, Record *record, const char *field_type)
{
    const ZirType *enumeration = vm_find_type(vm, record->owner, field_type, NULL);
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

int
union_member_write(Vm *vm, Record *record, const char *field_type, Value value)
{
    const ZirType *enumeration = vm_find_type(vm, record->owner, field_type, NULL);
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

static Value
indexed_element(Vm *vm, Value base, uint64_t index)
{
    Array *array = NULL;
    size_t offset = 0;
    if(base.kind == VALUE_RECORD && base.record != NULL &&
       vm_vec_element_type(vm, base.record->owner, base.record->type->name, NULL, 0)) {
        Value *data = record_field(base.record, "data");
        Value *count = record_field(base.record, "count");
        if(data == NULL || count == NULL || count->kind != VALUE_INT ||
           count->integer < 0 || index >= (uint64_t)count->integer ||
           data->kind != VALUE_ARRAY || data->array == NULL)
            return (Value){0};
        array = data->array;
    } else if(base.kind == VALUE_ARRAY) {
        array = base.array;
    } else if(base.kind == VALUE_SLICE && base.array != NULL &&
              base.offset <= (size_t)base.array->length &&
              base.length <= (size_t)base.array->length - base.offset &&
              index < base.length) {
        array = base.array;
        offset = base.offset;
    } else {
        return (Value){0};
    }
    if(array == NULL || index >= (uint64_t)array->length - offset)
        return (Value){0};
    return (Value){.kind = VALUE_POINTER, .indexed = 1, .array = array,
                   .offset = offset + (size_t)index};
}

int
pointer_target(Vm *vm, Value pointer)
{
    if(!place_valid(pointer) || (pointer.array != NULL && pointer.array->freed)) {
        vm->failed = 1;
        return 0;
    }
    if(pointer.bits != 0) {
        for(Frame *frame = vm->active_frame; frame != NULL; frame = frame->caller)
            if(frame->serial == pointer.bits)
                return 1;
        vm->failed = 1;
        return 0;
    }
    return 1;
}

Value
place_read(Vm *vm, Value place)
{
    if(!pointer_target(vm, place))
        return (Value){0};
    return place.indexed ? array_get(place.array, place.offset) : *place.pointee;
}

void
place_write(Vm *vm, Value place, Value value)
{
    if(!pointer_target(vm, place))
        return;
    if(place.indexed)
        array_set(place.array, place.offset, value);
    else
        *place.pointee = value;
}

/* A location names a stable slot or an array and logical index. Taking a
 * numeric element's address never expands the complete array into Values. */
static Value
owned_place(Frame *frame, int index, int depth)
{
    if(index < 0 || index >= frame->function->expr_count || depth >= VM_MAX_DEPTH)
        return (Value){0};
    const ZirExpr *expression = &frame->function->exprs[index];
    if(expression->kind == ZIR_EXPR_IDENT) {
        Local *local = find_local(frame, expression->name);
        return (Value){.kind = VALUE_POINTER,
            .pointee = local != NULL ? &local->value : find_global_value(frame, expression),
            .bits = local != NULL ? frame->serial : 0};
    }
    if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "*")) {
        Value pointer = eval(frame, expression->right, depth + 1);
        return pointer_target(frame->vm, pointer) ? pointer : (Value){0};
    }
    if(expression->kind == ZIR_EXPR_POINTER_MEMBER) {
        Value pointer = eval(frame, expression->left, depth + 1);
        Value target = place_read(frame->vm, pointer);
        if(target.kind != VALUE_RECORD || target.record == NULL) {
            frame->vm->failed = 1;
            return (Value){0};
        }
        return (Value){.kind = VALUE_POINTER, .record = target.record,
                      .pointee = record_field_path(target.record, expression->name)};
    }
    if(expression->kind == ZIR_EXPR_INDEX) {
        Value base = owned_place(frame, expression->left, depth + 1);
        Value held[2] = {base, place_valid(base) ? place_read(frame->vm, base) : (Value){0}};
        VmRoots roots = {frame->vm->evaluation_roots, held, 2};
        frame->vm->evaluation_roots = &roots;
        Value index_value = eval(frame, expression->right, depth + 1);
        frame->vm->evaluation_roots = roots.previous;
        if(!place_valid(base) || index_value.kind != VALUE_INT ||
           (!index_value.unsigned64 && index_value.integer < 0)) {
            frame->vm->failed = 1;
            return (Value){0};
        }
        Value element = indexed_element(frame->vm, place_read(frame->vm, base), integer_bits(index_value));
        if(!place_valid(element))
            frame->vm->failed = 1;
        return element;
    }
    if(expression->kind != ZIR_EXPR_MEMBER)
        return (Value){0};
    Value base_place = owned_place(frame, expression->left, depth + 1);
    if(!place_valid(base_place))
        return (Value){0};
    Value base = place_read(frame->vm, base_place);
    if(base.kind != VALUE_RECORD || base.record == NULL)
        return (Value){0};
    if(base.record->type != NULL && base.record->type->is_union) {
        frame->union_write_record = base.record;
        frame->union_write_type = expression->type;
        return (Value){.kind = VALUE_POINTER, .record = base.record,
                      .pointee = &base.record->fields[0].value};
    }
    return (Value){.kind = VALUE_POINTER, .record = base.record,
                  .pointee = record_field_path(base.record, expression->name)};
}

Value *
assignment_slot(Frame *frame, int index, int depth)
{
    return place_slot(owned_place(frame, index, depth));
}

Value
assignment_slot_root(Frame *frame, int index, int depth)
{
    return owned_place(frame, index, depth);
}

/* Decode an exact, checked operator spelling once per calculation. The text
 * stays the serialized representation; no program cache or extra allocation
 * is needed. Unknown and extended spellings must not match valid prefixes. */
typedef enum BinaryOperation {
    BinaryInvalid = 0,
    BinaryAdd = '+', BinarySubtract = '-', BinaryMultiply = '*',
    BinaryDivide = '/', BinaryRemainder = '%',
    BinaryAnd = '&', BinaryOr = '|', BinaryXor = '^',
    BinaryLess = '<', BinaryGreater = '>',
    BinaryEqual = '=' | ('=' << 8), BinaryNotEqual = '!' | ('=' << 8),
    BinaryLessEqual = '<' | ('=' << 8), BinaryGreaterEqual = '>' | ('=' << 8),
    BinaryShiftLeft = '<' | ('<' << 8), BinaryShiftRight = '>' | ('>' << 8)
} BinaryOperation;

static BinaryOperation
binary_operation(const char *op)
{
    unsigned code = (unsigned char)op[0];
    if(code == 0)
        return BinaryInvalid;
    if(op[1] != '\0') {
        if(op[2] != '\0')
            return BinaryInvalid;
        code |= (unsigned)(unsigned char)op[1] << 8;
    }
    switch(code) {
    case BinaryAdd: case BinarySubtract: case BinaryMultiply:
    case BinaryDivide: case BinaryRemainder:
    case BinaryAnd: case BinaryOr: case BinaryXor:
    case BinaryLess: case BinaryGreater:
    case BinaryEqual: case BinaryNotEqual: case BinaryLessEqual:
    case BinaryGreaterEqual: case BinaryShiftLeft: case BinaryShiftRight:
        return (BinaryOperation)code;
    default:
        return BinaryInvalid;
    }
}

/* Width policy for checked integer operations. A negative width denotes an
 * unsigned 64-bit value or opaque host handle. Named types and abstract
 * integers retain the existing 32-bit operation policy. Match the complete
 * spelling so names such as s8Extra never acquire a builtin width. */
static int
integer_operation_width(const char *type)
{
    if(type[0] == '*' || (type[0] == 'n' && !strcmp(type, "null")))
        return -64;
    if(type[0] != 's' && type[0] != 'u')
        return 32;
    if(type[1] == '6' && type[2] == '4' && type[3] == '\0')
        return type[0] == 'u' ? -64 : 64;
    if(type[1] == '8' && type[2] == '\0')
        return 8;
    if(type[1] == '1' && type[2] == '6' && type[3] == '\0')
        return 16;
    return 32;
}

/* Plan registers need only the live scalar words. Container references are
 * inactive for integers/reals and are never inspected or rooted for them. */
static Value *
int_result(Value *output, int64_t integer)
{
    output->kind = VALUE_INT;
    output->unsigned64 = 0;
    output->integer = integer;
    output->bits = (uint64_t)integer;
    return output;
}

static Value *
uint_result(Value *output, uint64_t bits)
{
    output->kind = VALUE_INT;
    output->unsigned64 = 1;
    output->integer = 0;
    output->bits = bits;
    return output;
}

static Value *
real_result(Value *output, double real)
{
    output->kind = VALUE_REAL;
    output->unsigned64 = 0;
    output->real = real;
    return output;
}

/* Shared numeric rules for the canonical evaluator and scalar plans. Write
 * to the caller's slot, without returning a whole aggregate through the ABI. */
static Value *
binary_numeric(Vm *vm, BinaryOperation operation, const Value *left_value,
               const Value *right_value, int left_width, int right_width,
               int left_unsigned, Value *output)
{
    int real = left_value->kind == VALUE_REAL || right_value->kind == VALUE_REAL;
    if(vm->failed || operation == BinaryInvalid)
        goto failed;
    /* Integer arithmetic never needs floating conversion. Keep conversion
     * and real comparisons together so integer loops do not construct two
     * doubles for every arithmetic, bitwise or comparison operation. */
    if(real) {
        double a = as_real(*left_value), b = as_real(*right_value);
        switch(operation) {
        case BinaryEqual: return int_result(output, a == b);
        case BinaryNotEqual: return int_result(output, a != b);
        case BinaryLess: return int_result(output, a < b);
        case BinaryLessEqual: return int_result(output, a <= b);
        case BinaryGreater: return int_result(output, a > b);
        case BinaryGreaterEqual: return int_result(output, a >= b);
        case BinaryAdd: return real_result(output, a + b);
        case BinarySubtract: return real_result(output, a - b);
        case BinaryMultiply: return real_result(output, a * b);
        case BinaryDivide: return real_result(output, a / b);
        default: goto failed;
        }
    }
    /* Shift width comes only from the left operand; other numeric operations
     * use either operand's wide representation. */
    int unsigned64 = left_width == -64 || right_width == -64;
    int signed_wide = !unsigned64 && (left_width == 64 || right_width == 64);
    uint64_t left_bits = integer_bits(*left_value);
    uint64_t right_bits = integer_bits(*right_value);
    if(operation == BinaryEqual)
        return int_result(output, unsigned64 ? left_bits == right_bits :
                         left_value->integer == right_value->integer);
    if(operation == BinaryNotEqual)
        return int_result(output, unsigned64 ? left_bits != right_bits :
                         left_value->integer != right_value->integer);
    if(operation == BinaryLess)
        return int_result(output, unsigned64 ? left_bits < right_bits :
                         left_value->integer < right_value->integer);
    if(operation == BinaryLessEqual)
        return int_result(output, unsigned64 ? left_bits <= right_bits :
                         left_value->integer <= right_value->integer);
    if(operation == BinaryGreater)
        return int_result(output, unsigned64 ? left_bits > right_bits :
                         left_value->integer > right_value->integer);
    if(operation == BinaryGreaterEqual)
        return int_result(output, unsigned64 ? left_bits >= right_bits :
                         left_value->integer >= right_value->integer);
    if(operation == BinaryAnd || operation == BinaryOr || operation == BinaryXor ||
       operation == BinaryShiftLeft || operation == BinaryShiftRight) {
        if(unsigned64 || signed_wide) {
            if(operation == BinaryAnd)
                return unsigned64 ? uint_result(output, left_bits & right_bits) :
                                    int_result(output, signed64(left_bits & right_bits));
            if(operation == BinaryOr)
                return unsigned64 ? uint_result(output, left_bits | right_bits) :
                                    int_result(output, signed64(left_bits | right_bits));
            if(operation == BinaryXor)
                return unsigned64 ? uint_result(output, left_bits ^ right_bits) :
                                    int_result(output, signed64(left_bits ^ right_bits));
            if((!right_value->unsigned64 && right_value->integer < 0) || right_bits >= 64)
                goto failed;
            if(operation == BinaryShiftLeft)
                return unsigned64 ? uint_result(output, left_bits << right_bits) :
                                    int_result(output, signed64(left_bits << right_bits));
            if(unsigned64)
                return uint_result(output, left_bits >> right_bits);
            uint64_t shifted = left_bits >> right_bits;
            if((left_bits & UINT64_C(0x8000000000000000)) != 0 &&
               right_bits > 0)
                shifted |= UINT64_MAX << (64 - right_bits);
            return int_result(output, signed64(shifted));
        }
        uint32_t a_bits = (uint32_t)left_bits;
        uint32_t b_bits = (uint32_t)right_bits;
        if(operation == BinaryAnd)
            return int_result(output, a_bits & b_bits);
        if(operation == BinaryOr)
            return int_result(output, a_bits | b_bits);
        if(operation == BinaryXor)
            return int_result(output, a_bits ^ b_bits);
        unsigned width = (unsigned)left_width;
        if((!right_value->unsigned64 && right_value->integer < 0) || right_bits >= width)
            goto failed;
        unsigned amount = (unsigned)right_bits;
        uint32_t mask = width == 32 ? UINT32_MAX :
                        (UINT32_C(1) << width) - 1;
        a_bits &= mask;
        if(operation == BinaryShiftLeft)
            return int_result(output, (a_bits << amount) & mask);
        uint32_t shifted = a_bits >> amount;
        if(!left_unsigned &&
           (a_bits & (UINT32_C(1) << (width - 1))) != 0 && amount > 0)
            shifted |= mask ^ (mask >> amount);
        return int_result(output, shifted & mask);
    }
    if(unsigned64) {
        if(operation == BinaryAdd) return uint_result(output, left_bits + right_bits);
        if(operation == BinarySubtract) return uint_result(output, left_bits - right_bits);
        if(operation == BinaryMultiply) return uint_result(output, left_bits * right_bits);
        if(operation == BinaryDivide && right_bits != 0)
            return uint_result(output, left_bits / right_bits);
        if(operation == BinaryRemainder && right_bits != 0)
            return uint_result(output, left_bits % right_bits);
        goto failed;
    }
    if(signed_wide) {
        if(operation == BinaryAdd)
            return int_result(output, signed64(left_bits + right_bits));
        if(operation == BinarySubtract)
            return int_result(output, signed64(left_bits - right_bits));
        if(operation == BinaryMultiply)
            return int_result(output, signed64(left_bits * right_bits));
        if(right_value->integer == 0)
            goto failed;
        if(left_value->integer == INT64_MIN && right_value->integer == -1)
            return int_result(output, operation == BinaryDivide ? INT64_MIN : 0);
        if(operation == BinaryDivide)
            return int_result(output, left_value->integer / right_value->integer);
        if(operation == BinaryRemainder)
            return int_result(output, left_value->integer % right_value->integer);
        goto failed;
    }
    /* A u32 product can exceed INT64_MAX before its result is narrowed.
     * Use defined unsigned intermediates for wrapping integer arithmetic,
     * including abstract integer operands on this operation-width path. */
    if(operation == BinaryAdd)
        return int_result(output, signed64((uint64_t)left_value->integer + (uint64_t)right_value->integer));
    if(operation == BinarySubtract)
        return int_result(output, signed64((uint64_t)left_value->integer - (uint64_t)right_value->integer));
    if(operation == BinaryMultiply)
        return int_result(output, signed64((uint64_t)left_value->integer * (uint64_t)right_value->integer));
    if(right_value->integer == 0)
        goto failed;
    if(left_value->integer == INT64_MIN && right_value->integer == -1)
        return int_result(output, operation == BinaryDivide ? INT64_MIN : 0);
    if(operation == BinaryDivide)
        return int_result(output, left_value->integer / right_value->integer);
    if(operation == BinaryRemainder)
        return int_result(output, left_value->integer % right_value->integer);
failed:
    vm->failed = 1;
    return int_result(output, 0);
}

/* Plan operands already live in stable slots for this calculation. Borrow
 * them so the C ABI need not copy two whole tagged Values into arguments. */
static Value
binary_with_policy(Vm *vm, BinaryOperation operation, const Value *left_value,
                   const Value *right_value,
                   int left_width, int right_width, int left_unsigned)
{
    int shift = operation == BinaryShiftLeft || operation == BinaryShiftRight;
    if(vm->failed || operation == BinaryInvalid ||
       left_value->kind == VALUE_VOID || right_value->kind == VALUE_VOID)
        goto failed;
    if(left_value->kind == VALUE_SLOT || right_value->kind == VALUE_SLOT) {
        if((left_value->kind != VALUE_SLOT &&
            (left_value->kind != VALUE_INT || integer_bits(*left_value) != 0)) ||
           (right_value->kind != VALUE_SLOT &&
            (right_value->kind != VALUE_INT || integer_bits(*right_value) != 0)))
            goto failed;
        const ZirFunction *a_function = left_value->kind == VALUE_SLOT ? left_value->slot_function : NULL;
        const ZirFunction *b_function = right_value->kind == VALUE_SLOT ? right_value->slot_function : NULL;
        int equal = a_function == b_function &&
            (a_function == NULL || left_value->slot_module == right_value->slot_module);
        if(operation == BinaryEqual) return int_value(equal);
        if(operation == BinaryNotEqual) return int_value(!equal);
        goto failed;
    }
    if(left_value->kind == VALUE_POINTER || right_value->kind == VALUE_POINTER) {
        /* Indexed pointers compare by container and logical slot. */
        int equal = left_value->kind == VALUE_POINTER && right_value->kind == VALUE_POINTER ?
            (left_value->indexed == right_value->indexed &&
             (left_value->indexed ? left_value->array == right_value->array && left_value->offset == right_value->offset :
              left_value->pointee == right_value->pointee)) : 0;
        if(operation == BinaryEqual)
            return int_value(equal);
        if(operation == BinaryNotEqual)
            return int_value(!equal);
        goto failed;
    }
    if(left_value->kind == VALUE_STRING || right_value->kind == VALUE_STRING) {
        if(left_value->kind != VALUE_STRING || right_value->kind != VALUE_STRING)
            goto failed;
        int equal = left_value->length == right_value->length &&
                    (left_value->length == 0 ||
                     memcmp(left_value->data, right_value->data, left_value->length) == 0);
        if(operation == BinaryEqual)
            return int_value(equal);
        if(operation == BinaryNotEqual)
            return int_value(!equal);
        goto failed;
    }
    if((left_value->kind == VALUE_ENUM && left_value->enumeration->is_enum_flags) ||
       (right_value->kind == VALUE_ENUM && right_value->enumeration->is_enum_flags)) {
        const ZirType *flags = left_value->kind == VALUE_ENUM ? left_value->enumeration :
                               right_value->enumeration;
        if((left_value->kind == VALUE_ENUM && left_value->enumeration != flags) ||
           (right_value->kind == VALUE_ENUM && right_value->enumeration != flags) ||
           (left_value->kind != VALUE_ENUM && left_value->kind != VALUE_INT) ||
           (right_value->kind != VALUE_ENUM && right_value->kind != VALUE_INT))
            goto failed;
        int width = integer_operation_width(flags->enum_backing);
        Value a = int_value(left_value->integer), b = int_value(right_value->integer);
        Value result = binary_with_policy(vm, operation, &a, &b, width,
                                          shift ? 32 : width,
                                          flags->enum_backing[0] == 'u');
        if(vm->failed || operation == BinaryEqual || operation == BinaryNotEqual)
            return result;
        result = coerce(vm, NULL, result, flags->enum_backing);
        return enum_value(flags, signed64(integer_bits(result)));
    }
    if(left_value->kind == VALUE_ENUM || right_value->kind == VALUE_ENUM) {
        if(left_value->kind != VALUE_ENUM || right_value->kind != VALUE_ENUM ||
           left_value->enumeration != right_value->enumeration)
            goto failed;
        if(operation == BinaryEqual)
            return int_value(left_value->integer == right_value->integer);
        if(operation == BinaryNotEqual)
            return int_value(left_value->integer != right_value->integer);
        goto failed;
    }
    Value result = {0};
    binary_numeric(vm, operation, left_value, right_value,
                   left_width, right_width, left_unsigned, &result);
    return result;
failed:
    vm->failed = 1;
    return int_value(0);
}

Value
binary_value(Vm *vm, const char *op, Value left, Value right,
             const char *left_type, const char *right_type)
{
    BinaryOperation operation = binary_operation(op);
    int left_width = 32, right_width = 32, left_unsigned = 0;
    if(!vm->failed && operation != BinaryInvalid &&
       (left.kind == VALUE_INT || left.kind == VALUE_REAL) &&
       (right.kind == VALUE_INT || right.kind == VALUE_REAL)) {
        left_width = integer_operation_width(left_type);
        if(operation != BinaryShiftLeft && operation != BinaryShiftRight)
            right_width = integer_operation_width(right_type);
        left_unsigned = left_type[0] == 'u';
        Value result = {0};
        binary_numeric(vm, operation, &left, &right,
                       left_width, right_width, left_unsigned, &result);
        return result;
    }
    return binary_with_policy(vm, operation, &left, &right,
                              left_width, right_width, left_unsigned);
}
/* Buffers eval keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */

Value eval(Frame *frame, int index, int depth);

static int
vm_expression_calls(const ZirFunction *function, int index, int depth)
{
    if(index < 0)
        return 0;
    if(index >= function->expr_count || depth >= VM_MAX_DEPTH)
        return 1;
    const ZirExpr *expression = &function->exprs[index];
    if(expression->kind == ZIR_EXPR_CALL)
        return 1;
    for(int child = expression->first_child; child >= 0;
        child = function->exprs[child].next_sibling)
        if(vm_expression_calls(function, child, depth + 1))
            return 1;
    return vm_expression_calls(function, expression->left, depth + 1) ||
           vm_expression_calls(function, expression->right, depth + 1) ||
           vm_expression_calls(function, expression->third, depth + 1);
}

typedef enum Builtin {
    BuiltinNamed, BuiltinTextView, BuiltinPrint, BuiltinNew, BuiltinFree,
    BuiltinVecPush, BuiltinVecClear, BuiltinVecFree, BuiltinVecSwap,
    BuiltinVecPop, BuiltinVecGet, BuiltinVecClone, BuiltinVecSlice,
    BuiltinBuilderAppend, BuiltinBuilderFinish
} Builtin;

/* Most calls name ordinary functions. Exclude them by their first byte
 * before comparing exact builtin spellings; prefixes remain ordinary names. */
static Builtin
builtin_call(const char *name)
{
    switch(name[0]) {
    case 'T':
        if(!strcmp(name, "TextView")) return BuiltinTextView;
        break;
    case 'p':
        if(!strcmp(name, "print")) return BuiltinPrint;
        break;
    case 'z':
        if(!strcmp(name, "zi_new")) return BuiltinNew;
        if(!strcmp(name, "zi_free")) return BuiltinFree;
        break;
    case 'B':
        if(!strcmp(name, "BuilderAppend")) return BuiltinBuilderAppend;
        if(!strcmp(name, "BuilderFinish")) return BuiltinBuilderFinish;
        break;
    case 'V':
        if(strncmp(name, "Vec", 3)) break;
        name += 3;
        switch(name[0]) {
        case 'P':
            if(!strcmp(name, "Push")) return BuiltinVecPush;
            if(!strcmp(name, "Pop")) return BuiltinVecPop;
            break;
        case 'C':
            if(!strcmp(name, "Clear")) return BuiltinVecClear;
            if(!strcmp(name, "Clone")) return BuiltinVecClone;
            break;
        case 'F':
            if(!strcmp(name, "Free")) return BuiltinVecFree;
            break;
        case 'G':
            if(!strcmp(name, "Get")) return BuiltinVecGet;
            break;
        case 'S':
            if(!strcmp(name, "Swap")) return BuiltinVecSwap;
            if(!strcmp(name, "Slice")) return BuiltinVecSlice;
            break;
        }
        break;
    }
    return BuiltinNamed;
}

/* Cache entries may be replaced by another literal. Borrow only until the
 * caller snapshots the live numeric words or returns its Value copy. */
static const Value *
numeric_literal(Frame *frame, const ZirExpr *expression, Value *output)
{
    Value value = int_value(0);
    /* Checked numeric literals are immutable. Parse and coerce once per
     * instance; the cache contains only scalars and enum type pointers. */
    enum { SLOTS = 4096 };
    uintptr_t hash = (uintptr_t)expression;
    hash ^= hash >> 17;
    hash ^= hash >> 9;
    size_t slot = (hash >> 4) & (SLOTS - 1);
    Vm *vm = frame->vm;
    if(vm->constant_sites == NULL)
        vm->constant_sites = calloc(SLOTS, sizeof(*vm->constant_sites));
    if(vm->constant_sites != NULL &&
       vm->constant_sites[slot].expression == expression &&
       vm->constant_sites[slot].module == frame->module)
        return &vm->constant_sites[slot].value;
    char *end;
    errno = 0;
    if(expression->kind == ZIR_EXPR_FLOAT)
        value = real_value(strtod(expression->text, &end));
    else if(expression->text[0] == '-')
        value = int_value(strtoll(expression->text, &end, 0));
    else {
        unsigned long long bits = strtoull(expression->text, &end, 0);
        value = bits > INT64_MAX ? uint_value(bits) : int_value((int64_t)bits);
    }
    if(errno || end == expression->text || *end ||
       (value.kind == VALUE_REAL && !isfinite(value.real))) {
        vm->failed = 1;
        *output = coerce_expression(vm, frame->module, value, expression->type);
        return output;
    }
    value = coerce_expression(vm, frame->module, value, expression->type);
    *output = value;
    if(!vm->failed && vm->constant_sites != NULL)
        vm->constant_sites[slot] = (VmConstantSite){expression, frame->module, value};
    return output;
}

/* Leaves can allocate literal or coercion storage, but cannot execute a
 * function or trigger VM collection. Their caller stores the returned value
 * before evaluating another operand. Keep the full temporary root chain for
 * compound expressions and calls, without constructing it for these reads. */
static Value
eval_leaf(Frame *frame, const ZirExpr *expression)
{
    Value value = int_value(0);
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
    case ZIR_EXPR_INT:
    case ZIR_EXPR_FLOAT: {
        Value literal;
        return *numeric_literal(frame, expression, &literal);
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
            const ZirType *slot = vm_find_type(frame->vm, frame->module,
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
        /* Most bindings cannot be a keyword. Preserve exact matching for
         * identifiers sharing a keyword's initial byte. */
        switch(expression->name[0]) {
        case 't':
            if(strcmp(expression->name, "true") == 0)
                return int_value(1);
            break;
        case 'f':
            if(strcmp(expression->name, "false") == 0)
                return int_value(0);
            break;
        case 'n':
            if(strcmp(expression->name, "null") == 0)
                return uint_value(0);
            break;
        }
        Local *local = find_local(frame, expression->name);
        if(local != NULL) {
            Value stored = local->value;
            if(expression->is_move) {
                if(stored.kind != VALUE_RECORD ||
                   !vm_type_contains_vec(frame->vm, frame->module,
                                         expression->type, 0)) {
                    frame->vm->failed = 1;
                    break;
                }
                /* A move hands the storage to its destination; the source
                 * binding no longer owns it, so scope exit must not release
                 * it again. A destination that cloned leaves the original
                 * with no owner, so it is retired here. */
                local->value = (Value){.kind = VALUE_INVALID};
                Value moved = coerce_expression(frame->vm, frame->module,
                                                stored, expression->type);
                if(moved.kind != VALUE_RECORD || moved.record != stored.record)
                    retire_value(frame->vm, stored, 0);
                return moved;
            }
            /* Declarations, assignments and call parameters already coerce
             * storage. A read with the identical interned type needs no
             * repeated parsing or conversion. Keep invalid moved storage
             * and differently spelled types on the checked path. */
            if(stored.kind != VALUE_INVALID && local->type == expression->type)
                return stored;
            return coerce_expression(frame->vm, frame->module,
                                     stored, expression->type);
        }
        GlobalSlot *global = find_global_slot(frame, expression);
        if(global != NULL) {
            /* Same-module aggregate storage is already typed. Imported
             * spellings still resolve in the caller's scope below. */
            if(global->module == frame->module && global->type == expression->type &&
               (global->value.kind == VALUE_ARRAY || global->value.kind == VALUE_RECORD))
                return global->value;
            return coerce_expression(frame->vm, frame->module,
                                     global->value, expression->type);
        }
        frame->vm->failed = 1;
        break;
    }
    default:
        frame->vm->failed = 1;
        break;
    }
    return coerce_expression(frame->vm, frame->module, value, expression->type);
}

/* Linear plans cover expressions without calls, moves or address-taking.
 * They keep evaluation order and each node's coercion, but avoid recursively
 * entering the evaluator and constructing temporary root chains per node.
 * No plan instruction can collect storage or invoke another plan. Plans hold
 * immutable IR only; every binding, field and index is read at execution. */
enum { SCALAR_PLAN_SLOTS = 16384, SCALAR_PLAN_STEPS = 32 };

typedef enum ScalarOp {
    SCALAR_LEAF, SCALAR_MEMBER, SCALAR_POINTER_MEMBER, SCALAR_INDEX,
    SCALAR_UNARY, SCALAR_BINARY, SCALAR_CAST, SCALAR_AND, SCALAR_OR,
    SCALAR_FALSE, SCALAR_COPY, SCALAR_JUMP, SCALAR_TRUTH
} ScalarOp;

typedef enum ScalarConversion {
    ScalarGeneric, ScalarInteger, ScalarBool,
    ScalarS8, ScalarU8, ScalarS16, ScalarU16,
    ScalarS32, ScalarU32, ScalarS64, ScalarU64,
    ScalarF32, ScalarF64
} ScalarConversion;

static ScalarConversion
scalar_conversion(const char *type)
{
    ValueKind kind = value_kind(type);
    if(kind == VALUE_REAL)
        return type[0] == 'f' && type[5] == '3' ? ScalarF32 : ScalarF64;
    if(kind != VALUE_INT)
        return ScalarGeneric;
    if(type[0] == 'b') return ScalarBool;
    if(type[0] == 'i') return ScalarInteger;
    if(type[0] != 's' && type[0] != 'u') return ScalarGeneric;
    int unsigned_type = type[0] == 'u';
    switch(type[1]) {
    case '8': return unsigned_type ? ScalarU8 : ScalarS8;
    case '1': return unsigned_type ? ScalarU16 : ScalarS16;
    case '6': return unsigned_type ? ScalarU64 : ScalarS64;
    default: return unsigned_type ? ScalarU32 : ScalarS32;
    }
}

/* The plan stores only checked primitive type facts. Real-to-integer range
 * checks, aggregate borrows, enum validation and pointers keep the canonical
 * conversion path; their current value is never cached in a plan. Borrow the
 * input slot rather than passing the whole tagged Value through the ABI. */
static const Value *
scalar_integer_result(const Value *input, Value *output, int64_t integer)
{
    if(input->kind == VALUE_INT && !input->unsigned64 &&
       input->integer == integer && input->bits == (uint64_t)integer)
        return input;
    return int_result(output, integer);
}

static const Value *
scalar_coerce(Frame *frame, const Value *input, const char *type,
              ScalarConversion conversion, Value *output)
{
    if(input->kind == VALUE_INT || input->kind == VALUE_REAL || input->kind == VALUE_ENUM) {
        if(conversion == ScalarInteger) return input;
        if(conversion == ScalarBool)
            return scalar_integer_result(input, output, truthy(*input));
        if(conversion == ScalarF32) return real_result(output, (float)as_real(*input));
        if(conversion == ScalarF64)
            return input->kind == VALUE_REAL ? input : real_result(output, as_real(*input));
        if(input->kind != VALUE_REAL) {
            uint64_t bits = integer_bits(*input);
            switch(conversion) {
            case ScalarS8: return scalar_integer_result(input, output, (int8_t)(uint8_t)bits);
            case ScalarU8: return scalar_integer_result(input, output, (uint8_t)bits);
            case ScalarS16: return scalar_integer_result(input, output, (int16_t)(uint16_t)bits);
            case ScalarU16: return scalar_integer_result(input, output, (uint16_t)bits);
            case ScalarS32: return scalar_integer_result(input, output, (uint32_t)bits <= INT32_MAX ?
                (int64_t)(uint32_t)bits : (int64_t)(uint32_t)bits - INT64_C(4294967296));
            case ScalarU32: return scalar_integer_result(input, output, (uint32_t)bits);
            case ScalarS64: return scalar_integer_result(input, output, signed64(bits));
            case ScalarU64:
                return input->kind == VALUE_INT && input->unsigned64 && input->integer == 0 ?
                    input : uint_result(output, bits);
            default: break;
            }
        }
    }
    *output = coerce_expression(frame->vm, frame->module, *input, type);
    return output;
}

static Value
scalar_result(Frame *frame, const Value *input, const char *type,
              ScalarConversion conversion)
{
    Value output = {0};
    return *scalar_coerce(frame, input, type, conversion, &output);
}

typedef struct ScalarStep {
    const ZirExpr *expression;
    unsigned short operation;
    unsigned char op, left, right, output, target, depth;
    signed char left_width, right_width;
    unsigned char left_unsigned, conversion, cast_conversion;
} ScalarStep;

typedef struct ScalarPlan {
    unsigned char count, output;
    ScalarStep steps[];
} ScalarPlan;

struct VmScalarSite {
    const ZirExpr *expression;
    const ZirModule *module;
    ScalarPlan *plan;
};

typedef struct ScalarBuilder {
    const ZirFunction *function;
    int count;
    ScalarStep steps[SCALAR_PLAN_STEPS];
} ScalarBuilder;

static int
scalar_step(ScalarBuilder *builder, ScalarOp op, const ZirExpr *expression,
            int left, int right, int depth)
{
    if(builder->count >= SCALAR_PLAN_STEPS)
        return -1;
    int index = builder->count++;
    ScalarStep *step = &builder->steps[index];
    *step = (ScalarStep){.expression = expression, .op = (unsigned char)op,
        .left = (unsigned char)left, .right = (unsigned char)right,
        .output = (unsigned char)index, .depth = (unsigned char)depth,
        .conversion = (unsigned char)scalar_conversion(expression->type)};
    if(op == SCALAR_CAST)
        step->cast_conversion = (unsigned char)scalar_conversion(expression->name);
    if(op == SCALAR_BINARY) {
        step->operation = (unsigned short)binary_operation(expression->op);
        const char *left_type = builder->function->exprs[expression->left].type;
        step->left_width = (signed char)integer_operation_width(left_type);
        step->right_width = (signed char)(step->operation == BinaryShiftLeft ||
            step->operation == BinaryShiftRight ? 32 :
            integer_operation_width(builder->function->exprs[expression->right].type));
        step->left_unsigned = left_type[0] == 'u';
    }
    return index;
}

static int
scalar_compile(ScalarBuilder *builder, int index, int depth)
{
    if(index < 0 || index >= builder->function->expr_count ||
       depth >= SCALAR_PLAN_STEPS || builder->count >= SCALAR_PLAN_STEPS)
        return -1;
    const ZirExpr *expression = &builder->function->exprs[index];
    if(expression->is_move || expression->is_function_value)
        return -1;
    int left = -1, right = -1;
    ScalarOp op;
    switch(expression->kind) {
    case ZIR_EXPR_SIZE_OF:
    case ZIR_EXPR_INT:
    case ZIR_EXPR_FLOAT:
    case ZIR_EXPR_STRING:
    case ZIR_EXPR_COMPILE_TIME:
    case ZIR_EXPR_IDENT:
        return scalar_step(builder, SCALAR_LEAF, expression, 0, 0, depth);
    case ZIR_EXPR_UNARY:
        if(!strcmp(expression->op, "&"))
            return -1;
        right = scalar_compile(builder, expression->right, depth + 1);
        if(right < 0) return -1;
        return scalar_step(builder, SCALAR_UNARY, expression, 0, right, depth);
    case ZIR_EXPR_CAST:
        /* Aggregate casts can clone ownership trees. Keep those on the
         * ordinary evaluator path rather than introducing plan ownership. */
        if(value_kind(expression->name) != VALUE_INT &&
           value_kind(expression->name) != VALUE_REAL && expression->name[0] != '*')
            return -1;
        right = scalar_compile(builder, expression->right, depth + 1);
        if(right < 0) return -1;
        return scalar_step(builder, SCALAR_CAST, expression, 0, right, depth);
    case ZIR_EXPR_MEMBER:
    case ZIR_EXPR_POINTER_MEMBER:
        left = scalar_compile(builder, expression->left, depth + 1);
        if(left < 0) return -1;
        op = expression->kind == ZIR_EXPR_MEMBER ? SCALAR_MEMBER : SCALAR_POINTER_MEMBER;
        return scalar_step(builder, op, expression, left, 0, depth);
    case ZIR_EXPR_INDEX:
    case ZIR_EXPR_BINARY:
        left = scalar_compile(builder, expression->left, depth + 1);
        if(left < 0) return -1;
        if(expression->kind == ZIR_EXPR_BINARY &&
           (!strcmp(expression->op, "&&") || !strcmp(expression->op, "||"))) {
            op = !strcmp(expression->op, "&&") ? SCALAR_AND : SCALAR_OR;
            int guard = scalar_step(builder, op, expression, left, 0, depth);
            if(guard < 0) return -1;
            right = scalar_compile(builder, expression->right, depth + 1);
            if(right < 0) return -1;
            int result = scalar_step(builder, SCALAR_TRUTH, expression, left, right, depth);
            if(result < 0) return -1;
            builder->steps[guard].output = (unsigned char)result;
            builder->steps[guard].target = (unsigned char)(result + 1);
            return result;
        }
        right = scalar_compile(builder, expression->right, depth + 1);
        if(right < 0) return -1;
        op = expression->kind == ZIR_EXPR_INDEX ? SCALAR_INDEX : SCALAR_BINARY;
        return scalar_step(builder, op, expression, left, right, depth);
    case ZIR_EXPR_CONDITIONAL: {
        left = scalar_compile(builder, expression->left, depth + 1);
        if(left < 0) return -1;
        int guard = scalar_step(builder, SCALAR_FALSE, expression, left, 0, depth);
        if(guard < 0) return -1;
        right = scalar_compile(builder, expression->right, depth + 1);
        if(right < 0) return -1;
        int first = scalar_step(builder, SCALAR_COPY, expression, right, 0, depth);
        int jump = scalar_step(builder, SCALAR_JUMP, expression, 0, 0, depth);
        if(first < 0 || jump < 0) return -1;
        builder->steps[guard].target = (unsigned char)builder->count;
        right = scalar_compile(builder, expression->third, depth + 1);
        if(right < 0) return -1;
        int result = scalar_step(builder, SCALAR_COPY, expression, right, 0, depth);
        if(result < 0) return -1;
        builder->steps[first].output = (unsigned char)result;
        builder->steps[jump].target = (unsigned char)(result + 1);
        return result;
    }
    default:
        return -1;
    }
}

static const ScalarPlan *
scalar_plan(Frame *frame, int index)
{
    const ZirExpr *expression = &frame->function->exprs[index];
    Vm *vm = frame->vm;
    if(vm->scalar_site_slots == 0 ||
       (vm->scalar_site_count * 2 >= vm->scalar_site_slots &&
        vm->scalar_site_slots < SCALAR_PLAN_SLOTS)) {
        size_t slots = vm->scalar_site_slots ? vm->scalar_site_slots * 2 : 256;
        VmScalarSite *sites = calloc(slots, sizeof(*sites));
        if(sites == NULL) return NULL;
        for(size_t i = 0; i < vm->scalar_site_slots; i++) {
            VmScalarSite prior = vm->scalar_sites[i];
            if(prior.expression == NULL) continue;
            size_t position = vm_expression_slot(prior.expression, slots);
            while(sites[position].expression != NULL)
                position = (position + 1) & (slots - 1);
            sites[position] = prior;
        }
        free(vm->scalar_sites);
        vm->scalar_sites = sites;
        vm->scalar_site_slots = slots;
    }
    size_t position = vm_expression_slot(expression, vm->scalar_site_slots);
    while(vm->scalar_sites[position].expression != NULL &&
          (vm->scalar_sites[position].expression != expression ||
           vm->scalar_sites[position].module != frame->module))
        position = (position + 1) & (vm->scalar_site_slots - 1);
    VmScalarSite *site = &vm->scalar_sites[position];
    if(site->expression != NULL)
        return site->plan;
    if(vm->scalar_site_count * 2 >= SCALAR_PLAN_SLOTS)
        return NULL;
    *site = (VmScalarSite){.expression = expression, .module = frame->module};
    vm->scalar_site_count++;
    ScalarBuilder builder = {.function = frame->function};
    int output = scalar_compile(&builder, index, 0);
    /* Even a binary operation on two leaves benefits from retaining its
     * checked operation and coercion policy across repeated execution. */
    if(output < 0 || builder.count < 3)
        return NULL;
    ScalarPlan *plan = malloc(sizeof(*plan) + (size_t)builder.count * sizeof(*plan->steps));
    if(plan == NULL)
        return NULL;
    plan->count = (unsigned char)builder.count;
    plan->output = (unsigned char)output;
    memcpy(plan->steps, builder.steps, (size_t)builder.count * sizeof(*plan->steps));
    site->plan = plan;
    return plan;
}

static void
free_scalar_plans(Vm *vm)
{
    if(vm->scalar_sites == NULL)
        return;
    for(size_t i = 0; i < vm->scalar_site_slots; i++)
        free(vm->scalar_sites[i].plan);
    free(vm->scalar_sites);
    vm->scalar_sites = NULL;
    vm->scalar_site_count = vm->scalar_site_slots = 0;
}

static Value
scalar_member(Frame *frame, const ZirExpr *expression, Value value, int pointer,
              ScalarConversion conversion)
{
    if(pointer)
        value = place_read(frame->vm, value);
    else if(!strcmp(expression->name, "count")) {
        if(value.kind == VALUE_STRING || value.kind == VALUE_SLICE) {
            value = int_value((int64_t)value.length);
            return scalar_result(frame, &value, expression->type, conversion);
        }
        if(value.kind == VALUE_ARRAY && value.array != NULL) {
            value = int_value(value.array->length);
            return scalar_result(frame, &value, expression->type, conversion);
        }
    } else if(value.kind == VALUE_ARRAY && value.array != NULL &&
              value.array->length == 0 && !strcmp(expression->name, "data")) {
        value = int_value(0);
        return scalar_result(frame, &value, expression->type, conversion);
    }
    Value *field = value.kind == VALUE_RECORD && value.record != NULL ?
        record_field_path(value.record, expression->name) : NULL;
    if(field == NULL) {
        frame->vm->failed = 1;
        return int_value(0);
    }
    if(value.record->type != NULL && value.record->type->is_union) {
        value = union_member_read(frame->vm, value.record, expression->type);
        return scalar_result(frame, &value, expression->type, conversion);
    }
    if(normalized_field_read(field, expression->type))
        return *field;
    return scalar_result(frame, field, expression->type, conversion);
}

static Value
scalar_execute(Frame *frame, const ScalarPlan *plan, int depth)
{
    /* A plan cannot mutate bindings, call functions or collect storage.
     * Borrow normalized local slots and completed registers until it ends.
     * Every materialized result has its own stable destination; inactive
     * container words of numeric results need not be initialized or copied. */
    Value storage[SCALAR_PLAN_STEPS];
    const Value *values[SCALAR_PLAN_STEPS];
    for(int pc = 0; pc < plan->count && !frame->vm->failed; pc++) {
        const ScalarStep *step = &plan->steps[pc];
        const ZirExpr *expression = step->expression;
        if(depth + step->depth >= VM_MAX_DEPTH) {
            frame->vm->failed = 1;
            break;
        }
        Value *output = &storage[step->output];
        Value value;
        switch(step->op) {
        case SCALAR_LEAF: {
            const char *name = expression->name;
            Local *local = expression->kind == ZIR_EXPR_IDENT &&
                !(name[0] == 't' && !strcmp(name, "true")) &&
                !(name[0] == 'f' && !strcmp(name, "false")) &&
                !(name[0] == 'n' && !strcmp(name, "null")) ?
                find_local(frame, name) : NULL;
            if(local != NULL && local->value.kind != VALUE_INVALID &&
               local->type == expression->type)
                values[step->output] = &local->value;
            else if(expression->kind == ZIR_EXPR_INT || expression->kind == ZIR_EXPR_FLOAT) {
                const Value *literal = numeric_literal(frame, expression, output);
                /* A later literal can evict this cache entry. Snapshot its
                 * live words before evaluating the next operand. */
                if(literal != output) {
                    if(literal->kind == VALUE_REAL)
                        real_result(output, literal->real);
                    else if(literal->kind == VALUE_INT && literal->unsigned64)
                        uint_result(output, literal->bits);
                    else if(literal->kind == VALUE_INT)
                        int_result(output, literal->integer);
                    else
                        *output = *literal;
                }
                values[step->output] = output;
            } else {
                *output = eval_leaf(frame, expression);
                values[step->output] = output;
            }
            continue;
        }
        case SCALAR_AND:
        case SCALAR_OR: {
            int truth = truthy(*values[step->left]);
            if((step->op == SCALAR_AND && !truth) || (step->op == SCALAR_OR && truth)) {
                values[step->output] = int_result(output, truth);
                pc = step->target - 1;
            }
            continue;
        }
        case SCALAR_FALSE:
            if(!truthy(*values[step->left])) pc = step->target - 1;
            continue;
        case SCALAR_JUMP:
            pc = step->target - 1;
            continue;
        case SCALAR_COPY:
            values[step->output] = scalar_coerce(frame, values[step->left],
                expression->type, (ScalarConversion)step->conversion, output);
            continue;
        case SCALAR_MEMBER:
        case SCALAR_POINTER_MEMBER:
            *output = scalar_member(frame, expression, *values[step->left],
                step->op == SCALAR_POINTER_MEMBER, (ScalarConversion)step->conversion);
            values[step->output] = output;
            continue;
        case SCALAR_INDEX: {
            Value left = *values[step->left], right = *values[step->right];
            if(right.kind != VALUE_INT || (!right.unsigned64 && right.integer < 0)) {
                frame->vm->failed = 1;
                break;
            }
            if(left.kind == VALUE_STRING && integer_bits(right) < left.length)
                value = int_value(left.data[integer_bits(right)]);
            else {
                Value element = indexed_element(frame->vm, left, integer_bits(right));
                if(!place_valid(element)) {
                    frame->vm->failed = 1;
                    break;
                }
                value = place_read(frame->vm, element);
                if(!frame->vm->failed && normalized_scalar_read(value,
                       element.array->element_type, expression->type)) {
                    *output = value;
                    values[step->output] = output;
                    continue;
                }
            }
            break;
        }
        case SCALAR_CAST: {
            const Value *cast = scalar_coerce(frame, values[step->right], expression->name,
                                              (ScalarConversion)step->cast_conversion, output);
            if(!frame->vm->failed)
                values[step->output] = scalar_coerce(frame, cast, expression->type,
                    (ScalarConversion)step->conversion, output);
            continue;
        }
        case SCALAR_UNARY: {
            Value right = *values[step->right];
            if(!strcmp(expression->op, "*")) value = place_read(frame->vm, right);
            else if(!strcmp(expression->op, "-"))
                value = right.kind == VALUE_REAL ? real_value(-right.real) :
                    right.unsigned64 && strcmp(expression->type, "integer") != 0 ?
                    uint_value(UINT64_C(0) - right.bits) :
                    int_value(signed64(UINT64_C(0) - integer_bits(right)));
            else if(!strcmp(expression->op, "+")) value = right;
            else if(!strcmp(expression->op, "!")) value = int_value(!truthy(right));
            else if(!strcmp(expression->op, "~") && right.kind == VALUE_INT)
                value = right.unsigned64 ? uint_value(~right.bits) : int_value(~right.integer);
            else frame->vm->failed = 1;
            break;
        }
        case SCALAR_TRUTH:
            int_result(output, truthy(*values[step->right]));
            values[step->output] = scalar_coerce(frame, output, expression->type,
                (ScalarConversion)step->conversion, output);
            continue;
        case SCALAR_BINARY:
            if((values[step->left]->kind == VALUE_INT || values[step->left]->kind == VALUE_REAL) &&
               (values[step->right]->kind == VALUE_INT || values[step->right]->kind == VALUE_REAL)) {
                binary_numeric(frame->vm, (BinaryOperation)step->operation,
                    values[step->left], values[step->right], step->left_width,
                    step->right_width, step->left_unsigned, output);
                if(!frame->vm->failed)
                    values[step->output] = scalar_coerce(frame, output, expression->type,
                        (ScalarConversion)step->conversion, output);
                continue;
            }
            value = binary_with_policy(frame->vm, (BinaryOperation)step->operation,
                values[step->left], values[step->right], step->left_width,
                step->right_width, step->left_unsigned);
            break;
        default:
            frame->vm->failed = 1;
            break;
        }
        if(!frame->vm->failed) {
            *output = value;
            values[step->output] = scalar_coerce(frame, output, expression->type,
                (ScalarConversion)step->conversion, output);
        }
    }
    return frame->vm->failed ? int_value(0) : *values[plan->output];
}

static Value
eval_expression(Frame *frame, int index, int depth)
{
    const ZirExpr *expression = &frame->function->exprs[index];
    Value value = int_value(0), left = int_value(0), right = int_value(0);
    VmRoots value_root = {frame->vm->evaluation_roots, &value, 1};
    VmRoots left_root = {&value_root, &left, 1};
    VmRoots right_root = {&left_root, &right, 1};
    frame->vm->evaluation_roots = &right_root;
    switch(expression->kind) {
    case ZIR_EXPR_UNARY:
        if(strcmp(expression->op, "&") == 0) {
            /* *place: a pointer at the storage the place names. */
            Value target = owned_place(frame, expression->right, depth + 1);
            if(!place_valid(target)) {
                frame->vm->failed = 1;
                break;
            }
            if(target.record != NULL) target.record->address_taken = 1;
            if(target.array != NULL) target.array->address_taken = 1;
            value = target;
            break;
        }
        right = eval(frame, expression->right, depth + 1);
        if(frame->vm->failed)
            break;
        if(strcmp(expression->op, "*") == 0) {
            value = place_read(frame->vm, right);
            break;
        }
        if(strcmp(expression->op, "-") == 0)
            value = right.kind == VALUE_REAL ? real_value(-right.real) :
                    /* A positive abstract literal can need unsigned storage
                     * even though its negation, notably INT64_MIN, is signed.
                     * Only a concrete unsigned expression keeps that form. */
                    right.unsigned64 && strcmp(expression->type, "integer") != 0 ?
                                       uint_value(UINT64_C(0) - right.bits) :
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
                array_set(value.array, (size_t)position++, coerce(frame->vm,
                    value.array->owner, initialized,
                    value.array->element_type));
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
    case ZIR_EXPR_POINTER_MEMBER: {
        /* p.field reads a field of the record p points at. */
        left = eval(frame, expression->left, depth + 1);
        Value target = frame->vm->failed ? (Value){0} : place_read(frame->vm, left);
        Value *field = target.kind == VALUE_RECORD && target.record != NULL ?
                       record_field_path(target.record, expression->name) : NULL;
        if(field == NULL)
            frame->vm->failed = 1;
        else if(target.record->type != NULL && target.record->type->is_union)
            value = union_member_read(frame->vm, target.record, expression->type);
        else if(normalized_field_read(field, expression->type))
            return *field;
        else
            value = *field;
        break;
    }
    case ZIR_EXPR_MEMBER: {
        if(expression->is_move) {
            Value *source = assignment_slot(frame, index, depth + 1);
            char element[ZIR_NAME_MAX];
            if(source == NULL ||
               !vm_vec_element_type(frame->vm, frame->module, expression->type,
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
        else if(normalized_field_read(field, expression->type))
            return *field;
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
            value = left;
            value.data = left.data != NULL ? left.data + start : NULL;
            value.length = (size_t)(integer_bits(high) - integer_bits(low));
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
        if(backing != NULL) backing->borrowed = 1;
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
                vm_vec_element_type(frame->vm, frame->module,
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
            Value element = indexed_element(frame->vm, left, integer_bits(right));
            if(place_valid(element)) {
                value = place_read(frame->vm, element);
                if(!frame->vm->failed && normalized_scalar_read(value,
                       element.array->element_type, expression->type))
                    return value;
            } else
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
        Builtin builtin = builtin_call(expression->name);
        if(builtin == BuiltinTextView) {
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
                Value part = place_read(frame->vm, indexed_element(frame->vm, bytes, i));
                if(part.kind != VALUE_INT) {
                    frame->vm->failed = 1;
                    free(item);
                    break;
                }
                item->data[i] = (unsigned char)integer_bits(part);
            }
            if(frame->vm->failed)
                break;
            item->data[bytes.length] = 0;
            item->length = bytes.length;
            value = keep_string(frame->vm, item, NULL, sizeof(*item) + bytes.length + 1);
            break;
        }
        if(builtin == BuiltinPrint) {
            vm_print(frame, expression, depth);
            value.kind = VALUE_VOID;
            break;
        }
        if(builtin == BuiltinNew) {
            /* New(T): one zeroed T in heap storage the pointer keeps alive. */
            const char *target = skip_ws(expression->type + 1);
            Array *storage = allocate_array_try(frame->vm, frame->module, target, 1, 1);
            if(storage == NULL)
                break;
            array_set(storage, 0, default_value(frame->vm, frame->module, target, 0));
            storage->address_taken = 1;
            storage->heap = 1;
            value = (Value){.kind = VALUE_POINTER, .indexed = 1, .array = storage};
            break;
        }
        if(builtin == BuiltinFree) {
            Value pointer = eval(frame, expression->first_child, depth + 1);
            if(frame->vm->failed)
                break;
            /* Only New storage is freed, once; a null pointer is ignored. */
            if(pointer.kind != VALUE_POINTER && integer_bits(pointer) == 0) {
                value.kind = VALUE_VOID;
                break;
            }
            if(pointer.kind != VALUE_POINTER || pointer.array == NULL ||
               !pointer.array->heap || pointer.array->freed) {
                frame->vm->failed = 1;
                break;
            }
            pointer.array->freed = 1;
            value.kind = VALUE_VOID;
            break;
        }
        if(builtin >= BuiltinVecPush && builtin <= BuiltinBuilderFinish) {
            int first = expression->first_child;
            int push = builtin == BuiltinVecPush;
            left = assignment_slot_root(frame, first, depth + 1);
            Value *vec = place_slot(left);
            char element[ZIR_NAME_MAX];
            if(vec == NULL || vec->kind != VALUE_RECORD ||
               !vm_vec_element_type(frame->vm, frame->module,
                   frame->function->exprs[first].type,
                   element, sizeof(element))) {
                frame->vm->failed = 1;
                break;
            }
            value = *vec;
            if(builtin == BuiltinVecSwap) {
                int second = frame->function->exprs[first].next_sibling;
                Value *other = assignment_slot(frame, second, depth + 1);
                if(other == NULL || other->kind != VALUE_RECORD ||
                   !same_record_type(vec->record->owner, vec->record->type,
                                     other->record)) {
                    frame->vm->failed = 1;
                    break;
                }
                Value saved = *vec;
                *vec = *other;
                *other = saved;
                value = (Value){.kind = VALUE_VOID};
                break;
            }
            if(builtin == BuiltinVecClone) {
                int second = frame->function->exprs[first].next_sibling;
                Value *source = assignment_slot(frame, second, depth + 1);
                Value *dest_data, *dest_count, *dest_capacity;
                Value *src_data, *src_count, *src_capacity;
                Array *copy = NULL;
                if(source == NULL || source->kind != VALUE_RECORD ||
                   !same_record_type(vec->record->owner, vec->record->type,
                                     source->record)) {
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
                        Value entry = src_data->kind == VALUE_ARRAY ?
                            array_get(src_data->array, (size_t)i) : (Value){0};
                        if(entry.kind == VALUE_INVALID) {
                            frame->vm->failed = 1;
                            break;
                        }
                        array_set(copy, (size_t)i, coerce(frame->vm, frame->module,
                                                   entry, element));
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
            if(builtin == BuiltinVecSlice) {
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
            if(builtin == BuiltinVecPop || builtin == BuiltinVecGet) {
                int get = builtin == BuiltinVecGet;
                const ZirModule *owner = NULL;
                const ZirType *record_type = vm_find_type(frame->vm, frame->module,
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
                    size_t position = (size_t)(get ? at : count->integer - 1);
                    Value source = data->kind == VALUE_ARRAY ?
                        array_get(data->array, position) : (Value){0};
                    if(source.kind == VALUE_INVALID) {
                        frame->vm->failed = 1;
                        break;
                    }
                    if(get)
                        *item = coerce(frame->vm, frame->module, source,
                                       element);
                    else {
                        *item = source;
                        array_set(data->array, position, (Value){0});
                        count->integer--;
                    }
                    if(frame->vm->failed)
                        break;
                    *has_value = int_value(1);
                }
                value = result;
                break;
            }
            if(builtin == BuiltinBuilderAppend || builtin == BuiltinBuilderFinish) {
                int finish = builtin == BuiltinBuilderFinish;
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
                                Value byte = data->kind == VALUE_ARRAY ?
                                    array_get(data->array, (size_t)i) : (Value){0};
                                if(byte.kind != VALUE_INT) {
                                    free(built);
                                    built = NULL;
                                    break;
                                }
                                built->data[i] =
                                    (unsigned char)integer_bits(byte);
                            }
                        }
                        if(built == NULL) {
                            frame->vm->failed = 1;
                            break;
                        }
                        built->length = (size_t)count->integer;
                    }
                    value = built != NULL ?
                        keep_string(frame->vm, built, NULL, sizeof(*built) + built->length) :
                        string_value((const unsigned char *)"", 0);
                    if(data->kind == VALUE_ARRAY)
                        retire_value(frame->vm, *data, 0);
                    *data = (Value){.kind = VALUE_ARRAY};
                    *count = int_value(0);
                    *capacity = int_value(0);
                    release_retired(frame->vm);
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
                                array_set(grown, (size_t)i, array_get(data->array, (size_t)i));
                                array_set(data->array, (size_t)i, (Value){0});
                            }
                            retire_value(frame->vm, *data, 0);
                        }
                        *data = (Value){.kind = VALUE_ARRAY, .array = grown};
                        *capacity = int_value(next_capacity);
                    }
                    for(position = 0; position < length; position++) {
                        array_set(data->array, (size_t)(count->integer + position),
                            int_value(text.data[position]));
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
                            array_set(grown, (size_t)i, array_get(data->array, (size_t)i));
                            array_set(data->array, (size_t)i, (Value){0});
                        }
                        retire_value(frame->vm, *data, 0);
                    }
                    *data = (Value){.kind = VALUE_ARRAY, .array = grown};
                    *capacity = int_value(next_capacity);
                }
                Value stored = coerce(frame->vm, frame->module, item, element);
                if(frame->vm->failed) break;
                array_set(data->array, (size_t)count->integer, stored);
                count->integer++;
                value = int_value(1);
            } else {
                if(data->array != NULL) {
                    for(int i = 0; i < count->integer; i++) {
                        retire_value(frame->vm, array_get(data->array, (size_t)i), 0);
                        array_set(data->array, (size_t)i, (Value){0});
                    }
                }
                *count = int_value(0);
                if(builtin == BuiltinVecFree) {
                    retire_value(frame->vm, *data, 0);
                    *data = (Value){.kind = VALUE_ARRAY};
                    *capacity = int_value(0);
                }
                value = (Value){.kind = VALUE_VOID};
            }
            break;
        }
        int count = 0;
        uint64_t used = 0;
        const ZirModule *owner = NULL;
        const ZirFunction *callee = NULL;
        const ZirImport *external = NULL;
        if(expression->slot_type[0]) {
            Value callable;
            if(expression->name[0]) {
                Local *binding = find_local(frame, expression->name);
                Value *global = binding == NULL ?
                    find_global_value(frame, expression) : NULL;
                callable = binding != NULL ? binding->value :
                    global != NULL ? *global : (Value){0};
            } else {
                callable = eval(frame, expression->left, depth + 1);
            }
            if(frame->vm->failed || callable.kind != VALUE_SLOT ||
               callable.slot_type != vm_find_type(frame->vm, frame->module, expression->slot_type, NULL)) {
                frame->vm->failed = 1;
                break;
            }
            owner = callable.slot_module;
            callee = callable.slot_function;
        } else {
            VmCallSite target = resolve_call(frame, expression);
            owner = target.owner;
            callee = target.callee;
            external = target.external;
        }
        if((owner == NULL || callee == NULL) && external == NULL) {
            frame->vm->failed = 1;
            break;
        }
        /* Arguments are sized by this call, not the most any call takes:
         * a nested call keeps its arguments until it returns. */
        int slots = 0;
        for(int child = expression->first_child; child >= 0;
            child = frame->function->exprs[child].next_sibling)
            if(frame->function->exprs[child].argument_index >= slots)
                slots = frame->function->exprs[child].argument_index + 1;
        if(slots > VM_MAX_PARAMS) {
            frame->vm->failed = 1;
            break;
        }
        ZirFunction *external_signature = NULL;
        if(external != NULL) {
            /* Foreign calls need their parameter types before arguments are
             * evaluated too. Keep the large function descriptor on the heap. */
            external_signature = AllocateOrExit(sizeof(*external_signature));
            memset(external_signature, 0, sizeof(*external_signature));
            copy_text(external_signature->name, sizeof(external_signature->name),
                      external->name);
            external_signature->args_text = KeepParameters(external->args);
            copy_text(external_signature->return_type,
                      sizeof(external_signature->return_type),
                      external->return_type);
            external_signature->is_extern = 1;
            external_signature->span = external->span;
            owner = frame->module;
            callee = external_signature;
        }
        Value few[4];
        Value *args = slots <= 4 ? few : AllocateOrExit((size_t)slots * sizeof(*args));
        memset(args, 0, (size_t)(slots <= 4 ? 4 : slots) * sizeof(*args));
        VmRoots argument_roots = {frame->vm->evaluation_roots, args, slots};
        frame->vm->evaluation_roots = &argument_roots;
        const VmSignature *parameters = vm_signature(frame->vm, owner, callee);
        for(int child = expression->first_child; child >= 0;
            child = frame->function->exprs[child].next_sibling) {
            int position = frame->function->exprs[child].argument_index;
            if(count >= VM_MAX_PARAMS || position < 0 ||
               position >= slots || (used & ((uint64_t)1 << position))) {
                frame->vm->failed = 1;
                break;
            }
            args[position] = eval(frame, child, depth + 1);
            /* Capture value parameters before another argument can mutate
             * their source through a pointer or fixed-array view. A read-only
             * callee may share this snapshot, never the caller's live record. */
            if(parameters != NULL && position < parameters->count &&
               (args[position].kind == VALUE_RECORD ||
                (args[position].kind == VALUE_ARRAY &&
                 ArrayElementType(parameters->parameters[position].type,
                                  NULL, 0, NULL))) &&
               !vm_vec_element_type(frame->vm, owner, parameters->parameters[position].type,
                               NULL, 0)) {
                for(int later = frame->function->exprs[child].next_sibling;
                    later >= 0;
                    later = frame->function->exprs[later].next_sibling) {
                    if(vm_expression_calls(frame->function, later, 0)) {
                        args[position] = coerce(frame->vm, owner, args[position],
                            parameters->parameters[position].type);
                        break;
                    }
                }
            }
            used |= (uint64_t)1 << position;
            count++;
        }
        if(!frame->vm->failed)
            value = run_function(frame->vm, owner, callee, args, count);
        frame->vm->evaluation_roots = argument_roots.previous;
        /* A consumed Vec is retired by its callee. The argument roots keep
         * it safe until that call returns; release it when they leave scope. */
        release_retired(frame->vm);
        free(external_signature);
        if(args != few)
            free(args);
        break;
    }
    default:
        frame->vm->failed = 1;
        break;
    }
    return coerce_expression(frame->vm, frame->module, value,
                             expression->type);
}

Value
eval(Frame *frame, int index, int depth)
{
    if(frame->vm->failed || index < 0 || index >= frame->function->expr_count ||
       depth >= VM_MAX_DEPTH) {
        frame->vm->failed = 1;
        return int_value(0);
    }
    const ZirExpr *expression = &frame->function->exprs[index];
    switch(expression->kind) {
    case ZIR_EXPR_SIZE_OF:
    case ZIR_EXPR_INT:
    case ZIR_EXPR_FLOAT:
    case ZIR_EXPR_STRING:
    case ZIR_EXPR_COMPILE_TIME:
    case ZIR_EXPR_IDENT:
        return eval_leaf(frame, expression);
    default:
        break;
    }
    if(expression->kind == ZIR_EXPR_BINARY || expression->kind == ZIR_EXPR_CONDITIONAL) {
        const ScalarPlan *plan = scalar_plan(frame, index);
        if(plan != NULL)
            return scalar_execute(frame, plan, depth);
    }
    VmRoots *previous = frame->vm->evaluation_roots;
    Value result = eval_expression(frame, index, depth);
    frame->vm->evaluation_roots = previous;
    return result;
}
