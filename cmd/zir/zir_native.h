#ifndef ZIR_NATIVE_H
#define ZIR_NATIVE_H
#include "zir.h"

typedef enum ZirTarget { ZIR_C, ZIR_CPP, ZIR_GO } ZirTarget;
const char *TargetType(const char *type, ZirTarget target);

/* Run after semantic checking and before native emission. */
int PrepareNativeNames(ZirProgram **programs, int count);
#endif
