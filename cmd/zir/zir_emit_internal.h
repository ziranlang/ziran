#ifndef ZIR_EMIT_INTERNAL_H
#define ZIR_EMIT_INTERNAL_H

#include "zir_emit.h"
#include "zir_check.h"
#include "zir_parse.h"
#include "zir_text.h"
#include "zir_expr.h"
#include "zir_diagnostic.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct TypePath {
    const ZirType *record;
    const struct TypePath *parent;
} TypePath;

typedef struct Local {
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    char drop_alias[ZIR_NAME_MAX];
    int depth;
} Local;
typedef struct Emitter {
    FILE *out;
    const ZirModule *module;
    const ZirFunction *fn;
    ZirTarget target;
    ZirResolveTarget resolve;
    void *context;
    int indent, serial, depth, local_count;
    Local *locals;
    int loop_count;
    int loop_start[128];
    int loop_id[128];
    int sequence_terminated;
    char numbers[64];
    /* Expression folding (Go target): "pure" marks the last emitted expression
     * as free of side effects, so it can be inlined into its consumer instead
     * of being captured in a value_N temporary. "minify" removes the inline
     * length bound for callers that want the densest possible output. */
    int pure;
    int minify;
    /* Set by a consumer that evaluates nothing after this expression: a call
     * result may then go straight into it instead of a temporary. */
    int call_in_place;
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

/* Shared between the parts only: the Makefile merges the parts into one
 * object and localizes these hidden symbols, so they never leave it. */
#pragma GCC visibility push(hidden)
extern int zir_minify_output;
int format(char *out, size_t size, const char *format_string, ...);
int function_mentions(const ZirFunction *fn, const char *name);
const char *canonical(const char *type);
void slot_native_type(const char *source, ZirTarget target, char *out, size_t size);
int width(const char *type);
int signed_type(const char *type);
int enum_type(const ZirModule *module, const char *type);
int enum_flags_type(const ZirModule *module, const char *type);
int record_type(const ZirModule *module, const char *type);
const ZirType *field_record(const ZirModule *module, const char *type);
void emit_field_path(const ZirModule *module, ZirTarget target, const char *base_type, const char *path, const char *base_expression, char *out, size_t size);
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
void declare(Emitter *e, const char *name, const char *type, const char *value);
void resolve(Emitter *e, const char *name, char *out, size_t size);
void clear_owned_value(Emitter *e, const char *value, const char *type, const ZirModule *module, int depth);
void track_local(Emitter *e, const char *name, const char *type);
void drop_locals(Emitter *e, int first);
void drop_temporary_vec(Emitter *e, const char *name);
int has_owned_locals(const Emitter *e);
int operation(const char *op);
int integer_literal_bits(const char *text, uint64_t *bits);
int enclosed(const char *text);
const char *bare(const char *text, char *out, size_t size);
void number(Emitter *e, const char *type, const char *a, const char *a_type, const char *b, const char *b_type, int op, char *out, size_t size);
void slice_index(Emitter *e, const char *type, const char *base, const char *index, char *out, size_t size);
void go_pointer_index(Emitter *e, const char *base, const char *index, char *out, size_t size);
void emit_destination(Emitter *e, int index, char *out, size_t size);
void literal(Emitter *e, const ZirExpr *expr, const char *type, int negative, char *out, size_t size);
void string_literal(const ZirExpr *expr, ZirTarget target, char *out, size_t size);
void emit_call(Emitter *e, const ZirExpr *expr, const char *array_result, char *out, size_t size);
void emit_function_value(Emitter *e, int index, char *out, size_t size);
int member_path(const ZirFunction *fn, int index);
int expression_calls(const ZirFunction *fn, int index);
/* Whether a call run later could change what expression index reads. */
int call_can_change(const Emitter *e, int index);
void emit_print(Emitter *e, const ZirExpr *expr);
void emit_expr(Emitter *e, int index, const char *expected, char *out, size_t size);
void zero_record(Emitter *e, const char *type, char *out, size_t size);
#pragma GCC visibility pop

#endif
