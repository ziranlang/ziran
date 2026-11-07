#ifndef ZIR_NATIVE_H
#define ZIR_NATIVE_H
#include "zir.h"

typedef enum ZirTarget { ZIR_C, ZIR_GO } ZirTarget;
const char *TargetType(const char *type, ZirTarget target);

/* 128-bit blocks in a native vector, such as s16xN, of the C code being
 * emitted: 1 or 2 in a #target clone, 0 where the build decides
 * (ZI_NATIVE_BLOCKS in ziran_simd_types.h). */
extern int zir_native_blocks;
/* The vector type `type` is in the code being emitted: a native vector is
 * its 128-bit block in Go, its fixed type in a #target clone and itself in
 * other C. Any other type is itself. */
const char *VectorTypeHere(const char *type, ZirTarget target);

/* Run after semantic checking and before native emission. */
int PrepareNativeNames(ZirProgram **programs, int count);
#endif
