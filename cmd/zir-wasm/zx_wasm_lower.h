#ifndef ZX_WASM_LOWER_H
#define ZX_WASM_LOWER_H
#include "zir.h"

#include <stddef.h>
#include <stdint.h>

/*
 * zx_wasm_lower - verified revision-28 executable ZX to one wasm32 module.
 *
 * Wasm is another lowering of the one pipeline:
 *   .zi -> ZIR -> LIR -> verify -> LIR optimizations -> .zx
 * and then the portable interpreter, native machine code, or this module.
 * The lowering reads only the decoded, verified executable payload, so one
 * .zx always gives the same .wasm bytes.
 *
 * Module ABI
 *   imports  (zx, overrun) () -> ()            a spent budget
 *            (zx, budget) mutable i64 global    charged by every LIR fuel
 *            (zx, host_i64) (i32, i64) -> i64   platform output, when used:
 *                index ZX_WASM_PRINT_*; a string is (length << 32 | address)
 *            (zx, host) (i32, i32) -> i32       host capabilities, when used
 *   exports  zx_entry () -> i64, memory; with module initializers
 *            zx_initialize () -> (), which zx_entry calls first and a host
 *            may call to initialize an instance when it opens; with a heap
 *            zx_reserve (i32 bytes) -> i32 address (0 when memory runs out)
 *   custom   zxhost: each capability's module, function and parameter
 *            count, by capability index
 *
 * host(capability, record) mirrors the interpreter's typed host call. The
 * record holds one 16-byte entry per argument and then one for the result.
 * A scalar or pointer (an opaque handle) is its canonical u64 bits at +0.
 * A value of any other shape is a host image:
 *   +0 u32  image address        +4 u32  for a view, its elements' address
 *   +8 u32  image length         +12 u32 flags: the host sets bit 0 on a
 *           []T argument whose image elements it changed, and the module
 *           copies them back into the slice
 * A host image is the value's bytes followed by the elements of every view
 * inside it; a view's two words are the offset of its elements from the
 * image start and their count. The host writes the result entry: bits, or
 * an image it placed in a block from zx_reserve, which the module then
 * owns. host returns nonzero on success; zero traps.
 *
 * The heap serves the LIR resize instruction: blocks are zero-filled,
 * freed blocks are reused, and a request memory cannot hold gives null.
 * Unsupported forms fail with a named wasm.gap.* diagnostic and never
 * select another engine.
 */
enum { ZX_WASM_VOID, ZX_WASM_BITS, ZX_WASM_IMAGE };

/* One capability value: its LIR type and how it crosses. */
typedef struct ZxWasmValue {
    int32_t type;
    int kind;
    /* For a view whose elements hold no views (string, []T), the element
     * size: such elements can be copied back. 0 otherwise. */
    size_t element_size;
} ZxWasmValue;

typedef struct ZxWasmCapability {
    char *module;
    char *function;
    /* The LIR function the capability is. */
    int32_t symbol;
    size_t parameter_count;
    ZxWasmValue *parameters;
    ZxWasmValue result;
} ZxWasmCapability;

typedef struct ZxWasmImage {
    unsigned char *bytes;
    size_t size;
    ZxWasmCapability *capabilities;
    size_t capability_count;
    /* Where each LIR global lives in the instance's memory, and its size. */
    uint32_t *global_addresses;
    uint32_t *global_sizes;
    size_t global_count;
    /* Whether the entry returns a value. */
    int returns;
} ZxWasmImage;

/* An instance's memory, for module state: read and write bytes, and
 * reserve zero-filled storage (zx_reserve), giving 0 when it cannot. */
typedef struct ZxWasmMemory {
    void *context;
    int (*read)(void *context, size_t address, void *destination, size_t size);
    int (*write)(void *context, size_t address, const void *source, size_t size);
    size_t (*reserve)(void *context, size_t size);
} ZxWasmMemory;

#if defined(__GNUC__) || defined(__clang__)
#define ZX_WASM_API __attribute__((visibility("default")))
#else
#define ZX_WASM_API
#endif

struct LirProgram;

/* Reads and verifies a revision-28 executable ZX image and lowers its
 * payload. Returns 1 with a filled image, or 0 after a diagnostic. */
ZX_WASM_API int ZxWasmLowerExecutable(const unsigned char *zx, size_t size,
                                      ZxWasmImage *image);
/* Lowers a program its caller has already read and verified. */
ZX_WASM_API int ZxWasmLowerProgram(const struct LirProgram *program,
                                   ZxWasmImage *image);
ZX_WASM_API void ZxWasmImageFree(ZxWasmImage *image);
/* The host image of LIR global `global` of an instance: its bytes, then
 * the elements of each view inside it, read from memory. The caller frees
 * *image. */
ZX_WASM_API int ZxWasmGlobalImage(const struct LirProgram *program, int32_t global, const ZxWasmImage *module,
                                  const ZxWasmMemory *memory, unsigned char **image, size_t *size);
/* Stores a host image as LIR global `global`; each view's elements go to
 * storage from memory->reserve. */
ZX_WASM_API int ZxWasmGlobalStore(const struct LirProgram *program, int32_t global, const ZxWasmImage *module,
                                  const ZxWasmMemory *memory, const unsigned char *image, size_t size);

#endif
