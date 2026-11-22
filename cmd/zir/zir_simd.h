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
#include "compiler_simd_metadata.h"

/* The checker uses the generated records with C's read-only text types. */
enum {
    SIMD_ALIGN = SimdShape_SIMD_ALIGN,
    SIMD_BYTE_SHIFT = SimdShape_SIMD_BYTE_SHIFT,
    SIMD_COUNT = SimdShape_SIMD_COUNT,
    SIMD_LANE = SimdShape_SIMD_LANE,
    SIMD_MAKE = SimdShape_SIMD_MAKE,
    SIMD_SHUFFLE = SimdShape_SIMD_SHUFFLE,
    SIMD_SPLAT = SimdShape_SIMD_SPLAT,
    SIMD_STORE = SimdShape_SIMD_STORE,
    SIMD_WITH_LANE = SimdShape_SIMD_WITH_LANE
};

#define SimdModule(module) SimdModule((ZirModule *)(module))
#define SimdOpNamed(name) SimdOpNamed((uint8_t *)(name))
#define SimdOpChecked(name) SimdOpChecked((uint8_t *)(name))

/* The type or NULL, with the reason in *message. The result argument is the
 * type operand (or a saved call's checked type), separate from value operands. */
static inline const char *
SimdCheckedSignature(const SimdOp *op, const char *result, SimdOperand *operands,
                     int count, const char **message)
{
    uint8_t *detail = NULL;
    uint8_t *type = SimdSignature((SimdOp *)op, (uint8_t *)result, operands, count, &detail);
    *message = (const char *)detail;
    return (const char *)type;
}

#endif
