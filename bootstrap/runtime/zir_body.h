#ifndef ZIR_BODY_H
#define ZIR_BODY_H
#include "zir.h"
/* Eligibility for the common typed-body emitters, independent of semantic
 * validity and the portable verifier's target-specific executable contract. */
int CanEmitBody(const ZirModule *module, const ZirFunction *function);
#endif
