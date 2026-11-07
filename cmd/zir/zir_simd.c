#include "zir_simd.h"
#include "zir_check.h"
#include "zir_scalar.h"
#define ZIRAN_SIMD_REF_ONLY 1
#include "ziran_simd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    U8 = 1 << ZI_K_U8, S8 = 1 << ZI_K_S8, U16 = 1 << ZI_K_U16, S16 = 1 << ZI_K_S16,
    U32 = 1 << ZI_K_U32, S32 = 1 << ZI_K_S32, U64 = 1 << ZI_K_U64, S64 = 1 << ZI_K_S64,
    F32 = 1 << ZI_K_F32, F64 = 1 << ZI_K_F64,
    INT = U8 | S8 | U16 | S16 | U32 | S32 | U64 | S64, FLOAT = F32 | F64,
    ANY = INT | FLOAT, SMALL = U8 | S8 | U16 | S16, NARROWABLE = U8 | S8 | U16 | S16 | U32 | S32
};

#define OP(id, name, snake, shape, lanes, typed) \
    {ZI_OP_##id, name, snake, "zi_simd_" snake, shape, lanes, typed}
static const SimdOp ops[] = {
    OP(ADD, "Add", "add", SIMD_BINARY, ANY, 0),
    OP(SUB, "Sub", "sub", SIMD_BINARY, ANY, 0),
    OP(MUL, "Mul", "mul", SIMD_BINARY, ANY, 0),
    OP(DIV, "Div", "div", SIMD_BINARY, FLOAT, 0),
    OP(ADD_SAT, "AddSat", "add_sat", SIMD_BINARY, SMALL, 0),
    OP(SUB_SAT, "SubSat", "sub_sat", SIMD_BINARY, SMALL, 0),
    OP(MIN, "Min", "min", SIMD_BINARY, ANY, 0),
    OP(MAX, "Max", "max", SIMD_BINARY, ANY, 0),
    OP(AVG, "Average", "avg", SIMD_BINARY, U8 | U16, 0),
    OP(MUL_HIGH, "MulHigh", "mul_high", SIMD_BINARY, U16 | S16, 0),
    OP(MUL_HIGH_ROUND, "MulHighRound", "mul_high_round", SIMD_BINARY, S16, 0),
    OP(AND, "And", "and", SIMD_BINARY, INT, 0),
    OP(OR, "Or", "or", SIMD_BINARY, INT, 0),
    OP(XOR, "Xor", "xor", SIMD_BINARY, INT, 0),
    OP(AND_NOT, "AndNot", "and_not", SIMD_BINARY, INT, 0),
    OP(NOT, "Not", "not", SIMD_UNARY, INT, 0),
    OP(ABS, "Abs", "abs", SIMD_UNARY, S8 | S16 | S32 | S64 | FLOAT, 0),
    OP(NEG, "Neg", "neg", SIMD_UNARY, ANY, 0),
    OP(SIGN, "Sign", "sign", SIMD_BINARY, S8 | S16 | S32 | S64, 0),
    OP(ABS_DIFF, "AbsDiff", "abs_diff", SIMD_BINARY, INT, 0),
    OP(EQ, "Equal", "eq", SIMD_COMPARE, ANY, 0),
    OP(LT, "Less", "lt", SIMD_COMPARE, ANY, 0),
    OP(GT, "Greater", "gt", SIMD_COMPARE, ANY, 0),
    OP(SELECT, "Select", "select", SIMD_SELECT, ANY, 0),
    OP(SHL, "ShiftLeft", "shl", SIMD_SHIFT, INT, 0),
    OP(SHR, "ShiftRight", "shr", SIMD_SHIFT, INT, 0),
    OP(SHR_ROUND, "ShiftRightRound", "shr_round", SIMD_SHIFT, INT, 0),
    OP(SHL_LANES, "ShiftLeftLanes", "shl_lanes", SIMD_SHIFT_LANES, INT, 0),
    OP(SHR_LANES, "ShiftRightLanes", "shr_lanes", SIMD_SHIFT_LANES, INT, 0),
    OP(SHL_BYTES, "ShiftBytesLeft", "shl_bytes", SIMD_BYTE_SHIFT, INT, 0),
    OP(SHR_BYTES, "ShiftBytesRight", "shr_bytes", SIMD_BYTE_SHIFT, INT, 0),
    OP(ALIGN_BYTES, "AlignBytes", "align_bytes", SIMD_ALIGN, INT, 0),
    OP(MAKE, "Make", "make", SIMD_MAKE, ANY, 1),
    OP(SPLAT, "Splat", "splat", SIMD_SPLAT, ANY, 1),
    OP(ZERO, "Zero", "zero", SIMD_ZERO, ANY, 1),
    OP(LANE, "Lane", "lane", SIMD_LANE, ANY, 0),
    OP(WITH_LANE, "WithLane", "with_lane", SIMD_WITH_LANE, ANY, 0),
    OP(LOAD, "Load", "load", SIMD_LOAD, ANY, 1),
    OP(LOAD_LOW, "LoadLow", "load_low", SIMD_LOAD, ANY, 1),
    OP(STORE, "Store", "store", SIMD_STORE, ANY, 0),
    OP(STORE_LOW, "StoreLow", "store_low", SIMD_STORE, ANY, 0),
    OP(GATHER, "Gather", "gather", SIMD_GATHER, U32 | S32, 0),
    OP(WIDEN_LOW, "WidenLow", "widen_low", SIMD_WIDEN_HALF, NARROWABLE, 0),
    OP(WIDEN_HIGH, "WidenHigh", "widen_high", SIMD_WIDEN_HALF, NARROWABLE, 0),
    OP(WIDEN, "Widen", "widen", SIMD_WIDEN, NARROWABLE, 0),
    OP(NARROW, "Narrow", "narrow", SIMD_NARROW, U16 | S16 | U32 | S32 | U64 | S64, 1),
    OP(MUL_WIDE_LOW, "MulWideLow", "mul_wide_low", SIMD_MUL_WIDE, NARROWABLE, 0),
    OP(MUL_WIDE_HIGH, "MulWideHigh", "mul_wide_high", SIMD_MUL_WIDE, NARROWABLE, 0),
    OP(MADD, "MulAddPairs", "madd", SIMD_MADD, S16, 0),
    OP(MADD_SAT, "MulAddPairsSat", "madd_sat", SIMD_MADD_SAT, U8, 0),
    OP(ADD_PAIRS, "AddPairs", "add_pairs", SIMD_ADD_PAIRS, NARROWABLE, 0),
    OP(SAD, "SumAbsDiff", "sad", SIMD_SAD, U8, 0),
    OP(REDUCE_ADD, "ReduceAdd", "reduce_add", SIMD_REDUCE, INT, 0),
    OP(REDUCE_MIN, "ReduceMin", "reduce_min", SIMD_REDUCE, INT, 0),
    OP(REDUCE_MAX, "ReduceMax", "reduce_max", SIMD_REDUCE, INT, 0),
    OP(REDUCE_MAX_INDEX, "ReduceMaxIndex", "reduce_max_index", SIMD_REDUCE, INT, 0),
    OP(ALL_ZERO, "AllZero", "all_zero", SIMD_ALL_ZERO, INT, 0),
    OP(BYTE_SHUFFLE, "ByteShuffle", "byte_shuffle", SIMD_BYTE_SHUFFLE, U8 | S8, 0),
    OP(SHUFFLE, "Shuffle", "shuffle", SIMD_SHUFFLE, ANY, 0),
    OP(REINTERPRET, "Reinterpret", "reinterpret", SIMD_REINTERPRET, ANY, 1),
    OP(CONVERT, "Convert", "convert", SIMD_CONVERT, ANY, 1),
    OP(LOW, "Low", "low", SIMD_HALF, ANY, 0),
    OP(HIGH, "High", "high", SIMD_HALF, ANY, 0),
    OP(COMBINE, "Combine", "combine", SIMD_COMBINE, ANY, 0),
    OP(INTERLEAVE_LOW, "InterleaveLow", "interleave_low", SIMD_BINARY, ANY, 0),
    OP(INTERLEAVE_HIGH, "InterleaveHigh", "interleave_high", SIMD_BINARY, ANY, 0),
};
#undef OP
#define OP_COUNT (sizeof(ops) / sizeof(*ops))

int
SimdLaneKind(const ZirVector *vector)
{
    static const char *const names[] = {
        "u8", "s8", "u16", "s16", "u32", "s32", "u64", "s64", "float32", "float64"
    };
    for(int i = 0; i < 10; i++)
        if(!strcmp(vector->lane, names[i])) return i;
    return -1;
}

const SimdOp *
SimdOpAt(int index)
{
    return index >= 0 && (size_t)index < OP_COUNT ? &ops[index] : NULL;
}

const SimdOp *
SimdOpNamed(const char *name)
{
    for(size_t i = 0; name != NULL && i < OP_COUNT; i++)
        if(!strcmp(name, ops[i].name)) return &ops[i];
    return NULL;
}

const SimdOp *
SimdOpChecked(const char *name)
{
    if(name == NULL || strncmp(name, "zi_simd_", 8)) return NULL;
    /* The VM asks this of every call it evaluates: compare first bytes. */
    name += 8;
    for(size_t i = 0; i < OP_COUNT; i++)
        if(ops[i].snake[0] == name[0] && !strcmp(name, ops[i].snake)) return &ops[i];
    return NULL;
}

/* std/simd however it was imported: as "std/simd" or "simd", under any
 * alias, and whatever identity a project or --module-path std=DIR gave it. */
int
SimdModule(const ZirModule *module)
{
    return module != NULL && !strcmp(module->standard_name, "simd");
}

int
SimdBlockLanes(const ZirVector *vector)
{
    return vector->bytes > 16 ? vector->lanes / 2 : vector->lanes;
}

int
SimdMemory(const char *type, char *element, size_t size, int *capacity)
{
    char inner[ZIR_NAME_MAX];
    int kind = SIMD_MEMORY_NONE, count = -1;
    if(type == NULL) return 0;
    if(SliceElementType(type, inner, sizeof(inner))) kind = SIMD_MEMORY_SLICE;
    else if(ArrayElementType(type, inner, sizeof(inner), &count)) kind = SIMD_MEMORY_ARRAY;
    else if(type[0] == '*' && strlen(type + 1) < sizeof(inner)) {
        kind = SIMD_MEMORY_POINTER;
        strcpy(inner, type + 1);
    } else return SIMD_MEMORY_NONE;
    const char *scalar = ScalarType(inner);
    if(element != NULL) snprintf(element, size, "%s", *scalar ? scalar : inner);
    if(capacity != NULL) *capacity = count;
    return kind;
}

static int
integer_scalar(const char *type)
{
    if(!strcmp(type, "integer")) return 1;
    const char *scalar = ScalarType(type);
    return ScalarWidth(scalar) > 0 || !strcmp(scalar, "isize") || !strcmp(scalar, "usize");
}

/* A scalar operand that converts to a lane of `vector` without loss. */
static int
lane_operand(const ZirVector *vector, const SimdOperand *operand)
{
    const char *type = operand->type, *scalar = ScalarType(type);
    if(vector->is_float)
        return !strcmp(type, "real") || !strcmp(type, "integer") ||
               !strcmp(scalar, vector->lane) ||
               (!strcmp(scalar, "float32") && !strcmp(vector->lane, "float64"));
    if(!strcmp(type, "integer")) {
        if(!operand->is_constant) return 1;
        if(vector->lane_bits == 64) return vector->is_signed || operand->value >= 0;
        int64_t high = vector->is_signed ? (INT64_C(1) << (vector->lane_bits - 1)) - 1 :
            (INT64_C(1) << vector->lane_bits) - 1;
        int64_t low = vector->is_signed ? -high - 1 : 0;
        return operand->value >= low && operand->value <= high;
    }
    unsigned width = *scalar ? ScalarWidth(scalar) : 0;
    if(!width) return 0;
    if(!strcmp(scalar, vector->lane)) return 1;
    return (int)width < vector->lane_bits && (scalar[0] == 'u' || vector->is_signed);
}

static const char *
wider(const ZirVector *vector, int lanes)
{
    char lane[8];
    snprintf(lane, sizeof(lane), "%c%d", vector->is_signed ? 's' : 'u', vector->lane_bits * 2);
    return VectorTypeName(lane, lanes);
}

static int
vector_of(const SimdOp *op, const SimdOperand *operand, ZirVector *vector)
{
    if(!VectorType(operand->type, vector)) return 0;
    int kind = SimdLaneKind(vector);
    return kind >= 0 && (op->lanes & (1u << kind));
}

static const char *
fail(const char **message, const char *text)
{
    *message = text;
    return NULL;
}

const char *
SimdSignature(const SimdOp *op, const char *result, const SimdOperand *operands,
              int count, const char **message)
{
    ZirVector a, b, r;
    char element[ZIR_NAME_MAX];
    static const char unsupported[] = "does not take this vector type";
    *message = "";
    if(op->type_argument && (result == NULL || !VectorType(result, &r)))
        return fail(message, "takes a vector type first, such as s16x8");
    switch(op->shape) {
    case SIMD_BINARY:
    case SIMD_COMPARE:
        if(count != 2) return fail(message, "takes two vectors of one type");
        if(!vector_of(op, &operands[0], &a)) return fail(message, unsupported);
        if(strcmp(operands[0].type, operands[1].type))
            return fail(message, "takes two vectors of one type");
        if((op->shape == SIMD_COMPARE && a.is_float) || op->id == ZI_OP_ABS_DIFF) {
            char lane[8];
            snprintf(lane, sizeof(lane), "u%d", a.lane_bits);
            return VectorTypeName(lane, a.lanes);
        }
        return a.name;
    case SIMD_UNARY:
        if(count != 1) return fail(message, "takes one vector");
        if(!vector_of(op, &operands[0], &a)) return fail(message, unsupported);
        return a.name;
    case SIMD_SELECT:
        if(count != 3) return fail(message, "takes a mask and two vectors of one type");
        if(!VectorType(operands[0].type, &b) || b.is_float ||
           !vector_of(op, &operands[1], &a) || strcmp(operands[1].type, operands[2].type) ||
           b.lanes != a.lanes || b.lane_bits != a.lane_bits)
            return fail(message, "takes an integer mask with the vectors' lane width, then two vectors of one type");
        return a.name;
    case SIMD_SHIFT:
        if(count != 2 || !vector_of(op, &operands[0], &a) || !integer_scalar(operands[1].type))
            return fail(message, "takes an integer vector and an integer count");
        return a.name;
    case SIMD_SHIFT_LANES:
        if(count != 2 || !vector_of(op, &operands[0], &a) || strcmp(operands[0].type, operands[1].type))
            return fail(message, "takes an integer vector and a vector of counts of the same type");
        return a.name;
    case SIMD_BYTE_SHIFT:
        if(count != 2 || !vector_of(op, &operands[0], &a))
            return fail(message, "takes an integer vector and a constant byte count");
        if(!operands[1].is_constant || operands[1].value < 0 || operands[1].value > 16)
            return fail(message, "takes a constant byte count from 0 to 16");
        return a.name;
    case SIMD_ALIGN:
        if(count != 3 || !vector_of(op, &operands[0], &a) || strcmp(operands[0].type, operands[1].type))
            return fail(message, "takes two integer vectors of one type and a constant byte count");
        if(!operands[2].is_constant || operands[2].value < 0 || operands[2].value > 16)
            return fail(message, "takes a constant byte count from 0 to 16");
        return a.name;
    case SIMD_MAKE: {
        int kind = SimdLaneKind(&r);
        if(!(op->lanes & (1u << kind))) return fail(message, unsupported);
        if(count != r.lanes) return fail(message, "takes the vector type and one value for each lane");
        for(int i = 0; i < count; i++)
            if(!lane_operand(&r, &operands[i])) return fail(message, "takes lane values of the vector's lane type");
        return r.name;
    }
    case SIMD_SPLAT:
        if(count != 1 || !lane_operand(&r, &operands[0]))
            return fail(message, "takes the vector type and one value of its lane type");
        return r.name;
    case SIMD_ZERO:
        if(count != 0) return fail(message, "takes only the vector type");
        return r.name;
    case SIMD_LANE:
        if(count != 2 || !vector_of(op, &operands[0], &a))
            return fail(message, "takes a vector and a constant lane index");
        if(!operands[1].is_constant || operands[1].value < 0 || operands[1].value >= a.lanes)
            return fail(message, "takes a constant lane index below the lane count");
        return a.lane;
    case SIMD_WITH_LANE:
        if(count != 3 || !vector_of(op, &operands[0], &a))
            return fail(message, "takes a vector, a constant lane index and a lane value");
        if(!operands[1].is_constant || operands[1].value < 0 || operands[1].value >= a.lanes)
            return fail(message, "takes a constant lane index below the lane count");
        if(!lane_operand(&a, &operands[2])) return fail(message, "takes a lane value of the vector's lane type");
        return a.name;
    case SIMD_LOAD:
        if(count != 2 || !SimdMemory(operands[0].type, element, sizeof(element), NULL) ||
           strcmp(element, r.lane) || !integer_scalar(operands[1].type))
            return fail(message, "takes the vector type, a slice, array or pointer of its lane type, and an index");
        return r.name;
    case SIMD_STORE: {
        int memory = count == 3 ? SimdMemory(operands[0].type, element, sizeof(element), NULL) : 0;
        if((memory != SIMD_MEMORY_SLICE && memory != SIMD_MEMORY_POINTER) ||
           !integer_scalar(operands[1].type) || !VectorType(operands[2].type, &a) ||
           strcmp(element, a.lane))
            return fail(message, "takes a slice or pointer of the vector's lane type, an index and the vector");
        return "void";
    }
    case SIMD_GATHER:
        if(count != 2 || !SimdMemory(operands[0].type, element, sizeof(element), NULL) ||
           !vector_of(op, &operands[1], &a) || a.lane_bits != 32 ||
           (strcmp(element, "s32") && strcmp(element, "u32") && strcmp(element, "float32")))
            return fail(message, "takes a table of s32, u32 or float32 and a vector of s32 or u32 indices");
        return VectorTypeName(element, a.lanes);
    case SIMD_WIDEN_HALF:
    case SIMD_MUL_WIDE:
    case SIMD_ADD_PAIRS:
        if(count != (op->shape == SIMD_MUL_WIDE ? 2 : 1) || !vector_of(op, &operands[0], &a) ||
           (count == 2 && strcmp(operands[0].type, operands[1].type)))
            return fail(message, op->shape == SIMD_MUL_WIDE ?
                "takes two integer vectors of one type with lanes up to 32 bits" :
                "takes an integer vector with lanes up to 32 bits");
        return wider(&a, a.lanes / 2);
    case SIMD_WIDEN:
        if(count != 1 || !vector_of(op, &operands[0], &a) || a.bytes != 16)
            return fail(message, "takes a 128-bit integer vector with lanes up to 32 bits");
        return wider(&a, a.lanes);
    case SIMD_NARROW:
        if(count != 2 || !vector_of(op, &operands[0], &a) || strcmp(operands[0].type, operands[1].type) ||
           r.is_float || r.lane_bits * 2 != a.lane_bits || r.lanes != a.lanes * 2)
            return fail(message, "takes the narrower result type and two integer vectors of one type");
        return r.name;
    case SIMD_MADD:
        if(count != 2 || !vector_of(op, &operands[0], &a) || strcmp(operands[0].type, operands[1].type))
            return fail(message, "takes two s16 vectors of one type");
        return VectorTypeName("s32", a.lanes / 2);
    case SIMD_MADD_SAT:
        if(count != 2 || !vector_of(op, &operands[0], &a) || !VectorType(operands[1].type, &b) ||
           strcmp(b.lane, "s8") || b.lanes != a.lanes)
            return fail(message, "takes a u8 vector and an s8 vector of the same width");
        return VectorTypeName("s16", a.lanes / 2);
    case SIMD_SAD:
        if(count != 2 || !vector_of(op, &operands[0], &a) || strcmp(operands[0].type, operands[1].type))
            return fail(message, "takes two u8 vectors of one type");
        return VectorTypeName("u64", a.lanes / 8);
    case SIMD_REDUCE:
        if(count != 1 || !vector_of(op, &operands[0], &a)) return fail(message, "takes one integer vector");
        return op->id == ZI_OP_REDUCE_MAX_INDEX ? "s64" : a.lane;
    case SIMD_ALL_ZERO:
        if(count != 1 || !vector_of(op, &operands[0], &a)) return fail(message, "takes one integer vector");
        return "bool";
    case SIMD_BYTE_SHUFFLE:
        if(count != 2 || !vector_of(op, &operands[0], &a) || !VectorType(operands[1].type, &b) ||
           strcmp(b.lane, "u8") || b.lanes != a.lanes)
            return fail(message, "takes a u8 or s8 table and u8 indices of the same width");
        return a.name;
    case SIMD_SHUFFLE: {
        if(count < 1 || !vector_of(op, &operands[0], &a)) return fail(message, "takes a vector and constant lane indices");
        int block = SimdBlockLanes(&a);
        if(count != block + 1) return fail(message, "takes one constant index for each lane of a 128-bit block");
        for(int i = 1; i < count; i++)
            if(!operands[i].is_constant || operands[i].value < 0 || operands[i].value >= block)
                return fail(message, "takes constant lane indices within one 128-bit block");
        return a.name;
    }
    case SIMD_REINTERPRET:
        if(count != 1 || !VectorType(operands[0].type, &a) || a.bytes != r.bytes)
            return fail(message, "takes the result type and a vector of the same width");
        return r.name;
    case SIMD_CONVERT:
        if(count != 1 || !VectorType(operands[0].type, &a) || a.lanes != r.lanes ||
           a.lane_bits != r.lane_bits || a.is_float == r.is_float)
            return fail(message, "converts between integer and float lanes of the same width");
        return r.name;
    case SIMD_HALF:
        if(count != 1 || !vector_of(op, &operands[0], &a) || a.bytes != 32)
            return fail(message, "takes one 256-bit vector");
        return VectorTypeName(a.lane, a.lanes / 2);
    case SIMD_COMBINE:
        if(count != 2 || !vector_of(op, &operands[0], &a) || a.bytes != 16 ||
           strcmp(operands[0].type, operands[1].type))
            return fail(message, "takes two 128-bit vectors of one type");
        return VectorTypeName(a.lane, a.lanes * 2);
    }
    return fail(message, unsupported);
}
