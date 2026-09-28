#ifndef ZIR_PARSE_INTERNAL_H
#define ZIR_PARSE_INTERNAL_H

/*
 * zir_parse.c - shared Ziran frontend: parse .zi source into a ZirProgram.
 * Linked by the IR and native backends.
 */
#include "zir.h"
#include "zir_parse.h"
#include "zir_text.h"
#include "zir_cleanup.h"
#include "zir_expr.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_token.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SOURCE_PATH_MAX = 1024,
    SOURCE_LINE_MAX = 4096
};

typedef struct SourceBuffer {
    char *text;
    size_t length;
    size_t capacity;
} SourceBuffer;

/* ---- compile-time conditionals ------------------------------------------
 * '#if COND { ... } else { ... }' selects a branch while parsing. Its braces
 * are consumed here and do not create a source-level scope. */

typedef struct {
    char name[ZIR_NAME_MAX];
    char expr[ZIR_TEXT_MAX];
    char type[ZIR_NAME_MAX]; /* active evaluator local; empty for file constants */
    char path[SOURCE_PATH_MAX];
    int is_file_private;
    int is_public;
    int source_line;
} ZirConst;

typedef struct {
    ZirConst *items;
    int count;
    int capacity;
} ZirConsts;

typedef struct {
    ZirUsing *items;
    int count;
    int capacity;
} ZirUsings;

typedef struct {
    ZirImport *items;
    int count;
    int capacity;
} ZirImports;

typedef struct {
    ZirType *items;
    int count;
    int capacity;
} ZirTypes;

typedef struct {
    char *source;
    char *path;
    char rel[SOURCE_PATH_MAX];
    int line;
    int is_public;
    int is_file_private;
} ZirDeferredType;

typedef struct {
    ZirDeferredType *items;
    int count;
    int capacity;
} ZirDeferredTypes;

typedef struct {
    ZirFunction *items;
    int count;
    int capacity;
} ZirFunctions;

typedef struct {
    ZirGlobal *items;
    int count;
    int capacity;
} ZirGlobals;

typedef struct {
    char **paths;
    int count;
    int capacity;
} ZirDiscoveredFiles;

typedef struct {
    int braces;                   /* net '{' until the region's closing '}' */
    int selected;
    int active;
    int parent_active;
} ZirCondFrame;

typedef struct ZirEval {
    const char *p;
    int known;
    long value;
    const ZirModule *module;
    const ZirConsts *consts;
    const char *lookup_path;
    int source_line;
    int depth;
    int *fuel;
} ZirEval;

typedef enum {
    COMPILE_INVALID, COMPILE_INTEGER, COMPILE_REAL, COMPILE_STRING,
    COMPILE_COMPOUND
} CompileKind;

typedef struct {
    CompileKind kind;
    long integer;
    double real;
    char type[ZIR_NAME_MAX];
    char literal[ZIR_TEXT_MAX];
    const ZirModule *type_owner;
} CompileValue;

typedef struct {
    const ZirModule *module;
    const ZirFunction *fn;
    ZirConsts names;
    int local_count;
    int capacity;
    int depth;
    int *fuel;
    ZirSourceSpan current_span;
} TypedBody;

typedef struct CompileParseContext {
    ZirProgram *program;
    const char *root;
    ZirCompileImportResolver resolver;
    void *resolver_context;
    const ZirConsts *future_constants;
    const ZirUsings *future_usings;
    const ZirImports *future_imports;
    const ZirTypes *future_types;
    const ZirFunctions *future_functions;
    const ZirGlobals *future_globals;
} CompileParseContext;

typedef struct LoadFrame {
    const char *path;
    const char *source;
    char *canonical;
    char *owned_source;
    char rel[SOURCE_PATH_MAX];
    char lookahead[SOURCE_LINE_MAX];
    int line_no;
    int physical_line_no;
    int have_look;
    int scope_public;
    int scope_file;
    int in_block_comment;
    int conditional_depth;
} LoadFrame;

/* Shared between the parts only: the Makefile merges the parts into one
 * object and localizes these hidden symbols, so they never leave it. */
#pragma GCC visibility push(hidden)
void die(const char *fmt, ...);
void die_at(ZirSourceSpan span, const char *fmt, ...);
void source_append(SourceBuffer *buffer, char byte);
int source_identifier_byte(unsigned char byte);
char *lower_jai_multiline_strings(const char *source, const char *path);
int starts_word(const char *s, const char *word);
int looks_like_non_jai_control(const char *text, const char *word);
int is_identifier_text(const char *text);
int parse_using_modifiers(const char **cursor, char *filter, size_t filter_size, const char *path, int line_no);
int is_member_path_text(const char *text);
void parse_file_global(ZirModule *module, const char *declaration, ZirSourceSpan span, int scope_public, int scope_file, char *name_out, size_t name_size);
int contains_source_directive(const char *source, const char *directive);
int source_column_for_trimmed(const char *line, const char *trimmed);
int source_end_column_for_trimmed(const char *line, const char *trimmed);
int parse_symbol_before_colons(const char *s, char *out, size_t out_size);
int parse_type_parameters(const char *after, const char *keyword, char *names, size_t capacity);
int line_is_abi_incomplete(const char *line, size_t length);
int take_abi_incomplete(ZirType *type);
int expand_type_this(ZirType *type);
const char *relative_path(const char *root, const char *path);
int parse_quoted(const char *s, char *out, size_t out_size);
int is_c_ident(const char *s);
ZirExternKind classify_extern_target(const char *target, char *symbol, size_t symbol_size, const char *path, int line_no);
int net_block_braces(const char *s);
int looks_like_label(const char *s);
ZirStmtKind classify_stmt(const char *s);
int parse_block_call_header(const char *text, char *callee, size_t callee_size, char *name, size_t name_size);
char *statement_separator(char *line);
void prepend_logical_line(char queue[16][SOURCE_LINE_MAX * 2], int *count, const char *line, ZirSourceSpan span);
int split_oneline_block(const char *t, char *head, size_t hsz, char *body, size_t bsz, char *tail, size_t tsz);
const char *closing_parenthesis(const char *open);
void parse_function_header(char *name, size_t name_size, char *args, size_t args_size, char *ret, size_t ret_size, const char *line);
int function_must_use(const char *line, const char *return_type, ZirSourceSpan span);
int strip_program_export(char *line, char *symbol, size_t symbol_size, ZirSourceSpan span);
void separate_parameter_defaults(char *args, size_t capacity, char *defaults, size_t defaults_capacity, ZirSourceSpan span);
uint64_t strip_using_parameters(char *args, size_t capacity, ZirSourceSpan span);
int lower_procedure_name_expression(char *part, size_t capacity, const ZirFunction *function);
void add_default_helpers(ZirProgram *program, ZirModule *module, const char *source_path, const char *root, ZirCompileImportResolver resolver, void *resolver_context);
int parse_import_line(ZirModule *module, const char *path, int line_no, const char *line, int scope_public);
int parse_foreign_library_line(const char *path, int line_no, const char *line, char names[][ZIR_NAME_MAX], char targets[][ZIR_PATH_MAX], int *count);
int parse_foreign_line(ZirModule *module, const char *path, int line_no, const char *line, int scope_public, char names[][ZIR_NAME_MAX], char targets[][ZIR_PATH_MAX], char paths[][SOURCE_PATH_MAX], const int *file_private, int count);
int brace_outside_literals(const char *text);
int looks_like_function_header(const char *line);
int split_oneline_function(const char *line, char *head, size_t head_size, char *body, size_t body_size);
void split_jai_control_line(char *line, size_t capacity, char queue[16][SOURCE_LINE_MAX * 2], int *count, ZirSourceSpan span);
int line_is_compile_else(const char *line);
int parse_cond_start(char *line, char **condition);
int line_starts_compile_condition(const char *line);
void expand_compile_expr(char *dst, size_t dst_size, const ZirConsts *consts, const char *src, const char *lookup_path);
char *find_top_comma(char *s);
int eval_integer_type(const char *type, long value);
int eval_close(const ZirFunction *fn, int opening, int stop);
int checked_add_long(long left, long right, long *result);
int checked_sub_long(long left, long right, long *result);
int checked_mul_long(long left, long right, long *result);
int eval_const_condition_with_fuel(const char *src, long *value, const ZirModule *module, const ZirConsts *consts, const char *lookup_path, int line, int depth, int *fuel);
int eval_const_condition(const char *src, long *value, const ZirModule *module, const ZirConsts *consts, const char *lookup_path, int line, int depth);
int compile_value_literal(CompileValue *value);
int compile_truth(const CompileValue *value, int *truth);
int compile_type_value(const char *type, CompileValue *value);
int wrap_compile_integer(const char *type, long *value);
int compile_values_equal(const CompileValue *left, const CompileValue *right, int *equal);
int compile_compound_value(const ZirFunction *probe, const ZirExpr *expression, const ZirModule *module, const char *path, int depth, int *fuel, CompileValue *result);
int compile_compound_member(const CompileValue *compound, const char *member, const ZirModule *module, const char *path, int depth, int *fuel, CompileValue *result);
int compile_compound_index(const CompileValue *compound, long index, const ZirModule *module, const char *path, int depth, int *fuel, CompileValue *result);
int evaluate_imported_typed_define(const ZirModule *module, const char *path, const char *name, int depth, int *fuel, CompileValue *result);
int evaluate_typed_node(const ZirFunction *probe, int index, const ZirModule *module, const char *path, int depth, int *fuel, CompileValue *result);
int evaluate_typed_expression(const ZirModule *module, const ZirConsts *names, const char *source, ZirSourceSpan span, int depth, int *fuel, CompileValue *result);
int evaluate_typed_integer_function(ZirEval *ev, const char *name, const long *values, const char argument_names[][ZIR_NAME_MAX], int argument_count, long *result);
int eval_typed_condition(const char *source, const ZirModule *module, const ZirConsts *names, ZirSourceSpan span, long *result);
ZirConsts visible_compile_constants(const ZirConsts *parsed, const ZirConsts *future);
ZirModule visible_compile_module(const ZirModule *parsed, const CompileParseContext *context, const ZirConsts *constants);
void free_visible_compile_module(ZirModule *visible);
int select_compile_condition(ZirModule *module, const ZirConsts *consts, const char *source, ZirSourceSpan span, const CompileParseContext *context, const char *source_path, int *deferred);
char *find_unquoted_text(char *source, const char *needle);
char *find_unquoted_word(char *source, const char *word);
int lower_compile_ifx_value(char *value, size_t capacity, const ZirModule *module, const ZirConsts *consts, ZirSourceSpan span, int allow_deferred);
int lower_compile_ifx_function(ZirFunction *fn, const ZirModule *module, const ZirConsts *consts, int allow_deferred);
void lower_size_of_value(char *value, size_t capacity, const ZirModule *module, ZirSourceSpan span);
int parse_compile_check(ZirModule *module, const char *path, int line_no, char *line, const ZirConsts *consts);
int cond_top_step(char *line, ZirCondFrame *frames, int *count, ZirModule *module, const ZirConsts *consts, const CompileParseContext *context, const char *source_path, const char *path, int line_no, int *deferred);
void cond_frame_settle(ZirCondFrame *frames, int count);
void strip_block_comments(char *s, int *comment_depth);
void normalize_jai_source_tokens(char *line, const char *path, const char *physical_path, int line_no);
char *read_source_line(char *line, size_t size, const char **source, const char *path, int line_no);
void free_deferred_types(ZirDeferredTypes *deferred);
void free_discovered_files(ZirDiscoveredFiles *files);
void free_discovered_functions(ZirFunctions *functions);
void discover_file_scope(const char *source, const char *path, const char *rel, const char *root, ZirConsts *future_constants, ZirUsings *future_usings, ZirImports *future_imports, ZirTypes *future_types, ZirDeferredTypes *deferred_types, ZirFunctions *future_functions, ZirGlobals *future_globals, ZirDiscoveredFiles *files, int depth);
void discover_conditional_types(const ZirDeferredTypes *deferred, ZirModule *module, const ZirConsts *constants, const CompileParseContext *context, ZirTypes *future);
void lower_enum_values(ZirType *type);
void parse_enum_backing(ZirType *type, const char *header);
char *read_lowered_source(const char *path);
char *import_string_source(const char *line, ZirSourceSpan span);
#pragma GCC visibility pop

#endif
