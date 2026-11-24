#ifndef ZIR_PROOF_H
#define ZIR_PROOF_H
#include "zir.h"

int PrepareLawProofs(ZirProgram **programs, int count);
ZirLaw *FindVisibleLaw(const ZirModule *module, const char *name, const ZirModule **owner);
int EvaluateTheorem(const ZirModule *module, const ZirLaw *law,
                    char *detail, size_t size);
/* Used by the source parser; block contents are parsed into certificates
 * after imports have been resolved. */
int ParseProofDeclaration(ZirModule *module, const char *source,
                          ZirSourceSpan span);
#endif
