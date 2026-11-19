#ifndef ZIR_ZX_MODULE_INTERNAL_H
#define ZIR_ZX_MODULE_INTERNAL_H

#include "zir_host_exec.h"

const ExecBundle *BundleVerifiedExec(const Bundle *bundle);
int BundleCopyHostValue(const HostValue *input, HostValue *output);
void BundleFreeHostValue(HostValue *value);

const char *ExecBundleModuleLinkage(const ExecBundle *bundle, size_t *length);
int ExecBundleModuleEntry(const ExecBundle *bundle);
/* parameter == -1 selects the result. A NULL return means that the type
 * is unsupported or disagrees with the verified LIR. Void returns "void". */
const char *ExecBundleModuleType(const ExecBundle *bundle, size_t host, int parameter);
int ExecBundleModuleTypeEqual(const ExecBundle *left, size_t left_host, int left_parameter,
                              const ExecBundle *right, size_t right_host, int right_parameter);
int ExecBundleModuleParameterCount(const ExecBundle *bundle, size_t host);

#endif
