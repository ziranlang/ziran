/* Native execution of the machine-pipeline subset behind the bundle API. */
#ifndef ZIR_HOST_NATIVE_H
#define ZIR_HOST_NATIVE_H

typedef struct ExecBundle ExecBundle;
typedef struct NativeBundleCode NativeBundleCode;
typedef struct NativeInstance NativeInstance;

NativeBundleCode *NativeBundleCodeOpen(const ExecBundle *bundle,
                                       const char **problem);
void NativeBundleCodeClose(NativeBundleCode *code);
NativeInstance *NativeInstanceOpen(const NativeBundleCode *code,
                                   long long max_steps);
int NativeInstanceRun(NativeInstance *instance, long long *result,
                      int *has_result);
void NativeInstanceLimitSteps(NativeInstance *instance, long long max_steps);
void NativeInstanceClose(NativeInstance *instance);

#endif
