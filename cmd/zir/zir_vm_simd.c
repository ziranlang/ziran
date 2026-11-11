/*
 * zir_vm_simd.c - std/simd operations on the portable VM.
 *
 * A vector of up to 128 bits is held inline in its Value (see VmVector);
 * a wider one is a fixed array of its lanes (see vm_array_element_type).
 * Each operation evaluates through zi_ref, the scalar reference in
 * include/ziran_simd.h that the C and Go backends match bit for bit.
 *
 * Each call site is prepared once: its operation, the part every operand
 * plays and the vector types it reads and makes. Executing it then parses
 * no names, and reads and writes the lanes of packed vectors directly.
 */
#include "zir_vm_internal.h"
#include "zir_simd.h"
#define ZIRAN_SIMD_REF_ONLY 1
#include "ziran_simd.h"

/* Make of u8x32 has the most operands: one per lane. */
enum { SIMD_OPERANDS = 32 };

/* One vector type as a site uses it. */
typedef struct SimdLanes {
    const char *lane; /* canonical lane type, as allocation names it */
    int kind, lanes;
    ArrayStorage storage;
    const VmVector *shape; /* an inline vector's, or NULL for an array */
} SimdLanes;

/* What an operand of a value operation supplies. */
enum {
    SIMD_PART_VECTOR,   /* a whole vector, into x, y or z */
    SIMD_PART_LANE,     /* one lane of Make's x */
    SIMD_PART_SPLAT,    /* the lane a Splat or WithLane puts in y[0] */
    SIMD_PART_CONSTANT  /* a shift count, lane index or shuffle pattern */
};

/* What a value operation returns. */
enum { SIMD_RESULT_VECTOR, SIMD_RESULT_BOOL, SIMD_RESULT_LANE, SIMD_RESULT_SCALAR };

struct VmSimdSite {
    const ZirExpr *expression;
    const SimdOp *op; /* NULL when checking left the call unable to run */
    int count, memory, in_place, result_kind, result_lanes, result_class, width;
    unsigned char parts[SIMD_OPERANDS], slots[SIMD_OPERANDS];
    /* Value operations: the operand vectors, the vector that owns the
     * lanes (a) and the result. Memory operations: result is the vector
     * loaded, stored or gathered, and vectors[0] a gather's indices. */
    SimdLanes vectors[3], a, result;
};

static ArrayStorage
lane_storage(int kind)
{
    static const ArrayStorage storage[] = {
        ARRAY_U8, ARRAY_S8, ARRAY_U16, ARRAY_S16, ARRAY_U32, ARRAY_S32,
        ARRAY_U64, ARRAY_S64, ARRAY_F32, ARRAY_F64
    };
    return kind >= 0 && kind < (int)(sizeof(storage) / sizeof(*storage)) ?
        storage[kind] : ARRAY_BOXED;
}

static int
site_lanes(const char *type, SimdLanes *lanes)
{
    ZirVector vector;
    if(!VectorType(type, &vector))
        return 0;
    lanes->lane = vector.lane;
    lanes->kind = SimdLaneKind(&vector);
    lanes->lanes = vector.lanes;
    lanes->storage = lane_storage(lanes->kind);
    lanes->shape = vm_vector(type);
    return lanes->kind >= 0;
}

static int
widest(int width, int lanes)
{
    return lanes > width ? lanes : width;
}

/* The checked facts of a load, store or gather. */
static int
prepare_memory(const ZirFunction *function, const ZirExpr *expression, VmSimdSite *site)
{
    int second = function->exprs[expression->first_child].next_sibling;
    if(site->op->shape == SIMD_GATHER)
        return site->count == 2 && site_lanes(expression->type, &site->result) &&
            site_lanes(function->exprs[second].type, &site->vectors[0]) &&
            site->vectors[0].lanes == site->result.lanes;
    int store = site->op->shape == SIMD_STORE;
    int third = second >= 0 ? function->exprs[second].next_sibling : -1;
    if(site->count != (store ? 3 : 2))
        return 0;
    return site_lanes(store ? function->exprs[third].type : expression->type, &site->result);
}

/* The checked facts of every other operation, in the order the operands
 * arrive: Make, Splat and Zero are named for their result; Select for its
 * values, after the mask; everything else for its first vector. */
static int
prepare_value(const ZirFunction *function, const ZirExpr *expression, VmSimdSite *site)
{
    const SimdOp *op = site->op;
    const char *first = NULL, *select_value = NULL;
    int vector_count = 0, scalar_count = 0, constants = 0;
    if(op->shape == SIMD_MAKE || op->shape == SIMD_SPLAT || op->shape == SIMD_ZERO)
        first = expression->type;
    int child = expression->first_child;
    for(int i = 0; i < site->count; i++, child = function->exprs[child].next_sibling) {
        const char *type = function->exprs[child].type;
        ZirVector vector;
        if(VectorType(type, &vector)) {
            if(vector_count == 3 || !site_lanes(type, &site->vectors[vector_count]))
                return 0;
            site->width = widest(site->width, vector.lanes);
            if(vector_count == 1 && op->shape == SIMD_SELECT)
                select_value = type;
            if(first == NULL && op->shape != SIMD_SELECT)
                first = type;
            site->parts[i] = SIMD_PART_VECTOR;
            site->slots[i] = (unsigned char)vector_count++;
            continue;
        }
        /* Scalars take their lane type from the vector named so far. */
        if(first == NULL || op->shape == SIMD_SELECT || !site_lanes(first, &site->a))
            return 0;
        if(op->shape == SIMD_MAKE) {
            if(scalar_count >= site->a.lanes)
                return 0;
            site->parts[i] = SIMD_PART_LANE;
            site->slots[i] = (unsigned char)scalar_count;
        } else if(op->shape == SIMD_SPLAT ||
                  (op->shape == SIMD_WITH_LANE && scalar_count == 1)) {
            site->parts[i] = SIMD_PART_SPLAT;
        } else {
            if(constants == SIMD_OPERANDS)
                return 0;
            site->parts[i] = SIMD_PART_CONSTANT;
            site->slots[i] = (unsigned char)constants++;
        }
        scalar_count++;
    }
    if(op->shape == SIMD_SELECT)
        first = select_value;
    if(first == NULL || !site_lanes(first, &site->a))
        return 0;
    if(site_lanes(expression->type, &site->result)) {
        site->result_class = SIMD_RESULT_VECTOR;
        site->result_kind = site->result.kind;
        site->result_lanes = site->result.lanes;
    } else {
        int lane = !strcmp(expression->type, site->a.lane);
        site->result_class = !strcmp(expression->type, "bool") ? SIMD_RESULT_BOOL :
            lane ? SIMD_RESULT_LANE : SIMD_RESULT_SCALAR;
        site->result_kind = lane ? site->a.kind : ZI_K_S64;
        site->result_lanes = 1;
    }
    site->width = widest(widest(widest(site->width, site->a.lanes),
                                site->result_lanes), constants);
    return 1;
}

static void
prepare_site(const ZirFunction *function, const ZirExpr *expression, VmSimdSite *site)
{
    const SimdOp *op = SimdOpChecked(expression->name);
    *site = (VmSimdSite){.expression = expression};
    if(op == NULL)
        return;
    site->op = op;
    site->memory = op->shape == SIMD_LOAD || op->shape == SIMD_STORE ||
        op->shape == SIMD_GATHER;
    for(int child = expression->first_child; child >= 0;
        child = function->exprs[child].next_sibling)
        if(++site->count > SIMD_OPERANDS) {
            site->op = NULL;
            return;
        }
    if(op->shape == SIMD_COUNT)
        return;
    if(site->count == 0) {
        if(site->memory || !prepare_value(function, expression, site))
            site->op = NULL;
        return;
    }
    /* An array a load or store reaches is used in place, as indexing
     * does, so a store changes the caller's array. */
    const char *type = function->exprs[expression->first_child].type;
    site->in_place = site->memory && type[0] == '[' && type[1] != ']';
    if(!(site->memory ? prepare_memory : prepare_value)(function, expression, site))
        site->op = NULL;
}

/* Sites are hashed by expression and kept apart from the table, so a
 * table grown by a nested call leaves the site being run in place. */
static const VmSimdSite *
simd_site(Frame *frame, const ZirExpr *expression)
{
    Vm *vm = frame->vm;
    if(vm->simd_site_count * 2 >= vm->simd_site_slots) {
        size_t slots = vm->simd_site_slots ? vm->simd_site_slots * 2 : 256;
        VmSimdSite **sites = calloc(slots, sizeof(*sites));
        if(sites == NULL)
            return NULL;
        for(size_t i = 0; i < vm->simd_site_slots; i++) {
            VmSimdSite *prior = vm->simd_sites[i];
            if(prior == NULL) continue;
            size_t position = vm_expression_slot(prior->expression, slots);
            while(sites[position] != NULL)
                position = (position + 1) & (slots - 1);
            sites[position] = prior;
        }
        free(vm->simd_sites);
        vm->simd_sites = sites;
        vm->simd_site_slots = slots;
    }
    size_t position = vm_expression_slot(expression, vm->simd_site_slots);
    while(vm->simd_sites[position] != NULL &&
          vm->simd_sites[position]->expression != expression)
        position = (position + 1) & (vm->simd_site_slots - 1);
    if(vm->simd_sites[position] != NULL)
        return vm->simd_sites[position];
    VmSimdSite *site = malloc(sizeof(*site));
    if(site == NULL)
        return NULL;
    prepare_site(frame->function, expression, site);
    vm->simd_sites[position] = site;
    vm->simd_site_count++;
    return site;
}

void
vm_simd_free_sites(Vm *vm)
{
    for(size_t i = 0; i < vm->simd_site_slots; i++)
        free(vm->simd_sites[i]);
    free(vm->simd_sites);
    vm->simd_sites = NULL;
    vm->simd_site_count = vm->simd_site_slots = 0;
}

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

/* The value a lane holds, as coercing its bits to the lane type makes it. */
static Value
lane_value(int kind, uint64_t bits)
{
    switch(kind) {
    case ZI_K_F32: {
        uint32_t narrow = (uint32_t)bits;
        float number;
        memcpy(&number, &narrow, sizeof(number));
        return real_value(number);
    }
    case ZI_K_F64: {
        double number;
        memcpy(&number, &bits, sizeof(number));
        return real_value(number);
    }
    case ZI_K_U64:
        return uint_value(bits);
    default:
        return int_value(zi_ksigned(kind) ? zi_sx(bits, kind) :
                         (int64_t)(bits & zi_kmask(kind)));
    }
}

/* Whether packed elements from..from+count-1 all hold a value. */
static int
packed_valid(const Array *array, size_t from, int count)
{
    const unsigned char *valid = array->data + array->data_bytes;
    for(size_t i = from; i < from + (size_t)count; i++)
        if(!(valid[i / 8] & (1u << (i % 8))))
            return 0;
    return 1;
}

/* Copies count packed elements of the vector's lane type, zero-extended. */
static void
packed_read(const unsigned char *data, ArrayStorage storage, int count, uint64_t *lanes)
{
    switch(storage) {
    case ARRAY_U8: case ARRAY_S8:
        for(int i = 0; i < count; i++) lanes[i] = data[i];
        return;
    case ARRAY_U16: case ARRAY_S16:
        for(int i = 0; i < count; i++) {
            uint16_t lane;
            memcpy(&lane, data + 2 * i, sizeof(lane));
            lanes[i] = lane;
        }
        return;
    case ARRAY_U32: case ARRAY_S32: case ARRAY_F32:
        for(int i = 0; i < count; i++) {
            uint32_t lane;
            memcpy(&lane, data + 4 * i, sizeof(lane));
            lanes[i] = lane;
        }
        return;
    default:
        memcpy(lanes, data, (size_t)count * sizeof(*lanes));
        return;
    }
}

/* Stores count lanes as array_set stores their values: a float32 lane
 * passes through the float64 a VM real holds. */
static void
packed_write(unsigned char *data, ArrayStorage storage, int count, const uint64_t *lanes)
{
    switch(storage) {
    case ARRAY_U8: case ARRAY_S8:
        for(int i = 0; i < count; i++) data[i] = (unsigned char)lanes[i];
        return;
    case ARRAY_U16: case ARRAY_S16:
        for(int i = 0; i < count; i++) {
            uint16_t lane = (uint16_t)lanes[i];
            memcpy(data + 2 * i, &lane, sizeof(lane));
        }
        return;
    case ARRAY_U32: case ARRAY_S32:
        for(int i = 0; i < count; i++) {
            uint32_t lane = (uint32_t)lanes[i];
            memcpy(data + 4 * i, &lane, sizeof(lane));
        }
        return;
    case ARRAY_F32:
        for(int i = 0; i < count; i++) {
            uint32_t bits = (uint32_t)lanes[i];
            float number;
            memcpy(&number, &bits, sizeof(number));
            double real = number;
            number = (float)real;
            memcpy(data + 4 * i, &number, sizeof(number));
        }
        return;
    default:
        memcpy(data, lanes, (size_t)count * sizeof(*lanes));
        return;
    }
}

static void
mark_valid(Array *array, size_t from, int count)
{
    unsigned char *valid = array->data + array->data_bytes;
    for(size_t i = from; i < from + (size_t)count; i++)
        valid[i / 8] |= (unsigned char)(1u << (i % 8));
}

static size_t
packed_width(ArrayStorage storage)
{
    switch(storage) {
    case ARRAY_U8: case ARRAY_S8: return 1;
    case ARRAY_U16: case ARRAY_S16: return 2;
    case ARRAY_U32: case ARRAY_S32: case ARRAY_F32: return 4;
    default: return 8;
    }
}

/* Inline vectors keep float32 lanes as their exact bits, as C does. */
static ArrayStorage
inline_storage(ArrayStorage storage)
{
    return storage == ARRAY_F32 ? ARRAY_U32 : storage;
}

static int
read_vector(Value value, const SimdLanes *vector, uint64_t *lanes)
{
    if(vector->shape != NULL) {
        if(value.kind != VALUE_VECTOR || value.vector != vector->shape)
            return 0;
        packed_read(vector_bytes(&value), vector->storage, vector->lanes, lanes);
        return 1;
    }
    if(value.kind != VALUE_ARRAY || value.array == NULL ||
       value.array->length != vector->lanes)
        return 0;
    const Array *array = value.array;
    if(array->storage == vector->storage && packed_valid(array, 0, vector->lanes)) {
        packed_read(array->data, vector->storage, vector->lanes, lanes);
        return 1;
    }
    for(int i = 0; i < vector->lanes; i++)
        lanes[i] = lane_bits(array_get(array, (size_t)i), vector->kind);
    return 1;
}

static Value
make_vector(Vm *vm, const ZirModule *module, const SimdLanes *vector,
            const uint64_t *lanes)
{
    if(vector->shape != NULL) {
        Value result = vector_zero(vector->shape);
        packed_write(vector_bytes(&result), inline_storage(vector->storage),
                     vector->lanes, lanes);
        return result;
    }
    Array *array = allocate_array_try(vm, module, vector->lane, vector->lanes, 1);
    if(array == NULL)
        return (Value){0};
    if(array->storage == vector->storage) {
        packed_write(array->data, vector->storage, vector->lanes, lanes);
        mark_valid(array, 0, vector->lanes);
    } else {
        for(int i = 0; i < vector->lanes; i++)
            array_set(array, (size_t)i, lane_value(vector->kind, lanes[i]));
    }
    return (Value){.kind = VALUE_ARRAY, .array = array};
}

/* The elements a load, store or gather reaches: a fixed array, a slice, or
 * from a pointer to the end of the storage it points into. */
typedef struct SimdMemorySpan {
    Array *array;
    Value *single; /* a pointer at one value outside any array */
    const unsigned char *text; /* immutable string bytes, only for loads */
    size_t offset, length;
} SimdMemorySpan;

static int
memory_span(Vm *vm, Value base, SimdMemorySpan *span)
{
    memset(span, 0, sizeof(*span));
    if(base.kind == VALUE_STRING) {
        span->text = base.data;
        span->length = base.length;
        return 1;
    }
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
    if(span->text != NULL) return int_value(span->text[index]);
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
memory_operation(Frame *frame, const VmSimdSite *site, const Value *values)
{
    Vm *vm = frame->vm;
    const SimdLanes *vector = &site->result;
    SimdMemorySpan span;
    uint64_t lanes[32] = {0};
    if(!memory_span(vm, values[0], &span)) {
        vm->failed = 1;
        return (Value){0};
    }
    if(site->op->shape == SIMD_GATHER) {
        const SimdLanes *index_vector = &site->vectors[0];
        if(!read_vector(values[1], index_vector, lanes)) {
            vm->failed = 1;
            return (Value){0};
        }
        /* Every index is checked before any element is read, as in C. */
        int64_t at[32];
        for(int i = 0; i < vector->lanes; i++) {
            at[i] = zi_ksigned(index_vector->kind) ? zi_sx(lanes[i], index_vector->kind) :
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
        for(int i = 0; i < vector->lanes; i++)
            lanes[i] = lane_bits(span_get(&span, (size_t)at[i]), vector->kind);
        return make_vector(vm, frame->module, vector, lanes);
    }
    int store = site->op->shape == SIMD_STORE;
    if(store && values[0].kind == VALUE_STRING) {
        vm->failed = 1;
        return (Value){0};
    }
    if(values[1].kind != VALUE_INT) {
        vm->failed = 1;
        return (Value){0};
    }
    int low = site->op->id == ZI_OP_LOAD_LOW || site->op->id == ZI_OP_STORE_LOW;
    int reach = low ? 64 / zi_kbits(vector->kind) : vector->lanes;
    int64_t at = index_operand(values[1]);
    if(!access_fits(frame, &span, at, reach))
        return (Value){0};
    /* Packed storage of the lane type is read and written in place. */
    Array *array = span.array;
    size_t first = span.offset + (size_t)at;
    int packed = span.single == NULL && array != NULL && array->storage == vector->storage;
    unsigned char *data = packed ? array->data + first * packed_width(vector->storage) : NULL;
    if(store) {
        if(!read_vector(values[2], vector, lanes)) {
            vm->failed = 1;
            return (Value){0};
        }
        if(packed) {
            packed_write(data, vector->storage, reach, lanes);
            mark_valid(array, first, reach);
        } else {
            for(int i = 0; i < reach; i++)
                span_set(&span, (size_t)at + (size_t)i, lane_value(vector->kind, lanes[i]));
        }
        return (Value){.kind = VALUE_VOID};
    }
    if(packed && packed_valid(array, first, reach))
        packed_read(data, vector->storage, reach, lanes);
    else
        for(int i = 0; i < reach; i++)
            lanes[i] = lane_bits(span_get(&span, (size_t)at + (size_t)i), vector->kind);
    return make_vector(vm, frame->module, vector, lanes);
}

static Value
value_operation(Frame *frame, const VmSimdSite *site, const Value *values)
{
    Vm *vm = frame->vm;
    uint64_t x[32], y[32], z[32], out[32];
    uint64_t *vectors[3] = {x, y, z};
    int64_t k[32];
    /* zi_ref reads no lane past the widest vector the site names. */
    size_t width = (size_t)site->width;
    memset(x, 0, width * sizeof(*x));
    memset(y, 0, width * sizeof(*y));
    memset(z, 0, width * sizeof(*z));
    memset(out, 0, width * sizeof(*out));
    memset(k, 0, width * sizeof(*k));
    for(int i = 0; i < site->count; i++) {
        int slot = site->slots[i];
        switch(site->parts[i]) {
        case SIMD_PART_VECTOR:
            if(!read_vector(values[i], &site->vectors[slot], vectors[slot])) {
                vm->failed = 1;
                return (Value){0};
            }
            break;
        case SIMD_PART_LANE:
            x[slot] = lane_bits(values[i], site->a.kind);
            break;
        case SIMD_PART_SPLAT:
            y[0] = lane_bits(values[i], site->a.kind);
            break;
        default:
            if(values[i].kind != VALUE_INT) {
                vm->failed = 1;
                return (Value){0};
            }
            k[slot] = (int64_t)integer_bits(values[i]);
            break;
        }
    }
    zi_ref(site->op->id, site->result_kind, site->result_lanes, site->a.kind,
           site->a.lanes, x, y, z, k, out);
    switch(site->result_class) {
    case SIMD_RESULT_VECTOR:
        return make_vector(vm, frame->module, &site->result, out);
    case SIMD_RESULT_BOOL:
        return int_value(out[0] != 0);
    case SIMD_RESULT_LANE:
        return lane_value(site->a.kind, out[0]);
    default:
        return coerce(vm, frame->module, int_value((int64_t)out[0]),
                      site->expression->type);
    }
}

Value
vm_simd_call(Frame *frame, const ZirExpr *expression, int depth)
{
    Vm *vm = frame->vm;
    const ZirFunction *function = frame->function;
    const VmSimdSite *site = simd_site(frame, expression);
    if(site == NULL || site->op == NULL) {
        vm->failed = 1;
        return (Value){0};
    }
    Value values[SIMD_OPERANDS];
    int count = 0;
    VmRoots roots = {vm->evaluation_roots, values, 0};
    vm->evaluation_roots = &roots;
    for(int child = expression->first_child; child >= 0 && !vm->failed;
        child = function->exprs[child].next_sibling) {
        Value *stored = count == 0 && site->in_place ?
            assignment_slot(frame, child, depth + 1) : NULL;
        values[count] = stored != NULL && stored->kind == VALUE_ARRAY ? *stored :
            vm->failed ? (Value){0} : eval(frame, child, depth + 1);
        roots.count = ++count;
    }
    Value result = {0};
    /* A native vector here is one block: Lanes is its lanes, Blocks 1. */
    if(!vm->failed && site->op->shape == SIMD_COUNT)
        result = int_value(count == 1 ? (int64_t)integer_bits(values[0]) : 1);
    else if(!vm->failed)
        result = site->memory ? memory_operation(frame, site, values) :
            value_operation(frame, site, values);
    vm->evaluation_roots = roots.previous;
    return result;
}
