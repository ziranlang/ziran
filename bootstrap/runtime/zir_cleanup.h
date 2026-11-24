#ifndef ZIR_CLEANUP_H
#define ZIR_CLEANUP_H
#include "zir.h"

/* Lower lexical cleanup once, before backend emission. Returns zero on error. */
int NormalizeJaiBodies(ZirFunction *fn);
int BindJaiLoopControls(ZirFunction *fn);
int LowerCleanup(ZirFunction *fn);
int LowerJaiFor(ZirFunction *fn, const ZirModule *module);
#endif
