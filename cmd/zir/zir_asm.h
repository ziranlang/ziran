#ifndef ZIR_ASM_H
#define ZIR_ASM_H

#include "zir.h"

enum { ASSEMBLY_OPERANDS = 30 };

typedef struct AssemblyOperand {
    const char *constraint;
    const char *alias;
    const char *binding;
    const char *type;
} AssemblyOperand;

/* Checked operand metadata shared by native emitters. Outputs precede inputs;
 * function outputs bind result fields (or "out"), block outputs bind locals.
 * Names, types and decoded template bytes borrow the immutable text store. */
typedef struct AssemblyVariant {
    const char *target;
    const char *text;
    const char *clobbers;
    const char *options;
    int outputs, inputs, record;
    AssemblyOperand operands[ASSEMBLY_OPERANDS];
} AssemblyVariant;

/* Advance over one checked variant. statement=-1 selects the procedure's
 * variants; otherwise it selects a local assembly block. 1=value, 0=end,
 * -1=invalid metadata. The frontend remains responsible for semantic checks. */
int AssemblyVariantNext(const ZirModule *module, const ZirFunction *function,
                         int statement, const char **cursor, AssemblyVariant *variant);
/* Inputs with a possible memory alternative must retain their storage when
 * an isolated feature helper is called. Unknown target classes are cautious. */
int AssemblyInputNeedsStorage(const char *constraint);

#endif
