#ifndef ZIR_EXPR_H
#define ZIR_EXPR_H
#include "zir.h"

/* Parse with token precedence; unsupported syntax is an explicit unknown node. */
int ParseExpr(ZirFunction *fn, const ZirModule *module, const char *text,
                 ZirSourceSpan span);
int ParseExprNoDefaults(ZirFunction *fn, const ZirModule *module,
                        const char *text, ZirSourceSpan span);
int ParseExprTyped(ZirFunction *fn, const ZirModule *module,
               const char *text, ZirSourceSpan span,
               const char *expected_type);
/* Whether DEFINE's value is a record or array literal. Checked uses carry
 * the value itself, so a target declares no name for it. */
int DefineIsAggregate(const ZirModule *module, const ZirDefine *define);
int CallerLocationLiteral(const ZirModule *module, ZirSourceSpan span,
                          char *output, size_t capacity);
void StructureFunction(ZirFunction *fn, const ZirModule *module);
#endif
