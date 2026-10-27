#ifndef ZIR_ZX_NATIVE_H
#define ZIR_ZX_NATIVE_H

#include "zir.h"
#include "ziran_host.h"

#include <stdint.h>

/* A bundle instance running as native code: the verified program lowered to
 * C, compiled in memory by TinyCC and called directly. Host capabilities
 * reach `host` with the same values the portable VM passes. */
typedef struct ZxNative ZxNative;

/* Headers natively compiled bundles include, embedded in the library. */
typedef struct ZxNativeHeader {
    const char *name;
    const char *text;
} ZxNativeHeader;
extern const ZxNativeHeader ZxNativeHeaders[];

/* Compile and initialize one instance. Capability i names the function
 * trampolines pass to `host`. `identity` is a SHA-256 of the bundle's bytes
 * and its libraries; with it, compiled code is cached and reused across
 * launches. Returns NULL after a diagnostic when the program cannot be
 * compiled or its module initializers fail. */
ZxNative *ZxNativeOpen(const ZirProgram *program, const ZirModule *entry_owner,
                       const ZirFunction *entry, const unsigned char identity[32],
                       const char *const *capability_modules,
                       const char *const *capability_functions,
                       size_t capability_count, VmHostCall host, void *context);
/* Whether compiled code for `identity` is cached. Only this compiler writes
 * it, from a checked program, so a host may open those bytes unchecked. */
int ZxNativeCached(const unsigned char identity[32]);
/* Like ZxNativeOpen from cached code alone, for a bundle opened unchecked.
 * Returns NULL with *missing set and no diagnostic when no cached code
 * links; the host then checks the program and calls ZxNativeOpen. */
ZxNative *ZxNativeOpenCached(const ZirFunction *entry, const unsigned char identity[32],
                             const char *const *capability_modules,
                             const char *const *capability_functions,
                             size_t capability_count, VmHostCall host, void *context,
                             int *missing);
int ZxNativeRun(ZxNative *native, long long *result, int *has_result);
void ZxNativeLimitSteps(ZxNative *native, int max_steps);
void ZxNativeMatchFieldsByName(ZxNative *native, int enabled);
void ZxNativeClose(ZxNative *native);

/* Code compiled ahead of time and linked into the host, for hosts that
 * cannot compile at run time (the browser). `zi2zx native-c` writes it with
 * ZxNativeWriteLinked, including a constructor that registers it here
 * under its bundle's identity. */
void ZxNativeRegister(const unsigned char identity[32], void (*start)(void), int64_t (*run)(void),
                      long long *budget, char **floor, unsigned long *stack_mask,
                      unsigned long *stack_margin, unsigned long *budget_slot,
                      char *data, char *data_end);
int ZxNativeLinked(const unsigned char identity[32]);
/* Like ZxNativeOpen on the linked code for `identity`. One instance runs
 * it at a time: while another is open this returns NULL with *busy set,
 * without a diagnostic, and the host runs the portable VM instead. */
ZxNative *ZxNativeOpenLinked(const ZirFunction *entry, const unsigned char identity[32],
                             const char *const *capability_modules,
                             const char *const *capability_functions,
                             size_t capability_count, VmHostCall host, void *context,
                             int *busy);
/* Write a verified program's C for linking in `directory` (existing and
 * empty), with zx_native.json naming its sources and their compile flags.
 * Built where the native engine is. */
int ZxNativeWriteLinked(const ZirProgram *program, const ZirModule *entry_owner,
                        const ZirFunction *entry, const unsigned char identity[32],
                        const char *const *capability_modules,
                        const char *const *capability_functions, size_t capability_count,
                        const char *directory, char *error, size_t error_size);

#endif
