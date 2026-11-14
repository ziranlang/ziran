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
/* The host interface section's text (see zir_asm_lower.c), or NULL when the
 * program has no host capability or the text could not be built. */
const char *AssemblyHostInterface(AssemblyProgram *program, size_t *length);

#endif
