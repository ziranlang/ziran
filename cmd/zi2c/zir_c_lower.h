#ifndef ZIR_C_LOWER_H
#define ZIR_C_LOWER_H

#include "zir.h"

/* Cross-module symbol table for resolving alias-qualified calls. */
typedef struct ZirCFunctionSym {
    char source[ZIR_NAME_MAX];
    char c[ZIR_NAME_MAX * 3];
} ZirCFunctionSym;

typedef struct ZirCHeaderUse {
    const ZirModule *module;
    unsigned char use; /* 1 public layout, 2 private layout */
} ZirCHeaderUse;

typedef struct ZirCHeaderUses {
    ZirCHeaderUse *items;
    int count;
} ZirCHeaderUses;

typedef struct ZirCModuleSyms {
    char module_stem[ZIR_PATH_MAX];  /* source path minus .zi */
    char module_slash[ZIR_PATH_MAX];
    ZirCFunctionSym *fns;            /* one entry per function, no fixed cap */
    int fn_count;
    /* The program's first module, the one the stem names, and its header's
     * declaration dependencies, worked out once for every lowering. */
    const ZirModule *module;
    ZirCHeaderUses header_uses;
} ZirCModuleSyms;

/* Lower a ZirProgram to C source (.c/.h). restab/restab_count resolve
 * cross-module calls; pass NULL/0 to resolve same-module only. */
int c_lower(const ZirProgram *program, const char *root,
               const char *out_dir, const ZirCModuleSyms *restab,
               int restab_count, int linked);

/* Build the symbol table entry for one program into out. Returns 0 when
 * memory runs out. Release each entry with c_free_syms. */
int c_build_syms(const ZirProgram *program, ZirCModuleSyms *out);
void c_free_syms(ZirCModuleSyms *syms, int count);

#endif
