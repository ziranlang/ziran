#ifndef ZIRAN_ZX_MODULE_H
#define ZIRAN_ZX_MODULE_H

#include "ziran_host.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZxModuleLoader ZxModuleLoader;
typedef struct ZxModuleProgram ZxModuleProgram;

/* Each root is searched for LOGICAL_NAME.zx. Distinct matching files are
 * ambiguous. Resolved libraries are cached for this loader's lifetime.
 * Roots and options are copied. Serialize access to one loader. Close its
 * programs before closing it. */
ZxModuleLoader *ZxModuleLoaderOpen(const char *const *roots, size_t root_count,
                                   const BundleInstanceOptions *options);
/* Explicitly allow loaded libraries to use these ordinary host bindings.
 * The binding array and names stay unchanged for the life of the loader,
 * and their contexts must outlive it. App
 * entry bindings are still supplied to ZxModuleProgramOpen. */
ZxModuleLoader *ZxModuleLoaderOpenWithBindings(const char *const *roots, size_t root_count,
                                              const BundleInstanceOptions *options,
                                              const HostBinding *bindings, size_t count);

typedef enum ZxModuleResolveResult {
    ZX_MODULE_IMAGE_MISSING,
    ZX_MODULE_IMAGE_FOUND,
    ZX_MODULE_IMAGE_ERROR
} ZxModuleResolveResult;

typedef struct ZxModuleImage {
    const unsigned char *data;
    size_t size;
    BundleImageRelease release;
} ZxModuleImage;

/* PATH is borrowed for the call. On FOUND, the immutable bytes stay valid
 * through synchronous verification. If supplied, release is called once
 * with the resolver context, even when the loader rejects the image. It
 * can run before execution planning, as with BundleOpenOwnedBytes. */
typedef ZxModuleResolveResult (*ZxModuleResolve)(void *context, const char *path,
                                                ZxModuleImage *image);

typedef struct ZxModuleLoaderConfig {
    const char *const *roots;
    size_t root_count;
    BundleInstanceOptions options;
    const HostBinding *library_bindings;
    size_t library_binding_count;
    ZxModuleResolve resolve;
    void *resolve_context;
    BundleConsoleWrite write;
    void *console_context;
} ZxModuleLoaderConfig;

/* With a resolver, all library reads, including replacement, go through it.
 * MISSING advances the search. ERROR stops it. Multiple guest candidates for
 * the same name are ambiguous. A write callback serves the app and all
 * libraries, including initializers, and needs an instruction-engine backend.
 * Callback contexts must outlive the loader. Resolver, release and console
 * callbacks must not reenter the loader. Roots, options and callback pointers
 * are copied. Library grants are borrowed as with OpenWithBindings. */
ZxModuleLoader *ZxModuleLoaderOpenConfigured(const ZxModuleLoaderConfig *config);
void ZxModuleLoaderClose(ZxModuleLoader *loader);
const char *ZxModuleLoaderError(const ZxModuleLoader *loader);

int ZxModuleProgramNeedsLoader(const Bundle *bundle);
int ZxModuleProgramHasExternalHosts(const Bundle *bundle);
/* BUNDLE and caller-owned binding contexts must outlive the program. */
ZxModuleProgram *ZxModuleProgramOpen(ZxModuleLoader *loader, const Bundle *bundle,
                                     const HostBinding *bindings, size_t count);
int ZxModuleProgramRun(ZxModuleProgram *program, long long *result, int *has_result);
/* With a non-null control, the loader must select BUNDLE_BACKEND_ENGINE.
 * Both calls borrow the control until return, including while guest-loaded
 * libraries initialize and run. Each program and its loader stay owned by
 * the calling thread. A supervisor uses only the control, and must arrange
 * for a blocked host callback to return. Open returns NULL on cancellation
 * and reports ZX_RUN_CANCELLED through the optional status pointer. Run
 * follows the lifetime and result rules of BundleInstanceRunControlled. */
ZxModuleProgram *ZxModuleProgramOpenControlled(
    ZxModuleLoader *loader, const Bundle *bundle, const HostBinding *bindings,
    size_t binding_count, const ZxRunControl *control, ZxRunStatus *status);
ZxRunStatus ZxModuleProgramRunControlled(ZxModuleProgram *program, long long *result,
                                        int *has_result, const ZxRunControl *control);
/* Apply BundleInstanceBorrowHostString to a binding of this app instance. */
int ZxModuleProgramBorrowHostString(ZxModuleProgram *program, const char *module,
                                   const char *function, size_t parameter);
/* Apply BundleInstanceBorrowHostSlice to a binding of this app instance. */
int ZxModuleProgramBorrowHostSlice(ZxModuleProgram *program, const char *module,
                                  const char *function, size_t parameter);
BundleExecutionBackend ZxModuleProgramBackend(const ZxModuleProgram *program);
void ZxModuleProgramClose(ZxModuleProgram *program);

/* Zero is the invalid handle. Each load creates an independent instance.
 * Handles can be used only with their loader. */
uint64_t ZxModuleLoad(ZxModuleLoader *loader, const char *logical_name);
int ZxModuleRelease(ZxModuleLoader *loader, uint64_t handle);
size_t ZxModuleExportCount(const ZxModuleLoader *loader, uint64_t handle);
/* Export names and types are borrowed until replacement or release. */
const char *ZxModuleExportName(const ZxModuleLoader *loader, uint64_t handle, size_t ordinal);
size_t ZxModuleParameterCount(const ZxModuleLoader *loader, uint64_t handle, size_t ordinal);
const char *ZxModuleParameterType(const ZxModuleLoader *loader, uint64_t handle,
                                  size_t ordinal, size_t index);
const char *ZxModuleResultType(const ZxModuleLoader *loader, uint64_t handle, size_t ordinal);
/* The result is borrowed until the next invocation, replacement, or release
 * of this instance. The HostValue ABI carries portable scalars, strings,
 * records, and fixed arrays. Borrowed slices, pointers, and procedures are
 * refused before initialization. */
int ZxModuleCall(ZxModuleLoader *loader, uint64_t handle, size_t ordinal,
                 const HostValue *args, size_t count, HostValue *result);
/* Replace only this instance from PATH, retaining its handle. With no callback,
 * migrate matching state by module, global name and type. Refuse a changed
 * interface, live child handles in either instance, or state that cannot be
 * migrated. A migration callback must not call back into this loader. Effects
 * of candidate host calls are controlled by the embedder. */
int ZxModuleReplace(ZxModuleLoader *loader, uint64_t handle, const char *path,
                    BundleStateMigrate migrate, void *context);

#ifdef __cplusplus
}
#endif
#endif
