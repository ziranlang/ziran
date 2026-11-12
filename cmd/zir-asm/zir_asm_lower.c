#include "zir_asm_lower.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
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
} AssemblyFunction;

struct AssemblyProgram {
    LirProgram result;
    AssemblyAllocation *allocations;
    AssemblyVector modules;
    AssemblyVector sources;
    AssemblyVector constants;
    AssemblyVector globals;
    AssemblyVector origins;
    AssemblyVector functions;
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
    LirOrigin origin = {.file = file, .line = span.line > 0 ? span.line : 1,
        .column = span.column > 0 ? span.column : 1,
        .end_line = span.end_line, .end_column = span.end_column,
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
    if(name[0] == '*') {
        int element = assembly_type_in(program, module, name + 1, span, depth + 1);
        int identity = assembly_pointer_type(program, element, span);
        if(program->failed) return -1;
        AssemblyType binding = {module, assembly_text(program, name), NULL, identity};
        return assembly_push(program, &program->type_names, &binding) ? identity : -1;
    }
    if(!strcmp(name, "string")) return assembly_string_type(program, span);
    LirType type = {0};
    if(!strcmp(name, "float32") || !strcmp(name, "float64") || !strcmp(name, "real")) {
        type.kind = LirTypeKind_Float;
        type.bits = !strcmp(name, "float32") ? 32 : 64;
    } else if(ArrayValueType(name)) type.kind = LirTypeKind_Array;
    else if(record && record->is_procedure_type && !record->is_c_call)
        type.kind = LirTypeKind_Function;
    else if(record && !record->is_procedure_type && !record->is_union && !record->is_record_template &&
            !record->is_owned_vec && !record->is_map && !record->is_extern && !record->foreign_target[0])
        type.kind = LirTypeKind_Record;
    else return assembly_fail(program, span, "this type has no portable lowered representation yet");
    int identity = program->types.count;
    AssemblyType binding = {module, assembly_text(program, name), record, identity};
    if(!assembly_push(program, &program->types, &type) ||
       !assembly_push(program, &program->type_names, &binding)) return -1;
    if(type.kind == LirTypeKind_Array) {
        char element[ZIR_NAME_MAX];
        int count = 0;
        if(!ArrayElementType(name, element, sizeof(element), &count) || count < 0)
            return assembly_fail(program, span, "invalid checked fixed-array type");
        int element_type = assembly_type_in(program, module, element, span, depth + 1);
        assembly_type_at(program, identity)->element = element_type;
        assembly_type_at(program, identity)->count = count;
    } else if(type.kind == LirTypeKind_Function) {
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
    }
    return program->failed ? -1 : identity;
}

static int
assembly_type(AssemblyProgram *program, AssemblyFunction *function, const char *name, ZirSourceSpan span)
{
    return assembly_type_in(program, function->module, name, span, 0);
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
        if(assembly_type_at(program, type)->kind == LirTypeKind_Function)
            return assembly_fail(program, source->span, "global procedure slots are not lowered yet");
        int constant = -1;
        if(source->init && *source->init) {
            LirType value_type = *assembly_type_at(program, type);
            if(value_type.kind != LirTypeKind_Integer && value_type.kind != LirTypeKind_Bool)
                return assembly_fail(program, source->span, "only zero defaults and integer/bool constant global initializers are lowered yet");
            unsigned long long literal = 0;
            int negative = 0;
            int64_t evaluated = 0;
            uint64_t bits;
            if(ZirSignedIntegerLiteral(source->init, &literal, &negative))
                bits = (uint64_t)literal;
            else if(EvaluateCompileExpression(owner, source->init, source->span, 0, &evaluated))
                bits = (uint64_t)evaluated;
            else return assembly_fail(program, source->span, "runtime global initialization is not lowered yet");
            if(value_type.bits > 0 && value_type.bits < 64)
                bits &= (UINT64_C(1) << value_type.bits) - 1;
            LirConstant initial = {.type = type, .bits = bits};
            constant = program->constants.count;
            if(!assembly_push(program, &program->constants, &initial)) return -1;
        }
        const char *copied_name = assembly_text(program, source->name);
        if(program->failed) return -1;
        AssemblyGlobal global = {.module = owner, .source = source,
            .result = {.module = assembly_name(program, &program->modules, owner->name),
                .name = {copied_name, strlen(copied_name)}, .type = type,
                .constant = constant, .writable = 1}};
        symbol = program->globals.count;
        if(!assembly_push(program, &program->globals, &global)) return -1;
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
        if(expression->kind != ZIR_EXPR_UNARY || strcmp(expression->op, "&")) continue;
        int target = expression->right;
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
    if(type.kind != LirTypeKind_Bool && type.kind != LirTypeKind_Integer &&
       type.kind != LirTypeKind_Float && type.kind != LirTypeKind_Pointer)
        return assembly_fail(program, span, type.kind == LirTypeKind_Record || type.kind == LirTypeKind_Array ?
            "addresses of aggregate locals are not lowered yet" :
            "this local type cannot use checked frame allocation yet");
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

static int
assembly_zero(AssemblyProgram *program, AssemblyFunction *function, int identity,
              ZirSourceSpan span, int depth)
{
    if(!assembly_spend(program, span)) return -1;
    if(depth > 256) return assembly_fail(program, span, "aggregate initialization nesting budget exhausted");
    LirType type = *assembly_type_at(program, identity);
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
    if(binding < 0) return assembly_fail(program, span, "callable receiver is not a lowered local binding");
    AssemblyBinding current = ((AssemblyBinding *)function->bindings.data)[binding];
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
    int value = assembly_expression(program, function, expression->left, depth + 1);
    if(program->failed) return -1;
    int identity = ((int *)function->values.data)[value];
    LirType type = *assembly_type_at(program, identity);
    int member = -1;
    if(expression->kind == ZIR_EXPR_INDEX) {
        if(expression->right < 0 || expression->right >= function->source->expr_count)
            return assembly_fail(program, expression->span, "invalid checked array index");
        const ZirExpr *index = &function->source->exprs[expression->right];
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
    } else if(identity == program->string_type && program->string_type > 0) {
        if(strcmp(expression->name, "count") && strcmp(expression->name, "data"))
            return assembly_fail(program, expression->span, "string has only data and count members");
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
    if(expression->kind == ZIR_EXPR_MEMBER || expression->kind == ZIR_EXPR_INDEX)
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
            return assembly_fail(program, expression->span, "address requires a promoted scalar local");
        return ((AssemblyBinding *)function->bindings.data)[binding].value;
    }
    if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "*"))
        return assembly_expression(program, function, expression->right, depth + 1);
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
        int initialized = assembly_expression(program, function, initializer->right, depth + 1);
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
assembly_expression(AssemblyProgram *program, AssemblyFunction *function, int root, int depth)
{
    if(!assembly_spend(program, function->source->span)) return -1;
    if(depth > 256 || root < 0 || root >= function->source->expr_count)
        return assembly_fail(program, function->source->span, "invalid or excessively deep expression graph");
    const ZirExpr *expression = &function->source->exprs[root];
    if(expression->is_move)
        return assembly_fail(program, expression->span, "owned moves are not lowered yet");
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
    if(expression->kind == ZIR_EXPR_INT) {
        unsigned long long bits = 0;
        if(!ZirIntegerLiteral(expression->text, &bits))
            return assembly_fail(program, expression->span, "invalid checked integer literal");
        if(assembly_type_at(program, type)->kind == LirTypeKind_Float) {
            double converted = (double)bits;
            if(assembly_type_at(program, type)->bits == 32) {
                float narrowed = (float)converted;
                uint32_t representation;
                memcpy(&representation, &narrowed, sizeof(representation));
                bits = representation;
            } else memcpy(&bits, &converted, sizeof(bits));
        }
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
    if(expression->kind == ZIR_EXPR_INDEX && assembly_addressable(program, function, root, depth + 1)) {
        int address = assembly_address(program, function, root, depth + 1);
        return assembly_emit(program, function, LirOp_Load, type, &address, 1, -1, expression->span);
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
        int operands[2] = {assembly_expression(program, function, expression->left, depth + 1), -1};
        if(!strcmp(expression->op, "&&") || !strcmp(expression->op, "||")) {
            int is_or = !strcmp(expression->op, "||");
            return assembly_select(program, function, expression, operands[0], expression->right,
                expression->right, is_or ? 1 : 2, depth + 1);
        }
        operands[1] = assembly_expression(program, function, expression->right, depth + 1);
        if(program->failed) return -1;
        LirOp op = assembly_binary(expression->op);
        if(op == LirOp_Invalid) return assembly_fail(program, expression->span, "unsupported binary operation");
        int left_type = ((int *)function->values.data)[operands[0]];
        int right_type = ((int *)function->values.data)[operands[1]];
        int compare = op >= LirOp_Equal && op <= LirOp_GreaterEqual;
        int common = compare ? left_type : type;
        if(compare && left_type != right_type) {
            LirType left = *assembly_type_at(program, left_type);
            LirType right = *assembly_type_at(program, right_type);
            if(left.kind == LirTypeKind_Float || right.kind == LirTypeKind_Float) {
                if(left.kind != LirTypeKind_Float || (right.kind == LirTypeKind_Float && right.bits > left.bits))
                    common = right_type;
            } else if(left.is_signed != right.is_signed)
                return assembly_fail(program, expression->span, "mixed signed comparisons require explicit casts in this backend");
            else if(right.bits > left.bits) common = right_type;
        }
        operands[0] = assembly_convert(program, function, operands[0], common, expression->span);
        if(op != LirOp_ShiftLeft && op != LirOp_ShiftRight)
            operands[1] = assembly_convert(program, function, operands[1], common, expression->span);
        return assembly_emit(program, function, op, compare ? 1 : type, operands, 2, -1, expression->span);
    }
    if(expression->kind == ZIR_EXPR_CALL) {
        int indirect = expression->slot_type && *expression->slot_type;
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
            int value = assembly_expression(program, function, child, depth + 1);
            values[position] = assembly_convert(program, function, value,
                ((int *)parameters.data)[position], argument->span);
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

static int
assembly_if(AssemblyProgram *program, AssemblyFunction *function, int index, int end, int depth)
{
    if(depth > 256) {
        assembly_fail(program, function->source->span, "branch nesting budget exhausted");
        return end;
    }
    const ZirStmt *statement = &function->source->stmts[index];
    int close = assembly_close(program, function->source, index, end);
    int count = function->bindings.count;
    AssemblyBinding *before = assembly_snapshot(program, function, count);
    int condition = assembly_expression(program, function, statement->expr_root, 0);
    int yes = assembly_block(program, function, before, count);
    int no = assembly_block(program, function, before, count);
    if(program->failed) return end;
    assembly_branch(program, function, condition, yes, no, count, statement->span);
    assembly_enter(function, yes, before, count);
    assembly_sequence(program, function, index + 1, close, depth + 1);
    int yes_end = function->current;
    AssemblyBinding *yes_bindings = assembly_snapshot(program, function, count);
    if(program->failed) return end;
    assembly_enter(function, no, before, count);
    int next = close + 1;
    if(next < end && function->source->stmts[next].is_else) {
        if(function->source->stmts[next].expr_root >= 0)
            next = assembly_if(program, function, next, end, depth + 1);
        else {
            int otherwise_close = assembly_close(program, function->source, next, end);
            assembly_sequence(program, function, next + 1, otherwise_close, depth + 1);
            next = otherwise_close + 1;
        }
    }
    int no_end = function->current;
    AssemblyBinding *no_bindings = assembly_snapshot(program, function, count);
    if(program->failed) return end;
    if(yes_end < 0 && no_end < 0) { function->current = -1; return next; }
    int join = assembly_block(program, function, before, count);
    if(program->failed) return end;
    if(yes_end >= 0) {
        function->current = yes_end;
        if(count) memcpy(function->bindings.data, yes_bindings, (size_t)count * sizeof(*before));
        assembly_jump(program, function, join, count, statement->span);
    }
    if(no_end >= 0) {
        function->current = no_end;
        if(count) memcpy(function->bindings.data, no_bindings, (size_t)count * sizeof(*before));
        assembly_jump(program, function, join, count, statement->span);
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
            int identity = assembly_type(program, function, function->source->exprs[statement->lhs_root].type, statement->span);
            int address = -1;
            AssemblyLocalPlace *destination = NULL;
            if(assembly_addressable(program, function, statement->lhs_root, 0))
                address = assembly_address(program, function, statement->lhs_root, 0);
            else destination = assembly_capture_local(program, function, statement->lhs_root, 0);
            if(program->failed) break;
            if(strcmp(statement->assignment_op, "="))
                previous = address >= 0 || destination->storage ?
                    assembly_emit(program, function, LirOp_Load, identity,
                        address >= 0 ? &address : &destination->value, 1, -1, statement->span) :
                    destination->value;
            int value = assembly_expression(program, function, statement->expr_root, 0);
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
            assembly_expression(program, function, statement->expr_root, 0);
        } else if(statement->kind == ZIR_STMT_RETURN) {
            int value = -1;
            if(function->result.result)
                value = assembly_convert(program, function, assembly_expression(program, function, statement->expr_root, 0),
                    function->result.result, statement->span);
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
    if((!host && (!source->checked || source->is_extern)) || source->is_template || source->is_global_initializer ||
       source->simd_target || (source->assembly_text && *source->assembly_text))
        return assembly_fail(program, source->span, "direct lowering requires a concrete checked function without runtime initializers, non-host foreign services or native variants");
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
    if(strncmp(import->target, "ziran:", 6) == 0)
        return assembly_fail(program, span, "bound host providers are not lowered yet");
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

AssemblyProgram *
AssemblyLower(const ZirProgram *source, const char *module_name, const char *function_name)
{
    AssemblyProgram *program = calloc(1, sizeof(*program));
    if(program == NULL) { DiagnosticOutOfMemory(); return NULL; }
    program->work = 16000000;
    program->modules.width = sizeof(String);
    program->sources.width = sizeof(String);
    program->constants.width = sizeof(LirConstant);
    program->globals.width = sizeof(AssemblyGlobal);
    program->origins.width = sizeof(LirOrigin);
    program->functions.width = sizeof(AssemblyFunction *);
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
    for(int module_index = 0; module_index < source->module_count && !program->failed; module_index++) {
        const ZirModule *candidate = &source->modules[module_index];
        for(int function_index = 0; function_index < candidate->function_count; function_index++) {
            if(!assembly_spend(program, candidate->functions[function_index].span)) break;
            if(candidate->functions[function_index].is_global_initializer) {
                assembly_fail(program, candidate->functions[function_index].span,
                    "runtime module initialization is not lowered yet");
                break;
            }
        }
    }
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
        assembly_sequence(program, builder, 0, builder->source->stmt_count, 0);
        if(builder->current >= 0 && !program->failed) {
            if(builder->result.result) assembly_fail(program, builder->source->span, "scalar procedure falls through without a return");
            else assembly_emit(program, builder, LirOp_Return, 0, NULL, 0, -1, builder->source->span);
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
    if(!program->failed) {
        for(int index = 0; index < program->functions.count; index++)
            functions[index] = ((AssemblyFunction **)program->functions.data)[index]->result;
        for(int index = 0; index < program->globals.count; index++)
            globals[index] = ((AssemblyGlobal *)program->globals.data)[index].result;
        program->result = (LirProgram){.version = LirVersion, .modules = assembly_slice(program->modules),
            .sources = assembly_slice(program->sources), .types = assembly_slice(program->types),
            .constants = assembly_slice(program->constants), .globals = {globals, program->globals.count},
            .functions = {functions, program->functions.count},
            .origins = assembly_slice(program->origins), .entry = 0};
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
