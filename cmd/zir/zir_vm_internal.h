#ifndef ZIR_VM_INTERNAL_H
#define ZIR_VM_INTERNAL_H

#include "zir_vm.h"
#include "zir_check.h"
#include "zir_parse.h"
#include "zir_expr.h"
#include "zir_diagnostic.h"
#include "zir_text.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { VM_MAX_PARAMS = 16, VM_MAX_LOCALS = 64, VM_MAX_GLOBALS = 4096,
       VM_MAX_DEPTH = 128,
       VM_MAX_STEPS = 1000000, VM_MAX_FIELDS = 1024,
       VM_MAX_RECORD_BYTES = 256 * 1024 * 1024,
       VM_MAX_ARRAY_BYTES = 256 * 1024 * 1024 };

typedef struct Parameter {
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
} Parameter;

typedef enum ValueKind {
    VALUE_INVALID,
    VALUE_VOID,
    VALUE_INT,
    VALUE_REAL,
    VALUE_STRING,
    VALUE_ENUM,
    VALUE_RECORD,
    VALUE_ARRAY,
    VALUE_SLICE,
    VALUE_SLOT
} ValueKind;

typedef struct Record Record;
typedef struct Array Array;
typedef struct Frame Frame;

typedef struct Value {
    ValueKind kind;
    int64_t integer;
    uint64_t bits;
    int unsigned64;
    double real;
    const unsigned char *data;
    size_t length;
    size_t offset;
    const ZirType *enumeration;
    Record *record;
    Array *array;
    const ZirType *slot_type;
    const ZirModule *slot_module;
    const ZirFunction *slot_function;
} Value;

typedef struct RecordField {
    ZirTypeField field;
    Value value;
} RecordField;

struct Record {
    Record *next;
    int retired;
    uint64_t pinned;
    uint64_t allocation;
    const ZirModule *owner;
    const ZirType *type;
    int field_count;
    RecordField fields[];
};

struct Array {
    Array *next;
    int retired;
    uint64_t pinned;
    uint64_t allocation;
    const ZirModule *owner;
    char element_type[ZIR_NAME_MAX];
    int length;
    Value elements[];
};

typedef struct StringLiteral {
    struct StringLiteral *next;
    const ZirExpr *expression;
    size_t length;
    unsigned char data[];
} StringLiteral;

typedef struct Local {
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    Value value;
} Local;

typedef struct GlobalSlot {
    const ZirModule *module;
    const ZirGlobal *declaration;
    Value value;
} GlobalSlot;

typedef struct Vm {
    const ZirProgram *program;
    int depth;
    int steps;
    int failed;
    size_t record_bytes;
    size_t array_bytes;
    size_t allocated_since_collection;
    uint64_t allocation;
    uint64_t pin_generation;
    uint64_t retire_floor;
    Record *records;
    Array *arrays;
    StringLiteral *strings;
    GlobalSlot *globals;
    int global_count;
    Frame *active_frame;
    VmHostCall host;
    void *host_context;
} Vm;

struct VmInstance {
    Vm vm;
    const ZirModule *module;
    const ZirFunction *entry;
};

struct Frame {
    Vm *vm;
    const ZirModule *module;
    const ZirFunction *function;
    Frame *caller;
    Local locals[VM_MAX_LOCALS];
    int local_count;
    int control_target;
    /* Set while a union member is the assignment destination. */
    Record *union_write_record;
    const char *union_write_type;
};

typedef enum Flow {
    FLOW_NEXT,
    FLOW_RETURN,
    FLOW_BREAK,
    FLOW_CONTINUE,
    FLOW_ERROR
} Flow;

typedef struct VmTypePath {
    const ZirType *record;
    const struct VmTypePath *parent;
} VmTypePath;

/* Shared between the parts only: the Makefile merges the parts into one
 * object and localizes these hidden symbols, so they never leave it. */
#pragma GCC visibility push(hidden)
const ZirImport *host_import(const ZirModule *module, const char *name);
const ZirFunction *bound_provider(const ZirProgram *program, const ZirImport *import, const ZirModule **owner);
int same_bound_type(const ZirModule *caller, const ZirModule *provider, const char *type);
ValueKind value_kind(const char *type);
int scalar_type(const char *type);
int portable_union(const ZirModule *module, const ZirType *record);
int portable_type(const ZirModule *module, const char *type);
int host_type_at(const ZirModule *module, const char *type, int depth, int slice_parameter);
Value int_value(int64_t integer);
Value uint_value(uint64_t bits);
Value real_value(double real);
Value string_value(const unsigned char *data, size_t length);
Value literal_string(Vm *vm, const ZirExpr *expression);
Value global_literal_string(Vm *vm, const char *source);
Value enum_value(const ZirType *type, int64_t integer);
uint64_t integer_bits(Value value);
int64_t signed64(uint64_t bits);
double as_real(Value value);
int truthy(Value value);
Record *allocate_record(Vm *vm, const ZirModule *owner, const ZirType *type, int count);
Array *allocate_array_try(Vm *vm, const ZirModule *owner, const char *element, int length, int fail_hard);
Value default_value(Vm *vm, const ZirModule *module, const char *type, int depth);
Value coerce(Vm *vm, const ZirModule *module, Value value, const char *type);
Value coerce_expression(Vm *vm, const ZirModule *module, Value value, const char *type);
void retire_value(Vm *vm, Value value, int depth);
int array_has_active_slice(Vm *vm, const Array *array);
void release_retired(Vm *vm);
int vm_type_contains_vec(const ZirModule *module, const char *type, int depth);
void drop_owned_locals(Frame *frame, int first);
int parse_parameters(const ZirModule *module, const ZirFunction *function, Parameter *parameters);
int bitwise_operator(const char *op);
const char *assignment_binary_operator(const char *op);
const ZirFunction *find_entry(const ZirProgram *program, const char *module_name, const char *function_name, const ZirModule **module_out);
int is_else_branch(const ZirStmt *statement);
int statement_close(const ZirFunction *function, int begin, int end);
Value *record_field(Record *record, const char *name);
Value union_member_read(Vm *vm, Record *record, const char *field_type);
int union_member_write(Vm *vm, Record *record, const char *field_type, Value value);
Value *assignment_slot(Frame *frame, int index, int depth);
Value binary_value(Vm *vm, const char *op, Value left, Value right, const char *left_type, const char *right_type);
Value eval(Frame *frame, int index, int depth);
void pin_value(Vm *vm, Value value, int depth);
Value run_function(Vm *vm, const ZirModule *module, const ZirFunction *function, const Value *args, int arg_count);
void free_records(Vm *vm);
void free_arrays(Vm *vm);
void free_strings(Vm *vm);
int fold_global_element(Vm *vm, const ZirModule *module, const ZirFunction *probe, int index, Value *target, const char *type, ZirSourceSpan span);
#pragma GCC visibility pop

#endif
