#ifndef ZIR_GO_ASM_H
#define ZIR_GO_ASM_H

#include "zir.h"

typedef struct GoAssembly GoAssembly;
GoAssembly *GoAssemblyOpen(const ZirProgram *const *programs, int count,
                            const ZirModule *module);
void GoAssemblyFunction(FILE *out, const GoAssembly *assembly,
                         const ZirFunction *function);
int GoAssemblyBlock(FILE *out, const ZirModule *module, const ZirFunction *function,
                     int statement, void *assembly);
int GoAssemblyWrite(const GoAssembly *assembly, const char *path, const char *package);
int GoAssemblyAvailability(const char *directory, const char *package, ZirSourceSpan span);
void GoAssemblyClose(GoAssembly *assembly);

#endif
