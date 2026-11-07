/*
 * zir.h - Ziran intermediate representation.
 *
 * ZIR is the shared compiler representation for .zi source and native
 * backends. This module owns the tree shape, allocation, and source spans.
 */
#ifndef ZIRAN_ZIR_H
#define ZIRAN_ZIR_H

#include <stdio.h>
#include <stdint.h>
#include "zir_profile.h"
#include "zir_context.h"
/* The IR records, their kinds and the buffer sizes are Ziran, in
 * cmd/compiler_ir.zi; the C below adds helpers and declarations. */
#include "compiler_ir.h"


static inline int
StatementHasAssembly(const ZirStmt *statement)
{
    return statement->kind == ZIR_STMT_BLOCK_OPEN &&
        statement->assembly_text && *statement->assembly_text;
}

enum { ZIR_INLINE_NONE, ZIR_INLINE_ALWAYS, ZIR_INLINE_COLD };

/* A function's parameters and defaulted parameters as written. They are
 * kept text (KeepParameters); a zeroed function has none, so read them
 * through these. */
static inline const char *
FunctionArgs(const ZirFunction *fn)
{
    return fn->args_text ? fn->args_text : "";
}

static inline const char *
FunctionDefaultArgs(const ZirFunction *fn)
{
    return fn->default_args_text ? fn->default_args_text : "";
}

static inline void
ExprWalkPush(ZirExprWalk *walk, int index)
{
    if(index >= 0 && index < walk->fn->expr_count && walk->count < walk->fn->expr_count)
        walk->pending[walk->count++] = index;
}

static inline int
ExprWalkNext(ZirExprWalk *walk)
{
    if(walk->count == 0)
        return -1;
    int index = walk->pending[--walk->count];
    const ZirExpr *expr = &walk->fn->exprs[index];
    ExprWalkPush(walk, expr->next_sibling);
    ExprWalkPush(walk, expr->third);
    ExprWalkPush(walk, expr->right);
    ExprWalkPush(walk, expr->left);
    ExprWalkPush(walk, expr->first_child);
    return index;
}

static inline int
ExprWalkFirst(ZirExprWalk *walk, int root)
{
    walk->count = 0;
    ExprWalkPush(walk, root);
    return ExprWalkNext(walk);
}

/* Start the opaque cursor at zero. Returns 1 for a field, 0 at end, -1 for
 * malformed syntax. The record body must remain unchanged during a walk;
 * reset the cursor after any rewrite. Fields come from one immutable parsed
 * snapshot. Names/types are trimmed without truncating source tokens. */
int TypeNextField(const ZirType *record, size_t *offset, ZirTypeField *field);
/* TypeNextField without the copy, for walks that only read: *field is valid
 * for as long as the record's compiler context. */
int TypeNextFieldShared(const ZirType *record, size_t *offset, const ZirTypeField **field);
/* FIELD as one record body line with TYPE as its type, keeping its using,
 * #go_tag and #c spellings. Returns what snprintf returns. */
int TypeFieldLine(const ZirTypeField *field, const char *type, char *out, size_t size);
/* Same first-match and malformed-prefix semantics as a TypeNextField walk,
 * using the immutable snapshot's field-name index. Clears FIELD on a miss. */
int TypeFindField(const ZirType *record, const char *name, ZirTypeField *field);
/* Conservative snapshot fact: a nonempty record of builtin values or
 * pointers. Void/compile-time-only, named/aggregate fields, empty records
 * and malformed bodies return false. */
int TypeHasOnlyPrimitiveFields(const ZirType *record);
int EnumMemberValue(const ZirType *type, const char *name, int64_t *value);

/* Parse a fixed-capacity array type text "[N]element". Returns 1 with the
 * element type copied out and the capacity stored, 0 for any other type. */
int SliceElementType(const char *type, char *element, size_t element_size);
int ArrayValueType(const char *type);
int ArrayElementType(const char *type, char *element, size_t element_size,
                        int *capacity);
/* Whether a constant's value names an array type, as in `Row :: [3]s32`,
 * rather than writing an array, as in `GRID :: [3]s32.[...]`. */
int ArrayTypeAlias(const char *value);
struct ZirModule;
/* Recognize the checked, specialized standard Vec storage shape. */
int VecElementType(const struct ZirModule *module, const char *type,
                   char *element, size_t element_size);
/* Recognize std/vec's owned `Text`: a record whose one field is
 * `bytes: Vec(u8)`. It moves and drops like any record owning a Vec; a
 * `TextView` of it borrows its storage. */
int OwnedTextType(const struct ZirModule *module, const char *type);
/* Whether NAME is a module identity of the standard library's std/LEAF:
 * LEAF alone, std_LEAF inside a project, or the p<hash>_LEAF identity a
 * `--module-path std=DIR` package gives it, also in saved IR. */
int StandardModuleName(const char *name, const char *leaf);
/* The identity `--module-path std=DIR` gives std/LEAF, into OUT. */
int StandardScopedName(const char *leaf, char *out, size_t size);

struct ZirDocument;
/* Direct applications share identity only for the same template and arguments. */
int same_type_application(const ZirModule *target_owner, const ZirType *target,
                          const ZirModule *source_owner, const ZirType *source);

/* Local records and direct unqualified Ziran imports; owner supplies field scope. */
/* Local declarations shadow imports. Returns 1 found, 0 absent, -1 ambiguous. */
int ResolveFunction(const ZirModule *module, const char *name,
                       const ZirModule **owner, const ZirFunction **function);
int ResolveFunctionAt(const ZirModule *module, const char *name,
                      const char *source_path, const ZirModule **owner,
                      const ZirFunction **function);
/* 1: unique visible global, 0: absent, -1: ambiguous. */
int ResolveGlobalAt(const ZirModule *module, const char *name,
                    const char *source_path, const ZirModule **owner,
                    const ZirGlobal **global);
int ResolveGlobal(const ZirModule *module, const char *name,
                  const ZirModule **owner, const ZirGlobal **global);
/* The constant NAME or ALIAS.NAME as MODULE's current file sees it: 1 with
 * its DEFINITION and OWNER, 0 when absent, -1 when ambiguous.
 * SELECTED_IMPORT, when given, receives the import an imported constant
 * was found through. */
int ResolveDefine(const ZirModule *module, const char *name,
                  const ZirDefine **definition, const ZirModule **owner,
                  const ZirImport **selected_import);
/* Runtime contracts are parsed from embedded declaration sources. */
const ZirType *FindType(const ZirModule *module, const char *name,
                        const ZirModule **owner);
const ZirType *BuiltinType(const char *name);
/* Set INSTANCE, an application of a template with the given scope to
 * ARGUMENTS, to the narrowest scope of the template and MODULE's types the
 * arguments name. */
void TypeApplicationScope(const ZirModule *module, const char *arguments,
                          int template_public, int template_file_private,
                          ZirType *instance);
/* True for a name the language defines as a type (s32, string, int, ...);
 * no module may declare a type with such a name. */
int BuiltinTypeName(const char *name);
/* Resolve a record member, including fields promoted by `using`.
 * Returns 1 for a unique member, 0 if absent, -1 if ambiguous or too deep. */
int ResolveRecordField(const ZirModule *owner, const ZirType *record,
                       const char *name, char *path, size_t path_size,
                       char *type, size_t type_size);
/* Check a concrete member path in checked IR; intermediate fields must use
 * `using` so source and saved modules agree on promotion. */
int RecordFieldPathType(const ZirModule *owner, const ZirType *record,
                        const char *path, char *type, size_t type_size);

ZirProgram *ProgramNew(void);
void ProgramFree(ZirProgram *program);
int ProgramInheritContexts(ZirProgram *target, const ZirProgram *source);
void copy_text(char *dst, size_t dst_size, const char *src);
ZirSourceSpan Span(const char *path, int line, int column);
int SourceFile(const char *path);
const char *KeepText(const char *text);
/* A long string literal travels through the line-based parser as a short
 * marker (`"\x1fR<n>\x1f"`) and returns to full text when it is kept. */
int RawTextRegister(const char *body, size_t length);
char *RawTextExpand(const char *text);
/* Edit kept text in place: take a copy with room to grow, change it, store it. */
char *TextEditBegin(const char *text, size_t *capacity);
const char *TextEditEnd(char *buffer);
/* FindType and ResolveFunctionAt keep entry-scope answers until this is
 * called. Call it after changing anything they read outside ModuleAddType,
 * ModuleAddFunction, ModuleAddImport and ProgramAddModule: a module's types,
 * procedures or imports (names, visibility, spans, resolved_module), or
 * freeing or moving modules. Lookup file spellings are part of the key.
 * With the environment
 * variable ZIRAN_VERIFY_SYMBOL_LOOKUPS=1 every kept answer is checked
 * against a fresh lookup, and a difference stops the compiler. */
void SymbolLookupsChanged(void);
/* A name kept like KeepText and cut to ZIR_NAME_MAX - 1 bytes, as the
 * fixed name buffers it replaces were. Kept names are never NULL once a
 * node is made: "" means none. */
const char *KeepName(const char *text);
/* A parameter list kept like KeepText and cut to ZIR_TEXT_MAX - 1 bytes,
 * as ZirFunction's parameter buffers were. */
const char *KeepParameters(const char *text);
/* KeepName of the text snprintf would write into a name buffer. */
const char *KeepNameFormat(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
/* Empty EXPR or STMT in place: zero fields and "" kept names. */
void ExprReset(ZirExpr *expr);
void StmtReset(ZirStmt *stmt);
void *AllocateOrExit(size_t size);
const char *SpanPath(ZirSourceSpan span);
ZirSourceSpan SpanEnd(const char *path, int line, int column,
                         int end_line, int end_column);
ZirModule *ProgramAddModule(ZirProgram *program, const char *name,
                               const char *source_path, ZirSourceSpan span);
ZirImport *ModuleAddImport(ZirModule *module, ZirImportKind kind,
                              const char *name, const char *target,
                              const char *signature, int required,
                              ZirSourceSpan span);
ZirFunction *ModuleAddFunction(ZirModule *module, const char *name,
                                  const char *args, const char *return_type,
                                  int exported, ZirSourceSpan span);
/* A default value that means the same at every call site: a number, a
 * plain string, true, false, or null. It needs no helper procedure. */
int DefaultIsLiteral(const char *value);
void FunctionDefaultHelperName(const ZirFunction *function, int parameter,
                               char *out, size_t size);
void ModuleAddGlobal(ZirModule *module, const char *name, const char *type,
                        const char *init, ZirSourceSpan span);
void ModuleAddStatic(ZirModule *module, const char *name, const char *type,
                        const char *init, ZirSourceSpan span);
ZirDefine *ModuleAddDefine(ZirModule *module, const char *name,
                              const char *value, ZirSourceSpan span);
int ModuleAddLaw(ZirModule *module, const char *name, const char *kind,
                 const char *payload, ZirSourceSpan span);
int ModuleAddLawWaiver(ZirModule *module, const char *name,
                       const char *reason, ZirSourceSpan span);
int ModuleAddProof(ZirModule *module, const char *name, const char *source,
                   ZirSourceSpan span);
int ModuleAddAssert(ZirModule *module, const char * condition,
                    const char *message, ZirSourceSpan span);
ZirUsing *ModuleAddUsing(ZirModule *module, const char *path,
                         ZirSourceSpan span);
ZirType *ModuleAddType(ZirModule *module, const char *name,
                          ZirSourceSpan span);
ZirStmt *FunctionAddStmt(ZirFunction *fn, ZirStmtKind kind,
                            const char *text, ZirSourceSpan span);
const char *ImportKindName(ZirImportKind kind);
const char *StmtKindName(ZirStmtKind kind);
const char *ExprKindName(ZirExprKind kind);
ZirExpr *FunctionAddExpr(ZirFunction *fn, ZirExprKind kind,
                            const char *text, ZirSourceSpan span);
void ProgramDump(const ZirProgram *program, FILE *out);
int GoForeignTargetValid(const char *target);
int GoCHeader(const char *package, char *header, size_t header_size);
int GoForeignCallParts(const char *target, char *package, size_t package_size,
                       char *receiver, size_t receiver_size,
                       char *symbol, size_t symbol_size);
int TargetNameKnown(const char *target);
int CheckTargetCapabilities(const ZirProgram *program, const char *target);
int TargetProcedureTypeSupported(const char *target, int c_call);
size_t ScalarByteWidth(const char *type);
/* Fixed-width SIMD vector types such as u8x16 or f32x8; the spelling rules
 * are in cmd/compiler_type.zi. Every string is static. */
typedef struct ZirVector {
    const char *name; /* canonical spelling */
    const char *lane; /* u8, s8 ... s64, float32 or float64 */
    int lanes, lane_bits, bytes, is_signed, is_float;
    /* A native vector, such as s16xN: one 128-bit block of `lanes` lanes
     * outside AVX2 code and two in it. `block` is the 128-bit type. */
    int is_native;
    const char *block;
} ZirVector;
int VectorType(const char *type, ZirVector *vector);
/* The vector with lane type `lane` and `lanes` lanes, or NULL. */
const char *VectorTypeName(const char *lane, int lanes);
/* The native vector with lane type `lane`, such as s16xN, or NULL. */
const char *NativeVectorName(const char *lane);
/* Whether `type` spells a native vector inside another type, as in
 * [4]s16xN or *u8xN, rather than being one. */
int HoldsNativeVector(const char *type);
int UnionScalarFields(const ZirModule *module, const ZirType *record);
int MapTypeParts(const ZirModule *module, const char *name,
                 char *key, size_t key_size, char *value, size_t value_size);
int MapPrimitiveName(const char *name);
/* The bit builtins clz, ctz, popcount and bswap. Source spells the short
 * name; the checker rewrites it to the reserved zi_ name the backends read. */
enum { ZIR_BIT_NONE, ZIR_BIT_CLZ, ZIR_BIT_CTZ, ZIR_BIT_POPCOUNT, ZIR_BIT_BSWAP };
int BitBuiltin(const char *checked_name);
const char *BitBuiltinName(const char *source_name);
int SameMapType(const ZirModule *a_owner, const ZirType *a,
                const ZirModule *b_owner, const ZirType *b);
int MapKeyComparable(const ZirModule *module, const char *type, int depth);

/* Native backends record each file they write. After a successful build,
 * GeneratedOutputPrune removes files in the output directory and its
 * subdirectories that start with the backend's generated-file marker but
 * were not written this time,
 * so a reused directory never links code from an earlier build. Files
 * without the marker are never touched. */
void GeneratedOutputRecord(const char *path);
/* Full paths recorded by this invocation, or NULL at the end. */
const char *GeneratedOutputPath(int index);
int GeneratedOutputPrune(const char *out_dir, const char *marker);

/* Backends write each file through a hidden temporary next to it.
 * GeneratedOutputOpen records path and opens that temporary, writing its
 * name to temp. After the caller closes the file, GeneratedOutputReplace
 * moves it over path only when the bytes differ, so an unchanged output
 * keeps its timestamp and build tools recompile only what changed. */
FILE *GeneratedOutputOpen(const char *path, char *temp, size_t size);
int GeneratedOutputReplace(const char *temp, const char *path);

/* Assembly variants supplement an ordinary checked, portable body. */
const char *ExtractAssemblyModifiers(char *header, ZirSourceSpan span);
int CheckAssembly(const ZirModule *module, const ZirFunction *function);
const char *AssemblyBlockText(const char *header, ZirSourceSpan span);
/* Put the literal of each constant string an #asm argument names in place
 * of the name, once #run constants are known. */
int ResolveAssemblyConstants(ZirModule *module);
typedef const char *(*ZirAssemblyBinding)(void *context, const char *name);
int CheckAssemblyBlock(const ZirFunction *function, int statement,
                       ZirAssemblyBinding binding, void *context);
/* Checked storage references, including native-only operand uses. */
int AssemblyBlockOperands(const ZirFunction *function, int statement,
                           const char **names, const char **types, int capacity,
                           int *outputs);

#endif /* ZIRAN_ZIR_H */
