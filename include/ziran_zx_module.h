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
void ZxModuleLoaderClose(ZxModuleLoader *loader);
const char *ZxModuleLoaderError(const ZxModuleLoader *loader);

int ZxModuleProgramNeedsLoader(const Bundle *bundle);
int ZxModuleProgramHasExternalHosts(const Bundle *bundle);
/* BUNDLE and caller-owned binding contexts must outlive the program. */
ZxModuleProgram *ZxModuleProgramOpen(ZxModuleLoader *loader, const Bundle *bundle,
                                     const HostBinding *bindings, size_t count);
int ZxModuleProgramRun(ZxModuleProgram *program, long long *result, int *has_result);
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
