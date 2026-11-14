/*
 * zir_host_exec.h - revision-28 executable ZX behind the embedding API
 * (include/ziran_host.h). The engine is generated Ziran; this interface keeps
 * its headers out of the rest of the library.
 */
#ifndef ZIR_HOST_EXEC_H
#define ZIR_HOST_EXEC_H

#include "ziran_host.h"

#include <stddef.h>

typedef struct ExecBundle ExecBundle;
typedef struct ExecInstance ExecInstance;

/* Whether the bytes' header names revision 28. */
int ExecBundleBytes(const unsigned char *bytes, size_t size);
/* Reads, verifies and plans a revision-28 executable; NULL after a
 * diagnostic naming `name` when it cannot. */
ExecBundle *ExecBundleOpen(const unsigned char *bytes, size_t size, const char *name);
void ExecBundleClose(ExecBundle *bundle);
/* Host capabilities in program order; the platform console services the
 * engine binds itself are not among them. */
size_t ExecBundleCapabilityCount(const ExecBundle *bundle);
const char *ExecBundleCapabilityModule(const ExecBundle *bundle, size_t index);
const char *ExecBundleCapabilityFunction(const ExecBundle *bundle, size_t index);
const char *ExecBundleEntryModule(const ExecBundle *bundle);
const char *ExecBundleEntryFunction(const ExecBundle *bundle);

/* Gives the binding for a capability, or NULL. */
typedef const HostBinding *(*ExecFindBinding)(void *context, const char *module, const char *function);
/* An instance with every capability bound through `find`, or NULL when one
 * is missing. max_steps > 0 bounds the work of all runs together. */
ExecInstance *ExecInstanceOpen(const ExecBundle *bundle, ExecFindBinding find, void *context,
                               long long max_steps);
int ExecInstanceRun(ExecInstance *instance, long long *result, int *has_result);
void ExecInstanceLimitSteps(ExecInstance *instance, long long max_steps);
void ExecInstanceMatchFieldsByName(ExecInstance *instance, int enabled);
void ExecInstanceClose(ExecInstance *instance);

#endif
