/*
 * zir_vm_simd.c - std/simd operations on the portable VM.
 *
 * A vector is a fixed array of its lanes here (see vm_array_element_type),
 * and each operation evaluates through zi_ref, the scalar reference in
 * include/ziran_simd.h that the C and Go backends match bit for bit.
 */
#include "zir_vm_internal.h"
#include "zir_simd.h"
#define ZIRAN_SIMD_REF_ONLY 1
#include "ziran_simd.h"

/* Make of u8x32 has the most operands: one per lane. */
enum { SIMD_OPERANDS = 32 };

/* A lane's bit pattern, zero-extended as zi_ref expects. */
static uint64_t
lane_bits(Value value, int kind)
{
    if(kind == ZI_K_F32) {
        float number = (float)as_real(value);
        uint32_t bits;
        memcpy(&bits, &number, sizeof(bits));
        return bits;
    }
    if(kind == ZI_K_F64) {
        double number = as_real(value);
        uint64_t bits;
        memcpy(&bits, &number, sizeof(bits));
        return bits;
    }
    return integer_bits(value) & zi_kmask(kind);
}

static Value
lane_value(Vm *vm, const ZirModule *module, uint64_t bits, const ZirVector *vector)
{
    if(vector->is_float && vector->lane_bits == 32) {
        uint32_t narrow = (uint32_t)bits;
        float number;
        memcpy(&number, &narrow, sizeof(number));
        return real_value(number);
    }
    if(vector->is_float) {
        double number;
        memcpy(&number, &bits, sizeof(number));
        return real_value(number);
    }
    return coerce(vm, module, uint_value(bits), vector->lane);
}

static int
read_vector(Value value, const ZirVector *vector, uint64_t *lanes)
{
    if(value.kind != VALUE_ARRAY || value.array == NULL ||
       value.array->length != vector->lanes)
        return 0;
    int kind = SimdLaneKind(vector);
    for(int i = 0; i < vector->lanes; i++)
        lanes[i] = lane_bits(array_get(value.array, (size_t)i), kind);
    return 1;
}

static Value
make_vector(Vm *vm, const ZirModule *module, const ZirVector *vector,
            const uint64_t *lanes)
{
    Array *array = allocate_array_try(vm, module, vector->lane, vector->lanes, 1);
    if(array == NULL)
        return (Value){0};
    for(int i = 0; i < vector->lanes && !vm->failed; i++)
        array_set(array, (size_t)i, lane_value(vm, module, lanes[i], vector));
    return (Value){.kind = VALUE_ARRAY, .array = array};
}

/* The elements a load, store or gather reaches: a fixed array, a slice, or
 * from a pointer to the end of the storage it points into. */
typedef struct SimdMemorySpan {
    Array *array;
    Value *single; /* a pointer at one value outside any array */
    size_t offset, length;
} SimdMemorySpan;

static int
memory_span(Vm *vm, Value base, SimdMemorySpan *span)
{
    memset(span, 0, sizeof(*span));
    if(base.kind == VALUE_ARRAY && base.array != NULL) {
        span->array = base.array;
        span->length = (size_t)base.array->length;
        return 1;
    }
    if(base.kind == VALUE_SLICE) {
        if(base.length == 0)
            return 1;
        if(base.array == NULL || base.offset > (size_t)base.array->length ||
           base.length > (size_t)base.array->length - base.offset)
            return 0;
        span->array = base.array;
        span->offset = base.offset;
        span->length = base.length;
        return 1;
    }
    if(base.kind == VALUE_POINTER && !(base.indexed & 2) && pointer_target(vm, base)) {
        if(base.indexed & 1) {
            span->array = base.array;
            span->offset = base.offset;
            span->length = (size_t)base.array->length - base.offset;
        } else {
            span->single = base.pointee;
            span->length = 1;
        }
        return 1;
    }
    return 0;
}

static Value
span_get(const SimdMemorySpan *span, size_t index)
{
    return span->single != NULL ? *span->single :
        array_get(span->array, span->offset + index);
}

static void
span_set(SimdMemorySpan *span, size_t index, Value value)
{
    if(span->single != NULL)
        *span->single = value;
    else
        array_set(span->array, span->offset + index, value);
}

/* The same message the C and Go helpers stop with. */
static int
access_fits(Frame *frame, const SimdMemorySpan *span, int64_t index, int count)
{
    if(index >= 0 && index <= (int64_t)span->length - count)
        return 1;
    Diagnostic(frame->statement != NULL ? frame->statement->span : (ZirSourceSpan){0},
               "zx.runtime", "SIMD access of %d lanes at %lld out of bounds (length %lld)",
               count, (long long)index, (long long)span->length);
    frame->vm->failed = 1;
    return 0;
}

static int64_t
index_operand(Value value)
{
    return value.unsigned64 ? (int64_t)(value.bits > INT64_MAX ? INT64_MAX : value.bits) :
        value.integer;
}

static Value
memory_operation(Frame *frame, const SimdOp *op, const ZirExpr *expression,
                 const Value *values, int count)
{
    Vm *vm = frame->vm;
    const ZirFunction *function = frame->function;
    SimdMemorySpan span;
    uint64_t lanes[32] = {0};
    ZirVector vector, index_vector;
    int value_child = function->exprs[expression->first_child].next_sibling;
    if(value_child >= 0)
        value_child = function->exprs[value_child].next_sibling;
    if(!memory_span(vm, values[0], &span)) {
        vm->failed = 1;
        return (Value){0};
    }
    if(op->shape == SIMD_GATHER) {
        int second = function->exprs[expression->first_child].next_sibling;
        if(count != 2 || !VectorType(expression->type, &vector) ||
           !VectorType(function->exprs[second].type, &index_vector) ||
           index_vector.lanes != vector.lanes ||
           !read_vector(values[1], &index_vector, lanes)) {
            vm->failed = 1;
            return (Value){0};
        }
        /* Every index is checked before any element is read, as in C. */
        int64_t at[32];
        for(int i = 0; i < vector.lanes; i++) {
            at[i] = index_vector.is_signed ? zi_sx(lanes[i], SimdLaneKind(&index_vector)) :
                (int64_t)lanes[i];
            if(at[i] < 0 || at[i] >= (int64_t)span.length) {
                Diagnostic(frame->statement != NULL ? frame->statement->span :
                           (ZirSourceSpan){0}, "zx.runtime",
                           "gather index %lld out of bounds (length %lld)",
                           (long long)at[i], (long long)span.length);
                vm->failed = 1;
                return (Value){0};
            }
        }
        int kind = SimdLaneKind(&vector);
        for(int i = 0; i < vector.lanes; i++)
            lanes[i] = lane_bits(span_get(&span, (size_t)at[i]), kind);
        return make_vector(vm, frame->module, &vector, lanes);
    }
    int store = op->shape == SIMD_STORE;
    const char *type = store ? (value_child >= 0 ? function->exprs[value_child].type : "") :
        expression->type;
    if(count != (store ? 3 : 2) || !VectorType(type, &vector) || values[1].kind != VALUE_INT) {
        vm->failed = 1;
        return (Value){0};
    }
    int low = op->id == ZI_OP_LOAD_LOW || op->id == ZI_OP_STORE_LOW;
    int reach = low ? 64 / vector.lane_bits : vector.lanes;
    int64_t at = index_operand(values[1]);
    if(!access_fits(frame, &span, at, reach))
        return (Value){0};
    if(store) {
        if(!read_vector(values[2], &vector, lanes)) {
            vm->failed = 1;
            return (Value){0};
        }
        for(int i = 0; i < reach && !vm->failed; i++)
            span_set(&span, (size_t)at + (size_t)i,
                     lane_value(vm, frame->module, lanes[i], &vector));
        return (Value){.kind = VALUE_VOID};
    }
    int kind = SimdLaneKind(&vector);
    for(int i = 0; i < reach; i++)
        lanes[i] = lane_bits(span_get(&span, (size_t)at + (size_t)i), kind);
    return make_vector(vm, frame->module, &vector, lanes);
}

static Value
value_operation(Frame *frame, const SimdOp *op, const ZirExpr *expression,
                const Value *values, int count)
{
    Vm *vm = frame->vm;
    const ZirFunction *function = frame->function;
    uint64_t x[32] = {0}, y[32] = {0}, z[32] = {0}, out[32] = {0};
    uint64_t *vectors[3] = {x, y, z};
    int64_t k[32] = {0};
    int vector_count = 0, scalar_count = 0, constants = 0;
    const char *first = NULL, *select_value = NULL;
    ZirVector result_vector, a;
    int vector_result = VectorType(expression->type, &result_vector);
    /* Make, Splat and Zero are named for their result; Select for its
     * values, after the mask; everything else for its first vector. */
    if(op->shape == SIMD_MAKE || op->shape == SIMD_SPLAT || op->shape == SIMD_ZERO)
        first = expression->type;
    int child = expression->first_child;
    for(int i = 0; i < count; i++, child = function->exprs[child].next_sibling) {
        const char *type = function->exprs[child].type;
        ZirVector vector;
        if(VectorType(type, &vector)) {
            if(vector_count == 3 || !read_vector(values[i], &vector, vectors[vector_count])) {
                vm->failed = 1;
                return (Value){0};
            }
            if(vector_count == 1 && op->shape == SIMD_SELECT)
                select_value = type;
            if(first == NULL && op->shape != SIMD_SELECT)
                first = type;
            vector_count++;
            continue;
        }
        if(first == NULL && op->shape != SIMD_SELECT) {
            vm->failed = 1;
            return (Value){0};
        }
        ZirVector lane_owner;
        if(op->shape == SIMD_SELECT || !VectorType(first, &lane_owner)) {
            vm->failed = 1;
            return (Value){0};
        }
        int kind = SimdLaneKind(&lane_owner);
        /* Make's lanes fill the first operand; a Splat or WithLane lane is
         * the second's first lane; the rest are constants and counts. */
        if(op->shape == SIMD_MAKE) {
            if(scalar_count >= lane_owner.lanes) {
                vm->failed = 1;
                return (Value){0};
            }
            x[scalar_count] = lane_bits(values[i], kind);
        } else if(op->shape == SIMD_SPLAT || (op->shape == SIMD_WITH_LANE && scalar_count == 1)) {
            y[0] = lane_bits(values[i], kind);
        } else {
            if(values[i].kind != VALUE_INT || constants == 32) {
                vm->failed = 1;
                return (Value){0};
            }
            k[constants++] = (int64_t)integer_bits(values[i]);
        }
        scalar_count++;
    }
    if(op->shape == SIMD_SELECT)
        first = select_value;
    if(first == NULL || !VectorType(first, &a)) {
        vm->failed = 1;
        return (Value){0};
    }
    int r_kind = vector_result ? SimdLaneKind(&result_vector) :
        !strcmp(expression->type, a.lane) ? SimdLaneKind(&a) : ZI_K_S64;
    int r_lanes = vector_result ? result_vector.lanes : 1;
    zi_ref(op->id, r_kind, r_lanes, SimdLaneKind(&a), a.lanes, x, y, z, k, out);
    if(vector_result)
        return make_vector(vm, frame->module, &result_vector, out);
    if(!strcmp(expression->type, "bool"))
        return int_value(out[0] != 0);
    if(!strcmp(expression->type, a.lane))
        return lane_value(vm, frame->module, out[0], &a);
    return coerce(vm, frame->module, int_value((int64_t)out[0]), expression->type);
}

Value
vm_simd_call(Frame *frame, const ZirExpr *expression, int depth)
{
    Vm *vm = frame->vm;
    const ZirFunction *function = frame->function;
    const SimdOp *op = SimdOpChecked(expression->name);
    int memory = op != NULL && (op->shape == SIMD_LOAD || op->shape == SIMD_STORE ||
                                op->shape == SIMD_GATHER);
    Value values[SIMD_OPERANDS];
    int count = 0;
    VmRoots roots = {vm->evaluation_roots, values, 0};
    vm->evaluation_roots = &roots;
    for(int child = expression->first_child; op != NULL && child >= 0 && !vm->failed;
        child = function->exprs[child].next_sibling) {
        if(count == SIMD_OPERANDS) {
            vm->failed = 1;
            break;
        }
        /* An array a load or store reaches is used in place, as indexing
         * does, so a store changes the caller's array. */
        Value *stored = NULL;
        const char *type = function->exprs[child].type;
        if(memory && count == 0 && type[0] == '[' && type[1] != ']')
            stored = assignment_slot(frame, child, depth + 1);
        values[count] = stored != NULL && stored->kind == VALUE_ARRAY ? *stored :
            vm->failed ? (Value){0} : eval(frame, child, depth + 1);
        roots.count = ++count;
    }
    Value result = {0};
    if(op == NULL)
        vm->failed = 1;
    else if(!vm->failed)
        result = memory ? memory_operation(frame, op, expression, values, count) :
            value_operation(frame, op, expression, values, count);
    vm->evaluation_roots = roots.previous;
    return result;
}
