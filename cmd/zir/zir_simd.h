/*
 * zir_simd.h - SIMD vector operations: names, signatures and constants.
 *
 * Source calls them through std/simd (`Simd.Add(a, b)`); checking rewrites
 * each call to its reserved name, such as zi_simd_add, which saved IR keeps
 * and every backend reads. The semantics are zi_ref in include/ziran_simd.h.
 */
#ifndef ZIR_SIMD_H
#define ZIR_SIMD_H

#include "zir.h"
#include <stdint.h>

typedef enum SimdShape {
    SIMD_BINARY, SIMD_UNARY, SIMD_COMPARE, SIMD_SELECT, SIMD_SHIFT,
    SIMD_SHIFT_LANES, SIMD_BYTE_SHIFT, SIMD_ALIGN, SIMD_MAKE, SIMD_SPLAT,
    SIMD_ZERO, SIMD_LANE, SIMD_WITH_LANE, SIMD_LOAD, SIMD_STORE, SIMD_GATHER,
    SIMD_WIDEN_HALF, SIMD_WIDEN, SIMD_NARROW, SIMD_MUL_WIDE, SIMD_MADD,
    SIMD_MADD_SAT, SIMD_ADD_PAIRS, SIMD_SAD, SIMD_REDUCE, SIMD_ALL_ZERO,
    SIMD_BYTE_SHUFFLE, SIMD_SHUFFLE, SIMD_REINTERPRET, SIMD_CONVERT,
    SIMD_HALF, SIMD_COMBINE
} SimdShape;

typedef struct SimdOp {
    int id;              /* ZI_OP_* in ziran_simd.h */
    const char *name;    /* source name in std/simd, such as AddSat */
    const char *snake;   /* runtime spelling, such as add_sat */
    const char *checked; /* reserved checked name, such as zi_simd_add_sat */
    SimdShape shape;
    unsigned lanes;      /* accepted lane kinds, bit ZI_K_* */
    int type_argument;   /* the first operand names the result type */
} SimdOp;

/* Lane kind of a vector, ZI_K_U8 .. ZI_K_F64, or -1. */
int SimdLaneKind(const ZirVector *vector);
/* The operation at index, from 0, or NULL past the last. */
const SimdOp *SimdOpAt(int index);
/* A source name in std/simd, or NULL. */
const SimdOp *SimdOpNamed(const char *name);
/* A checked call name, or NULL. */
const SimdOp *SimdOpChecked(const char *name);
/* The std/simd module that holds the operations. */
int SimdModule(const ZirModule *module);

/* One operand as the signature check sees it. */
typedef struct SimdOperand {
    const char *type;    /* checked type; "integer"/"real" for literals */
    int is_constant;     /* the value is known while checking */
    int64_t value;
} SimdOperand;

/*
 * The result type of `op`, or NULL with *message describing the problem.
 * `result` is the type operand (or the checked call's type when reading
 * saved IR); `operands` are the value operands after it.
 */
const char *SimdSignature(const SimdOp *op, const char *result,
                          const SimdOperand *operands, int count,
                          const char **message);

/* The memory operand of a load, store or gather: its element type, and
 * whether it is a slice, a fixed array or a pointer. */
enum { SIMD_MEMORY_NONE, SIMD_MEMORY_SLICE, SIMD_MEMORY_ARRAY, SIMD_MEMORY_POINTER };
int SimdMemory(const char *type, char *element, size_t size, int *capacity);

/* Lanes of `type` in one 128-bit block. */
int SimdBlockLanes(const ZirVector *vector);

/* Names the table borrows, in open-addressed slots that hold an entry's
 * index + 1. Zero-initialized. */
typedef struct SimdNames {
    const char **text;
    size_t *length;
    size_t count, capacity;
    size_t *slots, slot_count;
} SimdNames;
/* The entry of a name, added when absent and add is set, or -1. */
long SimdNamesFind(SimdNames *names, const char *name, size_t length, int add);
void SimdNamesFree(SimdNames *names);
/* The helper a word of generated C names, as ziran_simd_ops.h spells it:
 * zi_add_s16x8, zi_avx2_add_s16x16 and zi_add_s16x16_p_ name add_s16x8 and
 * add_s16x16. Narrows *word and *length; 0 for other words. */
int SimdHelperWord(const char **word, size_t *length);

/* The helpers one program's generated C names, and whether any of its files
 * includes ziran_simd_ops.h, which EmitSimdOps writes from them.
 * Zero-initialized; SimdUsesFree releases it. */
typedef struct ZirSimdUses {
    SimdNames names; /* owned copies */
    int header;
} ZirSimdUses;
/* Record a word of generated C; words naming no helper are ignored. */
void SimdUsesNote(ZirSimdUses *uses, const char *word, size_t length);
void SimdUsesFree(ZirSimdUses *uses);

#endif
