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

/* VM_MAX_PARAMS is at most 64: named arguments are tracked in a uint64_t. */
enum { VM_MAX_PARAMS = 64, VM_MAX_GLOBALS = 4096,
       VM_MAX_DEPTH = 128,
       VM_MAX_FIELDS = 1024,
       VM_MAX_RECORD_BYTES = 1024 * 1024 * 1024,
       VM_MAX_ARRAY_BYTES = 1024 * 1024 * 1024 };

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
    VALUE_SLOT,
    VALUE_POINTER /* pointee: storage in a frame, global, record, or array */
} ValueKind;

typedef struct Record Record;
typedef struct Array Array;
typedef struct Frame Frame;
typedef struct StringLiteral StringLiteral;

typedef struct Value {
    ValueKind kind;
    union {
        int unsigned64;
        int indexed; /* VALUE_POINTER: array plus logical element offset */
    };
    /* Only the payload selected by kind is live. Integers need both their
     * signed value and unsigned bits; pointers need a target, call serial
     * and both container references. Other kinds share those same words. */
    union {
        int64_t integer;
        double real;
        const unsigned char *data;
        size_t offset;
        const ZirType *slot_type;
        struct Value *pointee;
    };
    union {
        uint64_t bits;
        size_t length;
        const ZirModule *slot_module;
    };
    union {
        StringLiteral *string_owner; /* snapshots and ranges/copies of them */
        const ZirType *enumeration;
        Record *record;
        const ZirFunction *slot_function;
    };
    Array *array;
} Value;

/* Keep scalar construction visible to every evaluator part. Returning a
 * whole Value through a separate function forces an extra stack copy on
 * each construction, including the empty expression-root slots. */
static inline Value
int_value(int64_t integer)
{
    return (Value){.kind = VALUE_INT, .integer = integer,
                   .bits = (uint64_t)integer};
}

static inline Value
uint_value(uint64_t bits)
{
    return (Value){.kind = VALUE_INT, .bits = bits, .unsigned64 = 1};
}

static inline Value
real_value(double real)
{
    return (Value){.kind = VALUE_REAL, .real = real};
}

static inline Value
string_value(const unsigned char *data, size_t length)
{
    return (Value){.kind = VALUE_STRING, .data = data, .length = length};
}

/* Values held by an expression while another expression or call runs.
 * The nodes borrow stack or argument-buffer storage and are removed by the
 * evaluator before that storage goes away. */
typedef struct VmRoots {
    struct VmRoots *previous;
    const Value *values;
    int count;
} VmRoots;

/* One record field's name and type. Both strings are interned with
 * KeepText, so every record of a type shares them. */
typedef struct VmField {
    const char *name;
    const char *type;
} VmField;

/* A record type's fields, parsed once per VM from the type's text. The
 * table can move as it grows; `fields` stays at one address. */
typedef struct VmLayout {
    const ZirType *type;
    int count;          /* -1 when the fields are malformed or too many */
    VmField *fields;
    int *field_slots;
    size_t field_slot_count;
} VmLayout;

typedef struct RecordField {
    VmField field;
    Value value;
} RecordField;

struct Record {
    Record *next;
    Record *previous;
    Record *retired_next, *retired_previous;
    int retired;
    int address_taken; /* a pointer may reach a field; never retired early */
    uint64_t pinned;
    uint64_t allocation;
    const ZirModule *owner;
    const ZirType *type;
    int field_count;
    const int *field_slots;
    size_t field_slot_count;
    RecordField fields[];
};

typedef enum ArrayStorage {
    ARRAY_BOXED, ARRAY_S8, ARRAY_U8, ARRAY_S16, ARRAY_U16,
    ARRAY_S32, ARRAY_U32, ARRAY_S64, ARRAY_U64, ARRAY_F32, ARRAY_F64
} ArrayStorage;

struct Array {
    Array *next;
    Array *previous;
    Array *retired_next, *retired_previous;
    int retired;
    int address_taken; /* a pointer may reach an element; never retired early */
    int borrowed; /* a slice has observed this storage; returns must copy it */
    int heap;  /* New(T) storage, which free releases */
    int freed; /* released by free; reading through a pointer fails */
    uint64_t pinned;
    uint64_t allocation;
    const ZirModule *owner;
    const char *element_type; /* shared immutable checked spelling */
    int holds_references; /* elements can reach records or arrays */
    int length;
    ArrayStorage storage;
    size_t data_bytes;
    /* Stable logical locations let pointers and slices borrow typed storage.
     * Packed slots have one validity bit so popped/moved Vec slots retain
     * their invalid state, including writes through older slices. */
    _Alignas(Value) unsigned char data[];
};

static inline size_t
array_size(const Array *array)
{
    return sizeof(Array) + array->data_bytes +
        (array->storage == ARRAY_BOXED ? 0 : ((size_t)array->length + 7) / 8);
}

static inline Value *
array_boxed_slot(Array *array, size_t index)
{
    return array != NULL && array->storage == ARRAY_BOXED &&
        index < (size_t)array->length ? &((Value *)array->data)[index] : NULL;
}

static inline Value
array_get(const Array *array, size_t index)
{
    if(array == NULL || index >= (size_t)array->length)
        return (Value){0};
    if(array->storage == ARRAY_BOXED)
        return ((const Value *)array->data)[index];
    if(!(array->data[array->data_bytes + index / 8] & (1u << (index % 8))))
        return (Value){0};
    switch(array->storage) {
    case ARRAY_S8: return int_value(((const int8_t *)array->data)[index]);
    case ARRAY_U8: return int_value(array->data[index]);
    /* Fixed-size copies avoid aliasing typed scalars through byte storage. */
    case ARRAY_S16: { int16_t item; memcpy(&item, array->data + index * 2, 2); return int_value(item); }
    case ARRAY_U16: { uint16_t item; memcpy(&item, array->data + index * 2, 2); return int_value(item); }
    case ARRAY_S32: { int32_t item; memcpy(&item, array->data + index * 4, 4); return int_value(item); }
    case ARRAY_U32: { uint32_t item; memcpy(&item, array->data + index * 4, 4); return int_value(item); }
    case ARRAY_S64: { int64_t item; memcpy(&item, array->data + index * 8, 8); return int_value(item); }
    case ARRAY_U64: { uint64_t item; memcpy(&item, array->data + index * 8, 8); return uint_value(item); }
    case ARRAY_F32: { float item; memcpy(&item, array->data + index * 4, 4); return real_value(item); }
    case ARRAY_F64: { double item; memcpy(&item, array->data + index * 8, 8); return real_value(item); }
    default: return (Value){0};
    }
}

static inline void
array_set(Array *array, size_t index, Value value)
{
    if(array->storage == ARRAY_BOXED) {
        ((Value *)array->data)[index] = value;
        return;
    }
    unsigned char *valid = &array->data[array->data_bytes + index / 8];
    unsigned char bit = (unsigned char)(1u << (index % 8));
    if(value.kind == VALUE_INVALID) {
        *valid &= (unsigned char)~bit;
        return;
    }
    *valid |= bit;
    /* Storage boundaries already coerce to the declared element type. */
    uint64_t bits = value.kind == VALUE_INT || value.kind == VALUE_ENUM ?
        (value.unsigned64 ? value.bits : (uint64_t)value.integer) : 0;
    switch(array->storage) {
    case ARRAY_S8: array->data[index] = (uint8_t)bits; break;
    case ARRAY_U8: array->data[index] = (uint8_t)bits; break;
    case ARRAY_S16: case ARRAY_U16: {
        uint16_t item = (uint16_t)bits; memcpy(array->data + index * 2, &item, 2); break;
    }
    case ARRAY_S32: case ARRAY_U32: {
        uint32_t item = (uint32_t)bits; memcpy(array->data + index * 4, &item, 4); break;
    }
    case ARRAY_S64: memcpy(array->data + index * 8, &value.integer, 8); break;
    case ARRAY_U64: memcpy(array->data + index * 8, &bits, 8); break;
    case ARRAY_F32: {
        float item = (float)value.real; memcpy(array->data + index * 4, &item, 4); break;
    }
    case ARRAY_F64: memcpy(array->data + index * 8, &value.real, 8); break;
    default: break;
    }
}

static inline int
place_valid(Value place)
{
    return place.kind == VALUE_POINTER &&
        (place.indexed ? place.array != NULL &&
         place.offset < (size_t)place.array->length : place.pointee != NULL);
}

static inline Value *
place_slot(Value place)
{
    return place.indexed ? array_boxed_slot(place.array, place.offset) :
        place.pointee;
}

struct StringLiteral {
    struct StringLiteral *next;
    const ZirExpr *expression;
    size_t length;
    size_t bytes;
    uint64_t allocation, pinned;
    unsigned char data[];
};

typedef struct Local {
    /* Names and types belong to the checked statement or cached signature,
     * which outlive this call. Only the value is mutable local storage. */
    const char *name;
    const char *type;
    Value value;
} Local;

typedef struct GlobalSlot {
    const ZirModule *module;
    const ZirGlobal *declaration;
    const char *type;
    Value value;
} GlobalSlot;

/* Verified parameter names and types use the same immutable interned text
 * as checked expressions and local declarations. */
typedef struct VmParameter {
    const char *name;
    const char *type;
} VmParameter;

/* A function's parameters, or count -1 outside the portable subset. */
typedef struct VmSignature {
    const ZirModule *module;
    const ZirFunction *function;
    const char *extern_args; /* temporary foreign descriptors use kept text */
    int count;
    int local_bound;
    uint64_t read_only;
    VmParameter parameters[];
} VmSignature;

typedef struct VmVecType {
    const ZirModule *module;
    const char *type;
    int depth, contains;
} VmVecType;

typedef struct VmCallSite {
    const ZirExpr *expression;
    const ZirModule *module, *owner;
    const ZirFunction *callee;
    const ZirImport *external;
} VmCallSite;

typedef struct VmGlobalSite {
    const ZirExpr *expression;
    const ZirModule *module;
    GlobalSlot *slot;
} VmGlobalSite;

typedef struct VmConstantSite {
    const ZirExpr *expression;
    const ZirModule *module;
    Value value;
} VmConstantSite;

typedef struct VmTypeSite {
    const ZirModule *module, *owner;
    const ZirType *resolved;
    const char *name;
    const char *vec_element; /* NULL: unchecked; "": not an owned Vec */
} VmTypeSite;

typedef struct VmProfile VmProfile;

typedef struct Vm {
    const ZirProgram *program;
    VmProfile *profile; /* opt-in function costs; no values or arguments */
    /* Parameters and immutable call setup facts, once per function. */
    const VmSignature **signatures;
    size_t signature_count, signature_slots;
    int depth;
    /* Statements run so far, counted only when max_steps bounds the run
     * (the web playground); zero means unbounded, as on native targets. */
    int steps;
    int max_steps;
    int failed;
    /* Calls may nest until the C stack reaches stack_floor, where the
     * thread's stack bounds are known; elsewhere VM_MAX_DEPTH calls.
     * depth_exceeded holds the depth where a run stopped for that. */
    const char *stack_floor;
    int depth_exceeded;
    size_t record_bytes;
    size_t array_bytes;
    size_t string_bytes;
    size_t allocated_since_collection;
    size_t collection_threshold;
    uint64_t allocation;
    uint64_t pin_generation;
    Record *records;
    Array *arrays;
    Record *retired_records;
    Array *retired_arrays;
    StringLiteral *strings;
    GlobalSlot *globals;
    int global_count;
    VmVecType *vec_types;
    VmCallSite *call_sites;
    VmGlobalSite *global_sites;
    VmTypeSite *type_sites;
    VmConstantSite *constant_sites;
    StringLiteral **literal_sites;
    Frame *active_frame;
    VmRoots *evaluation_roots;
    VmHostCall host;
    void *host_context;
    uint64_t call_serial;
    VmLayout *layouts;  /* open-addressed by type pointer */
    size_t layout_slots;
    size_t layout_count;
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
    uint64_t serial; /* identifies this call to pointers at its locals */
    /* Sized for the whole call before it starts (function_local_bound):
     * pointers at locals hold their address, so the array never moves. */
    Local *locals;
    int local_capacity;
    int local_count;
    int returned_local;
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
int portable_function_value(const ZirModule *module, const char *type,
                            const ZirModule *value_module, const char *name,
                            const ZirModule **owner,
                            const ZirFunction **function);
int same_bound_type(const ZirModule *caller, const ZirModule *provider, const char *type);
int same_record_type(const ZirModule *owner, const ZirType *type, const Record *record);
int array_element_matches(const ZirModule *module, const char *element, const Array *array);
ValueKind value_kind(const char *type);
int scalar_type(const char *type);
int portable_union(const ZirModule *module, const ZirType *record);
int portable_type(const ZirModule *module, const char *type);
int host_type_at(const ZirModule *module, const char *type, int depth, int slice_parameter);
Value keep_string(Vm *vm, StringLiteral *item, const ZirExpr *expression, size_t bytes);
Value literal_string(Vm *vm, const ZirExpr *expression);
Value global_literal_string(Vm *vm, const char *source);
Value enum_value(const ZirType *type, int64_t integer);
uint64_t integer_bits(Value value);
int64_t signed64(uint64_t bits);
double as_real(Value value);
int truthy(Value value);
Record *allocate_record(Vm *vm, const ZirModule *owner, const ZirType *type, int count);
int array_length_fits(const char *element, size_t length);
Array *allocate_array_try(Vm *vm, const ZirModule *owner, const char *element, int length, int fail_hard);
Value default_value(Vm *vm, const ZirModule *module, const char *type, int depth);
Value coerce(Vm *vm, const ZirModule *module, Value value, const char *type);
const VmSignature *vm_signature(Vm *vm, const ZirModule *module,
                               const ZirFunction *function);
Value coerce_expression(Vm *vm, const ZirModule *module, Value value, const char *type);
void retire_value(Vm *vm, Value value, int depth);
void release_record(Vm *vm, Record *record);
void release_array(Vm *vm, Array *array);
int array_has_active_slice(Vm *vm, const Array *array);
void release_retired(Vm *vm);
int vm_type_contains_vec(Vm *vm, const ZirModule *module, const char *type, int depth);
int vm_vec_element_type(Vm *vm, const ZirModule *module, const char *type,
                        char *element, size_t element_size);
void drop_owned_locals(Frame *frame, int first);
int parse_parameters(const ZirModule *module, const ZirFunction *function, Parameter *parameters);
const VmSignature *vm_signature(Vm *vm, const ZirModule *module,
                                const ZirFunction *function);
void free_signatures(Vm *vm);
int function_local_bound(const ZirFunction *function, int parameters);
int bitwise_operator(const char *op);
const char *assignment_binary_operator(const char *op);
const ZirFunction *find_entry(const ZirProgram *program, const char *module_name, const char *function_name, const ZirModule **module_out);
int is_else_branch(const ZirStmt *statement);
int statement_close(const ZirFunction *function, int begin, int end);
Value *record_field(Record *record, const char *name);
size_t vm_field_hash(const char *name);
const ZirType *vm_find_type(Vm *vm, const ZirModule *module,
                            const char *name, const ZirModule **owner);
Value union_member_read(Vm *vm, Record *record, const char *field_type);
int union_member_write(Vm *vm, Record *record, const char *field_type, Value value);
Value *assignment_slot(Frame *frame, int index, int depth);
Value assignment_slot_root(Frame *frame, int index, int depth);
/* Validate a pointer's location and call lifetime, failing the VM if invalid. */
int pointer_target(Vm *vm, Value pointer);
Value place_read(Vm *vm, Value place);
void place_write(Vm *vm, Value place, Value value);
Value binary_value(Vm *vm, const char *op, Value left, Value right, const char *left_type, const char *right_type);
Value eval(Frame *frame, int index, int depth);
void pin_value(Vm *vm, Value value, int depth);
void pin_evaluation_roots(Vm *vm);
Value run_function(Vm *vm, const ZirModule *module, const ZirFunction *function, const Value *args, int arg_count);
const VmLayout *record_layout(Vm *vm, const ZirType *type);
void free_records(Vm *vm);
void free_layouts(Vm *vm);
void free_arrays(Vm *vm);
void free_strings(Vm *vm);
int fold_global_element(Vm *vm, const ZirModule *module, const ZirFunction *probe, int index, Value *target, const char *type, ZirSourceSpan span);
#pragma GCC visibility pop

#endif
