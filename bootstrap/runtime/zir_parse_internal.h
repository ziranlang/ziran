#ifndef ZIR_PARSE_INTERNAL_H
#define ZIR_PARSE_INTERNAL_H

#include "ziran_bootstrap_runtime.h"

int *ZirLawEvaluationAddress(void);

/* Recovery and parser operations share private names on every object format. */
#pragma GCC visibility push(hidden)
double ParseDecimalRange(const char *text, char **end, int *range_error);
void *ParseCacheEnter(void);
void ParseCacheLeave(void *previous);
void ParseRememberSource(const char *text);
ZirProgram *ParserWithRecovery(const char *path, const char *root, const char *source,
                               ZirCompileImportResolver resolver, void *context,
                               const char *const *defines, int define_count, const ZirImport *site);
ZirProgram *ParserParseOperationBody(const char *path, const char *root, const char *source,
                                    ZirCompileImportResolver resolver, void *context,
                                    const char *const *defines, int define_count, const ZirImport *site);
void die(const char *fmt, ...);
void die_at(ZirSourceSpan span, const char *fmt, ...);
void parse_stop(void);
#pragma GCC visibility pop

#endif
