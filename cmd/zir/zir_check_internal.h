#ifndef ZIR_CHECK_INTERNAL_H
#define ZIR_CHECK_INTERNAL_H

#include "zir_check.h"
#include "zir_borrow.h"
#include "zir_law.h"
#include "zir_text.h"
#include "zir_emit.h"
#include "zir_expr.h"
#include "zir_token.h"
#include "zir_parse.h"
#include "zir_diagnostic.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Binding {
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    char using_path[ZIR_NAME_MAX];
    int depth;
    int is_using_namespace;
    int is_enum_namespace;
    int root_index;
    int moved;
    char moved_paths[32][ZIR_NAME_MAX];
    int moved_path_count;
    int touched;
    int borrow_count;
    int borrows_index;
    char using_filter[160];
} Binding;

typedef struct SpecializationRequest {
    ZirModule *template_owner;
    ZirModule *instance_owner;
    int template_index;
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    ZirSourceSpan call_span;
} SpecializationRequest;

/* print of an enum value calls a generated procedure returning the member
 * name; it is created in the enum's module after checking, like an instance.
 * A print of a record calls a generated procedure too: ARGS and STATEMENTS
 * (heap text, one statement per line) are set for those and NULL otherwise. */
typedef struct EnumNameRequest {
    ZirModule *owner;
    char type[ZIR_NAME_MAX];     /* enum name within its module */
    char function[ZIR_NAME_MAX];
    ZirSourceSpan span;
    char *args;
    char *statements;
} EnumNameRequest;

typedef struct MoveState {
    int moved;
    int moved_path_count;
    char moved_paths[32][ZIR_NAME_MAX];
} MoveState;

typedef struct Checker {
    ZirProgram **programs;
    int program_count;
    ZirModule *module;
    ZirFunction *fn;
    ZirStmt *current_stmt;
    char expected_type[ZIR_NAME_MAX];
    Binding *bindings;
    int count, capacity, depth, errors, failed;
    int inference_only;
    int using_rewritten;
    int aggregate_rewritten;
    int assign_destination;
    int destination_was_moved;
    int conversions_applied;
    char vec_slice_type[ZIR_NAME_MAX];
    struct {
        int at;
        int count;
        MoveState *states;
    } restores[64];
    int restore_count;
    SpecializationRequest *specializations;
    int specialization_count, specialization_capacity;
    EnumNameRequest *enum_names;
    int enum_name_count, enum_name_capacity;
} Checker;

typedef struct ExprOrder {
    const ZirFunction *function;
    ZirExpr *ordered;
    int *map;
    unsigned char *state;
    int count;
} ExprOrder;

typedef struct CompoundConstant {
    char literal[ZIR_TEXT_MAX];
    char qualifier[ZIR_NAME_MAX];
    const ZirModule *owner;
} CompoundConstant;

/* Structural errors are invalid in every backend. Check them before function
 * eligibility can select a native emitter. */
typedef struct RecordPath {
    const ZirType *record;
    const struct RecordPath *parent;
    int depth;
} RecordPath;

typedef struct ValidatedRecords {
    const ZirType **items;
    size_t count;
} ValidatedRecords;

typedef struct PrivateFunctionName {
    ZirFunction *function;
    char internal[ZIR_NAME_MAX];
} PrivateFunctionName;

typedef struct PrivateDefineName {
    ZirDefine *definition;
    char original[ZIR_NAME_MAX];
    char internal[ZIR_NAME_MAX];
} PrivateDefineName;

typedef struct PrivateGlobalName {
    ZirGlobal *global;
    char original[ZIR_NAME_MAX];
    char internal[ZIR_NAME_MAX];
} PrivateGlobalName;

typedef struct PrivateTypeName {
    ZirType *type;
    char original[ZIR_NAME_MAX];
    char internal[ZIR_NAME_MAX];
} PrivateTypeName;

/* Shared between the parts only: the Makefile merges the parts into one
 * object and localizes these hidden symbols, so they never leave it. */
#pragma GCC visibility push(hidden)
void select_lookup_file(ZirModule *module, ZirSourceSpan span);
int in_lookup_file(const ZirModule *module, int is_file_private, ZirSourceSpan span);
int check_file_private_expression(const ZirModule *module, const char *source, ZirSourceSpan span);
int opened_file_enum(ZirModule *module, const char *name, ZirSourceSpan span, int64_t *value);
int check_file_scope_enum_names(const ZirModule *module, const ZirFunction *expression, ZirSourceSpan span);
int contains_vec(const ZirModule *module, const char *type, int depth);
int layout_type(const ZirModule *module, const char *source, int depth, size_t *size, size_t *alignment);
void error(Checker *c, ZirSourceSpan span, const char *message, const char *detail);
void signature_error(Checker *c, ZirSourceSpan span, const char *message, const char *name);
int bind_varargs_call(Checker *c, ZirExpr *call, char parts[][ZIR_TEXT_MAX], int fixed, const char *display_name);
int bind_call_arguments(Checker *c, ZirExpr *call, char parts[][ZIR_TEXT_MAX], int expected, const char *display_name, const char *default_args);
const char *import_type_alias(const ZirModule *module, const ZirModule *owner);
int qualified_global_type(const ZirModule *module, const char *name, const ZirModule *owner, const char *type, char *output, size_t capacity);
void bind(Checker *c, const char *name, const char *type, ZirSourceSpan span);
void activate_using_filtered(Checker *c, const char *path, const char *filter, ZirSourceSpan span);
void activate_using(Checker *c, const char *path, ZirSourceSpan span);
int lower_file_record_using(ZirModule *module, char *source, size_t capacity, ZirSourceSpan span);
int evaluate_global_startup_literal(const ZirModule *module, int before, const char *source, ZirSourceSpan span, char *literal, size_t literal_size, const ZirModule **type_owner);
int resolve_using_enum(Checker *c, const char *name, const ZirType **enumeration, char *source, size_t size);
void promote_using_tree(Checker *c, int index);
int order_using_expressions(ZirFunction *function);
const ZirFunction *function(Checker *c, const char *name, ZirSourceSpan span);
int replace_template_type(char *target, size_t capacity, const char *source, const char *parameter, const char *concrete);
int queue_specialization(Checker *c, const ZirModule *owner, const ZirModule *instance_owner, const ZirFunction *fn, const char *type, char *name, size_t name_size, ZirSourceSpan span);
uint64_t specialization_hash(const ZirModule *owner, const ZirModule *instance_owner, const ZirFunction *fn, const char *type);
const char *lookup_lexical(Checker *c, const char *name);
const ZirGlobal *global_binding(Checker *c, const char *name);
const char *lookup(Checker *c, const char *name);
int owned_vec_binding_type(Checker *c, const char *type);
Binding *lexical_vec_binding(Checker *c, int index);
Binding *lexical_owned_binding(Checker *c, int index);
void check_moved_path_use(Checker *c, int index);
int mark_moved_member_path(Checker *c, int index);
int global_vec_source(Checker *c, int index);
int owned_initializer_shape(Checker *c, int index);
int numeric(const char *type);
int integer_type(const char *type);
int bound_expression(const ZirModule *module, const ZirFunction *expression, int index, int depth, int64_t *value);
int bound_constant(const ZirModule *module, const char *name, int depth, int64_t *value);
int integer_constant(const ZirModule *module, const char *name, int64_t *value);
int bound_string_constant(const ZirModule *module, const char *name, int depth, char *literal, size_t size);
int bound_real_constant(const ZirModule *module, const char *name, int depth, char *literal, size_t size);
int record_field_type_at_use(const ZirModule *module, const ZirModule *record_owner, const char *record_name, char *field_type, size_t size);
int results_type_at_use(const ZirModule *module, const ZirModule *owner, const char *callee, char *type, size_t size);
int lower_compound_global(const ZirModule *module, const ZirGlobal *global, const CompoundConstant *compound, char *result, size_t size);
void compound_qualifier_for_global(CompoundConstant *compound, const char *expected_type);
int visible_define(const ZirModule *module, const char *name, const ZirDefine **definition, const ZirModule **owner, const ZirImport **selected_import);
int bound_compound_constant(const ZirModule *module, const char *name, int depth, CompoundConstant *result);
int array_capacity(const ZirModule *module, const char *type, int *capacity);
void normalize_array(const ZirModule *module, char *type, size_t size);
void normalize_template_array(const ZirModule *module, char *type, size_t size,
                              const char *parameters);
const char *normalized_array(const ZirModule *module, const char *type);
int compatible(const char *to, const char *from);
/* "SUBJECT (expected EXPECTED, found FOUND)" for a type mismatch. */
const char *mismatch_detail(char *out, size_t size, const char *subject,
                            const char *expected, const char *found);
/* True when every `from` value is exactly representable as `to`: a wider
 * integer of the same signedness, an unsigned value in a wider signed type,
 * or float32 in float64. Such values convert implicitly. */
int widens_losslessly(const char *to, const char *from);
/* Rewrite expression `index` into an explicit cast to `to` in the checked
 * graph. Returns the new type, or NULL on allocation failure. */
const char *widen_expression(Checker *c, int index, const char *to);
/* A procedure's results record used as one value selects its first
 * result; see select_first_result. */
const char *results_first_type(Checker *c, const char *type, char *out, size_t size);
const char *select_first_result(Checker *c, int index);
/* Create the enum name procedures print needs; see EnumNameRequest. */
int instantiate_enum_names(Checker *c);
const ZirType *flags_type(Checker *c, const char *name);
int same_declared_type(const ZirModule *module, const char *to, const char *from, int depth);
int compatible_checked(Checker *c, const char *to, const char *from);
int text_type(const char *type);
int assignable(Checker *c, int index);
int readonly_text_destination(Checker *c, int index);
void contextual_slot(Checker *c, int index, const char *expected);
int rewrite_checked_text(Checker *c, const ZirExpr *expr, const char *replacement);
int lower_enum_reference(Checker *c, ZirExpr *expr, const ZirType *enumeration, const char *member, int opened);
int vec_option_result_type(Checker *c, const char *element, ZirSourceSpan span, char *out, size_t size);
int reserve_compound_constants(const ZirModule *module, ZirFunction *fn);
int inline_compound_constant(Checker *c, int index, const CompoundConstant *compound);
const char *expression_type(Checker *c, int index);
int resolve_declared_type_of(Checker *c, char *type, size_t capacity, ZirSourceSpan span);
int compound_operator_assignment(Checker *c, ZirStmt *st);
const char *storage_type_error(const ZirModule *module, const char *source, const RecordPath *path, int indirect, ValidatedRecords *checked, char *detail, size_t detail_capacity);
const char *local_storage_error(const ZirModule *module, const char *type);
int check_type_declarations(ZirModule *module);
int normalize_record_arrays(ZirModule *module);
int all_arms_return(const ZirFunction *fn, int begin, int *last);
int sequence_returns(const ZirFunction *fn, int begin, int end);
int starts_word(const char *source, const char *word);
void if_case_error(Checker *c, ZirSourceSpan span, const char *message, const char *detail);
int scalar_case_type(const char *type);
int lower_if_case(Checker *c, int index, const char *checked_type);
int validate_loop_targets(Checker *c, const ZirFunction *fn);
int discarded_must_call(Checker *c, int index);
const char *try_conversion(Checker *c, int index, const char *to, ZirSourceSpan span);
int rebuild_conversion_layout(ZirFunction *fn);
void mark_expr_moves(Checker *c, int index);
void check_vec_call_results(Checker *c, int index, int transferred, int discarded);
int check_function(Checker *c, ZirFunction *fn);
int check_template_declaration(Checker *c, ZirFunction *fn);
int normalize_function_arrays(const ZirModule *module, ZirFunction *fn);
int canonical_type_arguments(const char *source, char *output, size_t capacity);
int rewrite_type_applications(ZirModule *module, const char *source, char *output, size_t capacity, ZirSourceSpan span, int recursion);
int normalize_type_applications(ZirModule *module);
int check_foreign_slice_returns(ZirProgram **programs, int count);
int fill_late_type_instances(ZirModule *module, ZirSourceSpan span);
int order_local_types(ZirModule *module);
int jai_module_types(const ZirModule *module);
int name_private_functions(ZirModule *module);
int name_private_defines(ZirModule *module);
int name_private_globals(ZirModule *module);
int name_private_types(ZirModule *module);
int instantiate_specializations(Checker *checker);
#pragma GCC visibility pop

#endif
