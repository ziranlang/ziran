#ifndef ZIR_ZX_WASM_H
#define ZIR_ZX_WASM_H

#include "zir.h"
#include "ziran_host.h"
#include "zir_host_exec.h"

#include <stdint.h>

/* A bundle instance running as WebAssembly, instantiated through the
 * installed provider (zir_zx_wasm_platform.h). A revision-28 bundle lowers
 * through cmd/zir-wasm/zx_wasm_lower.c, with typed host calls translated
 * by the embedding API's rules (ExecHosts); a semantic bundle lowers
 * through cmd/zir-wasm/zir_wasm_lower.c, whose tier carries scalars and
 * s64 -> s64 host capabilities. Lowering and provider rejection fail the
 * selected engine; the caller never starts a different engine. */
typedef struct ZxWasm ZxWasm;
struct Capability;

/* Lower, instantiate and initialize one instance of a revision-28 bundle,
 * every capability bound through `find`. max_steps > 0 bounds
 * initialization and all runs together, as on the instruction engine.
 * NULL after a diagnostic. */
ZxWasm *ZxWasmOpenExec(const ExecBundle *bundle, ExecFindBinding find, void *context,
                       long long max_steps);

/* Lower and instantiate one independently owned instance. Each host
 * capability is dispatched through its exact bundle identity and typed ABI.
 * Returns NULL when no provider is installed, lowering fails, or the
 * provider rejects the module. */
ZxWasm *ZxWasmOpen(const ZirProgram *program, const ZirModule *entry_owner,
                   const ZirFunction *entry, const struct Capability *capabilities,
                   size_t capability_count, VmHostCall host, void *host_context);
int ZxWasmRun(ZxWasm *wasm, long long *result, int *has_result);
const ZirProgram *ZxWasmProgram(const ZxWasm *wasm);
size_t ZxWasmStateCount(const ZxWasm *wasm);
int ZxWasmStateAt(const ZxWasm *wasm, size_t index, const char **module,
                  const char **name, const char **type, size_t *address,
                  size_t *size);
int ZxWasmReadState(const ZxWasm *wasm, size_t address, void *destination,
                    size_t size);
int ZxWasmWriteState(const ZxWasm *wasm, size_t address, const void *source,
                     size_t size);
size_t ZxWasmAllocateState(const ZxWasm *wasm, size_t size);
void ZxWasmLimitSteps(ZxWasm *wasm, int max_steps);
void ZxWasmMatchFieldsByName(ZxWasm *wasm, int enabled);
void ZxWasmClose(ZxWasm *wasm);
/* Builtin print dispatches carry a string handle or scalar bits. Ordinary
 * capability indices retain the exact declared capability identity. */
#define ZX_WASM_PRINT_STRING UINT32_MAX
#define ZX_WASM_PRINT_SIGNED (UINT32_MAX - 1)
#define ZX_WASM_PRINT_UNSIGNED (UINT32_MAX - 2)
#define ZX_WASM_PRINT_FLOAT64 (UINT32_MAX - 3)
#define ZX_WASM_PRINT_FLOAT32 (UINT32_MAX - 4)
#define ZX_WASM_PRINT_BOOL (UINT32_MAX - 5)
int ZxWasmHostCallI64(void *bridge, size_t index, int64_t argument,
                      int64_t *result);
/* The (zx, host) import of a revision-28 module: capability `index` with
 * the call record at `record` in the instance's memory (zx_wasm_lower.h).
 * Returns 1, or 0 when the call fails and the module traps. */
int ZxWasmHostCall(void *bridge, uint32_t index, uint32_t record);

#endif
