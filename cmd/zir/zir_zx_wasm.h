#ifndef ZIR_ZX_WASM_H
#define ZIR_ZX_WASM_H

#include "zir.h"
#include "ziran_host.h"

/* A bundle instance running as WebAssembly: the verified program lowered
 * to a wasm32 module by cmd/zir-wasm/zir_wasm_lower.c and instantiated
 * through the installed provider (zir_zx_wasm_platform.h). The current
 * tier lowers the scalar subset only; bundles with host capabilities or
 * other out-of-subset constructs fail here and the caller falls back to
 * the portable VM, so the engine never guesses. */
typedef struct ZxWasm ZxWasm;

/* Lower and instantiate one instance. Capability names are accepted for
 * signature parity with the native engine; a bundle that needs any host
 * binding is outside the scalar subset and fails with a diagnostic.
 * Returns NULL when no provider is installed, lowering fails, or the
 * provider rejects the module. */
ZxWasm *ZxWasmOpen(const ZirProgram *program, const ZirModule *entry_owner,
                   const ZirFunction *entry,
                   const char *const *capability_modules,
                   const char *const *capability_functions,
                   size_t capability_count);
int ZxWasmRun(ZxWasm *wasm, long long *result, int *has_result);
void ZxWasmLimitSteps(ZxWasm *wasm, int max_steps);
void ZxWasmMatchFieldsByName(ZxWasm *wasm, int enabled);
void ZxWasmClose(ZxWasm *wasm);

#endif
