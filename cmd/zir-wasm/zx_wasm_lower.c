/*
 * zx_wasm_lower.c - verified revision-28 executable ZX to a wasm32 module.
 *
 * The input is the decoded LIR payload that the interpreter runs and the
 * native backend lowers. Layout follows the interpreter: packed records,
 * arrays of packed elements, 8-byte pointers in memory, canonical
 * zero-extended integers in locals. Scalar values are wasm locals; record
 * and array values live in a per-call frame on a linear-memory stack.
 * Blocks become nested wasm blocks; a function with a backward edge or a
 * non-zero entry adds one dispatch loop with a br_table. Every iteration is
 * in program index order, so equal inputs give byte-identical modules.
 */
#include "zx_wasm_lower.h"
#include "zir_diagnostic.h"
#include "zir_zx_wasm.h"
#include "compiler_lowered_ir.h"
#include "compiler_lowered_verify.h"
#include "compiler_exec_link.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    W_I32 = 0x7f, W_I64 = 0x7e, W_F32 = 0x7d, W_F64 = 0x7c,
    ZXW_DATA_BASE = 1024,
    ZXW_STACK_SIZE = 1 << 20,
    ZXW_IMAGE_LIMIT = 1 << 28
};

/* Global indices: the imported budget, then the state arena cursor, the
 * stack pointer, the initializer flag and the stack limit. */
enum { G_BUDGET, G_ARENA, G_SP, G_INIT, G_LIMIT };

enum {
    HOST_NONE, HOST_WRITE, HOST_SIGNED, HOST_UNSIGNED, HOST_BOOL,
    HOST_FLOAT32, HOST_FLOAT64, HOST_CAPABILITY
};

typedef struct Buf {
    unsigned char *data;
    size_t count;
    size_t capacity;
    int failed;
} Buf;

static void
buf_reserve(Buf *buf, size_t extra)
{
    if(buf->failed)
        return;
    if(extra > ZXW_IMAGE_LIMIT || buf->count > ZXW_IMAGE_LIMIT - extra) {
        buf->failed = 1;
        return;
    }
    if(buf->count + extra <= buf->capacity)
        return;
    size_t capacity = buf->capacity ? buf->capacity : 256;
    while(capacity < buf->count + extra)
        capacity *= 2;
    unsigned char *data = realloc(buf->data, capacity);
    if(data == NULL) {
        buf->failed = 1;
        return;
    }
    buf->data = data;
    buf->capacity = capacity;
}

static void
buf_byte(Buf *buf, unsigned value)
{
    buf_reserve(buf, 1);
    if(!buf->failed)
        buf->data[buf->count++] = (unsigned char)value;
}

static void
buf_raw(Buf *buf, const void *data, size_t size)
{
    if(size == 0)
        return;
    buf_reserve(buf, size);
    if(!buf->failed) {
        memcpy(buf->data + buf->count, data, size);
        buf->count += size;
    }
}

static void
buf_uleb(Buf *buf, uint64_t value)
{
    do {
        unsigned byte = value & 0x7f;
        value >>= 7;
        buf_byte(buf, value ? byte | 0x80 : byte);
    } while(value);
}

static void
buf_sleb(Buf *buf, int64_t value)
{
    for(;;) {
        unsigned byte = (unsigned)(value & 0x7f);
        value >>= 7;
        if((value == 0 && !(byte & 0x40)) || (value == -1 && (byte & 0x40))) {
            buf_byte(buf, byte);
            return;
        }
        buf_byte(buf, byte | 0x80);
    }
}

static void
buf_name(Buf *buf, const char *name, size_t size)
{
    buf_uleb(buf, size);
    buf_raw(buf, name, size);
}

static void
buf_free(Buf *buf)
{
    free(buf->data);
    memset(buf, 0, sizeof(*buf));
}

/* One signature: parameter valtypes followed by an optional result. */
typedef struct Sig {
    unsigned char types[1 + 512];
    size_t params;
    unsigned result;
} Sig;

typedef struct Lower {
    const LirProgram *program;
    const LirType *types;
    int64_t type_count;
    const LirFunction *functions;
    int64_t function_count;
    const LirConstant *constants;
    int64_t constant_count;
    const LirGlobal *globals;
    int64_t global_count;
    const String *modules;
    int64_t module_count;
    int64_t *sizes;
    uint32_t *global_address;
    unsigned char *host_kind;
    uint32_t *host_index;
    uint32_t *wasm_function;
    Buf data;
    Sig *sigs;
    size_t sig_count;
    size_t sig_capacity;
    ZxWasmCapability *capabilities;
    size_t capability_count;
    int uses_host;
    int uses_bytes_equal;
    int uses_table;
    uint32_t imports;
    uint32_t bytes_equal_function;
    uint32_t stub_function;
    uint32_t entry_function;
    int failed;
} Lower;

static int
WasmGap(Lower *lower, const char *code, const char *format, ...)
{
    if(lower->failed)
        return 0;
    char text[1024];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    Diagnostic((ZirSourceSpan){0}, code,
               "%s: %s; the program is outside the wasm execution subset and no alternate engine was selected",
               code, text);
    lower->failed = 1;
    return 0;
}

static int
type_valid(Lower *lower, int32_t type)
{
    return type >= 0 && type < lower->type_count;
}

static const LirType *
type_at(Lower *lower, int32_t type)
{
    static const LirType invalid = {0};
    return type_valid(lower, type) ? &lower->types[type] : &invalid;
}

static int64_t
type_size_at(Lower *lower, int32_t type, int depth)
{
    if(!type_valid(lower, type) || depth > 256)
        return -1;
    if(lower->sizes[type] >= 0)
        return lower->sizes[type];
    const LirType *value = &lower->types[type];
    int64_t size = -1;
    switch(value->kind) {
    case LirTypeKind_Void: size = 0; break;
    case LirTypeKind_Bool: size = 1; break;
    case LirTypeKind_Integer:
    case LirTypeKind_Float: size = value->bits / 8; break;
    case LirTypeKind_Pointer:
    case LirTypeKind_Function: size = 8; break;
    case LirTypeKind_Array: {
        int64_t element = type_size_at(lower, value->element, depth + 1);
        if(element >= 0 && value->count >= 0 &&
           (element == 0 || value->count <= ZXW_IMAGE_LIMIT / element))
            size = element * value->count;
        break;
    }
    case LirTypeKind_Record: {
        const int32_t *fields = value->fields.data;
        size = 0;
        for(int64_t index = 0; index < value->fields.length; index++) {
            int64_t field = type_size_at(lower, fields[index], depth + 1);
            if(field < 0 || size > ZXW_IMAGE_LIMIT - field) {
                size = -1;
                break;
            }
            size += field;
        }
        break;
    }
    default: break;
    }
    lower->sizes[type] = size;
    return size;
}

static int64_t
type_size(Lower *lower, int32_t type)
{
    return type_size_at(lower, type, 0);
}

static int
type_aggregate(Lower *lower, int32_t type)
{
    LirTypeKind kind = type_at(lower, type)->kind;
    return kind == LirTypeKind_Array || kind == LirTypeKind_Record;
}

static const char *
kind_name(LirTypeKind kind)
{
    switch(kind) {
    case LirTypeKind_Void: return "void";
    case LirTypeKind_Bool: return "bool";
    case LirTypeKind_Integer: return "integer";
    case LirTypeKind_Float: return "float";
    case LirTypeKind_Pointer: return "pointer";
    case LirTypeKind_Array: return "array";
    case LirTypeKind_Record: return "record";
    case LirTypeKind_Vector: return "vector";
    case LirTypeKind_Function: return "function";
    case LirTypeKind_Capability: return "capability";
    default: return "invalid";
    }
}

/* The wasm value type of a scalar LIR type, or 0 for void and aggregates.
 * Unsupported scalar shapes are a named gap. */
static unsigned
valtype(Lower *lower, int32_t type)
{
    const LirType *value = type_at(lower, type);
    switch(value->kind) {
    case LirTypeKind_Bool:
    case LirTypeKind_Pointer:
    case LirTypeKind_Function: return W_I32;
    case LirTypeKind_Integer:
        if(value->bits == 8 || value->bits == 16 || value->bits == 32) return W_I32;
        if(value->bits == 64) return W_I64;
        WasmGap(lower, "wasm.gap.integer-width", "LIR integer width %d has no wasm32 representation", value->bits);
        return 0;
    case LirTypeKind_Float:
        if(value->bits == 32) return W_F32;
        if(value->bits == 64) return W_F64;
        WasmGap(lower, "wasm.gap.float-width", "LIR float width %d has no wasm32 representation", value->bits);
        return 0;
    case LirTypeKind_Void:
    case LirTypeKind_Array:
    case LirTypeKind_Record: return 0;
    default:
        WasmGap(lower, "wasm.gap.type", "LIR %s values have no wasm32 representation yet", kind_name(value->kind));
        return 0;
    }
}

static int
type_lowered(Lower *lower, int32_t type)
{
    if(!type_valid(lower, type))
        return WasmGap(lower, "wasm.gap.type", "LIR type t%d is out of range", type);
    if(type_aggregate(lower, type) || type_at(lower, type)->kind == LirTypeKind_Void)
        return type_size(lower, type) >= 0 ||
               WasmGap(lower, "wasm.gap.type", "LIR type t%d has no bounded wasm32 layout", type);
    return valtype(lower, type) != 0;
}

static int
integer_small(const LirType *type)
{
    return type->kind == LirTypeKind_Integer && type->bits < 32;
}

static uint64_t
integer_mask(int bits)
{
    return bits >= 64 ? ~(uint64_t)0 : (((uint64_t)1 << bits) - 1);
}

/* Find or add a signature; returns its type index. */
static uint32_t
signature_index(Lower *lower, const Sig *sig)
{
    for(size_t index = 0; index < lower->sig_count; index++) {
        const Sig *other = &lower->sigs[index];
        if(other->params == sig->params && other->result == sig->result &&
           memcmp(other->types, sig->types, sig->params) == 0)
            return (uint32_t)index;
    }
    if(lower->sig_count == lower->sig_capacity) {
        size_t capacity = lower->sig_capacity ? lower->sig_capacity * 2 : 16;
        Sig *sigs = realloc(lower->sigs, capacity * sizeof(*sigs));
        if(sigs == NULL) {
            lower->failed = 1;
            DiagnosticOutOfMemory();
            return 0;
        }
        lower->sigs = sigs;
        lower->sig_capacity = capacity;
    }
    lower->sigs[lower->sig_count] = *sig;
    return (uint32_t)lower->sig_count++;
}

/* The wasm signature of a LIR procedure shape: an aggregate result adds a
 * leading result address, and aggregate parameters pass their address. */
static int
signature_of(Lower *lower, const int32_t *parameters, int64_t count, int32_t result, Sig *sig)
{
    memset(sig, 0, sizeof(*sig));
    if(count < 0 || count > 500)
        return WasmGap(lower, "wasm.gap.signature", "procedure with %lld parameters", (long long)count);
    if(!type_lowered(lower, result))
        return 0;
    if(type_aggregate(lower, result))
        sig->types[sig->params++] = W_I32;
    else
        sig->result = valtype(lower, result);
    for(int64_t index = 0; index < count; index++) {
        if(!type_lowered(lower, parameters[index]))
            return 0;
        if(type_at(lower, parameters[index])->kind == LirTypeKind_Void)
            return WasmGap(lower, "wasm.gap.signature", "void parameter");
        sig->types[sig->params++] = type_aggregate(lower, parameters[index]) ? W_I32 :
            (unsigned char)valtype(lower, parameters[index]);
    }
    return !lower->failed;
}

static int
text_is(String text, const char *expected)
{
    size_t length = strlen(expected);
    return text.length == length && memcmp(text.data, expected, length) == 0;
}

static int
text_prefix(String text, const char *prefix)
{
    size_t length = strlen(prefix);
    return text.length >= length && memcmp(text.data, prefix, length) == 0;
}

static char *
text_copy(String text)
{
    char *copy = malloc(text.length + 1);
    if(copy != NULL) {
        memcpy(copy, text.data, text.length);
        copy[text.length] = '\0';
    }
    return copy;
}

/* Classify every external procedure once: platform output services map to
 * the reserved print indices, every other host procedure is a declared
 * capability with its own index. */
static int
classify_externals(Lower *lower)
{
    for(int64_t index = 0; index < lower->function_count; index++) {
        const LirFunction *function = &lower->functions[index];
        if(!function->external)
            continue;
        String name = function->name;
        const int32_t *parameters = function->parameters.data;
        int64_t count = function->parameters.length;
        const LirType *first = count > 0 ? type_at(lower, parameters[0]) : type_at(lower, -1);
        unsigned char kind = HOST_NONE;
        if(text_is(name, "ziran.platform.write_all")) kind = HOST_WRITE;
        else if(text_is(name, "ziran.print.signed")) kind = HOST_SIGNED;
        else if(text_is(name, "ziran.print.unsigned")) kind = HOST_UNSIGNED;
        else if(text_is(name, "ziran.print.bool")) kind = HOST_BOOL;
        else if(text_is(name, "ziran.print.float32")) kind = HOST_FLOAT32;
        else if(text_is(name, "ziran.print.float64")) kind = HOST_FLOAT64;
        else if(text_prefix(name, "ziran.platform.") || text_prefix(name, "ziran.print."))
            return WasmGap(lower, "wasm.gap.platform-service-unavailable",
                       "the wasm32 target does not implement platform service %.*s",
                       (int)name.length, name.data);
        else if(function->calling != LirCalling_Host)
            return WasmGap(lower, "wasm.gap.foreign-service",
                       "external procedure %.*s is not a host capability",
                       (int)name.length, name.data);
        else
            kind = HOST_CAPABILITY;
        int valid = 1;
        if(kind == HOST_WRITE)
            valid = count == 3 && type_at(lower, parameters[0])->kind == LirTypeKind_Integer &&
                type_at(lower, parameters[1])->kind == LirTypeKind_Pointer &&
                type_at(lower, parameters[2])->kind == LirTypeKind_Integer;
        else if(kind == HOST_SIGNED || kind == HOST_UNSIGNED)
            valid = count == 1 && first->kind == LirTypeKind_Integer;
        else if(kind == HOST_BOOL)
            valid = count == 1 && (first->kind == LirTypeKind_Bool || first->kind == LirTypeKind_Integer);
        else if(kind == HOST_FLOAT32 || kind == HOST_FLOAT64)
            valid = count == 1 && first->kind == LirTypeKind_Float &&
                first->bits == (kind == HOST_FLOAT32 ? 32 : 64);
        if(kind != HOST_CAPABILITY && (!valid || type_at(lower, function->result)->kind != LirTypeKind_Void))
            return WasmGap(lower, "wasm.gap.platform-service-abi",
                       "platform service %.*s has an unexpected LIR signature",
                       (int)name.length, name.data);
        if(kind == HOST_CAPABILITY) {
            const LirType *result = type_at(lower, function->result);
            if(count != 1 || first->kind != LirTypeKind_Integer || first->bits != 64 ||
               !first->is_signed || result->kind != LirTypeKind_Integer ||
               result->bits != 64 || !result->is_signed)
                return WasmGap(lower, "wasm.gap.host-capability-abi",
                           "host capability %.*s is not s64 -> s64; the wasm32 host bridge carries typed s64 -> s64 calls only",
                           (int)name.length, name.data);
            size_t capacity = lower->capability_count + 1;
            ZxWasmCapability *capabilities = realloc(lower->capabilities, capacity * sizeof(*capabilities));
            if(capabilities == NULL) {
                lower->failed = 1;
                DiagnosticOutOfMemory();
                return 0;
            }
            lower->capabilities = capabilities;
            String module = function->module >= 0 && function->module < lower->module_count ?
                lower->modules[function->module] : (String){"", 0};
            ZxWasmCapability *capability = &capabilities[lower->capability_count];
            capability->module = text_copy(module);
            capability->function = text_copy(name);
            if(capability->module == NULL || capability->function == NULL) {
                free(capability->module);
                free(capability->function);
                lower->failed = 1;
                DiagnosticOutOfMemory();
                return 0;
            }
            lower->host_index[index] = (uint32_t)lower->capability_count++;
        }
        lower->host_kind[index] = kind;
    }
    return 1;
}

/* Data region: writable and read-only globals first, then constant blobs. */
static uint32_t
data_reserve(Lower *lower, int64_t size)
{
    size_t start = (lower->data.count + 7) & ~(size_t)7;
    if(size < 0 || (uint64_t)size > (uint64_t)ZXW_IMAGE_LIMIT - start) {
        WasmGap(lower, "wasm.gap.data-size", "static data exceeds the wasm32 image limit");
        return 0;
    }
    buf_reserve(&lower->data, start - lower->data.count + (size_t)size);
    if(lower->data.failed) {
        lower->failed = 1;
        DiagnosticOutOfMemory();
        return 0;
    }
    memset(lower->data.data + lower->data.count, 0, start - lower->data.count + (size_t)size);
    lower->data.count = start + (size_t)size;
    return (uint32_t)(ZXW_DATA_BASE + start);
}

static void
data_write_bits(Lower *lower, uint32_t address, int64_t size, uint64_t bits)
{
    for(int64_t at = 0; at < size && at < 8; at++)
        lower->data.data[address - ZXW_DATA_BASE + at] = (unsigned char)(bits >> (8 * at));
}

static int
layout_globals(Lower *lower)
{
    for(int64_t index = 0; index < lower->global_count; index++) {
        const LirGlobal *global = &lower->globals[index];
        int64_t size = type_size(lower, global->type);
        if(size < 0 || !type_lowered(lower, global->type))
            return WasmGap(lower, "wasm.gap.global-layout", "global %.*s has no bounded wasm32 layout",
                       (int)global->name.length, global->name.data);
        uint32_t address = data_reserve(lower, size);
        if(lower->failed)
            return 0;
        lower->global_address[index] = address;
        if(global->constant == LirNoId)
            continue;
        if(global->constant < 0 || global->constant >= lower->constant_count)
            return WasmGap(lower, "wasm.gap.global-constant", "global %.*s names a missing constant",
                       (int)global->name.length, global->name.data);
        const LirConstant *constant = &lower->constants[global->constant];
        const LirType *type = type_at(lower, global->type);
        if(type->kind == LirTypeKind_Array) {
            int64_t count = constant->bytes.length < size ? constant->bytes.length : size;
            if(count > 0)
                memcpy(lower->data.data + (address - ZXW_DATA_BASE), constant->bytes.data, (size_t)count);
        } else if(size <= 8) {
            uint64_t bits = constant->bits;
            if(type->kind == LirTypeKind_Bool) bits = bits != 0;
            if(type->kind == LirTypeKind_Integer) bits &= integer_mask(type->bits);
            data_write_bits(lower, address, size, bits);
        } else {
            return WasmGap(lower, "wasm.gap.global-constant",
                       "global %.*s has a %s initial constant wider than one scalar",
                       (int)global->name.length, global->name.data, kind_name(type->kind));
        }
    }
    return 1;
}

/* ---------------------------------------------------------------- bodies */

typedef struct Fn {
    Lower *lower;
    const LirFunction *function;
    Buf code;
    unsigned char *locals;
    uint32_t local_count;
    uint32_t local_capacity;
    uint32_t param_count;
    int32_t *value_local;
    int64_t *value_slot;
    int64_t *alloca_slot;
    int64_t frame;
    int64_t scratch;
    uint32_t fp;
    uint32_t pc;
    uint32_t result_address;
    int has_frame;
    int dispatch;
    int64_t block_count;
    uint32_t temp[4][3];
    unsigned temp_used[4];
} Fn;

static uint32_t
fn_local(Fn *fn, unsigned type)
{
    if(fn->local_count == fn->local_capacity) {
        uint32_t capacity = fn->local_capacity ? fn->local_capacity * 2 : 32;
        unsigned char *locals = realloc(fn->locals, capacity);
        if(locals == NULL) {
            fn->lower->failed = 1;
            DiagnosticOutOfMemory();
            return 0;
        }
        fn->locals = locals;
        fn->local_capacity = capacity;
    }
    fn->locals[fn->local_count] = (unsigned char)type;
    return fn->local_count++;
}

static int
temp_slot(unsigned type)
{
    return type == W_I32 ? 0 : type == W_I64 ? 1 : type == W_F32 ? 2 : 3;
}

/* Scratch locals live only inside one instruction's sequence. */
static uint32_t
fn_temp(Fn *fn, unsigned type, unsigned which)
{
    int slot = temp_slot(type);
    while(fn->temp_used[slot] <= which) {
        fn->temp[slot][fn->temp_used[slot]] = fn_local(fn, type);
        fn->temp_used[slot]++;
    }
    return fn->temp[slot][which];
}

static void op(Fn *fn, unsigned code) { buf_byte(&fn->code, code); }
static void op_u(Fn *fn, unsigned code, uint64_t value) { buf_byte(&fn->code, code); buf_uleb(&fn->code, value); }
static void local_get(Fn *fn, uint32_t local) { op_u(fn, 0x20, local); }
static void local_set(Fn *fn, uint32_t local) { op_u(fn, 0x21, local); }
static void global_get(Fn *fn, uint32_t global) { op_u(fn, 0x23, global); }
static void global_set(Fn *fn, uint32_t global) { op_u(fn, 0x24, global); }
static void i32_const(Fn *fn, int32_t value) { buf_byte(&fn->code, 0x41); buf_sleb(&fn->code, value); }
static void i64_const(Fn *fn, int64_t value) { buf_byte(&fn->code, 0x42); buf_sleb(&fn->code, value); }
static void memarg(Fn *fn, unsigned code, uint32_t offset) { buf_byte(&fn->code, code); buf_uleb(&fn->code, 0); buf_uleb(&fn->code, offset); }
static void memory_copy(Fn *fn) { buf_byte(&fn->code, 0xfc); buf_uleb(&fn->code, 10); buf_byte(&fn->code, 0); buf_byte(&fn->code, 0); }
static void memory_fill(Fn *fn) { buf_byte(&fn->code, 0xfc); buf_uleb(&fn->code, 11); buf_byte(&fn->code, 0); }
static void trap_if(Fn *fn) { op(fn, 0x04); op(fn, 0x40); op(fn, 0x00); op(fn, 0x0b); }

static int
value_valid(Fn *fn, int32_t value)
{
    return value >= 0 && value < fn->function->values.length;
}

static int32_t
value_type(Fn *fn, int32_t value)
{
    return value_valid(fn, value) ? ((const int32_t *)fn->function->values.data)[value] : LirNoId;
}

static const LirType *
value_lir(Fn *fn, int32_t value)
{
    return type_at(fn->lower, value_type(fn, value));
}

static int
value_scalar(Fn *fn, int32_t value)
{
    return value_valid(fn, value) && fn->value_local[value] >= 0;
}

static unsigned
value_wasm(Fn *fn, int32_t value)
{
    return value_scalar(fn, value) ? fn->locals[fn->value_local[value]] : 0;
}

static void
get(Fn *fn, int32_t value)
{
    if(!value_scalar(fn, value)) {
        WasmGap(fn->lower, "wasm.gap.operand", "operand v%d is not a scalar value", value);
        return;
    }
    local_get(fn, (uint32_t)fn->value_local[value]);
}

static void
set(Fn *fn, int32_t value)
{
    if(!value_scalar(fn, value)) {
        WasmGap(fn->lower, "wasm.gap.result", "result v%d is not a scalar value", value);
        return;
    }
    local_set(fn, (uint32_t)fn->value_local[value]);
}

/* An address: a base (pointer local, frame pointer, or absolute zero) plus a
 * constant byte offset. */
typedef struct Addr {
    int kind;
    uint32_t local;
    int64_t offset;
} Addr;
enum { ADDR_LOCAL, ADDR_FRAME, ADDR_ABSOLUTE };

static Addr
addr_of_slot(Fn *fn, int32_t value)
{
    return (Addr){ADDR_FRAME, 0, value_valid(fn, value) ? fn->value_slot[value] : 0};
}

static void
push_base(Fn *fn, Addr address)
{
    if(address.kind == ADDR_LOCAL) local_get(fn, address.local);
    else if(address.kind == ADDR_FRAME) local_get(fn, fn->fp);
    else i32_const(fn, 0);
}

static void
push_full(Fn *fn, Addr address)
{
    if(address.kind == ADDR_ABSOLUTE) {
        i32_const(fn, (int32_t)address.offset);
        return;
    }
    push_base(fn, address);
    if(address.offset != 0) {
        i32_const(fn, (int32_t)address.offset);
        op(fn, 0x6a);
    }
}

static int
offset_fits(Fn *fn, int64_t offset)
{
    return offset >= 0 && offset <= 0x7fffffff ? 1 :
        WasmGap(fn->lower, "wasm.gap.offset", "memory offset %lld is outside wasm32", (long long)offset);
}

/* Scalar memory access of one LIR type at base + memarg offset. */
static void
load_scalar(Fn *fn, int32_t type, uint32_t offset)
{
    const LirType *value = type_at(fn->lower, type);
    switch(value->kind) {
    case LirTypeKind_Bool:
        memarg(fn, 0x2d, offset); i32_const(fn, 0); op(fn, 0x47); return;
    case LirTypeKind_Integer:
        if(value->bits == 8) memarg(fn, 0x2d, offset);
        else if(value->bits == 16) memarg(fn, 0x2f, offset);
        else if(value->bits == 32) memarg(fn, 0x28, offset);
        else memarg(fn, 0x29, offset);
        return;
    case LirTypeKind_Float:
        memarg(fn, value->bits == 32 ? 0x2a : 0x2b, offset); return;
    case LirTypeKind_Pointer:
    case LirTypeKind_Function:
        memarg(fn, 0x29, offset); op(fn, 0xa7); return;
    default:
        WasmGap(fn->lower, "wasm.gap.load", "load of LIR %s", kind_name(value->kind));
    }
}

/* Expects base then value on the stack. */
static void
store_scalar(Fn *fn, int32_t type, uint32_t offset)
{
    const LirType *value = type_at(fn->lower, type);
    switch(value->kind) {
    case LirTypeKind_Bool: memarg(fn, 0x3a, offset); return;
    case LirTypeKind_Integer:
        if(value->bits == 8) memarg(fn, 0x3a, offset);
        else if(value->bits == 16) memarg(fn, 0x3b, offset);
        else if(value->bits == 32) memarg(fn, 0x36, offset);
        else memarg(fn, 0x37, offset);
        return;
    case LirTypeKind_Float: memarg(fn, value->bits == 32 ? 0x38 : 0x39, offset); return;
    case LirTypeKind_Pointer:
    case LirTypeKind_Function: op(fn, 0xad); memarg(fn, 0x37, offset); return;
    default:
        WasmGap(fn->lower, "wasm.gap.store", "store of LIR %s", kind_name(value->kind));
    }
}

/* Copies a value (scalar or aggregate) of `type` into memory. */
static void
store_value(Fn *fn, Addr destination, int32_t value)
{
    if(!offset_fits(fn, destination.offset))
        return;
    if(value_scalar(fn, value)) {
        push_base(fn, destination);
        get(fn, value);
        store_scalar(fn, value_type(fn, value), (uint32_t)destination.offset);
        return;
    }
    int64_t size = type_size(fn->lower, value_type(fn, value));
    if(size <= 0)
        return;
    push_full(fn, destination);
    push_full(fn, addr_of_slot(fn, value));
    i32_const(fn, (int32_t)size);
    memory_copy(fn);
}

static void
load_value(Fn *fn, Addr source, int32_t value)
{
    if(!offset_fits(fn, source.offset))
        return;
    if(value_scalar(fn, value)) {
        push_base(fn, source);
        load_scalar(fn, value_type(fn, value), (uint32_t)source.offset);
        set(fn, value);
        return;
    }
    int64_t size = type_size(fn->lower, value_type(fn, value));
    if(size <= 0)
        return;
    push_full(fn, addr_of_slot(fn, value));
    push_full(fn, source);
    i32_const(fn, (int32_t)size);
    memory_copy(fn);
}

/* Keeps an i32 integer canonical after an operation of a narrow type. */
static void
canonical(Fn *fn, const LirType *type)
{
    if(integer_small(type)) {
        i32_const(fn, (int32_t)integer_mask(type->bits));
        op(fn, 0x71);
    }
}

/* Pushes an operand sign-extended within i32 when its type is signed narrow. */
static void
get_signed(Fn *fn, int32_t value)
{
    const LirType *type = value_lir(fn, value);
    get(fn, value);
    if(type->kind == LirTypeKind_Integer && type->is_signed) {
        if(type->bits == 8) op(fn, 0xc0);
        else if(type->bits == 16) op(fn, 0xc1);
    }
}

/* Pushes an integer operand widened to i64 (zero- or sign-extended). */
static void
get_i64(Fn *fn, int32_t value, int sign)
{
    unsigned wasm = value_wasm(fn, value);
    if(sign) get_signed(fn, value);
    else get(fn, value);
    if(wasm == W_I32) op(fn, sign && value_lir(fn, value)->is_signed ? 0xac : 0xad);
}

static int
member_offset(Fn *fn, int32_t type, uint64_t member, int64_t *offset, int32_t *member_type)
{
    Lower *lower = fn->lower;
    const LirType *value = type_at(lower, type);
    if(value->kind == LirTypeKind_Array) {
        if(member >= (uint64_t)value->count)
            return 0;
        *member_type = value->element;
        *offset = (int64_t)member * type_size(lower, value->element);
        return 1;
    }
    if(value->kind != LirTypeKind_Record || member >= (uint64_t)value->fields.length)
        return 0;
    const int32_t *fields = value->fields.data;
    int64_t at = 0;
    for(uint64_t index = 0; index < member; index++)
        at += type_size(lower, fields[index]);
    *member_type = fields[member];
    *offset = at;
    return 1;
}

static const LirInstruction *
instructions_of(const LirBlock *block)
{
    return block->instructions.data;
}

/* Edge copies: parameters are assigned in parallel. */
static void
edge_copy(Fn *fn, const LirEdge *edge)
{
    const LirBlock *blocks = fn->function->blocks.data;
    if(edge->block < 0 || edge->block >= fn->block_count) {
        WasmGap(fn->lower, "wasm.gap.edge", "edge target b%d is out of range", edge->block);
        return;
    }
    const LirBlock *target = &blocks[edge->block];
    const int32_t *arguments = edge->arguments.data;
    const int32_t *parameters = target->parameters.data;
    if(edge->arguments.length != target->parameters.length) {
        WasmGap(fn->lower, "wasm.gap.edge", "edge argument count differs from b%d", edge->block);
        return;
    }
    int64_t count = edge->arguments.length;
    int aggregates = 0;
    for(int64_t index = 0; index < count; index++)
        if(!value_scalar(fn, parameters[index]) && arguments[index] != parameters[index])
            aggregates++;
    if(aggregates == 1) {
        for(int64_t index = 0; index < count; index++)
            if(!value_scalar(fn, parameters[index]) && arguments[index] != parameters[index])
                store_value(fn, addr_of_slot(fn, parameters[index]), arguments[index]);
    } else if(aggregates > 1) {
        int64_t at = fn->scratch;
        for(int64_t index = 0; index < count; index++) {
            if(value_scalar(fn, parameters[index]) || arguments[index] == parameters[index]) continue;
            store_value(fn, (Addr){ADDR_FRAME, 0, at}, arguments[index]);
            at += type_size(fn->lower, value_type(fn, parameters[index]));
        }
        at = fn->scratch;
        for(int64_t index = 0; index < count; index++) {
            if(value_scalar(fn, parameters[index]) || arguments[index] == parameters[index]) continue;
            load_value(fn, (Addr){ADDR_FRAME, 0, at}, parameters[index]);
            at += type_size(fn->lower, value_type(fn, parameters[index]));
        }
    }
    for(int64_t index = 0; index < count; index++)
        if(value_scalar(fn, parameters[index]) && arguments[index] != parameters[index])
            get(fn, arguments[index]);
    for(int64_t index = count - 1; index >= 0; index--)
        if(value_scalar(fn, parameters[index]) && arguments[index] != parameters[index])
            set(fn, parameters[index]);
}

/* Transfers control from code(b_current) to b_target, `extra` labels deep. */
static void
edge_jump(Fn *fn, int64_t current, const LirEdge *edge, unsigned extra)
{
    edge_copy(fn, edge);
    int64_t target = edge->block;
    if(target > current) {
        if(target == current + 1 && extra == 0)
            return;
        op_u(fn, 0x0c, (uint64_t)(target - current - 1 + extra));
        return;
    }
    if(!fn->dispatch) {
        WasmGap(fn->lower, "wasm.gap.control", "backward edge without a dispatch loop");
        return;
    }
    i32_const(fn, (int32_t)target);
    local_set(fn, fn->pc);
    op_u(fn, 0x0c, (uint64_t)(fn->block_count - 1 - current + extra));
}

static void
epilogue(Fn *fn)
{
    if(fn->has_frame) {
        local_get(fn, fn->fp);
        global_set(fn, G_SP);
    }
}

static int
operand_count(Fn *fn, const LirInstruction *instruction, int64_t minimum, int64_t maximum)
{
    if(instruction->operands.length < minimum || instruction->operands.length > maximum)
        return WasmGap(fn->lower, "wasm.gap.instruction", "%.*s has %lld operands",
                   (int)compiler_lowered_ir_LirOpName(instruction->op).length,
                   compiler_lowered_ir_LirOpName(instruction->op).data,
                   (long long)instruction->operands.length);
    return 1;
}

static void
lower_constant(Fn *fn, const LirInstruction *instruction)
{
    Lower *lower = fn->lower;
    if(instruction->symbol < 0 || instruction->symbol >= lower->constant_count) {
        WasmGap(lower, "wasm.gap.constant", "constant c%d is out of range", instruction->symbol);
        return;
    }
    const LirConstant *constant = &lower->constants[instruction->symbol];
    int32_t type = value_type(fn, instruction->result);
    const LirType *lir = type_at(lower, type);
    if(!value_scalar(fn, instruction->result)) {
        int64_t size = type_size(lower, type);
        if(lir->kind != LirTypeKind_Array || size < 0) {
            WasmGap(lower, "wasm.gap.constant", "LIR %s constants", kind_name(lir->kind));
            return;
        }
        uint32_t address = data_reserve(lower, size);
        if(lower->failed)
            return;
        int64_t count = constant->bytes.length < size ? constant->bytes.length : size;
        if(count > 0)
            memcpy(lower->data.data + (address - ZXW_DATA_BASE), constant->bytes.data, (size_t)count);
        if(size > 0) {
            push_full(fn, addr_of_slot(fn, instruction->result));
            i32_const(fn, (int32_t)address);
            i32_const(fn, (int32_t)size);
            memory_copy(fn);
        }
        return;
    }
    uint64_t bits = constant->bits;
    switch(lir->kind) {
    case LirTypeKind_Bool: i32_const(fn, bits != 0); break;
    case LirTypeKind_Integer:
        bits &= integer_mask(lir->bits);
        if(lir->bits == 64) i64_const(fn, (int64_t)bits);
        else i32_const(fn, (int32_t)(uint32_t)bits);
        break;
    case LirTypeKind_Float: {
        unsigned char raw[8];
        for(int at = 0; at < 8; at++) raw[at] = (unsigned char)(bits >> (8 * at));
        op(fn, lir->bits == 32 ? 0x43 : 0x44);
        buf_raw(&fn->code, raw, lir->bits == 32 ? 4 : 8);
        break;
    }
    case LirTypeKind_Pointer:
    case LirTypeKind_Function:
        if(bits != 0) {
            WasmGap(lower, "wasm.gap.constant", "non-null address constant");
            return;
        }
        i32_const(fn, 0);
        break;
    default:
        WasmGap(lower, "wasm.gap.constant", "LIR %s constants", kind_name(lir->kind));
        return;
    }
    set(fn, instruction->result);
}

static void
lower_binary(Fn *fn, const LirInstruction *instruction)
{
    Lower *lower = fn->lower;
    if(!operand_count(fn, instruction, 2, 2))
        return;
    const int32_t *operands = instruction->operands.data;
    int32_t left = operands[0], right = operands[1];
    const LirType *type = value_lir(fn, left);
    unsigned wasm = value_wasm(fn, left);
    LirOp code = instruction->op;
    if(type->kind == LirTypeKind_Float) {
        int single = type->bits == 32;
        unsigned base = single ? 0x92 : 0xa0;
        unsigned opcode;
        if(code == LirOp_Add) opcode = base;
        else if(code == LirOp_Subtract) opcode = base + 1;
        else if(code == LirOp_Multiply) opcode = base + 2;
        else if(code == LirOp_Divide) opcode = base + 3;
        else {
            WasmGap(lower, "wasm.gap.float-operator", "float %.*s",
                (int)compiler_lowered_ir_LirOpName(code).length, compiler_lowered_ir_LirOpName(code).data);
            return;
        }
        get(fn, left); get(fn, right); op(fn, opcode);
        set(fn, instruction->result);
        return;
    }
    if(type->kind != LirTypeKind_Integer || (wasm != W_I32 && wasm != W_I64)) {
        WasmGap(lower, "wasm.gap.operator", "arithmetic on LIR %s", kind_name(type->kind));
        return;
    }
    int wide = wasm == W_I64;
    int sign = type->is_signed;
    if(code == LirOp_ShiftLeft || code == LirOp_ShiftRight) {
        const LirType *count_type = value_lir(fn, right);
        unsigned count_wasm = value_wasm(fn, right);
        if(count_type->kind != LirTypeKind_Integer) {
            WasmGap(lower, "wasm.gap.operator", "shift count of LIR %s", kind_name(count_type->kind));
            return;
        }
        get(fn, right);
        if(count_wasm == W_I64) { i64_const(fn, type->bits); op(fn, 0x5a); }
        else { i32_const(fn, type->bits); op(fn, 0x4f); }
        trap_if(fn);
        if(code == LirOp_ShiftRight && sign) get_signed(fn, left);
        else get(fn, left);
        get(fn, right);
        if(wide && count_wasm == W_I32) op(fn, 0xad);
        if(!wide && count_wasm == W_I64) op(fn, 0xa7);
        if(code == LirOp_ShiftLeft) op(fn, wide ? 0x86 : 0x74);
        else op(fn, wide ? (sign ? 0x87 : 0x88) : (sign ? 0x75 : 0x76));
        canonical(fn, type);
        set(fn, instruction->result);
        return;
    }
    if((code == LirOp_Divide || code == LirOp_Remainder) && sign && !integer_small(type)) {
        /* Wasm traps on MIN / -1; the interpreter defines MIN and 0. */
        get(fn, right);
        if(wide) { i64_const(fn, -1); op(fn, 0x51); } else { i32_const(fn, -1); op(fn, 0x46); }
        op(fn, 0x04); op(fn, wide ? W_I64 : W_I32);
        if(code == LirOp_Divide) {
            if(wide) { i64_const(fn, 0); get(fn, left); op(fn, 0x7d); }
            else { i32_const(fn, 0); get(fn, left); op(fn, 0x6b); }
        } else if(wide) i64_const(fn, 0);
        else i32_const(fn, 0);
        op(fn, 0x05);
        get(fn, left); get(fn, right);
        op(fn, code == LirOp_Divide ? (wide ? 0x7f : 0x6d) : (wide ? 0x81 : 0x6f));
        op(fn, 0x0b);
        set(fn, instruction->result);
        return;
    }
    int signed_operands = sign && (code == LirOp_Divide || code == LirOp_Remainder);
    if(signed_operands) { get_signed(fn, left); get_signed(fn, right); }
    else { get(fn, left); get(fn, right); }
    unsigned opcode;
    switch(code) {
    case LirOp_Add: opcode = wide ? 0x7c : 0x6a; break;
    case LirOp_Subtract: opcode = wide ? 0x7d : 0x6b; break;
    case LirOp_Multiply: opcode = wide ? 0x7e : 0x6c; break;
    case LirOp_Divide: opcode = wide ? (sign ? 0x7f : 0x80) : (sign ? 0x6d : 0x6e); break;
    case LirOp_Remainder: opcode = wide ? (sign ? 0x81 : 0x82) : (sign ? 0x6f : 0x70); break;
    case LirOp_BitAnd: opcode = wide ? 0x83 : 0x71; break;
    case LirOp_BitOr: opcode = wide ? 0x84 : 0x72; break;
    case LirOp_BitXor: opcode = wide ? 0x85 : 0x73; break;
    default:
        WasmGap(lower, "wasm.gap.operator", "integer operator %d", code);
        return;
    }
    op(fn, opcode);
    canonical(fn, type);
    set(fn, instruction->result);
}

static void
lower_compare(Fn *fn, const LirInstruction *instruction)
{
    if(!operand_count(fn, instruction, 2, 2))
        return;
    const int32_t *operands = instruction->operands.data;
    const LirType *type = value_lir(fn, operands[0]);
    unsigned wasm = value_wasm(fn, operands[0]);
    int relation = instruction->op - LirOp_Equal; /* eq ne lt le gt ge */
    static const unsigned char float32[] = {0x5b, 0x5c, 0x5d, 0x5f, 0x5e, 0x60};
    static const unsigned char float64[] = {0x61, 0x62, 0x63, 0x65, 0x64, 0x66};
    static const unsigned char i32s[] = {0x46, 0x47, 0x48, 0x4c, 0x4a, 0x4e};
    static const unsigned char i32u[] = {0x46, 0x47, 0x49, 0x4d, 0x4b, 0x4f};
    static const unsigned char i64s[] = {0x51, 0x52, 0x53, 0x57, 0x55, 0x59};
    static const unsigned char i64u[] = {0x51, 0x52, 0x54, 0x58, 0x56, 0x5a};
    if(relation < 0 || relation > 5 || wasm == 0) {
        WasmGap(fn->lower, "wasm.gap.compare", "comparison of LIR %s", kind_name(type->kind));
        return;
    }
    int sign = type->kind == LirTypeKind_Integer && type->is_signed;
    if(sign) { get_signed(fn, operands[0]); get_signed(fn, operands[1]); }
    else { get(fn, operands[0]); get(fn, operands[1]); }
    if(wasm == W_F32) op(fn, float32[relation]);
    else if(wasm == W_F64) op(fn, float64[relation]);
    else if(wasm == W_I64) op(fn, sign ? i64s[relation] : i64u[relation]);
    else op(fn, sign ? i32s[relation] : i32u[relation]);
    set(fn, instruction->result);
}

static void
lower_unary(Fn *fn, const LirInstruction *instruction)
{
    if(!operand_count(fn, instruction, 1, 1))
        return;
    int32_t source = ((const int32_t *)instruction->operands.data)[0];
    const LirType *type = value_lir(fn, source);
    unsigned wasm = value_wasm(fn, source);
    if(type->kind == LirTypeKind_Float && instruction->op == LirOp_Negate) {
        get(fn, source);
        op(fn, wasm == W_F32 ? 0x8c : 0x9a);
    } else if(type->kind == LirTypeKind_Integer) {
        if(instruction->op == LirOp_Negate) {
            if(wasm == W_I64) { i64_const(fn, 0); get(fn, source); op(fn, 0x7d); }
            else { i32_const(fn, 0); get(fn, source); op(fn, 0x6b); }
        } else {
            get(fn, source);
            if(wasm == W_I64) { i64_const(fn, -1); op(fn, 0x85); }
            else { i32_const(fn, -1); op(fn, 0x73); }
        }
        canonical(fn, type);
    } else {
        WasmGap(fn->lower, "wasm.gap.operator", "unary operator on LIR %s", kind_name(type->kind));
        return;
    }
    set(fn, instruction->result);
}

/* Narrow float-to-integer results must fit their destination. */
static void
check_narrow(Fn *fn, const LirType *destination)
{
    if(!integer_small(destination))
        return;
    uint32_t temp = fn_temp(fn, W_I32, 0);
    local_set(fn, temp);
    local_get(fn, temp);
    if(destination->is_signed) {
        op(fn, destination->bits == 8 ? 0xc0 : 0xc1);
        local_get(fn, temp);
        op(fn, 0x47);
    } else {
        i32_const(fn, (int32_t)integer_mask(destination->bits));
        op(fn, 0x4b);
    }
    trap_if(fn);
    local_get(fn, temp);
    canonical(fn, destination);
}

static void
lower_convert(Fn *fn, const LirInstruction *instruction)
{
    Lower *lower = fn->lower;
    if(!operand_count(fn, instruction, 1, 1))
        return;
    int32_t source = ((const int32_t *)instruction->operands.data)[0];
    const LirType *from = value_lir(fn, source);
    const LirType *to = value_lir(fn, instruction->result);
    unsigned from_wasm = value_wasm(fn, source);
    unsigned to_wasm = value_wasm(fn, instruction->result);
    if(from_wasm == 0 || to_wasm == 0) {
        WasmGap(lower, "wasm.gap.convert", "conversion between LIR %s and %s", kind_name(from->kind), kind_name(to->kind));
        return;
    }
    if(instruction->op == LirOp_Bitcast) {
        get(fn, source);
        if(from_wasm == W_I64 && to_wasm == W_F64) op(fn, 0xbf);
        else if(from_wasm == W_F64 && to_wasm == W_I64) op(fn, 0xbd);
        else if(from_wasm == W_I32 && to_wasm == W_F32) op(fn, 0xbe);
        else if(from_wasm == W_F32 && to_wasm == W_I32) op(fn, 0xbc);
        else if(from_wasm == W_I32 && to_wasm == W_I64) op(fn, 0xad);
        else if(from_wasm == W_I64 && to_wasm == W_I32) op(fn, 0xa7);
        else if(from_wasm != to_wasm) {
            WasmGap(lower, "wasm.gap.convert", "bitcast between LIR %s and %s", kind_name(from->kind), kind_name(to->kind));
            return;
        }
        if(to->kind == LirTypeKind_Bool) {
            if(to_wasm == W_I64) op(fn, 0xa7);
            i32_const(fn, 0); op(fn, 0x47);
        }
        canonical(fn, to);
        set(fn, instruction->result);
        return;
    }
    if(from->kind == LirTypeKind_Bool && to->kind == LirTypeKind_Integer) {
        get(fn, source);
        if(to_wasm == W_I64) op(fn, 0xad);
    } else if(from->kind == LirTypeKind_Integer && to->kind == LirTypeKind_Bool) {
        get(fn, source);
        op(fn, from_wasm == W_I64 ? 0x50 : 0x45);
        op(fn, 0x45);
    } else if(from->kind == LirTypeKind_Integer && to->kind == LirTypeKind_Integer) {
        get_signed(fn, source);
        if(from_wasm == W_I32 && to_wasm == W_I64) op(fn, from->is_signed ? 0xac : 0xad);
        else if(from_wasm == W_I64 && to_wasm == W_I32) op(fn, 0xa7);
        canonical(fn, to);
    } else if(from->kind == LirTypeKind_Integer && to->kind == LirTypeKind_Float) {
        get_signed(fn, source);
        int sign = from->is_signed;
        if(to_wasm == W_F32) op(fn, from_wasm == W_I64 ? (sign ? 0xb4 : 0xb5) : (sign ? 0xb2 : 0xb3));
        else op(fn, from_wasm == W_I64 ? (sign ? 0xb9 : 0xba) : (sign ? 0xb7 : 0xb8));
    } else if(from->kind == LirTypeKind_Float && to->kind == LirTypeKind_Float) {
        get(fn, source);
        if(from_wasm == W_F64 && to_wasm == W_F32) op(fn, 0xb6);
        else if(from_wasm == W_F32 && to_wasm == W_F64) op(fn, 0xbb);
    } else if(from->kind == LirTypeKind_Float && to->kind == LirTypeKind_Integer) {
        get(fn, source);
        int single = from_wasm == W_F32;
        int sign = to->is_signed;
        if(to_wasm == W_I64) op(fn, single ? (sign ? 0xae : 0xaf) : (sign ? 0xb0 : 0xb1));
        else op(fn, single ? (sign ? 0xa8 : 0xa9) : (sign ? 0xaa : 0xab));
        check_narrow(fn, to);
    } else {
        WasmGap(lower, "wasm.gap.convert", "conversion from LIR %s to %s", kind_name(from->kind), kind_name(to->kind));
        return;
    }
    set(fn, instruction->result);
}

/* Leaves the byte offset `index * size` as i32 on the stack after checking
 * index < count; index and count are integer values or count is constant. */
static void
checked_index(Fn *fn, int32_t index, int32_t count_value, int64_t count_constant, int64_t size)
{
    uint32_t temp = fn_temp(fn, W_I64, 0);
    get_i64(fn, index, 0);
    local_set(fn, temp);
    local_get(fn, temp);
    if(count_value >= 0) get_i64(fn, count_value, 0);
    else i64_const(fn, count_constant);
    op(fn, 0x5a); /* i64.ge_u */
    trap_if(fn);
    local_get(fn, temp);
    if(size != 1) { i64_const(fn, size); op(fn, 0x7e); }
    op(fn, 0xa7);
}

static void
lower_extract_insert(Fn *fn, const LirInstruction *instruction)
{
    Lower *lower = fn->lower;
    int insert = instruction->op == LirOp_Insert;
    int64_t fixed = insert ? 2 : 1;
    if(!operand_count(fn, instruction, fixed, fixed + 1))
        return;
    const int32_t *operands = instruction->operands.data;
    int32_t aggregate = operands[0];
    if(value_scalar(fn, aggregate)) {
        WasmGap(lower, "wasm.gap.aggregate", "%s of a scalar value", insert ? "insert" : "extract");
        return;
    }
    int32_t aggregate_type = value_type(fn, aggregate);
    int32_t target = insert ? instruction->result : aggregate;
    if(insert && instruction->result != aggregate) {
        int64_t size = type_size(lower, aggregate_type);
        if(size > 0) {
            push_full(fn, addr_of_slot(fn, instruction->result));
            push_full(fn, addr_of_slot(fn, aggregate));
            i32_const(fn, (int32_t)size);
            memory_copy(fn);
        }
    }
    Addr place = addr_of_slot(fn, target);
    if(instruction->operands.length == fixed + 1) {
        const LirType *type = type_at(lower, aggregate_type);
        if(type->kind != LirTypeKind_Array) {
            WasmGap(lower, "wasm.gap.aggregate", "dynamic member of a LIR %s", kind_name(type->kind));
            return;
        }
        uint32_t address = fn_temp(fn, W_I32, 1);
        checked_index(fn, operands[fixed], -1, type->count, type_size(lower, type->element));
        push_full(fn, place);
        op(fn, 0x6a);
        local_set(fn, address);
        place = (Addr){ADDR_LOCAL, address, 0};
    } else {
        int64_t offset;
        int32_t member_type;
        if(!member_offset(fn, aggregate_type, instruction->immediate, &offset, &member_type)) {
            WasmGap(lower, "wasm.gap.aggregate", "member %llu is outside its aggregate",
                (unsigned long long)instruction->immediate);
            return;
        }
        place.offset += offset;
    }
    if(insert) store_value(fn, place, operands[1]);
    else load_value(fn, place, instruction->result);
}

static void
lower_aggregate(Fn *fn, const LirInstruction *instruction)
{
    int32_t type = value_type(fn, instruction->result);
    const LirType *lir = type_at(fn->lower, type);
    const int32_t *operands = instruction->operands.data;
    int64_t expected = lir->kind == LirTypeKind_Array ? lir->count : lir->fields.length;
    if(value_scalar(fn, instruction->result) || instruction->operands.length != expected) {
        WasmGap(fn->lower, "wasm.gap.aggregate", "aggregate of LIR %s with %lld operands",
            kind_name(lir->kind), (long long)instruction->operands.length);
        return;
    }
    Addr base = addr_of_slot(fn, instruction->result);
    int64_t at = 0;
    for(int64_t index = 0; index < instruction->operands.length; index++) {
        Addr place = base;
        place.offset += at;
        store_value(fn, place, operands[index]);
        at += type_size(fn->lower, value_type(fn, operands[index]));
    }
}

static void
lower_address(Fn *fn, const LirInstruction *instruction)
{
    Lower *lower = fn->lower;
    const int32_t *operands = instruction->operands.data;
    int64_t count = instruction->operands.length;
    if(count < 1 || count > 4) {
        operand_count(fn, instruction, 1, 4);
        return;
    }
    const LirType *pointer = value_lir(fn, operands[0]);
    const LirType *result = value_lir(fn, instruction->result);
    if(pointer->kind != LirTypeKind_Pointer || result->kind != LirTypeKind_Pointer) {
        WasmGap(lower, "wasm.gap.address", "address of LIR %s", kind_name(pointer->kind));
        return;
    }
    int32_t base_type = pointer->element;
    if(instruction->op == LirOp_FieldAddress && count == 1) {
        int64_t offset;
        int32_t member_type;
        if(!member_offset(fn, base_type, instruction->immediate, &offset, &member_type)) {
            op(fn, 0x00);
            return;
        }
        get(fn, operands[0]);
        if(offset) { i32_const(fn, (int32_t)offset); op(fn, 0x6a); }
        set(fn, instruction->result);
        return;
    }
    if(instruction->op != LirOp_ElementAddress || count < 2) {
        WasmGap(lower, "wasm.gap.address", "%s with %lld operands",
            instruction->op == LirOp_FieldAddress ? "field.address" : "element.address", (long long)count);
        return;
    }
    if(count == 2) {
        const LirType *aggregate = type_at(lower, base_type);
        if(aggregate->kind != LirTypeKind_Array) {
            WasmGap(lower, "wasm.gap.address", "dynamic member of a LIR %s", kind_name(aggregate->kind));
            return;
        }
        checked_index(fn, operands[1], -1, aggregate->count, type_size(lower, aggregate->element));
        get(fn, operands[0]);
        op(fn, 0x6a);
        set(fn, instruction->result);
        return;
    }
    int64_t size = type_size(lower, base_type);
    if(size < 0) {
        WasmGap(lower, "wasm.gap.address", "element of unbounded size");
        return;
    }
    if(count == 3) {
        checked_index(fn, operands[1], operands[2], 0, size);
        get(fn, operands[0]);
        op(fn, 0x6a);
        set(fn, instruction->result);
        return;
    }
    /* Slice: low <= high <= count; a null base needs all zero; an empty
     * slice keeps its base. */
    uint32_t low = fn_temp(fn, W_I64, 0), high = fn_temp(fn, W_I64, 1);
    get_i64(fn, operands[1], 0); local_set(fn, low);
    get_i64(fn, operands[2], 0); local_set(fn, high);
    local_get(fn, low); local_get(fn, high); op(fn, 0x56); /* low > high */
    local_get(fn, high); get_i64(fn, operands[3], 0); op(fn, 0x56); /* high > count */
    op(fn, 0x72);
    get(fn, operands[0]); op(fn, 0x45);
    get_i64(fn, operands[3], 0); op(fn, 0x50); op(fn, 0x45); /* count != 0 */
    op(fn, 0x71);
    op(fn, 0x72);
    trap_if(fn);
    get(fn, operands[0]);
    local_get(fn, low);
    if(size != 1) { i64_const(fn, size); op(fn, 0x7e); }
    op(fn, 0xa7);
    op(fn, 0x6a);
    get(fn, operands[0]);
    local_get(fn, low); local_get(fn, high); op(fn, 0x52); /* low != high */
    op(fn, 0x1b); /* select */
    set(fn, instruction->result);
}

static void
lower_memory(Fn *fn, const LirInstruction *instruction)
{
    const int32_t *operands = instruction->operands.data;
    if(instruction->op == LirOp_Load) {
        if(!operand_count(fn, instruction, 1, 1)) return;
        if(!value_scalar(fn, operands[0])) { WasmGap(fn->lower, "wasm.gap.load", "address is not a pointer value"); return; }
        load_value(fn, (Addr){ADDR_LOCAL, (uint32_t)fn->value_local[operands[0]], 0}, instruction->result);
        return;
    }
    if(!operand_count(fn, instruction, 2, 2)) return;
    if(!value_scalar(fn, operands[0])) { WasmGap(fn->lower, "wasm.gap.store", "address is not a pointer value"); return; }
    store_value(fn, (Addr){ADDR_LOCAL, (uint32_t)fn->value_local[operands[0]], 0}, operands[1]);
}

static void
lower_allocate(Fn *fn, const LirInstruction *instruction, int64_t slot)
{
    const LirType *pointer = value_lir(fn, instruction->result);
    int64_t size = type_size(fn->lower, pointer->element);
    if(pointer->kind != LirTypeKind_Pointer || size < 0 || slot < 0) {
        WasmGap(fn->lower, "wasm.gap.allocate", "allocation of an unbounded value");
        return;
    }
    Addr place = {ADDR_FRAME, 0, slot};
    if(instruction->operands.length == 1) {
        store_value(fn, place, ((const int32_t *)instruction->operands.data)[0]);
    } else if(size > 0) {
        push_full(fn, place);
        i32_const(fn, 0);
        i32_const(fn, (int32_t)size);
        memory_fill(fn);
    }
    push_full(fn, place);
    set(fn, instruction->result);
}

static void
lower_call(Fn *fn, const LirInstruction *instruction)
{
    Lower *lower = fn->lower;
    const int32_t *operands = instruction->operands.data;
    int indirect = instruction->op == LirOp_CallIndirect;
    const int32_t *parameters;
    int64_t parameter_count;
    int32_t result_type;
    if(indirect) {
        if(instruction->operands.length < 1) { operand_count(fn, instruction, 1, 1); return; }
        const LirType *callee = value_lir(fn, operands[0]);
        if(callee->kind != LirTypeKind_Function) { WasmGap(lower, "wasm.gap.call", "indirect call through LIR %s", kind_name(callee->kind)); return; }
        parameters = callee->parameters.data;
        parameter_count = callee->parameters.length;
        result_type = callee->result;
    } else {
        if(instruction->symbol < 0 || instruction->symbol >= lower->function_count) {
            WasmGap(lower, "wasm.gap.call", "call target @%d is out of range", instruction->symbol);
            return;
        }
        const LirFunction *callee = &lower->functions[instruction->symbol];
        if(callee->external) {
            WasmGap(lower, "wasm.gap.call", "direct call to external procedure %.*s",
                (int)callee->name.length, callee->name.data);
            return;
        }
        parameters = callee->parameters.data;
        parameter_count = callee->parameters.length;
        result_type = callee->result;
    }
    int64_t skip = indirect ? 1 : 0;
    if(instruction->operands.length - skip != parameter_count) {
        WasmGap(lower, "wasm.gap.call", "call argument count differs from its procedure");
        return;
    }
    (void)parameters;
    int aggregate_result = type_aggregate(lower, result_type);
    if(aggregate_result) {
        if(instruction->result == LirNoId) {
            WasmGap(lower, "wasm.gap.call", "discarded aggregate result");
            return;
        }
        push_full(fn, addr_of_slot(fn, instruction->result));
    }
    for(int64_t index = skip; index < instruction->operands.length; index++) {
        if(value_scalar(fn, operands[index])) get(fn, operands[index]);
        else push_full(fn, addr_of_slot(fn, operands[index]));
    }
    if(indirect) {
        Sig sig;
        if(!signature_of(lower, parameters, parameter_count, result_type, &sig))
            return;
        get(fn, operands[0]);
        op_u(fn, 0x11, signature_index(lower, &sig));
        buf_byte(&fn->code, 0);
    } else {
        op_u(fn, 0x10, lower->wasm_function[instruction->symbol]);
    }
    if(!aggregate_result && valtype(lower, result_type) != 0) {
        if(instruction->result == LirNoId) op(fn, 0x1a);
        else set(fn, instruction->result);
    }
}

static void
lower_host_call(Fn *fn, const LirInstruction *instruction)
{
    Lower *lower = fn->lower;
    if(instruction->symbol < 0 || instruction->symbol >= lower->function_count ||
       !lower->functions[instruction->symbol].external) {
        WasmGap(lower, "wasm.gap.host", "host call to a non-external procedure");
        return;
    }
    const LirFunction *callee = &lower->functions[instruction->symbol];
    const int32_t *operands = instruction->operands.data;
    if(instruction->operands.length != callee->parameters.length) {
        WasmGap(lower, "wasm.gap.host", "host call argument count differs from %.*s",
            (int)callee->name.length, callee->name.data);
        return;
    }
    unsigned kind = lower->host_kind[instruction->symbol];
    switch(kind) {
    case HOST_WRITE: {
        /* Only descriptor 1 is the module's builtin output stream. */
        get_i64(fn, operands[0], 0);
        i64_const(fn, 1);
        op(fn, 0x52);
        trap_if(fn);
        i32_const(fn, (int32_t)ZX_WASM_PRINT_STRING);
        get_i64(fn, operands[2], 0);
        i64_const(fn, 32);
        op(fn, 0x86);
        get(fn, operands[1]);
        op(fn, 0xad);
        op(fn, 0x84);
        break;
    }
    case HOST_SIGNED:
        i32_const(fn, (int32_t)ZX_WASM_PRINT_SIGNED);
        get_i64(fn, operands[0], 1);
        break;
    case HOST_UNSIGNED:
        i32_const(fn, (int32_t)ZX_WASM_PRINT_UNSIGNED);
        get_i64(fn, operands[0], 0);
        break;
    case HOST_BOOL:
        i32_const(fn, (int32_t)ZX_WASM_PRINT_BOOL);
        get_i64(fn, operands[0], 0);
        if(value_lir(fn, operands[0])->kind == LirTypeKind_Integer) { i64_const(fn, 0); op(fn, 0x52); op(fn, 0xad); }
        break;
    case HOST_FLOAT32:
        i32_const(fn, (int32_t)ZX_WASM_PRINT_FLOAT32);
        get(fn, operands[0]); op(fn, 0xbc); op(fn, 0xad);
        break;
    case HOST_FLOAT64:
        i32_const(fn, (int32_t)ZX_WASM_PRINT_FLOAT64);
        get(fn, operands[0]); op(fn, 0xbd);
        break;
    case HOST_CAPABILITY:
        i32_const(fn, (int32_t)lower->host_index[instruction->symbol]);
        get(fn, operands[0]);
        break;
    default:
        WasmGap(lower, "wasm.gap.host", "unclassified host procedure %.*s", (int)callee->name.length, callee->name.data);
        return;
    }
    op_u(fn, 0x10, 1);
    if(kind == HOST_CAPABILITY && instruction->result != LirNoId) set(fn, instruction->result);
    else op(fn, 0x1a);
}

static void
lower_fuel(Fn *fn, uint64_t amount)
{
    if(amount == 0)
        return;
    if(amount > INT64_MAX) {
        WasmGap(fn->lower, "wasm.gap.fuel", "fuel charge is outside i64");
        return;
    }
    global_get(fn, G_BUDGET); i64_const(fn, (int64_t)amount); op(fn, 0x53);
    op(fn, 0x04); op(fn, 0x40); op_u(fn, 0x10, 0); op(fn, 0x0b);
    global_get(fn, G_BUDGET); i64_const(fn, (int64_t)amount); op(fn, 0x7d); global_set(fn, G_BUDGET);
}

static void
lower_bytes_equal(Fn *fn, const LirInstruction *instruction)
{
    if(!operand_count(fn, instruction, 4, 4))
        return;
    const int32_t *operands = instruction->operands.data;
    get(fn, operands[0]); get_i64(fn, operands[1], 0);
    get(fn, operands[2]); get_i64(fn, operands[3], 0);
    op_u(fn, 0x10, fn->lower->bytes_equal_function);
    set(fn, instruction->result);
}

static void
lower_instruction(Fn *fn, int64_t block, const LirInstruction *instruction, int64_t slot)
{
    Lower *lower = fn->lower;
    const int32_t *operands = instruction->operands.data;
    const LirEdge *edges = instruction->edges.data;
    switch(instruction->op) {
    case LirOp_Constant: lower_constant(fn, instruction); return;
    case LirOp_Null: i32_const(fn, 0); set(fn, instruction->result); return;
    case LirOp_GlobalAddress:
        if(instruction->symbol < 0 || instruction->symbol >= lower->global_count) {
            WasmGap(lower, "wasm.gap.global", "global g%d is out of range", instruction->symbol);
            return;
        }
        i32_const(fn, (int32_t)lower->global_address[instruction->symbol]);
        set(fn, instruction->result);
        return;
    case LirOp_FunctionAddress:
        if(instruction->symbol < 0 || instruction->symbol >= lower->function_count) {
            WasmGap(lower, "wasm.gap.function", "function @%d is out of range", instruction->symbol);
            return;
        }
        i32_const(fn, instruction->symbol + 1);
        set(fn, instruction->result);
        return;
    case LirOp_Add: case LirOp_Subtract: case LirOp_Multiply: case LirOp_Divide:
    case LirOp_Remainder: case LirOp_BitAnd: case LirOp_BitOr: case LirOp_BitXor:
    case LirOp_ShiftLeft: case LirOp_ShiftRight:
        lower_binary(fn, instruction); return;
    case LirOp_Negate: case LirOp_BitNot: lower_unary(fn, instruction); return;
    case LirOp_Equal: case LirOp_NotEqual: case LirOp_Less: case LirOp_LessEqual:
    case LirOp_Greater: case LirOp_GreaterEqual:
        lower_compare(fn, instruction); return;
    case LirOp_Convert: case LirOp_Bitcast: lower_convert(fn, instruction); return;
    case LirOp_Aggregate: lower_aggregate(fn, instruction); return;
    case LirOp_Extract: case LirOp_Insert: lower_extract_insert(fn, instruction); return;
    case LirOp_Allocate: lower_allocate(fn, instruction, slot); return;
    case LirOp_Load: case LirOp_Store: lower_memory(fn, instruction); return;
    case LirOp_FieldAddress: case LirOp_ElementAddress: lower_address(fn, instruction); return;
    case LirOp_BytesEqual: lower_bytes_equal(fn, instruction); return;
    case LirOp_Call: case LirOp_CallIndirect: lower_call(fn, instruction); return;
    case LirOp_HostCall: lower_host_call(fn, instruction); return;
    case LirOp_Fuel: lower_fuel(fn, instruction->immediate); return;
    case LirOp_Trap: case LirOp_Unreachable: op(fn, 0x00); return;
    case LirOp_Jump:
        if(instruction->edges.length != 1) { WasmGap(lower, "wasm.gap.control", "jump without one edge"); return; }
        edge_jump(fn, block, &edges[0], 0);
        return;
    case LirOp_Branch:
        if(instruction->edges.length != 2 || !operand_count(fn, instruction, 1, 1)) {
            WasmGap(lower, "wasm.gap.control", "branch without a condition and two edges");
            return;
        }
        get(fn, operands[0]);
        op(fn, 0x04); op(fn, 0x40);
        edge_jump(fn, block, &edges[0], 1);
        op(fn, 0x0b);
        edge_jump(fn, block, &edges[1], 0);
        return;
    case LirOp_Return: {
        int32_t result = fn->function->result;
        if(type_aggregate(lower, result)) {
            if(!operand_count(fn, instruction, 1, 1)) return;
            int64_t size = type_size(lower, result);
            if(size > 0) {
                local_get(fn, fn->result_address);
                push_full(fn, addr_of_slot(fn, operands[0]));
                i32_const(fn, (int32_t)size);
                memory_copy(fn);
            }
        } else if(valtype(lower, result) != 0) {
            if(!operand_count(fn, instruction, 1, 1)) return;
            get(fn, operands[0]);
        }
        epilogue(fn);
        op(fn, 0x0f);
        return;
    }
    default:
        WasmGap(lower, "wasm.gap.instruction", "LIR %.*s instructions are not lowered to wasm32",
            (int)compiler_lowered_ir_LirOpName(instruction->op).length,
            compiler_lowered_ir_LirOpName(instruction->op).data);
    }
}

/* Lays out locals and frame slots, then emits one function body. */
static int
lower_function(Lower *lower, int64_t index, Buf *output)
{
    const LirFunction *function = &lower->functions[index];
    Fn fn = {0};
    fn.lower = lower;
    fn.function = function;
    fn.block_count = function->blocks.length;
    int64_t value_count = function->values.length;
    const int32_t *values = function->values.data;
    const LirBlock *blocks = function->blocks.data;
    const int32_t *parameters = function->parameters.data;
    int ok = 0;
    int64_t instruction_count = 0;
    for(int64_t block = 0; block < fn.block_count; block++)
        instruction_count += blocks[block].instructions.length;
    fn.value_local = calloc((size_t)(value_count ? value_count : 1), sizeof(*fn.value_local));
    fn.value_slot = calloc((size_t)(value_count ? value_count : 1), sizeof(*fn.value_slot));
    fn.alloca_slot = calloc((size_t)(instruction_count ? instruction_count : 1), sizeof(*fn.alloca_slot));
    if(fn.value_local == NULL || fn.value_slot == NULL || fn.alloca_slot == NULL) {
        DiagnosticOutOfMemory();
        lower->failed = 1;
        goto done;
    }
    if(fn.block_count < 1 || function->entry < 0 || function->entry >= fn.block_count) {
        WasmGap(lower, "wasm.gap.control", "procedure %.*s has no entry block",
            (int)function->name.length, function->name.data);
        goto done;
    }
    Sig sig;
    if(!signature_of(lower, parameters, function->parameters.length, function->result, &sig))
        goto done;
    for(size_t at = 0; at < sig.params; at++)
        fn_local(&fn, sig.types[at]);
    fn.param_count = (uint32_t)sig.params;
    uint32_t first_parameter = type_aggregate(lower, function->result) ? 1 : 0;
    fn.result_address = 0;
    for(int64_t value = 0; value < value_count; value++) {
        fn.value_local[value] = -1;
        fn.value_slot[value] = -1;
        if(!type_lowered(lower, values[value])) goto done;
        if(type_aggregate(lower, values[value])) {
            fn.frame = (fn.frame + 7) & ~(int64_t)7;
            fn.value_slot[value] = fn.frame;
            fn.frame += type_size(lower, values[value]);
        } else if(type_at(lower, values[value])->kind != LirTypeKind_Void) {
            fn.value_local[value] = (int32_t)fn_local(&fn, valtype(lower, values[value]));
        }
        if(fn.frame > ZXW_STACK_SIZE / 2) {
            WasmGap(lower, "wasm.gap.frame", "procedure %.*s needs a frame over %d bytes",
                (int)function->name.length, function->name.data, ZXW_STACK_SIZE / 2);
            goto done;
        }
    }
    int64_t at = 0;
    for(int64_t block = 0; block < fn.block_count; block++) {
        const LirInstruction *instructions = instructions_of(&blocks[block]);
        int64_t scratch = 0;
        const int32_t *block_parameters = blocks[block].parameters.data;
        for(int64_t parameter = 0; parameter < blocks[block].parameters.length; parameter++)
            if(value_valid(&fn, block_parameters[parameter]) && !value_scalar(&fn, block_parameters[parameter]))
                scratch += type_size(lower, value_type(&fn, block_parameters[parameter]));
        if(scratch > fn.scratch) fn.scratch = scratch;
        for(int64_t item = 0; item < blocks[block].instructions.length; item++, at++) {
            const LirInstruction *instruction = &instructions[item];
            fn.alloca_slot[at] = -1;
            if(instruction->op == LirOp_Allocate) {
                int64_t size = type_size(lower, value_lir(&fn, instruction->result)->element);
                if(size < 0) { WasmGap(lower, "wasm.gap.allocate", "allocation of an unbounded value"); goto done; }
                fn.frame = (fn.frame + 7) & ~(int64_t)7;
                fn.alloca_slot[at] = fn.frame;
                fn.frame += size;
            }
            const LirEdge *edges = instruction->edges.data;
            for(int64_t edge = 0; edge < instruction->edges.length; edge++)
                if(edges[edge].block <= block || function->entry != 0)
                    fn.dispatch = 1;
        }
    }
    if(fn.scratch > 0) {
        fn.frame = (fn.frame + 7) & ~(int64_t)7;
        int64_t scratch = fn.scratch;
        fn.scratch = fn.frame;
        fn.frame += scratch;
    }
    fn.frame = (fn.frame + 15) & ~(int64_t)15;
    if(fn.frame > ZXW_STACK_SIZE / 2) {
        WasmGap(lower, "wasm.gap.frame", "procedure %.*s needs a frame over %d bytes",
            (int)function->name.length, function->name.data, ZXW_STACK_SIZE / 2);
        goto done;
    }
    fn.has_frame = fn.frame > 0;
    if(fn.has_frame) fn.fp = fn_local(&fn, W_I32);
    if(fn.dispatch) fn.pc = fn_local(&fn, W_I32);
    if(lower->failed) goto done;

    if(fn.has_frame) {
        uint32_t top = fn_temp(&fn, W_I32, 0);
        global_get(&fn, G_SP); local_set(&fn, fn.fp);
        local_get(&fn, fn.fp); i32_const(&fn, (int32_t)fn.frame); op(&fn, 0x6a);
        local_set(&fn, top);
        local_get(&fn, top); global_get(&fn, G_LIMIT); op(&fn, 0x4b);
        trap_if(&fn);
        local_get(&fn, top); global_set(&fn, G_SP);
    }
    {
        const LirBlock *entry = &blocks[function->entry];
        const int32_t *entry_parameters = entry->parameters.data;
        if(entry->parameters.length != function->parameters.length) {
            WasmGap(lower, "wasm.gap.control", "entry block parameters differ from the procedure");
            goto done;
        }
        for(int64_t parameter = 0; parameter < entry->parameters.length; parameter++) {
            int32_t value = entry_parameters[parameter];
            uint32_t local = first_parameter + (uint32_t)parameter;
            if(value_scalar(&fn, value)) {
                local_get(&fn, local);
                set(&fn, value);
            } else {
                int64_t size = type_size(lower, value_type(&fn, value));
                if(size > 0) {
                    push_full(&fn, addr_of_slot(&fn, value));
                    local_get(&fn, local);
                    i32_const(&fn, (int32_t)size);
                    memory_copy(&fn);
                }
            }
        }
    }
    if(fn.dispatch) {
        i32_const(&fn, function->entry);
        local_set(&fn, fn.pc);
        op(&fn, 0x03); op(&fn, 0x40);
    }
    for(int64_t block = fn.block_count - 1; block >= (fn.dispatch ? 0 : 1); block--) {
        op(&fn, 0x02); op(&fn, 0x40);
    }
    if(fn.dispatch) {
        local_get(&fn, fn.pc);
        op_u(&fn, 0x0e, (uint64_t)fn.block_count);
        for(int64_t block = 0; block < fn.block_count; block++)
            buf_uleb(&fn.code, (uint64_t)block);
        buf_uleb(&fn.code, (uint64_t)(fn.block_count - 1));
    }
    at = 0;
    for(int64_t block = 0; block < fn.block_count && !lower->failed; block++) {
        if(fn.dispatch || block > 0)
            op(&fn, 0x0b);
        const LirInstruction *instructions = instructions_of(&blocks[block]);
        for(int64_t item = 0; item < blocks[block].instructions.length && !lower->failed; item++, at++) {
            lower_instruction(&fn, block, &instructions[item], fn.alloca_slot[at]);
        }
    }
    if(fn.dispatch)
        op(&fn, 0x0b);
    op(&fn, 0x00);
    op(&fn, 0x0b);
    if(lower->failed) goto done;
    if(fn.code.failed) { DiagnosticOutOfMemory(); lower->failed = 1; goto done; }
    {
        Buf body = {0};
        uint32_t runs = 0;
        for(uint32_t local = fn.param_count; local < fn.local_count; ) {
            uint32_t next = local;
            while(next < fn.local_count && fn.locals[next] == fn.locals[local]) next++;
            runs++;
            local = next;
        }
        buf_uleb(&body, runs);
        for(uint32_t local = fn.param_count; local < fn.local_count; ) {
            uint32_t next = local;
            while(next < fn.local_count && fn.locals[next] == fn.locals[local]) next++;
            buf_uleb(&body, next - local);
            buf_byte(&body, fn.locals[local]);
            local = next;
        }
        buf_raw(&body, fn.code.data, fn.code.count);
        buf_uleb(output, body.count);
        buf_raw(output, body.data, body.count);
        int failed = body.failed;
        buf_free(&body);
        if(failed) { DiagnosticOutOfMemory(); lower->failed = 1; goto done; }
    }
    ok = 1;
done:
    buf_free(&fn.code);
    free(fn.locals);
    free(fn.value_local);
    free(fn.value_slot);
    free(fn.alloca_slot);
    return ok && !lower->failed;
}

/* --------------------------------------------------------------- module */

static void
section(Buf *module, unsigned id, Buf *content)
{
    buf_byte(module, id);
    buf_uleb(module, content->count);
    buf_raw(module, content->data, content->count);
    if(content->failed) module->failed = 1;
    content->count = 0;
}

static void
state_type_name(Lower *lower, int32_t type, char *text, size_t size)
{
    const LirType *value = type_at(lower, type);
    if(value->kind == LirTypeKind_Bool) snprintf(text, size, "bool");
    else if(value->kind == LirTypeKind_Integer) snprintf(text, size, "%c%d", value->is_signed ? 's' : 'u', value->bits);
    else if(value->kind == LirTypeKind_Float) snprintf(text, size, "float%d", value->bits);
    else snprintf(text, size, "lir.%s", kind_name(value->kind));
}

static int
scan_program(Lower *lower)
{
    for(int64_t index = 0; index < lower->function_count; index++) {
        const LirFunction *function = &lower->functions[index];
        const LirBlock *blocks = function->blocks.data;
        for(int64_t block = 0; block < function->blocks.length; block++) {
            const LirInstruction *instructions = blocks[block].instructions.data;
            for(int64_t item = 0; item < blocks[block].instructions.length; item++) {
                LirOp code = instructions[item].op;
                if(code == LirOp_HostCall) lower->uses_host = 1;
                if(code == LirOp_BytesEqual) lower->uses_bytes_equal = 1;
                if(code == LirOp_FunctionAddress || code == LirOp_CallIndirect) lower->uses_table = 1;
                if(code == LirOp_Copy || code == LirOp_Zero || code == LirOp_Invalid || code > LirOp_Unreachable)
                    return WasmGap(lower, "wasm.gap.instruction", "LIR %.*s instructions are not lowered to wasm32",
                               (int)compiler_lowered_ir_LirOpName(code).length, compiler_lowered_ir_LirOpName(code).data);
            }
        }
    }
    return 1;
}

static void
emit_bytes_equal(Lower *lower, Buf *output)
{
    Fn fn = {0};
    fn.lower = lower;
    /* (i32 a, i64 la, i32 b, i64 lb) -> i32, local 4 is the i64 cursor. */
    local_get(&fn, 1); local_get(&fn, 3); op(&fn, 0x52);
    op(&fn, 0x04); op(&fn, 0x40); i32_const(&fn, 0); op(&fn, 0x0f); op(&fn, 0x0b);
    op(&fn, 0x02); op(&fn, 0x40); op(&fn, 0x03); op(&fn, 0x40);
    local_get(&fn, 4); local_get(&fn, 1); op(&fn, 0x5a); op_u(&fn, 0x0d, 1);
    local_get(&fn, 0); local_get(&fn, 4); op(&fn, 0xa7); op(&fn, 0x6a); memarg(&fn, 0x2d, 0);
    local_get(&fn, 2); local_get(&fn, 4); op(&fn, 0xa7); op(&fn, 0x6a); memarg(&fn, 0x2d, 0);
    op(&fn, 0x47);
    op(&fn, 0x04); op(&fn, 0x40); i32_const(&fn, 0); op(&fn, 0x0f); op(&fn, 0x0b);
    local_get(&fn, 4); i64_const(&fn, 1); op(&fn, 0x7c); local_set(&fn, 4);
    op_u(&fn, 0x0c, 0);
    op(&fn, 0x0b); op(&fn, 0x0b);
    i32_const(&fn, 1);
    op(&fn, 0x0b);
    Buf body = {0};
    buf_uleb(&body, 1); buf_uleb(&body, 1); buf_byte(&body, W_I64);
    buf_raw(&body, fn.code.data, fn.code.count);
    buf_uleb(output, body.count);
    buf_raw(output, body.data, body.count);
    if(fn.code.failed || body.failed) output->failed = 1;
    buf_free(&body);
    buf_free(&fn.code);
}

static int
emit_entry(Lower *lower, Buf *output, uint32_t stack_base)
{
    const LirProgram *program = lower->program;
    if(program->entry < 0 || program->entry >= lower->function_count ||
       lower->functions[program->entry].external)
        return WasmGap(lower, "wasm.gap.entry", "the executable has no internal entry procedure");
    const LirFunction *entry = &lower->functions[program->entry];
    const LirType *result = type_at(lower, entry->result);
    if(entry->parameters.length != 0 ||
       (result->kind != LirTypeKind_Void && result->kind != LirTypeKind_Bool && result->kind != LirTypeKind_Integer))
        return WasmGap(lower, "wasm.gap.entry", "zx_entry needs a procedure with no parameters returning void, bool or an integer");
    Fn fn = {0};
    fn.lower = lower;
    i32_const(&fn, (int32_t)stack_base); global_set(&fn, G_SP);
    const int32_t *initializers = program->initializers.data;
    if(program->initializers.length > 0) {
        global_get(&fn, G_INIT); op(&fn, 0x45);
        op(&fn, 0x04); op(&fn, 0x40);
        for(int64_t index = 0; index < program->initializers.length; index++) {
            int32_t initializer = initializers[index];
            if(initializer < 0 || initializer >= lower->function_count || lower->functions[initializer].external ||
               lower->functions[initializer].parameters.length != 0 ||
               type_at(lower, lower->functions[initializer].result)->kind != LirTypeKind_Void) {
                buf_free(&fn.code);
                return WasmGap(lower, "wasm.gap.initializer", "module initializer @%d is not a () -> void procedure", initializer);
            }
            op_u(&fn, 0x10, lower->wasm_function[initializer]);
        }
        i32_const(&fn, 1); global_set(&fn, G_INIT);
        op(&fn, 0x0b);
    }
    op_u(&fn, 0x10, lower->wasm_function[program->entry]);
    if(result->kind == LirTypeKind_Void) i64_const(&fn, 0);
    else if(result->kind == LirTypeKind_Bool) op(&fn, 0xad);
    else if(result->bits < 64) {
        if(result->is_signed && result->bits == 8) op(&fn, 0xc0);
        if(result->is_signed && result->bits == 16) op(&fn, 0xc1);
        op(&fn, result->is_signed ? 0xac : 0xad);
    }
    op(&fn, 0x0b);
    buf_uleb(output, fn.code.count + 1);
    buf_uleb(output, 0);
    buf_raw(output, fn.code.data, fn.code.count);
    int failed = fn.code.failed;
    buf_free(&fn.code);
    return failed ? 0 : 1;
}

static int
lower_module(const LirProgram *program, ZxWasmImage *image)
{
    Lower lower = {0};
    Buf module = {0}, content = {0}, code = {0};
    int ok = 0;
    lower.program = program;
    lower.types = program->types.data;
    lower.type_count = program->types.length;
    lower.functions = program->functions.data;
    lower.function_count = program->functions.length;
    lower.constants = program->constants.data;
    lower.constant_count = program->constants.length;
    lower.globals = program->globals.data;
    lower.global_count = program->globals.length;
    lower.modules = program->modules.data;
    lower.module_count = program->modules.length;
    size_t types = (size_t)(lower.type_count ? lower.type_count : 1);
    size_t functions = (size_t)(lower.function_count ? lower.function_count : 1);
    size_t globals = (size_t)(lower.global_count ? lower.global_count : 1);
    lower.sizes = malloc(types * sizeof(*lower.sizes));
    lower.global_address = calloc(globals, sizeof(*lower.global_address));
    lower.host_kind = calloc(functions, 1);
    lower.host_index = calloc(functions, sizeof(*lower.host_index));
    lower.wasm_function = calloc(functions, sizeof(*lower.wasm_function));
    if(lower.sizes == NULL || lower.global_address == NULL || lower.host_kind == NULL ||
       lower.host_index == NULL || lower.wasm_function == NULL) {
        DiagnosticOutOfMemory();
        goto done;
    }
    for(size_t index = 0; index < types; index++)
        lower.sizes[index] = -1;
    if(!scan_program(&lower) || !classify_externals(&lower) || !layout_globals(&lower))
        goto done;

    /* Function index space: imports, LIR procedures, helpers, zx_entry. */
    lower.imports = lower.uses_host ? 2 : 1;
    uint32_t next = lower.imports;
    for(int64_t index = 0; index < lower.function_count; index++)
        if(!lower.functions[index].external)
            lower.wasm_function[index] = next++;
    if(lower.uses_bytes_equal) lower.bytes_equal_function = next++;
    if(lower.uses_table) lower.stub_function = next++;
    lower.entry_function = next++;

    Sig unit = {{0}, 0, 0};
    Sig host = {{W_I32, W_I64}, 2, W_I64};
    Sig entry = {{0}, 0, W_I64};
    Sig bytes = {{W_I32, W_I64, W_I32, W_I64}, 4, W_I32};
    uint32_t unit_type = signature_index(&lower, &unit);
    uint32_t host_type = lower.uses_host ? signature_index(&lower, &host) : 0;
    uint32_t *function_types = calloc(functions, sizeof(*function_types));
    if(function_types == NULL) { DiagnosticOutOfMemory(); goto done; }
    for(int64_t index = 0; index < lower.function_count && !lower.failed; index++) {
        const LirFunction *function = &lower.functions[index];
        if(function->external) continue;
        Sig sig;
        if(signature_of(&lower, function->parameters.data, function->parameters.length, function->result, &sig))
            function_types[index] = signature_index(&lower, &sig);
    }
    uint32_t bytes_type = lower.uses_bytes_equal ? signature_index(&lower, &bytes) : 0;
    uint32_t entry_type = signature_index(&lower, &entry);

    /* Code first: constant blobs extend the data region while lowering. */
    uint32_t defined = next - lower.imports;
    buf_uleb(&code, defined);
    for(int64_t index = 0; index < lower.function_count && !lower.failed; index++)
        if(!lower.functions[index].external && !lower_function(&lower, index, &code))
            break;
    if(lower.failed) { free(function_types); goto done; }
    if(lower.uses_bytes_equal) emit_bytes_equal(&lower, &code);
    if(lower.uses_table) { buf_uleb(&code, 3); buf_uleb(&code, 0); buf_byte(&code, 0x00); buf_byte(&code, 0x0b); }
    size_t data_end = ZXW_DATA_BASE + lower.data.count;
    uint32_t stack_base = (uint32_t)((data_end + 15) & ~(size_t)15);
    uint32_t stack_limit = stack_base + ZXW_STACK_SIZE;
    uint32_t arena = stack_limit;
    if(!emit_entry(&lower, &code, stack_base)) { free(function_types); goto done; }

    buf_raw(&module, "\0asm\1\0\0\0", 8);
    /* Type section. */
    buf_uleb(&content, lower.sig_count);
    for(size_t index = 0; index < lower.sig_count; index++) {
        const Sig *sig = &lower.sigs[index];
        buf_byte(&content, 0x60);
        buf_uleb(&content, sig->params);
        buf_raw(&content, sig->types, sig->params);
        buf_uleb(&content, sig->result ? 1 : 0);
        if(sig->result) buf_byte(&content, sig->result);
    }
    section(&module, 1, &content);
    /* Import section. */
    buf_uleb(&content, lower.uses_host ? 3 : 2);
    buf_name(&content, "zx", 2); buf_name(&content, "overrun", 7);
    buf_byte(&content, 0x00); buf_uleb(&content, unit_type);
    if(lower.uses_host) {
        buf_name(&content, "zx", 2); buf_name(&content, "host_i64", 8);
        buf_byte(&content, 0x00); buf_uleb(&content, host_type);
    }
    buf_name(&content, "zx", 2); buf_name(&content, "budget", 6);
    buf_byte(&content, 0x03); buf_byte(&content, W_I64); buf_byte(&content, 0x01);
    section(&module, 2, &content);
    /* Function section. */
    buf_uleb(&content, defined);
    for(int64_t index = 0; index < lower.function_count; index++)
        if(!lower.functions[index].external) buf_uleb(&content, function_types[index]);
    if(lower.uses_bytes_equal) buf_uleb(&content, bytes_type);
    if(lower.uses_table) buf_uleb(&content, unit_type);
    buf_uleb(&content, entry_type);
    section(&module, 3, &content);
    free(function_types);
    /* Table: slot 0 and external procedures trap; slot n + 1 is LIR @n. */
    if(lower.uses_table) {
        buf_uleb(&content, 1); buf_byte(&content, 0x70); buf_byte(&content, 0x01);
        buf_uleb(&content, (uint64_t)lower.function_count + 1);
        buf_uleb(&content, (uint64_t)lower.function_count + 1);
        section(&module, 4, &content);
    }
    /* Memory: data, a fixed stack, then the growable state arena. */
    buf_uleb(&content, 1); buf_byte(&content, 0x00);
    buf_uleb(&content, (arena + 65535u) / 65536u);
    section(&module, 5, &content);
    /* Globals: arena cursor, stack pointer, initializer flag, stack limit. */
    buf_uleb(&content, 4);
    buf_byte(&content, W_I32); buf_byte(&content, 0x01); buf_byte(&content, 0x41); buf_sleb(&content, (int32_t)arena); buf_byte(&content, 0x0b);
    buf_byte(&content, W_I32); buf_byte(&content, 0x01); buf_byte(&content, 0x41); buf_sleb(&content, (int32_t)stack_base); buf_byte(&content, 0x0b);
    buf_byte(&content, W_I32); buf_byte(&content, 0x01); buf_byte(&content, 0x41); buf_sleb(&content, 0); buf_byte(&content, 0x0b);
    buf_byte(&content, W_I32); buf_byte(&content, 0x00); buf_byte(&content, 0x41); buf_sleb(&content, (int32_t)stack_limit); buf_byte(&content, 0x0b);
    section(&module, 6, &content);
    /* Exports. */
    buf_uleb(&content, 3);
    buf_name(&content, "zx_entry", 8); buf_byte(&content, 0x00); buf_uleb(&content, lower.entry_function);
    buf_name(&content, "memory", 6); buf_byte(&content, 0x02); buf_uleb(&content, 0);
    buf_name(&content, "zx_state_arena", 14); buf_byte(&content, 0x03); buf_uleb(&content, G_ARENA);
    section(&module, 7, &content);
    if(lower.uses_table) {
        buf_uleb(&content, 1); buf_uleb(&content, 0);
        buf_byte(&content, 0x41); buf_sleb(&content, 0); buf_byte(&content, 0x0b);
        buf_uleb(&content, (uint64_t)lower.function_count + 1);
        buf_uleb(&content, lower.stub_function);
        for(int64_t index = 0; index < lower.function_count; index++)
            buf_uleb(&content, lower.functions[index].external ? lower.stub_function : lower.wasm_function[index]);
        section(&module, 9, &content);
    }
    section(&module, 10, &code);
    /* Data: one segment at the data base, trailing zero bytes omitted. */
    size_t used = lower.data.count;
    while(used > 0 && lower.data.data[used - 1] == 0) used--;
    if(used > 0) {
        buf_uleb(&content, 1); buf_uleb(&content, 0);
        buf_byte(&content, 0x41); buf_sleb(&content, ZXW_DATA_BASE); buf_byte(&content, 0x0b);
        buf_uleb(&content, used);
        buf_raw(&content, lower.data.data, used);
        section(&module, 11, &content);
    }
    /* zxstate: writable globals for portable state migration. */
    {
        int64_t count = 0;
        for(int64_t index = 0; index < lower.global_count; index++)
            if(lower.globals[index].writable) count++;
        buf_name(&content, "zxstate", 7);
        buf_uleb(&content, (uint64_t)count);
        for(int64_t index = 0; index < lower.global_count; index++) {
            const LirGlobal *global = &lower.globals[index];
            if(!global->writable) continue;
            String owner = global->module >= 0 && global->module < lower.module_count ?
                lower.modules[global->module] : (String){"", 0};
            char type[64];
            state_type_name(&lower, global->type, type, sizeof(type));
            buf_name(&content, owner.data, owner.length);
            buf_name(&content, global->name.data, global->name.length);
            buf_name(&content, type, strlen(type));
            buf_uleb(&content, lower.global_address[index] - ZXW_DATA_BASE);
            buf_uleb(&content, (uint64_t)type_size(&lower, global->type));
        }
        section(&module, 0, &content);
    }
    if(module.failed || content.failed || code.failed) {
        DiagnosticOutOfMemory();
        goto done;
    }
    image->bytes = module.data;
    image->size = module.count;
    image->capabilities = lower.capabilities;
    image->capability_count = lower.capability_count;
    module.data = NULL;
    lower.capabilities = NULL;
    lower.capability_count = 0;
    ok = 1;
done:
    for(size_t index = 0; index < lower.capability_count; index++) {
        free(lower.capabilities[index].module);
        free(lower.capabilities[index].function);
    }
    free(lower.capabilities);
    free(lower.sizes);
    free(lower.global_address);
    free(lower.host_kind);
    free(lower.host_index);
    free(lower.wasm_function);
    free(lower.sigs);
    buf_free(&lower.data);
    buf_free(&module);
    buf_free(&content);
    buf_free(&code);
    return ok;
}

void
ZxWasmImageFree(ZxWasmImage *image)
{
    if(image == NULL)
        return;
    for(size_t index = 0; index < image->capability_count; index++) {
        free(image->capabilities[index].module);
        free(image->capabilities[index].function);
    }
    free(image->capabilities);
    free(image->bytes);
    memset(image, 0, sizeof(*image));
}

int
ZxWasmLowerExecutable(const unsigned char *zx, size_t size, ZxWasmImage *image)
{
    if(image == NULL)
        return 0;
    memset(image, 0, sizeof(*image));
    if(zx == NULL || size == 0 || size > ZXW_IMAGE_LIMIT) {
        Diagnostic((ZirSourceSpan){0}, "wasm.container", "executable ZX input is empty or larger than the wasm32 image limit");
        return 0;
    }
    ZxExecOwner owner = {0};
    ZxExecResult read = compiler_exec_link_ZxExecRead(&owner, (String){(const char *)zx, size},
                                                      compiler_lowered_ir_LirDefaultLimits());
    if(read.problem != ZxExecProblem_None) {
        String problem = compiler_exec_link_ZxExecProblemName(read.problem);
        String container = compiler_bundle_ExecutableMessage(read.container);
        Diagnostic((ZirSourceSpan){0}, "wasm.container",
                   "executable ZX rejected: problem=%.*s container=%.*s codec=%d offset=%lld verifier=%d",
                   (int)problem.length, problem.data, (int)container.length, container.data,
                   read.codec, (long long)read.offset, read.diagnostic.problem);
        compiler_exec_link_ZxExecReadFree(&owner);
        return 0;
    }
    LirProgram program = compiler_exec_link_ZxExecReadProgram(&owner);
    LirDiagnostic diagnostic = compiler_lowered_verify_LirVerify(program, compiler_lowered_ir_LirDefaultLimits());
    int ok = 0;
    if(diagnostic.problem != LirProblem_None)
        Diagnostic((ZirSourceSpan){0}, "wasm.verify",
                   "lowered IR rejected: problem=%d function=%d block=%d instruction=%d value=%d origin=%d",
                   diagnostic.problem, diagnostic.function, diagnostic.block, diagnostic.instruction,
                   diagnostic.value, diagnostic.origin);
    else
        ok = lower_module(&program, image);
    compiler_exec_link_ZxExecReadFree(&owner);
    return ok;
}
