#ifndef ZIR_C_LOWER_H
#define ZIR_C_LOWER_H

#include "zir.h"

/* Cross-module symbol table for resolving alias-qualified calls. */
typedef struct ZirCModuleSyms {
    char module_stem[ZIR_PATH_MAX];  /* source path minus .zi */
    char module_slash[ZIR_PATH_MAX];
    struct {
        char source[ZIR_NAME_MAX];
        char c[ZIR_NAME_MAX * 3];
    } fns[256];
    int fn_count;
} ZirCModuleSyms;

/* Lower a ZirProgram to C source (.c/.h). restab/restab_count resolve
 * cross-module calls; pass NULL/0 to resolve same-module only. */
int c_lower(const ZirProgram *program, const char *root,
               const char *out_dir, const ZirCModuleSyms *restab,
               int restab_count, int linked);

/* Build the symbol table entry for one program into out. */
void c_build_syms(const ZirProgram *program, ZirCModuleSyms *out);

/* Full C name for a function (module prefix unless exported). */
void c_function_name(const ZirModule *m, const ZirFunction *fn,
                         char *dst, size_t dst_size);

#endif
