/* Native execution of verified ZX behind the bundle API: the machine
 * pipeline's x86-64 image, loaded into W^X memory on linux-x86_64. Every
 * other host has no native backend, and asking for it names that gap. */
#ifndef ZIR_HOST_NATIVE_H
#define ZIR_HOST_NATIVE_H

#include <stddef.h>

typedef struct ExecBundle ExecBundle;
struct HostBinding;
typedef const struct HostBinding *(*NativeFindBinding)(
    void *context, const char *module, const char *function);
typedef struct NativeBundleCode NativeBundleCode;
typedef struct NativeInstance NativeInstance;

/* One bundle's native code, compiled on first use. Code compiled for the
 * same content digest, CPU and backend is shared while any bundle holds it
 * and kept briefly after, so reopening a file does not compile it again. */
typedef struct NativeBundleSlot {
    NativeBundleCode *code;
    int ready;
    char problem[512];
} NativeBundleSlot;

/* The code, compiling it under a lock on first use; NULL when native
 * execution cannot run this bundle. UNBOUNDED code has no fuel checks, so
 * its instances take no step limit; a slot holds one kind. */
const NativeBundleCode *NativeBundleSlotCode(NativeBundleSlot *slot, const ExecBundle *bundle,
                                             int unbounded);
/* "" when the code exists, else the stable preflight diagnostic. */
const char *NativeBundleSlotProblem(NativeBundleSlot *slot, const ExecBundle *bundle);
void NativeBundleSlotClose(NativeBundleSlot *slot);

/* Instances name their own bundle: code is shared between bundles with the
 * same verified program. Host bindings and capability lookups are per bundle.
 * Initializers run here; a trap in one prints its diagnostic. */
NativeInstance *NativeInstanceOpen(const NativeBundleCode *code, const ExecBundle *bundle,
                                   NativeFindBinding find, void *context,
                                   long long max_steps);
/* A trap prints a zx.runtime diagnostic and leaves the instance terminal. */
int NativeInstanceRun(NativeInstance *instance, long long *result,
                      int *has_result);
void NativeInstanceLimitSteps(NativeInstance *instance, long long max_steps);
void NativeInstanceMatchFieldsByName(NativeInstance *instance, int enabled);
/* Module state, as the engine gives and takes it: named globals as host
 * values. A store converts every value before any global changes. */
struct HostValue;
size_t NativeInstanceStateCount(const NativeInstance *instance);
int NativeInstanceStateAt(NativeInstance *instance, size_t index, const char **module,
                          const char **name, const char **type, struct HostValue *value);
int NativeInstanceStateStore(NativeInstance *instance, const struct HostValue *const *values);
/* Bytes of globals and resizable storage the instance holds, as the engine
 * counts live value bytes. */
size_t NativeInstanceLiveBytes(const NativeInstance *instance);
void NativeInstanceClose(NativeInstance *instance);

#endif
