#include "zir_asm_lower.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"
#include "zir_parse.h"
#include "zir_scalar.h"
#include "zir_text.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct AssemblyAllocation {
    struct AssemblyAllocation *next;
    max_align_t alignment;
} AssemblyAllocation;

typedef struct AssemblyVector {
    void *data;
    int count;
    int capacity;
    size_t width;
} AssemblyVector;

typedef struct AssemblyBinding {
    const char *name;
    int type;
    int value;
    int storage_type;
} AssemblyBinding;

typedef struct AssemblyLocalPlace {
    int binding;
    int type;
    int value;
    int member;
    int index;
    int storage;
    struct AssemblyLocalPlace *parent;
} AssemblyLocalPlace;

typedef struct AssemblyType {
    const ZirModule *module;
    const char *name;
    const ZirType *record;
    int identity;
} AssemblyType;

typedef struct AssemblyGlobal {
    const ZirModule *module;
    const ZirGlobal *source;
    LirGlobal result;
} AssemblyGlobal;

typedef struct AssemblyBlock {
    AssemblyVector parameters;
    AssemblyVector instructions;
    int effect;
} AssemblyBlock;

/* A frame slot of one type that the procedure allocates on entry and never
 * writes: the source of that type's long zero values. */
typedef struct AssemblyZeroSlot {
    int type;
    int address;
} AssemblyZeroSlot;

typedef struct AssemblyLoop {
    int header;
    int exit;
    int bindings;
    int identity;
    struct AssemblyLoop *parent;
} AssemblyLoop;

typedef struct AssemblyFunction {
    const ZirModule *module;
    const ZirFunction *source;
    const ZirImport *import;
    LirFunction result;
    AssemblyVector blocks;
    AssemblyVector values;
    AssemblyVector bindings;
    unsigned char *addressed;
    int addressed_count;
    int current;
    AssemblyLoop *loop;
    /* A constant global initializer: one more than the global it stores
     * into, and the root of its parsed initializer expression. */
    int initializes;
    int initializer_root;
    /* One more than the expression whose untyped numeric literal takes
     * `literal_type`: a member or global the literal initializes. */
    int literal_at;
    int literal_type;
    AssemblyVector zero_slots;
} AssemblyFunction;

struct AssemblyProgram {
    LirProgram result;
    const ZirProgram *source;
    AssemblyAllocation *allocations;
    AssemblyVector modules;
    AssemblyVector sources;
    AssemblyVector constants;
    AssemblyVector globals;
    AssemblyVector origins;
    AssemblyVector functions;
    AssemblyVector initializers;
    /* Initializers of constant aggregate, float and string globals. They
     * run before every module startup procedure. */
    AssemblyVector constant_initializers;
    /* Parsed initializer expressions, freed with the program. */
    AssemblyVector probes;
    AssemblyVector types;
    AssemblyVector type_names;
    size_t bytes;
    int64_t work;
    int failed;
    int string_type;
};

static int
assembly_fail(AssemblyProgram *program, ZirSourceSpan span, const char *message)
{
    if(!program->failed)
        DiagnosticTarget(span, "assembly.lower", "asm", "lowered-scalar", "%s", message);
    program->failed = 1;
    return -1;
}

static int
assembly_spend(AssemblyProgram *program, ZirSourceSpan span)
{
    if(program->failed) return 0;
    if(program->work-- > 0) return 1;
    assembly_fail(program, span, "lowering work budget exhausted");
    return 0;
}

static void *
assembly_allocate(AssemblyProgram *program, size_t count, size_t width)
{
    if(program->failed || width == 0 || program->bytes > 268435456 - sizeof(AssemblyAllocation) ||
       count > (268435456 - sizeof(AssemblyAllocation) - program->bytes) / width) {
        assembly_fail(program, (ZirSourceSpan){0}, "lowered program allocation budget exhausted");
        return NULL;
    }
    size_t size = count * width;
    if(size == 0) return NULL;
    AssemblyAllocation *allocation = calloc(1, sizeof(*allocation) + size);
    if(allocation == NULL) {
        assembly_fail(program, (ZirSourceSpan){0}, "cannot allocate lowered program");
        return NULL;
    }
    allocation->next = program->allocations;
    program->allocations = allocation;
    program->bytes += sizeof(*allocation) + size;
    return allocation + 1;
}

static void *
assembly_push(AssemblyProgram *program, AssemblyVector *vector, const void *value)
{
    if(vector->count == INT_MAX || vector->width == 0 || program->failed) return NULL;
    if(vector->count == vector->capacity) {
        int capacity = vector->capacity < 8 ? 8 :
            vector->capacity > INT_MAX / 2 ? INT_MAX : vector->capacity * 2;
        void *data = assembly_allocate(program, (size_t)capacity, vector->width);
        if(data == NULL) return NULL;
        if(vector->count) memcpy(data, vector->data, (size_t)vector->count * vector->width);
        vector->data = data;
        vector->capacity = capacity;
    }
    void *destination = (char *)vector->data + (size_t)vector->count * vector->width;
    if(value) memcpy(destination, value, vector->width);
    vector->count++;
    return destination;
}

static Slice
assembly_slice(AssemblyVector vector)
{
    return (Slice){vector.data, vector.count};
}

static const char *
assembly_text(AssemblyProgram *program, const char *text)
{
    if(text == NULL) text = "";
    size_t length = strlen(text);
    char *copy = assembly_allocate(program, length + 1, 1);
    if(copy) memcpy(copy, text, length + 1);
    return copy ? copy : "";
}

static int
assembly_name(AssemblyProgram *program, AssemblyVector *names, const char *name)
{
    String *values = names->data;
    for(int index = 0; index < names->count; index++)
        if(strlen(name) == values[index].length && !memcmp(name, values[index].data, values[index].length))
            return index;
    const char *copy = assembly_text(program, name);
    String value = {copy, strlen(copy)};
    int index = names->count;
    return assembly_push(program, names, &value) ? index : -1;
}

static int
assembly_origin(AssemblyProgram *program, ZirSourceSpan span)
{
    int file = assembly_name(program, &program->sources, SpanPath(span));
    int line = span.line > 0 ? span.line : 1;
    int column = span.column > 0 ? span.column : 1;
    int has_end = span.end_line >= line && span.end_column > 0 &&
        (span.end_line != line || span.end_column >= column);
    int end_line = has_end ? span.end_line : 0;
    int end_column = has_end ? span.end_column : 0;
    LirOrigin origin = {.file = file, .line = line,
        .column = column,
        .end_line = end_line, .end_column = end_column,
        .begin = -1, .end = -1, .parent = -1};
    int index = program->origins.count;
    return assembly_push(program, &program->origins, &origin) ? index : -1;
}

static LirType *
assembly_type_at(AssemblyProgram *program, int identity)
{
    return &((LirType *)program->types.data)[identity];
}

static int
assembly_pointer_type(AssemblyProgram *program, int element, ZirSourceSpan span)
{
    if(program->failed || element < 0) return -1;
    for(int index = 0; index < program->types.count; index++) {
        if(!assembly_spend(program, span)) return -1;
        LirType type = *assembly_type_at(program, index);
        if(type.kind == LirTypeKind_Pointer && type.element == element) return index;
    }
    LirType type = {.kind = LirTypeKind_Pointer, .element = element};
    int identity = program->types.count;
    return assembly_push(program, &program->types, &type) ? identity : -1;
}

/* The element type of an owned Vec(T) record, or -1 for any other type. */
static int
assembly_vec_element(AssemblyProgram *program, int identity)
{
    AssemblyType *names = program->type_names.data;
    for(int index = 0; index < program->type_names.count; index++) {
        if(names[index].identity != identity || names[index].record == NULL ||
           !names[index].record->is_owned_vec) continue;
        LirType vector = *assembly_type_at(program, identity);
        if(vector.kind != LirTypeKind_Record || vector.fields.length != 3) return -1;
        LirType pointer = *assembly_type_at(program, ((int *)vector.fields.data)[0]);
        return pointer.kind == LirTypeKind_Pointer ? pointer.element : -1;
    }
    return -1;
}

/* Whether values of a type own heap storage: a vector, or a record or
 * fixed array that holds one. Pointers and views own nothing. */
static int
assembly_owned(AssemblyProgram *program, int identity, int depth)
{
    if(depth > 32 || identity <= 0 || identity >= program->types.count) return 0;
    if(assembly_vec_element(program, identity) >= 0) return 1;
    LirType type = *assembly_type_at(program, identity);
    if(type.kind == LirTypeKind_Array)
        return type.count > 0 && assembly_owned(program, type.element, depth + 1);
    if(type.kind != LirTypeKind_Record) return 0;
    for(int64_t field = 0; field < type.fields.length; field++)
        if(assembly_owned(program, ((int *)type.fields.data)[field], depth + 1)) return 1;
    return 0;
}

/* `string` is the record {data: *u8, count: s64}. Literal bytes live in
 * read-only byte-array globals; an empty literal has a null data pointer. */
static int
assembly_string_type(AssemblyProgram *program, ZirSourceSpan span)
{
    if(program->string_type > 0) return program->string_type;
    int pointer = assembly_pointer_type(program, 3, span);
    int *fields = assembly_allocate(program, 2, sizeof(*fields));
    if(program->failed || fields == NULL) return -1;
    fields[0] = pointer;
    fields[1] = 8;
    LirType type = {.kind = LirTypeKind_Record, .fields = {fields, 2}};
    int identity = program->types.count;
    if(!assembly_push(program, &program->types, &type)) return -1;
    program->string_type = identity;
    return identity;
}

static int
assembly_type_in(AssemblyProgram *program, const ZirModule *module,
                 const char *name, ZirSourceSpan span, int depth)
{
    if(!assembly_spend(program, span)) return -1;
    if(depth > 256) return assembly_fail(program, span, "lowered type nesting budget exhausted");
    if(name == NULL || !*name || !strcmp(name, "void")) return 0;
    if(!strcmp(name, "bool")) return 1;
    unsigned width = ScalarWidth(name);
    if(width) {
        int index = width == 8 ? 0 : width == 16 ? 1 : width == 32 ? 2 : 3;
        return 2 + index * 2 + (name[0] == 'u');
    }
    AssemblyType *names = program->type_names.data;
    for(int index = 0; index < program->type_names.count; index++)
        if(names[index].module == module && !strcmp(names[index].name, name)) return names[index].identity;
    const ZirModule *owner = module;
    const ZirType *record = FindType(module, name, &owner);
    if(record && record->is_enum)
        return assembly_type_in(program, owner, record->enum_backing, span, depth + 1);
    if(record)
        for(int index = 0; index < program->type_names.count; index++)
            if(names[index].record == record) return names[index].identity;
    if(record && record->is_union) {
        /* A union of integers and floats is one unsigned integer as wide as
         * its widest member. A member is read and written by reinterpreting
         * its low bytes, which is what overlapping storage means on a
         * little-endian target. */
        size_t cursor = 0;
        ZirTypeField field;
        int status, width = 0;
        while((status = TypeNextField(record, &cursor, &field)) == 1) {
            int member = assembly_type_in(program, owner, field.type, span, depth + 1);
            if(program->failed) return -1;
            LirType shape = *assembly_type_at(program, member);
            if(shape.kind != LirTypeKind_Integer && shape.kind != LirTypeKind_Float)
                return assembly_fail(program, span, "this union has a member that is not an integer or a float");
            if(shape.bits > width) width = shape.bits;
        }
        if(status < 0 || !width) return assembly_fail(program, span, "invalid checked union members");
        int identity = width == 8 ? 3 : width == 16 ? 5 : width == 32 ? 7 : 9;
        AssemblyType binding = {module, assembly_text(program, name), NULL, identity};
        return assembly_push(program, &program->type_names, &binding) ? identity : -1;
    }
    if(name[0] == '*') {
        int element = assembly_type_in(program, module, name + 1, span, depth + 1);
        int identity = assembly_pointer_type(program, element, span);
        if(program->failed) return -1;
        AssemblyType binding = {module, assembly_text(program, name), NULL, identity};
        return assembly_push(program, &program->type_names, &binding) ? identity : -1;
    }
    if(!strcmp(name, "string")) return assembly_string_type(program, span);
    char slice_element[ZIR_NAME_MAX];
    if(SliceElementType(name, slice_element, sizeof(slice_element))) {
        /* Views are structural: every spelling of []T (an alias, a generic
         * parameter, another module's name) is the same {data: *T, count}. */
        int element = assembly_type_in(program, module, slice_element, span, depth + 1);
        int pointer = assembly_pointer_type(program, element, span);
        if(program->failed) return -1;
        names = program->type_names.data;
        for(int index = 0; index < program->type_names.count; index++) {
            if(!SliceElementType(names[index].name, NULL, 0)) continue;
            LirType known = *assembly_type_at(program, names[index].identity);
            if(known.kind == LirTypeKind_Record && known.fields.length == 2 &&
               ((int *)known.fields.data)[0] == pointer) {
                AssemblyType alias = {module, assembly_text(program, name), NULL, names[index].identity};
                return assembly_push(program, &program->type_names, &alias) ? alias.identity : -1;
            }
        }
        int *fields = assembly_allocate(program, 2, sizeof(*fields));
        if(program->failed || fields == NULL) return -1;
        fields[0] = pointer;
        fields[1] = 8;
        LirType view = {.kind = LirTypeKind_Record, .fields = {fields, 2}};
        int identity = program->types.count;
        AssemblyType binding = {module, assembly_text(program, name), NULL, identity};
        if(!assembly_push(program, &program->types, &view) ||
           !assembly_push(program, &program->type_names, &binding)) return -1;
        return identity;
    }
    LirType type = {0};
    if(!strcmp(name, "float32") || !strcmp(name, "float64") || !strcmp(name, "real")) {
        /* One float type per width for the whole program: a float passed
         * between modules is the same type on both sides. */
        int bits = !strcmp(name, "float32") ? 32 : 64;
        for(int index = 0; index < program->types.count; index++) {
            LirType known = *assembly_type_at(program, index);
            if(known.kind == LirTypeKind_Float && known.bits == bits) return index;
        }
        type.kind = LirTypeKind_Float;
        type.bits = bits;
    } else if(ArrayValueType(name)) {
        /* Fixed arrays are structural like views: every spelling of [N]T,
         * whichever module wrote it, is one array of that element and count. */
        char element[ZIR_NAME_MAX];
        int count = 0;
        if(!ArrayElementType(name, element, sizeof(element), &count) || count < 0)
            return assembly_fail(program, span, "invalid checked fixed-array type");
        int element_type = assembly_type_in(program, module, element, span, depth + 1);
        if(program->failed) return -1;
        int identity = -1;
        for(int index = 0; index < program->types.count && identity < 0; index++) {
            LirType known = *assembly_type_at(program, index);
            if(known.kind == LirTypeKind_Array && known.element == element_type && known.count == count)
                identity = index;
        }
        if(identity < 0) {
            type.kind = LirTypeKind_Array;
            type.element = element_type;
            type.count = count;
            identity = program->types.count;
            if(!assembly_push(program, &program->types, &type)) return -1;
        }
        AssemblyType binding = {module, assembly_text(program, name), record, identity};
        return assembly_push(program, &program->type_names, &binding) ? identity : -1;
    }
    else if(record && record->is_procedure_type && !record->is_c_call)
        type.kind = LirTypeKind_Function;
    else if(record && !record->is_procedure_type && !record->is_union && !record->is_record_template &&
            !record->is_map && !record->is_extern && !record->foreign_target[0])
        type.kind = LirTypeKind_Record;
    else return assembly_fail(program, span, "this type has no portable lowered representation yet");
    if(record && record->is_owned_vec) {
        /* Vectors are structural like views: every specialization of Vec(T),
         * whichever module spelled it, is one {data: *T, count, capacity}. */
        size_t cursor = 0;
        ZirTypeField data_field;
        if(TypeNextField(record, &cursor, &data_field) != 1)
            return assembly_fail(program, span, "owned vector has an unexpected record shape");
        int data = assembly_type_in(program, owner, data_field.type, span, depth + 1);
        if(program->failed) return -1;
        names = program->type_names.data;
        for(int index = 0; index < program->type_names.count; index++) {
            if(names[index].record == NULL || !names[index].record->is_owned_vec) continue;
            LirType known = *assembly_type_at(program, names[index].identity);
            if(known.kind == LirTypeKind_Record && known.fields.length == 3 &&
               ((int *)known.fields.data)[0] == data) {
                AssemblyType alias = {module, assembly_text(program, name), record, names[index].identity};
                return assembly_push(program, &program->type_names, &alias) ? alias.identity : -1;
            }
        }
    }
    int identity = program->types.count;
    AssemblyType binding = {module, assembly_text(program, name), record, identity};
    if(!assembly_push(program, &program->types, &type) ||
       !assembly_push(program, &program->type_names, &binding)) return -1;
    if(type.kind == LirTypeKind_Function) {
        const ZirParameters *parameters = ParametersOf(record->body);
        int *types = assembly_allocate(program, (size_t)parameters->count, sizeof(*types));
        if(parameters->count && types == NULL) return -1;
        for(int index = 0; index < parameters->count && !program->failed; index++)
            types[index] = assembly_type_in(program, owner, parameters->items[index].value_type, span, depth + 1);
        int returns = assembly_type_in(program, owner, record->procedure_return_type, span, depth + 1);
        assembly_type_at(program, identity)->parameters = (Slice){types, parameters->count};
        assembly_type_at(program, identity)->result = returns;
    } else if(type.kind == LirTypeKind_Record) {
        AssemblyVector fields = {.width = sizeof(int)};
        size_t cursor = 0;
        ZirTypeField field;
        int status;
        while((status = TypeNextField(record, &cursor, &field)) == 1 && !program->failed) {
            int field_type = assembly_type_in(program, owner, field.type, span, depth + 1);
            assembly_push(program, &fields, &field_type);
        }
        if(status < 0) return assembly_fail(program, span, "invalid checked record fields");
        assembly_type_at(program, identity)->fields = assembly_slice(fields);
        /* Vec(T) is {data: *T, count: s64, capacity: s64}. */
        if(record->is_owned_vec) {
            int *members = fields.data;
            if(fields.count != 3 || assembly_type_at(program, members[0])->kind != LirTypeKind_Pointer ||
               members[1] != 8 || members[2] != 8)
                return assembly_fail(program, span, "owned vector has an unexpected record shape");
        }
    }
    return program->failed ? -1 : identity;
}

static int
assembly_type(AssemblyProgram *program, AssemblyFunction *function, const char *name, ZirSourceSpan span)
{
    return assembly_type_in(program, function->module, name, span, 0);
}

static int
assembly_view_element(AssemblyProgram *program, int identity)
{
    if(identity == program->string_type && identity > 0) return 3;
    AssemblyType *names = program->type_names.data;
    for(int index = 0; index < program->type_names.count; index++) {
        if(names[index].identity != identity ||
           !SliceElementType(names[index].name, NULL, 0)) continue;
        LirType view = *assembly_type_at(program, identity);
        if(view.kind != LirTypeKind_Record || view.fields.length != 2) return -1;
        LirType pointer = *assembly_type_at(program, ((int *)view.fields.data)[0]);
        return pointer.kind == LirTypeKind_Pointer ? pointer.element : -1;
    }
    return -1;
}

/* Strings, slices and owned vectors index and slice alike: member 0 is
 * the data pointer and member 1 the element count. */
static int
assembly_indexable_element(AssemblyProgram *program, int identity)
{
    int element = assembly_view_element(program, identity);
    return element >= 0 ? element : assembly_vec_element(program, identity);
}

/* Whether a checked source type name is an owned Vec(T). */
static int
assembly_source_vec(const AssemblyFunction *function, const char *name)
{
    const ZirModule *owner = function->module;
    const ZirType *record = name && *name ? FindType(function->module, name, &owner) : NULL;
    return record != NULL && record->is_owned_vec;
}

static int
assembly_value(AssemblyProgram *program, AssemblyFunction *function, int type)
{
    int value = function->values.count;
    return assembly_push(program, &function->values, &type) ? value : -1;
}

static AssemblyBlock *
assembly_block_at(AssemblyFunction *function, int index)
{
    return &((AssemblyBlock *)function->blocks.data)[index];
}

static int
assembly_block(AssemblyProgram *program, AssemblyFunction *function,
               const AssemblyBinding *bindings, int count)
{
    AssemblyBlock block = {.parameters.width = sizeof(int), .instructions.width = sizeof(LirInstruction)};
    for(int index = 0; index < count && !program->failed; index++) {
        int type = bindings[index].storage_type >= 0 ? bindings[index].storage_type : bindings[index].type;
        int value = assembly_value(program, function, type);
        assembly_push(program, &block.parameters, &value);
    }
    int index = function->blocks.count;
    return assembly_push(program, &function->blocks, &block) ? index : -1;
}

static void
assembly_enter(AssemblyFunction *function, int block, const AssemblyBinding *bindings, int count)
{
    function->current = block;
    if(count) memcpy(function->bindings.data, bindings, (size_t)count * sizeof(*bindings));
    function->bindings.count = count;
    AssemblyBinding *active = function->bindings.data;
    int *parameters = assembly_block_at(function, block)->parameters.data;
    for(int index = 0; index < count; index++) active[index].value = parameters[index];
}

static AssemblyBinding *
assembly_snapshot(AssemblyProgram *program, AssemblyFunction *function, int count)
{
    AssemblyBinding *copy = assembly_allocate(program, (size_t)count, sizeof(*copy));
    if(copy) memcpy(copy, function->bindings.data, (size_t)count * sizeof(*copy));
    return copy;
}

static int
assembly_emit(AssemblyProgram *program, AssemblyFunction *function, LirOp op,
              int type, const int *operands, int count, int symbol, ZirSourceSpan span)
{
    if(program->failed || function->current < 0) return -1;
    int *copy = assembly_allocate(program, (size_t)count, sizeof(*copy));
    if(count && copy == NULL) return -1;
    if(count) memcpy(copy, operands, (size_t)count * sizeof(*copy));
    LirInstruction instruction = {.op = op, .result = type ? assembly_value(program, function, type) : -1,
        .operands = {copy, count}, .symbol = symbol, .origin = assembly_origin(program, span)};
    if(op == LirOp_Fuel) instruction.immediate = 1;
    AssemblyBlock *block = assembly_block_at(function, function->current);
    if(compiler_lowered_ir_LirEffectful(op)) instruction.effect = ++block->effect;
    return assembly_push(program, &block->instructions, &instruction) ? instruction.result : -1;
}

static LirEdge
assembly_edge(AssemblyProgram *program, int target, const AssemblyBinding *bindings, int count)
{
    int *arguments = assembly_allocate(program, (size_t)count, sizeof(*arguments));
    if(arguments) for(int index = 0; index < count; index++) arguments[index] = bindings[index].value;
    return (LirEdge){target, {arguments, count}};
}

static void
assembly_jump(AssemblyProgram *program, AssemblyFunction *function, int target, int bindings, ZirSourceSpan span)
{
    assembly_emit(program, function, LirOp_Jump, 0, NULL, 0, -1, span);
    if(program->failed) return;
    AssemblyBlock *block = assembly_block_at(function, function->current);
    LirEdge *edge = assembly_allocate(program, 1, sizeof(*edge));
    if(edge == NULL) return;
    *edge = assembly_edge(program, target, function->bindings.data, bindings);
    LirInstruction *instructions = block->instructions.data;
    instructions[block->instructions.count - 1].edges = (Slice){edge, 1};
    function->current = -1;
}

static void
assembly_branch(AssemblyProgram *program, AssemblyFunction *function, int condition,
                int yes, int no, int bindings, ZirSourceSpan span)
{
    if(condition >= 0 && !program->failed && ((int *)function->values.data)[condition] != 1)
        condition = assembly_emit(program, function, LirOp_Convert, 1, &condition, 1, -1, span);
    assembly_emit(program, function, LirOp_Branch, 0, &condition, 1, -1, span);
    if(program->failed) return;
    AssemblyBlock *block = assembly_block_at(function, function->current);
    LirEdge *edges = assembly_allocate(program, 2, sizeof(*edges));
    if(edges == NULL) return;
    edges[0] = assembly_edge(program, yes, function->bindings.data, bindings);
    edges[1] = assembly_edge(program, no, function->bindings.data, bindings);
    LirInstruction *instructions = block->instructions.data;
    instructions[block->instructions.count - 1].edges = (Slice){edges, 2};
    function->current = -1;
}

static int
assembly_constant(AssemblyProgram *program, AssemblyFunction *function, int type,
                  uint64_t bits, ZirSourceSpan span)
{
    if(program->failed || type <= 0 || type >= program->types.count) return -1;
    int width = assembly_type_at(program, type)->bits;
    if(width > 0 && width < 64) bits &= (UINT64_C(1) << width) - 1;
    LirConstant constant = {.type = type, .bits = bits};
    int symbol = program->constants.count;
    assembly_push(program, &program->constants, &constant);
    return assembly_emit(program, function, LirOp_Constant, type, NULL, 0, symbol, span);
}

static int
assembly_constant_initializer(AssemblyProgram *program, const ZirModule *owner, const ZirGlobal *source, int symbol)
{
    ZirFunction *probe = assembly_allocate(program, 1, sizeof(*probe));
    AssemblyFunction *function = assembly_allocate(program, 1, sizeof(*function));
    if(probe == NULL || function == NULL) return 0;
    int root = ParseExprTyped(probe, owner, source->init, source->span, source->type);
    if(!assembly_push(program, &program->probes, &probe)) return 0;
    if(root < 0)
        return assembly_fail(program, source->span, "global initializer is not a lowered constant expression") >= 0;
    probe->span = source->span;
    function->module = owner;
    function->source = probe;
    function->blocks.width = sizeof(AssemblyBlock);
    function->values.width = sizeof(int);
    function->bindings.width = sizeof(AssemblyBinding);
    function->current = -1;
    function->initializes = symbol + 1;
    function->initializer_root = root;
    function->result.module = assembly_name(program, &program->modules, owner->name);
    size_t length = strlen(source->name) + sizeof(".initializer");
    char *name = assembly_allocate(program, length, 1);
    if(name == NULL) return 0;
    snprintf(name, length, "%s.initializer", source->name);
    function->result.name = (String){name, strlen(name)};
    function->result.origin = assembly_origin(program, source->span);
    int identity = program->functions.count;
    return assembly_push(program, &program->functions, &function) &&
        assembly_push(program, &program->constant_initializers, &identity);
}

static int
assembly_global_address(AssemblyProgram *program, AssemblyFunction *function,
                        const char *name, ZirSourceSpan span)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *source = NULL;
    if(ResolveGlobalAt(function->module, name, SpanPath(span), &owner, &source) != 1)
        return assembly_fail(program, span, "global reference has no unique checked declaration");
    int symbol = -1;
    AssemblyGlobal *globals = program->globals.data;
    for(int index = 0; index < program->globals.count; index++) {
        if(!assembly_spend(program, span)) return -1;
        if(globals[index].module == owner && globals[index].source == source) {
            symbol = index;
            break;
        }
    }
    if(symbol < 0) {
        int type = assembly_type_in(program, owner, source->type, source->span, 0);
        if(program->failed) return -1;
        int constant = -1;
        int parsed = 0;
        if(source->init && *source->init) {
            LirType value_type = *assembly_type_at(program, type);
            unsigned long long literal = 0;
            int negative = 0;
            int64_t evaluated = 0;
            uint64_t bits = 0;
            int folded = 0;
            if(value_type.kind == LirTypeKind_Integer || value_type.kind == LirTypeKind_Bool) {
                if(ZirSignedIntegerLiteral(source->init, &literal, &negative)) { bits = (uint64_t)literal; folded = 1; }
                else if(EvaluateCompileExpression(owner, source->init, source->span, 0, &evaluated)) {
                    bits = (uint64_t)evaluated;
                    folded = 1;
                }
            }
            if(folded) {
                if(value_type.bits > 0 && value_type.bits < 64)
                    bits &= (UINT64_C(1) << value_type.bits) - 1;
                LirConstant initial = {.type = type, .bits = bits};
                constant = program->constants.count;
                if(!assembly_push(program, &program->constants, &initial)) return -1;
            } else parsed = 1;
        }
        const char *copied_name = assembly_text(program, source->name);
        if(program->failed) return -1;
        AssemblyGlobal global = {.module = owner, .source = source,
            .result = {.module = assembly_name(program, &program->modules, owner->name),
                .name = {copied_name, strlen(copied_name)}, .type = type,
                .constant = constant, .writable = 1}};
        symbol = program->globals.count;
        if(!assembly_push(program, &program->globals, &global)) return -1;
        /* A record, array, float or string initializer is parsed as the
         * checked expression it is and stored by a private initializer. */
        if(parsed && !assembly_constant_initializer(program, owner, source, symbol)) return -1;
    }
    int element = ((AssemblyGlobal *)program->globals.data)[symbol].result.type;
    int pointer_type = assembly_pointer_type(program, element, span);
    return assembly_emit(program, function, LirOp_GlobalAddress, pointer_type, NULL, 0, symbol, span);
}

static int
assembly_convert(AssemblyProgram *program, AssemblyFunction *function, int value, int type, ZirSourceSpan span)
{
    if(value < 0 || type < 0 || program->failed) return -1;
    int actual = ((int *)function->values.data)[value];
    if(actual == type) return value;
    return assembly_emit(program, function, LirOp_Convert, type, &value, 1, -1, span);
}

static int
assembly_binding(AssemblyFunction *function, const char *name)
{
    AssemblyBinding *bindings = function->bindings.data;
    for(int index = function->bindings.count; index > 0; index--)
        if(!strcmp(bindings[index - 1].name, name)) return index - 1;
    return -1;
}

static int assembly_function(AssemblyProgram *, const ZirModule *, const ZirFunction *);
static int assembly_host_function(AssemblyProgram *, const ZirModule *, const char *, ZirSourceSpan);
static int assembly_expression(AssemblyProgram *, AssemblyFunction *, int, int);
static int assembly_address(AssemblyProgram *, AssemblyFunction *, int, int);
static AssemblyLocalPlace *assembly_capture_local(AssemblyProgram *, AssemblyFunction *, int, int);
static void assembly_move_out(AssemblyProgram *, AssemblyFunction *, int, int, ZirSourceSpan);
static void assembly_store_captured(AssemblyProgram *, AssemblyFunction *,
                                    const AssemblyLocalPlace *, int, ZirSourceSpan);

enum {
    ASSEMBLY_PRINT_BYTES,
    ASSEMBLY_PRINT_SIGNED,
    ASSEMBLY_PRINT_UNSIGNED,
    ASSEMBLY_PRINT_BOOL,
    ASSEMBLY_PRINT_FLOAT32,
    ASSEMBLY_PRINT_FLOAT64,
    ASSEMBLY_PRINT_SERVICE_COUNT
};

static const char *const assembly_print_services[ASSEMBLY_PRINT_SERVICE_COUNT] = {
    "ziran.platform.write_all",
    "ziran.print.signed",
    "ziran.print.unsigned",
    "ziran.print.bool",
    "ziran.print.float32",
    "ziran.print.float64"
};

static int
assembly_indexed(AssemblyProgram *program, AssemblyFunction *function, LirOp op,
                 int type, const int *operands, int count, uint64_t index, ZirSourceSpan span)
{
    int result = assembly_emit(program, function, op, type, operands, count, -1, span);
    if(!program->failed && function->current >= 0) {
        AssemblyBlock *block = assembly_block_at(function, function->current);
        ((LirInstruction *)block->instructions.data)[block->instructions.count - 1].immediate = index;
    }
    return result;
}

typedef struct AssemblyScopeBinding {
    const char *name;
    int source;
} AssemblyScopeBinding;

/* Resolve address-taking to the declaration the checker saw. A declaration's
 * initializer still sees an outer declaration with the same name, and closing
 * a lexical block restores the previous binding. */
static int
assembly_find_addressed(AssemblyProgram *program, AssemblyFunction *function)
{
    const ZirFunction *source = function->source;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(source));
    int count = source->stmt_count + parameters->count;
    function->addressed = assembly_allocate(program, (size_t)count, 1);
    function->addressed_count = count;
    int *binding_of = assembly_allocate(program, (size_t)source->expr_count, sizeof(*binding_of));
    int *pending = assembly_allocate(program, (size_t)source->expr_count, sizeof(*pending));
    AssemblyScopeBinding *scope = assembly_allocate(program, (size_t)count + 1, sizeof(*scope));
    int *opens = assembly_allocate(program, (size_t)source->stmt_count + 1, sizeof(*opens));
    if(program->failed) return 0;
    for(int index = 0; index < source->expr_count; index++) binding_of[index] = -1;
    int scope_count = 0, open_count = 0;
    for(int index = 0; index < parameters->count; index++) {
        const ZirParameter *parameter = &parameters->items[index];
        const char *name = parameter->binding_name ? parameter->binding_name : parameter->name;
        if(name && *name)
            scope[scope_count++] = (AssemblyScopeBinding){name, source->stmt_count + index};
    }
    ZirExprWalk walk = {.fn = source, .pending = pending};
    for(int statement = 0; statement < source->stmt_count && !program->failed; statement++) {
        const ZirStmt *body = &source->stmts[statement];
        int roots[] = {body->expr_root, body->lhs_root};
        for(int root = 0; root < 2; root++)
            for(int expression = ExprWalkFirst(&walk, roots[root]); expression >= 0;
                expression = ExprWalkNext(&walk)) {
                if(!assembly_spend(program, source->exprs[expression].span)) return 0;
                const ZirExpr *value = &source->exprs[expression];
                if(value->kind != ZIR_EXPR_IDENT || value->is_global_value ||
                   value->is_function_value || value->name == NULL) continue;
                for(int binding = scope_count; binding > 0; binding--)
                    if(!strcmp(scope[binding - 1].name, value->name)) {
                        binding_of[expression] = scope[binding - 1].source;
                        break;
                    }
            }
        if(body->kind == ZIR_STMT_DECL && body->name && *body->name)
            scope[scope_count++] = (AssemblyScopeBinding){body->name, statement};
        if(body->kind == ZIR_STMT_IF || body->kind == ZIR_STMT_WHILE ||
           body->kind == ZIR_STMT_BLOCK_OPEN)
            opens[open_count++] = scope_count;
        if(body->kind == ZIR_STMT_BLOCK_CLOSE) {
            if(open_count == 0)
                return assembly_fail(program, body->span, "unbalanced checked statement blocks") >= 0;
            scope_count = opens[--open_count];
        }
    }
    for(int index = 0; index < source->expr_count && !program->failed; index++) {
        const ZirExpr *expression = &source->exprs[index];
        int target = -1;
        if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "&"))
            target = expression->right;
        else if(expression->kind == ZIR_EXPR_SLICE && expression->left >= 0 &&
                ArrayValueType(source->exprs[expression->left].type))
            target = expression->left;
        else if(expression->kind == ZIR_EXPR_MEMBER && expression->left >= 0 &&
                !strcmp(expression->name, "data") &&
                ArrayValueType(source->exprs[expression->left].type))
            target = expression->left;
        if(target < 0) continue;
        while(target >= 0 && target < source->expr_count &&
              (source->exprs[target].kind == ZIR_EXPR_MEMBER ||
               source->exprs[target].kind == ZIR_EXPR_INDEX))
            target = source->exprs[target].left;
        if(target >= 0 && target < source->expr_count && binding_of[target] >= 0)
            function->addressed[binding_of[target]] = 1;
    }
    return !program->failed;
}

static int
assembly_promote_binding(AssemblyProgram *program, AssemblyFunction *function,
                         int binding_index, ZirSourceSpan span)
{
    AssemblyBinding *binding = &((AssemblyBinding *)function->bindings.data)[binding_index];
    LirType type = *assembly_type_at(program, binding->type);
    if(type.kind == LirTypeKind_Invalid || type.kind == LirTypeKind_Void ||
       type.kind == LirTypeKind_Vector || type.kind == LirTypeKind_Capability)
        return assembly_fail(program, span, "this local type has no portable checked storage representation");
    int pointer_type = assembly_pointer_type(program, binding->type, span);
    if(program->failed) return -1;
    int operands[1] = {binding->value};
    int address = assembly_emit(program, function, LirOp_Allocate, pointer_type,
        operands, 1, -1, span);
    if(program->failed || address < 0) return -1;
    binding->value = address;
    binding->storage_type = pointer_type;
    return address;
}

/* The zero value of a long fixed array is read from a frame slot that the
 * procedure allocates on entry and never writes. Building it from one
 * constant per element would cost an instruction per element: 65,536 of them
 * for a `[65536]u8` buffer. A frame slot rather than a global, because the
 * native encoder lists every element of a global. */
#define ASSEMBLY_ZERO_ARRAY_INLINE 256

static int
assembly_zero_slot(AssemblyProgram *program, AssemblyFunction *function, int identity, ZirSourceSpan span)
{
    if(program->failed || function->current < 0) return -1;
    if(!function->zero_slots.width) function->zero_slots.width = sizeof(AssemblyZeroSlot);
    AssemblyZeroSlot *slots = function->zero_slots.data;
    int address = -1;
    for(int index = 0; index < function->zero_slots.count && address < 0; index++)
        if(slots[index].type == identity) address = slots[index].address;
    if(address < 0) {
        int pointer_type = assembly_pointer_type(program, identity, span);
        if(program->failed) return -1;
        address = assembly_value(program, function, pointer_type);
        LirInstruction allocate = {.op = LirOp_Allocate, .result = address, .symbol = -1,
            .origin = assembly_origin(program, span)};
        AssemblyBlock *entry = assembly_block_at(function, function->result.entry);
        if(program->failed || !assembly_push(program, &entry->instructions, &allocate)) return -1;
        /* The slot is allocated first, after the block's fuel charge, so it
         * dominates every use; the block's effects are numbered again. */
        LirInstruction *instructions = entry->instructions.data;
        int count = entry->instructions.count;
        int at = count > 1 && instructions[0].op == LirOp_Fuel ? 1 : 0;
        memmove(&instructions[at + 1], &instructions[at], (size_t)(count - 1 - at) * sizeof(*instructions));
        instructions[at] = allocate;
        entry->effect = 0;
        for(int index = 0; index < count; index++)
            if(compiler_lowered_ir_LirEffectful(instructions[index].op)) instructions[index].effect = ++entry->effect;
        AssemblyZeroSlot slot = {identity, address};
        if(!assembly_push(program, &function->zero_slots, &slot)) return -1;
    }
    return assembly_emit(program, function, LirOp_Load, identity, &address, 1, -1, span);
}

static int
assembly_zero(AssemblyProgram *program, AssemblyFunction *function, int identity,
              ZirSourceSpan span, int depth)
{
    if(!assembly_spend(program, span)) return -1;
    if(depth > 256) return assembly_fail(program, span, "aggregate initialization nesting budget exhausted");
    LirType type = *assembly_type_at(program, identity);
    if(type.kind == LirTypeKind_Array && type.count > ASSEMBLY_ZERO_ARRAY_INLINE)
        return assembly_zero_slot(program, function, identity, span);
    if(type.kind == LirTypeKind_Record || type.kind == LirTypeKind_Array) {
        int64_t count = type.kind == LirTypeKind_Record ? type.fields.length : type.count;
        if(count < 0 || count > INT_MAX) return assembly_fail(program, span, "aggregate initialization size limit exceeded");
        int *operands = assembly_allocate(program, (size_t)count, sizeof(*operands));
        if(count && operands == NULL) return -1;
        for(int index = 0; index < count && !program->failed; index++)
            operands[index] = assembly_zero(program, function,
                type.kind == LirTypeKind_Record ? ((int *)type.fields.data)[index] : type.element, span, depth + 1);
        return assembly_emit(program, function, LirOp_Aggregate, identity, operands, (int)count, -1, span);
    }
    if(type.kind == LirTypeKind_Pointer || type.kind == LirTypeKind_Function)
        return assembly_emit(program, function, LirOp_Null, identity, NULL, 0, -1, span);
    return assembly_constant(program, function, identity, 0, span);
}

static const ZirType *
assembly_record(AssemblyProgram *program, int identity)
{
    AssemblyType *names = program->type_names.data;
    for(int index = 0; index < program->type_names.count; index++)
        if(names[index].identity == identity) return names[index].record;
    return NULL;
}

static int
assembly_field(AssemblyProgram *program, int identity, const char *name, ZirSourceSpan span)
{
    const ZirType *record = assembly_record(program, identity);
    if(record) {
        size_t cursor = 0;
        ZirTypeField field;
        int ordinal = 0;
        while(TypeNextField(record, &cursor, &field) == 1) {
            if(!assembly_spend(program, span)) return -1;
            if(!strcmp(field.name, name)) return ordinal;
            ordinal++;
        }
    }
    return assembly_fail(program, span, "record member has no lowered field identity");
}

static int
assembly_member_type(AssemblyProgram *program, int identity, int member)
{
    LirType type = *assembly_type_at(program, identity);
    if(type.kind == LirTypeKind_Record && member >= 0 && member < type.fields.length)
        return ((int *)type.fields.data)[member];
    if(type.kind == LirTypeKind_Array && member >= 0 && member < type.count) return type.element;
    return -1;
}

static int
assembly_named_value(AssemblyProgram *program, AssemblyFunction *function,
                     const char *name, ZirSourceSpan span)
{
    size_t length = strlen(name);
    char *path = assembly_allocate(program, length + 1, 1);
    if(path == NULL) return -1;
    memcpy(path, name, length + 1);
    char *member = strchr(path, '.');
    if(member) *member++ = '\0';
    int binding = assembly_binding(function, path);
    AssemblyBinding current;
    if(binding >= 0) {
        current = ((AssemblyBinding *)function->bindings.data)[binding];
        if(current.storage_type >= 0)
            current.value = assembly_emit(program, function, LirOp_Load, current.type,
                &current.value, 1, -1, span);
    } else {
        /* A receiver that is not local is a module global slot. */
        const ZirModule *owner = NULL;
        const ZirGlobal *global = NULL;
        if(ResolveGlobalAt(function->module, path, SpanPath(span), &owner, &global) != 1)
            return assembly_fail(program, span, "callable receiver is not a lowered local or global binding");
        int address = assembly_global_address(program, function, path, span);
        if(program->failed) return -1;
        LirType pointer = *assembly_type_at(program, ((int *)function->values.data)[address]);
        current = (AssemblyBinding){.type = pointer.element, .storage_type = -1};
        current.value = assembly_emit(program, function, LirOp_Load, current.type, &address, 1, -1, span);
    }
    while(member && !program->failed) {
        if(!assembly_spend(program, span)) return -1;
        char *next = strchr(member, '.');
        if(next) *next++ = '\0';
        if(assembly_type_at(program, current.type)->kind != LirTypeKind_Record)
            return assembly_fail(program, span, "callable receiver path requires a lowered record value");
        int field = assembly_field(program, current.type, member, span);
        if(program->failed) return -1;
        current.type = assembly_member_type(program, current.type, field);
        current.value = assembly_indexed(program, function, LirOp_Extract, current.type,
            &current.value, 1, (uint64_t)field, span);
        member = next;
    }
    return current.value;
}

static int
assembly_string_global(AssemblyProgram *program, const ZirModule *module,
                       const unsigned char *bytes, size_t length, ZirSourceSpan span)
{
    AssemblyGlobal *globals = program->globals.data;
    LirConstant *constants = program->constants.data;
    for(int index = 0; index < program->globals.count; index++) {
        LirGlobal *global = &globals[index].result;
        if(global->writable || global->constant < 0) continue;
        LirConstant *constant = &constants[global->constant];
        if((size_t)constant->bytes.length == length && !memcmp(constant->bytes.data, bytes, length)) return index;
    }
    int array = -1;
    for(int index = 0; index < program->types.count && array < 0; index++) {
        const LirType *type = assembly_type_at(program, index);
        if(type->kind == LirTypeKind_Array && type->element == 3 && type->count == (int64_t)length) array = index;
    }
    if(array < 0) {
        LirType type = {.kind = LirTypeKind_Array, .element = 3, .count = (int64_t)length};
        array = program->types.count;
        if(!assembly_push(program, &program->types, &type)) return -1;
    }
    unsigned char *copy = assembly_allocate(program, length, 1);
    if(copy == NULL) return -1;
    memcpy(copy, bytes, length);
    LirConstant initial = {.type = array, .bytes = {copy, (int64_t)length}};
    int constant = program->constants.count;
    if(!assembly_push(program, &program->constants, &initial)) return -1;
    char name[32];
    snprintf(name, sizeof(name), "string.%d", program->globals.count);
    const char *copied_name = assembly_text(program, name);
    if(program->failed) return -1;
    AssemblyGlobal global = {.module = module, .source = NULL,
        .result = {.module = assembly_name(program, &program->modules, module->name),
            .name = {copied_name, strlen(copied_name)}, .type = array,
            .constant = constant, .writable = 0}};
    int symbol = program->globals.count;
    return assembly_push(program, &program->globals, &global) ? symbol : assembly_fail(program, span, "cannot record string data");
}

static int
assembly_string(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression)
{
    size_t capacity = strlen(expression->text) + 1;
    unsigned char *bytes = assembly_allocate(program, capacity, 1);
    size_t length = 0;
    if(bytes == NULL || !DecodeStringLiteral(expression->text, bytes, capacity, &length))
        return assembly_fail(program, expression->span, "invalid checked string literal");
    ZirSourceSpan span = expression->span;
    int record = assembly_string_type(program, span);
    int pointer = assembly_pointer_type(program, 3, span);
    if(program->failed) return -1;
    int data;
    if(length == 0) data = assembly_emit(program, function, LirOp_Null, pointer, NULL, 0, -1, span);
    else {
        int global = assembly_string_global(program, function->module, bytes, length, span);
        if(global < 0) return -1;
        int array = ((AssemblyGlobal *)program->globals.data)[global].result.type;
        int base = assembly_emit(program, function, LirOp_GlobalAddress,
            assembly_pointer_type(program, array, span), NULL, 0, global, span);
        int operands[] = {base, assembly_constant(program, function, 8, 0, span)};
        data = assembly_emit(program, function, LirOp_ElementAddress, pointer, operands, 2, -1, span);
    }
    int fields[] = {data, assembly_constant(program, function, 8, (uint64_t)length, span)};
    if(program->failed) return -1;
    return assembly_emit(program, function, LirOp_Aggregate, record, fields, 2, -1, span);
}

static int
assembly_member(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression, int depth)
{
    if(expression->kind == ZIR_EXPR_MEMBER) {
        const ZirModule *owner = function->module;
        const ZirType *record = FindType(function->module, expression->type, &owner);
        int64_t bits;
        if(record && record->is_enum && EnumMemberValue(record, expression->name, &bits))
            return assembly_constant(program, function, assembly_type(program, function, expression->type, expression->span),
                (uint64_t)bits, expression->span);
    }
    if(expression->kind == ZIR_EXPR_MEMBER && !strcmp(expression->name, "data") &&
       expression->left >= 0 && ArrayValueType(function->source->exprs[expression->left].type)) {
        int base = assembly_address(program, function, expression->left, depth + 1);
        if(program->failed) return -1;
        LirType pointer = *assembly_type_at(program, ((int *)function->values.data)[base]);
        if(pointer.kind != LirTypeKind_Pointer) return assembly_fail(program, expression->span,
            "fixed-array data requires persistent checked storage");
        LirType array = *assembly_type_at(program, pointer.element);
        if(array.kind != LirTypeKind_Array) return assembly_fail(program, expression->span,
            "fixed-array data requires an array address");
        int result_type = assembly_pointer_type(program, array.element, expression->span);
        if(array.count == 0)
            return assembly_emit(program, function, LirOp_Null, result_type, NULL, 0, -1, expression->span);
        int zero = assembly_constant(program, function, 8, 0, expression->span);
        int operands[] = {base, zero};
        return assembly_emit(program, function, LirOp_ElementAddress, result_type,
            operands, 2, -1, expression->span);
    }
    int value = assembly_expression(program, function, expression->left, depth + 1);
    if(program->failed) return -1;
    int identity = ((int *)function->values.data)[value];
    LirType type = *assembly_type_at(program, identity);
    int member = -1;
    if(expression->kind == ZIR_EXPR_INDEX) {
        if(expression->right < 0 || expression->right >= function->source->expr_count)
            return assembly_fail(program, expression->span, "invalid checked array index");
        const ZirExpr *index = &function->source->exprs[expression->right];
        int view_element = assembly_indexable_element(program, identity);
        if(view_element >= 0) {
            int data = assembly_indexed(program, function, LirOp_Extract,
                assembly_member_type(program, identity, 0), &value, 1, 0, expression->span);
            int count = assembly_indexed(program, function, LirOp_Extract,
                assembly_member_type(program, identity, 1), &value, 1, 1, expression->span);
            int selected = assembly_expression(program, function, expression->right, depth + 1);
            if(program->failed || assembly_type_at(program,
                ((int *)function->values.data)[selected])->kind != LirTypeKind_Integer)
                return assembly_fail(program, expression->span, "string or slice index requires an integer");
            int operands[] = {data, selected, count};
            int pointer = assembly_pointer_type(program, view_element, expression->span);
            int address = assembly_emit(program, function, LirOp_ElementAddress,
                pointer, operands, 3, -1, expression->span);
            return assembly_emit(program, function, LirOp_Load, view_element, &address, 1, -1, expression->span);
        }
        unsigned long long ordinal = 0;
        if(type.kind != LirTypeKind_Array)
            return assembly_fail(program, expression->span, "indexed value is not a lowered fixed array");
        if(index->kind != ZIR_EXPR_INT || !ZirIntegerLiteral(index->text, &ordinal) ||
           ordinal > INT_MAX || ordinal >= (uint64_t)type.count) {
            int selected = assembly_expression(program, function, expression->right, depth + 1);
            if(program->failed) return -1;
            int operands[] = {value, selected};
            return assembly_indexed(program, function, LirOp_Extract, type.element,
                operands, 2, 0, expression->span);
        }
        member = (int)ordinal;
    } else if(assembly_view_element(program, identity) >= 0) {
        if(strcmp(expression->name, "count") && strcmp(expression->name, "data"))
            return assembly_fail(program, expression->span, "string or slice has only data and count members");
        member = !strcmp(expression->name, "count");
    } else if(type.kind == LirTypeKind_Array && !strcmp(expression->name, "count")) {
        int result_type = assembly_type(program, function, expression->type, expression->span);
        return assembly_constant(program, function, result_type, (uint64_t)type.count, expression->span);
    } else member = assembly_field(program, identity, expression->name, expression->span);
    if(program->failed) return -1;
    int result_type = assembly_member_type(program, identity, member);
    if(result_type < 0) return assembly_fail(program, expression->span, "invalid lowered aggregate member");
    return assembly_indexed(program, function, LirOp_Extract, result_type, &value, 1, (uint64_t)member, expression->span);
}

static int
assembly_addressable(AssemblyProgram *program, AssemblyFunction *function, int index, int depth)
{
    if(program->failed || index < 0 || index >= function->source->expr_count) return 0;
    const ZirExpr *expression = &function->source->exprs[index];
    if(!assembly_spend(program, expression->span)) return 0;
    if(depth > 256) return assembly_fail(program, expression->span, "address nesting budget exhausted") >= 0;
    if(expression->kind == ZIR_EXPR_IDENT) {
        if(expression->is_global_value) return 1;
        int binding = assembly_binding(function, expression->name);
        return binding >= 0 && ((AssemblyBinding *)function->bindings.data)[binding].storage_type >= 0;
    }
    if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "*")) return 1;
    /* `pointer.field` reaches the field through the pointer's own address. */
    if(expression->kind == ZIR_EXPR_POINTER_MEMBER) return 1;
    if(expression->kind == ZIR_EXPR_INDEX) {
        const char *base = function->source->exprs[expression->left].type;
        /* A borrowed view's elements live behind its checked data pointer,
         * whatever holds the view itself. */
        if(!strcmp(base, "string") || SliceElementType(base, NULL, 0) ||
           assembly_source_vec(function, base)) return 1;
        return ArrayValueType(base) && assembly_addressable(program, function, expression->left, depth + 1);
    }
    if(expression->kind == ZIR_EXPR_MEMBER)
        return assembly_addressable(program, function, expression->left, depth + 1);
    return 0;
}

static int
assembly_address(AssemblyProgram *program, AssemblyFunction *function, int index, int depth)
{
    if(program->failed) return -1;
    if(index < 0 || index >= function->source->expr_count)
        return assembly_fail(program, function->source->span, "invalid checked address expression");
    const ZirExpr *expression = &function->source->exprs[index];
    if(!assembly_spend(program, expression->span)) return -1;
    if(depth > 256) return assembly_fail(program, expression->span, "address nesting budget exhausted");
    if(expression->kind == ZIR_EXPR_IDENT && expression->is_global_value)
        return assembly_global_address(program, function, expression->name, expression->span);
    if(expression->kind == ZIR_EXPR_IDENT) {
        int binding = assembly_binding(function, expression->name);
        if(binding < 0 || ((AssemblyBinding *)function->bindings.data)[binding].storage_type < 0)
            return assembly_fail(program, expression->span, "address requires a promoted local");
        return ((AssemblyBinding *)function->bindings.data)[binding].value;
    }
    if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "*"))
        return assembly_expression(program, function, expression->right, depth + 1);
    if(expression->kind == ZIR_EXPR_POINTER_MEMBER) {
        int address = assembly_expression(program, function, expression->left, depth + 1);
        if(program->failed) return -1;
        LirType type = *assembly_type_at(program, ((int *)function->values.data)[address]);
        if(type.kind != LirTypeKind_Pointer)
            return assembly_fail(program, expression->span, "pointer member requires a pointer");
        int field = assembly_field(program, type.element, expression->name, expression->span);
        if(program->failed) return -1;
        int member = assembly_member_type(program, type.element, field);
        if(member < 0) return assembly_fail(program, expression->span, "pointer member is outside its record's shape");
        return assembly_indexed(program, function, LirOp_FieldAddress,
            assembly_pointer_type(program, member, expression->span), &address, 1, (uint64_t)field, expression->span);
    }
    if(expression->kind == ZIR_EXPR_INDEX && expression->left >= 0 &&
       expression->left < function->source->expr_count &&
       (!strcmp(function->source->exprs[expression->left].type, "string") ||
        SliceElementType(function->source->exprs[expression->left].type, NULL, 0) ||
        assembly_source_vec(function, function->source->exprs[expression->left].type))) {
        int view = assembly_expression(program, function, expression->left, depth + 1);
        if(program->failed) return -1;
        int identity = ((int *)function->values.data)[view];
        int element = assembly_indexable_element(program, identity);
        if(element < 0) return assembly_fail(program, expression->span, "indexed view has no lowered element type");
        int data = assembly_indexed(program, function, LirOp_Extract,
            assembly_member_type(program, identity, 0), &view, 1, 0, expression->span);
        int count = assembly_indexed(program, function, LirOp_Extract,
            assembly_member_type(program, identity, 1), &view, 1, 1, expression->span);
        int ordinal = assembly_expression(program, function, expression->right, depth + 1);
        if(program->failed) return -1;
        if(assembly_type_at(program, ((int *)function->values.data)[ordinal])->kind != LirTypeKind_Integer)
            return assembly_fail(program, expression->span, "string or slice index requires an integer");
        int operands[] = {data, ordinal, count};
        return assembly_emit(program, function, LirOp_ElementAddress,
            assembly_pointer_type(program, element, expression->span), operands, 3, -1, expression->span);
    }
    if(expression->kind == ZIR_EXPR_INDEX) {
        int address = assembly_address(program, function, expression->left, depth + 1);
        if(program->failed) return -1;
        int pointer = ((int *)function->values.data)[address];
        LirType type = *assembly_type_at(program, pointer);
        if(type.kind != LirTypeKind_Pointer || assembly_type_at(program, type.element)->kind != LirTypeKind_Array)
            return assembly_fail(program, expression->span, "element address requires a fixed-array pointer");
        int element = assembly_type_at(program, type.element)->element;
        int ordinal = assembly_expression(program, function, expression->right, depth + 1);
        if(program->failed) return -1;
        if(assembly_type_at(program, ((int *)function->values.data)[ordinal])->kind != LirTypeKind_Integer)
            return assembly_fail(program, expression->span, "element address requires a fixed integer index");
        int result_type = assembly_pointer_type(program, element, expression->span);
        int operands[] = {address, ordinal};
        return assembly_emit(program, function, LirOp_ElementAddress, result_type, operands, 2, -1, expression->span);
    }
    if(expression->kind == ZIR_EXPR_MEMBER) {
        int address = assembly_address(program, function, expression->left, depth + 1);
        if(program->failed) return -1;
        int pointer = ((int *)function->values.data)[address];
        LirType type = *assembly_type_at(program, pointer);
        if(type.kind != LirTypeKind_Pointer)
            return assembly_fail(program, expression->span, "record address requires a pointer");
        int field = assembly_field(program, type.element, expression->name, expression->span);
        if(program->failed) return -1;
        int member = assembly_member_type(program, type.element, field);
        if(member < 0) return assembly_fail(program, expression->span, "record address is outside its declared shape");
        int result_type = assembly_pointer_type(program, member, expression->span);
        return assembly_indexed(program, function, LirOp_FieldAddress, result_type, &address, 1,
            (uint64_t)field, expression->span);
    }
    return assembly_fail(program, expression->span, "only globals, dereferenced pointers and their fields or fixed-array elements have lowered addresses yet");
}

static int
assembly_compound(AssemblyProgram *program, AssemblyFunction *function,
                  const ZirExpr *expression, int identity, int depth)
{
    int value = assembly_zero(program, function, identity, expression->span, depth + 1);
    if(program->failed) return -1;
    LirType type = *assembly_type_at(program, identity);
    int ordinal = 0;
    for(int child = expression->first_child; child >= 0;) {
        if(child >= function->source->expr_count || !assembly_spend(program, expression->span)) return -1;
        const ZirExpr *initializer = &function->source->exprs[child];
        if(initializer->kind != ZIR_EXPR_FIELD_INIT)
            return assembly_fail(program, initializer->span, "invalid checked aggregate initializer");
        int member = type.kind == LirTypeKind_Record && initializer->name[0] ?
            assembly_field(program, identity, initializer->name, initializer->span) : ordinal;
        if(program->failed) return -1;
        int member_type = assembly_member_type(program, identity, member);
        if(member_type < 0) return assembly_fail(program, initializer->span, "aggregate initializer is outside its declared shape");
        /* A nested literal takes its member's type; a parsed global
         * initializer does not spell it on the inner literal. */
        int nested = initializer->right >= 0 && initializer->right < function->source->expr_count &&
            function->source->exprs[initializer->right].kind == ZIR_EXPR_COMPOUND;
        int outer_at = function->literal_at, outer_type = function->literal_type;
        function->literal_at = initializer->right + 1;
        function->literal_type = member_type;
        int initialized = nested ?
            assembly_compound(program, function, &function->source->exprs[initializer->right], member_type, depth + 1) :
            assembly_expression(program, function, initializer->right, depth + 1);
        function->literal_at = outer_at;
        function->literal_type = outer_type;
        initialized = assembly_convert(program, function, initialized, member_type, initializer->span);
        int operands[2] = {value, initialized};
        value = assembly_indexed(program, function, LirOp_Insert, identity, operands, 2, (uint64_t)member, initializer->span);
        ordinal++;
        child = initializer->next_sibling;
    }
    return value;
}

static LirOp
assembly_binary(const char *op)
{
    if(!strcmp(op, "+")) return LirOp_Add;
    if(!strcmp(op, "-")) return LirOp_Subtract;
    if(!strcmp(op, "*")) return LirOp_Multiply;
    if(!strcmp(op, "/")) return LirOp_Divide;
    if(!strcmp(op, "%")) return LirOp_Remainder;
    if(!strcmp(op, "&")) return LirOp_BitAnd;
    if(!strcmp(op, "|")) return LirOp_BitOr;
    if(!strcmp(op, "^")) return LirOp_BitXor;
    if(!strcmp(op, "<<")) return LirOp_ShiftLeft;
    if(!strcmp(op, ">>")) return LirOp_ShiftRight;
    if(!strcmp(op, "==")) return LirOp_Equal;
    if(!strcmp(op, "!=")) return LirOp_NotEqual;
    if(!strcmp(op, "<")) return LirOp_Less;
    if(!strcmp(op, "<=")) return LirOp_LessEqual;
    if(!strcmp(op, ">")) return LirOp_Greater;
    if(!strcmp(op, ">=")) return LirOp_GreaterEqual;
    return LirOp_Invalid;
}

static int
assembly_select(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression,
                int condition, int yes_root, int no_root, int short_circuit, int depth)
{
    int count = function->bindings.count;
    AssemblyBinding *before = assembly_snapshot(program, function, count);
    int yes = assembly_block(program, function, before, count);
    int no = assembly_block(program, function, before, count);
    if(program->failed) return -1;
    assembly_branch(program, function, condition, yes, no, count, expression->span);
    assembly_enter(function, yes, before, count);
    int yes_value = short_circuit == 1 ? assembly_constant(program, function, 1, 1, expression->span) :
        assembly_expression(program, function, yes_root, depth + 1);
    int type = assembly_type(program, function, expression->type, expression->span);
    yes_value = assembly_convert(program, function, yes_value, type, expression->span);
    int yes_end = function->current;
    assembly_enter(function, no, before, count);
    int no_value = short_circuit == 2 ? assembly_constant(program, function, 1, 0, expression->span) :
        assembly_expression(program, function, no_root, depth + 1);
    no_value = assembly_convert(program, function, no_value, type, expression->span);
    int no_end = function->current;
    AssemblyBinding *no_bindings = assembly_snapshot(program, function, count);
    AssemblyBinding *joined = assembly_allocate(program, (size_t)count + 1, sizeof(*joined));
    if(program->failed) return -1;
    if(count) memcpy(joined, before, (size_t)count * sizeof(*joined));
    joined[count] = (AssemblyBinding){"", type, yes_value, -1};
    int join = assembly_block(program, function, joined, count + 1);
    if(program->failed) return -1;
    LirEdge edges[2] = {assembly_edge(program, join, joined, count + 1), {0}};
    if(count) memcpy(joined, no_bindings, (size_t)count * sizeof(*joined));
    joined[count].value = no_value;
    edges[1] = assembly_edge(program, join, joined, count + 1);
    int ends[2] = {yes_end, no_end};
    for(int branch = 0; branch < 2 && !program->failed; branch++) {
        function->current = ends[branch];
        assembly_emit(program, function, LirOp_Jump, 0, NULL, 0, -1, expression->span);
        AssemblyBlock *block = assembly_block_at(function, function->current);
        LirEdge *edge = assembly_allocate(program, 1, sizeof(*edge));
        if(edge == NULL) return -1;
        *edge = edges[branch];
        LirInstruction *instructions = block->instructions.data;
        instructions[block->instructions.count - 1].edges = (Slice){edge, 1};
    }
    assembly_enter(function, join, before, count);
    return ((int *)assembly_block_at(function, join)->parameters.data)[count];
}

static int
assembly_lower_slice(AssemblyProgram *program, AssemblyFunction *function,
               const ZirExpr *expression, int result_type, int depth)
{
    if(expression->left < 0 || expression->left >= function->source->expr_count)
        return assembly_fail(program, expression->span, "invalid checked slice source");
    const ZirExpr *source = &function->source->exprs[expression->left];
    int data = -1, count = -1, element = -1;
    if(ArrayValueType(source->type)) {
        int base = assembly_address(program, function, expression->left, depth + 1);
        if(program->failed) return -1;
        LirType pointer = *assembly_type_at(program, ((int *)function->values.data)[base]);
        if(pointer.kind != LirTypeKind_Pointer)
            return assembly_fail(program, expression->span, "slice source has no persistent array address");
        LirType array = *assembly_type_at(program, pointer.element);
        if(array.kind != LirTypeKind_Array)
            return assembly_fail(program, expression->span, "slice source is not a lowered fixed array");
        element = array.element;
        count = assembly_constant(program, function, 8, (uint64_t)array.count, expression->span);
        int pointer_type = assembly_pointer_type(program, element, expression->span);
        if(array.count == 0)
            data = assembly_emit(program, function, LirOp_Null, pointer_type, NULL, 0, -1, expression->span);
        else {
            int zero = assembly_constant(program, function, 8, 0, expression->span);
            int operands[] = {base, zero};
            data = assembly_emit(program, function, LirOp_ElementAddress,
                pointer_type, operands, 2, -1, expression->span);
        }
    } else {
        int view = assembly_expression(program, function, expression->left, depth + 1);
        if(program->failed) return -1;
        int identity = ((int *)function->values.data)[view];
        element = assembly_indexable_element(program, identity);
        if(element < 0) return assembly_fail(program, expression->span,
            "slice source is not a lowered string, slice or vector");
        data = assembly_indexed(program, function, LirOp_Extract,
            assembly_member_type(program, identity, 0), &view, 1, 0, expression->span);
        count = assembly_indexed(program, function, LirOp_Extract,
            assembly_member_type(program, identity, 1), &view, 1, 1, expression->span);
    }
    int low = expression->right >= 0 ?
        assembly_expression(program, function, expression->right, depth + 1) :
        assembly_constant(program, function, 8, 0, expression->span);
    int high = expression->third >= 0 ?
        assembly_expression(program, function, expression->third, depth + 1) : count;
    low = assembly_convert(program, function, low, 8, expression->span);
    high = assembly_convert(program, function, high, 8, expression->span);
    if(program->failed) return -1;
    int pointer_type = assembly_pointer_type(program, element, expression->span);
    int address_operands[] = {data, low, high, count};
    int selected = assembly_emit(program, function, LirOp_ElementAddress,
        pointer_type, address_operands, 4, -1, expression->span);
    int count_operands[] = {high, low};
    int selected_count = assembly_emit(program, function, LirOp_Subtract,
        8, count_operands, 2, -1, expression->span);
    int fields[] = {selected, selected_count};
    return assembly_emit(program, function, LirOp_Aggregate,
        result_type, fields, 2, -1, expression->span);
}

/* Native services are typed LIR host calls with Ziran-owned names. A target
 * either supplies the exact veneer or reports the service as unavailable; it
 * is never rewritten to libc or to another execution engine. */
static int
assembly_platform_function(AssemblyProgram *program, const ZirModule *module,
                           int service, const int *parameters, int count,
                           ZirSourceSpan span)
{
    const char *name = assembly_print_services[service];
    AssemblyFunction **functions = program->functions.data;
    for(int index = 0; index < program->functions.count; index++) {
        LirFunction *function = &functions[index]->result;
        if(function->external && function->name.length == strlen(name) &&
           !memcmp(function->name.data, name, function->name.length)) return index;
    }
    AssemblyFunction *function = assembly_allocate(program, 1, sizeof(*function));
    int *types = assembly_allocate(program, (size_t)count, sizeof(*types));
    if(function == NULL || (count && types == NULL)) return -1;
    if(count) memcpy(types, parameters, (size_t)count * sizeof(*types));
    function->module = module;
    function->blocks.width = sizeof(AssemblyBlock);
    function->values.width = sizeof(int);
    function->bindings.width = sizeof(AssemblyBinding);
    function->current = -1;
    function->result.module = assembly_name(program, &program->modules, "ziran.platform");
    name = assembly_text(program, name);
    function->result.name = (String){name, strlen(name)};
    function->result.parameters = (Slice){types, count};
    function->result.result = 0;
    function->result.calling = LirCalling_Host;
    function->result.external = 1;
    function->result.entry = -1;
    function->result.origin = assembly_origin(program, span);
    int identity = program->functions.count;
    return assembly_push(program, &program->functions, &function) ? identity : -1;
}

static int
assembly_print_service(AssemblyProgram *program, AssemblyFunction *function,
                       int service, const int *operands, int count,
                       ZirSourceSpan span)
{
    int parameters[3];
    for(int index = 0; index < count; index++)
        parameters[index] = ((int *)function->values.data)[operands[index]];
    int symbol = assembly_platform_function(program, function->module, service,
        parameters, count, span);
    if(symbol < 0) return -1;
    return assembly_emit(program, function, LirOp_HostCall, 0,
        operands, count, symbol, span);
}

static int
assembly_print_bytes(AssemblyProgram *program, AssemblyFunction *function,
                     int view, ZirSourceSpan span)
{
    int identity = ((int *)function->values.data)[view];
    LirType type = *assembly_type_at(program, identity);
    if(type.kind != LirTypeKind_Record || type.fields.length != 2)
        return assembly_fail(program, span, "print text has no lowered byte-view representation");
    int pointer = ((int *)type.fields.data)[0];
    LirType pointer_type = *assembly_type_at(program, pointer);
    if(pointer_type.kind != LirTypeKind_Pointer || pointer_type.element != 3 ||
       ((int *)type.fields.data)[1] != 8)
        return assembly_fail(program, span, "print text is not a byte view");
    int operands[3];
    operands[0] = assembly_constant(program, function, 9, 1, span);
    operands[1] = assembly_indexed(program, function, LirOp_Extract,
        pointer, &view, 1, 0, span);
    operands[2] = assembly_indexed(program, function, LirOp_Extract,
        8, &view, 1, 1, span);
    operands[2] = assembly_convert(program, function, operands[2], 9, span);
    return assembly_print_service(program, function, ASSEMBLY_PRINT_BYTES,
        operands, 3, span);
}

static int
assembly_text_view(AssemblyProgram *program, AssemblyFunction *function,
                   const ZirExpr *expression, int result_type, int depth)
{
    int child = expression->first_child;
    if(child < 0 || child >= function->source->expr_count ||
       function->source->exprs[child].next_sibling >= 0)
        return assembly_fail(program, expression->span, "invalid checked TextView call");
    int source = assembly_expression(program, function, child, depth + 1);
    if(program->failed) return -1;
    int identity = ((int *)function->values.data)[source];
    LirType type = *assembly_type_at(program, identity);
    /* Text is the record {bytes: Vec(u8)}; its view is the vector's data
     * pointer and count. */
    if(type.kind == LirTypeKind_Record && type.fields.length == 1 &&
       assembly_vec_element(program, ((int *)type.fields.data)[0]) == 3) {
        identity = ((int *)type.fields.data)[0];
        source = assembly_indexed(program, function, LirOp_Extract, identity, &source, 1, 0, expression->span);
        type = *assembly_type_at(program, identity);
    }
    if(type.kind != LirTypeKind_Record || (type.fields.length != 2 && assembly_vec_element(program, identity) != 3))
        return assembly_fail(program, expression->span, "TextView source has no lowered slice representation");
    int *fields = type.fields.data;
    LirType pointer = *assembly_type_at(program, fields[0]);
    if(pointer.kind != LirTypeKind_Pointer || pointer.element != 3 || fields[1] != 8)
        return assembly_fail(program, expression->span, "TextView source is not a byte slice");
    int values[2];
    values[0] = assembly_indexed(program, function, LirOp_Extract,
        fields[0], &source, 1, 0, expression->span);
    values[1] = assembly_indexed(program, function, LirOp_Extract,
        fields[1], &source, 1, 1, expression->span);
    return assembly_emit(program, function, LirOp_Aggregate,
        result_type, values, 2, -1, expression->span);
}

static int
assembly_print(AssemblyProgram *program, AssemblyFunction *function,
               const ZirExpr *expression, int depth)
{
    int format = expression->first_child;
    if(format < 0 || format >= function->source->expr_count ||
       function->source->exprs[format].kind != ZIR_EXPR_STRING)
        return assembly_fail(program, expression->span, "invalid checked print format");
    int argument_count = 0;
    for(int child = function->source->exprs[format].next_sibling; child >= 0;
        child = function->source->exprs[child].next_sibling) {
        if(child >= function->source->expr_count || argument_count >= PRINT_PIECES_MAX)
            return assembly_fail(program, expression->span, "invalid checked print arguments");
        argument_count++;
    }
    int *values = assembly_allocate(program, (size_t)argument_count, sizeof(*values));
    const char **types = assembly_allocate(program, (size_t)argument_count, sizeof(*types));
    if(argument_count && (values == NULL || types == NULL)) return -1;
    int argument = 0;
    for(int child = function->source->exprs[format].next_sibling; child >= 0;
        child = function->source->exprs[child].next_sibling) {
        /* Evaluate every argument before the first output service. */
        values[argument] = assembly_expression(program, function, child, depth + 1);
        types[argument] = ScalarType(function->source->exprs[child].type);
        if(program->failed) return -1;
        argument++;
    }
    PrintPiece *pieces = calloc(PRINT_PIECES_MAX, sizeof(*pieces));
    int count = pieces ? PrintFormatPieces(function->source->exprs[format].text,
        pieces, PRINT_PIECES_MAX) : -1;
    if(count < 0) {
        free(pieces);
        return assembly_fail(program, expression->span, "invalid checked print format");
    }
    argument = 0;
    for(int index = 0; index < count && !program->failed; index++) {
        if(!pieces[index].is_argument) {
            ZirExpr literal = function->source->exprs[format];
            literal.text = pieces[index].literal;
            int view = assembly_string(program, function, &literal);
            if(view >= 0) assembly_print_bytes(program, function, view, expression->span);
            continue;
        }
        if(argument >= argument_count) {
            assembly_fail(program, expression->span, "invalid checked print argument count");
            break;
        }
        int value = values[argument];
        const char *scalar = types[argument++];
        if(!strcmp(scalar, "string")) {
            assembly_print_bytes(program, function, value, expression->span);
        } else if(!strcmp(scalar, "bool")) {
            int converted = assembly_convert(program, function, value, 1, expression->span);
            assembly_print_service(program, function, ASSEMBLY_PRINT_BOOL,
                &converted, 1, expression->span);
        } else if(!strcmp(scalar, "float32") || !strcmp(scalar, "float64")) {
            int service = !strcmp(scalar, "float32") ?
                ASSEMBLY_PRINT_FLOAT32 : ASSEMBLY_PRINT_FLOAT64;
            assembly_print_service(program, function, service,
                &value, 1, expression->span);
        } else {
            int source_type = ((int *)function->values.data)[value];
            LirType integer = *assembly_type_at(program, source_type);
            if(integer.kind != LirTypeKind_Integer) {
                assembly_fail(program, expression->span,
                    "print argument has no native scalar formatter");
                break;
            }
            int target = integer.is_signed ? 8 : 9;
            int converted = assembly_convert(program, function, value, target, expression->span);
            assembly_print_service(program, function,
                integer.is_signed ? ASSEMBLY_PRINT_SIGNED : ASSEMBLY_PRINT_UNSIGNED,
                &converted, 1, expression->span);
        }
    }
    free(pieces);
    return -1;
}

/* Where an owned vector lives: a checked address, or a local binding path. */
typedef struct AssemblyVecPlace {
    int address;
    const AssemblyLocalPlace *local;
} AssemblyVecPlace;

static int
assembly_vec_read(AssemblyProgram *program, AssemblyFunction *function, int root, int identity,
                  AssemblyVecPlace *place, ZirSourceSpan span)
{
    place->address = -1;
    place->local = NULL;
    if(assembly_addressable(program, function, root, 0))
        place->address = assembly_address(program, function, root, 0);
    else place->local = assembly_capture_local(program, function, root, 0);
    if(program->failed) return -1;
    if(place->address >= 0)
        return assembly_emit(program, function, LirOp_Load, identity, &place->address, 1, -1, span);
    if(place->local->storage)
        return assembly_emit(program, function, LirOp_Load, identity, &place->local->value, 1, -1, span);
    return place->local->value;
}

static void
assembly_vec_write(AssemblyProgram *program, AssemblyFunction *function, const AssemblyVecPlace *place,
                   int value, ZirSourceSpan span)
{
    if(program->failed) return;
    if(place->address >= 0) {
        int operands[] = {place->address, value};
        assembly_emit(program, function, LirOp_Store, 0, operands, 2, -1, span);
    } else assembly_store_captured(program, function, place->local, value, span);
}

/* Free the heap block of one vector value. A null data pointer with
 * capacity 0, which a default or freed vector holds, frees nothing. */
static void
assembly_vec_release(AssemblyProgram *program, AssemblyFunction *function, int vector, int identity,
                     ZirSourceSpan span)
{
    int element = assembly_vec_element(program, identity);
    if(program->failed || element < 0) return;
    int pointer = assembly_pointer_type(program, element, span);
    int data = assembly_indexed(program, function, LirOp_Extract, pointer, &vector, 1, 0, span);
    int capacity = assembly_indexed(program, function, LirOp_Extract, 8, &vector, 1, 2, span);
    int zero = assembly_constant(program, function, 8, 0, span);
    int operands[] = {data, capacity, zero};
    assembly_emit(program, function, LirOp_Resize, pointer, operands, 3, -1, span);
}

static void assembly_drop_value(AssemblyProgram *, AssemblyFunction *, int, int, ZirSourceSpan, int);

/* Drop every element of a vector whose elements own storage. The vector's
 * own block and fields are left as they are. */
static void
assembly_drop_elements(AssemblyProgram *program, AssemblyFunction *function, int vector, int identity,
                       ZirSourceSpan span, int depth)
{
    int element = assembly_vec_element(program, identity);
    if(program->failed || function->current < 0 || element < 0 || !assembly_owned(program, element, 0)) return;
    int pointer = assembly_pointer_type(program, element, span);
    int data = assembly_indexed(program, function, LirOp_Extract, pointer, &vector, 1, 0, span);
    int count = assembly_indexed(program, function, LirOp_Extract, 8, &vector, 1, 1, span);
    int base = function->bindings.count;
    AssemblyBinding cursor = {"", 8, assembly_constant(program, function, 8, 0, span), -1};
    assembly_push(program, &function->bindings, &cursor);
    int live = base + 1;
    AssemblyBinding *before = assembly_snapshot(program, function, live);
    int header = assembly_block(program, function, before, live);
    int body = assembly_block(program, function, before, live);
    int exit = assembly_block(program, function, before, live);
    if(program->failed || before == NULL) return;
    assembly_jump(program, function, header, live, span);
    assembly_enter(function, header, before, live);
    assembly_emit(program, function, LirOp_Fuel, 0, NULL, 0, -1, span);
    int more_operands[] = {((AssemblyBinding *)function->bindings.data)[base].value, count};
    int more = assembly_emit(program, function, LirOp_Less, 1, more_operands, 2, -1, span);
    assembly_branch(program, function, more, body, exit, live, span);
    assembly_enter(function, body, before, live);
    int at = ((AssemblyBinding *)function->bindings.data)[base].value;
    int slot_operands[] = {data, at, count};
    int slot = assembly_emit(program, function, LirOp_ElementAddress, pointer, slot_operands, 3, -1, span);
    int item = assembly_emit(program, function, LirOp_Load, element, &slot, 1, -1, span);
    assembly_drop_value(program, function, item, element, span, depth + 1);
    int next_operands[] = {at, assembly_constant(program, function, 8, 1, span)};
    ((AssemblyBinding *)function->bindings.data)[base].value =
        assembly_emit(program, function, LirOp_Add, 8, next_operands, 2, -1, span);
    assembly_jump(program, function, header, live, span);
    assembly_enter(function, exit, before, live);
    function->bindings.count = base;
}

/* Free everything one owned value holds: a vector's elements and block,
 * and the owned fields and elements of records and fixed arrays. */
static void
assembly_drop_value(AssemblyProgram *program, AssemblyFunction *function, int value, int identity,
                    ZirSourceSpan span, int depth)
{
    if(program->failed || function->current < 0 || depth > 32 || !assembly_owned(program, identity, 0)) return;
    if(assembly_vec_element(program, identity) >= 0) {
        assembly_drop_elements(program, function, value, identity, span, depth);
        assembly_vec_release(program, function, value, identity, span);
        return;
    }
    LirType type = *assembly_type_at(program, identity);
    int64_t members = type.kind == LirTypeKind_Array ? type.count : type.fields.length;
    for(int64_t member = 0; member < members && !program->failed; member++) {
        int member_type = type.kind == LirTypeKind_Array ? type.element : ((int *)type.fields.data)[member];
        if(!assembly_owned(program, member_type, 0)) continue;
        int item = assembly_indexed(program, function, LirOp_Extract, member_type, &value, 1,
            (uint64_t)member, span);
        assembly_drop_value(program, function, item, member_type, span, depth + 1);
    }
}

/* Leaving a scope drops its owned values, newest first, as the C and Go
 * emitters do at block close, return, break and continue. */
static void
assembly_drop_bindings(AssemblyProgram *program, AssemblyFunction *function, int first, ZirSourceSpan span)
{
    for(int index = function->bindings.count - 1;
        index >= first && !program->failed && function->current >= 0; index--) {
        AssemblyBinding binding = ((AssemblyBinding *)function->bindings.data)[index];
        if(binding.name == NULL || !*binding.name || !assembly_owned(program, binding.type, 0)) continue;
        int value = binding.storage_type >= 0 ?
            assembly_emit(program, function, LirOp_Load, binding.type, &binding.value, 1, -1, span) :
            binding.value;
        assembly_drop_value(program, function, value, binding.type, span, 0);
    }
}

/* Empty the source of a move. Only a local, or a member path rooted at
 * one, can be moved from; the checker rejects every other source. */
static void
assembly_move_out(AssemblyProgram *program, AssemblyFunction *function, int root, int identity,
                  ZirSourceSpan span)
{
    if(program->failed || function->current < 0 || !assembly_owned(program, identity, 0)) return;
    int at = root;
    while(at >= 0 && at < function->source->expr_count && function->source->exprs[at].kind == ZIR_EXPR_MEMBER)
        at = function->source->exprs[at].left;
    if(at < 0 || at >= function->source->expr_count) return;
    const ZirExpr *base = &function->source->exprs[at];
    if(base->kind != ZIR_EXPR_IDENT || base->is_global_value || assembly_binding(function, base->name) < 0)
        return;
    AssemblyVecPlace place = {-1, NULL};
    if(assembly_addressable(program, function, root, 0))
        place.address = assembly_address(program, function, root, 0);
    else place.local = assembly_capture_local(program, function, root, 0);
    assembly_vec_write(program, function, &place, assembly_zero(program, function, identity, span, 0), span);
}

/* An unnamed binding carries a value through block parameters. */
static int
assembly_carry(AssemblyProgram *program, AssemblyFunction *function, int type, int value)
{
    AssemblyBinding binding = {"", type, value, -1};
    int index = function->bindings.count;
    return assembly_push(program, &function->bindings, &binding) ? index : -1;
}

static int
assembly_carried(AssemblyFunction *function, int index)
{
    return ((AssemblyBinding *)function->bindings.data)[index].value;
}

/* A block template of the first `count` bindings plus one unnamed value. */
static AssemblyBinding *
assembly_template(AssemblyProgram *program, const AssemblyBinding *before, int count, int type)
{
    AssemblyBinding *template = assembly_allocate(program, (size_t)count + 1, sizeof(*template));
    if(template == NULL) return NULL;
    if(count) memcpy(template, before, (size_t)count * sizeof(*template));
    template[count] = (AssemblyBinding){"", type, -1, -1};
    return template;
}

/* Give the vector carried in binding `carried` room for `needed` elements,
 * with the C runtime's policy: capacity 0 starts at 8 and doubles until it
 * fits. Returns a bool value; on false the vector is unchanged. `carried`
 * must be the newest binding. */
static int
assembly_vec_reserve(AssemblyProgram *program, AssemblyFunction *function, int carried, int needed,
                     int identity, ZirSourceSpan span)
{
    int element = assembly_vec_element(program, identity);
    if(program->failed || function->current < 0 || element < 0) return -1;
    int pointer = assembly_pointer_type(program, element, span);
    int live = function->bindings.count;
    int vector = assembly_carried(function, carried);
    int data = assembly_indexed(program, function, LirOp_Extract, pointer, &vector, 1, 0, span);
    int capacity = assembly_indexed(program, function, LirOp_Extract, 8, &vector, 1, 2, span);
    int zero = assembly_constant(program, function, 8, 0, span);
    AssemblyBinding *before = assembly_snapshot(program, function, live);
    AssemblyBinding *answered = assembly_template(program, before, live, 1);
    AssemblyBinding *sized = assembly_template(program, before, live, 8);
    if(program->failed || before == NULL || answered == NULL || sized == NULL) return -1;
    int fits = assembly_block(program, function, before, live);
    int grow = assembly_block(program, function, before, live);
    int header = assembly_block(program, function, sized, live + 1);
    int doubled = assembly_block(program, function, sized, live + 1);
    int resize = assembly_block(program, function, sized, live + 1);
    int granted = assembly_block(program, function, before, live);
    int refused = assembly_block(program, function, before, live);
    int done = assembly_block(program, function, answered, live + 1);
    if(program->failed) return -1;
    int enough_operands[] = {needed, capacity};
    int enough = assembly_emit(program, function, LirOp_LessEqual, 1, enough_operands, 2, -1, span);
    assembly_branch(program, function, enough, fits, grow, live, span);

    assembly_enter(function, fits, before, live);
    assembly_carry(program, function, 1, assembly_constant(program, function, 1, 1, span));
    assembly_jump(program, function, done, live + 1, span);

    assembly_enter(function, grow, before, live);
    int empty_operands[] = {capacity, zero};
    int empty = assembly_emit(program, function, LirOp_Equal, 1, empty_operands, 2, -1, span);
    int initial_operands[] = {assembly_emit(program, function, LirOp_Convert, 8, &empty, 1, -1, span),
        assembly_constant(program, function, 8, 8, span)};
    int start_operands[] = {capacity,
        assembly_emit(program, function, LirOp_Multiply, 8, initial_operands, 2, -1, span)};
    assembly_carry(program, function, 8, assembly_emit(program, function, LirOp_Add, 8, start_operands, 2, -1, span));
    assembly_jump(program, function, header, live + 1, span);

    assembly_enter(function, header, sized, live + 1);
    assembly_emit(program, function, LirOp_Fuel, 0, NULL, 0, -1, span);
    int small_operands[] = {assembly_carried(function, live), needed};
    int small = assembly_emit(program, function, LirOp_Less, 1, small_operands, 2, -1, span);
    assembly_branch(program, function, small, doubled, resize, live + 1, span);

    assembly_enter(function, doubled, sized, live + 1);
    int twice_operands[] = {assembly_carried(function, live), assembly_carried(function, live)};
    ((AssemblyBinding *)function->bindings.data)[live].value =
        assembly_emit(program, function, LirOp_Add, 8, twice_operands, 2, -1, span);
    assembly_jump(program, function, header, live + 1, span);

    assembly_enter(function, resize, sized, live + 1);
    int next = assembly_carried(function, live);
    int resize_operands[] = {data, capacity, next};
    int moved = assembly_emit(program, function, LirOp_Resize, pointer, resize_operands, 3, -1, span);
    int failed_operands[] = {moved, assembly_emit(program, function, LirOp_Null, pointer, NULL, 0, -1, span)};
    int failed = assembly_emit(program, function, LirOp_Equal, 1, failed_operands, 2, -1, span);
    assembly_branch(program, function, failed, refused, granted, live, span);

    assembly_enter(function, granted, before, live);
    int data_operands[] = {assembly_carried(function, carried), moved};
    int with_data = assembly_indexed(program, function, LirOp_Insert, identity, data_operands, 2, 0, span);
    int capacity_operands[] = {with_data, next};
    ((AssemblyBinding *)function->bindings.data)[carried].value =
        assembly_indexed(program, function, LirOp_Insert, identity, capacity_operands, 2, 2, span);
    assembly_carry(program, function, 1, assembly_constant(program, function, 1, 1, span));
    assembly_jump(program, function, done, live + 1, span);

    assembly_enter(function, refused, before, live);
    assembly_carry(program, function, 1, assembly_constant(program, function, 1, 0, span));
    assembly_jump(program, function, done, live + 1, span);

    if(program->failed) return -1;
    assembly_enter(function, done, answered, live + 1);
    int answer = assembly_carried(function, live);
    function->bindings.count = live;
    return answer;
}

/* BuilderAppend(builder, text): an empty text succeeds without allocating;
 * otherwise room is reserved first, so a refused reserve appends nothing. */
static int
assembly_builder_append(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression, int depth)
{
    ZirSourceSpan span = expression->span;
    int first = expression->first_child;
    int second = first >= 0 && first < function->source->expr_count ?
        function->source->exprs[first].next_sibling : -1;
    if(second < 0 || second >= function->source->expr_count)
        return assembly_fail(program, span, "BuilderAppend requires a builder and text");
    int identity = assembly_type(program, function, function->source->exprs[first].type, span);
    int string = assembly_string_type(program, span);
    if(program->failed) return -1;
    if(assembly_vec_element(program, identity) != 3)
        return assembly_fail(program, span, "BuilderAppend requires a Vec(u8) builder");
    int pointer = assembly_pointer_type(program, 3, span);
    int text = assembly_convert(program, function,
        assembly_expression(program, function, second, depth + 1), string, span);
    AssemblyVecPlace place;
    int current = assembly_vec_read(program, function, first, identity, &place, span);
    if(program->failed) return -1;
    int bytes = assembly_indexed(program, function, LirOp_Extract, pointer, &text, 1, 0, span);
    int length = assembly_indexed(program, function, LirOp_Extract, 8, &text, 1, 1, span);
    int used = assembly_indexed(program, function, LirOp_Extract, 8, &current, 1, 1, span);
    int zero = assembly_constant(program, function, 8, 0, span);
    int carried = assembly_carry(program, function, identity, current);
    int live = function->bindings.count;
    AssemblyBinding *before = assembly_snapshot(program, function, live);
    AssemblyBinding *answered = assembly_template(program, before, live, 1);
    AssemblyBinding *counted = assembly_template(program, before, live, 8);
    if(program->failed || before == NULL || answered == NULL || counted == NULL) return -1;
    int nothing = assembly_block(program, function, before, live);
    int work = assembly_block(program, function, before, live);
    int room = assembly_block(program, function, before, live);
    int refused = assembly_block(program, function, before, live);
    int header = assembly_block(program, function, counted, live + 1);
    int body = assembly_block(program, function, counted, live + 1);
    int copied = assembly_block(program, function, counted, live + 1);
    int done = assembly_block(program, function, answered, live + 1);
    if(program->failed) return -1;
    int empty_operands[] = {length, zero};
    int empty = assembly_emit(program, function, LirOp_Equal, 1, empty_operands, 2, -1, span);
    assembly_branch(program, function, empty, nothing, work, live, span);

    assembly_enter(function, nothing, before, live);
    assembly_carry(program, function, 1, assembly_constant(program, function, 1, 1, span));
    assembly_jump(program, function, done, live + 1, span);

    assembly_enter(function, work, before, live);
    int needed_operands[] = {used, length};
    int needed = assembly_emit(program, function, LirOp_Add, 8, needed_operands, 2, -1, span);
    int reserved = assembly_vec_reserve(program, function, carried, needed, identity, span);
    assembly_branch(program, function, reserved, room, refused, live, span);

    assembly_enter(function, refused, before, live);
    assembly_carry(program, function, 1, assembly_constant(program, function, 1, 0, span));
    assembly_jump(program, function, done, live + 1, span);

    assembly_enter(function, room, before, live);
    int grown = assembly_carried(function, carried);
    int target = assembly_indexed(program, function, LirOp_Extract, pointer, &grown, 1, 0, span);
    assembly_carry(program, function, 8, zero);
    assembly_jump(program, function, header, live + 1, span);

    assembly_enter(function, header, counted, live + 1);
    assembly_emit(program, function, LirOp_Fuel, 0, NULL, 0, -1, span);
    int more_operands[] = {assembly_carried(function, live), length};
    int more = assembly_emit(program, function, LirOp_Less, 1, more_operands, 2, -1, span);
    assembly_branch(program, function, more, body, copied, live + 1, span);

    assembly_enter(function, body, counted, live + 1);
    int at = assembly_carried(function, live);
    int from_operands[] = {bytes, at, length};
    int from = assembly_emit(program, function, LirOp_ElementAddress, pointer, from_operands, 3, -1, span);
    int byte = assembly_emit(program, function, LirOp_Load, 3, &from, 1, -1, span);
    int offset_operands[] = {used, at};
    int to_operands[] = {target, assembly_emit(program, function, LirOp_Add, 8, offset_operands, 2, -1, span), needed};
    int to = assembly_emit(program, function, LirOp_ElementAddress, pointer, to_operands, 3, -1, span);
    int store_operands[] = {to, byte};
    assembly_emit(program, function, LirOp_Store, 0, store_operands, 2, -1, span);
    int next_operands[] = {at, assembly_constant(program, function, 8, 1, span)};
    ((AssemblyBinding *)function->bindings.data)[live].value =
        assembly_emit(program, function, LirOp_Add, 8, next_operands, 2, -1, span);
    assembly_jump(program, function, header, live + 1, span);

    assembly_enter(function, copied, counted, live + 1);
    function->bindings.count = live;
    int count_operands[] = {assembly_carried(function, carried), needed};
    ((AssemblyBinding *)function->bindings.data)[carried].value =
        assembly_indexed(program, function, LirOp_Insert, identity, count_operands, 2, 1, span);
    assembly_carry(program, function, 1, assembly_constant(program, function, 1, 1, span));
    assembly_jump(program, function, done, live + 1, span);

    if(program->failed) return -1;
    assembly_enter(function, done, answered, live + 1);
    int final = assembly_carried(function, carried);
    int answer = assembly_carried(function, live);
    function->bindings.count = carried;
    assembly_vec_write(program, function, &place, final, span);
    return answer;
}

/* BuilderFinish(builder): the string keeps the builder's bytes, and the
 * builder is left empty without freeing them. An empty builder frees its
 * reserved block and gives the empty string. */
static int
assembly_builder_finish(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression)
{
    ZirSourceSpan span = expression->span;
    int first = expression->first_child;
    if(first < 0 || first >= function->source->expr_count)
        return assembly_fail(program, span, "BuilderFinish requires a builder");
    int identity = assembly_type(program, function, function->source->exprs[first].type, span);
    int string = assembly_string_type(program, span);
    if(program->failed) return -1;
    if(assembly_vec_element(program, identity) != 3)
        return assembly_fail(program, span, "BuilderFinish requires a Vec(u8) builder");
    int pointer = assembly_pointer_type(program, 3, span);
    AssemblyVecPlace place;
    int current = assembly_vec_read(program, function, first, identity, &place, span);
    if(program->failed) return -1;
    int data = assembly_indexed(program, function, LirOp_Extract, pointer, &current, 1, 0, span);
    int count = assembly_indexed(program, function, LirOp_Extract, 8, &current, 1, 1, span);
    int capacity = assembly_indexed(program, function, LirOp_Extract, 8, &current, 1, 2, span);
    int zero = assembly_constant(program, function, 8, 0, span);
    int live = function->bindings.count;
    AssemblyBinding *before = assembly_snapshot(program, function, live);
    AssemblyBinding *answered = assembly_template(program, before, live, pointer);
    if(program->failed || (live && before == NULL) || answered == NULL) return -1;
    int unused = assembly_block(program, function, before, live);
    int kept = assembly_block(program, function, before, live);
    int done = assembly_block(program, function, answered, live + 1);
    if(program->failed) return -1;
    int empty_operands[] = {count, zero};
    int empty = assembly_emit(program, function, LirOp_Equal, 1, empty_operands, 2, -1, span);
    assembly_branch(program, function, empty, unused, kept, live, span);

    assembly_enter(function, unused, before, live);
    int release_operands[] = {data, capacity, zero};
    assembly_carry(program, function, pointer,
        assembly_emit(program, function, LirOp_Resize, pointer, release_operands, 3, -1, span));
    assembly_jump(program, function, done, live + 1, span);

    assembly_enter(function, kept, before, live);
    assembly_carry(program, function, pointer, data);
    assembly_jump(program, function, done, live + 1, span);

    if(program->failed) return -1;
    assembly_enter(function, done, answered, live + 1);
    int fields[] = {assembly_carried(function, live), count};
    function->bindings.count = live;
    assembly_vec_write(program, function, &place, assembly_zero(program, function, identity, span, 0), span);
    return assembly_emit(program, function, LirOp_Aggregate, string, fields, 2, -1, span);
}

/* New(T): one zeroed T on the heap, or null when it cannot be had. */
static int
assembly_new(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression, int type)
{
    ZirSourceSpan span = expression->span;
    if(assembly_type_at(program, type)->kind != LirTypeKind_Pointer)
        return assembly_fail(program, span, "New requires a pointer result");
    int operands[] = {assembly_emit(program, function, LirOp_Null, type, NULL, 0, -1, span),
        assembly_constant(program, function, 8, 0, span), assembly_constant(program, function, 8, 1, span)};
    return assembly_emit(program, function, LirOp_Resize, type, operands, 3, -1, span);
}

/* free(pointer): gives back what New made; freeing null does nothing. */
static int
assembly_free(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression, int depth)
{
    ZirSourceSpan span = expression->span;
    if(expression->first_child < 0 || expression->first_child >= function->source->expr_count)
        return assembly_fail(program, span, "free requires a pointer");
    int pointer = assembly_expression(program, function, expression->first_child, depth + 1);
    if(program->failed) return -1;
    int type = ((int *)function->values.data)[pointer];
    if(assembly_type_at(program, type)->kind != LirTypeKind_Pointer)
        return assembly_fail(program, span, "free requires a pointer");
    int live = function->bindings.count;
    AssemblyBinding *before = assembly_snapshot(program, function, live);
    if(program->failed || (live && before == NULL)) return -1;
    int release = assembly_block(program, function, before, live);
    int done = assembly_block(program, function, before, live);
    if(program->failed) return -1;
    int null_operands[] = {pointer, assembly_emit(program, function, LirOp_Null, type, NULL, 0, -1, span)};
    int absent = assembly_emit(program, function, LirOp_Equal, 1, null_operands, 2, -1, span);
    assembly_branch(program, function, absent, done, release, live, span);

    assembly_enter(function, release, before, live);
    int operands[] = {pointer, assembly_constant(program, function, 8, 1, span),
        assembly_constant(program, function, 8, 0, span)};
    assembly_emit(program, function, LirOp_Resize, type, operands, 3, -1, span);
    assembly_jump(program, function, done, live, span);

    assembly_enter(function, done, before, live);
    return -1;
}

/* Store over elements [from, to) of a vector's block, dropping each one
 * first when `drop`: the element at the same index of the block `source`, or
 * the zero value when `source` is negative. `bound` is the block's capacity. */
static void
assembly_vec_reset(AssemblyProgram *program, AssemblyFunction *function, int data, int from, int to,
                   int bound, int element, int drop, int source, ZirSourceSpan span)
{
    if(program->failed || function->current < 0) return;
    int pointer = assembly_pointer_type(program, element, span);
    int base = function->bindings.count;
    AssemblyBinding cursor = {"", 8, from, -1};
    assembly_push(program, &function->bindings, &cursor);
    int live = base + 1;
    AssemblyBinding *before = assembly_snapshot(program, function, live);
    int header = assembly_block(program, function, before, live);
    int body = assembly_block(program, function, before, live);
    int exit = assembly_block(program, function, before, live);
    if(program->failed || before == NULL) return;
    assembly_jump(program, function, header, live, span);
    assembly_enter(function, header, before, live);
    assembly_emit(program, function, LirOp_Fuel, 0, NULL, 0, -1, span);
    int more_operands[] = {((AssemblyBinding *)function->bindings.data)[base].value, to};
    int more = assembly_emit(program, function, LirOp_Less, 1, more_operands, 2, -1, span);
    assembly_branch(program, function, more, body, exit, live, span);
    assembly_enter(function, body, before, live);
    int at = ((AssemblyBinding *)function->bindings.data)[base].value;
    int slot_operands[] = {data, at, bound};
    int slot = assembly_emit(program, function, LirOp_ElementAddress, pointer, slot_operands, 3, -1, span);
    if(drop)
        assembly_drop_value(program, function,
            assembly_emit(program, function, LirOp_Load, element, &slot, 1, -1, span), element, span, 0);
    int stored;
    if(source >= 0) {
        int from_operands[] = {source, at, bound};
        int from_slot = assembly_emit(program, function, LirOp_ElementAddress, pointer, from_operands, 3, -1, span);
        stored = assembly_emit(program, function, LirOp_Load, element, &from_slot, 1, -1, span);
    } else stored = assembly_zero(program, function, element, span, 0);
    int store_operands[] = {slot, stored};
    assembly_emit(program, function, LirOp_Store, 0, store_operands, 2, -1, span);
    int next_operands[] = {at, assembly_constant(program, function, 8, 1, span)};
    ((AssemblyBinding *)function->bindings.data)[base].value =
        assembly_emit(program, function, LirOp_Add, 8, next_operands, 2, -1, span);
    assembly_jump(program, function, header, live, span);
    assembly_enter(function, exit, before, live);
    function->bindings.count = base;
}

/* VecReserve(values, capacity): room for at least that many elements, with
 * the count unchanged. The portable VM gives exactly the requested capacity
 * when it grows; a negative request or a refused allocation gives false and
 * leaves the vector as it was. VecResize(values, count) reserves the same
 * way and then sets the count: elements it removes are dropped, and elements
 * it adds are zero. */
static int
assembly_vec_reserve_call(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression,
                          int resize, int depth)
{
    ZirSourceSpan span = expression->span;
    int first = expression->first_child;
    int second = first >= 0 && first < function->source->expr_count ?
        function->source->exprs[first].next_sibling : -1;
    if(second < 0 || second >= function->source->expr_count)
        return assembly_fail(program, span, "VecReserve requires a vector place and a capacity");
    int identity = assembly_type(program, function, function->source->exprs[first].type, span);
    int element = assembly_vec_element(program, identity);
    if(program->failed) return -1;
    if(element < 0) return assembly_fail(program, span, "VecReserve requires an owned vector");
    int pointer = assembly_pointer_type(program, element, span);
    int requested = assembly_convert(program, function,
        assembly_expression(program, function, second, depth + 1), 8, span);
    AssemblyVecPlace place;
    int current = assembly_vec_read(program, function, first, identity, &place, span);
    if(program->failed) return -1;
    int data = assembly_indexed(program, function, LirOp_Extract, pointer, &current, 1, 0, span);
    int capacity = assembly_indexed(program, function, LirOp_Extract, 8, &current, 1, 2, span);
    int zero = assembly_constant(program, function, 8, 0, span);
    int carried = assembly_carry(program, function, identity, current);
    int live = function->bindings.count;
    AssemblyBinding *before = assembly_snapshot(program, function, live);
    AssemblyBinding *answered = assembly_template(program, before, live, 1);
    if(program->failed || before == NULL || answered == NULL) return -1;
    int valid = assembly_block(program, function, before, live);
    int rejected = assembly_block(program, function, before, live);
    int fits = assembly_block(program, function, before, live);
    int grow = assembly_block(program, function, before, live);
    int granted = assembly_block(program, function, before, live);
    int done = assembly_block(program, function, answered, live + 1);
    if(program->failed) return -1;
    int negative_operands[] = {requested, zero};
    int negative = assembly_emit(program, function, LirOp_Less, 1, negative_operands, 2, -1, span);
    assembly_branch(program, function, negative, rejected, valid, live, span);

    assembly_enter(function, rejected, before, live);
    assembly_carry(program, function, 1, assembly_constant(program, function, 1, 0, span));
    assembly_jump(program, function, done, live + 1, span);

    assembly_enter(function, valid, before, live);
    int enough_operands[] = {requested, capacity};
    int enough = assembly_emit(program, function, LirOp_LessEqual, 1, enough_operands, 2, -1, span);
    assembly_branch(program, function, enough, fits, grow, live, span);

    assembly_enter(function, fits, before, live);
    assembly_carry(program, function, 1, assembly_constant(program, function, 1, 1, span));
    assembly_jump(program, function, done, live + 1, span);

    assembly_enter(function, grow, before, live);
    int resize_operands[] = {data, capacity, requested};
    int moved = assembly_emit(program, function, LirOp_Resize, pointer, resize_operands, 3, -1, span);
    int failed_operands[] = {moved, assembly_emit(program, function, LirOp_Null, pointer, NULL, 0, -1, span)};
    int failed = assembly_emit(program, function, LirOp_Equal, 1, failed_operands, 2, -1, span);
    assembly_branch(program, function, failed, rejected, granted, live, span);

    assembly_enter(function, granted, before, live);
    int data_operands[] = {assembly_carried(function, carried), moved};
    int with_data = assembly_indexed(program, function, LirOp_Insert, identity, data_operands, 2, 0, span);
    int capacity_operands[] = {with_data, requested};
    ((AssemblyBinding *)function->bindings.data)[carried].value =
        assembly_indexed(program, function, LirOp_Insert, identity, capacity_operands, 2, 2, span);
    assembly_carry(program, function, 1, assembly_constant(program, function, 1, 1, span));
    assembly_jump(program, function, done, live + 1, span);

    if(program->failed) return -1;
    assembly_enter(function, done, answered, live + 1);
    if(resize) {
        int all = live + 1;
        AssemblyBinding *settled = assembly_snapshot(program, function, all);
        int sizing = assembly_block(program, function, settled, all);
        int finished = assembly_block(program, function, settled, all);
        if(program->failed || settled == NULL) return -1;
        assembly_branch(program, function, assembly_carried(function, live), sizing, finished, all, span);

        assembly_enter(function, sizing, settled, all);
        int vector = assembly_carried(function, carried);
        int block = assembly_indexed(program, function, LirOp_Extract, pointer, &vector, 1, 0, span);
        int count = assembly_indexed(program, function, LirOp_Extract, 8, &vector, 1, 1, span);
        int room = assembly_indexed(program, function, LirOp_Extract, 8, &vector, 1, 2, span);
        if(assembly_owned(program, element, 0))
            assembly_vec_reset(program, function, block, requested, count, room, element, 1, -1, span);
        assembly_vec_reset(program, function, block, count, requested, room, element, 0, -1, span);
        int count_operands[] = {assembly_carried(function, carried), requested};
        ((AssemblyBinding *)function->bindings.data)[carried].value =
            assembly_indexed(program, function, LirOp_Insert, identity, count_operands, 2, 1, span);
        assembly_jump(program, function, finished, all, span);

        if(program->failed) return -1;
        assembly_enter(function, finished, settled, all);
    }
    int final = assembly_carried(function, carried);
    int answer = assembly_carried(function, live);
    function->bindings.count = carried;
    assembly_vec_write(program, function, &place, final, span);
    return answer;
}

/* VecClone(destination, source): the destination becomes a copy of the
 * source's elements with exactly that many as its capacity, and what it held
 * is dropped. When the copy cannot be allocated the answer is false and the
 * destination is unchanged. */
static int
assembly_vec_clone(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression, int depth)
{
    ZirSourceSpan span = expression->span;
    int first = expression->first_child;
    int second = first >= 0 && first < function->source->expr_count ?
        function->source->exprs[first].next_sibling : -1;
    if(second < 0 || second >= function->source->expr_count)
        return assembly_fail(program, span, "VecClone requires a destination and a source vector");
    int identity = assembly_type(program, function, function->source->exprs[first].type, span);
    int element = assembly_vec_element(program, identity);
    if(program->failed) return -1;
    if(element < 0) return assembly_fail(program, span, "VecClone requires an owned vector");
    if(assembly_owned(program, element, 0))
        return assembly_fail(program, span, "VecClone of elements that own storage is not lowered yet");
    int pointer = assembly_pointer_type(program, element, span);
    int source = assembly_convert(program, function,
        assembly_expression(program, function, second, depth + 1), identity, span);
    AssemblyVecPlace place;
    int current = assembly_vec_read(program, function, first, identity, &place, span);
    if(program->failed) return -1;
    int from = assembly_indexed(program, function, LirOp_Extract, pointer, &source, 1, 0, span);
    int count = assembly_indexed(program, function, LirOp_Extract, 8, &source, 1, 1, span);
    int zero = assembly_constant(program, function, 8, 0, span);
    int empty = assembly_emit(program, function, LirOp_Null, pointer, NULL, 0, -1, span);
    int carried = assembly_carry(program, function, identity, current);
    int live = function->bindings.count;
    AssemblyBinding *before = assembly_snapshot(program, function, live);
    AssemblyBinding *copied = assembly_template(program, before, live, pointer);
    AssemblyBinding *answered = assembly_template(program, before, live, 1);
    if(program->failed || before == NULL || copied == NULL || answered == NULL) return -1;
    int allocate = assembly_block(program, function, before, live);
    int nothing = assembly_block(program, function, before, live);
    int copy = assembly_block(program, function, before, live);
    int rejected = assembly_block(program, function, before, live);
    int publish = assembly_block(program, function, copied, live + 1);
    int done = assembly_block(program, function, answered, live + 1);
    if(program->failed) return -1;
    int some_operands[] = {zero, count};
    int some = assembly_emit(program, function, LirOp_Less, 1, some_operands, 2, -1, span);
    assembly_branch(program, function, some, allocate, nothing, live, span);

    assembly_enter(function, nothing, before, live);
    assembly_carry(program, function, pointer, empty);
    assembly_jump(program, function, publish, live + 1, span);

    assembly_enter(function, allocate, before, live);
    int resize_operands[] = {empty, zero, count};
    int block = assembly_emit(program, function, LirOp_Resize, pointer, resize_operands, 3, -1, span);
    int failed_operands[] = {block, empty};
    int failed = assembly_emit(program, function, LirOp_Equal, 1, failed_operands, 2, -1, span);
    assembly_branch(program, function, failed, rejected, copy, live, span);

    assembly_enter(function, rejected, before, live);
    assembly_carry(program, function, 1, assembly_constant(program, function, 1, 0, span));
    assembly_jump(program, function, done, live + 1, span);

    assembly_enter(function, copy, before, live);
    assembly_vec_reset(program, function, block, zero, count, count, element, 0, from, span);
    assembly_carry(program, function, pointer, block);
    assembly_jump(program, function, publish, live + 1, span);

    if(program->failed) return -1;
    assembly_enter(function, publish, copied, live + 1);
    int data = assembly_carried(function, live);
    function->bindings.count = live;
    assembly_drop_value(program, function, assembly_carried(function, carried), identity, span, 0);
    int fields[] = {data, count, count};
    ((AssemblyBinding *)function->bindings.data)[carried].value =
        assembly_emit(program, function, LirOp_Aggregate, identity, fields, 3, -1, span);
    assembly_carry(program, function, 1, assembly_constant(program, function, 1, 1, span));
    assembly_jump(program, function, done, live + 1, span);

    if(program->failed) return -1;
    assembly_enter(function, done, answered, live + 1);
    int final = assembly_carried(function, carried);
    int answer = assembly_carried(function, live);
    function->bindings.count = carried;
    assembly_vec_write(program, function, &place, final, span);
    return answer;
}

/* VecGet(values, index) and VecPop(values): an Option that holds the element
 * when there is one. VecGet copies it and leaves the vector alone, with
 * nothing for an index at or past the count; VecPop moves the last element
 * out and shortens the vector by one. */
static int
assembly_vec_take(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression,
                  int option, int get, int depth)
{
    ZirSourceSpan span = expression->span;
    int first = expression->first_child;
    int second = first >= 0 && first < function->source->expr_count ?
        function->source->exprs[first].next_sibling : -1;
    if(first < 0 || first >= function->source->expr_count ||
       (get && (second < 0 || second >= function->source->expr_count)))
        return assembly_fail(program, span, "VecGet and VecPop require a vector place");
    int identity = assembly_type(program, function, function->source->exprs[first].type, span);
    int element = assembly_vec_element(program, identity);
    if(program->failed) return -1;
    LirType shape = *assembly_type_at(program, option);
    if(element < 0 || shape.kind != LirTypeKind_Record || shape.fields.length != 2 ||
       ((int *)shape.fields.data)[0] != 1 || ((int *)shape.fields.data)[1] != element)
        return assembly_fail(program, span, "VecGet and VecPop require an owned vector and its Option");
    if(get && assembly_owned(program, element, 0))
        return assembly_fail(program, span, "VecGet of an element that owns storage is not lowered yet");
    int pointer = assembly_pointer_type(program, element, span);
    int at = get ? assembly_convert(program, function,
        assembly_expression(program, function, second, depth + 1), 8, span) : -1;
    AssemblyVecPlace place;
    int current = assembly_vec_read(program, function, first, identity, &place, span);
    if(program->failed) return -1;
    int data = assembly_indexed(program, function, LirOp_Extract, pointer, &current, 1, 0, span);
    int count = assembly_indexed(program, function, LirOp_Extract, 8, &current, 1, 1, span);
    int zero = assembly_constant(program, function, 8, 0, span);
    int carried = assembly_carry(program, function, identity, current);
    int live = function->bindings.count;
    AssemblyBinding *before = assembly_snapshot(program, function, live);
    AssemblyBinding *answered = assembly_template(program, before, live, option);
    if(program->failed || before == NULL || answered == NULL) return -1;
    int present = assembly_block(program, function, before, live);
    int absent = assembly_block(program, function, before, live);
    int done = assembly_block(program, function, answered, live + 1);
    if(program->failed) return -1;
    /* A negative index compares below the count and then fails the element
     * address, as it fails on the portable VM. */
    int some_operands[] = {get ? at : zero, count};
    int some = assembly_emit(program, function, LirOp_Less, 1, some_operands, 2, -1, span);
    assembly_branch(program, function, some, present, absent, live, span);

    assembly_enter(function, present, before, live);
    if(!get) {
        int last_operands[] = {count, assembly_constant(program, function, 8, 1, span)};
        at = assembly_emit(program, function, LirOp_Subtract, 8, last_operands, 2, -1, span);
    }
    int slot_operands[] = {data, at, count};
    int slot = assembly_emit(program, function, LirOp_ElementAddress, pointer, slot_operands, 3, -1, span);
    int item = assembly_emit(program, function, LirOp_Load, element, &slot, 1, -1, span);
    if(!get) {
        int shorter_operands[] = {assembly_carried(function, carried), at};
        ((AssemblyBinding *)function->bindings.data)[carried].value =
            assembly_indexed(program, function, LirOp_Insert, identity, shorter_operands, 2, 1, span);
    }
    int fields[] = {assembly_constant(program, function, 1, 1, span), item};
    assembly_carry(program, function, option,
        assembly_emit(program, function, LirOp_Aggregate, option, fields, 2, -1, span));
    assembly_jump(program, function, done, live + 1, span);

    assembly_enter(function, absent, before, live);
    assembly_carry(program, function, option, assembly_zero(program, function, option, span, 0));
    assembly_jump(program, function, done, live + 1, span);

    if(program->failed) return -1;
    assembly_enter(function, done, answered, live + 1);
    int final = assembly_carried(function, carried);
    int answer = assembly_carried(function, live);
    function->bindings.count = carried;
    if(!get) assembly_vec_write(program, function, &place, final, span);
    return answer;
}

/* VecSwap(left, right): the two places exchange their vectors; no element
 * moves and nothing is allocated. */
static int
assembly_vec_swap(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression)
{
    ZirSourceSpan span = expression->span;
    int first = expression->first_child;
    int second = first >= 0 && first < function->source->expr_count ?
        function->source->exprs[first].next_sibling : -1;
    if(second < 0 || second >= function->source->expr_count)
        return assembly_fail(program, span, "VecSwap requires two vector places");
    int identity = assembly_type(program, function, function->source->exprs[first].type, span);
    int other = assembly_type(program, function, function->source->exprs[second].type, span);
    if(program->failed) return -1;
    if(assembly_vec_element(program, identity) < 0 || other != identity)
        return assembly_fail(program, span, "VecSwap requires two vectors of one element type");
    AssemblyVecPlace left_place, right_place;
    int left = assembly_vec_read(program, function, first, identity, &left_place, span);
    assembly_vec_write(program, function, &left_place,
        assembly_vec_read(program, function, second, identity, &right_place, span), span);
    /* The right place is captured again: writing the left one may have
     * rebuilt an aggregate both places live in. */
    assembly_vec_read(program, function, second, identity, &right_place, span);
    assembly_vec_write(program, function, &right_place, left, span);
    return -1;
}

/* VecSlice(values, low, high): a borrowed view of the vector's elements,
 * bounds-checked against its count like any slice of a slice. */
static int
assembly_vec_slice(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression, int depth)
{
    ZirSourceSpan span = expression->span;
    int first = expression->first_child;
    int second = first >= 0 && first < function->source->expr_count ?
        function->source->exprs[first].next_sibling : -1;
    int third = second >= 0 && second < function->source->expr_count ?
        function->source->exprs[second].next_sibling : -1;
    if(third < 0 || third >= function->source->expr_count)
        return assembly_fail(program, span, "VecSlice requires a vector, a low bound and a high bound");
    int vector = assembly_expression(program, function, first, depth + 1);
    if(program->failed) return -1;
    int identity = ((int *)function->values.data)[vector];
    int element = assembly_vec_element(program, identity);
    if(element < 0) return assembly_fail(program, span, "VecSlice requires an owned vector");
    int pointer = assembly_pointer_type(program, element, span);
    int data = assembly_indexed(program, function, LirOp_Extract, pointer, &vector, 1, 0, span);
    int count = assembly_indexed(program, function, LirOp_Extract, 8, &vector, 1, 1, span);
    int low = assembly_convert(program, function,
        assembly_expression(program, function, second, depth + 1), 8, span);
    int high = assembly_convert(program, function,
        assembly_expression(program, function, third, depth + 1), 8, span);
    int result_type = assembly_type(program, function, expression->type, span);
    if(program->failed) return -1;
    int address_operands[] = {data, low, high, count};
    int selected = assembly_emit(program, function, LirOp_ElementAddress, pointer, address_operands, 4, -1, span);
    int count_operands[] = {high, low};
    int selected_count = assembly_emit(program, function, LirOp_Subtract, 8, count_operands, 2, -1, span);
    int fields[] = {selected, selected_count};
    return assembly_emit(program, function, LirOp_Aggregate, result_type, fields, 2, -1, span);
}

/* VecPush, VecClear and VecFree on a vector place. Growth follows the C
 * runtime: capacity 0 becomes 8, a full vector doubles, and a refused
 * allocation leaves the vector unchanged and gives false. */
static int
assembly_vec_call(AssemblyProgram *program, AssemblyFunction *function, const ZirExpr *expression, int depth)
{
    ZirSourceSpan span = expression->span;
    int first = expression->first_child;
    if(first < 0 || first >= function->source->expr_count)
        return assembly_fail(program, span, "vector operation requires a vector place");
    const ZirExpr *source = &function->source->exprs[first];
    int identity = assembly_type(program, function, source->type, span);
    int element = assembly_vec_element(program, identity);
    if(program->failed) return -1;
    if(element < 0) return assembly_fail(program, span, "vector operation requires an owned vector");
    int pointer = assembly_pointer_type(program, element, span);
    int push = !strcmp(expression->name, "VecPush");
    int item = -1;
    if(push) {
        int second = source->next_sibling;
        if(second < 0 || second >= function->source->expr_count)
            return assembly_fail(program, span, "VecPush requires a value");
        item = assembly_convert(program, function,
            assembly_expression(program, function, second, depth + 1), element, span);
    }
    AssemblyVecPlace place;
    int current = assembly_vec_read(program, function, first, identity, &place, span);
    if(program->failed) return -1;
    int zero = assembly_constant(program, function, 8, 0, span);
    if(!strcmp(expression->name, "VecClear")) {
        assembly_drop_elements(program, function, current, identity, span, 0);
        int operands[] = {current, zero};
        assembly_vec_write(program, function, &place,
            assembly_indexed(program, function, LirOp_Insert, identity, operands, 2, 1, span), span);
        return -1;
    }
    if(!strcmp(expression->name, "VecFree")) {
        assembly_drop_value(program, function, current, identity, span, 0);
        assembly_vec_write(program, function, &place, assembly_zero(program, function, identity, span, 0), span);
        return -1;
    }
    int data = assembly_indexed(program, function, LirOp_Extract, pointer, &current, 1, 0, span);
    int count = assembly_indexed(program, function, LirOp_Extract, 8, &current, 1, 1, span);
    int capacity = assembly_indexed(program, function, LirOp_Extract, 8, &current, 1, 2, span);
    /* The vector travels through the branches as one unnamed binding. */
    int base = function->bindings.count;
    AssemblyBinding carried = {"", identity, current, -1};
    assembly_push(program, &function->bindings, &carried);
    int live = base + 1;
    AssemblyBinding *before = assembly_snapshot(program, function, live);
    AssemblyBinding *joined = assembly_allocate(program, (size_t)live + 1, sizeof(*joined));
    if(program->failed || before == NULL || joined == NULL) return -1;
    memcpy(joined, before, (size_t)live * sizeof(*joined));
    joined[live] = (AssemblyBinding){"", 1, -1, -1};
    int grow = assembly_block(program, function, before, live);
    int grown = assembly_block(program, function, before, live);
    int refused = assembly_block(program, function, before, live);
    int store = assembly_block(program, function, before, live);
    int done = assembly_block(program, function, joined, live + 1);
    if(program->failed) return -1;
    int full_operands[] = {count, capacity};
    int full = assembly_emit(program, function, LirOp_GreaterEqual, 1, full_operands, 2, -1, span);
    assembly_branch(program, function, full, grow, store, live, span);

    assembly_enter(function, grow, before, live);
    int empty_operands[] = {capacity, zero};
    int empty = assembly_emit(program, function, LirOp_Equal, 1, empty_operands, 2, -1, span);
    int initial_operands[] = {assembly_emit(program, function, LirOp_Convert, 8, &empty, 1, -1, span),
        assembly_constant(program, function, 8, 8, span)};
    int initial = assembly_emit(program, function, LirOp_Multiply, 8, initial_operands, 2, -1, span);
    int doubled_operands[] = {capacity, capacity};
    int doubled = assembly_emit(program, function, LirOp_Add, 8, doubled_operands, 2, -1, span);
    int next_operands[] = {doubled, initial};
    int next = assembly_emit(program, function, LirOp_Add, 8, next_operands, 2, -1, span);
    int resize_operands[] = {data, capacity, next};
    int moved = assembly_emit(program, function, LirOp_Resize, pointer, resize_operands, 3, -1, span);
    int failed_operands[] = {moved, assembly_emit(program, function, LirOp_Null, pointer, NULL, 0, -1, span)};
    int failed = assembly_emit(program, function, LirOp_Equal, 1, failed_operands, 2, -1, span);
    assembly_branch(program, function, failed, refused, grown, live, span);

    assembly_enter(function, grown, before, live);
    {
        int data_operands[] = {((AssemblyBinding *)function->bindings.data)[base].value, moved};
        int with_data = assembly_indexed(program, function, LirOp_Insert, identity, data_operands, 2, 0, span);
        int capacity_operands[] = {with_data, next};
        ((AssemblyBinding *)function->bindings.data)[base].value =
            assembly_indexed(program, function, LirOp_Insert, identity, capacity_operands, 2, 2, span);
        assembly_jump(program, function, store, live, span);
    }

    assembly_enter(function, refused, before, live);
    {
        AssemblyBinding result = {"", 1, assembly_constant(program, function, 1, 0, span), -1};
        assembly_push(program, &function->bindings, &result);
        assembly_jump(program, function, done, live + 1, span);
    }

    assembly_enter(function, store, before, live);
    {
        int vector = ((AssemblyBinding *)function->bindings.data)[base].value;
        int here = assembly_indexed(program, function, LirOp_Extract, pointer, &vector, 1, 0, span);
        int used = assembly_indexed(program, function, LirOp_Extract, 8, &vector, 1, 1, span);
        int more_operands[] = {used, assembly_constant(program, function, 8, 1, span)};
        int more = assembly_emit(program, function, LirOp_Add, 8, more_operands, 2, -1, span);
        int slot_operands[] = {here, used, more};
        int slot = assembly_emit(program, function, LirOp_ElementAddress, pointer, slot_operands, 3, -1, span);
        int store_operands[] = {slot, item};
        assembly_emit(program, function, LirOp_Store, 0, store_operands, 2, -1, span);
        int count_operands[] = {vector, more};
        ((AssemblyBinding *)function->bindings.data)[base].value =
            assembly_indexed(program, function, LirOp_Insert, identity, count_operands, 2, 1, span);
        AssemblyBinding result = {"", 1, assembly_constant(program, function, 1, 1, span), -1};
        assembly_push(program, &function->bindings, &result);
        assembly_jump(program, function, done, live + 1, span);
    }

    if(program->failed) return -1;
    assembly_enter(function, done, joined, live + 1);
    int final = ((AssemblyBinding *)function->bindings.data)[base].value;
    int pushed = ((AssemblyBinding *)function->bindings.data)[base + 1].value;
    function->bindings.count = base;
    assembly_vec_write(program, function, &place, final, span);
    return pushed;
}

static int assembly_expression_value(AssemblyProgram *, AssemblyFunction *, int, int);

/* The checker marks each move. A move reads the owned value and leaves its
 * source empty, as the C emitter's clear_owned_value does, so the source's
 * drop at scope exit frees nothing. */
static int
assembly_expression(AssemblyProgram *program, AssemblyFunction *function, int root, int depth)
{
    int value = assembly_expression_value(program, function, root, depth);
    if(program->failed || value < 0 || root < 0 || root >= function->source->expr_count ||
       !function->source->exprs[root].is_move) return value;
    assembly_move_out(program, function, root, ((int *)function->values.data)[value],
        function->source->exprs[root].span);
    return value;
}

/* size_of(T) is the C layout size, as the C target gives it: each scalar is
 * aligned to its own size and a record to its widest member. */
static int64_t
assembly_layout_size(AssemblyProgram *program, int identity, int64_t *alignment, int depth)
{
    LirType type = *assembly_type_at(program, identity);
    int64_t size = 0, align = 1;
    if(depth > 256) return -1;
    if(type.kind == LirTypeKind_Bool) size = 1;
    else if(type.kind == LirTypeKind_Integer || type.kind == LirTypeKind_Float) size = align = type.bits / 8;
    else if(type.kind == LirTypeKind_Pointer || type.kind == LirTypeKind_Function) size = align = 8;
    else if(type.kind == LirTypeKind_Array) {
        int64_t element = assembly_layout_size(program, type.element, &align, depth + 1);
        if(element < 0 || (type.count && element > INT64_MAX / type.count)) return -1;
        size = element * type.count;
    } else if(type.kind == LirTypeKind_Record) {
        for(int index = 0; index < type.fields.length; index++) {
            int64_t field_align = 1;
            int64_t field = assembly_layout_size(program, ((int *)type.fields.data)[index], &field_align, depth + 1);
            if(field < 0 || size > INT64_MAX - field - field_align) return -1;
            size = (size + field_align - 1) / field_align * field_align + field;
            if(field_align > align) align = field_align;
        }
        size = (size + align - 1) / align * align;
    } else return -1;
    *alignment = align;
    return size;
}

/* The lowered type of member `name` when `type` is a union, else -1. */
static int
assembly_union_member(AssemblyProgram *program, AssemblyFunction *function, const char *type,
                      const char *name, ZirSourceSpan span)
{
    const ZirModule *owner = function->module;
    const ZirType *record = type && *type ? FindType(function->module, type, &owner) : NULL;
    if(record == NULL || !record->is_union) return -1;
    size_t cursor = 0;
    ZirTypeField field;
    while(TypeNextField(record, &cursor, &field) == 1)
        if(!strcmp(field.name, name)) return assembly_type_in(program, owner, field.type, span, 0);
    return assembly_fail(program, span, "union has no such member");
}

static int
assembly_unsigned_of(int bits)
{
    return bits == 8 ? 3 : bits == 16 ? 5 : bits == 32 ? 7 : 9;
}

/* Read member type `member` out of the union value `whole`: its low bytes,
 * reinterpreted. */
static int
assembly_union_read(AssemblyProgram *program, AssemblyFunction *function, int whole, int member, ZirSourceSpan span)
{
    LirType part = *assembly_type_at(program, member);
    int bits = assembly_convert(program, function, whole, assembly_unsigned_of(part.bits), span);
    if(part.kind == LirTypeKind_Float)
        return assembly_emit(program, function, LirOp_Bitcast, member, &bits, 1, -1, span);
    return assembly_convert(program, function, bits, member, span);
}

/* The union value `old` with `value`, of member type `member`, written over
 * its low bytes; the bytes above the member stay. */
static int
assembly_union_write(AssemblyProgram *program, AssemblyFunction *function, int old, int value,
                     int whole, int member, ZirSourceSpan span)
{
    LirType part = *assembly_type_at(program, member);
    int narrow = assembly_unsigned_of(part.bits);
    int bits = part.kind == LirTypeKind_Float ?
        assembly_emit(program, function, LirOp_Bitcast, narrow, &value, 1, -1, span) :
        assembly_convert(program, function, value, narrow, span);
    int widened = assembly_convert(program, function, bits, whole, span);
    if(part.bits == assembly_type_at(program, whole)->bits) return widened;
    int kept_operands[] = {old, assembly_constant(program, function, whole,
        ~((UINT64_C(1) << part.bits) - 1), span)};
    int merged_operands[] = {assembly_emit(program, function, LirOp_BitAnd, whole, kept_operands, 2, -1, span), widened};
    return assembly_emit(program, function, LirOp_BitOr, whole, merged_operands, 2, -1, span);
}

/* A bare `null` has no type of its own in checked ZIR: it is the null of the
 * pointer or procedure type it is passed, assigned or returned as. */
static int
assembly_expression_for(AssemblyProgram *program, AssemblyFunction *function, int root, int wanted, int depth)
{
    if(!program->failed && root >= 0 && root < function->source->expr_count && wanted > 0) {
        const ZirExpr *expression = &function->source->exprs[root];
        LirTypeKind kind = assembly_type_at(program, wanted)->kind;
        if(expression->kind == ZIR_EXPR_IDENT && !expression->is_global_value &&
           !strcmp(expression->name, "null") && !strcmp(expression->type, "null") &&
           (kind == LirTypeKind_Pointer || kind == LirTypeKind_Function))
            return assembly_emit(program, function, LirOp_Null, wanted, NULL, 0, -1, expression->span);
    }
    return assembly_expression(program, function, root, depth);
}

static int
assembly_expression_value(AssemblyProgram *program, AssemblyFunction *function, int root, int depth)
{
    if(!assembly_spend(program, function->source->span)) return -1;
    if(depth > 256 || root < 0 || root >= function->source->expr_count)
        return assembly_fail(program, function->source->span, "invalid or excessively deep expression graph");
    const ZirExpr *expression = &function->source->exprs[root];
    int type = assembly_type(program, function, expression->type, expression->span);
    if(program->failed) return -1;
    if(expression->is_function_value) {
        const ZirModule *owner = NULL;
        const ZirFunction *callee = NULL;
        if(assembly_type_at(program, type)->kind != LirTypeKind_Function ||
           ResolveFunctionAt(function->module, expression->name, SpanPath(expression->span), &owner, &callee) != 1)
            return assembly_fail(program, expression->span, "procedure value is not a concrete portable function");
        if(callee->is_extern)
            return assembly_fail(program, expression->span, "host and foreign procedures cannot become portable procedure values");
        int symbol = assembly_function(program, owner, callee);
        if(program->failed) return -1;
        return assembly_emit(program, function, LirOp_FunctionAddress, type, NULL, 0, symbol, expression->span);
    }
    if(expression->kind == ZIR_EXPR_STRING) return assembly_string(program, function, expression);
    if(expression->kind == ZIR_EXPR_SIZE_OF) {
        int measured = assembly_type(program, function, expression->name, expression->span);
        if(program->failed) return -1;
        int64_t alignment = 1;
        int64_t size = assembly_layout_size(program, measured, &alignment, 0);
        if(size < 0) return assembly_fail(program, expression->span, "size_of this type has no portable lowered size");
        return assembly_constant(program, function, type, (uint64_t)size, expression->span);
    }
    /* A parsed global initializer leaves its numeric literals untyped; each
     * takes the type of the member or global it initializes. */
    if(function->literal_at == root + 1 &&
       (expression->kind == ZIR_EXPR_INT || expression->kind == ZIR_EXPR_FLOAT)) {
        LirTypeKind own = assembly_type_at(program, type)->kind;
        if(own != LirTypeKind_Float && (expression->kind == ZIR_EXPR_FLOAT ||
           (own != LirTypeKind_Integer && own != LirTypeKind_Bool)))
            type = function->literal_type;
    }
    if(expression->kind == ZIR_EXPR_INT) {
        /* A folded constant such as a negative enum member carries its sign
         * in the text: the magnitude is read, then negated in two's complement. */
        unsigned long long bits = 0;
        const char *digits = expression->text;
        int negative = digits[0] == '-';
        if(negative || digits[0] == '+') digits++;
        if(!ZirIntegerLiteral(digits, &bits))
            return assembly_fail(program, expression->span, "invalid checked integer literal");
        if(assembly_type_at(program, type)->kind == LirTypeKind_Float) {
            double converted = negative ? -(double)bits : (double)bits;
            if(assembly_type_at(program, type)->bits == 32) {
                float narrowed = (float)converted;
                uint32_t representation;
                memcpy(&representation, &narrowed, sizeof(representation));
                bits = representation;
            } else memcpy(&bits, &converted, sizeof(bits));
        } else if(negative) bits = 0 - bits;
        return assembly_constant(program, function, type, bits, expression->span);
    }
    if(expression->kind == ZIR_EXPR_FLOAT) {
        char *end = NULL;
        double value = ParseDecimal(expression->text, &end);
        if(end == expression->text || *end || assembly_type_at(program, type)->kind != LirTypeKind_Float)
            return assembly_fail(program, expression->span, "invalid checked floating-point literal");
        uint64_t bits = 0;
        if(assembly_type_at(program, type)->bits == 32) {
            float narrowed = (float)value;
            uint32_t representation;
            memcpy(&representation, &narrowed, sizeof(representation));
            bits = representation;
        } else memcpy(&bits, &value, sizeof(bits));
        return assembly_constant(program, function, type, bits, expression->span);
    }
    if(expression->kind == ZIR_EXPR_COMPOUND)
        return assembly_compound(program, function, expression, type, depth + 1);
    if(expression->kind == ZIR_EXPR_SLICE)
        return assembly_lower_slice(program, function, expression, type, depth + 1);
    if(expression->kind == ZIR_EXPR_INDEX && assembly_addressable(program, function, root, depth + 1)) {
        int address = assembly_address(program, function, root, depth + 1);
        return assembly_emit(program, function, LirOp_Load, type, &address, 1, -1, expression->span);
    }
    if(expression->kind == ZIR_EXPR_POINTER_MEMBER) {
        int address = assembly_address(program, function, root, depth + 1);
        if(program->failed) return -1;
        return assembly_emit(program, function, LirOp_Load, type, &address, 1, -1, expression->span);
    }
    if(expression->kind == ZIR_EXPR_MEMBER && expression->left >= 0 &&
       expression->left < function->source->expr_count) {
        int member = assembly_union_member(program, function,
            function->source->exprs[expression->left].type, expression->name, expression->span);
        if(program->failed) return -1;
        if(member >= 0)
            return assembly_union_read(program, function,
                assembly_expression(program, function, expression->left, depth + 1), member, expression->span);
    }
    if(expression->kind == ZIR_EXPR_MEMBER || expression->kind == ZIR_EXPR_INDEX)
        return assembly_member(program, function, expression, depth + 1);
    if(expression->kind == ZIR_EXPR_IDENT) {
        if(expression->is_global_value) {
            int address = assembly_global_address(program, function, expression->name, expression->span);
            return assembly_emit(program, function, LirOp_Load, type, &address, 1, -1, expression->span);
        }
        if(!strcmp(expression->name, "null")) {
            LirTypeKind kind = assembly_type_at(program, type)->kind;
            if(kind != LirTypeKind_Pointer && kind != LirTypeKind_Function)
                return assembly_fail(program, expression->span, "null requires a pointer or procedure type");
            return assembly_emit(program, function, LirOp_Null, type, NULL, 0, -1, expression->span);
        }
        if(!strcmp(expression->name, "true") || !strcmp(expression->name, "false"))
            return assembly_constant(program, function, 1, !strcmp(expression->name, "true"), expression->span);
        int binding = assembly_binding(function, expression->name);
        if(binding >= 0) {
            AssemblyBinding *local = &((AssemblyBinding *)function->bindings.data)[binding];
            if(local->storage_type < 0) return local->value;
            return assembly_emit(program, function, LirOp_Load, local->type, &local->value, 1, -1,
                expression->span);
        }
        return assembly_fail(program, expression->span, "only local scalar bindings are lowered in identifier expressions");
    }
    if(expression->kind == ZIR_EXPR_CAST) {
        int value = assembly_expression(program, function, expression->right, depth + 1);
        return assembly_convert(program, function, value, type, expression->span);
    }
    if(expression->kind == ZIR_EXPR_UNARY) {
        if(!strcmp(expression->op, "&"))
            return assembly_address(program, function, expression->right, depth + 1);
        /* A signed literal in a parsed global initializer is typed like the
         * bare literal: by the member or global it initializes. */
        if(function->literal_at == root + 1 && (!strcmp(expression->op, "-") || !strcmp(expression->op, "+"))) {
            LirTypeKind own = assembly_type_at(program, type)->kind;
            if(own != LirTypeKind_Float && own != LirTypeKind_Integer) type = function->literal_type;
            function->literal_at = expression->right + 1;
        }
        int value = assembly_expression(program, function, expression->right, depth + 1);
        if(program->failed) return -1;
        if(!strcmp(expression->op, "*"))
            return assembly_emit(program, function, LirOp_Load, type, &value, 1, -1, expression->span);
        if(!strcmp(expression->op, "+")) return assembly_convert(program, function, value, type, expression->span);
        if(!strcmp(expression->op, "!")) {
            int actual = ((int *)function->values.data)[value];
            int operands[2] = {value, assembly_zero(program, function, actual, expression->span, depth + 1)};
            return assembly_emit(program, function, LirOp_Equal, 1, operands, 2, -1, expression->span);
        }
        LirOp op = !strcmp(expression->op, "-") ? LirOp_Negate :
            !strcmp(expression->op, "~") ? LirOp_BitNot : LirOp_Invalid;
        if(op == LirOp_Invalid) return assembly_fail(program, expression->span, "unsupported unary operation");
        value = assembly_convert(program, function, value, type, expression->span);
        return assembly_emit(program, function, op, type, &value, 1, -1, expression->span);
    }
    if(expression->kind == ZIR_EXPR_CONDITIONAL) {
        int condition = assembly_expression(program, function, expression->left, depth + 1);
        return assembly_select(program, function, expression, condition, expression->right, expression->third, 0, depth + 1);
    }
    if(expression->kind == ZIR_EXPR_BINARY) {
        const ZirExpr *left_source = expression->left >= 0 && expression->left < function->source->expr_count ?
            &function->source->exprs[expression->left] : NULL;
        if(left_source && left_source->kind == ZIR_EXPR_IDENT && !strcmp(left_source->name, "null") &&
           !strcmp(left_source->type, "null") && (!strcmp(expression->op, "==") || !strcmp(expression->op, "!="))) {
            /* null has no effects, so comparing it on the right is the same. */
            int other = assembly_expression(program, function, expression->right, depth + 1);
            if(program->failed) return -1;
            int other_type = ((int *)function->values.data)[other];
            LirTypeKind kind = assembly_type_at(program, other_type)->kind;
            if(kind != LirTypeKind_Pointer && kind != LirTypeKind_Function)
                return assembly_fail(program, left_source->span, "null requires a pointer or procedure type");
            int compared[2] = {other, assembly_emit(program, function, LirOp_Null, other_type, NULL, 0, -1, left_source->span)};
            return assembly_emit(program, function, assembly_binary(expression->op), 1, compared, 2, -1, expression->span);
        }
        int operands[2] = {assembly_expression(program, function, expression->left, depth + 1), -1};
        if(!strcmp(expression->op, "&&") || !strcmp(expression->op, "||")) {
            int is_or = !strcmp(expression->op, "||");
            return assembly_select(program, function, expression, operands[0], expression->right,
                expression->right, is_or ? 1 : 2, depth + 1);
        }
        const ZirExpr *right_source = expression->right >= 0 && expression->right < function->source->expr_count ?
            &function->source->exprs[expression->right] : NULL;
        int right_null = right_source && right_source->kind == ZIR_EXPR_IDENT &&
            !strcmp(right_source->name, "null") && !strcmp(right_source->type, "null");
        if(right_null && operands[0] >= 0) {
            /* An untyped null meets the other operand's pointer or slot type. */
            int other = ((int *)function->values.data)[operands[0]];
            LirTypeKind kind = assembly_type_at(program, other)->kind;
            if(kind != LirTypeKind_Pointer && kind != LirTypeKind_Function)
                return assembly_fail(program, right_source->span, "null requires a pointer or procedure type");
            operands[1] = assembly_emit(program, function, LirOp_Null, other, NULL, 0, -1, right_source->span);
        } else operands[1] = assembly_expression(program, function, expression->right, depth + 1);
        if(program->failed) return -1;
        LirOp op = assembly_binary(expression->op);
        if(op == LirOp_Invalid) return assembly_fail(program, expression->span, "unsupported binary operation");
        int left_type = ((int *)function->values.data)[operands[0]];
        int right_type = ((int *)function->values.data)[operands[1]];
        if(left_type == program->string_type && right_type == program->string_type &&
           (op == LirOp_Equal || op == LirOp_NotEqual)) {
            int compared[4];
            compared[0] = assembly_indexed(program, function, LirOp_Extract,
                assembly_member_type(program, left_type, 0), &operands[0], 1, 0, expression->span);
            compared[1] = assembly_indexed(program, function, LirOp_Extract,
                assembly_member_type(program, left_type, 1), &operands[0], 1, 1, expression->span);
            compared[2] = assembly_indexed(program, function, LirOp_Extract,
                assembly_member_type(program, right_type, 0), &operands[1], 1, 0, expression->span);
            compared[3] = assembly_indexed(program, function, LirOp_Extract,
                assembly_member_type(program, right_type, 1), &operands[1], 1, 1, expression->span);
            int equal = assembly_emit(program, function, LirOp_BytesEqual, 1,
                compared, 4, -1, expression->span);
            if(op == LirOp_Equal) return equal;
            int inverted[] = {equal, assembly_constant(program, function, 1, 0, expression->span)};
            return assembly_emit(program, function, LirOp_Equal, 1,
                inverted, 2, -1, expression->span);
        }
        int compare = op >= LirOp_Equal && op <= LirOp_GreaterEqual;
        int common = compare ? left_type : type;
        if(compare && left_type != right_type) {
            LirType left = *assembly_type_at(program, left_type);
            LirType right = *assembly_type_at(program, right_type);
            if(left.kind == LirTypeKind_Float || right.kind == LirTypeKind_Float) {
                if(left.kind != LirTypeKind_Float || (right.kind == LirTypeKind_Float && right.bits > left.bits))
                    common = right_type;
            } else if(left.is_signed != right.is_signed) {
                /* The checked language compares mixed-signedness integers
                 * by value, except that a u64 operand compares 64-bit
                 * patterns (an untyped literal such as -1 meets u64 as its
                 * two's-complement bits). s64 holds every narrower value. */
                int wide_unsigned = (!left.is_signed && left.bits == 64) ||
                    (!right.is_signed && right.bits == 64);
                common = wide_unsigned ? 9 : 8;
            } else if(right.bits > left.bits) common = right_type;
        }
        operands[0] = assembly_convert(program, function, operands[0], common, expression->span);
        if(op != LirOp_ShiftLeft && op != LirOp_ShiftRight)
            operands[1] = assembly_convert(program, function, operands[1], common, expression->span);
        return assembly_emit(program, function, op, compare ? 1 : type, operands, 2, -1, expression->span);
    }
    if(expression->kind == ZIR_EXPR_CALL) {
        int indirect = expression->slot_type && *expression->slot_type;
        if(!indirect && !strcmp(expression->name, "print"))
            return assembly_print(program, function, expression, depth + 1);
        if(!indirect && !strcmp(expression->name, "TextView"))
            return assembly_text_view(program, function, expression, type, depth + 1);
        if(!indirect && (!strcmp(expression->name, "VecPush") || !strcmp(expression->name, "VecClear") ||
           !strcmp(expression->name, "VecFree")))
            return assembly_vec_call(program, function, expression, depth + 1);
        if(!indirect && !strcmp(expression->name, "VecSlice"))
            return assembly_vec_slice(program, function, expression, depth + 1);
        if(!indirect && (!strcmp(expression->name, "VecReserve") || !strcmp(expression->name, "VecResize")))
            return assembly_vec_reserve_call(program, function, expression,
                !strcmp(expression->name, "VecResize"), depth + 1);
        if(!indirect && !strcmp(expression->name, "VecSwap"))
            return assembly_vec_swap(program, function, expression);
        if(!indirect && !strcmp(expression->name, "VecClone"))
            return assembly_vec_clone(program, function, expression, depth + 1);
        if(!indirect && (!strcmp(expression->name, "VecGet") || !strcmp(expression->name, "VecPop")))
            return assembly_vec_take(program, function, expression, type,
                !strcmp(expression->name, "VecGet"), depth + 1);
        if(!indirect && !strcmp(expression->name, "BuilderAppend"))
            return assembly_builder_append(program, function, expression, depth + 1);
        if(!indirect && !strcmp(expression->name, "BuilderFinish"))
            return assembly_builder_finish(program, function, expression);
        int symbol = -1;
        int receiver = -1;
        int result;
        Slice parameters;
        if(indirect) {
            receiver = expression->left >= 0 ?
                assembly_expression(program, function, expression->left, depth + 1) :
                assembly_named_value(program, function, expression->name, expression->span);
            if(program->failed) return -1;
            LirType signature = *assembly_type_at(program, ((int *)function->values.data)[receiver]);
            if(signature.kind != LirTypeKind_Function)
                return assembly_fail(program, expression->span, "indirect call requires a portable procedure value");
            parameters = signature.parameters;
            result = signature.result;
        } else {
            const ZirModule *owner = NULL;
            const ZirFunction *callee = NULL;
            if(ResolveFunctionAt(function->module, expression->name, SpanPath(expression->span), &owner, &callee) == 1)
                symbol = assembly_function(program, owner, callee);
            else if(!strcmp(expression->name, "zi_new")) return assembly_new(program, function, expression, type);
            else if(!strcmp(expression->name, "zi_free")) return assembly_free(program, function, expression, depth + 1);
            else symbol = assembly_host_function(program, function->module, expression->name, expression->span);
            if(program->failed) return -1;
            if(symbol < 0) return assembly_fail(program, expression->span, "call is not a direct concrete Ziran procedure");
            AssemblyFunction *target = ((AssemblyFunction **)program->functions.data)[symbol];
            parameters = target->result.parameters;
            result = target->result.result;
        }
        int count = (int)parameters.length;
        int *arguments = assembly_allocate(program, (size_t)count + indirect, sizeof(*arguments));
        if((count || indirect) && arguments == NULL) return -1;
        if(indirect) arguments[0] = receiver;
        int *values = indirect ? arguments + 1 : arguments;
        for(int index = 0; index < count; index++) values[index] = -1;
        int ordinal = 0;
        for(int child = expression->first_child; child >= 0;) {
            if(child >= function->source->expr_count || !assembly_spend(program, expression->span)) return -1;
            const ZirExpr *argument = &function->source->exprs[child];
            int position = argument->argument_index >= 0 ? argument->argument_index : ordinal;
            if(position < 0 || position >= count || values[position] >= 0)
                return assembly_fail(program, argument->span, "invalid checked call argument mapping");
            int wanted = ((int *)parameters.data)[position];
            int value = assembly_expression_for(program, function, child, wanted, depth + 1);
            values[position] = assembly_convert(program, function, value, wanted, argument->span);
            ordinal++;
            child = argument->next_sibling;
        }
        for(int index = 0; index < count; index++) if(values[index] < 0)
            return assembly_fail(program, expression->span, "call arguments are not fully lowered");
        int host = !indirect && ((AssemblyFunction **)program->functions.data)[symbol]->result.external;
        return assembly_emit(program, function, indirect ? LirOp_CallIndirect : host ? LirOp_HostCall : LirOp_Call, result,
            arguments, count + indirect, symbol, expression->span);
    }
    return assembly_fail(program, expression->span, "expression is outside the initial direct scalar backend");
}

static int
assembly_close(AssemblyProgram *program, const ZirFunction *source, int begin, int end)
{
    int nesting = 1;
    for(int index = begin + 1; index < end; index++) {
        if(!assembly_spend(program, source->stmts[index].span)) return end;
        ZirStmtKind kind = source->stmts[index].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE || kind == ZIR_STMT_BLOCK_OPEN) nesting++;
        if(kind == ZIR_STMT_BLOCK_CLOSE && --nesting == 0) return index;
    }
    assembly_fail(program, source->stmts[begin].span, "unbalanced checked statement blocks");
    return end;
}

static void assembly_sequence(AssemblyProgram *, AssemblyFunction *, int, int, int);

static AssemblyLocalPlace *
assembly_capture_local(AssemblyProgram *program, AssemblyFunction *function, int root, int depth)
{
    if(!assembly_spend(program, function->source->span)) return NULL;
    if(depth > 256 || root < 0 || root >= function->source->expr_count) {
        assembly_fail(program, function->source->span, "invalid or excessively deep local assignment");
        return NULL;
    }
    const ZirExpr *destination = &function->source->exprs[root];
    if(destination->kind == ZIR_EXPR_IDENT && !destination->is_global_value) {
        int binding = assembly_binding(function, destination->name);
        if(binding < 0) {
            assembly_fail(program, destination->span, "assignment requires a local binding");
            return NULL;
        }
        AssemblyLocalPlace *place = assembly_allocate(program, 1, sizeof(*place));
        if(place == NULL) return NULL;
        AssemblyBinding current = ((AssemblyBinding *)function->bindings.data)[binding];
        *place = (AssemblyLocalPlace){.binding = binding, .type = current.type,
            .value = current.value, .index = -1, .storage = current.storage_type >= 0};
        return place;
    }
    if(destination->kind != ZIR_EXPR_MEMBER && destination->kind != ZIR_EXPR_INDEX) {
        assembly_fail(program, destination->span, "assignment destination has no lowered storage representation yet");
        return NULL;
    }
    AssemblyLocalPlace *parent = assembly_capture_local(program, function, destination->left, depth + 1);
    if(parent == NULL) return NULL;
    int member = -1, selected = -1, member_type = -1;
    if(destination->kind == ZIR_EXPR_MEMBER) {
        member = assembly_field(program, parent->type, destination->name, destination->span);
        if(!program->failed) member_type = assembly_member_type(program, parent->type, member);
    }
    else {
        LirType type = *assembly_type_at(program, parent->type);
        if(type.kind != LirTypeKind_Array || destination->right < 0 || destination->right >= function->source->expr_count) {
            assembly_fail(program, destination->span, "invalid checked array assignment index");
            return NULL;
        }
        const ZirExpr *index = &function->source->exprs[destination->right];
        unsigned long long ordinal = 0;
        if(index->kind == ZIR_EXPR_INT && ZirIntegerLiteral(index->text, &ordinal) &&
           ordinal <= INT_MAX && ordinal < (uint64_t)type.count) member = (int)ordinal;
        else selected = assembly_expression(program, function, destination->right, depth + 1);
        member_type = type.element;
    }
    if(program->failed) return NULL;
    if(member_type < 0) {
        assembly_fail(program, destination->span, "aggregate assignment is outside its declared shape");
        return NULL;
    }
    int operands[] = {parent->value, selected};
    int current = assembly_indexed(program, function, LirOp_Extract, member_type,
        operands, selected < 0 ? 1 : 2, selected < 0 ? (uint64_t)member : 0, destination->span);
    if(program->failed) return NULL;
    AssemblyLocalPlace *place = assembly_allocate(program, 1, sizeof(*place));
    if(place) *place = (AssemblyLocalPlace){.type = member_type, .value = current,
        .member = member, .index = selected, .parent = parent};
    return place;
}

static void
assembly_store_captured(AssemblyProgram *program, AssemblyFunction *function,
                       const AssemblyLocalPlace *place, int value, ZirSourceSpan span)
{
    if(program->failed || place == NULL || !assembly_spend(program, span)) return;
    value = assembly_convert(program, function, value, place->type, span);
    if(program->failed) return;
    if(place->parent == NULL) {
        if(place->storage) {
            int operands[2] = {place->value, value};
            assembly_emit(program, function, LirOp_Store, 0, operands, 2, -1, span);
            return;
        }
        ((AssemblyBinding *)function->bindings.data)[place->binding].value = value;
        return;
    }
    int operands[] = {place->parent->value, value, place->index};
    int updated = assembly_indexed(program, function, LirOp_Insert, place->parent->type,
        operands, place->index < 0 ? 2 : 3, place->index < 0 ? (uint64_t)place->member : 0, span);
    assembly_store_captured(program, function, place->parent, updated, span);
}

/* One arm of an integer case list: the value it matches, where its body
 * begins and closes, and the block the body is lowered into. */
typedef struct AssemblyCase {
    uint64_t value;
    int open;
    int close;
    int block;
} AssemblyCase;

static int assembly_case_signed;

static int
assembly_case_order(const void *left, const void *right)
{
    uint64_t a = ((const AssemblyCase *)left)->value, b = ((const AssemblyCase *)right)->value;
    if(assembly_case_signed) return (int64_t)a < (int64_t)b ? -1 : (int64_t)a > (int64_t)b;
    return a < b ? -1 : a > b;
}

/* Branch from the current block to the arm among cases[low..high) whose
 * value `subject` equals, or to `otherwise`: a binary search, so a list of
 * n cases costs about log2(n) compares instead of n. */
static void
assembly_dispatch(AssemblyProgram *program, AssemblyFunction *function, int subject, int type,
                  const AssemblyCase *cases, int low, int high, int otherwise,
                  const AssemblyBinding *before, int count, ZirSourceSpan span)
{
    if(program->failed) return;
    int middle = low + (high - low) / 2;
    int operands[] = {subject, assembly_constant(program, function, type, cases[middle].value, span)};
    if(high - low == 1) {
        int same = assembly_emit(program, function, LirOp_Equal, 1, operands, 2, -1, span);
        assembly_branch(program, function, same, cases[low].block, otherwise, count, span);
        return;
    }
    int lower = assembly_block(program, function, before, count);
    int upper = assembly_block(program, function, before, count);
    int below = assembly_emit(program, function, LirOp_Less, 1, operands, 2, -1, span);
    assembly_branch(program, function, below, lower, upper, count, span);
    assembly_enter(function, lower, before, count);
    assembly_dispatch(program, function, subject, type, cases, low, middle, otherwise, before, count, span);
    assembly_enter(function, upper, before, count);
    assembly_dispatch(program, function, subject, type, cases, middle, high, otherwise, before, count, span);
}

/* The arms of an `if` chain when it is a case list over one local: every
 * link compares the same identifier with a distinct integer literal. Returns
 * the number of arms, with `*otherwise` the `else` statement or -1 and
 * `*next` the statement after the chain; 0 when the chain is anything else. */
static int
assembly_case_list(AssemblyProgram *program, AssemblyFunction *function, int index, int end,
                   AssemblyVector *arms, int *otherwise, int *next)
{
    const ZirFunction *source = function->source;
    const char *subject = NULL;
    *otherwise = -1;
    for(int at = index;;) {
        const ZirStmt *statement = &source->stmts[at];
        int close = assembly_close(program, source, at, end);
        if(at != index && statement->expr_root < 0) { *otherwise = at; *next = close + 1; break; }
        if(statement->expr_root < 0 || statement->expr_root >= source->expr_count) return 0;
        const ZirExpr *condition = &source->exprs[statement->expr_root];
        if(condition->kind != ZIR_EXPR_BINARY || strcmp(condition->op, "==") ||
           condition->left < 0 || condition->right < 0) return 0;
        const ZirExpr *left = &source->exprs[condition->left], *right = &source->exprs[condition->right];
        /* The value is an integer literal, bare or negated. */
        int negative = 0;
        if(right->kind == ZIR_EXPR_UNARY && !strcmp(right->op, "-") &&
           right->right >= 0 && right->right < source->expr_count) {
            right = &source->exprs[right->right];
            negative = 1;
        }
        if(left->kind != ZIR_EXPR_IDENT || left->is_global_value || right->kind != ZIR_EXPR_INT ||
           (subject && strcmp(subject, left->name))) return 0;
        subject = left->name;
        unsigned long long bits = 0;
        const char *digits = right->text;
        if(digits[0] == '-') negative = !negative;
        if(digits[0] == '-' || digits[0] == '+') digits++;
        if(!ZirIntegerLiteral(digits, &bits)) return 0;
        AssemblyCase arm = {negative ? 0 - bits : bits, at, close, -1};
        if(!assembly_push(program, arms, &arm)) return 0;
        *next = close + 1;
        at = close + 1;
        if(at >= end || !source->stmts[at].is_else) break;
    }
    return arms->count;
}

static int
assembly_if(AssemblyProgram *program, AssemblyFunction *function, int index, int end, int depth)
{
    if(depth > 256) {
        assembly_fail(program, function->source->span, "branch nesting budget exhausted");
        return end;
    }
    /* An `if` with its `else if` links and `else` is walked as one chain, so
     * a case list of any length nests no deeper than a single `if`. Every
     * arm that falls through meets the others in one join block. */
    typedef struct { int block; AssemblyBinding *bindings; } Arm;
    ZirSourceSpan span = function->source->stmts[index].span;
    int count = function->bindings.count;
    AssemblyBinding *before = assembly_snapshot(program, function, count);
    AssemblyVector arms = {.width = sizeof(Arm)};
    int next = index;
    /* A case list of eight or more integer values is a binary search. */
    AssemblyVector listed = {.width = sizeof(AssemblyCase)};
    int otherwise_at = -1, after = index;
    int subject = -1, subject_type = 0;
    if(assembly_case_list(program, function, index, end, &listed, &otherwise_at, &after) >= 8) {
        const ZirExpr *first = &function->source->exprs[function->source->stmts[index].expr_root];
        subject = assembly_expression(program, function, first->left, 0);
        if(program->failed) return end;
        subject_type = ((int *)function->values.data)[subject];
        LirType shape = *assembly_type_at(program, subject_type);
        AssemblyCase *cases = listed.data;
        if(shape.kind == LirTypeKind_Integer) {
            /* Each value as the subject's type holds it, then in order. */
            for(int arm = 0; arm < listed.count; arm++) {
                if(shape.bits < 64) cases[arm].value &= (UINT64_C(1) << shape.bits) - 1;
                if(shape.is_signed && shape.bits < 64 && (cases[arm].value >> (shape.bits - 1)))
                    cases[arm].value |= ~((UINT64_C(1) << shape.bits) - 1);
            }
            assembly_case_signed = shape.is_signed;
            qsort(cases, (size_t)listed.count, sizeof(*cases), assembly_case_order);
            for(int arm = 1; arm < listed.count; arm++)
                if(cases[arm].value == cases[arm - 1].value) subject = -1;
        } else subject = -1;
    }
    if(subject >= 0 && listed.count >= 8) {
        AssemblyCase *cases = listed.data;
        for(int arm = 0; arm < listed.count; arm++) cases[arm].block = assembly_block(program, function, before, count);
        int otherwise = assembly_block(program, function, before, count);
        if(program->failed) return end;
        assembly_dispatch(program, function, subject, subject_type, cases, 0, listed.count, otherwise,
            before, count, span);
        for(int arm = 0; arm < listed.count && !program->failed; arm++) {
            assembly_enter(function, cases[arm].block, before, count);
            assembly_sequence(program, function, cases[arm].open + 1, cases[arm].close, depth + 1);
            Arm reached = {function->current, assembly_snapshot(program, function, count)};
            if(!assembly_push(program, &arms, &reached)) return end;
        }
        if(program->failed) return end;
        assembly_enter(function, otherwise, before, count);
        if(otherwise_at >= 0)
            assembly_sequence(program, function, otherwise_at + 1,
                assembly_close(program, function->source, otherwise_at, end), depth + 1);
        Arm rest = {function->current, assembly_snapshot(program, function, count)};
        if(program->failed || !assembly_push(program, &arms, &rest)) return end;
        next = after;
    } else for(;;) {
        const ZirStmt *statement = &function->source->stmts[next];
        int close = assembly_close(program, function->source, next, end);
        int conditional = next == index || statement->expr_root >= 0;
        int no = -1;
        if(conditional) {
            int condition = assembly_expression(program, function, statement->expr_root, 0);
            int yes = assembly_block(program, function, before, count);
            no = assembly_block(program, function, before, count);
            if(program->failed) return end;
            assembly_branch(program, function, condition, yes, no, count, statement->span);
            assembly_enter(function, yes, before, count);
        }
        assembly_sequence(program, function, next + 1, close, depth + 1);
        Arm arm = {function->current, assembly_snapshot(program, function, count)};
        if(program->failed || !assembly_push(program, &arms, &arm)) return end;
        next = close + 1;
        if(!conditional) break;
        assembly_enter(function, no, before, count);
        if(next >= end || !function->source->stmts[next].is_else) {
            /* No `else`: the untaken edge falls through as it stands. */
            Arm rest = {function->current, assembly_snapshot(program, function, count)};
            if(program->failed || !assembly_push(program, &arms, &rest)) return end;
            break;
        }
    }
    Arm *reached = arms.data;
    int live = 0;
    for(int arm = 0; arm < arms.count; arm++) if(reached[arm].block >= 0) live++;
    if(!live) { function->current = -1; return next; }
    int join = assembly_block(program, function, before, count);
    if(program->failed) return end;
    for(int arm = 0; arm < arms.count; arm++) {
        if(reached[arm].block < 0) continue;
        function->current = reached[arm].block;
        if(count) memcpy(function->bindings.data, reached[arm].bindings, (size_t)count * sizeof(*before));
        assembly_jump(program, function, join, count, span);
    }
    assembly_enter(function, join, before, count);
    return next;
}

static void
assembly_sequence(AssemblyProgram *program, AssemblyFunction *function, int begin, int end, int depth)
{
    if(depth > 256) { assembly_fail(program, function->source->span, "statement nesting budget exhausted"); return; }
    int outer_bindings = function->bindings.count;
    for(int index = begin; index < end && !program->failed && function->current >= 0; index++) {
        const ZirStmt *statement = &function->source->stmts[index];
        if(!assembly_spend(program, statement->span)) return;
        if(statement->is_parallel || statement->is_gpu || StatementHasAssembly(statement)) {
            assembly_fail(program, statement->span, "parallel, GPU and assembly variants are not lowered yet");
            break;
        }
        if(statement->kind == ZIR_STMT_DECL) {
            int type = assembly_type(program, function, statement->type, statement->span);

            int value = statement->expr_root >= 0 ? assembly_expression(program, function, statement->expr_root, 0) :
                assembly_zero(program, function, type, statement->span, 0);
            value = assembly_convert(program, function, value, type, statement->span);
            AssemblyBinding binding = {assembly_text(program, statement->name), type, value, -1};
            assembly_push(program, &function->bindings, &binding);
            if(index < function->addressed_count && function->addressed[index])
                assembly_promote_binding(program, function, function->bindings.count - 1,
                    statement->span);
        } else if(statement->kind == ZIR_STMT_ASSIGN) {
            if(statement->lhs_root < 0 || statement->lhs_root >= function->source->expr_count) {
                assembly_fail(program, statement->span, "invalid checked assignment destination");
                break;
            }
            int previous = -1;
            /* `whole.member = value` on a union assigns the whole union with
             * the member's bytes replaced. */
            int place = statement->lhs_root;
            const ZirExpr *written = &function->source->exprs[place];
            int union_member = written->kind == ZIR_EXPR_MEMBER && written->left >= 0 &&
                written->left < function->source->expr_count ?
                assembly_union_member(program, function, function->source->exprs[written->left].type,
                    written->name, statement->span) : -1;
            if(program->failed) break;
            if(union_member >= 0) {
                if(strcmp(statement->assignment_op, "=")) {
                    assembly_fail(program, statement->span, "compound assignment to a union member is not lowered yet");
                    break;
                }
                place = written->left;
            }
            int identity = assembly_type(program, function, function->source->exprs[place].type, statement->span);

            int address = -1;
            AssemblyLocalPlace *destination = NULL;
            if(assembly_addressable(program, function, place, 0))
                address = assembly_address(program, function, place, 0);
            else destination = assembly_capture_local(program, function, place, 0);
            if(program->failed) break;
            if(strcmp(statement->assignment_op, "="))
                previous = address >= 0 || destination->storage ?
                    assembly_emit(program, function, LirOp_Load, identity,
                        address >= 0 ? &address : &destination->value, 1, -1, statement->span) :
                    destination->value;
            int value = assembly_expression_for(program, function, statement->expr_root,
                union_member >= 0 ? union_member : identity, 0);
            if(union_member >= 0 && !program->failed) {
                int old = address >= 0 || destination->storage ?
                    assembly_emit(program, function, LirOp_Load, identity,
                        address >= 0 ? &address : &destination->value, 1, -1, statement->span) :
                    destination->value;
                value = assembly_union_write(program, function, old,
                    assembly_convert(program, function, value, union_member, statement->span),
                    identity, union_member, statement->span);
            }
            if(strcmp(statement->assignment_op, "=")) {
                char operator_text[4];
                memcpy(operator_text, statement->assignment_op, sizeof(operator_text));
                size_t length = strlen(operator_text);
                if(length) operator_text[length - 1] = 0;
                LirOp op = assembly_binary(operator_text);
                if(op == LirOp_Invalid) { assembly_fail(program, statement->span, "unsupported compound assignment"); break; }
                if(op != LirOp_ShiftLeft && op != LirOp_ShiftRight)
                    value = assembly_convert(program, function, value, identity, statement->span);
                int operands[2] = {previous, value};
                value = assembly_emit(program, function, op, identity, operands, 2, -1, statement->span);
            }
            if(address >= 0) {
                int operands[2] = {address, assembly_convert(program, function, value, identity, statement->span)};
                assembly_emit(program, function, LirOp_Store, 0, operands, 2, -1, statement->span);
            } else assembly_store_captured(program, function, destination, value, statement->span);
        } else if(statement->kind == ZIR_STMT_EXPR || statement->kind == ZIR_STMT_UNUSED) {
            int value = assembly_expression(program, function, statement->expr_root, 0);
            /* A fresh owned result nobody keeps is dropped at once. */
            if(!program->failed && value >= 0 && function->current >= 0 &&
               statement->expr_root >= 0 && statement->expr_root < function->source->expr_count &&
               function->source->exprs[statement->expr_root].kind == ZIR_EXPR_CALL)
                assembly_drop_value(program, function, value, ((int *)function->values.data)[value],
                    statement->span, 0);
        } else if(statement->kind == ZIR_STMT_RETURN) {
            int value = -1;
            if(function->result.result)
                value = assembly_convert(program, function,
                    assembly_expression_for(program, function, statement->expr_root, function->result.result, 0),
                    function->result.result, statement->span);
            assembly_drop_bindings(program, function, 0, statement->span);
            assembly_emit(program, function, LirOp_Return, 0, &value, function->result.result ? 1 : 0, -1, statement->span);
            function->current = -1;
        } else if(statement->kind == ZIR_STMT_IF) {
            index = assembly_if(program, function, index, end, depth) - 1;
        } else if(statement->kind == ZIR_STMT_WHILE) {
            int close = assembly_close(program, function->source, index, end);
            int constant = -1;
            if(statement->expr_root >= 0 && statement->expr_root < function->source->expr_count) {
                const ZirExpr *condition = &function->source->exprs[statement->expr_root];
                if(condition->kind == ZIR_EXPR_IDENT && !strcmp(condition->name, "true")) constant = 1;
                if(condition->kind == ZIR_EXPR_IDENT && !strcmp(condition->name, "false")) constant = 0;
            }
            if(constant == 0) { index = close; continue; }
            int count = function->bindings.count;
            AssemblyBinding *before = assembly_snapshot(program, function, count);
            int header = assembly_block(program, function, before, count);
            int body = assembly_block(program, function, before, count);
            int exit = constant == 1 ? -1 : assembly_block(program, function, before, count);
            if(program->failed) break;
            assembly_jump(program, function, header, count, statement->span);
            assembly_enter(function, header, before, count);
            assembly_emit(program, function, LirOp_Fuel, 0, NULL, 0, -1, statement->span);
            int condition = assembly_expression(program, function, statement->expr_root, 0);
            if(constant == 1) assembly_jump(program, function, body, count, statement->span);
            else assembly_branch(program, function, condition, body, exit, count, statement->span);
            AssemblyLoop loop = {header, exit, count, statement->loop_id, function->loop};
            function->loop = &loop;
            assembly_enter(function, body, before, count);
            assembly_sequence(program, function, index + 1, close, depth + 1);
            if(function->current >= 0) assembly_jump(program, function, header, count, statement->span);
            function->loop = loop.parent;
            if(loop.exit >= 0) assembly_enter(function, loop.exit, before, count);
            else function->current = -1;
            index = close;
        } else if(statement->kind == ZIR_STMT_BREAK || statement->kind == ZIR_STMT_CONTINUE) {
            AssemblyLoop *loop = function->loop;
            while(loop && statement->target_id && loop->identity != statement->target_id) loop = loop->parent;
            if(loop == NULL) { assembly_fail(program, statement->span, "invalid checked loop-control target"); break; }
            if(statement->kind == ZIR_STMT_BREAK && loop->exit < 0)
                loop->exit = assembly_block(program, function, function->bindings.data, loop->bindings);
            if(program->failed) break;
            assembly_drop_bindings(program, function, loop->bindings, statement->span);
            assembly_jump(program, function, statement->kind == ZIR_STMT_BREAK ? loop->exit : loop->header,
                loop->bindings, statement->span);
        } else if(statement->kind == ZIR_STMT_BLOCK_OPEN) {
            int close = assembly_close(program, function->source, index, end);
            assembly_sequence(program, function, index + 1, close, depth + 1);
            index = close;
        } else if(statement->kind == ZIR_STMT_UNREACHABLE) {
            assembly_emit(program, function, LirOp_Unreachable, 0, NULL, 0, -1, statement->span);
            function->current = -1;
        } else {
            assembly_fail(program, statement->span, "statement is outside the initial direct scalar backend");
        }
    }
    if(end > begin) assembly_drop_bindings(program, function, outer_bindings,
        function->source->stmts[end - 1].span);
    function->bindings.count = outer_bindings;
}

static int
assembly_function(AssemblyProgram *program, const ZirModule *module, const ZirFunction *source)
{
    AssemblyFunction **functions = program->functions.data;
    for(int index = 0; index < program->functions.count; index++)
        if(functions[index]->source == source && functions[index]->module == module) return index;
    /* A host_api declaration becomes a bodiless capability signature; its
     * callers lower ordinary arguments into an explicit host.call. */
    int host = source->is_extern && source->extern_kind == ZIR_EXTERN_HOST;
    if((!host && (!source->checked || source->is_extern)) || source->is_template ||
       source->simd_target || (source->assembly_text && *source->assembly_text))
        return assembly_fail(program, source->span, "direct lowering requires a concrete checked function without non-host foreign services or native variants");
    AssemblyFunction *function = assembly_allocate(program, 1, sizeof(*function));
    if(function == NULL) return -1;
    function->module = module;
    function->source = source;
    function->blocks.width = sizeof(AssemblyBlock);
    function->values.width = sizeof(int);
    function->bindings.width = sizeof(AssemblyBinding);
    function->current = -1;
    function->result.module = assembly_name(program, &program->modules, module->name);
    const char *name = assembly_text(program, host && source->extern_symbol[0] ? source->extern_symbol : source->name);
    function->result.name = (String){name, strlen(name)};
    if(host) {
        function->result.calling = LirCalling_Host;
        function->result.external = 1;
    }
    function->result.result = assembly_type(program, function, source->return_type, source->span);
    function->result.origin = assembly_origin(program, source->span);
    const ZirParameters *parameters = ParametersOf(FunctionArgs(source));
    int *types = assembly_allocate(program, (size_t)parameters->count, sizeof(*types));
    for(int index = 0; index < parameters->count && !program->failed; index++) {
        const ZirParameter *parameter = &parameters->items[index];
        types[index] = assembly_type(program, function, parameter->value_type, source->span);
        AssemblyBinding binding = {assembly_text(program, parameter->binding_name), types[index], -1, -1};
        assembly_push(program, &function->bindings, &binding);
    }
    function->result.parameters = (Slice){types, parameters->count};
    assembly_find_addressed(program, function);
    int identity = program->functions.count;
    return assembly_push(program, &program->functions, &function) ? identity : -1;
}

/* A `#foreign host_api` declaration is a checked module import, not a function.
 * Its signature-only function record is created once per import. */
static int
assembly_host_function(AssemblyProgram *program, const ZirModule *module, const char *name, ZirSourceSpan span)
{
    const ZirImport *import = NULL;
    for(int index = 0; index < module->import_count && import == NULL; index++) {
        const ZirImport *candidate = &module->imports[index];
        if(candidate->kind == ZIR_IMPORT_EXTERN && candidate->extern_kind == ZIR_EXTERN_HOST &&
           strcmp(candidate->name, name) == 0) import = candidate;
    }
    if(import == NULL) return -1;
    AssemblyFunction **functions = program->functions.data;
    for(int index = 0; index < program->functions.count; index++)
        if(functions[index]->import == import) return index;
    if(strncmp(import->target, "ziran:", 6) == 0) {
        const ZirModule *provider_module = NULL;
        const ZirFunction *provider = NULL;
        const char *provider_name = import->extern_symbol[0] ? import->extern_symbol : import->name;
        for(int module_index = 0; program->source != NULL &&
            module_index < program->source->module_count; module_index++) {
            const ZirModule *candidate = &program->source->modules[module_index];
            if(strcmp(candidate->name, import->target + 6)) continue;
            if(provider_module != NULL)
                return assembly_fail(program, span, "bound host provider module is ambiguous");
            provider_module = candidate;
        }
        for(int function_index = 0; provider_module != NULL &&
            function_index < provider_module->function_count; function_index++) {
            const ZirFunction *candidate = &provider_module->functions[function_index];
            if(candidate->is_extern || !candidate->exported || strcmp(candidate->name, provider_name)) continue;
            if(provider != NULL)
                return assembly_fail(program, span, "bound host provider procedure is ambiguous");
            provider = candidate;
        }
        if(provider == NULL)
            return assembly_fail(program, span, "bound host provider procedure is missing");
        return assembly_function(program, provider_module, provider);
    }
    if(import->is_varargs) return assembly_fail(program, span, "variadic host capabilities are not lowered");
    ZirFunction *signature = assembly_allocate(program, 1, sizeof(*signature));
    if(signature == NULL) return -1;
    snprintf(signature->name, sizeof(signature->name), "%s", import->name);
    snprintf(signature->return_type, sizeof(signature->return_type), "%s", import->return_type);
    snprintf(signature->extern_symbol, sizeof(signature->extern_symbol), "%s",
             import->extern_symbol[0] ? import->extern_symbol : import->name);
    signature->args_text = KeepParameters(import->args);
    signature->is_extern = 1;
    signature->extern_kind = ZIR_EXTERN_HOST;
    signature->span = import->span;
    int identity = assembly_function(program, module, signature);
    if(identity >= 0) ((AssemblyFunction **)program->functions.data)[identity]->import = import;
    return identity;
}

static int
assembly_startup_module(AssemblyProgram *program, const ZirProgram *source,
                        int module_index, unsigned char *state)
{
    const ZirModule *module = &source->modules[module_index];
    if(state[module_index] == 2) return 1;
    if(state[module_index] == 1)
        return assembly_fail(program, module->span,
            "cyclic runtime module initialization dependency") >= 0;
    state[module_index] = 1;
    for(int import_index = 0; import_index < module->import_count; import_index++) {
        const ZirModule *dependency = module->imports[import_index].resolved_module;
        if(dependency == NULL || !ModuleNeedsStartup(dependency)) continue;
        int dependency_index = -1;
        for(int index = 0; index < source->module_count; index++)
            if(dependency == &source->modules[index]) { dependency_index = index; break; }
        if(dependency_index < 0)
            return assembly_fail(program, module->imports[import_index].span,
                "runtime initializer dependency is outside the checked program") >= 0;
        if(!assembly_startup_module(program, source, dependency_index, state)) return 0;
    }
    for(int function_index = 0; function_index < module->function_count; function_index++) {
        const ZirFunction *function = &module->functions[function_index];
        if(!function->is_global_initializer) continue;
        int identity = assembly_function(program, module, function);
        if(identity < 0 || !assembly_push(program, &program->initializers, &identity)) return 0;
    }
    state[module_index] = 2;
    return 1;
}

AssemblyProgram *
AssemblyLower(const ZirProgram *source, const char *module_name, const char *function_name)
{
    AssemblyProgram *program = calloc(1, sizeof(*program));
    if(program == NULL) { DiagnosticOutOfMemory(); return NULL; }
    program->work = 16000000;
    program->source = source;
    program->modules.width = sizeof(String);
    program->sources.width = sizeof(String);
    program->constants.width = sizeof(LirConstant);
    program->globals.width = sizeof(AssemblyGlobal);
    program->origins.width = sizeof(LirOrigin);
    program->functions.width = sizeof(AssemblyFunction *);
    program->initializers.width = sizeof(int);
    program->constant_initializers.width = sizeof(int);
    program->probes.width = sizeof(ZirFunction *);
    program->types.width = sizeof(LirType);
    program->type_names.width = sizeof(AssemblyType);
    LirType void_type = {.kind = LirTypeKind_Void};
    LirType bool_type = {.kind = LirTypeKind_Bool, .bits = 1};
    assembly_push(program, &program->types, &void_type);
    assembly_push(program, &program->types, &bool_type);
    for(int index = 0; index < 8; index++) {
        LirType integer = {.kind = LirTypeKind_Integer,
            .bits = 8 << (index / 2), .is_signed = index % 2 == 0};
        assembly_push(program, &program->types, &integer);
    }
    const ZirModule *module = NULL;
    const ZirFunction *function = NULL;
    for(int index = 0; index < source->module_count; index++)
        if(!strcmp(source->modules[index].name, module_name)) {
            if(module) { assembly_fail(program, source->modules[index].span, "ambiguous entry module"); break; }
            module = &source->modules[index];
        }
    if(module)
        for(int index = 0; index < module->function_count; index++)
            if(!strcmp(module->functions[index].name, function_name)) function = &module->functions[index];
    if(function == NULL) assembly_fail(program, (ZirSourceSpan){0}, "entry procedure was not found");
    if(!program->failed) assembly_function(program, module, function);
    unsigned char *startup_state = assembly_allocate(program, (size_t)source->module_count, 1);
    if(!program->failed)
        for(int index = 0; index < source->module_count; index++)
            if(ModuleNeedsStartup(&source->modules[index]) &&
               !assembly_startup_module(program, source, index, startup_state)) break;
    for(int index = 0; index < program->functions.count && !program->failed; index++) {
        AssemblyFunction *builder = ((AssemblyFunction **)program->functions.data)[index];
        if(builder->result.external) { builder->result.entry = -1; continue; }
        int count = builder->bindings.count;
        AssemblyBinding *parameters = assembly_snapshot(program, builder, count);
        int entry = assembly_block(program, builder, parameters, count);
        if(program->failed) break;
        builder->result.entry = entry;
        assembly_enter(builder, entry, parameters, count);
        assembly_emit(program, builder, LirOp_Fuel, 0, NULL, 0, -1, builder->source->span);
        for(int parameter = 0; parameter < count && !program->failed; parameter++) {
            AssemblyBinding *binding = &((AssemblyBinding *)builder->bindings.data)[parameter];
            int source_binding = builder->source->stmt_count + parameter;
            if(source_binding < builder->addressed_count && builder->addressed[source_binding])
                assembly_promote_binding(program, builder, parameter, builder->source->span);
        }
        if(builder->initializes) {
            int symbol = builder->initializes - 1;
            int type = ((AssemblyGlobal *)program->globals.data)[symbol].result.type;
            const ZirExpr *root = &builder->source->exprs[builder->initializer_root];
            builder->literal_at = builder->initializer_root + 1;
            builder->literal_type = type;
            int value = assembly_convert(program, builder, root->kind == ZIR_EXPR_COMPOUND ?
                assembly_compound(program, builder, root, type, 0) :
                assembly_expression(program, builder, builder->initializer_root, 0), type, builder->source->span);
            int operands[] = {assembly_emit(program, builder, LirOp_GlobalAddress,
                assembly_pointer_type(program, type, builder->source->span), NULL, 0, symbol, builder->source->span), value};
            assembly_emit(program, builder, LirOp_Store, 0, operands, 2, -1, builder->source->span);
        }
        assembly_sequence(program, builder, 0, builder->source->stmt_count, 0);
        if(builder->current >= 0 && !program->failed) {
            if(builder->result.result) assembly_fail(program, builder->source->span, "scalar procedure falls through without a return");
            else {
                assembly_drop_bindings(program, builder, 0, builder->source->span);
                assembly_emit(program, builder, LirOp_Return, 0, NULL, 0, -1, builder->source->span);
            }
        }
        LirBlock *blocks = assembly_allocate(program, (size_t)builder->blocks.count, sizeof(*blocks));
        if(program->failed) break;
        for(int block = 0; block < builder->blocks.count; block++) {
            AssemblyBlock *body = assembly_block_at(builder, block);
            blocks[block] = (LirBlock){assembly_slice(body->parameters), assembly_slice(body->instructions)};
        }
        builder->result.blocks = (Slice){blocks, builder->blocks.count};
        builder->result.values = assembly_slice(builder->values);
    }
    LirFunction *functions = assembly_allocate(program, (size_t)program->functions.count, sizeof(*functions));
    LirGlobal *globals = assembly_allocate(program, (size_t)program->globals.count, sizeof(*globals));
    /* Constant initializers run first, so module startup code and other
     * initializers read initialized globals. */
    AssemblyVector initializers = {.width = sizeof(int)};
    for(int index = 0; index < program->constant_initializers.count; index++)
        assembly_push(program, &initializers, &((int *)program->constant_initializers.data)[index]);
    for(int index = 0; index < program->initializers.count; index++)
        assembly_push(program, &initializers, &((int *)program->initializers.data)[index]);
    if(!program->failed) {
        for(int index = 0; index < program->functions.count; index++)
            functions[index] = ((AssemblyFunction **)program->functions.data)[index]->result;
        for(int index = 0; index < program->globals.count; index++)
            globals[index] = ((AssemblyGlobal *)program->globals.data)[index].result;
        program->result = (LirProgram){.version = LirVersion, .modules = assembly_slice(program->modules),
            .sources = assembly_slice(program->sources), .types = assembly_slice(program->types),
            .constants = assembly_slice(program->constants), .globals = {globals, program->globals.count},
            .functions = {functions, program->functions.count},
            .origins = assembly_slice(program->origins),
            .initializers = assembly_slice(initializers), .entry = 0};
    }
    if(program->failed) { AssemblyFree(program); return NULL; }
    return program;
}

const LirProgram *
AssemblyView(const AssemblyProgram *program)
{
    return &program->result;
}

void
AssemblyFree(AssemblyProgram *program)
{
    if(program == NULL) return;
    for(int index = 0; index < program->probes.count; index++)
        free(((ZirFunction **)program->probes.data)[index]->exprs);
    AssemblyAllocation *allocation = program->allocations;
    while(allocation) {
        AssemblyAllocation *next = allocation->next;
        free(allocation);
        allocation = next;
    }
    free(program);
}

static void
assembly_print_string(FILE *output, String value)
{
    const unsigned char *bytes = (const unsigned char *)value.data;
    fputc('"', output);
    for(size_t index = 0; index < value.length; index++) {
        unsigned char byte = bytes[index];
        if(byte == '"' || byte == '\\') fprintf(output, "\\%c", byte);
        else if(byte < 32 || byte >= 127) fprintf(output, "\\x%02x", byte);
        else fputc(byte, output);
    }
    fputc('"', output);
}

int
AssemblyPrintLir(const LirProgram *view, FILE *output)
{
    fprintf(output, "lir %d entry @%d\n", view->version, view->entry);
    String *modules = view->modules.data;
    for(int index = 0; index < view->modules.length; index++) {
        fprintf(output, "module m%d ", index);
        assembly_print_string(output, modules[index]);
        fputc('\n', output);
    }
    String *sources = view->sources.data;
    for(int index = 0; index < view->sources.length; index++) {
        fprintf(output, "source s%d ", index);
        assembly_print_string(output, sources[index]);
        fputc('\n', output);
    }
    LirType *types = view->types.data;
    for(int index = 0; index < view->types.length; index++)
    {
        LirType type = types[index];
        fprintf(output, "type t%d kind=%d bits=%d signed=%d native=%d element=t%d count=%lld result=t%d fields=(",
            index, type.kind, type.bits, type.is_signed, type.native_width, type.element,
            (long long)type.count, type.result);
        int *fields = type.fields.data;
        for(int field = 0; field < type.fields.length; field++) fprintf(output, "%st%d", field ? "," : "", fields[field]);
        fputs(") parameters=(", output);
        int *parameters = type.parameters.data;
        for(int parameter = 0; parameter < type.parameters.length; parameter++)
            fprintf(output, "%st%d", parameter ? "," : "", parameters[parameter]);
        fputs(")\n", output);
    }
    LirConstant *constants = view->constants.data;
    for(int index = 0; index < view->constants.length; index++) {
        fprintf(output, "constant c%d:t%d = 0x%016llx bytes=", index, constants[index].type,
            (unsigned long long)constants[index].bits);
        unsigned char *bytes = constants[index].bytes.data;
        for(int64_t offset = 0; offset < constants[index].bytes.length; offset++) fprintf(output, "%02x", bytes[offset]);
        fputc('\n', output);
    }
    LirGlobal *globals = view->globals.data;
    for(int index = 0; index < view->globals.length; index++) {
        LirGlobal global = globals[index];
        fprintf(output, "global g%d:t%d module=m%d constant=c%d writable=%d name=",
            index, global.type, global.module, global.constant, global.writable);
        assembly_print_string(output, global.name);
        fputc('\n', output);
    }
    LirOrigin *origins = view->origins.data;
    for(int index = 0; index < view->origins.length; index++) {
        LirOrigin origin = origins[index];
        fprintf(output, "origin o%d source=s%d line=%d column=%d end_line=%d end_column=%d begin=%lld end=%lld parent=o%d\n",
            index, origin.file, origin.line, origin.column, origin.end_line, origin.end_column,
            (long long)origin.begin, (long long)origin.end, origin.parent);
    }
    LirFunction *functions = view->functions.data;
    for(int function = 0; function < view->functions.length; function++) {
        LirFunction body = functions[function];
        fprintf(output, "function @%d module=m%d name=", function, body.module);
        assembly_print_string(output, body.name);
        fprintf(output, " result=t%d entry=b%d calling=%d external=%d variadic=%d origin=o%d parameters=(",
            body.result, body.entry, body.calling, body.external, body.variadic, body.origin);
        int *parameter_types = body.parameters.data;
        for(int parameter = 0; parameter < body.parameters.length; parameter++)
            fprintf(output, "%st%d", parameter ? "," : "", parameter_types[parameter]);
        fputs(") values=(", output);
        int *value_types = body.values.data;
        for(int value = 0; value < body.values.length; value++)
            fprintf(output, "%sv%d:t%d", value ? "," : "", value, value_types[value]);
        fputs(")\n", output);
        LirBlock *blocks = body.blocks.data;
        int *types_of_values = body.values.data;
        for(int block = 0; block < body.blocks.length; block++) {
            fprintf(output, "  b%d(", block);
            int *parameters = blocks[block].parameters.data;
            for(int parameter = 0; parameter < blocks[block].parameters.length; parameter++)
                fprintf(output, "%sv%d:t%d", parameter ? ", " : "", parameters[parameter], types_of_values[parameters[parameter]]);
            fputs("):\n", output);
            LirInstruction *instructions = blocks[block].instructions.data;
            for(int index = 0; index < blocks[block].instructions.length; index++) {
                LirInstruction instruction = instructions[index];
                String name = compiler_lowered_ir_LirOpName(instruction.op);
                fprintf(output, "    i%d ", index);
                if(instruction.result >= 0) fprintf(output, "v%d:t%d = ", instruction.result, types_of_values[instruction.result]);
                fprintf(output, "%.*s", (int)name.length, name.data);
                int *operands = instruction.operands.data;
                for(int operand = 0; operand < instruction.operands.length; operand++)
                    fprintf(output, " v%d", operands[operand]);
                if(instruction.symbol >= 0) fprintf(output, " symbol=%d", instruction.symbol);
                fprintf(output, " immediate=%llu alignment=%u", (unsigned long long)instruction.immediate, instruction.alignment);
                LirEdge *edges = instruction.edges.data;
                for(int edge = 0; edge < instruction.edges.length; edge++) {
                    fprintf(output, " -> b%d(", edges[edge].block);
                    int *arguments = edges[edge].arguments.data;
                    for(int argument = 0; argument < edges[edge].arguments.length; argument++)
                        fprintf(output, "%sv%d", argument ? "," : "", arguments[argument]);
                    fputc(')', output);
                }
                fprintf(output, " effect=%d origin=o%d\n", instruction.effect, instruction.origin);
            }
        }
    }
    return !ferror(output);
}
