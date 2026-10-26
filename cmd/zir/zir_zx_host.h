#ifndef ZIR_ZX_HOST_H
#define ZIR_ZX_HOST_H

#include "zir.h"

#include <stdio.h>

typedef struct ZxCapabilityName {
    const char *module;
    const char *function;
    /* Written back: the C symbol the module's code calls. When several
     * modules declare that symbol, each must call zx_capability_<index>
     * instead, as the portable VM routes each module's call to its own
     * binding; `renamed` is set for those. */
    char symbol[ZIR_NAME_MAX];
    int renamed;
} ZxCapabilityName;

/* Write the C definitions natively compiled bundles call for their host
 * capabilities. Capability i calls ZxNativeHost(i, ...). The definitions
 * include the module headers C lowering wrote, so a signature that differs
 * from the generated declaration fails to compile. Returns 0 with a message
 * in error when a capability passes a type the host ABI cannot carry.
 * allow_pruned is only for a verified program linked to its entry: library
 * capabilities removed by that pass keep their binding indices but need no
 * trampoline. Missing declarations in an unpruned program remain errors. */
int ZxWriteHostTrampolines(const ZirProgram *program,
                           ZxCapabilityName *capabilities,
                           size_t capability_count, int allow_pruned, FILE *out,
                           char *error, size_t error_size);

/* Declarations of the engine services the trampolines call. */
extern const char ZxNativeServicesHeader[];

#endif
