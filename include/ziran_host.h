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
    HOST_ARRAY,
    HOST_BORROWED_SLICE
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

typedef int (*BundleConsoleWrite)(void *context, int descriptor,
                                 const void *bytes, size_t size);

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
typedef void (*BundleImageRelease)(void *context, const unsigned char *data, size_t size);
/* Transfer the image with its matching release callback. The callback runs
 * once before return, including on failure. After decoding, asset copying
 * and hashing succeed, it runs before execution planning. A NULL release
 * callback rejects the call and leaves ownership with the caller. */
Bundle *BundleOpenOwnedBytes(const unsigned char *data, size_t size,
                             BundleImageRelease release, void *context);
#define ZIRAN_HOST_HAS_BUNDLE_READ_AT 1
/* Fill exactly size bytes at the absolute image offset and return size.
 * A different count fails verification. Return zero for a read error. */
typedef size_t (*BundleReadAt)(void *context, uint64_t offset, void *output, size_t size);
/* Opens a revision-28 app image of at most 256 MiB using bounded positional
 * reads. Keep the source stable during this call. The callback and context
 * are borrowed synchronously, and the verified bundle owns its decoded data.
 * On failure returns NULL. If given, unsupported is set to 1 only for a
 * recognized image outside this reader's profile (assets, module linkage,
 * or additional attachments), and 0 otherwise. The caller may choose to
 * retry that image with the owned-byte API, which verifies it independently. */
Bundle *BundleOpenReadAt(uint64_t size, BundleReadAt read_at, void *context, int *unsupported);
#define ZIRAN_HOST_HAS_BUNDLE_ENGINE_READ_AT 1
/* Like realloc, but always receives a smaller, nonzero size. On success
 * return storage whose first size bytes match the original and that the
 * runtime can release with free. Returning NULL leaves the original valid. */
typedef void *(*BundleShrink)(void *storage, size_t size);
/* Same verified reader and lifetime as BundleOpenReadAt. This bundle requires
 * the instruction engine and uses compact storage when the image permits it.
 * Use BUNDLE_BACKEND_ENGINE when instantiating it. Optional shrink only runs
 * during open when packing leaves unused storage. NULL uses realloc. */
Bundle *BundleOpenEngineReadAt(uint64_t size, BundleReadAt read_at, void *context,
                              int *unsupported, BundleShrink shrink);
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
/* Run on the instruction engine with platform output sent to write, including
 * during global initialization. The callback receives descriptor 1 or 2 and
 * returns nonzero after writing all bytes. The bytes are borrowed for the call.
 * The context must outlive the instance. A zero step budget is unbounded. */
BundleInstance *BundleInstantiateWithConsole(
    const Bundle *bundle, const HostBinding *bindings, size_t binding_count,
    int max_steps, BundleConsoleWrite write, void *context);
/* Compose backend selection and console routing. A non-null write requires
 * that the selected backend be the instruction engine. An unsupported
 * combination fails before initialization. A null write uses platform output. */
BundleInstance *BundleInstantiateWithOptionsAndConsole(
    const Bundle *bundle, const HostBinding *bindings, size_t binding_count,
    const BundleInstanceOptions *options, BundleConsoleWrite write, void *context);
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
/* Request a fresh full step budget after this HostCall successfully returns.
 * Returns nonzero only inside a typed host callback on the calling thread
 * during a bounded instruction-engine entry. Applies to the innermost call.
 * Initializers and invocations with no positive budget cannot renew. The host
 * should call this only after an event that it accepts as progress. The next
 * run's settings and the instance's budget mode are unaffected. */
int BundleHostRenewSteps(void);
/* Opt in to borrowing one zero-based, top-level string argument of an
 * already granted typed host function on a portable instruction instance.
 * Call between runs, after initialization. Returns nonzero on success.
 * The callback must read the bytes synchronously, never modify or retain
 * them, and must not reenter or close this instance. A result may refer to
 * the bytes for immediate serialization by the runtime. */
int BundleInstanceBorrowHostString(BundleInstance *instance, const char *module,
                                   const char *function, size_t parameter);
/* Opt in to a read-only, top-level slice of built-in fixed-width integers or floats on
 * an already granted portable instruction-engine host callback. The declared
 * function must use #reads. Register after initialization and between runs.
 * The callback receives HOST_BORROWED_SLICE, the declared slice type, length
 * in elements, and data containing contiguous little-endian scalar bytes.
 * Data can be unaligned and is NULL when length is zero. All other fields are
 * zero. Read with byte accesses or memcpy, using the declared element width.
 * The view is input-only and valid until the callback returns. The callback
 * must never write, save or return it, or reenter or close this instance.
 * Returns zero for an unsupported parameter, binding or execution backend.
 * An ordinary HOST_SLICE binding can still be used without opting in. */
int BundleInstanceBorrowHostSlice(BundleInstance *instance, const char *module,
                                  const char *function, size_t parameter);
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
