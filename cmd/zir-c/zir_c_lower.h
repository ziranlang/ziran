#ifndef ZIR_C_LOWER_H
#define ZIR_C_LOWER_H

#include "zir.h"

/* Cross-module symbol table for resolving alias-qualified calls. */
typedef struct ZirCFunctionSym {
    char source[ZIR_NAME_MAX];
    char c[ZIR_NAME_MAX * 3];
} ZirCFunctionSym;

typedef struct ZirCModuleSyms {
    char module_stem[ZIR_PATH_MAX];  /* source path minus .zi */
    char module_slash[ZIR_PATH_MAX];
    ZirCFunctionSym *fns;            /* one entry per function, no fixed cap */
    int fn_count;
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

/* Full C name for a function (module prefix unless exported). */
void c_function_name(const ZirModule *m, const ZirFunction *fn,
                         char *dst, size_t dst_size);

#endif
