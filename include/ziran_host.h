#ifndef ZIRAN_HOST_H
#define ZIRAN_HOST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Bundle Bundle;
typedef struct BundleInstance BundleInstance;
typedef struct BundleState BundleState;
typedef struct BundleComponent BundleComponent;
typedef struct BundleComponentHandle BundleComponentHandle;

typedef enum VmHostValueKind {
    VM_HOST_VOID,
    VM_HOST_INTEGER,
    VM_HOST_UNSIGNED,
    VM_HOST_REAL,
    VM_HOST_STRING,
    VM_HOST_RECORD,
    VM_HOST_SLICE,
    VM_HOST_POINTER,
    VM_HOST_ARRAY
} VmHostValueKind;

typedef struct VmHostField VmHostField;

typedef struct VmHostValue {
    VmHostValueKind kind;
    const char *type;
    int64_t integer;
    uint64_t bits;
    double real;
    const unsigned char *data;
    size_t length;
    const VmHostField *fields;
    size_t field_count;
    struct VmHostValue *elements;
    void *pointer;
} VmHostValue;

struct VmHostField {
    const char *name;
    VmHostValue value;
};

/* Return nonzero after writing a result of the declared return type.
 * Record fields are in declaration order and carry their declared names and
 * types. Argument fields are borrowed for the call. A scalar, string, or record slice
 * argument has kind VM_HOST_SLICE, length elements, and a mutable elements array.
 * Modify elements in place and keep the elements pointer and length intact;
 * the VM validates and copies every element back after the call. Returned
 * fields, collection elements and string bytes are copied after the callback
 * returns, before the next host call. Keep them valid until that next call;
 * a later call may reuse their storage. Strings copied into the VM retain
 * their bytes while reachable, including across BundleInstanceRun calls.
 * Fixed arrays have kind VM_HOST_ARRAY and exactly their declared length.
 * Their elements may be inspected during the call; mutations to argument
 * elements are not copied back. A zero-length array has no elements.
 * A returned zero-length slice may have a null elements pointer. Longer
 * slices need elements for their declared length; oversized lengths fail.
 * A declared pointer type crosses as kind VM_HOST_POINTER with the raw address
 * in `pointer`; the VM treats it as an opaque handle and never dereferences
 * it, so the host owns the storage behind it. */
typedef int (*VmHostCall)(void *context, const char *module,
                          const char *function, const VmHostValue *args,
                          int arg_count, VmHostValue *result);

typedef struct HostBinding {
    const char *module;
    const char *function;
    VmHostCall call;
    void *context;
} HostBinding;

typedef struct BundleStateValue {
    const char *module;
    const char *name;
    const char *type;
    VmHostValue value;
} BundleStateValue;

typedef int (*BundleStateMigrate)(void *context,
                                  const BundleState *old_state,
                                  BundleState *new_state);
typedef int (*BundleComponentVerify)(void *context,
                                     BundleInstance *instance);

typedef struct BundleCapabilityAbi {
    const char *module;
    const char *function;
    const char *signature;
    const char *arguments;
    const char *return_type;
    int variable_arguments;
    int reads_arguments;
} BundleCapabilityAbi;

/* Open and validate a version 26 portable bundle. Close releases all names. */
Bundle *BundleOpen(const char *path);
/* Select portable execution for subsequent bundle opens and instances. */
void BundleUsePortableVm(int enabled);
/* Validate an embedded/downloaded bundle without an application pathname.
 * Input bytes are borrowed only during this call; the opened bundle owns IR. */
Bundle *BundleOpenBytes(const unsigned char *data, size_t size);
/* Dependencies must already be verified by the publisher-aware host.
 * Libraries are retained until this bundle closes. Code/assets are shared;
 * every instance owns its globals. Missing/conflicting imports are rejected. */
Bundle *BundleOpenWithLibraries(const char *path,
                               Bundle *const *libraries, size_t count);
Bundle *BundleOpenBytesWithLibraries(const unsigned char *data, size_t size,
                                    Bundle *const *libraries, size_t count);
void BundleClose(Bundle *bundle);
/* Required capability names are borrowed until BundleClose. Enumeration covers
 * the final linked code, including retained libraries, and is cached on open. */
size_t BundleCapabilityCount(const Bundle *bundle);
const char *BundleCapabilityModule(const Bundle *bundle, size_t index);
const char *BundleCapabilityFunction(const Bundle *bundle, size_t index);
int BundleCapabilityAbiAt(const Bundle *bundle, size_t index,
                          BundleCapabilityAbi *abi);
int BundleAbiCompatible(const Bundle *left, const Bundle *right);
const char *BundleEntryModuleName(const Bundle *bundle);
const char *BundleEntryFunctionName(const Bundle *bundle);
void BundleCodeIdentity(const Bundle *bundle, unsigned char identity[32]);

/* Embedded files have canonical relative paths and immutable bytes. Names and
 * data are borrowed until BundleClose; an invalid index returns NULL/zero. */
size_t BundleAssetCount(const Bundle *bundle);
size_t BundleOwnAssetCount(const Bundle *bundle);
const char *BundleAssetName(const Bundle *bundle, size_t index);
const unsigned char *BundleAssetData(const Bundle *bundle, size_t index);
size_t BundleAssetSize(const Bundle *bundle, size_t index);

/* Every required capability must have exactly one binding before execution.
 * Integer/bool entry results are returned in result; void sets has_result=0.
 * For u64, cast result to uint64_t to recover all 64 unsigned bits. */
int BundleRun(const Bundle *bundle, const HostBinding *bindings,
              size_t binding_count, long long *result, int *has_result);

/* Law closure recorded at bundle time. Status strings are "proved",
 * "disproved", or "unknown"; a consumer can refuse bundles whose laws were
 * not all proved at save time. */
size_t BundleLawCount(const Bundle *bundle);
const char *BundleLawModule(const Bundle *bundle, size_t index);
const char *BundleLawName(const Bundle *bundle, size_t index);
const char *BundleLawStatus(const Bundle *bundle, size_t index);
size_t BundleLawWaiverCount(const Bundle *bundle);
const char *BundleLawWaiverName(const Bundle *bundle, size_t index);
const char *BundleLawWaiverReason(const Bundle *bundle, size_t index);

/* An instance preserves module globals across runs of the bundle entry.
 * The bundle's immutable code is verified once when opened; instantiation
 * validates bindings and initializes independent state without rechecking IR.
 * Separate instances may execute concurrently over the same live Bundle.
 * Keep the Bundle and any binding contexts alive until the instance closes.
 * Binding names and the binding array are copied during instantiation.
 * Caller order is preserved; callbacks are matched by module/function names.
 * A runtime failure makes the instance unusable. */
BundleInstance *BundleInstantiate(const Bundle *bundle,
                                   const HostBinding *bindings,
                                   size_t binding_count);
/* Explicit portable execution with a positive, cumulative statement budget.
 * The budget covers global initialization and all subsequent entry runs.
 * Initialization failure returns NULL; this never selects a compiled engine. */
BundleInstance *BundleInstantiateLimited(const Bundle *bundle,
                                         const HostBinding *bindings,
                                         size_t binding_count, int max_steps);
/* Always initialize and run the verified portable IR, even where compiled
 * engines are available. This is the execution tier used by component swap. */
BundleInstance *BundleInstantiatePortable(const Bundle *bundle,
                                          const HostBinding *bindings,
                                          size_t binding_count);
int BundleInstanceRun(BundleInstance *instance, long long *result,
                      int *has_result);
/* Bound each subsequent run to max_steps units of work, so a runaway bundle
 * stops with an error: statements in the portable VM, function calls and
 * loop iterations in natively compiled code. Zero is unbounded. */
void BundleInstanceLimitSteps(BundleInstance *instance, int max_steps);
/* Let records the host returns to this instance omit fields the bundle
 * declares (they take zero values) or carry fields it does not (ignored),
 * so a bundle and its host may be built against different layouts. */
void BundleInstanceMatchFieldsByName(BundleInstance *instance, int enabled);
void BundleInstanceClose(BundleInstance *instance);

/* Portable module state. Owned strings, fixed arrays, records, enums, and
 * vectors are copied recursively with bounded depth and bytes. Pointers,
 * slices, and other borrowed/host resources are rejected. Names, types, and
 * nested values are owned by the snapshot until BundleStateClose, independently
 * of the source instance. Migration sees the old snapshot and a mutable,
 * freshly initialized candidate snapshot. */
int BundleInstanceExportState(BundleInstance *instance, BundleState **state);
int BundleInstanceImportState(BundleInstance *instance,
                              const BundleState *state);
size_t BundleStateCount(const BundleState *state);
size_t BundleStateValueBytes(const BundleState *state);
int BundleStateValueAt(const BundleState *state, size_t index,
                       BundleStateValue *value);
BundleStateValue *BundleStateValueRef(BundleState *state, size_t index);
int BundleStateFind(const BundleState *state, const char *module,
                    const char *name, size_t *index);
int BundleStateSetInteger(BundleState *state, size_t index,
                          int64_t value);
int BundleStateSetUnsigned(BundleState *state, size_t index,
                           uint64_t value);
int BundleStateSetReal(BundleState *state, size_t index, double value);
int BundleStateSetString(BundleState *state, size_t index,
                         const unsigned char *data, size_t length);
void BundleStateClose(BundleState *state);

/* A hot-swappable component owns one positive-budget portable instance and
 * copied bindings.
 * Replacement checks identity and typed ABI first, exports old state,
 * initializes replacement code, migrates and imports state, and calls VERIFY;
 * only then does the generation advance. Any failure leaves the old instance
 * live. Handles acquired before a replacement become stale and fail safely. */
BundleComponent *BundleComponentOpen(const Bundle *bundle,
                                     const HostBinding *bindings,
                                     size_t binding_count, int max_steps);
BundleComponentHandle *BundleComponentAcquire(BundleComponent *component);
int BundleComponentHandleValid(const BundleComponentHandle *handle);
int BundleComponentRun(BundleComponentHandle *handle, long long *result,
                       int *has_result);
int BundleComponentReplace(BundleComponent *component,
                           const Bundle *replacement,
                           BundleStateMigrate migrate,
                           BundleComponentVerify verify, void *context,
                           BundleComponentHandle **new_handle);
size_t BundleComponentLiveValueBytes(const BundleComponent *component);
void BundleComponentClose(BundleComponent *component);
void BundleComponentHandleClose(BundleComponentHandle *handle);

#ifdef __cplusplus
}
#endif

#endif
