#include "zir_vm_internal.h"
/* Buffers vm_print keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct VmPrintBuffers {
    Value values[PRINT_PIECES_MAX];
    unsigned char bytes[ZIR_TEXT_MAX];
} VmPrintBuffers;

static void vm_print(Frame *frame, const ZirExpr *expression, int depth);

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
    for(int child = function->exprs[first].next_sibling;
        child >= 0 && !frame->vm->failed && argument < PRINT_PIECES_MAX;
        child = function->exprs[child].next_sibling, argument++) {
        types[argument] = ScalarType(function->exprs[child].type);
        buffers->values[argument] = eval(frame, child, depth + 1);
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
}

/* Checked `print`: evaluate every argument left to right, then write literal
 * pieces and values to standard output with the native targets' spelling. */
static void
vm_print(Frame *frame, const ZirExpr *expression, int depth)
{
    static _Thread_local VmPrintBuffers *spares[16];
    static _Thread_local int spare_count;
    VmPrintBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    vm_print_with_buffers(frame, expression, depth, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

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
    if(ResolveGlobalAt(frame->module, name, SpanPath(frame->function->span),
                       &owner, &declaration) != 1)
        return NULL;
    for(int i = 0; i < frame->vm->global_count; i++) {
        GlobalSlot *slot = &frame->vm->globals[i];
        if(slot->module == owner && slot->declaration == declaration)
            return &slot->value;
    }
    return NULL;
}

Value *
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
Value
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

int
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

Value *
pointer_target(Vm *vm, Value pointer)
{
    if(pointer.kind != VALUE_POINTER || pointer.pointee == NULL ||
       (pointer.array != NULL && pointer.array->freed)) {
        vm->failed = 1;
        return NULL;
    }
    if(pointer.bits != 0) {
        /* A pointer at a local is valid only while its call runs. */
        for(Frame *frame = vm->active_frame; frame != NULL; frame = frame->caller)
            if(frame->serial == pointer.bits)
                return pointer.pointee;
        vm->failed = 1;
        return NULL;
    }
    return pointer.pointee;
}

/* The storage an assignable expression names, and the record or array that
 * holds it, if any, so a pointer at it can keep that container alive. */
static Value *
owned_slot(Frame *frame, int index, int depth, Record **record, Array **array,
           uint64_t *call)
{
    if(index < 0 || index >= frame->function->expr_count ||
       depth >= VM_MAX_DEPTH)
        return NULL;
    const ZirExpr *expression = &frame->function->exprs[index];
    if(expression->kind == ZIR_EXPR_IDENT) {
        Local *local = find_local(frame, expression->name);
        if(local != NULL) {
            *call = frame->serial;
            return &local->value;
        }
        return find_global_value(frame, expression->name);
    }
    if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "*")) {
        Value pointer = eval(frame, expression->right, depth + 1);
        Value *target = pointer_target(frame->vm, pointer);
        if(target != NULL) {
            *record = pointer.record;
            *array = pointer.array;
            *call = pointer.bits;
        }
        return target;
    }
    if(expression->kind == ZIR_EXPR_POINTER_MEMBER) {
        Value pointer = eval(frame, expression->left, depth + 1);
        Value *target = pointer_target(frame->vm, pointer);
        if(target == NULL || target->kind != VALUE_RECORD || target->record == NULL) {
            frame->vm->failed = 1;
            return NULL;
        }
        *record = target->record;
        *array = NULL;
        *call = 0;
        return record_field_path(target->record, expression->name);
    }
    if(expression->kind == ZIR_EXPR_INDEX) {
        Value *base = owned_slot(frame, expression->left, depth + 1, record, array, call);
        Value index_value = eval(frame, expression->right, depth + 1);
        if(base == NULL || index_value.kind != VALUE_INT ||
           (!index_value.unsigned64 && index_value.integer < 0)) {
            frame->vm->failed = 1;
            return NULL;
        }
        Value *element = indexed_element(*base, integer_bits(index_value));
        if(element == NULL)
            frame->vm->failed = 1;
        else if(base->kind == VALUE_ARRAY || base->kind == VALUE_SLICE) {
            *record = NULL;
            *array = base->array;
            *call = 0;
        }
        return element;
    }
    if(expression->kind != ZIR_EXPR_MEMBER)
        return NULL;
    Value *base = owned_slot(frame, expression->left, depth + 1, record, array, call);
    if(base == NULL || base->kind != VALUE_RECORD)
        return NULL;
    *record = base->record;
    *array = NULL;
    *call = 0;
    if(base->record->type != NULL && base->record->type->is_union) {
        /* Union writes land in the shared bit slot; the assignment layer
         * reinterprets through the declared field type. */
        frame->union_write_record = base->record;
        frame->union_write_type = expression->type;
        return &base->record->fields[0].value;
    }
    return record_field_path(base->record, expression->name);
}

Value *
assignment_slot(Frame *frame, int index, int depth)
{
    Record *record = NULL;
    Array *array = NULL;
    uint64_t call = 0;
    return owned_slot(frame, index, depth, &record, &array, &call);
}

Value
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
    if(left.kind == VALUE_POINTER || right.kind == VALUE_POINTER) {
        /* Pointers compare by target; null is the handle 0. */
        const Value *a_target = left.kind == VALUE_POINTER ? left.pointee :
            integer_bits(left) == 0 ? NULL : (const Value *)&left;
        const Value *b_target = right.kind == VALUE_POINTER ? right.pointee :
            integer_bits(right) == 0 ? NULL : (const Value *)&right;
        if(strcmp(op, "==") == 0)
            return int_value(a_target == b_target);
        if(strcmp(op, "!=") == 0)
            return int_value(a_target != b_target);
        goto failed;
    }
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
/* Buffers eval keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */

Value eval(Frame *frame, int index, int depth);

Value
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
        if(strcmp(expression->op, "&") == 0) {
            /* *place: a pointer at the storage the place names. */
            Record *record = NULL;
            Array *array = NULL;
            uint64_t call = 0;
            Value *target = owned_slot(frame, expression->right, depth + 1,
                                       &record, &array, &call);
            if(target == NULL) {
                frame->vm->failed = 1;
                break;
            }
            if(record != NULL) record->address_taken = 1;
            if(array != NULL) array->address_taken = 1;
            value = (Value){.kind = VALUE_POINTER, .pointee = target,
                            .record = record, .array = array, .bits = call};
            break;
        }
        right = eval(frame, expression->right, depth + 1);
        if(frame->vm->failed)
            break;
        if(strcmp(expression->op, "*") == 0) {
            Value *target = pointer_target(frame->vm, right);
            if(target != NULL)
                value = *target;
            break;
        }
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
    case ZIR_EXPR_POINTER_MEMBER: {
        /* p.field reads a field of the record p points at. */
        left = eval(frame, expression->left, depth + 1);
        Value *target = frame->vm->failed ? NULL : pointer_target(frame->vm, left);
        Value *field = target != NULL && target->kind == VALUE_RECORD &&
                       target->record != NULL ?
                       record_field_path(target->record, expression->name) : NULL;
        if(field == NULL)
            frame->vm->failed = 1;
        else if(target->record->type != NULL && target->record->type->is_union)
            value = union_member_read(frame->vm, target->record, expression->type);
        else
            value = *field;
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
        if(!strcmp(expression->name, "print")) {
            vm_print(frame, expression, depth);
            value.kind = VALUE_VOID;
            break;
        }
        if(!strcmp(expression->name, "zi_new")) {
            /* New(T): one zeroed T in heap storage the pointer keeps alive. */
            const char *target = skip_ws(expression->type + 1);
            Array *storage = allocate_array_try(frame->vm, frame->module, target, 1, 1);
            if(storage == NULL)
                break;
            storage->elements[0] = default_value(frame->vm, frame->module, target, 0);
            storage->address_taken = 1;
            storage->heap = 1;
            value = (Value){.kind = VALUE_POINTER, .pointee = &storage->elements[0],
                            .array = storage};
            break;
        }
        if(!strcmp(expression->name, "zi_free")) {
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
            if(!strcmp(expression->name, "VecClone")) {
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
        int count = 0;
        uint64_t used = 0;
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
        Value few[4];
        Value *args = slots <= 4 ? few : AllocateOrExit((size_t)slots * sizeof(*args));
        memset(args, 0, (size_t)(slots <= 4 ? 4 : slots) * sizeof(*args));
        for(int child = expression->first_child; child >= 0;
            child = frame->function->exprs[child].next_sibling) {
            int position = frame->function->exprs[child].argument_index;
            if(count >= VM_MAX_PARAMS || position < 0 ||
               position >= slots || (used & ((uint64_t)1 << position))) {
                frame->vm->failed = 1;
                break;
            }
            args[position] = eval(frame, child, depth + 1);
            used |= (uint64_t)1 << position;
            count++;
        }
        if(!frame->vm->failed) {
            if(external != NULL) {
                /* Allocated only here: a whole ZirFunction per expression
                 * level made every nested call cost kilobytes. */
                ZirFunction *signature = AllocateOrExit(sizeof(*signature));
                memset(signature, 0, sizeof(*signature));
                copy_text(signature->name, sizeof(signature->name),
                          external->name);
                signature->args_text = KeepParameters(external->args);
                copy_text(signature->return_type,
                          sizeof(signature->return_type),
                          external->return_type);
                signature->is_extern = 1;
                signature->span = external->span;
                value = run_function(frame->vm, frame->module,
                                     signature, args, count);
                free(signature);
            } else {
                value = run_function(frame->vm, owner, callee, args, count);
            }
        }
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

