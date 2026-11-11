#ifndef ZIR_ASM_LOWER_H
#define ZIR_ASM_LOWER_H

#include "zir.h"
#include "compiler_lowered_ir.h"

typedef struct AssemblyProgram AssemblyProgram;

AssemblyProgram *AssemblyLower(const ZirProgram *source, const char *module,
                               const char *function);
const LirProgram *AssemblyView(const AssemblyProgram *program);
void AssemblyFree(AssemblyProgram *program);
int AssemblyPrintLir(const LirProgram *program, FILE *output);

#endif
