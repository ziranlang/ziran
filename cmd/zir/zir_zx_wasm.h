#ifndef ZIR_ZX_WASM_H
#define ZIR_ZX_WASM_H

#include "zir.h"
#include "ziran_host.h"

/* A bundle instance running as WebAssembly: the verified program lowered
 * to a wasm32 module by cmd/zir-wasm/zir_wasm_lower.c and instantiated
 * through the installed provider (zir_zx_wasm_platform.h). The current
 * tier lowers the scalar subset and typed s64 -> s64 host capabilities;
 * other out-of-subset constructs fail here and the caller falls back to
 * the portable VM, so the engine never guesses. */
typedef struct ZxWasm ZxWasm;
struct Capability;

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

#endif
