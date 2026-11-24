#ifndef ZIR_SEMANTIC_H
#define ZIR_SEMANTIC_H
#include "zir.h"
/* Position-independent checked declaration identity, shared by API/diagnostics. */
void SemanticId(const ZirModule *module, const char *kind, const char *name,
                const char *signature, const char *result, char id[68]);
void FunctionSemanticId(const ZirModule *module, const ZirFunction *function, char id[68]);
#endif
