#ifndef ZIR_CHECK_H
#define ZIR_CHECK_H
#include "zir.h"

/* Resolve scalar expression types and lexical bindings. */
int CheckPrograms(ZirProgram **programs, int count);
int LinkImports(ZirProgram **programs, int count);
int LowerFileScopeUsing(ZirModule *module, char *source, size_t capacity,
                        ZirSourceSpan span);
const char *ScalarType(const char *type);
int JaiTypeSpelling(ZirSourceSpan span, const char *type);
int TypeOfOperand(const char *source, char *operand, size_t capacity);
int InferExpressionType(const ZirModule *module, const char *expression,
                        ZirSourceSpan span, char *type, size_t capacity);
int TypeLayout(const ZirModule *module, const char *type,
               size_t *size, size_t *alignment);
int MapTypePartsAtUse(const ZirModule *module, const char *name,
                      char *key, size_t key_size, char *value, size_t value_size);
#endif
