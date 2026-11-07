#ifndef ZIR_EMIT_INTERNAL_H
#define ZIR_EMIT_INTERNAL_H

#include "zir_emit.h"
#include "zir_check.h"
#include "zir_parse.h"
#include "zir_text.h"
#include "zir_expr.h"
#include "zir_diagnostic.h"
#include "zir_stream.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* Scratch output is used only to detect setup statements. Memory streams
 * keep compiler/runtime libraries usable on Android API 21; platforms without
 * memory streams can count bytes in a temporary stream instead. */
static inline FILE *
EmitScratchOpen(unsigned char **text, size_t *size)
{
#if defined(ZIR_MEMORY_STREAMS)
    return ZirWriteMemory(text, size);
#else
    *text = NULL;
    *size = 0;
    return tmpfile();
#endif
}

static inline void
EmitScratchClose(FILE *file, size_t *size)
{
#if !defined(ZIR_MEMORY_STREAMS)
    long written = ftell(file);
    if(written < 0) {
        fclose(file);
        *size = 1;
        return;
    }
    *size = (size_t)written;
#endif
    if(fclose(file) != 0)
        *size = 1;
}

typedef struct ModuleVisits {
    const ZirModule **items;
    size_t count;
    size_t capacity;
} ModuleVisits;

/* Every emitted name asks for its module's identity, and each answer scans
 * all modules for colliding stems. Remember answers per program set so a
 * large program costs one scan per module instead of one per name. */
typedef struct {
    const ZirModule *module;
    char file_stem[ZIR_PATH_MAX];
    char guard[256];
} ModuleIdentity;


typedef struct Local {
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    char drop_alias[ZIR_NAME_MAX];
    /* Text that stands for the binding, such as (*shared) for an array a
     * parallel worker reaches through a pointer; empty for the name. */
    char alias[ZIR_NAME_MAX * 2];
    int depth;
    /* Nesting depth of the block whose declaration of the same name gave
     * this binding its drop alias, which ends with that block; 0 when the
     * alias, if any, lasts as long as the binding. */
    int shadow_depth;
} Local;
typedef struct RangeFacts RangeFacts;
typedef struct Emitter {
    FILE *out;
    const ZirModule *module;
    const ZirFunction *fn;
    ZirTarget target;
    ZirResolveTarget resolve;
    void *context;
    ZirAssemblyBlockEmitter assembly;
    void *assembly_context;
    int indent, serial, depth, local_count;
    Local *locals;
    int loop_count;
    int loop_start[128];
    int loop_id[128];
    /* Whether each open loop is a native counting loop whose header does
     * the stepping, so its marked step statements are left out. */
    int loop_native[128];
    /* A native header for the next lowered for loop's while, set by the
     * block that holds it, and how many leading body statements the header
     * already binds. */
    char loop_header[ZIR_TEXT_MAX];
    int loop_header_binds;
    int sequence_terminated;
    /* Expression folding (Go target): "pure" marks the last emitted expression
     * as free of side effects, so it can be inlined into its consumer instead
     * of being captured in a value_N temporary. "minify" removes the inline
     * length bound for callers that want the densest possible output. */
    int pure;
    int minify;
    /* Set by a consumer that evaluates nothing after this expression: a call
     * result may then go straight into it instead of a temporary. */
    int call_in_place;
    /* C text of an array a statement wants the next array-valued call to
     * write directly; the call sets array_destination_used when it does. */
    const char *array_destination;
    int array_destination_used;
    /* Set by a consumer that copies an array-valued expression straight out
     * of its C text, such as a return: the expression is not captured. */
    int array_operand;
    /* What the C body's index range proofs know about fn, or NULL. */
    RangeFacts *ranges;
    /* Set for a Go declaration whose value already has the declared type,
     * so it can read name := value. */
    int short_declaration;
    /* Set by a C declaration whose initializer is a record or array
     * value, which may then be written as a brace list. */
    int braced_initializer;
    /* "ssse3", "sse41" or "avx2" while a #target procedure's clone for that
     * x86-64 tier is emitted, whose std/simd helpers carry that prefix. */
    const char *simd_tier;
    /* While a small procedure's body is emitted in place of a call to it:
     * the caller's variable its result goes to (NULL for void), the label
     * after the body that a return jumps to unless it is the body's last
     * statement, how many returns jumped, and how deep inlining nests. */
    const char *inline_result;
    const char *inline_label;
    int inline_tail, inline_jumps, inline_depth;
} Emitter;

/* Bound for readable inlined expressions. Longer results stay in named
 * temporaries so the generated code keeps human-auditable steps. */
#define ZIR_INLINE_MAX 96

typedef struct {
    const ZirModule *module;
    const ZirGlobal *global;
    const ZirFunction *probe;
    ZirTarget target;
    ZirSourceSpan span;
    ZirGlobalScalarRewrite scalar;
    ZirGlobalTypeRewrite type_name;
    ZirGlobalFieldRewrite field_name;
    void *context;
} GlobalLiteralEmit;

/* A run of print output for one standard call: the format text, the same
 * text unescaped for a call without values, and the value arguments. */
typedef struct PrintRun {
    unsigned char format[ZIR_TEXT_MAX];
    size_t format_length;
    unsigned char plain[ZIR_TEXT_MAX];
    size_t plain_length;
    char arguments[ZIR_TEXT_MAX];
    int values;
} PrintRun;

/* ---- #parallel for regions (native threading) ------------------------ */

typedef struct ParallelRegion {
    int while_index;
    int close;
    int body_begin;
    int body_end;
    char first[ZIR_NAME_MAX];
    char last[ZIR_NAME_MAX];
    char cursor[ZIR_NAME_MAX];
    char binder[ZIR_NAME_MAX];
    int forward;
    char captures[16][ZIR_NAME_MAX];
    char capture_types[16][ZIR_NAME_MAX];
    int capture_count;
    char worker[ZIR_NAME_MAX * 2];
} ParallelRegion;

/* A region as a compute shader, and how each capture reaches it. */
typedef struct GpuRegionCode {
    char *text;
    char reason[160];
    int kinds[16];
    int is_buffer[16];
    int writes[16];
} GpuRegionCode;

/* Shared between the parts only: the Makefile merges the parts into one
 * object and localizes these hidden symbols, so they never leave it. */
#pragma GCC visibility push(hidden)
extern int zir_minify_output;
extern const char *zir_emit_simd_tier;
int format(char *out, size_t size, const char *format_string, ...);
int GpuLowerRegion(const ZirModule *module, const ZirFunction *fn, const ParallelRegion *region, GpuRegionCode *code);
void GpuEmitShaderLines(Emitter *e, const char *text);
int function_mentions(const ZirFunction *fn, const char *name);
const char *canonical(const char *type);
void slot_native_type(const char *source, ZirTarget target, char *out, size_t size);
void slot_type_at_use(const char *source, ZirTarget target,
                      ZirResolveTarget resolve_type, void *context,
                      char *out, size_t size);
void resolve_type_at_use(void *context, const char *source, char *out, size_t size);
int width(const char *type);
int signed_type(const char *type);
int enum_type(const ZirModule *module, const char *type);
int enum_flags_type(const ZirModule *module, const char *type);
int record_type(const ZirModule *module, const char *type);
/* The record TYPE names in MODULE; OWNER, when given, gets its declaring module. */
const ZirType *field_record(const ZirModule *module, const char *type, const ZirModule **owner);
/* The C or Go text for PATH's fields from BASE_EXPRESSION. A READ of a pointer
 * field C spells its own way is cast to the field's Ziran type. */
void emit_field_path(const ZirModule *module, ZirTarget target, const char *base_type, const char *path, int read, const char *base_expression, char *out, size_t size);
/* Whether RECORD's field NAME is a pointer C declares its own way, such as
 * const char * for *u8: reads cast to the Ziran type, stores to the spelling. */
int spelled_pointer_field(const ZirType *record, const char *name, ZirTypeField *field);
/* VALUE, stored to RECORD's field NAME, cast in place to a spelled pointer
 * field's C type on C. */
void spelled_field_store(ZirTarget target, const ZirType *record, const char *name, char *value, size_t size);
const char *zero_value(const char *type, ZirTarget target);
void number_prefix(const ZirModule *module, char *out, size_t size);
int emitter_type_contains_vec(const ZirModule *module, const char *type, int depth);
int plain_identifier(const char *text);
int folds_text(const Emitter *e, const char *text, const char *type);
void line(Emitter *e, const char *format, ...);
void fatal(const ZirExpr *expr, const char *message);
void fresh(Emitter *e, char *name);
void assign_value(Emitter *e, const char *destination, const char *type, const char *source);
void declare_array(Emitter *e, const char *name, const char *type, const char *value);
void array_target_type(Emitter *e, const char *type, char *target_element,
                       size_t element_size, char *bounds, size_t bounds_size);
void declare(Emitter *e, const char *name, const char *type, const char *value);
void resolve(Emitter *e, const char *name, char *out, size_t size);
void drop_owned_value(Emitter *e, const char *value, const char *type, const ZirModule *module, int depth);
void clear_owned_value(Emitter *e, const char *value, const char *type, const ZirModule *module, int depth);
void track_local(Emitter *e, const char *name, const char *type);
void alias_shadowed_local(Emitter *e, const char *name);
void leave_block_locals(Emitter *e, int saved);
void drop_locals(Emitter *e, int first);
void drop_temporary_vec(Emitter *e, const char *name, const char *type);
int has_owned_locals(const Emitter *e);
int operation(const char *op);
int integer_literal_bits(const char *text, uint64_t *bits);
int enclosed(const char *text);
void postfix_base(const char *text, char *out, size_t size);
const char *bare(const char *text, char *out, size_t size);
void number(Emitter *e, const char *type, const char *a, const char *a_type, const char *b, const char *b_type, int op, char *out, size_t size);
void slice_index(Emitter *e, const char *type, const char *base, const char *index, int checked, char *out, size_t size);
void go_pointer_index(Emitter *e, const char *base, const char *index, char *out, size_t size);
void emit_destination(Emitter *e, int index, char *out, size_t size);
/* A constant array literal read in place by a consumer that never writes
 * or keeps it, such as a SIMD load: the shared or function-local static
 * table, with no copy. Returns 0, having emitted nothing, for any other
 * expression. */
int emit_constant_array_read(Emitter *e, int index, char *out, size_t size);
void literal(Emitter *e, const ZirExpr *expr, const char *type, int negative, char *out, size_t size);
void emit_call(Emitter *e, const ZirExpr *expr, const char *array_result, char *out, size_t size);
void emit_function_value(Emitter *e, int index, char *out, size_t size);
int member_path(const ZirFunction *fn, int index);
RangeFacts *RangeFactsBuild(const ZirFunction *fn);
void RangeFactsFree(RangeFacts *facts);
/* Whether index expression index provably lies in 0..count-1. */
int IndexInRange(const Emitter *e, int index, int64_t count);
/* Whether index provably lies below the count of the slice or string that
 * identifier base names. */
int IndexBelowCount(const Emitter *e, int index, int base);
/* Whether the run elements from index provably lie inside the slice or
 * string that identifier base names: 0 <= index, index + run <= count. */
int RunBelowCount(const Emitter *e, int index, int64_t run, int base);
/* Whether the C view of slice expression index, base[low:high] of a fixed
 * array of capacity elements or of a slice when capacity < 0, provably keeps
 * 0 <= low <= high <= count. */
int SliceRangeInRange(const Emitter *e, int index, int capacity);
/* Whether every element of the local that declaration, with no value,
 * declares is provably stored before anything can read it, so C need not
 * fill it with zeros first. */
int WrittenBeforeRead(const Emitter *e, int declaration);
int expression_calls(const ZirFunction *fn, int index);
/* Emit call, a call of a small procedure of this module, as that
 * procedure's body in place, leaving the result's C text in out. Returns 0,
 * having emitted nothing, when the call is not inlined. */
int emit_inline_call(Emitter *e, const ZirExpr *call, char *out, size_t size);
/* Whether a call run later could change what expression index reads. */
int call_can_change(const Emitter *e, int index);
void emit_print(Emitter *e, const ZirExpr *expr);
/* A std/simd operation; 1 when it was a statement (a store). */
int emit_simd_call(Emitter *e, const ZirExpr *expr, char *out, size_t size);
/* Whether call, in a #target clone, goes to its callee's copy for the
 * clone's tier (see SimdTierCopied). */
int SimdTierCopyCall(const Emitter *e, const ZirExpr *call);
void emit_expr(Emitter *e, int index, const char *expected, char *out, size_t size);
void zero_record(Emitter *e, const char *type, char *out, size_t size);
#pragma GCC visibility pop

#endif
