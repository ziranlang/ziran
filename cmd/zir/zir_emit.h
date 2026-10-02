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
/* Resolve compound Go types in the scope that owns their declaration. */
int NativeGoType(const ZirModule *module, const char *type,
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
/* 1: public header types need this import, 2: only private header types do,
 * 0: implementation-only. Source visibility does not change. */
int NativeHeaderImportUse(const ZirModule *module, const ZirImport *import);
int ModuleUsesVecOperations(const ZirModule *module);
void TargetBindingName(const ZirFunction *fn, ZirTarget target,
                       const char *name, char *out, size_t size);
void TargetFieldName(const ZirType *record, ZirTarget target,
                     const char *name, char *out, size_t size);
void TargetGlobalName(const ZirModule *module, ZirTarget target,
                      const char *name, char *out, size_t size);
/* Sets native_name_collision on every global and constant whose native C,
 * C++, or Go name equals another value's or a procedure's. Returns 0 when
 * out of memory. */
int MarkNativeNameCollisions(ZirProgram **programs, int count);
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
/* The line EmitNumbers writes where C and C++ numeric helpers go. */
#define NUMBER_HELPERS_MARK "/* ziran numeric helpers */\n"
int EmitResolveNumberHelpers(const char *path);
void EmitGoPrintSupport(FILE *out);
/* Appends each Go numeric helper that text calls and *written lacks. */
void EmitGoNumberHelpers(FILE *out, const char *text, unsigned *written);
int NativeMainReturnsStatus(const ZirFunction *fn);
int FoldIntegerOperation(const char *op, const char *type, uint64_t a, uint64_t b,
                         uint64_t *result);
void EmitStringType(FILE *out);
void EmitSlotWrappers(FILE *out, const ZirModule *module, const ZirFunction *fn,
                         ZirTarget target, ZirResolveTarget resolver, void *context);
void EmitSlotType(FILE *out, const ZirType *slot, ZirTarget target,
                     ZirResolveTarget resolve_type, void *context);
int EmitBody(FILE *out, const ZirModule *module, const ZirFunction *fn,
                ZirTarget target, ZirResolveTarget resolve, void *context);
/* Dense-output switch for the Go target: remove the inlined-expression length
 * bound so single-use temporaries fold without a readability cap. Default off. */
void EmitUseMinifiedOutput(int enabled);
#endif
