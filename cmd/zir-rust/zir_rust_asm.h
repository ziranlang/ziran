#ifndef ZIR_RUST_ASM_H
#define ZIR_RUST_ASM_H

#include "zir.h"
#include "zir_emit.h"

typedef struct RustAssemblyTarget {
    void *context;
    ZirResolveTarget name;
    ZirResolveTarget type;
    ZirResolveTarget field;
} RustAssemblyTarget;

/* Functions emit native early returns before the ordinary portable body.
 * Blocks open a surrounding scope and their conditional portable body;
 * the caller emits that body and closes both scopes when this returns 1. */
int RustEmitAssembly(FILE *out, const ZirModule *module, const ZirFunction *function,
                     int statement, int indent, const RustAssemblyTarget *target);

#endif
