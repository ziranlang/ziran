/*
 * zir_wasm_lower.c - checked Ziran programs to a wasm32 module.
 *
 * The walker follows the zi2py model: statements are the checked body's
 * linear array and expressions its index graph. Scalars map to wasm
 * locals of i32/i64/f32/f64; sub-width integers stay normalized in their
 * container so arithmetic wraps exactly as the VM's values do. The step
 * guard import mirrors ZIR_ENTER/ZIR_STEP: one count per function entry
 * and per loop iteration.
 */
#include "zir_wasm_lower.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_text.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { ZX_WASM_MAX_PARAMS = 64 };

/* ---------------------------------------------------------------- bytes */

typedef struct WasmBytes {
    unsigned char *data;
    size_t count;
    size_t capacity;
} WasmBytes;

static void
bytes_free(WasmBytes *bytes)
{
    free(bytes->data);
    bytes->data = NULL;
    bytes->count = bytes->capacity = 0;
}

static int
bytes_reserve(WasmBytes *bytes, size_t extra)
{
    if(bytes->count + extra <= bytes->capacity)
        return 1;
    size_t capacity = bytes->capacity ? bytes->capacity * 2 : 256;
    while(capacity < bytes->count + extra)
        capacity *= 2;
    unsigned char *grown = realloc(bytes->data, capacity);
    if(grown == NULL)
        return 0;
    bytes->data = grown;
    bytes->capacity = capacity;
    return 1;
}

static int
byte(WasmBytes *bytes, unsigned char value)
{
    if(!bytes_reserve(bytes, 1))
        return 0;
    bytes->data[bytes->count++] = value;
    return 1;
}

static int
raw(WasmBytes *bytes, const void *data, size_t length)
{
    if(!bytes_reserve(bytes, length))
        return 0;
    memcpy(bytes->data + bytes->count, data, length);
    bytes->count += length;
    return 1;
}

static int
uleb(WasmBytes *bytes, uint64_t value)
{
    do {
        unsigned char part = value & 0x7f;
        value >>= 7;
        if(value != 0)
            part |= 0x80;
        if(!byte(bytes, part))
            return 0;
    } while(value != 0);
    return 1;
}

static int
sleb64(WasmBytes *bytes, int64_t value)
{
    int more = 1;
    while(more) {
        unsigned char part = value & 0x7f;
        value >>= 7;
        if((value == 0 && !(part & 0x40)) || (value == -1 && (part & 0x40)))
            more = 0;
        else
            part |= 0x80;
        if(!byte(bytes, part))
            return 0;
    }
    return 1;
}

static int
sleb32(WasmBytes *bytes, int32_t value)
{
    return sleb64(bytes, value);
}

/* --------------------------------------------------------------- typing */

enum { WASM_VOID = 0, WASM_I32 = 0x7f, WASM_I64 = 0x7e, WASM_F32 = 0x7d,
       WASM_F64 = 0x7c };
typedef int WasmValtype;

/* Unsuffixed literals carry the meta-types "integer" and "real"; their
 * natural containers are s64 and float64, as the VM's value kinds say. */
static const char *
concrete_type(const char *type)
{
    if(type != NULL && !strcmp(type, "integer"))
        return "s64";
    if(type != NULL && !strcmp(type, "real"))
        return "float64";
    return type;
}

/* Records and fixed arrays live in the module's arena; values of those
 * types are i32 addresses. Slices, strings and pointers stay outside the
 * subset for now. */
static int
type_is_heap(const ZirModule *module, const char *type)
{
    if(module == NULL || type == NULL || !*type)
        return 0;
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(ArrayElementType(type, element, sizeof(element), &capacity))
        return 1;
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    return record != NULL && !record->is_enum && !record->is_procedure_type &&
           !record->is_extern && !record->is_map && !record->is_record_template &&
           !record->is_type_instance && !record->is_union;
}

/* Scalar mapping only: callers that must never treat heap pointers as
 * plain i32 use this and reject heap types beforehand. */
static WasmValtype
wasm_scalar_type_of(const char *type)
{
    const char *scalar = ScalarType(concrete_type(type));
    if(!strcmp(scalar, "bool") || !strcmp(scalar, "s8") || !strcmp(scalar, "u8") ||
       !strcmp(scalar, "s16") || !strcmp(scalar, "u16") ||
       !strcmp(scalar, "s32") || !strcmp(scalar, "u32"))
        return WASM_I32;
    if(!strcmp(scalar, "s64") || !strcmp(scalar, "u64"))
        return WASM_I64;
    if(!strcmp(scalar, "float32"))
        return WASM_F32;
    if(!strcmp(scalar, "float64"))
        return WASM_F64;
    return WASM_VOID;
}

static WasmValtype
wasm_type_of_in(const ZirModule *module, const char *type)
{
    if(type_is_heap(module, type))
        return WASM_I32;
    return wasm_scalar_type_of(type);
}

static int
type_is_signed(const char *type)
{
    type = concrete_type(type);
    return type[0] == 's' || type[0] == 'f' || !strcmp(type, "bool");
}

static int
type_is_integer(const char *type)
{
    const char *scalar = ScalarType(concrete_type(type));
    return strcmp(scalar, "float32") && strcmp(scalar, "float64") &&
           strcmp(scalar, "string") && strcmp(scalar, "bool") &&
           strcmp(scalar, "void") && scalar[0] != 0;
}

static int
type_bit_width(const char *type)
{
    const char *scalar = ScalarType(concrete_type(type));
    if(!strcmp(scalar, "s8") || !strcmp(scalar, "u8"))
        return 8;
    if(!strcmp(scalar, "s16") || !strcmp(scalar, "u16"))
        return 16;
    if(!strcmp(scalar, "s32") || !strcmp(scalar, "u32") ||
       !strcmp(scalar, "bool"))
        return 32;
    if(!strcmp(scalar, "s64") || !strcmp(scalar, "u64"))
        return 64;
    return 0;
}

static int
wasm_width(const char *type)
{
    return type_bit_width(type) > 32 ? 64 : 32;
}

/* ---------------------------------------------------------------- plans */

typedef struct WasmLocal {
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    WasmValtype wasm;
} WasmLocal;

enum { ZX_WASM_MAX_REGIONS = 64, ZX_WASM_SCRATCH_SLOTS = 16,
       ZX_WASM_ARENA_BASE = 65536, ZX_WASM_ARENA_PAGES = 64 };

typedef struct WasmPlan {
    const ZirModule *module;
    const ZirFunction *function;
    int type_index;
    int function_index;
    int param_count;
    char result_type[ZIR_NAME_MAX];
    WasmLocal *locals;
    int local_count;
    int local_capacity;
    /* Hidden locals: one arena scratch, one save slot per structured
     * region depth, and a small stack of scratch slots for nested place
     * and literal emission. */
    int arena_scratch;
    int region_saves[ZX_WASM_MAX_REGIONS];
    int region_depth;
    int scratch_slots[16];
    int scratch_depth;
    WasmBytes body;
} WasmPlan;

enum { BLOCK_KIND_PLAIN = 0, BLOCK_KIND_LOOP = 1, BLOCK_KIND_IF = 2 };

typedef struct WasmFrame {
    int kind;
    int loop_id;
    int save_slot; /* loop frames: the arena slot continue restores */
} WasmFrame;

typedef struct WasmEmitter {
    const ZirProgram *program;
    WasmPlan *plans;
    int plan_count;
    WasmFrame frames[256];
    int frame_count;
    int failed;
} WasmEmitter;

/* The binary operator of a compound assignment, or NULL. Mirrors the
 * runtime's assignment_binary_operator table. */
static const char *
wasm_assignment_binary_operator(const char *operation)
{
    struct { const char *assignment, *binary; } table[] = {
        {"+=", "+"}, {"-=", "-"}, {"*=", "*"}, {"/=", "/"}, {"%=", "%"},
        {"&=", "&"}, {"|=", "|"}, {"^=", "^"}, {"<<=", "<<"}, {">>=", ">>"},
        {NULL, NULL},
    };
    for(int i = 0; table[i].assignment; i++)
        if(!strcmp(operation, table[i].assignment))
            return table[i].binary;
    return NULL;
}

static void
fail(WasmEmitter *emitter, ZirSourceSpan span, const char *what)
{
    if(!emitter->failed)
        Diagnostic(span, "zx.wasm", "%s is outside the wasm execution subset",
                   what);
    emitter->failed = 1;
}

static int
block_end(const ZirFunction *function, int begin, int end)
{
    int depth = 1;
    for(int index = begin + 1; index < end; index++) {
        ZirStmtKind kind = function->stmts[index].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE ||
           kind == ZIR_STMT_BLOCK_OPEN)
            depth++;
        else if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0)
            return index;
    }
    return end;
}

static WasmPlan *
plan_of(WasmEmitter *emitter, const ZirModule *owner, const char *name)
{
    for(int i = 0; i < emitter->plan_count; i++)
        if(emitter->plans[i].module == owner &&
           !strcmp(emitter->plans[i].function->name, name))
            return &emitter->plans[i];
    return NULL;
}

static int
add_local(WasmPlan *plan, const char *name, const char *type, WasmValtype wasm)
{
    if(plan->local_count == plan->local_capacity) {
        int capacity = plan->local_capacity ? plan->local_capacity * 2 : 16;
        WasmLocal *grown = realloc(plan->locals,
                                   (size_t)capacity * sizeof(*grown));
        if(grown == NULL)
            return -1;
        plan->locals = grown;
        plan->local_capacity = capacity;
    }
    WasmLocal *local = &plan->locals[plan->local_count];
    snprintf(local->name, sizeof(local->name), "%s", name);
    snprintf(local->type, sizeof(local->type), "%s", type);
    local->wasm = wasm;
    return plan->local_count++;
}

static int
find_local(WasmPlan *plan, const char *name, char *type)
{
    for(int i = plan->local_count - 1; i >= 0; i--)
        if(!strcmp(plan->locals[i].name, name)) {
            if(type != NULL)
                snprintf(type, ZIR_NAME_MAX, "%s", plan->locals[i].type);
            return i;
        }
    return -1;
}

/* ------------------------------------------------------- layouts */

/* Byte size and alignment of a heap type (record or fixed array). */
static int
heap_layout(const ZirModule *module, const char *type, size_t *size,
            size_t *alignment)
{
    return TypeLayout(module, type, size, alignment);
}

/* The aligned offset of one record field, mirroring the checker's
 * layout: fields in declaration order, each aligned naturally. */
static int
field_place(const ZirModule *module, const char *record_type,
            const char *field_name, size_t *offset, char *field_type)
{
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, record_type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_extern || record->is_map || record->is_record_template ||
       record->is_type_instance || record->is_union)
        return 0;
    owner = owner ? owner : module;
    size_t at = 0, cursor = 0;
    ZirTypeField field;
    int result;
    while((result = TypeNextField(record, &cursor, &field)) == 1) {
        size_t field_size, field_alignment;
        if(!TypeLayout(owner, field.type, &field_size, &field_alignment))
            return 0;
        size_t pad = at % field_alignment;
        if(pad != 0)
            at += field_alignment - pad;
        if(strcmp(field.name, field_name) == 0) {
            *offset = at;
            snprintf(field_type, ZIR_NAME_MAX, "%s", field.type);
            return 1;
        }
        at += field_size;
    }
    return 0;
}

/* ------------------------------------------------------------ opcodes */

static int
op(WasmBytes *code, unsigned char value)
{
    return byte(code, value);
}

static int
op_index(WasmBytes *code, unsigned char value, int index)
{
    return byte(code, value) && uleb(code, (uint64_t)index);
}

static int
op_const_i32(WasmBytes *code, int32_t value)
{
    return byte(code, 0x41) && sleb32(code, value);
}

static int
op_const_i64(WasmBytes *code, int64_t value)
{
    return byte(code, 0x42) && sleb64(code, value);
}

static int
op_const_f32(WasmBytes *code, float value)
{
    unsigned char bits[4];
    uint32_t pattern;
    memcpy(&pattern, &value, sizeof(pattern));
    for(int i = 0; i < 4; i++)
        bits[i] = (unsigned char)(pattern >> (8 * i));
    return byte(code, 0x43) && raw(code, bits, 4);
}

static int
op_const_f64(WasmBytes *code, double value)
{
    unsigned char bits[8];
    uint64_t pattern;
    memcpy(&pattern, &value, sizeof(pattern));
    for(int i = 0; i < 8; i++)
        bits[i] = (unsigned char)(pattern >> (8 * i));
    return byte(code, 0x44) && raw(code, bits, 8);
}

static int
op_local(WasmBytes *code, unsigned char value, int index)
{
    return op_index(code, value, index);
}

/* Scalar load of one field: natural alignment, static offset. */
static int
op_load_scalar(WasmBytes *code, const char *type, int offset)
{
    const char *scalar = ScalarType(concrete_type(type));
    unsigned char opcode;
    int align = 2;
    if(!strcmp(scalar, "s8")) { opcode = 0x2c; align = 0; }        /* i32.load8_s */
    else if(!strcmp(scalar, "u8") || !strcmp(scalar, "bool")) { opcode = 0x2d; align = 0; }
    else if(!strcmp(scalar, "s16")) { opcode = 0x2e; align = 1; }  /* i32.load16_s */
    else if(!strcmp(scalar, "u16")) { opcode = 0x2f; align = 1; }
    else if(!strcmp(scalar, "s64") || !strcmp(scalar, "u64")) opcode = 0x29;
    else if(!strcmp(scalar, "float32")) opcode = 0x2a;
    else if(!strcmp(scalar, "float64")) opcode = 0x2b;
    else opcode = 0x28;                                            /* i32.load */
    return byte(code, opcode) && uleb(code, (uint64_t)align) &&
           uleb(code, (uint64_t)(offset < 0 ? 0 : offset));
}

/* Scalar store of one field. */
static int
op_store_scalar(WasmBytes *code, const char *type, int offset)
{
    const char *scalar = ScalarType(concrete_type(type));
    unsigned char opcode;
    int align = 2;
    if(!strcmp(scalar, "s8") || !strcmp(scalar, "u8") ||
       !strcmp(scalar, "bool")) { opcode = 0x3a; align = 0; }      /* i32.store8 */
    else if(!strcmp(scalar, "s16") || !strcmp(scalar, "u16")) { opcode = 0x3b; align = 1; }
    else if(!strcmp(scalar, "s64") || !strcmp(scalar, "u64")) opcode = 0x37;
    else if(!strcmp(scalar, "float32")) opcode = 0x38;
    else if(!strcmp(scalar, "float64")) opcode = 0x39;
    else opcode = 0x36;                                            /* i32.store */
    return byte(code, opcode) && uleb(code, (uint64_t)align) &&
           uleb(code, (uint64_t)(offset < 0 ? 0 : offset));
}

/* memory.copy with dst and src addresses already on the stack. */
static int
op_memory_copy(WasmBytes *code, size_t size)
{
    return op_const_i32(code, (int32_t)size) &&
           byte(code, 0xfc) && uleb(code, 10) &&
           byte(code, 0x00) && byte(code, 0x00);
}

/* memory.fill with the address already on the stack. */
static int
op_memory_zero(WasmBytes *code, size_t size)
{
    return op_const_i32(code, 0) && op_const_i32(code, (int32_t)size) &&
           byte(code, 0xfc) && uleb(code, 11) && byte(code, 0x00);
}

/* One aligned arena allocation; leaves the address on the stack. */
static int
op_arena_alloc(WasmPlan *plan, size_t size, size_t alignment)
{
    if(alignment < 1)
        alignment = 1;
    size_t pad = size % alignment;
    if(pad != 0)
        size += alignment - pad;
    return op_index(&plan->body, 0x23, 1) &&           /* global.get $arena */
           op_local(&plan->body, 0x21, plan->arena_scratch) &&
           op_local(&plan->body, 0x20, plan->arena_scratch) &&
           op_const_i32(&plan->body, (int32_t)size) &&
           op(&plan->body, 0x6a) &&                    /* i32.add */
           op_index(&plan->body, 0x24, 1) &&           /* global.set $arena */
           op_local(&plan->body, 0x20, plan->arena_scratch);
}

/* Region discipline: allocations inside a structured region die at its
 * exit, so the arena pointer is saved on entry and restored on exit. */
static int
region_enter(WasmPlan *plan)
{
    if(plan->region_depth >= ZX_WASM_MAX_REGIONS)
        return 0;
    int slot = plan->region_saves[plan->region_depth];
    plan->region_depth++;
    return op_index(&plan->body, 0x23, 1) &&           /* global.get $arena */
           op_local(&plan->body, 0x21, slot);
}

static int
region_leave(WasmPlan *plan)
{
    if(plan->region_depth <= 0)
        return 0;
    plan->region_depth--;
    int slot = plan->region_saves[plan->region_depth];
    return op_local(&plan->body, 0x20, slot) &&
           op_index(&plan->body, 0x24, 1);             /* global.set $arena */
}

static int
op_zero(WasmBytes *code, WasmValtype wasm)
{
    switch(wasm) {
    case WASM_I32: return op_const_i32(code, 0);
    case WASM_I64: return op_const_i64(code, 0);
    case WASM_F32: return op_const_f32(code, 0.0f);
    case WASM_F64: return op_const_f64(code, 0.0);
    default: return 0;
    }
}

/* Clamp the container's top of stack to the declared integer width. */
static int
op_normalize(WasmBytes *code, const char *type)
{
    if(!type_is_integer(type))
        return 1;
    int width = type_bit_width(type);
    if(width >= 32 || width == 0)
        return 1;
    int shift = 32 - width;
    if(type_is_signed(type))
        return op_const_i32(code, shift) && op(code, 0x74) /* shl */ &&
               op_const_i32(code, shift) && op(code, 0x75); /* shr_s */
    uint32_t mask = (uint32_t)((1ULL << width) - 1);
    return op_const_i32(code, (int32_t)mask) && op(code, 0x71); /* and */
}

/* Convert the stack top from one scalar type to another. */
static int
op_convert(WasmBytes *code, const char *from, const char *to)
{
    WasmValtype source = wasm_scalar_type_of(from);
    WasmValtype target = wasm_scalar_type_of(to);
    if(!source || !target)
        return 0;
    if(source == target)
        return 1;
    /* Integer container changes: widen with the operand's signedness, or
     * wrap and clamp to the destination width. */
    if(source == WASM_I64 && target == WASM_I32)
        return op(code, 0xa7) && op_normalize(code, to); /* i32.wrap_i64 */
    if(source == WASM_I32 && target == WASM_I64)
        return op(code, type_is_signed(from) ? 0xac : 0xad) &&
               op_normalize(code, to); /* extend_i32_s/u */
    if(source == WASM_F32 && target == WASM_F64)
        return op(code, 0xbb); /* promote */
    if(source == WASM_F64 && target == WASM_F32)
        return op(code, 0xb6); /* demote */
    if((source == WASM_F32 || source == WASM_F64) &&
       (target == WASM_I32 || target == WASM_I64)) {
        int signed_to = type_is_signed(to);
        if(source == WASM_F32 && target == WASM_I32)
            return op(code, signed_to ? 0xa8 : 0xa9) && op_normalize(code, to);
        if(source == WASM_F64 && target == WASM_I32)
            return op(code, signed_to ? 0xaa : 0xab) && op_normalize(code, to);
        if(source == WASM_F32)
            return op(code, signed_to ? 0xae : 0xaf) && op_normalize(code, to);
        return op(code, signed_to ? 0xb0 : 0xb1) && op_normalize(code, to);
    }
    if((source == WASM_I32 || source == WASM_I64) &&
       (target == WASM_F32 || target == WASM_F64)) {
        int signed_from = type_is_signed(from);
        if(source == WASM_I32 && target == WASM_F32)
            return op(code, signed_from ? 0xb2 : 0xb3);
        if(source == WASM_I32)
            return op(code, signed_from ? 0xb7 : 0xb8);
        if(target == WASM_F32)
            return op(code, signed_from ? 0xb4 : 0xb5);
        return op(code, signed_from ? 0xb9 : 0xba);
    }
    return 0;
}

/* ---------------------------------------------------------- expressions */

static int emit_expression(WasmEmitter *emitter, WasmPlan *plan, int index);

/* Emits the address of a place expression (identifier, member or index)
 * onto the stack. Returns 0 after a diagnostic for anything else. */
static int
emit_address(WasmEmitter *emitter, WasmPlan *plan, int index)
{
    const ZirFunction *function = plan->function;
    if(index < 0 || index >= function->expr_count)
        return 0;
    const ZirExpr *expression = &function->exprs[index];
    switch(expression->kind) {
    case ZIR_EXPR_IDENT: {
        char type[ZIR_NAME_MAX];
        int local = find_local(plan, expression->name, type);
        if(local < 0) {
            fail(emitter, expression->span, "module global state");
            return 0;
        }
        if(!type_is_heap(plan->module, type)) {
            fail(emitter, expression->span, "scalar address");
            return 0;
        }
        return op_local(&plan->body, 0x20, local);
    }
    case ZIR_EXPR_MEMBER: {
        if(!emit_address(emitter, plan, expression->left))
            return 0;
        size_t offset = 0;
        char field_type[ZIR_NAME_MAX];
        if(!field_place(plan->module,
                        function->exprs[expression->left].type,
                        expression->name, &offset, field_type)) {
            fail(emitter, expression->span, "record field");
            return 0;
        }
        if(offset == 0)
            return 1;
        return op_const_i32(&plan->body, (int32_t)offset) &&
               op(&plan->body, 0x6a); /* i32.add */
    }
    case ZIR_EXPR_INDEX: {
        if(!emit_address(emitter, plan, expression->left))
            return 0;
        char element[ZIR_NAME_MAX];
        int capacity = 0;
        if(!ArrayElementType(function->exprs[expression->left].type,
                             element, sizeof(element), &capacity) ||
           capacity <= 0) {
            fail(emitter, expression->span, "indexed place");
            return 0;
        }
        /* Bounds check against the constant capacity, then scale. */
        if(!emit_expression(emitter, plan, expression->right))
            return 0;
        if(emitter->failed)
            return 0;
        if(strcmp(concrete_type(function->exprs[expression->right].type),
                  "s64") == 0) {
            /* Loop counters are s64; the arena is 32-bit. */
            if(!op(&plan->body, 0xa7)) /* i32.wrap_i64 */
                return 0;
        }
        if(plan->scratch_depth >= ZX_WASM_SCRATCH_SLOTS) {
            fail(emitter, expression->span, "nested index places");
            return 0;
        }
        int slot = plan->scratch_slots[plan->scratch_depth++];
        if(!op_local(&plan->body, 0x21, slot) ||
           !op_local(&plan->body, 0x20, slot) ||
           !op_const_i32(&plan->body, capacity) ||
           !op(&plan->body, 0x4f)) /* idx >=u capacity traps */
            return 0;
        if(!(op(&plan->body, 0x04) && byte(&plan->body, 0x40) &&
             op(&plan->body, 0x00) && op(&plan->body, 0x0b)))
            return 0;
        size_t element_size, element_alignment;
        if(!heap_layout(plan->module, element, &element_size,
                        &element_alignment)) {
            fail(emitter, expression->span, "array element layout");
            return 0;
        }
        int scaled = op_local(&plan->body, 0x20, slot) &&
                     op_const_i32(&plan->body, (int32_t)element_size) &&
                     op(&plan->body, 0x6c) && /* i32.mul */
                     op(&plan->body, 0x6a); /* i32.add */
        plan->scratch_depth--;
        return scaled;
    }
    default:
        fail(emitter, expression->span, "address");
        return 0;
    }
}

static int
emit_convert(WasmEmitter *emitter, WasmPlan *plan, int index, const char *to)
{
    const ZirExpr *expression = &plan->function->exprs[index];
    if(!emit_expression(emitter, plan, index))
        return 0;
    if(emitter->failed)
        return 0;
    if(!op_convert(&plan->body, expression->type, to)) {
        Diagnostic(expression->span, "zx.wasm",
                   "conversion from %s to %s is outside the wasm execution subset",
                   expression->type, to);
        emitter->failed = 1;
        return 0;
    }
    return 1;
}

/* Comparison opcodes indexed [eq, ne, lt, le, gt, ge]. */
static const unsigned char compare_i32_s[6] = {0x46, 0x47, 0x48, 0x4c, 0x4a, 0x4e};
static const unsigned char compare_i32_u[6] = {0x46, 0x47, 0x49, 0x4d, 0x4b, 0x4f};
static const unsigned char compare_i64_s[6] = {0x51, 0x52, 0x53, 0x57, 0x55, 0x59};
static const unsigned char compare_i64_u[6] = {0x51, 0x52, 0x54, 0x58, 0x56, 0x5a};
static const unsigned char compare_f32[6] = {0x5b, 0x5c, 0x5d, 0x5f, 0x5e, 0x60};
static const unsigned char compare_f64[6] = {0x61, 0x62, 0x63, 0x65, 0x64, 0x66};
/* Arithmetic opcodes indexed [add, sub, mul, div, rem]. */
static const unsigned char arith_i32_s[5] = {0x6a, 0x6b, 0x6c, 0x6d, 0x6f};
static const unsigned char arith_i32_u[5] = {0x6a, 0x6b, 0x6c, 0x6e, 0x70};
static const unsigned char arith_i64_s[5] = {0x7c, 0x7d, 0x7e, 0x7f, 0x81};
static const unsigned char arith_i64_u[5] = {0x7c, 0x7d, 0x7e, 0x80, 0x82};
static const unsigned char arith_f32[5] = {0x92, 0x93, 0x94, 0x95, 0x95};
static const unsigned char arith_f64[5] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa3};

static int
is_comparison(const char *operation)
{
    return !strcmp(operation, "==") || !strcmp(operation, "!=") ||
           !strcmp(operation, "<") || !strcmp(operation, "<=") ||
           !strcmp(operation, ">") || !strcmp(operation, ">=");
}

static int
emit_binary(WasmEmitter *emitter, WasmPlan *plan, int index)
{
    const ZirExpr *expression = &plan->function->exprs[index];
    const char *operation = expression->op;
    const char *result_type = expression->type;
    if(!strcmp(operation, "&&") || !strcmp(operation, "||")) {
        int is_and = operation[0] == '&';
        if(!emit_expression(emitter, plan, expression->left))
            return 0;
        /* if (result i32) <right> else <short value> end */
        if(!(op(&plan->body, 0x04) && byte(&plan->body, WASM_I32)))
            return 0;
        if(!emit_expression(emitter, plan, expression->right))
            return 0;
        if(!op(&plan->body, 0x05) || !op_const_i32(&plan->body, is_and ? 0 : 1) ||
           !op(&plan->body, 0x0b))
            return 0;
        return 1;
    }
    int comparison = is_comparison(operation);
    /* Operands are computed in the container of the left operand for
     * comparisons and of the checked result for arithmetic. */
    const char *operand_type = comparison ?
        plan->function->exprs[expression->left].type : result_type;
    WasmValtype container = wasm_scalar_type_of(operand_type);
    if(!container) {
        fail(emitter, expression->span, "operand type");
        return 0;
    }
    int signed_operand = type_is_signed(operand_type);
    if(!emit_convert(emitter, plan, expression->left, operand_type))
        return 0;
    if(!emit_convert(emitter, plan, expression->right, operand_type))
        return 0;
    if(comparison) {
        int which = !strcmp(operation, "==") ? 0 : !strcmp(operation, "!=") ? 1 :
                    !strcmp(operation, "<") ? 2 : !strcmp(operation, "<=") ? 3 :
                    !strcmp(operation, ">") ? 4 : 5;
        const unsigned char *table;
        if(container == WASM_I32)
            table = signed_operand ? compare_i32_s : compare_i32_u;
        else if(container == WASM_I64)
            table = signed_operand ? compare_i64_s : compare_i64_u;
        else if(container == WASM_F32)
            table = compare_f32;
        else
            table = compare_f64;
        return op(&plan->body, table[which]);
    }
    if(!strcmp(operation, "+") || !strcmp(operation, "-") ||
       !strcmp(operation, "*") || !strcmp(operation, "/") ||
       !strcmp(operation, "%")) {
        int which = operation[0] == '+' ? 0 : operation[0] == '-' ? 1 :
                    operation[0] == '*' ? 2 : operation[0] == '/' ? 3 : 4;
        const unsigned char *table;
        if(container == WASM_I32)
            table = signed_operand ? arith_i32_s : arith_i32_u;
        else if(container == WASM_I64)
            table = signed_operand ? arith_i64_s : arith_i64_u;
        else if(container == WASM_F32)
            table = arith_f32;
        else
            table = arith_f64;
        if(which == 4 && container != WASM_I32 && container != WASM_I64) {
            fail(emitter, expression->span, "float remainder");
            return 0;
        }
        if(!op(&plan->body, table[which]))
            return 0;
        return op_normalize(&plan->body, result_type);
    }
    if(!strcmp(operation, "&") || !strcmp(operation, "|") ||
       !strcmp(operation, "^")) {
        unsigned char opcode = container == WASM_I64 ?
            (operation[0] == '&' ? 0x83 : operation[0] == '|' ? 0x84 : 0x85) :
            (operation[0] == '&' ? 0x71 : operation[0] == '|' ? 0x72 : 0x73);
        if(!op(&plan->body, opcode))
            return 0;
        return op_normalize(&plan->body, result_type);
    }
    if(!strcmp(operation, "<<") || !strcmp(operation, ">>")) {
        int left_shift = operation[0] == '<';
        unsigned char opcode;
        if(container == WASM_I64)
            opcode = left_shift ? 0x86 : (signed_operand ? 0x87 : 0x88);
        else
            opcode = left_shift ? 0x74 : (signed_operand ? 0x75 : 0x76);
        if(!op(&plan->body, opcode))
            return 0;
        return op_normalize(&plan->body, result_type);
    }
    fail(emitter, expression->span, "operator");
    return 0;
}

static int
emit_expression(WasmEmitter *emitter, WasmPlan *plan, int index)
{
    if(emitter->failed)
        return 0;
    const ZirFunction *function = plan->function;
    if(index < 0 || index >= function->expr_count)
        return 0;
    const ZirExpr *expression = &function->exprs[index];
    switch(expression->kind) {
    case ZIR_EXPR_INT: {
        char *end;
        errno = 0;
        if(wasm_width(expression->type) == 64) {
            uint64_t value;
            if(expression->text[0] == '-')
                value = (uint64_t)strtoll(expression->text, &end, 0);
            else
                value = strtoull(expression->text, &end, 0);
            if(errno || end == expression->text || *end)
                return 0;
            if(!op_const_i64(&plan->body, (int64_t)value))
                return 0;
        } else {
            long long value;
            if(expression->text[0] == '-')
                value = strtoll(expression->text, &end, 0);
            else
                value = (long long)strtoull(expression->text, &end, 0);
            if(errno || end == expression->text || *end)
                return 0;
            if(!op_const_i32(&plan->body, (int32_t)value))
                return 0;
        }
        return op_normalize(&plan->body, expression->type);
    }
    case ZIR_EXPR_FLOAT: {
        char *end;
        errno = 0;
        double number = ParseDecimal(expression->text, &end);
        if(errno || end == expression->text || *end || !isfinite(number))
            return 0;
        if(!strcmp(ScalarType(expression->type), "float32"))
            return op_const_f32(&plan->body, (float)number);
        return op_const_f64(&plan->body, number);
    }
    case ZIR_EXPR_IDENT: {
        if(expression->is_function_value || expression->is_global_value ||
           expression->is_move) {
            fail(emitter, expression->span, "function or global value");
            return 0;
        }
        if(!strcmp(expression->name, "true"))
            return op_const_i32(&plan->body, 1);
        if(!strcmp(expression->name, "false"))
            return op_const_i32(&plan->body, 0);
        char type[ZIR_NAME_MAX];
        int local = find_local(plan, expression->name, type);
        if(local < 0) {
            fail(emitter, expression->span, "module global state");
            return 0;
        }
        return op_local(&plan->body, 0x20, local);
    }
    case ZIR_EXPR_UNARY: {
        const char *operation = expression->op;
        if(!strcmp(operation, "+"))
            return emit_expression(emitter, plan, expression->right);
        if(!strcmp(operation, "-")) {
            if(wasm_scalar_type_of(expression->type) == WASM_F32 ||
               wasm_scalar_type_of(expression->type) == WASM_F64) {
                if(!emit_expression(emitter, plan, expression->right))
                    return 0;
                return op(&plan->body,
                          wasm_scalar_type_of(expression->type) == WASM_F32 ? 0x8c : 0x9a);
            }
            /* Integer negation is 0 - x, so the zero goes under first. */
            if(!op_zero(&plan->body, wasm_width(expression->type) == 64 ?
                        WASM_I64 : WASM_I32))
                return 0;
            if(!emit_expression(emitter, plan, expression->right))
                return 0;
            if(!op(&plan->body, wasm_width(expression->type) == 64 ? 0x7d : 0x6b))
                return 0;
            return op_normalize(&plan->body, expression->type);
        }
        if(!strcmp(operation, "!")) {
            if(!emit_expression(emitter, plan, expression->right))
                return 0;
            return op(&plan->body, 0x45); /* i32.eqz */
        }
        if(!strcmp(operation, "~")) {
            if(!emit_expression(emitter, plan, expression->right))
                return 0;
            if(wasm_width(expression->type) == 64) {
                if(!op_const_i64(&plan->body, -1) || !op(&plan->body, 0x85))
                    return 0;
            } else {
                if(!op_const_i32(&plan->body, -1) || !op(&plan->body, 0x73))
                    return 0;
            }
            return op_normalize(&plan->body, expression->type);
        }
        fail(emitter, expression->span, "unary operator");
        return 0;
    }
    case ZIR_EXPR_BINARY:
        return emit_binary(emitter, plan, index);
    case ZIR_EXPR_CAST:
        if(!emit_expression(emitter, plan, expression->right))
            return 0;
        if(emitter->failed)
            return 0;
        if(!op_convert(&plan->body, function->exprs[expression->right].type,
                       expression->name)) {
            fail(emitter, expression->span, "cast");
            return 0;
        }
        return 1;
    case ZIR_EXPR_CONDITIONAL: {
        if(!emit_expression(emitter, plan, expression->left))
            return 0;
        WasmValtype result = wasm_scalar_type_of(expression->type);
        if(!result) {
            fail(emitter, expression->span, "conditional result type");
            return 0;
        }
        if(!(op(&plan->body, 0x04) && byte(&plan->body, result)))
            return 0;
        if(!emit_convert(emitter, plan, expression->right, expression->type))
            return 0;
        if(!op(&plan->body, 0x05))
            return 0;
        if(!emit_convert(emitter, plan, expression->third, expression->type))
            return 0;
        return op(&plan->body, 0x0b);
    }
    case ZIR_EXPR_MEMBER: {
        /* A scalar field loads; a nested record's address is the value. */
        if(!emit_address(emitter, plan, index))
            return 0;
        if(emitter->failed)
            return 0;
        if(type_is_heap(plan->module, expression->type))
            return 1;
        return op_load_scalar(&plan->body, expression->type, 0);
    }
    case ZIR_EXPR_INDEX: {
        if(!emit_address(emitter, plan, index))
            return 0;
        if(emitter->failed)
            return 0;
        if(type_is_heap(plan->module, expression->type))
            return 1;
        return op_load_scalar(&plan->body, expression->type, 0);
    }
    case ZIR_EXPR_COMPOUND: {
        size_t size, alignment;
        if(!heap_layout(plan->module, expression->name, &size, &alignment)) {
            fail(emitter, expression->span, "record layout");
            return 0;
        }
        if(plan->scratch_depth >= ZX_WASM_SCRATCH_SLOTS) {
            fail(emitter, expression->span, "nested record literals");
            return 0;
        }
        int slot = plan->scratch_slots[plan->scratch_depth++];
        if(!op_arena_alloc(plan, size, alignment) ||
           !op_local(&plan->body, 0x21, slot))
            return 0;
        /* Each initializer writes its field into the fresh slot. */
        for(int child = expression->first_child; child >= 0;
            child = function->exprs[child].next_sibling) {
            const ZirExpr *initializer = &function->exprs[child];
            if(initializer->kind != ZIR_EXPR_FIELD_INIT) {
                fail(emitter, initializer->span, "record initializer");
                return 0;
            }
            size_t offset = 0;
            char field_type[ZIR_NAME_MAX];
            if(!field_place(plan->module, expression->name,
                            initializer->name, &offset, field_type)) {
                fail(emitter, initializer->span, "record field");
                return 0;
            }
            if(type_is_heap(plan->module, field_type)) {
                size_t field_size, field_alignment;
                if(!heap_layout(plan->module, field_type, &field_size,
                                &field_alignment)) {
                    fail(emitter, initializer->span, "record field layout");
                    return 0;
                }
                if(!op_local(&plan->body, 0x20, slot) ||
                   !op_const_i32(&plan->body, (int32_t)offset) ||
                   !op(&plan->body, 0x6a))
                    return 0;
                if(!emit_expression(emitter, plan, initializer->right))
                    return 0;
                if(emitter->failed)
                    return 0;
                if(!op_memory_copy(&plan->body, field_size))
                    return 0;
            } else {
                if(!op_local(&plan->body, 0x20, slot))
                    return 0;
                if(!emit_expression(emitter, plan, initializer->right))
                    return 0;
                if(emitter->failed)
                    return 0;
                if(strcmp(function->exprs[initializer->right].type,
                          field_type) != 0 &&
                   !op_convert(&plan->body,
                               function->exprs[initializer->right].type,
                               field_type)) {
                    fail(emitter, initializer->span, "record initializer conversion");
                    return 0;
                }
                if(!op_store_scalar(&plan->body, field_type, (int)offset))
                    return 0;
            }
        }
        plan->scratch_depth--;
        return op_local(&plan->body, 0x20, slot);
    }
    case ZIR_EXPR_CALL: {
        const ZirModule *owner = NULL;
        const ZirFunction *callee = NULL;
        if(ResolveFunction(plan->module, expression->name, &owner, &callee) != 1) {
            fail(emitter, expression->span, "call target");
            return 0;
        }
        if(callee->is_extern) {
            fail(emitter, expression->span, "host capability call");
            return 0;
        }
        WasmPlan *target = plan_of(emitter, owner, callee->name);
        if(target == NULL) {
            fail(emitter, expression->span, "call target plan");
            return 0;
        }
        const ZirParameters *parameters = ParametersOf(FunctionArgs(callee));
        int count = parameters->count;
        if(count > ZX_WASM_MAX_PARAMS) {
            fail(emitter, expression->span, "callee parameters");
            return 0;
        }
        int ordered[ZX_WASM_MAX_PARAMS];
        for(int p = 0; p < count; p++)
            ordered[p] = -1;
        int given = 0;
        for(int child = expression->first_child; child >= 0;
            child = function->exprs[child].next_sibling) {
            const ZirExpr *argument = &function->exprs[child];
            int position = argument->argument_index >= 0 ?
                argument->argument_index : given;
            if(position < 0 || position >= count) {
                fail(emitter, argument->span, "call argument position");
                return 0;
            }
            ordered[position] = child;
            given++;
        }
        if(given != count) {
            fail(emitter, expression->span, "call argument count");
            return 0;
        }
        for(int p = 0; p < count; p++) {
            if(ordered[p] < 0) {
                fail(emitter, expression->span, "missing call argument");
                return 0;
            }
            const char *argument_type =
                function->exprs[ordered[p]].type;
            if(type_is_heap(plan->module, parameters->items[p].value_type)) {
                if(strcmp(argument_type, parameters->items[p].value_type) != 0) {
                    fail(emitter, expression->span, "call argument conversion");
                    return 0;
                }
                if(!emit_expression(emitter, plan, ordered[p]))
                    return 0;
                if(emitter->failed)
                    return 0;
                continue;
            }
            if(!emit_convert(emitter, plan, ordered[p],
                             parameters->items[p].value_type))
                return 0;
        }
        return op_index(&plan->body, 0x10, target->function_index);
    }
    default:
        fail(emitter, expression->span, "expression");
        return 0;
    }
}

/* ----------------------------------------------------------- statements */

static void emit_sequence(WasmEmitter *emitter, WasmPlan *plan, int begin,
                          int end);

/* Emits one IF statement with its whole else chain as nested wasm ifs and
 * returns the index of the last statement the chain consumed. */
static int
emit_if_chain(WasmEmitter *emitter, WasmPlan *plan, int index, int end)
{
    const ZirStmt *statement = &plan->function->stmts[index];
    int close = block_end(plan->function, index, end);
    int last = close;
    if(!emit_expression(emitter, plan, statement->expr_root))
        return last;
    if(emitter->failed)
        return last;
    emitter->frames[emitter->frame_count].kind = BLOCK_KIND_IF;
    emitter->frames[emitter->frame_count].loop_id = 0;
    emitter->frame_count++;
    int region = region_enter(plan);
    if(region && op(&plan->body, 0x04) && byte(&plan->body, 0x40)) {
        emit_sequence(emitter, plan, index + 1, close);
        if(close + 1 < end &&
           plan->function->stmts[close + 1].kind == ZIR_STMT_IF &&
           plan->function->stmts[close + 1].is_else) {
            int next = close + 1;
            if(!op(&plan->body, 0x05))
                emitter->failed = 1;
            if(!emitter->failed) {
                if(plan->function->stmts[next].expr_root >= 0)
                    last = emit_if_chain(emitter, plan, next, end);
                else {
                    int else_close = block_end(plan->function, next, end);
                    emit_sequence(emitter, plan, next + 1, else_close);
                    last = else_close;
                }
            }
        }
        if(!op(&plan->body, 0x0b))
            emitter->failed = 1;
    } else {
        emitter->failed = 1;
    }
    if(!region_leave(plan))
        emitter->failed = 1;
    emitter->frame_count--;
    return last;
}

/* One step of the run budget: decrement the imported mutable global and
 * call the overrun import only when it is spent. Counted at every function
 * entry and loop iteration like ZIR_ENTER/ZIR_STEP in the native tier. */
static int
op_step_guard(WasmBytes *code)
{
    /* if (budget < 1) overrun(); budget -= 1; -- wasm has no global.tee,
     * so the comparison runs before the decrement instead of after. */
    return op_index(code, 0x23, 0) && /* global.get $budget */
           op_const_i64(code, 1) &&
           op(code, 0x53) && /* i64.lt_s */
           op(code, 0x04) && byte(code, 0x40) && /* if */
           op_index(code, 0x10, 0) && /* call $overrun */
           op(code, 0x0b) && /* end */
           op_index(code, 0x23, 0) && /* global.get $budget */
           op_const_i64(code, 1) &&
           op(code, 0x7d) && /* i64.sub */
           op_index(code, 0x24, 0); /* global.set $budget */
}

static int
emit_while(WasmEmitter *emitter, WasmPlan *plan, int index, int end)
{
    const ZirStmt *statement = &plan->function->stmts[index];
    int close = block_end(plan->function, index, end);
    if(statement->is_parallel || statement->is_gpu) {
        fail(emitter, statement->span, "#parallel region");
        return close;
    }
    if(emitter->frame_count + 2 >=
       (int)(sizeof(emitter->frames) / sizeof(*emitter->frames))) {
        emitter->failed = 1;
        return close;
    }
    /* block $exit { loop $next { guard; <cond>; br_if $exit; body;
     * restore; br $next } }. The loop's region is saved before the loop
     * starts and restored at every back edge and after the loop, so
     * per-iteration allocations do not accumulate. */
    int ok = region_enter(plan);
    emitter->frames[emitter->frame_count].kind = BLOCK_KIND_PLAIN;
    emitter->frames[emitter->frame_count].loop_id = 0;
    emitter->frame_count++;
    ok = ok && op(&plan->body, 0x02) && byte(&plan->body, 0x40);
    int loop_frame = emitter->frame_count;
    emitter->frames[loop_frame].kind = BLOCK_KIND_LOOP;
    emitter->frames[loop_frame].loop_id = statement->loop_id;
    emitter->frames[loop_frame].save_slot =
        plan->region_saves[plan->region_depth - 1];
    emitter->frame_count++;
    ok = ok && op(&plan->body, 0x03) && byte(&plan->body, 0x40);
    if(!ok) {
        emitter->failed = 1;
        emitter->frame_count -= 2;
        return close;
    }
    ok = op_step_guard(&plan->body);
    ok = ok && emit_expression(emitter, plan, statement->expr_root);
    ok = ok && !emitter->failed && op(&plan->body, 0x45); /* i32.eqz */
    /* br_if to the loop's exit block: depth counted from the loop frame. */
    ok = ok && op_index(&plan->body, 0x0d, 1);
    if(ok)
        emit_sequence(emitter, plan, index + 1, close);
    /* The back edge restores the loop's arena slot first. */
    ok = ok && !emitter->failed &&
         op_local(&plan->body, 0x20, emitter->frames[loop_frame].save_slot) &&
         op_index(&plan->body, 0x24, 1) && op_index(&plan->body, 0x0c, 0);
    ok = ok && op(&plan->body, 0x0b); /* end loop */
    emitter->frame_count--;
    ok = ok && op(&plan->body, 0x0b); /* end block */
    emitter->frame_count--;
    ok = ok && region_leave(plan);
    if(!ok)
        emitter->failed = 1;
    return close;
}

/* Branch depth to a loop's exit block (break) or header (continue). */
static int
branch_depth(WasmEmitter *emitter, int target_loop, int is_break)
{
    int loop_frame = -1;
    for(int i = emitter->frame_count - 1; i >= 0; i--) {
        if(emitter->frames[i].kind != BLOCK_KIND_LOOP)
            continue;
        if(target_loop == 0 || emitter->frames[i].loop_id == target_loop) {
            loop_frame = i;
            break;
        }
    }
    if(loop_frame < 0)
        return -1;
    int target = is_break ? loop_frame - 1 : loop_frame;
    return emitter->frame_count - 1 - target;
}

static void
emit_statement(WasmEmitter *emitter, WasmPlan *plan, int index, int end)
{
    const ZirStmt *statement = &plan->function->stmts[index];
    switch(statement->kind) {
    case ZIR_STMT_DECL: {
        int heap = type_is_heap(plan->module, statement->type);
        WasmValtype wasm = heap ? WASM_I32 : wasm_scalar_type_of(statement->type);
        if(!wasm) {
            fail(emitter, statement->span, "declaration type");
            return;
        }
        if(heap) {
            size_t size, alignment;
            if(!heap_layout(plan->module, statement->type, &size, &alignment)) {
                fail(emitter, statement->span, "declaration layout");
                return;
            }
            if(plan->scratch_depth >= ZX_WASM_SCRATCH_SLOTS) {
                fail(emitter, statement->span, "nested declarations");
                return;
            }
            int slot = plan->scratch_slots[plan->scratch_depth++];
            int local = -1;
            if(statement->expr_root >= 0 &&
               plan->function->exprs[statement->expr_root].kind ==
                   ZIR_EXPR_COMPOUND) {
                /* The literal's fresh slot is the variable's storage. */
                plan->scratch_depth--;
                if(!emit_expression(emitter, plan, statement->expr_root))
                    return;
                if(emitter->failed)
                    return;
                local = add_local(plan, statement->name, statement->type,
                                  WASM_I32);
                if(local < 0) {
                    DiagnosticOutOfMemory();
                    emitter->failed = 1;
                    return;
                }
                if(!op_local(&plan->body, 0x21, local))
                    emitter->failed = 1;
                return;
            }
            if(statement->expr_root >= 0) {
                if(!op_arena_alloc(plan, size, alignment) ||
                   !op_local(&plan->body, 0x21, slot))
                    return;
                if(!op_local(&plan->body, 0x20, slot))
                    return;
                if(!emit_expression(emitter, plan, statement->expr_root))
                    return;
                if(emitter->failed)
                    return;
                if(!op_memory_copy(&plan->body, size))
                    return;
            } else {
                if(!op_arena_alloc(plan, size, alignment) ||
                   !op_local(&plan->body, 0x21, slot) ||
                   !op_local(&plan->body, 0x20, slot) ||
                   !op_memory_zero(&plan->body, size) ||
                   !op_local(&plan->body, 0x20, slot))
                    return;
            }
            plan->scratch_depth--;
            local = add_local(plan, statement->name, statement->type, WASM_I32);
            if(local < 0) {
                DiagnosticOutOfMemory();
                emitter->failed = 1;
                return;
            }
            if(!op_local(&plan->body, 0x21, local))
                emitter->failed = 1;
            return;
        }
        /* The initializer is lowered before the name is bound so it can
         * read a shadowed outer binding, matching zi2py's order. */
        int initialized = statement->expr_root >= 0;
        if(initialized) {
            if(!emit_expression(emitter, plan, statement->expr_root))
                return;
            if(emitter->failed)
                return;
            if(strcmp(plan->function->exprs[statement->expr_root].type,
                      statement->type) != 0 &&
               !op_convert(&plan->body,
                           plan->function->exprs[statement->expr_root].type,
                           statement->type)) {
                fail(emitter, statement->span, "initializer conversion");
                return;
            }
        }
        int local = add_local(plan, statement->name, statement->type, wasm);
        if(local < 0) {
            DiagnosticOutOfMemory();
            emitter->failed = 1;
            return;
        }
        if(initialized ? !op_local(&plan->body, 0x21, local) :
           (!op_zero(&plan->body, wasm) || !op_local(&plan->body, 0x21, local))) {
            emitter->failed = 1;
            return;
        }
        return;
    }
    case ZIR_STMT_ASSIGN: {
        const ZirExpr *destination = statement->lhs_root >= 0 &&
            statement->lhs_root < plan->function->expr_count ?
            &plan->function->exprs[statement->lhs_root] : NULL;
        if(destination == NULL) {
            fail(emitter, statement->span, "assignment destination");
            return;
        }
        const char *operation = statement->assignment_op;
        int simple = operation[0] == '=' && operation[1] == 0;

        /* Emit one binary operator whose operands -- the place's current
         * value and the right side -- are already on the stack. */
        #define ZX_WASM_COMPOUND_OPERATOR(TARGET_TYPE) \
        do { \
            const char *binary = wasm_assignment_binary_operator(operation); \
            if(binary == NULL) { \
                fail(emitter, statement->span, "assignment operator"); \
                return; \
            } \
            WasmValtype container = wasm_scalar_type_of(TARGET_TYPE); \
            int signed_type = type_is_signed(TARGET_TYPE); \
            if(!strcmp(binary, "+") || !strcmp(binary, "-") || \
               !strcmp(binary, "*") || !strcmp(binary, "/") || \
               !strcmp(binary, "%")) { \
                int which = binary[0] == '+' ? 0 : binary[0] == '-' ? 1 : \
                            binary[0] == '*' ? 2 : binary[0] == '/' ? 3 : 4; \
                const unsigned char *table; \
                if(container == WASM_I32) \
                    table = signed_type ? arith_i32_s : arith_i32_u; \
                else if(container == WASM_I64) \
                    table = signed_type ? arith_i64_s : arith_i64_u; \
                else if(container == WASM_F32) \
                    table = arith_f32; \
                else \
                    table = arith_f64; \
                if(which == 4 && container != WASM_I32 && \
                   container != WASM_I64) { \
                    fail(emitter, statement->span, "float remainder"); \
                    return; \
                } \
                if(!op(&plan->body, table[which])) \
                    return; \
            } else if(!strcmp(binary, "&") || !strcmp(binary, "|") || \
                      !strcmp(binary, "^") || !strcmp(binary, "<<") || \
                      !strcmp(binary, ">>")) { \
                int left_shift = binary[0] == '<'; \
                unsigned char opcode; \
                if(!strcmp(binary, "&")) \
                    opcode = container == WASM_I64 ? 0x83 : 0x71; \
                else if(!strcmp(binary, "|")) \
                    opcode = container == WASM_I64 ? 0x84 : 0x72; \
                else if(!strcmp(binary, "^")) \
                    opcode = container == WASM_I64 ? 0x85 : 0x73; \
                else if(container == WASM_I64) \
                    opcode = left_shift ? 0x86 : (signed_type ? 0x87 : 0x88); \
                else \
                    opcode = left_shift ? 0x74 : (signed_type ? 0x75 : 0x76); \
                if(!op(&plan->body, opcode)) \
                    return; \
            } else { \
                fail(emitter, statement->span, "assignment operator"); \
                return; \
            } \
            if(!op_normalize(&plan->body, TARGET_TYPE)) \
                return; \
        } while(0)

        if(destination->kind == ZIR_EXPR_IDENT) {
            char type[ZIR_NAME_MAX];
            int local = find_local(plan, destination->name, type);
            if(local < 0) {
                fail(emitter, statement->span, "global assignment");
                return;
            }
            if(type_is_heap(plan->module, type)) {
                if(!simple) {
                    fail(emitter, statement->span, "assignment operator");
                    return;
                }
                size_t size, alignment;
                if(!heap_layout(plan->module, type, &size, &alignment)) {
                    fail(emitter, statement->span, "assignment layout");
                    return;
                }
                if(!op_local(&plan->body, 0x20, local))
                    return;
                if(!emit_expression(emitter, plan, statement->expr_root))
                    return;
                if(emitter->failed)
                    return;
                if(!op_memory_copy(&plan->body, size))
                    emitter->failed = 1;
                return;
            }
            if(!simple && !op_local(&plan->body, 0x20, local))
                return;
            if(!emit_expression(emitter, plan, statement->expr_root))
                return;
            if(emitter->failed)
                return;
            const char *value_type =
                plan->function->exprs[statement->expr_root].type;
            if(strcmp(value_type, type) != 0 &&
               !op_convert(&plan->body, value_type, type)) {
                fail(emitter, statement->span, "assignment conversion");
                return;
            }
            if(!simple)
                ZX_WASM_COMPOUND_OPERATOR(type);
            if(!op_local(&plan->body, 0x21, local))
                emitter->failed = 1;
            return;
        }
        if(destination->kind == ZIR_EXPR_MEMBER ||
           destination->kind == ZIR_EXPR_INDEX) {
            const char *place_type = destination->type;
            int heap_place = type_is_heap(plan->module, place_type);
            if(!simple && heap_place) {
                fail(emitter, statement->span, "assignment operator");
                return;
            }
            if(!emit_address(emitter, plan, statement->lhs_root))
                return;
            if(emitter->failed)
                return;
            if(heap_place) {
                size_t size, alignment;
                if(!heap_layout(plan->module, place_type, &size, &alignment)) {
                    fail(emitter, statement->span, "assignment layout");
                    return;
                }
                if(!emit_expression(emitter, plan, statement->expr_root))
                    return;
                if(emitter->failed)
                    return;
                if(!op_memory_copy(&plan->body, size))
                    emitter->failed = 1;
                return;
            }
            if(!simple && !op_load_scalar(&plan->body, place_type, 0))
                return;
            if(!emit_expression(emitter, plan, statement->expr_root))
                return;
            if(emitter->failed)
                return;
            const char *value_type =
                plan->function->exprs[statement->expr_root].type;
            if(strcmp(value_type, place_type) != 0 &&
               !op_convert(&plan->body, value_type, place_type)) {
                fail(emitter, statement->span, "assignment conversion");
                return;
            }
            if(!simple)
                ZX_WASM_COMPOUND_OPERATOR(place_type);
            if(!op_store_scalar(&plan->body, place_type, 0))
                emitter->failed = 1;
            return;
        }
        #undef ZX_WASM_COMPOUND_OPERATOR
        fail(emitter, statement->span, "assignment destination");
        return;
    }

    case ZIR_STMT_EXPR:
        if(!emit_expression(emitter, plan, statement->expr_root))
            return;
        if(emitter->failed)
            return;
        if(!op(&plan->body, 0x1a)) /* drop */
            emitter->failed = 1;
        return;
    case ZIR_STMT_RETURN:
        if(statement->expr_root >= 0) {
            if(!emit_expression(emitter, plan, statement->expr_root))
                return;
            if(emitter->failed)
                return;
            const char *actual =
                plan->function->exprs[statement->expr_root].type;
            if(type_is_heap(plan->module, plan->result_type)) {
                if(strcmp(actual, plan->result_type) != 0) {
                    fail(emitter, statement->span, "return conversion");
                    return;
                }
            } else if(strcmp(actual, plan->result_type) != 0) {
                if(strcmp(plan->result_type, "void") == 0) {
                    if(!op(&plan->body, 0x1a)) /* drop */
                        emitter->failed = 1;
                    if(!emitter->failed && !op(&plan->body, 0x0f))
                        emitter->failed = 1;
                    return;
                }
                if(!op_convert(&plan->body, actual, plan->result_type)) {
                    fail(emitter, statement->span, "return conversion");
                    return;
                }
            }
        } else if(strcmp(plan->result_type, "void") != 0) {
            if(!op_zero(&plan->body, wasm_scalar_type_of(plan->result_type))) {
                emitter->failed = 1;
                return;
            }
        }
        if(!op(&plan->body, 0x0f)) /* return */
            emitter->failed = 1;
        return;
    case ZIR_STMT_BREAK: {
        int depth = branch_depth(emitter, statement->target_id, 1);
        if(depth < 0 || !op_index(&plan->body, 0x0c, depth))
            emitter->failed = 1;
        return;
    }
    case ZIR_STMT_CONTINUE: {
        int depth = branch_depth(emitter, statement->target_id, 0);
        int loop_frame = -1;
        for(int i = emitter->frame_count - 1; i >= 0; i--)
            if(emitter->frames[i].kind == BLOCK_KIND_LOOP &&
               (statement->target_id == 0 ||
                emitter->frames[i].loop_id == statement->target_id)) {
                loop_frame = i;
                break;
            }
        if(depth < 0 || loop_frame < 0 ||
           !op_local(&plan->body, 0x20, emitter->frames[loop_frame].save_slot) ||
           !op_index(&plan->body, 0x24, 1) ||
           !op_index(&plan->body, 0x0c, depth))
            emitter->failed = 1;
        return;
    }
    case ZIR_STMT_BLOCK_OPEN: {
        if(StatementHasAssembly(statement)) {
            fail(emitter, statement->span, "#asm block");
            return;
        }
        emitter->frames[emitter->frame_count].kind = BLOCK_KIND_PLAIN;
        emitter->frames[emitter->frame_count].loop_id = 0;
        emitter->frame_count++;
        int region = region_enter(plan);
        if(region && op(&plan->body, 0x02) && byte(&plan->body, 0x40)) {
            int close = block_end(plan->function, index, end);
            emit_sequence(emitter, plan, index + 1, close);
            if(!op(&plan->body, 0x0b))
                emitter->failed = 1;
        } else {
            emitter->failed = 1;
        }
        if(!region_leave(plan))
            emitter->failed = 1;
        emitter->frame_count--;
        return;
    }
    case ZIR_STMT_BLOCK_CLOSE:
        return;
    case ZIR_STMT_UNUSED:
        return;
    case ZIR_STMT_UNREACHABLE:
        if(!op(&plan->body, 0x00))
            emitter->failed = 1;
        return;
    default:
        fail(emitter, statement->span, "statement");
        return;
    }
}

static void
emit_sequence(WasmEmitter *emitter, WasmPlan *plan, int begin, int end)
{
    int index = begin;
    while(index < end && !emitter->failed) {
        const ZirStmt *statement = &plan->function->stmts[index];
        switch(statement->kind) {
        case ZIR_STMT_IF: {
            int last = emit_if_chain(emitter, plan, index, end);
            index = last > index ? last : index;
            index++;
            break;
        }
        case ZIR_STMT_WHILE: {
            int close = emit_while(emitter, plan, index, end);
            index = close > index ? close : index;
            index++;
            break;
        }
        case ZIR_STMT_BLOCK_OPEN: {
            int close = block_end(plan->function, index, end);
            emit_statement(emitter, plan, index, end);
            index = close > index ? close : index;
            index++;
            break;
        }
        default:
            emit_statement(emitter, plan, index, end);
            index++;
            break;
        }
    }
}

/* -------------------------------------------------------------- module */

typedef struct WasmSignature {
    WasmValtype parameters[ZX_WASM_MAX_PARAMS];
    int parameter_count;
    WasmValtype result;
} WasmSignature;

static int
same_signature(const WasmSignature *left, const WasmSignature *right)
{
    if(left->parameter_count != right->parameter_count ||
       left->result != right->result)
        return 0;
    for(int i = 0; i < left->parameter_count; i++)
        if(left->parameters[i] != right->parameters[i])
            return 0;
    return 1;
}

static int
signature_index(WasmSignature ***table, int *count, int *capacity,
                const WasmSignature *signature)
{
    for(int i = 0; i < *count; i++)
        if(same_signature((*table)[i], signature))
            return i;
    if(*count == *capacity) {
        int room = *capacity ? *capacity * 2 : 16;
        WasmSignature **grown = realloc(*table,
                                        (size_t)room * sizeof(*grown));
        if(grown == NULL)
            return -1;
        *table = grown;
        *capacity = room;
    }
    WasmSignature *copy = malloc(sizeof(*copy));
    if(copy == NULL)
        return -1;
    *copy = *signature;
    (*table)[*count] = copy;
    return (*count)++;
}

static int
supported_statement(const ZirStmt *statement)
{
    switch(statement->kind) {
    case ZIR_STMT_BLOCK_OPEN:
        return !StatementHasAssembly(statement);
    case ZIR_STMT_BLOCK_CLOSE:
    case ZIR_STMT_DECL:
    case ZIR_STMT_ASSIGN:
    case ZIR_STMT_EXPR:
    case ZIR_STMT_IF:
    case ZIR_STMT_WHILE:
    case ZIR_STMT_RETURN:
    case ZIR_STMT_BREAK:
    case ZIR_STMT_CONTINUE:
    case ZIR_STMT_UNUSED:
    case ZIR_STMT_UNREACHABLE:
        return 1;
    default:
        return 0;
    }
}

static int
supported_expression(const ZirExpr *expression)
{
    switch(expression->kind) {
    case ZIR_EXPR_INT:
    case ZIR_EXPR_FLOAT:
    case ZIR_EXPR_IDENT:
    case ZIR_EXPR_CALL:
    case ZIR_EXPR_BINARY:
    case ZIR_EXPR_UNARY:
    case ZIR_EXPR_CAST:
    case ZIR_EXPR_CONDITIONAL:
    case ZIR_EXPR_MEMBER:
    case ZIR_EXPR_INDEX:
    case ZIR_EXPR_COMPOUND:
    case ZIR_EXPR_FIELD_INIT:
        return 1;
    default:
        return 0;
    }
}

/* A whole-function gate before emission: fail once, up front, with the
 * first offending source location. */
static int
supported_function(WasmEmitter *emitter, const WasmPlan *plan)
{
    const ZirFunction *function = plan->function;
    for(int s = 0; s < function->stmt_count; s++) {
        const ZirStmt *statement = &function->stmts[s];
        if(statement->is_parallel || statement->is_gpu) {
            fail(emitter, statement->span, "#parallel region");
            return 0;
        }
        if(!supported_statement(statement)) {
            fail(emitter, statement->span, "statement");
            return 0;
        }
    }
    for(int e = 0; e < function->expr_count; e++) {
        const ZirExpr *expression = &function->exprs[e];
        if(!supported_expression(expression)) {
            fail(emitter, expression->span, "expression");
            return 0;
        }
        if(!wasm_type_of_in(plan->module, expression->type) &&
           !type_is_heap(plan->module, expression->type)) {
            Diagnostic(expression->span, "zx.wasm",
                       "type %s is outside the wasm execution subset",
                       expression->type);
            emitter->failed = 1;
            return 0;
        }
    }
    return 1;
}

static void
plan_free(WasmPlan *plan)
{
    free(plan->locals);
    bytes_free(&plan->body);
}

unsigned char *
zir_wasm_lower(const ZirProgram *program, const char *entry_module,
               const char *entry_function, size_t *size)
{
    unsigned char *image = NULL;
    WasmSignature *signatures_store[16];
    WasmSignature **signatures = signatures_store;
    int signature_count = 0, signature_capacity = 16;
    WasmEmitter emitter = {0};
    emitter.program = program;
    int entry_plan = -1;
    int wrapper_type = -1;
    if(program == NULL || entry_module == NULL || entry_function == NULL)
        return NULL;

    /* Type 0 is the step guard and later the zx_entry wrapper: () -> (). */
    WasmSignature guard = {0};
    guard.result = WASM_VOID;
    if(signature_index(&signatures, &signature_count, &signature_capacity,
                       &guard) != 0) {
        DiagnosticOutOfMemory();
        return NULL;
    }
    /* The zx_entry wrapper signature: () -> i64. */
    WasmSignature wrapper = {0};
    wrapper.result = WASM_I64;
    wrapper_type = signature_index(&signatures, &signature_count,
                                   &signature_capacity, &wrapper);
    if(wrapper_type < 0) {
        DiagnosticOutOfMemory();
        return NULL;
    }

    /* One plan per linked function with a body. */
    int capacity = 16;
    emitter.plans = calloc((size_t)capacity, sizeof(*emitter.plans));
    if(emitter.plans == NULL) {
        DiagnosticOutOfMemory();
        goto done;
    }
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        for(int f = 0; f < module->function_count; f++) {
            const ZirFunction *function = &module->functions[f];
            if(function->is_extern) {
                if(function->extern_kind == ZIR_EXTERN_HOST) {
                    Diagnostic(function->span, "zx.wasm",
                               "host capability %s:%s is outside the wasm execution subset",
                               module->name, function->name);
                    goto done;
                }
                continue;
            }
            if(function->is_template || function->is_specialization)
                continue;
            if(emitter.plan_count == capacity) {
                capacity *= 2;
                WasmPlan *grown = realloc(emitter.plans,
                                          (size_t)capacity * sizeof(*grown));
                if(grown == NULL) {
                    DiagnosticOutOfMemory();
                    goto done;
                }
                emitter.plans = grown;
            }
            WasmPlan *plan = &emitter.plans[emitter.plan_count];
            memset(plan, 0, sizeof(*plan));
            plan->module = module;
            plan->function = function;
            snprintf(plan->result_type, sizeof(plan->result_type), "%s",
                     function->return_type[0] ? function->return_type : "void");
            if(strcmp(plan->result_type, "void") &&
               !wasm_type_of_in(module, plan->result_type)) {
                Diagnostic(function->span, "zx.wasm",
                           "return type %s of %s:%s is outside the wasm execution subset",
                           plan->result_type, module->name, function->name);
                goto done;
            }
            const ZirParameters *parameters = ParametersOf(FunctionArgs(function));
            int count = parameters->count;
            if(count > ZX_WASM_MAX_PARAMS) {
                Diagnostic(function->span, "zx.wasm",
                           "parameters of %s:%s are outside the wasm execution subset",
                           module->name, function->name);
                goto done;
            }
            WasmSignature signature = {0};
            signature.parameter_count = count;
            for(int p = 0; p < count; p++) {
                WasmValtype wasm = wasm_type_of_in(module,
                                                   parameters->items[p].value_type);
                if(!wasm) {
                    Diagnostic(function->span, "zx.wasm",
                               "parameter type %s of %s:%s is outside the wasm execution subset",
                               parameters->items[p].value_type, module->name,
                               function->name);
                    goto done;
                }
                signature.parameters[p] = wasm;
                if(add_local(plan, parameters->items[p].binding_name,
                             parameters->items[p].value_type, wasm) < 0) {
                    DiagnosticOutOfMemory();
                    goto done;
                }
            }
            signature.result = !strcmp(plan->result_type, "void") ?
                WASM_VOID : wasm_type_of_in(module, plan->result_type);
            plan->param_count = count;
            plan->type_index = signature_index(&signatures, &signature_count,
                                               &signature_capacity, &signature);
            if(plan->type_index < 0) {
                DiagnosticOutOfMemory();
                goto done;
            }
            if(!strcmp(module->name, entry_module) &&
               !strcmp(function->name, entry_function)) {
                if(count != 0) {
                    Diagnostic(function->span, "zx.wasm",
                               "entry %s:%s must take no arguments",
                               module->name, function->name);
                    goto done;
                }
                if(!type_is_integer(plan->result_type) &&
                   strcmp(ScalarType(plan->result_type), "bool") != 0 &&
                   strcmp(ScalarType(plan->result_type), "void") != 0) {
                    Diagnostic(function->span, "zx.wasm",
                               "entry %s:%s must return an integer or bool",
                               module->name, function->name);
                    goto done;
                }
                entry_plan = emitter.plan_count;
            }
            emitter.plan_count++;
        }
    }
    if(entry_plan < 0) {
        Diagnostic((ZirSourceSpan){0}, "zx.wasm", "cannot find entry %s:%s",
                   entry_module, entry_function);
        goto done;
    }
    /* Function index 0 is the step guard import; bodies follow, and the
     * zx_entry wrapper is the last declared function. */
    for(int i = 0; i < emitter.plan_count; i++)
        emitter.plans[i].function_index = 1 + i;

    for(int i = 0; i < emitter.plan_count && !emitter.failed; i++) {
        WasmPlan *plan = &emitter.plans[i];
        if(!supported_function(&emitter, plan))
            break;
        /* Hidden locals: the arena scratch, one save per region depth, and
         * the emission scratch stack. */
        plan->arena_scratch = add_local(plan, "$arena", "s32", WASM_I32);
        for(int r = 0; r < ZX_WASM_MAX_REGIONS; r++)
            plan->region_saves[r] = add_local(plan, "$region", "s32", WASM_I32);
        for(int r = 0; r < ZX_WASM_SCRATCH_SLOTS; r++)
            plan->scratch_slots[r] = add_local(plan, "$scratch", "s32", WASM_I32);
        if(plan->arena_scratch < 0 || plan->region_saves[0] < 0 ||
           plan->scratch_slots[0] < 0) {
            DiagnosticOutOfMemory();
            emitter.failed = 1;
            break;
        }
        /* Guard at function entry, like ZIR_ENTER in the native tier. */
        if(!op_step_guard(&plan->body)) {
            emitter.failed = 1;
            break;
        }
        /* Record and array parameters are values: copy each into a fresh
         * arena slot so the callee owns its own storage. */
        for(int p = 0; p < plan->param_count && !emitter.failed; p++) {
            if(!type_is_heap(plan->module, plan->locals[p].type))
                continue;
            size_t size, alignment;
            if(!heap_layout(plan->module, plan->locals[p].type, &size,
                            &alignment)) {
                emitter.failed = 1;
                break;
            }
            if(!op_arena_alloc(plan, size, alignment) ||
               !op_local(&plan->body, 0x21, plan->arena_scratch) ||
               !op_local(&plan->body, 0x20, plan->arena_scratch) ||
               !op_local(&plan->body, 0x20, p) ||
               !op_memory_copy(&plan->body, size) ||
               !op_local(&plan->body, 0x20, plan->arena_scratch) ||
               !op_local(&plan->body, 0x21, p)) {
                emitter.failed = 1;
                break;
            }
        }
        if(emitter.failed)
            break;
        emit_sequence(&emitter, plan, 0, plan->function->stmt_count);
        if(!emitter.failed) {
            /* Whatever the body did, leave a value (or nothing) behind;
             * code after a return is unreachable and validates anyway. */
            if(strcmp(plan->result_type, "void") != 0) {
                WasmValtype tail = type_is_heap(plan->module, plan->result_type) ?
                    WASM_I32 : wasm_scalar_type_of(plan->result_type);
                if(!tail)
                    tail = WASM_I32;
                if(!op_zero(&plan->body, tail) ||
                   !op(&plan->body, 0x0f)) {
                    emitter.failed = 1;
                    break;
                }
            }
            if(!op(&plan->body, 0x0b))
                emitter.failed = 1;
        }
    }
    if(emitter.failed)
        goto done;

    /* Assemble the module image. */
    WasmBytes module = {0};
    WasmBytes section = {0};
    WasmBytes code_entries = {0};
    int assemble_ok = 1;
    assemble_ok = assemble_ok && raw(&module, "\0asm", 4) &&
                  raw(&module, "\1\0\0\0", 4);

    /* Type section. */
    assemble_ok = assemble_ok && uleb(&section, (uint64_t)signature_count);
    for(int i = 0; assemble_ok && i < signature_count; i++) {
        const WasmSignature *signature = signatures[i];
        assemble_ok = assemble_ok && byte(&section, 0x60) &&
                      uleb(&section, (uint64_t)signature->parameter_count);
        for(int p = 0; assemble_ok && p < signature->parameter_count; p++)
            assemble_ok = byte(&section, (unsigned char)signature->parameters[p]);
        if(!assemble_ok)
            break;
        if(signature->result == WASM_VOID)
            assemble_ok = uleb(&section, 0);
        else
            assemble_ok = uleb(&section, 1) &&
                          byte(&section, (unsigned char)signature->result);
    }
    assemble_ok = assemble_ok && byte(&module, 1) &&
                  uleb(&module, (uint64_t)section.count) &&
                  raw(&module, section.data, section.count);

    /* Import section: (zx, overrun) -> () is function index 0, and the
     * mutable i64 budget global is global index 0. */
    section.count = 0;
    assemble_ok = assemble_ok && uleb(&section, 2) &&
                  uleb(&section, 2) && raw(&section, "zx", 2) &&
                  uleb(&section, 7) && raw(&section, "overrun", 7) &&
                  byte(&section, 0x00) && uleb(&section, 0) &&
                  uleb(&section, 2) && raw(&section, "zx", 2) &&
                  uleb(&section, 6) && raw(&section, "budget", 6) &&
                  byte(&section, 0x03) && byte(&section, WASM_I64) &&
                  byte(&section, 0x01);
    assemble_ok = assemble_ok && byte(&module, 2) &&
                  uleb(&module, (uint64_t)section.count) &&
                  raw(&module, section.data, section.count);

    /* Function section: every plan plus the zx_entry wrapper. */
    section.count = 0;
    assemble_ok = assemble_ok &&
                  uleb(&section, (uint64_t)emitter.plan_count + 1);
    for(int i = 0; assemble_ok && i < emitter.plan_count; i++)
        assemble_ok = uleb(&section, (uint64_t)emitter.plans[i].type_index);
    assemble_ok = assemble_ok && uleb(&section, (uint64_t)wrapper_type);
    assemble_ok = assemble_ok && byte(&module, 3) &&
                  uleb(&module, (uint64_t)section.count) &&
                  raw(&module, section.data, section.count);

    /* Memory section: one memory sized for the arena's region discipline;
     * lexical reuse keeps live allocations small, not iteration counts. */
    section.count = 0;
    assemble_ok = assemble_ok && uleb(&section, 1) && byte(&section, 0x00) &&
                  uleb(&section, ZX_WASM_ARENA_PAGES);
    assemble_ok = assemble_ok && byte(&module, 5) &&
                  uleb(&module, (uint64_t)section.count) &&
                  raw(&module, section.data, section.count);

    /* Global section: the arena pointer, mutable, starting past nothing
     * else in linear memory yet. */
    section.count = 0;
    assemble_ok = assemble_ok && uleb(&section, 1) &&
                  byte(&section, WASM_I32) && byte(&section, 0x01) &&
                  op_const_i32(&section, ZX_WASM_ARENA_BASE) &&
                  op(&section, 0x0b);
    assemble_ok = assemble_ok && byte(&module, 6) &&
                  uleb(&module, (uint64_t)section.count) &&
                  raw(&module, section.data, section.count);

    /* Export section: zx_entry and memory. */
    section.count = 0;
    assemble_ok = assemble_ok && uleb(&section, 2) && uleb(&section, 8) &&
                  raw(&section, "zx_entry", 8) && byte(&section, 0x00) &&
                  uleb(&section, (uint64_t)(1 + emitter.plan_count)) &&
                  uleb(&section, 6) && raw(&section, "memory", 6) &&
                  byte(&section, 0x02) && uleb(&section, 0);
    assemble_ok = assemble_ok && byte(&module, 7) &&
                  uleb(&module, (uint64_t)section.count) &&
                  raw(&module, section.data, section.count);

    /* Code section: locals vector and body per function, wrapper last. */
    for(int i = 0; assemble_ok && i < emitter.plan_count; i++) {
        WasmPlan *plan = &emitter.plans[i];
        WasmBytes entry = {0};
        /* Locals vector: count of (n, type) runs over the extra locals. */
        int runs = 0;
        for(int position = plan->param_count; position < plan->local_count; ) {
            WasmValtype wasm = plan->locals[position].wasm;
            while(position < plan->local_count &&
                  plan->locals[position].wasm == wasm)
                position++;
            runs++;
        }
        assemble_ok = uleb(&entry, (uint64_t)runs);
        for(int position = plan->param_count; assemble_ok &&
            position < plan->local_count; ) {
            WasmValtype wasm = plan->locals[position].wasm;
            int length = 0;
            while(position < plan->local_count &&
                  plan->locals[position].wasm == wasm) {
                position++;
                length++;
            }
            assemble_ok = uleb(&entry, (uint64_t)length) &&
                          byte(&entry, (unsigned char)wasm);
        }
        assemble_ok = assemble_ok && raw(&entry, plan->body.data,
                                         plan->body.count);
        assemble_ok = assemble_ok &&
                      uleb(&code_entries, (uint64_t)entry.count) &&
                      raw(&code_entries, entry.data, entry.count);
        bytes_free(&entry);
    }
    if(assemble_ok) {
        /* zx_entry: call the entry, widen to i64, or zero for void. */
        WasmPlan *entry = &emitter.plans[entry_plan];
        WasmBytes wrapper_body = {0};
        assemble_ok = uleb(&wrapper_body, 0); /* no locals */
        assemble_ok = assemble_ok &&
                      op_const_i32(&wrapper_body, ZX_WASM_ARENA_BASE) &&
                      op_index(&wrapper_body, 0x24, 1); /* global.set $arena */
        if(!strcmp(entry->result_type, "void"))
            assemble_ok = assemble_ok &&
                          op_index(&wrapper_body, 0x10, entry->function_index) &&
                          op_const_i64(&wrapper_body, 0);
        else if(wasm_width(entry->result_type) == 64)
            assemble_ok = assemble_ok &&
                          op_index(&wrapper_body, 0x10, entry->function_index);
        else
            assemble_ok = assemble_ok &&
                          op_index(&wrapper_body, 0x10, entry->function_index) &&
                          op(&wrapper_body, type_is_signed(entry->result_type) ?
                             0xac : 0xad);
        assemble_ok = assemble_ok && op(&wrapper_body, 0x0b) &&
                      uleb(&code_entries, (uint64_t)wrapper_body.count) &&
                      raw(&code_entries, wrapper_body.data, wrapper_body.count);
        bytes_free(&wrapper_body);
    }
    /* Code section: entry count, then locals vector and body per function,
     * with the zx_entry wrapper last. */
    {
        WasmBytes code_section = {0};
        assemble_ok = assemble_ok &&
                      uleb(&code_section, (uint64_t)emitter.plan_count + 1) &&
                      raw(&code_section, code_entries.data, code_entries.count);
        assemble_ok = assemble_ok && byte(&module, 10) &&
                      uleb(&module, (uint64_t)code_section.count) &&
                      raw(&module, code_section.data, code_section.count);
        bytes_free(&code_section);
    }
    if(assemble_ok) {
        image = module.data;
        if(size != NULL)
            *size = module.count;
        module.data = NULL;
    } else {
        Diagnostic((ZirSourceSpan){0}, "zx.wasm", "cannot assemble module");
    }
    bytes_free(&code_entries);
    bytes_free(&section);
    bytes_free(&module);
done:
    for(int i = 0; i < emitter.plan_count; i++)
        plan_free(&emitter.plans[i]);
    free(emitter.plans);
    if(signatures != signatures_store) {
        for(int i = 0; i < signature_count; i++)
            free(signatures[i]);
        free(signatures);
    } else {
        for(int i = 0; i < signature_count; i++)
            free(signatures_store[i]);
    }
    return image;
}
