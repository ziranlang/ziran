#ifndef ZIR_RUST_INTERNAL_H
#define ZIR_RUST_INTERNAL_H

/*
 * zir_rust_lower.c - checked ZIR to Rust backend.
 */
#include "zir_rust_lower.h"
#include "zir_rust_asm.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"
#include "zir_text.h"
#include "zir_scalar.h"
#include "zir_check.h"
#include "zir_parse.h"
#include "zir_stream.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#endif

#define ZIR_RUST_TEXT_MAX 65536
#define ZIR_RUST_NAME_MAX 256
#define ZIR_RUST_LOCAL_MAX 512

typedef struct RustLocal {
    char source[ZIR_NAME_MAX];
    char rust[ZIR_NAME_MAX];
} RustLocal;

typedef struct RustEmitter {
    FILE *output;
    const ZirProgram *const *programs;
    int program_count;
    const ZirModule *module;
    const ZirFunction *function;
    RustLocal locals[ZIR_RUST_LOCAL_MAX];
    int local_count;
    int indent;
    /* Loops written as for ... in, whose marked steps the range takes. */
    int native_loops[64];
    int native_loop_count;
} RustEmitter;

typedef struct RustModuleVisit {
    const ZirModule *module;
    int state;
} RustModuleVisit;

typedef struct RustModuleVisits {
    RustModuleVisit *items;
    size_t count;
    size_t capacity;
} RustModuleVisits;


/* Private interfaces between this backend's types, values and output passes. */
void
emit_destination(RustEmitter *emitter, int index, char *output,
                             size_t size);
void
emit_expression(RustEmitter *emitter, int index, char *output,
                            size_t size);
void
emit_extern_definitions(RustEmitter *emitter, FILE *output);
void
emit_global_definitions(RustEmitter *emitter, FILE *output);
int
emit_print_statement(RustEmitter *emitter, const ZirExpr *expression);
int emit_rust_assembly(RustEmitter *emitter, int statement);
void
emit_sequence(RustEmitter *emitter, int begin, int end);
int emit_startup(RustEmitter *emitter, FILE *output);
void
emit_type_definitions(RustEmitter *emitter, FILE *output);
void
emit_typed_expression(RustEmitter *emitter, int index,
                                  const char *type, char *output, size_t size);
void emit_ziran_vec_runtime(FILE *output);
int float_type(const char *type);
void function_symbol(RustEmitter *emitter, const ZirModule *module,
                            const ZirFunction *function, char *output,
                            size_t size);
int global_reference(RustEmitter *emitter, const char *name,
                            char *output, size_t size);
void global_symbol(RustEmitter *emitter, const ZirModule *module,
                           const ZirGlobal *global, char *output, size_t size);
int integer_type(const char *type);
const char *local_name(RustEmitter *emitter, const char *source);
void
lower_function(RustEmitter *emitter, const ZirModule *module,
                           const ZirFunction *function);
void make_directories(const char *path);
void register_local(RustEmitter *emitter, const char *source);
void require_rust_type(RustEmitter *emitter, ZirSourceSpan span,
                              const char *type, char *output, size_t size);
const char *rust_bare(const char *text, char *output, size_t size);
const char *rust_common_integer(const char *left, const char *right);
int rust_copyable_type(RustEmitter *emitter, const char *type);
unsigned rust_direction_codepoint(const unsigned char *bytes, size_t length);
int rust_enum_type(RustEmitter *emitter, const char *type,
                          const ZirModule **owner, const ZirType **enumeration);
void rust_extern_symbol(RustEmitter *emitter, const ZirModule *module,
                               const ZirImport *import, char *output,
                               size_t size);
void rust_field_name(const ZirType *record, const char *source,
                            char *output, size_t size);
int rust_function_needs_unsafe(const ZirModule *module, const ZirFunction *function);
void rust_identifier(const char *source, char *output, size_t size);
void rust_member_path(RustEmitter *emitter, const ZirModule *owner,
                             const ZirType *record, const char *path,
                             char *output, size_t size);
int rust_name_mutable(const ZirFunction *function, const char *name,
                             const char *type, int declaration);
int rust_option_type(RustEmitter *emitter, const char *type,
                            const ZirModule **owner, const ZirType **record,
                            char *element, size_t element_size);
int rust_owned_vec_type(RustEmitter *emitter, const char *type,
                               const ZirModule **owner,
                               const ZirType **record, char *element,
                               size_t element_size);
int rust_plain_receiver(const char *text);
int rust_procedure_type(RustEmitter *emitter, const char *type,
                               const ZirModule **owner,
                               const ZirType **procedure);
int rust_record_type(RustEmitter *emitter, const char *type,
                            const ZirModule **owner, const ZirType **record);
const char *rust_scalar_type(const char *type);
int rust_string_literal(const char *source, char *output, size_t size);
int rust_text_needs_unsafe(const char *text, size_t size);
int rust_type(RustEmitter *emitter, const char *type, char *output,
                     size_t size);
void
rust_zero_value(RustEmitter *emitter, const char *type,
                            char *output, size_t size);
void slot_wrapper_symbol(RustEmitter *emitter, int expression,
                                 char *output, size_t size);
void unsupported_expression(RustEmitter *emitter,
                                   const ZirExpr *expression);
void
validate_module(const ZirModule *module);
const char *wrapping_method(const char *operation);
int wrapping_operation(const char *operation);
void write_line(RustEmitter *emitter, const char *format, ...);
void write_rust_lint_allowances(FILE *output, const char *runtime, size_t runtime_size,
                                       const char *body, size_t body_size, int executable);
int write_used_runtime(FILE *output, const char *runtime, size_t runtime_size,
                              const char *body, size_t body_size);
#endif
