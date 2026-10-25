#ifndef ZIR_WASM_LOWER_H
#define ZIR_WASM_LOWER_H
#include "zir.h"
/*
 * zir_wasm_lower - checked Ziran programs to a wasm32 module.
 *
 * The emitter targets the portable execution subset the .zx verifier
 * accepts: scalar locals and parameters, structured control flow, direct
 * calls between linked functions, and the run-step guard import. Anything
 * outside the subset fails lowering with a diagnostic; callers fall back
 * to the portable VM rather than guessing.
 */
/* Lowers the linked program and returns a malloc'd module image, or NULL
 * after a diagnostic. The entry must be a zero-argument function with an
 * integer or bool (or void) result; it is exported as zx_entry with an
 * i64 result. The module imports (zx, step) -> (), which the embedding
 * runtime counts against BundleInstanceLimitSteps like the native tier:
 * once per function entry and once per loop iteration. */
unsigned char *zir_wasm_lower(const ZirProgram *program,
                              const char *entry_module, const char *entry_function,
                              size_t *size);
#endif
