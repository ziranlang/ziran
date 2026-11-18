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

typedef enum HostValueKind {
    HOST_VOID,
    HOST_INTEGER,
    HOST_UNSIGNED,
    HOST_REAL,
    HOST_STRING,
    HOST_RECORD,
    HOST_SLICE,
    HOST_POINTER,
    HOST_ARRAY
} HostValueKind;

typedef struct HostField HostField;

typedef struct HostValue {
    HostValueKind kind;
    const char *type;
    int64_t integer;
    uint64_t bits;
    double real;
    const unsigned char *data;
    size_t length;
    const HostField *fields;
    size_t field_count;
    struct HostValue *elements;
    void *pointer;
} HostValue;

struct HostField {
    const char *name;
    HostValue value;
};

/* Return nonzero after writing a result of the declared return type.
 * Record fields are in declaration order and carry their declared names and
 * types. Argument fields are borrowed for the call. A scalar, string, or record slice
 * argument has kind HOST_SLICE, length elements, and a mutable elements array.
 * Modify elements in place and keep the elements pointer and length intact;
 * the engine validates and copies every element back after the call. Returned
 * fields, collection elements and string bytes are copied after the callback
 * returns, before the next host call. Keep them valid until that next call;
 * a later call may reuse their storage. Strings copied into the instance retain
 * their bytes while reachable, including across BundleInstanceRun calls.
 * Fixed arrays have kind HOST_ARRAY and exactly their declared length.
 * Their elements may be inspected during the call; mutations to argument
 * elements are not copied back. A zero-length array has no elements.
 * A returned zero-length slice may have a null elements pointer. Longer
 * slices need elements for their declared length; oversized lengths fail.
 * A declared pointer type crosses as kind HOST_POINTER with the raw address
 * in `pointer`; the engine treats it as an opaque handle and never dereferences
 * it, so the host owns the storage behind it. */
typedef int (*HostCall)(void *context, const char *module,
                          const char *function, const HostValue *args,
                          int arg_count, HostValue *result);

typedef struct HostBinding {
    const char *module;
    const char *function;
    HostCall call;
    void *context;
} HostBinding;

typedef enum BundleBackendPolicy {
    BUNDLE_BACKEND_AUTO,
    BUNDLE_BACKEND_ENGINE,
    BUNDLE_BACKEND_NATIVE
} BundleBackendPolicy;

typedef enum BundleExecutionBackend {
    BUNDLE_EXECUTION_NONE,
    BUNDLE_EXECUTION_ENGINE,
    BUNDLE_EXECUTION_WASM,
    BUNDLE_EXECUTION_NATIVE_X86_64,
    BUNDLE_EXECUTION_NATIVE_AARCH64,
    BUNDLE_EXECUTION_NATIVE_ARMV7
} BundleExecutionBackend;

typedef struct BundleInstanceOptions {
    BundleBackendPolicy backend;
    int max_steps;
    /* Nonzero: the instance never limits its steps, so native code is built
     * without fuel checks, like an unbounded executable. max_steps must be
     * zero, and BundleInstanceLimitSteps has no effect on native code. */
    int unbounded;
} BundleInstanceOptions;

typedef struct BundleStateValue {
    const char *module;
    const char *name;
    const char *type;
    HostValue value;
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

/* Open, verify and plan a revision-28 executable ZX file. Close releases all
 * names. */
Bundle *BundleOpen(const char *path);
/* Validate an embedded/downloaded bundle without an application pathname.
 * Input bytes are borrowed only during this call. */
Bundle *BundleOpenBytes(const unsigned char *data, size_t size);
void BundleClose(Bundle *bundle);
/* Required capability names, in program order, are borrowed until
 * BundleClose. */
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
const char *BundleAssetName(const Bundle *bundle, size_t index);
const unsigned char *BundleAssetData(const Bundle *bundle, size_t index);
size_t BundleAssetSize(const Bundle *bundle, size_t index);

/* Every required capability must have exactly one binding before execution.
 * Integer/bool entry results are returned in result; void sets has_result=0.
 * For u64, cast result to uint64_t to recover all 64 unsigned bits. */
int BundleRun(const Bundle *bundle, const HostBinding *bindings,
              size_t binding_count, long long *result, int *has_result);

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
/* A positive, cumulative step budget covering global initialization and all
 * subsequent entry runs. Initialization failure returns NULL. */
BundleInstance *BundleInstantiateLimited(const Bundle *bundle,
                                         const HostBinding *bindings,
                                         size_t binding_count, int max_steps);
/* Select one backend for the whole instance before initialization. Engine and
 * Native are strict. Auto is fixed by the platform: Wasm in a browser, Native
 * only after that CPU backend is declared complete, and Engine otherwise.
 * Native x86-64 is strict-only while its parity work remains incomplete. A
 * program-preflight, compile, relocation, initialization, trap, or host
 * failure is never retried on another backend. */
BundleInstance *BundleInstantiateWithOptions(
    const Bundle *bundle, const HostBinding *bindings, size_t binding_count,
    const BundleInstanceOptions *options);
BundleExecutionBackend BundleInstanceBackend(const BundleInstance *instance);
/* Native preflight: compiles the bundle for this host on first use, sharing
 * code with any open bundle of the same content. An empty string when it
 * runs natively, else a diagnostic whose first word is its stable code. */
const char *BundleNativeProblem(const Bundle *bundle);
int BundleInstanceRun(BundleInstance *instance, long long *result,
                      int *has_result);
/* Bound each subsequent run to max_steps steps (function calls and loop
 * iterations), so a runaway bundle stops with an error. Zero is unbounded. */
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
