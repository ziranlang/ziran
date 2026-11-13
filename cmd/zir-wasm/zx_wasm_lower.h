#ifndef ZX_WASM_LOWER_H
#define ZX_WASM_LOWER_H
#include "zir.h"

#include <stddef.h>

/*
 * zx_wasm_lower - verified revision-28 executable ZX to one wasm32 module.
 *
 * Wasm is another lowering of the one pipeline:
 *   .zi -> ZIR -> LIR -> verify -> LIR optimizations -> .zx
 * and then the portable interpreter, native machine code, or this module.
 * The lowering reads only the decoded, verified executable payload, so one
 * .zx always gives the same .wasm bytes.
 *
 * Module ABI: imports (zx, overrun) () -> (), the mutable (zx, budget) i64
 * global charged by every LIR fuel instruction, and, when the program makes
 * host calls, (zx, host_i64) (i32 index, i64 value) -> i64. Exports zx_entry
 * () -> i64, memory, and the zx_state_arena cursor; a zxstate custom section
 * lists writable globals. Platform output services use the reserved
 * ZX_WASM_PRINT_* indices; each declared s64 -> s64 host capability uses its
 * position in ZxWasmImage.capabilities. Unsupported forms fail with a named
 * wasm.gap.* diagnostic and never select another engine.
 */
typedef struct ZxWasmCapability {
    char *module;
    char *function;
} ZxWasmCapability;

typedef struct ZxWasmImage {
    unsigned char *bytes;
    size_t size;
    ZxWasmCapability *capabilities;
    size_t capability_count;
} ZxWasmImage;

#if defined(__GNUC__) || defined(__clang__)
#define ZX_WASM_API __attribute__((visibility("default")))
#else
#define ZX_WASM_API
#endif

/* Reads and verifies a revision-28 executable ZX image and lowers its
 * payload. Returns 1 with a filled image, or 0 after a diagnostic. */
ZX_WASM_API int ZxWasmLowerExecutable(const unsigned char *zx, size_t size,
                                      ZxWasmImage *image);
ZX_WASM_API void ZxWasmImageFree(ZxWasmImage *image);

#endif
