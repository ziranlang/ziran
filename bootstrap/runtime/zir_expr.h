#ifndef ZIR_EXPR_H
#define ZIR_EXPR_H
#include "zir.h"

/* The Ziran parser borrows these inputs. Keep const in the C-facing adapters. */
int32_t CompilerParseExpr(ZirFunction *, ZirModule *, void *, ZirSourceSpan);
int32_t CompilerParseExprNoDefaults(ZirFunction *, ZirModule *, void *, ZirSourceSpan);
int32_t CompilerParseExprTyped(ZirFunction *, ZirModule *, void *, ZirSourceSpan, void *);
int32_t CompilerDefineIsAggregate(ZirModule *, ZirDefine *);
int32_t CompilerCallerLocationLiteral(ZirModule *, ZirSourceSpan, void *, size_t);
void CompilerStructureFunction(ZirFunction *, ZirModule *);

/* Parse with token precedence; unsupported syntax is an explicit unknown node. */
static inline int ParseExpr(ZirFunction *fn, const ZirModule *module,
                            const char *text, ZirSourceSpan span)
{
    return CompilerParseExpr(fn, (ZirModule *)module, (void *)text, span);
}
static inline int ParseExprNoDefaults(ZirFunction *fn, const ZirModule *module,
                                      const char *text, ZirSourceSpan span)
{
    return CompilerParseExprNoDefaults(fn, (ZirModule *)module, (void *)text, span);
}
static inline int ParseExprTyped(ZirFunction *fn, const ZirModule *module,
                                 const char *text, ZirSourceSpan span,
                                 const char *expected_type)
{
    return CompilerParseExprTyped(fn, (ZirModule *)module, (void *)text,
                                  span, (void *)expected_type);
}
/* Whether DEFINE's value is a record or array literal. Checked uses carry
 * the value itself, so a target declares no name for it. */
static inline int DefineIsAggregate(const ZirModule *module, const ZirDefine *define)
{
    return CompilerDefineIsAggregate((ZirModule *)module, (ZirDefine *)define);
}
static inline int CallerLocationLiteral(const ZirModule *module, ZirSourceSpan span,
                                        char *output, size_t capacity)
{
    return CompilerCallerLocationLiteral((ZirModule *)module, span, output, capacity);
}
static inline void StructureFunction(ZirFunction *fn, const ZirModule *module)
{
    CompilerStructureFunction(fn, (ZirModule *)module);
}
#endif
