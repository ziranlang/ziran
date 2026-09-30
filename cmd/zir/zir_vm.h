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
VmInstance *VmInstanceOpen(const ZirProgram *program,
                           const char *entry_module,
                           const char *entry_function,
                           VmHostCall host, void *context);
int VmInstanceRun(VmInstance *instance, long long *result, int *has_result);
/* Bounds later runs to MAX_STEPS statements; zero removes the bound. */
void VmInstanceLimitSteps(VmInstance *instance, int max_steps);
/* Live portable value storage, excluding module IR and interned strings. */
size_t VmInstanceLiveValueBytes(const VmInstance *instance);
void VmInstanceClose(VmInstance *instance);

#endif
