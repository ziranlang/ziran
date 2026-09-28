#ifndef ZIR_CPP_LOWER_H
#define ZIR_CPP_LOWER_H

#include "zir.h"

/* Cross-module symbol table for resolving alias-qualified calls. */
typedef struct ZirCppModuleSyms {
    char module_stem[ZIR_PATH_MAX];  /* source path minus .zi */
    char module_slash[ZIR_PATH_MAX];
    struct {
        char source[ZIR_NAME_MAX];
        char c[ZIR_NAME_MAX * 3];
    } fns[256];
    int fn_count;
} ZirCppModuleSyms;

/* Lower a ZirProgram to C++ source (.cpp/.hpp). restab/restab_count resolve
 * cross-module calls; pass NULL/0 to resolve same-module only. */
int cpp_lower(const ZirProgram *program, const char *root,
               const char *out_dir, const ZirCppModuleSyms *restab,
               int restab_count);

/* Build the symbol table entry for one program into out. */
void cpp_build_syms(const ZirProgram *program, ZirCppModuleSyms *out);

/* Full C name for a function (module prefix unless exported). */
void cpp_function_name(const ZirModule *m, const ZirFunction *fn,
                         char *dst, size_t dst_size);

#endif
