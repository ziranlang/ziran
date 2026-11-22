/*
 * zir_host_exec.h - revision-28 executable ZX behind the embedding API
 * (include/ziran_host.h). The engine is generated Ziran; this interface keeps
 * its headers out of the rest of the library.
 */
#ifndef ZIR_HOST_EXEC_H
#define ZIR_HOST_EXEC_H

#include "ziran_host.h"

#include <stddef.h>
#include <stdint.h>

typedef struct ExecBundle ExecBundle;
typedef struct ExecHosts ExecHosts;
typedef struct ExecInstance ExecInstance;

/* Reads, verifies and plans a revision-28 executable. The input is borrowed
 * during this call. NULL indicates failure after a diagnostic naming `name`. */
ExecBundle *ExecBundleOpen(const unsigned char *bytes, size_t size, const char *name);
/* When present, consumed runs once after a successful read and asset copy,
 * before planning. It can free the input. The caller remains responsible
 * for release if it does not run. */
ExecBundle *ExecBundleOpenWithRelease(const unsigned char *bytes, size_t size, const char *name,
                                      BundleImageRelease consumed, void *context);
/* Verifies the source and writes the bundle's raw SHA-256 on success. */
ExecBundle *ExecBundleOpenReadAt(uint64_t size, BundleReadAt read_at, void *context,
                                unsigned char digest[32], int *unsupported, int engine_only, BundleShrink shrink);
void ExecBundleClose(ExecBundle *bundle);
/* The verified program, a `const LirProgram *`, or NULL for an engine-only bundle. */
const void *ExecBundleProgram(const ExecBundle *bundle);
/* The program with the file's optional debug attachment, its source texts
 * and origins, restored into DEBUG, a `ZxDebugOwner *` that borrows the
 * bundle's program. False when the file has none or it does not belong to
 * this program. */
int ExecBundleDebugProgram(const ExecBundle *bundle, void *debug);
/* Host capabilities in program order; the platform console services the
 * engine binds itself are not among them. */
size_t ExecBundleCapabilityCount(const ExecBundle *bundle);
const char *ExecBundleCapabilityModule(const ExecBundle *bundle, size_t index);
const char *ExecBundleCapabilityFunction(const ExecBundle *bundle, size_t index);
/* The verified LIR function a capability names, or -1. */
int32_t ExecBundleCapabilityFunctionId(const ExecBundle *bundle, size_t index);
/* Verified LIR type identifiers for the native bridge, or -1. */
int32_t ExecBundleCapabilityParameterTypeId(const ExecBundle *bundle,
                                            size_t index, size_t parameter);
int32_t ExecBundleCapabilityResultTypeId(const ExecBundle *bundle,
                                         size_t index);
/* The capability's declaration as written; 0 for an invalid index. */
int ExecBundleCapabilityAbi(const ExecBundle *bundle, size_t index, BundleCapabilityAbi *abi);
/* Whether parameter `parameter` of capability `index` is a []T slice: the
 * host may change its elements, which are copied back after the call, and
 * two such arguments of one call may not share elements. */
int ExecBundleHostMutable(const ExecBundle *bundle, size_t index, size_t parameter);
const char *ExecBundleEntryModule(const ExecBundle *bundle);
const char *ExecBundleEntryFunction(const ExecBundle *bundle);
/* The entry's parameters and return type as written. */
const char *ExecBundleEntryArguments(const ExecBundle *bundle);
const char *ExecBundleEntryReturnType(const ExecBundle *bundle);
/* Embedded files, in name order; borrowed until ExecBundleClose. */
size_t ExecBundleAssetCount(const ExecBundle *bundle);
const char *ExecBundleAssetName(const ExecBundle *bundle, size_t index);
const unsigned char *ExecBundleAssetData(const ExecBundle *bundle, size_t index);
size_t ExecBundleAssetSize(const ExecBundle *bundle, size_t index);

/* Gives the binding for a capability, or NULL. */
typedef const HostBinding *(*ExecFindBinding)(void *context, const char *module, const char *function);

/* One value of a host call in the engine's form: a scalar or pointer in
 * `bits`, any other value as its host image in `bytes`. */
typedef struct ExecHostArgument {
    int32_t type;
    uint64_t bits;
    const unsigned char *bytes;
    size_t size;
    int changed;
    int borrowed;
} ExecHostArgument;

/* A value at a native call boundary. Scalars and pointers use `bits`.
 * Aggregate values use their in-process representation at mutable `bytes`;
 * string and slice views therefore contain real data pointers rather than
 * offsets into an engine image. ExecHostsCallNative translates that form
 * through the same typed host conversion as ExecHostsCall. Mutable slice
 * elements are copied back into the caller's storage after a successful
 * synchronous call. A non-scalar result's bytes are borrowed until the next
 * call on `hosts`. */
typedef struct ExecHostNativeValue {
    int32_t type;
    uint64_t bits;
    unsigned char *bytes;
    size_t size;
} ExecHostNativeValue;

/* Host calls of one bundle translated between host images and HostValue
 * trees, with every capability bound through `find`; NULL when one is
 * missing. ExecHostsCall calls capability `host` (an index as above). A
 * mutable slice argument comes back with `changed` set and `bytes` holding
 * its new image, the same element count. Those and the result's bytes stay
 * valid until the next call. */
ExecHosts *ExecHostsOpen(const ExecBundle *bundle, ExecFindBinding find, void *context);
int ExecHostsCall(ExecHosts *hosts, size_t host, ExecHostArgument *arguments, size_t count,
                  ExecHostArgument *result);
/* Native data pointers are tokens of the instance's pointer table. RESOLVE
 * gives the address BYTES bytes may be read (or written) at, or NULL; CREATE
 * names BYTES read-only bytes at ADDRESS that a host returned. Without them
 * native values are plain addresses. */
typedef void *(*ExecNativeResolve)(void *context, uint64_t token, size_t bytes, int writable);
typedef uint64_t (*ExecNativeCreate)(void *context, const void *address, size_t bytes);
void ExecHostsNativePointers(ExecHosts *hosts, ExecNativeResolve resolve,
                             ExecNativeCreate create, void *context);
/* Module state of a native instance: global INDEX's native bytes as a host
 * value, and a host value written over them (nested views get read-only
 * storage through the native hooks). StateNativeCheck converts without
 * writing, so a store can be checked whole before any global changes. */
int ExecHostsNativeStateValue(ExecHosts *hosts, size_t index, const unsigned char *native,
                              HostValue *value);
int ExecHostsNativeStateCheck(ExecHosts *hosts, size_t index, const HostValue *value);
int ExecHostsNativeStateStore(ExecHosts *hosts, size_t index, const HostValue *value,
                              unsigned char *native);
int ExecHostsCallNative(ExecHosts *hosts, size_t host,
                        ExecHostNativeValue *arguments, size_t count,
                        ExecHostNativeValue *result);
void ExecHostsMatchFieldsByName(ExecHosts *hosts, int enabled);
/* Module globals the interface section names, for an engine that keeps them
 * itself: `global` is the LIR global. A global crosses as its host image;
 * ExecHostsStateValue's value and ExecHostsStateImage's bytes are borrowed
 * until the next call on `hosts`. */
size_t ExecBundleStateCount(const ExecBundle *bundle);
int ExecBundleStateAt(const ExecBundle *bundle, size_t index, const char **module, const char **name,
                      const char **type, int32_t *global);
int ExecHostsStateValue(ExecHosts *hosts, size_t index, const unsigned char *image, size_t size,
                        HostValue *value);
int ExecHostsStateImage(ExecHosts *hosts, size_t index, const HostValue *value,
                        const unsigned char **image, size_t *size);
void ExecHostsClose(ExecHosts *hosts);

/* An instance with its globals initialized, or NULL when a capability is
 * missing or initialization fails. With max_steps > 0 initialization and all
 * runs together have that many steps. */
ExecInstance *ExecInstanceOpen(const ExecBundle *bundle, ExecFindBinding find, void *context,
                               long long max_steps, BundleConsoleWrite write, void *console_context);
/* A failed run leaves the instance unusable. */
int ExecInstanceRun(ExecInstance *instance, long long *result, int *has_result);
int ExecInstanceBorrowHostString(ExecInstance *instance, const char *module,
                                 const char *function, size_t parameter);
/* Bounds each later run, or all of them on an instance opened with a limit. */
void ExecInstanceLimitSteps(ExecInstance *instance, long long max_steps);
void ExecInstanceMatchFieldsByName(ExecInstance *instance, int enabled);
/* Bytes the instance's globals and owned storage hold. */
size_t ExecInstanceLiveBytes(const ExecInstance *instance);
/* Module globals the interface section names, with the value when `value`
 * is not NULL. It is borrowed until the next call into the instance. */
size_t ExecInstanceStateCount(const ExecInstance *instance);
int ExecInstanceStateAt(ExecInstance *instance, size_t index, const char **module, const char **name,
                        const char **type, HostValue *value);
/* Gives global i the value values[i], unless that is NULL; changes none
 * when one is rejected. */
int ExecInstanceStateStore(ExecInstance *instance, const HostValue *const *values);
void ExecInstanceClose(ExecInstance *instance);

/* A bundle's entry return type as written, for printing its result. */
const char *BundleEntryReturnType(const Bundle *bundle);

#endif
