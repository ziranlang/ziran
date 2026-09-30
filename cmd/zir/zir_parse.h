#ifndef ZIRAN_ZIR_PARSE_H
#define ZIRAN_ZIR_PARSE_H

#include "zir.h"

/*
 * Parse a .zi source file into a ZirProgram. Returns a heap-allocated
 * program (caller frees with ProgramFree), or NULL on fatal error.
 * This is the shared Ziran frontend used by the IR and native backends.
 */
ZirProgram *parse_file(const char *path, const char *root);

/* Resolve already encountered imports before ordinary graph loading finishes.
 * A NULL condition requests every module import, for type inference. */
typedef int (*ZirCompileImportResolver)(void *context, ZirProgram *program,
                                       ZirModule *module,
                                       const char *source_path,
                                       const char *root,
                                       const char *condition);
ZirProgram *parse_file_with_imports(const char *path, const char *root,
                                    ZirCompileImportResolver resolver,
                                    void *context);
ZirProgram *parse_file_with_imports_defined(const char *path, const char *root,
                                            ZirCompileImportResolver resolver,
                                            void *context,
                                            const char *const *defines,
                                            int define_count);

/* Parse immutable embedded text with the same frontend and source spans. */
ZirProgram *parse_source_text(const char *path, const char *source);

/* Evaluate the pure translation-time integer/boolean subset after imports
 * have been linked. The result is stored as a literal before IR emission. */
/* Length of the `[]` or `*` in front of `$T` in a polymorphic parameter type,
 * 0 for a bare `$T`, or -1 when the type binds no type parameter. */
int TemplateBinderPrefix(const char *type);
/* Index of NAME (LENGTH bytes) in a polymorphic procedure's comma-separated
 * type parameter list, such as "A,B", or -1. */
int TemplateParameterIndex(const char *list, const char *name, size_t length);
int ParseExprNoDefaults(ZirFunction *fn, const ZirModule *module,
                const char *text, ZirSourceSpan span);
int ParseExprTyped(ZirFunction *fn, const ZirModule *module,
               const char *text, ZirSourceSpan span,
               const char *expected_type);
int EvaluateCompileExpression(const ZirModule *module, const char *source,
                              ZirSourceSpan span, int executing, long *value);
int EvaluateCompileConditionBound(const ZirModule *module, const char *source,
                                  ZirSourceSpan span,
                                  const char names[][ZIR_NAME_MAX],
                                  const char types[][ZIR_NAME_MAX],
                                  const char exprs[][ZIR_NAME_MAX],
                                  const long *values, int count, int *truth);
/* Aggregate results also return their declaring module as a borrowed pointer. */
int EvaluateCompileLiteral(const ZirModule *module, const char *source,
                           ZirSourceSpan span, int executing, char *literal,
                           size_t literal_size,
                           const ZirModule **type_owner);

/* Finish compile-time expressions whose conditions depend on linked imports.
 * With allow_deferred, unresolved conditions remain for a later pass. */
int LowerLinkedCompileExpressions(ZirModule *module, int allow_deferred);

int InstantiateGenericRecord(ZirType *instance, const ZirType *generic);
int SubstituteGenericType(const char *source, char *output, size_t capacity,
                          char params[][ZIR_NAME_MAX],
                          char actual[][ZIR_NAME_MAX], int count);

#endif /* ZIRAN_ZIR_PARSE_H */
