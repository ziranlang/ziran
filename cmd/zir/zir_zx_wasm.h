#ifndef ZIR_ZX_WASM_H
#define ZIR_ZX_WASM_H

#include "ziran_host.h"
#include "zir_host_exec.h"

#include <stddef.h>
#include <stdint.h>

/* A revision-28 bundle instance running as WebAssembly: the verified
 * program lowered by cmd/zir-wasm/zx_wasm_lower.c and instantiated through
 * the installed provider (zir_zx_wasm_platform.h), with host calls and
 * module state translated by the embedding API's rules (ExecHosts).
 * Lowering and provider rejection fail the selected engine; the caller
 * never starts a different engine. */
typedef struct ZxWasm ZxWasm;

/* Lower, instantiate and initialize one instance, every capability bound
 * through `find`. max_steps > 0 bounds initialization and all runs
 * together, as on the instruction engine. NULL after a diagnostic. */
ZxWasm *ZxWasmOpenExec(const ExecBundle *bundle, ExecFindBinding find, void *context,
                       long long max_steps);
/* has_result is 0 for an entry that returns nothing. */
int ZxWasmRun(ZxWasm *wasm, long long *result, int *has_result);
/* Bounds each later run, or all of them on an instance opened with a limit. */
void ZxWasmLimitSteps(ZxWasm *wasm, long long max_steps);
void ZxWasmMatchFieldsByName(ZxWasm *wasm, int enabled);
/* Bytes the instance's globals hold. */
size_t ZxWasmLiveBytes(const ZxWasm *wasm);
/* Module globals the interface section names, as ExecInstanceStateCount,
 * ExecInstanceStateAt and ExecInstanceStateStore give and take them. */
size_t ZxWasmStateCount(const ZxWasm *wasm);
int ZxWasmStateAt(ZxWasm *wasm, size_t index, const char **module, const char **name,
                  const char **type, VmHostValue *value);
int ZxWasmStateStore(ZxWasm *wasm, const VmHostValue *const *values);
void ZxWasmClose(ZxWasm *wasm);

/* Platform output services of the (zx, host_i64) import: a string as
 * (length << 32 | address), or scalar bits. */
#define ZX_WASM_PRINT_STRING UINT32_MAX
#define ZX_WASM_PRINT_SIGNED (UINT32_MAX - 1)
#define ZX_WASM_PRINT_UNSIGNED (UINT32_MAX - 2)
#define ZX_WASM_PRINT_FLOAT64 (UINT32_MAX - 3)
#define ZX_WASM_PRINT_FLOAT32 (UINT32_MAX - 4)
#define ZX_WASM_PRINT_BOOL (UINT32_MAX - 5)
/* The (zx, host_i64) import: prints one value; 0 when it cannot. */
int ZxWasmHostPrint(void *wasm, uint32_t service, int64_t argument);
/* The (zx, host) import: capability `index` with the call record at
 * `record` in the instance's memory (zx_wasm_lower.h). Returns 1, or 0
 * when the call fails and the module traps. */
int ZxWasmHostCall(void *wasm, uint32_t index, uint32_t record);

#endif
