#ifndef ZIR_EMIT_H
#define ZIR_EMIT_H
#include "zir.h"
#include "zir_native.h"
#include "zir_body.h"

void EmitAssemblyHelpers(FILE *out, const ZirModule *module, const ZirFunction *function,
                         ZirTarget target, const char *result_type, const char *args);
/* Emits guarded native returns before the ordinary portable body. */
void EmitAssembly(FILE *out, const ZirModule *module, const ZirFunction *function,
                 ZirTarget target, const char *result_type);
/* Native-only bridge: set *available when a variant executes. The caller
 * adds that pointer to args and retains responsibility for portable fallback. */
void EmitAssemblyTryHelpers(FILE *out, const ZirModule *module, const ZirFunction *function,
                            ZirTarget target, const char *result_type, const char *args,
                            const char *available);
void EmitAssemblyTry(FILE *out, const ZirModule *module, const ZirFunction *function,
                     ZirTarget target, const char *result_type, const char *available);
/* A C availability predicate, without executing any author-supplied template. */
void EmitAssemblyAvailable(FILE *out, const ZirFunction *function, int statement);
void EmitAssemblyBlockHelpersAt(FILE *out, const ZirModule *module,
                                const ZirFunction *function, int statement);
typedef void (*ZirAssemblyOperandResolver)(void *context, int operand,
                                           char *out, size_t size);
int EmitAssemblyBlockMapped(FILE *out, const ZirModule *module,
                            const ZirFunction *function, int statement,
                            ZirAssemblyOperandResolver resolve, void *context);
void EmitAssemblyBlockHelpers(FILE *out, const ZirModule *module,
                              const ZirFunction *function, ZirTarget target);
/* Opens a native block plus its conditional portable body; caller closes
 * the two scopes when this returns nonzero. */
int EmitAssemblyBlock(FILE *out, const ZirModule *module,
                      const ZirFunction *function, int statement, ZirTarget target);
/* Resolve target symbol spelling and call ABI. Input contains identifiers and
 * captured argument names only, never a source expression to reinterpret. */
typedef void (*ZirResolveTarget)(void *context, const char *text, char *out, size_t size);
typedef int (*ZirAssemblyBlockEmitter)(FILE *out, const ZirModule *module,
                                      const ZirFunction *function, int statement,
                                      void *context);
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
void NativeCForeignName(const ZirModule *module, const ZirImport *foreign,
                        char *out, size_t size);
void NativeCModuleInitName(const ZirModule *module, char *out, size_t size);
int ModuleNeedsStartup(const ZirModule *module);
void NativeNamesReset(void);
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
/* C private ABI names and source-shaped arguments for array values. */
int ModuleUsesSlices(const ZirModule *module);
/* 1: public header types need this import, 2: only private header types do,
 * 0: implementation-only. Source visibility does not change. */
int NativeHeaderImportUse(const ZirModule *module, const ZirImport *import);
int ModuleUsesVecOperations(const ZirModule *module);
void TargetBindingName(const ZirFunction *fn, ZirTarget target,
                       const char *name, char *out, size_t size);
/* A C or C++ keyword, which header-visible C names avoid. */
int NativeHeaderKeyword(const char *name);
void TargetFieldName(const ZirType *record, ZirTarget target,
                     const char *name, char *out, size_t size);
void TargetGlobalName(const ZirModule *module, ZirTarget target,
                      const char *name, char *out, size_t size);
/* Sets native_name_collision on every global and constant whose native C
 * or Go name equals another value's or a procedure's. Returns 0 when
 * out of memory. */
int MarkNativeNameCollisions(ZirProgram **programs, int count);
void TargetDefineName(const ZirModule *module, ZirTarget target,
                      const char *name, char *out, size_t size);
void ArrayAbiName(const ZirFunction *fn, int parameter, char *out, size_t size);
void ArrayAbiArgs(const ZirFunction *fn, char *out, size_t size);
int FunctionHasParallelRegions(const ZirFunction *fn);
void EmitParallelWorkers(FILE *out, const ZirModule *module,
                         const ZirFunction *fn, ZirTarget target,
                         ZirResolveTarget resolve, void *context);
void EmitNumbers(FILE *out, const ZirModule *module, ZirTarget target);
/* Resolve runtime includes and numeric helpers in generated native text. */
#define RUNTIME_INCLUDES_MARK "/* ziran runtime includes */\n"
#define NUMBER_HELPERS_MARK "/* ziran numeric helpers */\n"
int EmitNativeHelpers(FILE *out, const char *text);
void EmitGoPrintSupport(FILE *out);
/* Appends each Go numeric helper that text calls and *written lacks. */
void EmitGoNumberHelpers(FILE *out, const char *text, unsigned *written);
/* A native program entry has a typed Ziran body plus a standard main adapter. */
int NativeProgramMain(const ZirModule *module, const ZirFunction *fn);
int CheckNativeEntry(const ZirFunction *fn, const char *target);
void EmitNativeMainDeclaration(FILE *out);
/* Declares a function bound to SYMBOL. Mach-O spells C symbols with a
 * leading underscore, which an assembler label must name explicitly. */
void EmitSymbolDeclaration(FILE *out, const char *result, const char *name,
                           const char *parameters, const char *symbol);
void EmitNativeMain(FILE *out, const ZirModule *module, const ZirFunction *fn,
                    ZirTarget target);
int FoldIntegerOperation(const char *op, const char *type, uint64_t a, uint64_t b,
                         uint64_t *result);
void EmitStringType(FILE *out);
/* Encode a checked literal for native expressions and constant byte arrays. */
void EmitStringLiteral(const ZirExpr *expr, ZirTarget target, char *out, size_t size);
void EmitSlotWrappers(FILE *out, const ZirModule *module, const ZirFunction *fn,
                         ZirTarget target, ZirResolveTarget resolver, void *context);
void EmitSlotType(FILE *out, const ZirType *slot, ZirTarget target,
                     ZirResolveTarget resolve_type, void *context);
int EmitBody(FILE *out, const ZirModule *module, const ZirFunction *fn,
                ZirTarget target, ZirResolveTarget resolve, void *context,
                ZirAssemblyBlockEmitter assembly, void *assembly_context);
/* Dense-output switch for the Go target: remove the inlined-expression length
 * bound so single-use temporaries fold without a readability cap. Default off. */
void EmitUseMinifiedOutput(int enabled);
#endif
