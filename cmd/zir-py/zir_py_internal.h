#ifndef ZIR_PY_INTERNAL_H
#define ZIR_PY_INTERNAL_H

/*
 * zir_py_lower.c - checked ZIR to Python backend.
 *
 * Python has no fixed-width integers, value records, or raw pointers, so
 * the generated code keeps each Ziran rule explicitly:
 *
 * - Integer arithmetic wraps at its type's width through _s32, _u8, and
 *   friends; division truncates toward zero; shifts check their count.
 * - Records are classes and fixed arrays are lists. Ziran copies them by
 *   value, so a value taken from a place is copied, and an assignment to a
 *   place that a pointer or view may reach overwrites it in place.
 * - A pointer is a ZiranPointer to a list item, an object attribute, or an
 *   object. A local whose address is taken lives in a one-item list.
 * - Strings are immutable bytes; a view of mutable bytes is a snapshot.
 */
#include "zir_py_lower.h"
#include "zir_py_runtime.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"
#include "zir_text.h"
#include "zir_check.h"
#include "zir_parse.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    PY_VOID = 1,
    PY_BOOL,
    PY_INT,
    PY_FLOAT,
    PY_STRING,
    PY_POINTER,
    PY_NULL,
    PY_PROCEDURE,
    PY_SLICE,
    PY_ARRAY,
    PY_VEC,
    PY_RECORD,
    PY_UNION
};

/* A checked type as the Python target sees it. */
typedef struct PyType {
    int kind;
    int bits;          /* integer and float width */
    int is_signed;
    int capacity;      /* fixed array length */
    char element[ZIR_NAME_MAX]; /* array, slice, Vec, or pointer element */
    const ZirModule *owner;     /* module that declares a named type */
    const ZirType *declared;
} PyType;

typedef struct PyLocal {
    char source[ZIR_NAME_MAX];
    char python[ZIR_NAME_MAX];
    int depth;
    int boxed;
} PyLocal;

typedef struct PyNames {
    char (*items)[ZIR_NAME_MAX];
    int count;
    int capacity;
} PyNames;

typedef struct PyEmitter {
    FILE *output;
    const ZirProgram *const *programs;
    int program_count;
    const ZirModule *module;
    const ZirFunction *function;
    PyLocal *locals;
    int local_count;
    int local_capacity;
    PyNames module_names;   /* every top-level Python name */
    PyNames boxed;          /* source locals whose address is taken */
    PyNames in_place;       /* source locals a pointer or view may reach */
    PyNames counters;       /* Python names of loop counters that never go below zero */
    int depth;
    int indent;
    long lines;
    int loops[128];         /* enclosing loop ids, innermost last */
    int loop_count;
    int native_loops[128];
    int native_loop_count;
    int temporaries;
    int uses_host;
} PyEmitter;


/* Private interfaces between this backend's types, values and output passes. */
const char *base_type_of(PyEmitter *emitter, const ZirExpr *expression);
PyLocal *declare_local(PyEmitter *emitter, const char *source);
char *emit_expression(PyEmitter *emitter, int index);
char *emit_place(PyEmitter *emitter, int index);
char *emit_value(PyEmitter *emitter, int index, const char *type);
const ZirExpr *expression_at(PyEmitter *emitter, int index);
int expression_calls(const ZirFunction *function, int index);
int expression_is_place(const ZirExpr *expression);
PyLocal *find_local(PyEmitter *emitter, const char *source);
int foreign_parameters(const ZirImport *import, char types[][ZIR_NAME_MAX], int maximum);
void foreign_symbol(PyEmitter *emitter, const ZirModule *module,
                           const ZirImport *import, char *output, size_t size);
int function_parameter_names(const ZirFunction *function,
                                    char names[][ZIR_NAME_MAX],
                                    char types[][ZIR_NAME_MAX], int maximum);
void function_symbol(PyEmitter *emitter, const ZirModule *module,
                            const ZirFunction *function, char *output, size_t size);
int global_reference(PyEmitter *emitter, const char *name,
                            char *output, size_t size, const ZirGlobal **found);
void global_symbol(PyEmitter *emitter, const ZirModule *module,
                          const ZirGlobal *global, char *output, size_t size);
int identifier_character(int character);
int is_nonnegative_literal(const char *text);
void leave_scope(PyEmitter *emitter);
void lower_function(PyEmitter *emitter, const ZirModule *module,
                           const ZirFunction *function);
void names_add(PyNames *names, const char *name);
int names_has(const PyNames *names, const char *name);
const char *place_base(const ZirFunction *function, int index, int *through_member);
int py_aggregate(int kind);
void *py_allocate(size_t size);
void py_assign_in_place(PyEmitter *emitter, const ZirModule *scope,
                               const char *type, const char *place, const char *value);
char *py_bare(const char *text);
void py_class_name(const PyType *info, char *output, size_t size);
int py_classify(const ZirModule *scope, const char *type, PyType *out);
char *py_convert(PyEmitter *emitter, const char *text, const char *from,
                        const char *to);
char *py_copy_string(const char *text);
char *py_copy_text(PyEmitter *emitter, const ZirModule *scope,
                          const char *type, const char *text, int depth);
const char *py_ctype(PyEmitter *emitter, const char *type);
void py_field_name(const char *source, char *output, size_t size);
const ZirImport *py_foreign_import(const ZirModule *module, const char *name);
char *py_format(const char *format, ...);
void py_identifier(const char *source, char *output, size_t size);
char *py_index(PyEmitter *emitter, const char *text);
int py_integer_literal(const char *text, long long *value);
int py_integer_width(const char *type, int *bits, int *is_signed);
char *py_member_path(const char *path);
void py_module_symbol(const char *path, char *output, size_t size);
const char *py_pointer_ctype(PyEmitter *emitter, const char *type);
void py_require_type(const ZirModule *scope, ZirSourceSpan span,
                            const char *type, PyType *out);
char *py_round_float32_literal(double value);
char *py_string_literal(const unsigned char *bytes, size_t length);
char *py_wrap(const char *text, int bits, int is_signed);
const char *py_wrap_name(int bits, int is_signed);
char *py_wrapped_literal(unsigned long long bits, int width, int is_signed);
char *py_zero(PyEmitter *emitter, const ZirModule *scope, const char *type);
int text_integer(const char *text, long long *value);
void unsupported_expression(const ZirExpr *expression);
void write_line(PyEmitter *emitter, const char *format, ...);
#endif
