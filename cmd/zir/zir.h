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

enum {
    ZIR_PATH_MAX = 1024,
    ZIR_NAME_MAX = 128,
    ZIR_TEXT_MAX = 4096
};

typedef enum ZirImportKind {
    ZIR_IMPORT_OPEN = 1,
    ZIR_IMPORT_MODULE,
    ZIR_IMPORT_EXTERN
} ZirImportKind;

typedef enum ZirExternKind {
    ZIR_EXTERN_NONE = 0,
    ZIR_EXTERN_HOST,
    ZIR_EXTERN_GO,
    ZIR_EXTERN_C
} ZirExternKind;

typedef enum ZirStmtKind {
    ZIR_STMT_UNKNOWN = 0,
    ZIR_STMT_BLOCK_OPEN,
    ZIR_STMT_BLOCK_CLOSE,
    ZIR_STMT_DECL,
    ZIR_STMT_ASSIGN,
    ZIR_STMT_EXPR,
    ZIR_STMT_IF,
    ZIR_STMT_WHILE,
    ZIR_STMT_FOR,
    ZIR_STMT_CASE,
    ZIR_STMT_RETURN,
    ZIR_STMT_BREAK,
    ZIR_STMT_CONTINUE,
    ZIR_STMT_DEFER,
    ZIR_STMT_UNUSED,
    ZIR_STMT_UNREACHABLE,
    ZIR_STMT_IF_CASE
} ZirStmtKind;

typedef enum ZirExprKind {
    ZIR_EXPR_UNKNOWN = 0,
    ZIR_EXPR_IDENT,
    ZIR_EXPR_INT,
    ZIR_EXPR_FLOAT,
    ZIR_EXPR_STRING,
    ZIR_EXPR_CALL,
    ZIR_EXPR_BINARY,
    ZIR_EXPR_UNARY,
    ZIR_EXPR_MEMBER,
    ZIR_EXPR_POINTER_MEMBER,
    ZIR_EXPR_INDEX,
    ZIR_EXPR_CAST,
    ZIR_EXPR_COMPOUND,
    ZIR_EXPR_SIZE_OF,
    ZIR_EXPR_CONDITIONAL,
    ZIR_EXPR_FIELD_INIT,
    ZIR_EXPR_SLICE,
    ZIR_EXPR_COMPILE_TIME
} ZirExprKind;

typedef struct ZirSourceSpan {
    int file; /* SourceFile number of the path; read it with SpanPath */
    int line;
    int column;
    int end_line;
    int end_column;
} ZirSourceSpan;

typedef struct ZirImport {
    ZirImportKind kind;
    ZirExternKind extern_kind;
    int is_public;
    int is_file_private;
    char name[ZIR_NAME_MAX];
    char target[ZIR_PATH_MAX];
    char extern_symbol[ZIR_NAME_MAX];
    char signature[ZIR_TEXT_MAX];
    char args[ZIR_TEXT_MAX];       /* parsed extern parameters */
    char return_type[ZIR_NAME_MAX];
    int must_use;
    int is_varargs; /* trailing `..any` extern parameter accepts extra call arguments */
    int go_results; /* direct Go results populate the declared result record in field order */
    int go_field; /* one Go receiver argument reads its declared native field */
    int go_defer; /* a standalone call schedules its Go foreign target at function exit */
    int required;
    int is_using; /* `using Alias :: #import` re-exports public names */
    ZirSourceSpan span;
    const struct ZirModule *resolved_module; /* borrowed from the checked program set */
} ZirImport;

typedef struct ZirStmt {
    ZirStmtKind kind;
    int is_using; /* template namespace activation; lowered in checked bodies */
    int is_parallel; /* `#parallel for` region; serial lowering, checked rules */
    int is_gpu; /* `#parallel_gpu for` region; pointers rejected pre-offload */
    const char *text; /* shared and immutable: assign KeepText(...) */
    int is_else;        /* checked branch role; source text is diagnostic only */
    int loop_id;        /* checked loop identity for named control flow */
    int target_id;      /* target loop for named break/continue; zero means innermost */
    int for_form;       /* on a lowered for loop's while: 1 counts, 2 walks a collection */
    int for_step;       /* on a counting loop's step: that loop's loop_id */
    int expr_root;      /* index into enclosing function exprs, or -1 */
    int lhs_root;       /* structured assignment destination, or -1 */
    char name[ZIR_NAME_MAX]; /* declaration binding */
    char type[ZIR_NAME_MAX]; /* declared or inferred type */
    char assignment_op[4];
    ZirSourceSpan span;
} ZirStmt;

typedef struct ZirExpr {
    ZirExprKind kind;
    int is_function_value; /* declaration bound to an expected slot signature */
    int is_this; /* #this resolves to the enclosing procedure despite shadowing */
    int is_global_value; /* in-memory only: identifier/call bound to a global */
    int is_move; /* in-memory only: a checked owned binding is consumed here */
    char slot_type[ZIR_NAME_MAX]; /* lexical callable signature, empty for ordinary calls */
    const char *text; /* shared and immutable: assign KeepText(...) */
    char name[ZIR_NAME_MAX];
    char argument_name[ZIR_NAME_MAX]; /* name on a call argument, if supplied */
    int argument_index; /* checked callee parameter position, or -1 */
    char op[8];
    int left;
    int right;
    int first_child;
    int next_sibling;
    int third;         /* false conditional arm or slice upper bound, or -1 */
    char type[ZIR_NAME_MAX]; /* resolved type; empty means unresolved */
    ZirSourceSpan span;
} ZirExpr;

typedef struct ZirFunction {
    char name[ZIR_NAME_MAX];
    char args[ZIR_TEXT_MAX];
    char default_args[ZIR_TEXT_MAX]; /* declaration parameters with defaults */
    uint64_t using_parameters; /* template parameter namespace flags */
    char return_type[ZIR_NAME_MAX];
    int must_use; /* #must requires callers to keep the result */
    int is_conversion; /* `Name :: #as (source: Type) -> Result` */
    char effect_class[12]; /* pure | observing | mutating | external */
    int exported;
    char export_symbol[ZIR_NAME_MAX]; /* optional #program_export linker name */
    int is_extern;
    ZirExternKind extern_kind;
    int is_public;  /* function emitted in generated interfaces */
    int is_file_private;
    int is_template; /* Jai $T procedure declaration; no native body */
    int is_specialization; /* checked instance generated from a template */
    int is_global_initializer; /* private startup body generated from a global */
    char template_param[ZIR_NAME_MAX];
    char specialization_type[ZIR_NAME_MAX];
    char overload_name[ZIR_NAME_MAX]; /* source name shared by overloads; name is unique */
    int checked;    /* shared checker resolved the function without errors */
    int from_ir;    /* in-memory only: expression graph came from saved IR */
    int default_helpers_created; /* in-memory only: source helper pass ran */
    int uses_host; /* direct or transitive host services or retained-state access */
    char extern_target[ZIR_NAME_MAX];   /* resolved #foreign library/symbol */
    char extern_symbol[ZIR_NAME_MAX];   /* stripped C symbol for c.* externs */
    ZirSourceSpan span;
    ZirStmt *stmts;
    int stmt_count;
    int stmt_cap;
    ZirExpr *exprs;
    int expr_count;
    int expr_cap;
} ZirFunction;

typedef struct ZirGlobal {
    char name[ZIR_NAME_MAX];
    char type[ZIR_TEXT_MAX];
    char init[ZIR_TEXT_MAX];
    int is_static;   /* internal linkage in generated native code */
    int is_file_private;
    int native_name_collision; /* in-memory: another global maps to this native name */
    ZirSourceSpan span;
} ZirGlobal;

/* A Ziran `Name :: value` constant emitted into generated target interfaces. */
typedef struct ZirDefine {
    char name[ZIR_NAME_MAX];
    char value[ZIR_TEXT_MAX];
    int is_public; /* visible to importing modules */
    int is_file_private;
    int requires_open_enum; /* unresolved source alias, cleared by checking */
    int native_name_collision; /* in-memory: another constant maps to this native name */
    ZirSourceSpan span;
} ZirDefine;

typedef struct ZirAssert {
    char condition[ZIR_TEXT_MAX]; /* compile-time Ziran expression */
    char message[ZIR_TEXT_MAX];
    ZirSourceSpan span;
} ZirAssert;

typedef struct ZirUsing {
    char path[ZIR_NAME_MAX];
    /* Compact only/except/map filter: "O:a,b", "E:c", or "M:new=old". */
    char filter[160];
    int is_file_private;
    ZirSourceSpan span;
} ZirUsing;

typedef struct ZirLawEvidence {
    char method[16]; /* structural | evaluation | exhaustive | kernel */
    char domain[ZIR_TEXT_MAX];
    uint64_t cases_checked;
    char counterexample[ZIR_TEXT_MAX]; /* JSON object, empty when absent */
    int waived;
} ZirLawEvidence;

typedef enum ZirProofStepKind {
    ZIR_PROOF_UNFOLD = 1, ZIR_PROOF_CASES, ZIR_PROOF_REWRITE,
    ZIR_PROOF_USE, ZIR_PROOF_REFL, ZIR_PROOF_RING, ZIR_PROOF_ORDER
} ZirProofStepKind;

typedef struct ZirProofStep {
    ZirProofStepKind kind;
    char target[ZIR_NAME_MAX];
    int term_root; /* theorem application in the proof's terms graph */
    ZirSourceSpan span;
} ZirProofStep;

typedef struct ZirProof {
    char name[ZIR_NAME_MAX];
    char source[ZIR_TEXT_MAX]; /* diagnostic only after parsing */
    ZirSourceSpan span;
    ZirProofStep *steps;
    int step_count;
    ZirFunction terms; /* checked theorem arguments; never executable code */
    const struct ZirModule *owner; /* borrowed, never serialized */
} ZirProof;

/* A `#law NAME kind payload;` obligation (see LANGUAGE_DIRECTION.md). */
typedef struct ZirLaw {
    char name[ZIR_NAME_MAX];
    char kind[16];
    char payload[ZIR_TEXT_MAX];
    ZirSourceSpan span;
    ZirLawEvidence evidence;
    ZirFunction *claim; /* typed theorem proposition, NULL for other kinds */
    const ZirProof *proof; /* associated certificate, borrowed, never saved */
    /* In-memory EvaluateLaw result, reused while module and proof match. */
    const struct ZirModule *evaluated_in;
    const ZirProof *evaluated_proof;
    int evaluated_status;
    const char *evaluated_detail; /* KeepText */
} ZirLaw;

/* A `#law_waive NAME reason;` declaration. */
typedef struct ZirLawWaiver {
    char name[ZIR_NAME_MAX];
    char reason[ZIR_TEXT_MAX];
    ZirSourceSpan span;
} ZirLawWaiver;

/* A `Name :: struct { fields }` type declaration. body holds the raw field
 * lines (one per line, no braces). */
typedef struct ZirType {
    char name[ZIR_NAME_MAX];
    char body[ZIR_TEXT_MAX * 2];
    char template_params[ZIR_NAME_MAX]; /* generic record parameters */
    char template_name[ZIR_NAME_MAX]; /* specialization or checked direct origin */
    char template_args[ZIR_TEXT_MAX];
    char foreign_target[ZIR_PATH_MAX]; /* opaque Go type: go:package.Type */
    int is_procedure_type; /* named procedure type; body holds parameters */
    int is_c_call; /* procedure type uses the native C callback ABI */
    char procedure_return_type[ZIR_NAME_MAX];
    int is_public; /* visible to importing modules */
    int is_file_private;
    int is_enum;   /* 'Name :: enum' — emit typedef enum, not struct */
    int is_union;  /* fields share storage */
    int is_enum_flags;
    int is_enum_specified;
    char enum_backing[ZIR_NAME_MAX]; /* checked integer storage type */
    int is_record_template;
    int is_type_instance; /* unresolved until imports are linked */
    int is_synthetic_application; /* private name made from a direct type call */
    int is_owned_vec; /* specialized standard Vec storage */
    int is_map; /* Go map; body records the private key/value types */
    int is_extern; /* host-owned record or opaque foreign type */
    int is_abi_incomplete; /* native ABI treats the complete record as opaque */
    int is_results; /* holds the results of a procedure with several, value_0... */
    int native_name_mangled; /* in-memory: shared spelling or target keyword */
    ZirSourceSpan span;
} ZirType;

typedef struct ZirTypeField {
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    char go_tag[512]; /* checked #go_tag string literal; Go reflection metadata */
    int is_using;
} ZirTypeField;

/* Start offset at zero. Returns 1 for a field, 0 at end, -1 for malformed
 * record syntax. Names/types are trimmed without truncating source tokens. */
int TypeNextField(const ZirType *record, size_t *offset, ZirTypeField *field);
int EnumMemberValue(const ZirType *type, const char *name, int64_t *value);

/* Parse a fixed-capacity array type text "[N]element". Returns 1 with the
 * element type copied out and the capacity stored, 0 for any other type. */
int SliceElementType(const char *type, char *element, size_t element_size);
int ArrayElementType(const char *type, char *element, size_t element_size,
                        int *capacity);
struct ZirModule;
/* Recognize the checked, specialized standard Vec storage shape. */
int VecElementType(const struct ZirModule *module, const char *type,
                   char *element, size_t element_size);

typedef struct ZirModule {
    char name[ZIR_NAME_MAX];
    char source_path[ZIR_PATH_MAX];
    char source_root[ZIR_PATH_MAX]; /* source-only path for call-site defaults */
    char lookup_path[ZIR_PATH_MAX]; /* checker context; never serialized */
    ZirSourceSpan span;
    ZirGlobal *globals;
    int global_count;
    int global_cap;
    ZirDefine *defines;
    int define_count;
    int define_cap;
    ZirAssert *asserts;
    int assert_count;
    int assert_cap;
    ZirUsing *usings; /* source scope; references lower before saving IR */
    int using_count;
    int using_cap;
    ZirLaw *laws;
    int law_count;
    int law_cap;
    ZirLawWaiver *law_waivers;
    int law_waiver_count;
    int law_waiver_cap;
    ZirProof *proofs;
    int proof_count;
    int proof_cap;
    ZirType *types;
    int type_count;
    int type_cap;
    ZirImport *imports;
    int import_count;
    int import_cap;
    ZirFunction *functions;
    int function_count;
    int function_cap;
} ZirModule;

/* Direct applications share identity only for the same template and arguments. */
int same_type_application(const ZirModule *target_owner, const ZirType *target,
                          const ZirModule *source_owner, const ZirType *source);

typedef struct ZirProgram {
    ZirModule *modules;
    int module_count;
    int module_cap;
} ZirProgram;

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
/* Runtime contracts are parsed from embedded declaration sources. */
const ZirType *FindType(const ZirModule *module, const char *name,
                        const ZirModule **owner);
const ZirType *BuiltinType(const char *name);
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
void copy_text(char *dst, size_t dst_size, const char *src);
ZirSourceSpan Span(const char *path, int line, int column);
int SourceFile(const char *path);
const char *KeepText(const char *text);
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
int GoForeignCallParts(const char *target, char *package, size_t package_size,
                       char *receiver, size_t receiver_size,
                       char *symbol, size_t symbol_size);
int RejectForeignGoTypes(const ZirProgram *program);
int MapTypeParts(const ZirModule *module, const char *name,
                 char *key, size_t key_size, char *value, size_t value_size);
int MapPrimitiveName(const char *name);
int SameMapType(const ZirModule *a_owner, const ZirType *a,
                const ZirModule *b_owner, const ZirType *b);
int MapKeyComparable(const ZirModule *module, const char *type, int depth);

/* Native backends record each file they write. After a successful build,
 * GeneratedOutputPrune removes files in the output directory that start
 * with the backend's generated-file marker but were not written this time,
 * so a reused directory never links code from an earlier build. Files
 * without the marker are never touched. */
void GeneratedOutputRecord(const char *path);
int GeneratedOutputPrune(const char *out_dir, const char *marker);

/* Backends write each file through a hidden temporary next to it.
 * GeneratedOutputOpen records path and opens that temporary, writing its
 * name to temp. After the caller closes the file, GeneratedOutputReplace
 * moves it over path only when the bytes differ, so an unchanged output
 * keeps its timestamp and build tools recompile only what changed. */
FILE *GeneratedOutputOpen(const char *path, char *temp, size_t size);
int GeneratedOutputReplace(const char *temp, const char *path);

#endif /* ZIRAN_ZIR_H */
