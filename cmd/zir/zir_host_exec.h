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

/* Whether the bytes' header names revision 28. */
int ExecBundleBytes(const unsigned char *bytes, size_t size);
/* Reads, verifies and plans a revision-28 executable; NULL after a
 * diagnostic naming `name` when it cannot. The bytes are borrowed only
 * during the call. */
ExecBundle *ExecBundleOpen(const unsigned char *bytes, size_t size, const char *name);
void ExecBundleClose(ExecBundle *bundle);
/* The verified program, a `const LirProgram *`. */
const void *ExecBundleProgram(const ExecBundle *bundle);
/* Host capabilities in program order; the platform console services the
 * engine binds itself are not among them. */
size_t ExecBundleCapabilityCount(const ExecBundle *bundle);
const char *ExecBundleCapabilityModule(const ExecBundle *bundle, size_t index);
const char *ExecBundleCapabilityFunction(const ExecBundle *bundle, size_t index);
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
} ExecHostArgument;

/* Host calls of one bundle translated between host images and VmHostValue
 * trees, with every capability bound through `find`; NULL when one is
 * missing. ExecHostsCall calls capability `host` (an index as above). A
 * mutable slice argument comes back with `changed` set and `bytes` holding
 * its new image, the same element count. Those and the result's bytes stay
 * valid until the next call. */
ExecHosts *ExecHostsOpen(const ExecBundle *bundle, ExecFindBinding find, void *context);
int ExecHostsCall(ExecHosts *hosts, size_t host, ExecHostArgument *arguments, size_t count,
                  ExecHostArgument *result);
void ExecHostsMatchFieldsByName(ExecHosts *hosts, int enabled);
void ExecHostsClose(ExecHosts *hosts);

/* An instance with its globals initialized, or NULL when a capability is
 * missing or initialization fails. With max_steps > 0 initialization and all
 * runs together have that many steps. */
ExecInstance *ExecInstanceOpen(const ExecBundle *bundle, ExecFindBinding find, void *context,
                               long long max_steps);
/* A failed run leaves the instance unusable. */
int ExecInstanceRun(ExecInstance *instance, long long *result, int *has_result);
/* Bounds each later run, or all of them on an instance opened with a limit. */
void ExecInstanceLimitSteps(ExecInstance *instance, long long max_steps);
void ExecInstanceMatchFieldsByName(ExecInstance *instance, int enabled);
/* Bytes the instance's globals and owned storage hold. */
size_t ExecInstanceLiveBytes(const ExecInstance *instance);
/* Module globals the interface section names, with the value when `value`
 * is not NULL. It is borrowed until the next call into the instance. */
size_t ExecInstanceStateCount(const ExecInstance *instance);
int ExecInstanceStateAt(ExecInstance *instance, size_t index, const char **module, const char **name,
                        const char **type, VmHostValue *value);
/* Gives global i the value values[i], unless that is NULL; changes none
 * when one is rejected. */
int ExecInstanceStateStore(ExecInstance *instance, const VmHostValue *const *values);
void ExecInstanceClose(ExecInstance *instance);

#endif
