#ifndef ZIR_EMIT_H
#define ZIR_EMIT_H
#include "zir.h"

typedef enum ZirTarget { ZIR_C, ZIR_CPP, ZIR_GO } ZirTarget;
/* Resolve target symbol spelling and call ABI. Input contains identifiers and
 * captured argument names only, never a source expression to reinterpret. */
typedef void (*ZirResolveTarget)(void *context, const char *text, char *out, size_t size);
const char *TargetType(const char *type, ZirTarget target);
void NativeTypeName(const ZirModule *owner, const ZirType *type,
                    char *out, size_t size);
int NativeTypeAtUse(const ZirModule *module, const char *type,
                    char *out, size_t size);
int NativeEnumMemberName(const ZirModule *owner, const ZirType *type,
                         const char *member, char *out, size_t size);
int TypeHasZeroArray(const ZirModule *module, const char *type);
void NativeExportName(const ZirModule *module, const ZirFunction *fn,
                      char *out, size_t size);
void NativeCFunctionName(const ZirModule *module, const ZirFunction *fn,
                         char *out, size_t size);
void NativeCModuleInitName(const ZirModule *module, char *out, size_t size);
int ModuleNeedsStartup(const ZirModule *module);
void NativeGoModuleIdentity(const ZirProgram *const *programs, int count,
                            const ZirModule *module, char *file_stem,
                            size_t file_size, char *guard, size_t guard_size);
void NativeGoFunctionName(const ZirProgram *const *programs, int count,
                          const ZirModule *module, const ZirFunction *fn,
                          char *out, size_t size);
void NativeHeaderGuard(const char *stem, char *out, size_t size);
int ScalarLiteral(const char *type, const char *text, ZirTarget target,
                      ZirSourceSpan span, char *out, size_t size);
typedef int (*ZirGlobalScalarRewrite)(const ZirModule *module,
                                      const char *source, char *out,
                                      size_t size, void *context);
typedef int (*ZirGlobalTypeRewrite)(const ZirModule *module,
                                    const char *source, char *out,
                                    size_t size, void *context);
typedef void (*ZirGlobalFieldRewrite)(const ZirType *record,
                                      const char *source, char *out,
                                      size_t size, void *context);
/* Return 1 for a lowered record/array initializer, 0 for a scalar expression,
 * and -1 when a compound initializer cannot be represented on the target. */
int EmitGlobalInitializer(const ZirModule *module, const ZirGlobal *global,
                          ZirTarget target, ZirGlobalScalarRewrite scalar,
                          ZirGlobalTypeRewrite type_name,
                          ZirGlobalFieldRewrite field_name, void *context,
                          char *out, size_t size);
void EmitGlobalSlotWrappers(FILE *out, const ZirModule *module,
                            const ZirGlobal *global, ZirTarget target,
                            ZirResolveTarget resolver, void *context);
/* C/C++ private ABI names and source-shaped arguments for array values. */
int ArrayValueType(const char *type);
int ModuleUsesSlices(const ZirModule *module);
int ModuleUsesVecOperations(const ZirModule *module);
void TargetBindingName(const ZirFunction *fn, ZirTarget target,
                       const char *name, char *out, size_t size);
void TargetFieldName(const ZirType *record, ZirTarget target,
                     const char *name, char *out, size_t size);
void TargetGlobalName(const ZirModule *module, ZirTarget target,
                      const char *name, char *out, size_t size);
int NativeGlobalNameConflict(const ZirModule *left, const ZirGlobal *a,
                             const ZirModule *right, const ZirGlobal *b);
int NativeDefineNameConflict(const ZirModule *left, const ZirDefine *a,
                             const ZirModule *right, const ZirDefine *b);
int NativeValueNameConflict(const ZirModule *global_module,
                            const ZirGlobal *global,
                            const ZirModule *define_module,
                            const ZirDefine *define);
int NativeFunctionValueNameConflict(const ZirProgram *const *programs,
                                    int count, const ZirModule *function_module,
                                    const ZirFunction *function,
                                    const ZirModule *value_module,
                                    const char *value_name, int is_global);
void TargetDefineName(const ZirModule *module, ZirTarget target,
                      const char *name, char *out, size_t size);
void ArrayAbiName(const ZirFunction *fn, int parameter, char *out, size_t size);
void ArrayAbiArgs(const ZirFunction *fn, char *out, size_t size);
int CanEmitBody(const ZirModule *module, const ZirFunction *fn);
int FunctionHasParallelRegions(const ZirFunction *fn);
void EmitParallelWorkers(FILE *out, const ZirModule *module,
                         const ZirFunction *fn, ZirTarget target,
                         ZirResolveTarget resolve, void *context);
void EmitNumbers(FILE *out, const ZirModule *module, ZirTarget target);
void EmitNumberSupport(FILE *out, ZirTarget target, const char *prefix);
void EmitGoPrintSupport(FILE *out, const ZirModule *module);
int NativeMainReturnsStatus(const ZirFunction *fn);
int FoldIntegerOperation(const char *op, const char *type, uint64_t a, uint64_t b,
                         uint64_t *result);
void EmitStringType(FILE *out);
void EmitSlotWrappers(FILE *out, const ZirModule *module, const ZirFunction *fn,
                         ZirTarget target, ZirResolveTarget resolver, void *context);
void EmitSlotType(FILE *out, const ZirType *slot, ZirTarget target,
                     ZirResolveTarget resolve_type, void *context);
/* number_support overrides the module-local numeric helper prefix when a
 * package shares one definition. */
int EmitBody(FILE *out, const ZirModule *module, const ZirFunction *fn,
                ZirTarget target, ZirResolveTarget resolve, void *context,
                const char *number_support);
/* Dense-output switch for the Go target: remove the inlined-expression length
 * bound so single-use temporaries fold without a readability cap. Default off. */
void EmitUseMinifiedOutput(int enabled);
#endif
