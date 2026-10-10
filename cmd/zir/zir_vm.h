#ifndef ZIRAN_ZIR_VM_H
#define ZIRAN_ZIR_VM_H

#include "zir.h"
#include "ziran_host.h"
#include <stddef.h>
#include <stdint.h>

/* Initial portable scalar execution subset. Verification precedes execution. */
int VmVerify(const ZirProgram *program, const char *entry_module,
                const char *entry_function);
int VmRun(const ZirProgram *program, const char *entry_module,
             const char *entry_function, long long *result, int *has_result);
int VmRunWithHost(const ZirProgram *program, const char *entry_module,
                  const char *entry_function, VmHostCall host, void *context,
                  long long *result, int *has_result);
/* Runs like VmRunWithHost but fails after MAX_STEPS statements, so a web
 * page running untrusted source cannot hang. Other runs are unbounded. */
int VmRunBounded(const ZirProgram *program, const char *entry_module,
                 const char *entry_function, VmHostCall host, void *context,
                 int max_steps, long long *result, int *has_result);

typedef struct VmInstance VmInstance;
/* Independent instances may execute concurrently over a program that remains
 * immutable and alive until they close. Each instance and host callback context
 * has one executing owner; concurrent runs of the same instance are unsupported.
 * Load/check/rewrite/free the program outside those concurrent runs. */
VmInstance *VmInstanceOpen(const ZirProgram *program,
                           const char *entry_module,
                           const char *entry_function,
                           VmHostCall host, void *context);
int VmInstanceRun(VmInstance *instance, long long *result, int *has_result);
/* Bounds later runs to MAX_STEPS statements; zero removes the bound. */
void VmInstanceLimitSteps(VmInstance *instance, int max_steps);
/* Lets host records omit or add fields; see Vm.match_fields_by_name. */
void VmInstanceMatchFieldsByName(VmInstance *instance, int enabled);
/* Live portable value storage, including text snapshots, excluding module IR. */
size_t VmInstanceLiveValueBytes(const VmInstance *instance);
void VmInstanceClose(VmInstance *instance);

#endif
