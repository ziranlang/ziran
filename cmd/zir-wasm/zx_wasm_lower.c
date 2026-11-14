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
 *
 * Memory: static data, a fixed stack, then the heap. Heap blocks have a
 * 16-byte header (capacity, state, element count) and power-of-two
 * capacities from 16 bytes; a freed block joins the free list of its
 * capacity, and the block that ends the heap gives its bytes back.
 */
#include "zx_wasm_lower.h"
#include "zir_diagnostic.h"

#ifndef ZIRAN_NO_ENGINE

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
    ZXW_IMAGE_LIMIT = 1 << 28,
    /* The largest heap block and host image. */
    ZXW_BLOCK_LIMIT = 1 << 30,
    /* Free lists by capacity 16 << class; a block is at most 1 GiB. */
    ZXW_CLASSES = 27
};

/* Global indices: the imported budget, the stack pointer, the initializer
 * flag and the stack limit (which is also the heap base); with a heap the
 * heap top; with host images the image base, cursor and length. */
enum { G_BUDGET, G_SP, G_INIT, G_LIMIT, G_TOP, G_BASE, G_CURSOR, G_LENGTH };

/* Heap block states. */
enum { BLOCK_FREE, BLOCK_PROGRAM, BLOCK_HOST };

enum {
    HOST_NONE, HOST_WRITE, HOST_SIGNED, HOST_UNSIGNED, HOST_BOOL,
    HOST_FLOAT32, HOST_FLOAT64, HOST_CAPABILITY
};

/* Host shape flags of a type, computed once. */
enum {
    SHAPE_VALID = 1, SHAPE_VIEWS = 2, SHAPE_BOOLS = 4,
    SHAPE_BUSY = 0x40, SHAPE_DONE = 0x80
};
/* Image helpers a type needs: measure and encode, or decode. */
enum { NEED_ENCODE = 1, NEED_DECODE = 2 };

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
    unsigned char *shapes;
    unsigned char *needs;
    uint32_t *measure_function;
    uint32_t *encode_function;
    uint32_t *decode_function;
    int uses_print;
    int uses_capability;
    int uses_images;
    int uses_heap;
    int uses_bytes_equal;
    int uses_table;
    uint32_t imports;
    uint32_t print_import;
    uint32_t host_import;
    uint32_t free_table;
    uint32_t alloc_function;
    uint32_t free_function;
    uint32_t resize_function;
    uint32_t reserve_function;
    uint32_t bytes_equal_function;
    uint32_t stub_function;
    uint32_t initialize_function;
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

/* A view is the record {data: *T, count: s64} of a non-void element, as
 * LirHostView defines it: string, []T. Gives the element type or -1. */
static int32_t
view_element(Lower *lower, int32_t type)
{
    const LirType *value = type_at(lower, type);
    if(value->kind != LirTypeKind_Record || value->fields.length != 2)
        return -1;
    const int32_t *fields = value->fields.data;
    const LirType *pointer = type_at(lower, fields[0]);
    const LirType *count = type_at(lower, fields[1]);
    if(pointer->kind != LirTypeKind_Pointer || !type_valid(lower, pointer->element) ||
       type_at(lower, pointer->element)->kind == LirTypeKind_Void ||
       count->kind != LirTypeKind_Integer || count->bits != 64 || !count->is_signed)
        return -1;
    return pointer->element;
}

/* Host shape flags, by the rules of LirHostShapes: scalars, pointers as
 * handles, and records, arrays and views of host shapes. A type that
 * reaches itself or nests past 64 levels is not a shape. */
static unsigned
host_shape_at(Lower *lower, int32_t type, int depth)
{
    if(!type_valid(lower, type) || depth > 64)
        return 0;
    unsigned char *shape = &lower->shapes[type];
    if(*shape & SHAPE_DONE)
        return *shape & 7;
    if(*shape & SHAPE_BUSY)
        return 0;
    *shape |= SHAPE_BUSY;
    const LirType *value = &lower->types[type];
    unsigned flags = 0;
    int32_t element = view_element(lower, type);
    if(value->kind == LirTypeKind_Bool)
        flags = SHAPE_VALID | SHAPE_BOOLS;
    else if(value->kind == LirTypeKind_Integer || value->kind == LirTypeKind_Float ||
            value->kind == LirTypeKind_Pointer)
        flags = SHAPE_VALID;
    else if(element >= 0) {
        unsigned inner = host_shape_at(lower, element, depth + 1);
        if(inner & SHAPE_VALID)
            flags = SHAPE_VALID | SHAPE_VIEWS | (inner & SHAPE_BOOLS);
    } else if(value->kind == LirTypeKind_Array) {
        flags = host_shape_at(lower, value->element, depth + 1);
    } else if(value->kind == LirTypeKind_Record) {
        const int32_t *fields = value->fields.data;
        flags = SHAPE_VALID;
        for(int64_t index = 0; index < value->fields.length; index++) {
            unsigned field = host_shape_at(lower, fields[index], depth + 1);
            if(!(field & SHAPE_VALID)) { flags = 0; break; }
            flags |= field;
        }
    }
    *shape = (unsigned char)(SHAPE_DONE | flags);
    return flags;
}

static unsigned
host_shape(Lower *lower, int32_t type)
{
    return host_shape_at(lower, type, 0);
}

/* Scalars and pointers cross as bits; other shapes cross as images. */
static int
host_bits(Lower *lower, int32_t type)
{
    LirTypeKind kind = type_at(lower, type)->kind;
    return kind == LirTypeKind_Bool || kind == LirTypeKind_Integer ||
           kind == LirTypeKind_Float || kind == LirTypeKind_Pointer;
}

/* Marks the image helpers a type and everything inside it needs. */
static void
mark_needs(Lower *lower, int32_t type, unsigned need)
{
    unsigned shape = host_shape(lower, type);
    if(need == NEED_ENCODE && !(shape & SHAPE_VIEWS)) return;
    if(need == NEED_DECODE && !(shape & (SHAPE_VIEWS | SHAPE_BOOLS))) return;
    if(lower->needs[type] & need) return;
    lower->needs[type] |= (unsigned char)need;
    const LirType *value = type_at(lower, type);
    int32_t element = view_element(lower, type);
    if(element >= 0)
        mark_needs(lower, element, need);
    else if(value->kind == LirTypeKind_Array)
        mark_needs(lower, value->element, need);
    else if(value->kind == LirTypeKind_Record) {
        const int32_t *fields = value->fields.data;
        for(int64_t index = 0; index < value->fields.length; index++)
            mark_needs(lower, fields[index], need);
    }
}

/* How one capability value crosses, or 0 after a gap. */
static int
capability_value(Lower *lower, const LirFunction *function, int32_t type, int result, ZxWasmValue *value)
{
    memset(value, 0, sizeof(*value));
    value->type = type;
    if(result && type_at(lower, type)->kind == LirTypeKind_Void) {
        value->kind = ZX_WASM_VOID;
        return 1;
    }
    if(!(host_shape(lower, type) & SHAPE_VALID) || !type_lowered(lower, type))
        return WasmGap(lower, "wasm.gap.host-capability-abi",
                   "host capability %.*s has a %s %s that cannot cross a typed host boundary",
                   (int)function->name.length, function->name.data,
                   kind_name(type_at(lower, type)->kind), result ? "result" : "parameter");
    value->kind = host_bits(lower, type) ? ZX_WASM_BITS : ZX_WASM_IMAGE;
    int32_t element = view_element(lower, type);
    if(element >= 0 && !(host_shape(lower, element) & SHAPE_VIEWS) && type_size(lower, element) > 0)
        value->element_size = (size_t)type_size(lower, element);
    if(value->kind == ZX_WASM_IMAGE) {
        lower->uses_images = 1;
        mark_needs(lower, type, result ? NEED_DECODE : NEED_ENCODE);
    }
    return 1;
}

static int
add_capability(Lower *lower, int64_t index)
{
    const LirFunction *function = &lower->functions[index];
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
    ZxWasmCapability *capability = &capabilities[lower->capability_count++];
    memset(capability, 0, sizeof(*capability));
    capability->symbol = (int32_t)index;
    capability->parameter_count = (size_t)function->parameters.length;
    capability->module = text_copy(module);
    capability->function = text_copy(function->name);
    capability->parameters = calloc(capability->parameter_count + 1, sizeof(*capability->parameters));
    if(capability->module == NULL || capability->function == NULL || capability->parameters == NULL) {
        lower->failed = 1;
        DiagnosticOutOfMemory();
        return 0;
    }
    const int32_t *parameters = function->parameters.data;
    for(size_t at = 0; at < capability->parameter_count; at++)
        if(!capability_value(lower, function, parameters[at], 0, &capability->parameters[at]))
            return 0;
    if(!capability_value(lower, function, function->result, 1, &capability->result))
        return 0;
    lower->host_index[index] = (uint32_t)(lower->capability_count - 1);
    return 1;
}

/* Classify every external procedure once: platform output services map to
 * the reserved print indices, every other host procedure is a declared
 * capability with its own index and typed signature. */
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
        lower->host_kind[index] = kind;
        if(kind == HOST_CAPABILITY) {
            if(!add_capability(lower, index))
                return 0;
            continue;
        }
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
        if(!valid || type_at(lower, function->result)->kind != LirTypeKind_Void)
            return WasmGap(lower, "wasm.gap.platform-service-abi",
                       "platform service %.*s has an unexpected LIR signature",
                       (int)name.length, name.data);
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
    /* Frame offset of the host call record. */
    int64_t host_area;
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
static void if_begin(Fn *fn) { op(fn, 0x04); op(fn, 0x40); }
static void block_begin(Fn *fn) { op(fn, 0x02); op(fn, 0x40); }
static void loop_begin(Fn *fn) { op(fn, 0x03); op(fn, 0x40); }
static void end(Fn *fn) { op(fn, 0x0b); }
static void call(Fn *fn, uint32_t function) { op_u(fn, 0x10, function); }
static void return_zero(Fn *fn) { i32_const(fn, 0); op(fn, 0x0f); }

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

/* Pushes the header address of the heap block whose payload is the i32
 * local `address`. */
static void
block_header(Fn *fn, uint32_t address)
{
    local_get(fn, address); i32_const(fn, 16); op(fn, 0x6b);
}

/* Traps unless `address` (an i32 local) is the payload of a heap block in
 * `state`. */
static void
check_block(Fn *fn, uint32_t address, unsigned state)
{
    local_get(fn, address); global_get(fn, G_LIMIT); i32_const(fn, 16); op(fn, 0x6a); op(fn, 0x49);
    local_get(fn, address); global_get(fn, G_TOP); op(fn, 0x4f); op(fn, 0x72);
    local_get(fn, address); i32_const(fn, 15); op(fn, 0x71); op(fn, 0x72);
    trap_if(fn);
    block_header(fn, address); memarg(fn, 0x28, 4); i32_const(fn, (int32_t)state); op(fn, 0x47);
    trap_if(fn);
}

/* Pushes a bits value as its canonical u64: zero-extended integers, 0/1
 * bools, IEEE patterns and pointer handles. */
static void
push_bits(Fn *fn, int32_t value)
{
    const LirType *type = value_lir(fn, value);
    unsigned wasm = value_wasm(fn, value);
    get(fn, value);
    if(type->kind == LirTypeKind_Float && wasm == W_F32) { op(fn, 0xbc); op(fn, 0xad); }
    else if(type->kind == LirTypeKind_Float) op(fn, 0xbd);
    else if(wasm == W_I32) op(fn, 0xad);
}

/* A typed host capability call; see the module ABI in zx_wasm_lower.h. */
static void
lower_capability_call(Fn *fn, const LirInstruction *instruction)
{
    Lower *lower = fn->lower;
    const ZxWasmCapability *capability = &lower->capabilities[lower->host_index[instruction->symbol]];
    const int32_t *operands = instruction->operands.data;
    uint32_t count = (uint32_t)capability->parameter_count;
    int64_t base = fn->host_area;
    uint32_t total = fn_temp(fn, W_I64, 0);
    if(base < 0 || !offset_fits(fn, base + 16 * (int64_t)count + 16))
        return;
    for(uint32_t index = 0; index < count && !lower->failed; index++) {
        const ZxWasmValue *value = &capability->parameters[index];
        uint32_t at = (uint32_t)base + 16 * index;
        if(value->kind == ZX_WASM_BITS) {
            local_get(fn, fn->fp); push_bits(fn, operands[index]); memarg(fn, 0x37, at);
            local_get(fn, fn->fp); i64_const(fn, 0); memarg(fn, 0x37, at + 8);
            continue;
        }
        /* An image in a heap block of its own: the value's bytes, then the
         * elements of its views. */
        int64_t size = type_size(lower, value->type);
        int views = (host_shape(lower, value->type) & SHAPE_VIEWS) != 0;
        uint32_t block = fn_temp(fn, W_I32, 2 + index);
        Addr source = addr_of_slot(fn, operands[index]);
        i64_const(fn, size);
        if(views) { push_full(fn, source); call(fn, lower->measure_function[value->type]); op(fn, 0x7c); }
        local_set(fn, total);
        local_get(fn, total); i64_const(fn, ZXW_BLOCK_LIMIT); op(fn, 0x56); trap_if(fn);
        local_get(fn, total); op(fn, 0xa7); i32_const(fn, BLOCK_PROGRAM); call(fn, lower->alloc_function);
        local_set(fn, block);
        local_get(fn, block); op(fn, 0x45); trap_if(fn);
        if(size > 0) {
            local_get(fn, block); push_full(fn, source); i32_const(fn, (int32_t)size); memory_copy(fn);
        }
        if(views) {
            local_get(fn, block); global_set(fn, G_BASE);
            local_get(fn, block); i32_const(fn, (int32_t)size); op(fn, 0x6a); global_set(fn, G_CURSOR);
            push_full(fn, source); local_get(fn, block); call(fn, lower->encode_function[value->type]);
        }
        local_get(fn, fn->fp); local_get(fn, block); memarg(fn, 0x36, at);
        local_get(fn, fn->fp);
        if(view_element(lower, value->type) >= 0) { push_base(fn, source); memarg(fn, 0x29, (uint32_t)source.offset); op(fn, 0xa7); }
        else i32_const(fn, 0);
        memarg(fn, 0x36, at + 4);
        local_get(fn, fn->fp); local_get(fn, total); op(fn, 0xa7); memarg(fn, 0x36, at + 8);
        local_get(fn, fn->fp); i32_const(fn, 0); memarg(fn, 0x36, at + 12);
    }
    uint32_t result = (uint32_t)base + 16 * count;
    local_get(fn, fn->fp); i64_const(fn, 0); memarg(fn, 0x37, result);
    local_get(fn, fn->fp); i64_const(fn, 0); memarg(fn, 0x37, result + 8);
    i32_const(fn, (int32_t)lower->host_index[instruction->symbol]);
    local_get(fn, fn->fp); i32_const(fn, (int32_t)base); op(fn, 0x6a);
    call(fn, lower->host_import);
    op(fn, 0x45); trap_if(fn);
    /* A host that changed a []T argument's elements gives them back in
     * place; the module copies them into the slice. Nothing else may
     * change. */
    for(uint32_t index = 0; index < count && !lower->failed; index++) {
        const ZxWasmValue *value = &capability->parameters[index];
        if(value->kind != ZX_WASM_IMAGE)
            continue;
        uint32_t at = (uint32_t)base + 16 * index;
        uint32_t block = fn_temp(fn, W_I32, 2 + index);
        int64_t width = (int64_t)value->element_size;
        local_get(fn, fn->fp); memarg(fn, 0x28, at + 12);
        if(width <= 0) {
            trap_if(fn);
        } else {
            Addr slice = addr_of_slot(fn, operands[index]);
            if_begin(fn);
            /* The same element count, inside the image. */
            local_get(fn, block); memarg(fn, 0x29, 8);
            push_base(fn, slice); memarg(fn, 0x29, (uint32_t)slice.offset + 8);
            op(fn, 0x52); trap_if(fn);
            local_get(fn, block); memarg(fn, 0x29, 0);
            push_base(fn, slice); memarg(fn, 0x29, (uint32_t)slice.offset + 8);
            i64_const(fn, width); op(fn, 0x7e); op(fn, 0x7c);
            local_get(fn, fn->fp); memarg(fn, 0x35, at + 8);
            op(fn, 0x56); trap_if(fn);
            push_base(fn, slice); memarg(fn, 0x29, (uint32_t)slice.offset); op(fn, 0xa7);
            local_get(fn, block); local_get(fn, block); memarg(fn, 0x29, 0); op(fn, 0xa7); op(fn, 0x6a);
            push_base(fn, slice); memarg(fn, 0x29, (uint32_t)slice.offset + 8);
            i64_const(fn, width); op(fn, 0x7e); op(fn, 0xa7);
            memory_copy(fn);
            end(fn);
        }
        local_get(fn, block); call(fn, lower->free_function);
    }
    const ZxWasmValue *returned = &capability->result;
    if(returned->kind == ZX_WASM_VOID) {
        local_get(fn, fn->fp); memarg(fn, 0x28, result + 8); trap_if(fn);
        return;
    }
    if(returned->kind == ZX_WASM_BITS) {
        const LirType *type = type_at(lower, returned->type);
        local_get(fn, fn->fp); memarg(fn, 0x29, result); local_set(fn, total);
        if(type->kind == LirTypeKind_Integer && type->bits < 64) {
            local_get(fn, total); i64_const(fn, (int64_t)~integer_mask(type->bits)); op(fn, 0x83);
            op(fn, 0x50); op(fn, 0x45); trap_if(fn);
        } else if(type->kind == LirTypeKind_Bool) {
            local_get(fn, total); i64_const(fn, 1); op(fn, 0x56); trap_if(fn);
        } else if(type->kind == LirTypeKind_Pointer || (type->kind == LirTypeKind_Float && type->bits == 32)) {
            local_get(fn, total); i64_const(fn, 0xffffffffLL); op(fn, 0x56); trap_if(fn);
        }
        if(instruction->result == LirNoId)
            return;
        unsigned wasm = valtype(lower, returned->type);
        local_get(fn, total);
        if(wasm == W_I32) op(fn, 0xa7);
        else if(wasm == W_F32) { op(fn, 0xa7); op(fn, 0xbe); }
        else if(wasm == W_F64) op(fn, 0xbf);
        set(fn, instruction->result);
        return;
    }
    /* An image result: the host placed it in a block from zx_reserve. Its
     * views become pointers into the block, which the module then keeps;
     * a value without views is copied out and the block freed. */
    int64_t size = type_size(lower, returned->type);
    unsigned shape = host_shape(lower, returned->type);
    uint32_t address = fn_temp(fn, W_I32, 0), length = fn_temp(fn, W_I32, 1);
    local_get(fn, fn->fp); memarg(fn, 0x29, result); local_set(fn, total);
    local_get(fn, total); i64_const(fn, 0xffffffffLL); op(fn, 0x56); trap_if(fn);
    local_get(fn, total); op(fn, 0xa7); local_set(fn, address);
    local_get(fn, fn->fp); memarg(fn, 0x28, result + 8); local_set(fn, length);
    local_get(fn, length); i32_const(fn, (int32_t)size); op(fn, (shape & SHAPE_VIEWS) ? 0x49 : 0x47); trap_if(fn);
    local_get(fn, address);
    if_begin(fn);
    check_block(fn, address, BLOCK_HOST);
    block_header(fn, address); memarg(fn, 0x28, 0); local_get(fn, length); op(fn, 0x49); trap_if(fn);
    op(fn, 0x05);
    local_get(fn, length); trap_if(fn);
    end(fn);
    if(shape & (SHAPE_VIEWS | SHAPE_BOOLS)) {
        local_get(fn, address); global_set(fn, G_BASE);
        local_get(fn, length); global_set(fn, G_LENGTH);
        local_get(fn, address); call(fn, lower->decode_function[returned->type]);
    }
    if(size > 0 && instruction->result != LirNoId) {
        push_full(fn, addr_of_slot(fn, instruction->result)); local_get(fn, address);
        i32_const(fn, (int32_t)size); memory_copy(fn);
    }
    if(!(shape & SHAPE_VIEWS)) {
        local_get(fn, address);
        if_begin(fn); local_get(fn, address); call(fn, lower->free_function); end(fn);
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
        lower_capability_call(fn, instruction);
        return;
    default:
        WasmGap(lower, "wasm.gap.host", "unclassified host procedure %.*s", (int)callee->name.length, callee->name.data);
        return;
    }
    call(fn, lower->print_import);
    op(fn, 0x1a);
}

/* resize(pointer, old_count, new_count) -> pointer through the heap. */
static void
lower_resize(Fn *fn, const LirInstruction *instruction)
{
    if(!operand_count(fn, instruction, 3, 3))
        return;
    const int32_t *operands = instruction->operands.data;
    const LirType *pointer = value_lir(fn, instruction->result);
    int64_t size = pointer->kind == LirTypeKind_Pointer ? type_size(fn->lower, pointer->element) : -1;
    if(size < 0 || size > ZXW_BLOCK_LIMIT || value_lir(fn, operands[1])->kind != LirTypeKind_Integer ||
       value_lir(fn, operands[2])->kind != LirTypeKind_Integer) {
        WasmGap(fn->lower, "wasm.gap.resize", "resize of an unbounded or non-pointer value");
        return;
    }
    get(fn, operands[0]);
    get_i64(fn, operands[1], 0);
    get_i64(fn, operands[2], 0);
    i32_const(fn, (int32_t)size);
    call(fn, fn->lower->resize_function);
    set(fn, instruction->result);
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
    case LirOp_Resize: lower_resize(fn, instruction); return;
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

/* Writes one function body: run-length locals after the parameters, then
 * the code, which already ends. */
static int
fn_body(Fn *fn, Buf *output)
{
    Buf body = {0};
    uint32_t runs = 0;
    for(uint32_t local = fn->param_count; local < fn->local_count; ) {
        uint32_t next = local;
        while(next < fn->local_count && fn->locals[next] == fn->locals[local]) next++;
        runs++;
        local = next;
    }
    buf_uleb(&body, runs);
    for(uint32_t local = fn->param_count; local < fn->local_count; ) {
        uint32_t next = local;
        while(next < fn->local_count && fn->locals[next] == fn->locals[local]) next++;
        buf_uleb(&body, next - local);
        buf_byte(&body, fn->locals[local]);
        local = next;
    }
    buf_raw(&body, fn->code.data, fn->code.count);
    buf_uleb(output, body.count);
    buf_raw(output, body.data, body.count);
    int failed = body.failed || fn->code.failed;
    buf_free(&body);
    if(failed) {
        DiagnosticOutOfMemory();
        fn->lower->failed = 1;
        return 0;
    }
    return 1;
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
    int64_t host_area = 0;
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
            if(instruction->op == LirOp_HostCall && instruction->symbol >= 0 &&
               instruction->symbol < lower->function_count &&
               lower->host_kind[instruction->symbol] == HOST_CAPABILITY &&
               16 * (instruction->operands.length + 1) > host_area)
                host_area = 16 * (instruction->operands.length + 1);
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
    fn.host_area = -1;
    if(host_area > 0) {
        fn.frame = (fn.frame + 7) & ~(int64_t)7;
        fn.host_area = fn.frame;
        fn.frame += host_area;
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
    if(!fn_body(&fn, output)) goto done;
    ok = 1;
done:
    buf_free(&fn.code);
    free(fn.locals);
    free(fn.value_local);
    free(fn.value_slot);
    free(fn.alloca_slot);
    return ok && !lower->failed;
}

/* ------------------------------------------------------- heap and images */

static void
helper_start(Fn *fn, Lower *lower, const unsigned char *parameters, uint32_t count)
{
    memset(fn, 0, sizeof(*fn));
    fn->lower = lower;
    for(uint32_t at = 0; at < count; at++)
        fn_local(fn, parameters[at]);
    fn->param_count = count;
}

static void
helper_finish(Fn *fn, Buf *output)
{
    end(fn);
    fn_body(fn, output);
    buf_free(&fn->code);
    free(fn->locals);
}

static void
memory_size_bytes(Fn *fn)
{
    op(fn, 0x3f); buf_byte(&fn->code, 0); i32_const(fn, 16); op(fn, 0x74);
}

/* Grows memory to hold `end` (an i32 local) bytes, or returns 0 from the
 * helper when it cannot. */
static void
emit_room(Fn *fn, uint32_t end_local)
{
    local_get(fn, end_local); memory_size_bytes(fn); op(fn, 0x4b);
    if_begin(fn);
    local_get(fn, end_local); memory_size_bytes(fn); op(fn, 0x6b);
    i32_const(fn, 65535); op(fn, 0x6a); i32_const(fn, 16); op(fn, 0x76);
    op(fn, 0x40); buf_byte(&fn->code, 0);
    i32_const(fn, -1); op(fn, 0x46);
    if_begin(fn); return_zero(fn); end(fn);
    end(fn);
}

/* Pushes 28 - clz(bytes - 1): the class of a block over 16 bytes. */
static void
size_class(Fn *fn, uint32_t bytes)
{
    i32_const(fn, 28); local_get(fn, bytes); i32_const(fn, 1); op(fn, 0x6b); op(fn, 0x67); op(fn, 0x6b);
}

/* alloc(bytes, state) -> payload: a zero-filled block from the free list
 * of its class or from the heap top; 0 when memory cannot grow. */
static void
emit_alloc(Lower *lower, Buf *output)
{
    static const unsigned char parameters[] = {W_I32, W_I32};
    Fn fn;
    helper_start(&fn, lower, parameters, 2);
    uint32_t class = fn_local(&fn, W_I32), payload = fn_local(&fn, W_I32);
    uint32_t header = fn_local(&fn, W_I32), limit = fn_local(&fn, W_I32);
    uint32_t table = lower->free_table;
    i32_const(&fn, 0);
    size_class(&fn, 0);
    local_get(&fn, 0); i32_const(&fn, 16); op(&fn, 0x4d);
    op(&fn, 0x1b);
    local_set(&fn, class);
    local_get(&fn, class); i32_const(&fn, 2); op(&fn, 0x74); memarg(&fn, 0x28, table);
    op_u(&fn, 0x22, payload);
    op(&fn, 0x04); op(&fn, 0x40);
    local_get(&fn, class); i32_const(&fn, 2); op(&fn, 0x74);
    local_get(&fn, payload); memarg(&fn, 0x28, 0);
    memarg(&fn, 0x36, table);
    local_get(&fn, payload); i32_const(&fn, 16); op(&fn, 0x6b); local_set(&fn, header);
    op(&fn, 0x05);
    global_get(&fn, G_TOP); op_u(&fn, 0x22, header);
    i32_const(&fn, 16); op(&fn, 0x6a);
    i32_const(&fn, 16); local_get(&fn, class); op(&fn, 0x74); op(&fn, 0x6a);
    op_u(&fn, 0x22, limit);
    i32_const(&fn, INT32_MIN); op(&fn, 0x4b);
    if_begin(&fn); return_zero(&fn); end(&fn);
    emit_room(&fn, limit);
    local_get(&fn, limit); global_set(&fn, G_TOP);
    local_get(&fn, header); i32_const(&fn, 16); local_get(&fn, class); op(&fn, 0x74); memarg(&fn, 0x36, 0);
    local_get(&fn, header); i32_const(&fn, 16); op(&fn, 0x6a); local_set(&fn, payload);
    end(&fn);
    local_get(&fn, header); local_get(&fn, 1); memarg(&fn, 0x36, 4);
    local_get(&fn, header); i64_const(&fn, 0); memarg(&fn, 0x37, 8);
    local_get(&fn, payload); i32_const(&fn, 0); local_get(&fn, 0); memory_fill(&fn);
    local_get(&fn, payload);
    helper_finish(&fn, output);
}

/* free(payload): the block that ends the heap gives its bytes back; any
 * other joins the free list of its class. */
static void
emit_free(Lower *lower, Buf *output)
{
    static const unsigned char parameters[] = {W_I32};
    Fn fn;
    helper_start(&fn, lower, parameters, 1);
    uint32_t header = fn_local(&fn, W_I32), capacity = fn_local(&fn, W_I32);
    uint32_t table = lower->free_table;
    local_get(&fn, 0); i32_const(&fn, 16); op(&fn, 0x6b); op_u(&fn, 0x22, header);
    memarg(&fn, 0x28, 0); local_set(&fn, capacity);
    local_get(&fn, header); i32_const(&fn, BLOCK_FREE); memarg(&fn, 0x36, 4);
    local_get(&fn, 0); local_get(&fn, capacity); op(&fn, 0x6a); global_get(&fn, G_TOP); op(&fn, 0x46);
    if_begin(&fn); local_get(&fn, header); global_set(&fn, G_TOP); op(&fn, 0x0f); end(&fn);
    local_get(&fn, 0);
    local_get(&fn, capacity); op(&fn, 0x68); i32_const(&fn, 4); op(&fn, 0x6b); i32_const(&fn, 2); op(&fn, 0x74);
    memarg(&fn, 0x28, table);
    memarg(&fn, 0x36, 0);
    local_get(&fn, capacity); op(&fn, 0x68); i32_const(&fn, 4); op(&fn, 0x6b); i32_const(&fn, 2); op(&fn, 0x74);
    local_get(&fn, 0);
    memarg(&fn, 0x36, table);
    helper_finish(&fn, output);
}

/* resize(pointer, old_count, new_count, element_size) -> pointer, the
 * interpreter's rules: null with count 0 allocates, count 0 frees, the
 * first min(old, new) elements stay and the rest is zero, and a request
 * memory cannot hold gives null with the old block intact. Only a live
 * program block's own pointer with its exact count may be resized. */
static void
emit_resize(Lower *lower, Buf *output)
{
    static const unsigned char parameters[] = {W_I32, W_I64, W_I64, W_I32};
    Fn fn;
    helper_start(&fn, lower, parameters, 4);
    uint32_t header = fn_local(&fn, W_I32), capacity = fn_local(&fn, W_I32);
    uint32_t bytes = fn_local(&fn, W_I32), kept = fn_local(&fn, W_I32);
    uint32_t moved = fn_local(&fn, W_I32), limit = fn_local(&fn, W_I32);
    local_get(&fn, 0);
    op(&fn, 0x04); op(&fn, 0x40);
    check_block(&fn, 0, BLOCK_PROGRAM);
    block_header(&fn, 0); local_set(&fn, header);
    local_get(&fn, header); memarg(&fn, 0x29, 8); local_get(&fn, 1); op(&fn, 0x52); trap_if(&fn);
    op(&fn, 0x05);
    local_get(&fn, 1); i64_const(&fn, 0); op(&fn, 0x52); trap_if(&fn);
    end(&fn);
    local_get(&fn, 2); op(&fn, 0x50);
    if_begin(&fn);
    local_get(&fn, 0); if_begin(&fn); local_get(&fn, 0); call(&fn, lower->free_function); end(&fn);
    return_zero(&fn);
    end(&fn);
    local_get(&fn, 3);
    if_begin(&fn);
    local_get(&fn, 2); i64_const(&fn, ZXW_BLOCK_LIMIT); local_get(&fn, 3); op(&fn, 0xad); op(&fn, 0x80); op(&fn, 0x56);
    if_begin(&fn); return_zero(&fn); end(&fn);
    end(&fn);
    local_get(&fn, 2); local_get(&fn, 3); op(&fn, 0xad); op(&fn, 0x7e); op(&fn, 0xa7); local_set(&fn, bytes);
    local_get(&fn, 1); local_get(&fn, 3); op(&fn, 0xad); op(&fn, 0x7e); op(&fn, 0xa7); local_set(&fn, kept);
    local_get(&fn, 0);
    if_begin(&fn);
    local_get(&fn, header); memarg(&fn, 0x28, 0); local_set(&fn, capacity);
    /* In place within its capacity. */
    local_get(&fn, bytes); local_get(&fn, capacity); op(&fn, 0x4d);
    if_begin(&fn);
    local_get(&fn, bytes); local_get(&fn, kept); op(&fn, 0x4b);
    if_begin(&fn);
    local_get(&fn, 0); local_get(&fn, kept); op(&fn, 0x6a); i32_const(&fn, 0);
    local_get(&fn, bytes); local_get(&fn, kept); op(&fn, 0x6b); memory_fill(&fn);
    end(&fn);
    local_get(&fn, header); local_get(&fn, 2); memarg(&fn, 0x37, 8);
    local_get(&fn, 0); op(&fn, 0x0f);
    end(&fn);
    /* The block that ends the heap grows where it is. */
    local_get(&fn, 0); local_get(&fn, capacity); op(&fn, 0x6a); global_get(&fn, G_TOP); op(&fn, 0x46);
    if_begin(&fn);
    local_get(&fn, 0); i32_const(&fn, 16); size_class(&fn, bytes); op(&fn, 0x74);
    op_u(&fn, 0x22, capacity); op(&fn, 0x6a); op_u(&fn, 0x22, limit);
    i32_const(&fn, INT32_MIN); op(&fn, 0x4b);
    if_begin(&fn); return_zero(&fn); end(&fn);
    emit_room(&fn, limit);
    local_get(&fn, limit); global_set(&fn, G_TOP);
    local_get(&fn, header); local_get(&fn, capacity); memarg(&fn, 0x36, 0);
    local_get(&fn, 0); local_get(&fn, kept); op(&fn, 0x6a); i32_const(&fn, 0);
    local_get(&fn, bytes); local_get(&fn, kept); op(&fn, 0x6b); memory_fill(&fn);
    local_get(&fn, header); local_get(&fn, 2); memarg(&fn, 0x37, 8);
    local_get(&fn, 0); op(&fn, 0x0f);
    end(&fn);
    end(&fn);
    /* A new block, then the kept elements. */
    local_get(&fn, bytes); i32_const(&fn, BLOCK_PROGRAM); call(&fn, lower->alloc_function);
    op_u(&fn, 0x22, moved); op(&fn, 0x45);
    if_begin(&fn); return_zero(&fn); end(&fn);
    local_get(&fn, 0);
    if_begin(&fn);
    local_get(&fn, moved); local_get(&fn, 0); local_get(&fn, kept); memory_copy(&fn);
    local_get(&fn, 0); call(&fn, lower->free_function);
    end(&fn);
    block_header(&fn, moved); local_get(&fn, 2); memarg(&fn, 0x37, 8);
    local_get(&fn, moved);
    helper_finish(&fn, output);
}

/* zx_reserve(bytes) -> payload of a zero-filled host block, or 0. */
static void
emit_reserve(Lower *lower, Buf *output)
{
    static const unsigned char parameters[] = {W_I32};
    Fn fn;
    helper_start(&fn, lower, parameters, 1);
    local_get(&fn, 0); i32_const(&fn, ZXW_BLOCK_LIMIT); op(&fn, 0x4b);
    if_begin(&fn); return_zero(&fn); end(&fn);
    local_get(&fn, 0); i32_const(&fn, BLOCK_HOST); call(&fn, lower->alloc_function);
    helper_finish(&fn, output);
}

/* Calls `function` (i32 address -> ...) for each of the elements at
 * `first` (an i32 local) up to `bytes` (a local, or the constant `fixed`
 * when `bytes` is UINT32_MAX), `width` apart. `second`, when not
 * UINT32_MAX, is a parallel destination; `sum`, when not UINT32_MAX, is an
 * i64 local that adds each result. */
static void
each_element(Fn *fn, uint32_t first, uint32_t second, uint32_t bytes, int64_t fixed,
             int64_t width, uint32_t function, uint32_t sum)
{
    uint32_t index = fn_local(fn, W_I32);
    if(width <= 0)
        return;
    i32_const(fn, 0); local_set(fn, index);
    block_begin(fn); loop_begin(fn);
    local_get(fn, index);
    if(bytes == UINT32_MAX) i32_const(fn, (int32_t)fixed);
    else local_get(fn, bytes);
    op(fn, 0x4f); op_u(fn, 0x0d, 1);
    if(sum != UINT32_MAX) local_get(fn, sum);
    local_get(fn, first); local_get(fn, index); op(fn, 0x6a);
    if(second != UINT32_MAX) { local_get(fn, second); local_get(fn, index); op(fn, 0x6a); }
    call(fn, function);
    if(sum != UINT32_MAX) { op(fn, 0x7c); local_set(fn, sum); }
    local_get(fn, index); i32_const(fn, (int32_t)width); op(fn, 0x6a); local_set(fn, index);
    op_u(fn, 0x0c, 0);
    end(fn); end(fn);
}

/* measure(source) -> i64: the bytes the view elements inside a value add
 * to its host image. */
static void
emit_measure(Lower *lower, int32_t type, Buf *output)
{
    static const unsigned char parameters[] = {W_I32};
    Fn fn;
    helper_start(&fn, lower, parameters, 1);
    const LirType *value = type_at(lower, type);
    int32_t element = view_element(lower, type);
    uint32_t total = fn_local(&fn, W_I64);
    i64_const(&fn, 0); local_set(&fn, total);
    if(element >= 0) {
        int64_t width = type_size(lower, element);
        uint32_t count = fn_local(&fn, W_I64), data = fn_local(&fn, W_I32), bytes = fn_local(&fn, W_I32);
        local_get(&fn, 0); memarg(&fn, 0x29, 8); op_u(&fn, 0x22, count); op(&fn, 0x50);
        if_begin(&fn); i64_const(&fn, 0); op(&fn, 0x0f); end(&fn);
        local_get(&fn, count); i64_const(&fn, width); op(&fn, 0x7e); local_set(&fn, total);
        if(lower->needs[element] & NEED_ENCODE) {
            local_get(&fn, 0); memarg(&fn, 0x29, 0); op(&fn, 0xa7); local_set(&fn, data);
            local_get(&fn, count); i64_const(&fn, width); op(&fn, 0x7e); op(&fn, 0xa7); local_set(&fn, bytes);
            each_element(&fn, data, UINT32_MAX, bytes, 0, width, lower->measure_function[element], total);
        }
    } else if(value->kind == LirTypeKind_Array) {
        int64_t width = type_size(lower, value->element);
        each_element(&fn, 0, UINT32_MAX, UINT32_MAX, width * value->count, width,
                     lower->measure_function[value->element], total);
    } else {
        const int32_t *fields = value->fields.data;
        int64_t offset = 0;
        for(int64_t index = 0; index < value->fields.length; index++) {
            if(lower->needs[fields[index]] & NEED_ENCODE) {
                local_get(&fn, total);
                local_get(&fn, 0); i32_const(&fn, (int32_t)offset); op(&fn, 0x6a);
                call(&fn, lower->measure_function[fields[index]]);
                op(&fn, 0x7c); local_set(&fn, total);
            }
            offset += type_size(lower, fields[index]);
        }
    }
    local_get(&fn, total);
    helper_finish(&fn, output);
}

/* encode(source, destination): the destination holds a copy of the
 * value's bytes; each view inside it becomes the offset and count of its
 * elements, appended at the image cursor. */
static void
emit_encode(Lower *lower, int32_t type, Buf *output)
{
    static const unsigned char parameters[] = {W_I32, W_I32};
    Fn fn;
    helper_start(&fn, lower, parameters, 2);
    const LirType *value = type_at(lower, type);
    int32_t element = view_element(lower, type);
    if(element >= 0) {
        int64_t width = type_size(lower, element);
        uint32_t count = fn_local(&fn, W_I64), at = fn_local(&fn, W_I32);
        uint32_t bytes = fn_local(&fn, W_I32), data = fn_local(&fn, W_I32);
        local_get(&fn, 0); memarg(&fn, 0x29, 8); op_u(&fn, 0x22, count); op(&fn, 0x50);
        if_begin(&fn);
        local_get(&fn, 1); i64_const(&fn, 0); memarg(&fn, 0x37, 0);
        local_get(&fn, 1); i64_const(&fn, 0); memarg(&fn, 0x37, 8);
        op(&fn, 0x0f);
        end(&fn);
        local_get(&fn, 0); memarg(&fn, 0x29, 0); op(&fn, 0xa7); local_set(&fn, data);
        local_get(&fn, count); i64_const(&fn, width); op(&fn, 0x7e); op(&fn, 0xa7); local_set(&fn, bytes);
        global_get(&fn, G_CURSOR); op_u(&fn, 0x22, at); local_get(&fn, bytes); op(&fn, 0x6a); global_set(&fn, G_CURSOR);
        local_get(&fn, 1); local_get(&fn, at); global_get(&fn, G_BASE); op(&fn, 0x6b); op(&fn, 0xad); memarg(&fn, 0x37, 0);
        local_get(&fn, 1); local_get(&fn, count); memarg(&fn, 0x37, 8);
        local_get(&fn, at); local_get(&fn, data); local_get(&fn, bytes); memory_copy(&fn);
        if(lower->needs[element] & NEED_ENCODE)
            each_element(&fn, data, at, bytes, 0, width, lower->encode_function[element], UINT32_MAX);
    } else if(value->kind == LirTypeKind_Array) {
        int64_t width = type_size(lower, value->element);
        each_element(&fn, 0, 1, UINT32_MAX, width * value->count, width,
                     lower->encode_function[value->element], UINT32_MAX);
    } else {
        const int32_t *fields = value->fields.data;
        int64_t offset = 0;
        for(int64_t index = 0; index < value->fields.length; index++) {
            if(lower->needs[fields[index]] & NEED_ENCODE) {
                local_get(&fn, 0); i32_const(&fn, (int32_t)offset); op(&fn, 0x6a);
                local_get(&fn, 1); i32_const(&fn, (int32_t)offset); op(&fn, 0x6a);
                call(&fn, lower->encode_function[fields[index]]);
            }
            offset += type_size(lower, fields[index]);
        }
    }
    helper_finish(&fn, output);
}

/* decode(at): checks a value inside the host image at G_BASE and turns
 * each view's offset into a pointer into the image. */
static void
emit_decode(Lower *lower, int32_t type, Buf *output)
{
    static const unsigned char parameters[] = {W_I32};
    Fn fn;
    helper_start(&fn, lower, parameters, 1);
    const LirType *value = type_at(lower, type);
    int32_t element = view_element(lower, type);
    if(value->kind == LirTypeKind_Bool) {
        local_get(&fn, 0); memarg(&fn, 0x2d, 0); i32_const(&fn, 1); op(&fn, 0x4b); trap_if(&fn);
    } else if(element >= 0) {
        int64_t width = type_size(lower, element);
        uint32_t start = fn_local(&fn, W_I64), count = fn_local(&fn, W_I64);
        uint32_t data = fn_local(&fn, W_I32), bytes = fn_local(&fn, W_I32);
        local_get(&fn, 0); memarg(&fn, 0x29, 0); local_set(&fn, start);
        local_get(&fn, 0); memarg(&fn, 0x29, 8); op_u(&fn, 0x22, count); op(&fn, 0x50);
        if_begin(&fn);
        local_get(&fn, start); i64_const(&fn, 0); op(&fn, 0x52); trap_if(&fn);
        op(&fn, 0x0f);
        end(&fn);
        if(width <= 0) {
            op(&fn, 0x00);
        } else {
            local_get(&fn, start); global_get(&fn, G_LENGTH); op(&fn, 0xad); op(&fn, 0x56); trap_if(&fn);
            local_get(&fn, count);
            global_get(&fn, G_LENGTH); op(&fn, 0xad); local_get(&fn, start); op(&fn, 0x7d);
            i64_const(&fn, width); op(&fn, 0x80);
            op(&fn, 0x56); trap_if(&fn);
            local_get(&fn, 0);
            global_get(&fn, G_BASE); local_get(&fn, start); op(&fn, 0xa7); op(&fn, 0x6a);
            op_u(&fn, 0x22, data); op(&fn, 0xad); memarg(&fn, 0x37, 0);
            if(lower->needs[element] & NEED_DECODE) {
                local_get(&fn, count); i64_const(&fn, width); op(&fn, 0x7e); op(&fn, 0xa7); local_set(&fn, bytes);
                each_element(&fn, data, UINT32_MAX, bytes, 0, width, lower->decode_function[element], UINT32_MAX);
            }
        }
    } else if(value->kind == LirTypeKind_Array) {
        int64_t width = type_size(lower, value->element);
        each_element(&fn, 0, UINT32_MAX, UINT32_MAX, width * value->count, width,
                     lower->decode_function[value->element], UINT32_MAX);
    } else {
        const int32_t *fields = value->fields.data;
        int64_t offset = 0;
        for(int64_t index = 0; index < value->fields.length; index++) {
            if(lower->needs[fields[index]] & NEED_DECODE) {
                local_get(&fn, 0); i32_const(&fn, (int32_t)offset); op(&fn, 0x6a);
                call(&fn, lower->decode_function[fields[index]]);
            }
            offset += type_size(lower, fields[index]);
        }
    }
    helper_finish(&fn, output);
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

static int
scan_program(Lower *lower)
{
    for(int64_t index = 0; index < lower->function_count; index++) {
        const LirFunction *function = &lower->functions[index];
        const LirBlock *blocks = function->blocks.data;
        for(int64_t block = 0; block < function->blocks.length; block++) {
            const LirInstruction *instructions = blocks[block].instructions.data;
            for(int64_t item = 0; item < blocks[block].instructions.length; item++) {
                const LirInstruction *instruction = &instructions[item];
                LirOp code = instruction->op;
                int32_t symbol = instruction->symbol;
                if(code == LirOp_HostCall && symbol >= 0 && symbol < lower->function_count) {
                    if(lower->host_kind[symbol] == HOST_CAPABILITY) lower->uses_capability = 1;
                    else if(lower->host_kind[symbol] != HOST_NONE) lower->uses_print = 1;
                }
                if(code == LirOp_BytesEqual) lower->uses_bytes_equal = 1;
                if(code == LirOp_FunctionAddress || code == LirOp_CallIndirect) lower->uses_table = 1;
                if(code == LirOp_Resize) lower->uses_heap = 1;
                if(code == LirOp_Copy || code == LirOp_Zero || code == LirOp_Invalid || code > LirOp_Resize)
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

/* zx_initialize () -> (): runs the module initializers once. zx_entry
 * calls it; a host may call it first, so an instance is initialized when
 * it opens. */
static int
emit_initialize(Lower *lower, Buf *output, uint32_t stack_base)
{
    const LirProgram *program = lower->program;
    const int32_t *initializers = program->initializers.data;
    Fn fn = {0};
    fn.lower = lower;
    global_get(&fn, G_INIT);
    if_begin(&fn); op(&fn, 0x0f); end(&fn);
    i32_const(&fn, (int32_t)stack_base); global_set(&fn, G_SP);
    for(int64_t index = 0; index < program->initializers.length; index++) {
        int32_t initializer = initializers[index];
        if(initializer < 0 || initializer >= lower->function_count || lower->functions[initializer].external ||
           lower->functions[initializer].parameters.length != 0 ||
           type_at(lower, lower->functions[initializer].result)->kind != LirTypeKind_Void) {
            buf_free(&fn.code);
            return WasmGap(lower, "wasm.gap.initializer", "module initializer @%d is not a () -> void procedure", initializer);
        }
        call(&fn, lower->wasm_function[initializer]);
    }
    i32_const(&fn, 1); global_set(&fn, G_INIT);
    end(&fn);
    buf_uleb(output, fn.code.count + 1);
    buf_uleb(output, 0);
    buf_raw(output, fn.code.data, fn.code.count);
    int failed = fn.code.failed;
    buf_free(&fn.code);
    return failed ? 0 : 1;
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
    if(program->initializers.length > 0)
        call(&fn, lower->initialize_function);
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
    lower.shapes = calloc(types, 1);
    lower.needs = calloc(types, 1);
    lower.measure_function = calloc(types, sizeof(*lower.measure_function));
    lower.encode_function = calloc(types, sizeof(*lower.encode_function));
    lower.decode_function = calloc(types, sizeof(*lower.decode_function));
    uint32_t *function_types = calloc(functions, sizeof(*function_types));
    if(lower.sizes == NULL || lower.global_address == NULL || lower.host_kind == NULL ||
       lower.host_index == NULL || lower.wasm_function == NULL || lower.shapes == NULL ||
       lower.needs == NULL || lower.measure_function == NULL || lower.encode_function == NULL ||
       lower.decode_function == NULL || function_types == NULL) {
        DiagnosticOutOfMemory();
        goto done;
    }
    for(size_t index = 0; index < types; index++)
        lower.sizes[index] = -1;
    if(!classify_externals(&lower) || !scan_program(&lower) || !layout_globals(&lower))
        goto done;
    if(lower.uses_capability && lower.uses_images) lower.uses_heap = 1;
    /* Imported state places the elements of a global's views with
     * zx_reserve. */
    for(int64_t index = 0; index < lower.global_count; index++)
        if(lower.globals[index].writable && (host_shape(&lower, lower.globals[index].type) & SHAPE_VIEWS))
            lower.uses_heap = 1;
    if(lower.uses_heap) {
        lower.free_table = data_reserve(&lower, 4 * ZXW_CLASSES);
        if(lower.failed) goto done;
    }

    /* Function index space: imports, LIR procedures, helpers, zx_entry. */
    uint32_t next = 1;
    if(lower.uses_print) lower.print_import = next++;
    if(lower.uses_capability) lower.host_import = next++;
    lower.imports = next;
    for(int64_t index = 0; index < lower.function_count; index++)
        if(!lower.functions[index].external)
            lower.wasm_function[index] = next++;
    if(lower.uses_bytes_equal) lower.bytes_equal_function = next++;
    if(lower.uses_table) lower.stub_function = next++;
    if(lower.uses_heap) {
        lower.alloc_function = next++;
        lower.free_function = next++;
        lower.resize_function = next++;
        lower.reserve_function = next++;
    }
    for(int64_t type = 0; type < lower.type_count; type++) {
        if(lower.needs[type] & NEED_ENCODE) {
            lower.measure_function[type] = next++;
            lower.encode_function[type] = next++;
        }
        if(lower.needs[type] & NEED_DECODE) lower.decode_function[type] = next++;
    }
    int initializes = program->initializers.length > 0;
    if(initializes) lower.initialize_function = next++;
    lower.entry_function = next++;

    Sig unit = {{0}, 0, 0};
    Sig print = {{W_I32, W_I64}, 2, W_I64};
    Sig host = {{W_I32, W_I32}, 2, W_I32};
    Sig entry = {{0}, 0, W_I64};
    Sig bytes = {{W_I32, W_I64, W_I32, W_I64}, 4, W_I32};
    Sig alloc = {{W_I32, W_I32}, 2, W_I32};
    Sig address = {{W_I32}, 1, 0};
    Sig resize = {{W_I32, W_I64, W_I64, W_I32}, 4, W_I32};
    Sig reserve = {{W_I32}, 1, W_I32};
    Sig measure = {{W_I32}, 1, W_I64};
    Sig encode = {{W_I32, W_I32}, 2, 0};
    uint32_t unit_type = signature_index(&lower, &unit);
    uint32_t print_type = lower.uses_print ? signature_index(&lower, &print) : 0;
    uint32_t host_type = lower.uses_capability ? signature_index(&lower, &host) : 0;
    for(int64_t index = 0; index < lower.function_count && !lower.failed; index++) {
        const LirFunction *function = &lower.functions[index];
        if(function->external) continue;
        Sig sig;
        if(signature_of(&lower, function->parameters.data, function->parameters.length, function->result, &sig))
            function_types[index] = signature_index(&lower, &sig);
    }
    uint32_t bytes_type = lower.uses_bytes_equal ? signature_index(&lower, &bytes) : 0;
    uint32_t alloc_type = 0, address_type = 0, resize_type = 0, reserve_type = 0;
    uint32_t measure_type = 0, encode_type = 0;
    if(lower.uses_heap) {
        alloc_type = signature_index(&lower, &alloc);
        address_type = signature_index(&lower, &address);
        resize_type = signature_index(&lower, &resize);
        reserve_type = signature_index(&lower, &reserve);
    }
    if(lower.uses_images) {
        measure_type = signature_index(&lower, &measure);
        encode_type = signature_index(&lower, &encode);
        address_type = signature_index(&lower, &address);
    }
    uint32_t entry_type = signature_index(&lower, &entry);

    /* Code first: constant blobs extend the data region while lowering;
     * the helpers that follow need the heap base, known only after. */
    uint32_t defined = next - lower.imports;
    buf_uleb(&code, defined);
    for(int64_t index = 0; index < lower.function_count && !lower.failed; index++)
        if(!lower.functions[index].external && !lower_function(&lower, index, &code))
            break;
    if(lower.failed) goto done;
    if(lower.uses_bytes_equal) emit_bytes_equal(&lower, &code);
    if(lower.uses_table) { buf_uleb(&code, 3); buf_uleb(&code, 0); buf_byte(&code, 0x00); buf_byte(&code, 0x0b); }
    size_t data_end = ZXW_DATA_BASE + lower.data.count;
    uint32_t stack_base = (uint32_t)((data_end + 15) & ~(size_t)15);
    uint32_t stack_limit = stack_base + ZXW_STACK_SIZE;
    if(lower.uses_heap) {
        emit_alloc(&lower, &code);
        emit_free(&lower, &code);
        emit_resize(&lower, &code);
        emit_reserve(&lower, &code);
    }
    for(int64_t type = 0; type < lower.type_count && !lower.failed; type++) {
        if(lower.needs[type] & NEED_ENCODE) {
            emit_measure(&lower, (int32_t)type, &code);
            emit_encode(&lower, (int32_t)type, &code);
        }
        if(lower.needs[type] & NEED_DECODE) emit_decode(&lower, (int32_t)type, &code);
    }
    if(lower.failed || (initializes && !emit_initialize(&lower, &code, stack_base)) ||
       !emit_entry(&lower, &code, stack_base)) goto done;

    /* An array, not a string literal: a literal that starts with NUL can
     * be merged with an empty string when objects are linked together. */
    static const unsigned char magic[8] = {0, 'a', 's', 'm', 1, 0, 0, 0};
    buf_raw(&module, magic, sizeof(magic));
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
    buf_uleb(&content, lower.imports + 1);
    buf_name(&content, "zx", 2); buf_name(&content, "overrun", 7);
    buf_byte(&content, 0x00); buf_uleb(&content, unit_type);
    if(lower.uses_print) {
        buf_name(&content, "zx", 2); buf_name(&content, "host_i64", 8);
        buf_byte(&content, 0x00); buf_uleb(&content, print_type);
    }
    if(lower.uses_capability) {
        buf_name(&content, "zx", 2); buf_name(&content, "host", 4);
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
    if(lower.uses_heap) {
        buf_uleb(&content, alloc_type);
        buf_uleb(&content, address_type);
        buf_uleb(&content, resize_type);
        buf_uleb(&content, reserve_type);
    }
    for(int64_t type = 0; type < lower.type_count; type++) {
        if(lower.needs[type] & NEED_ENCODE) {
            buf_uleb(&content, measure_type);
            buf_uleb(&content, encode_type);
        }
        if(lower.needs[type] & NEED_DECODE) buf_uleb(&content, address_type);
    }
    if(initializes) buf_uleb(&content, unit_type);
    buf_uleb(&content, entry_type);
    section(&module, 3, &content);
    /* Table: slot 0 and external procedures trap; slot n + 1 is LIR @n. */
    if(lower.uses_table) {
        buf_uleb(&content, 1); buf_byte(&content, 0x70); buf_byte(&content, 0x01);
        buf_uleb(&content, (uint64_t)lower.function_count + 1);
        buf_uleb(&content, (uint64_t)lower.function_count + 1);
        section(&module, 4, &content);
    }
    /* Memory: data, a fixed stack, then the growable heap. */
    buf_uleb(&content, 1); buf_byte(&content, 0x00);
    buf_uleb(&content, (stack_limit + 65535u) / 65536u);
    section(&module, 5, &content);
    /* Globals: stack pointer, initializer flag, stack limit, then the heap
     * top and the image base, cursor and length. */
    {
        unsigned count = lower.uses_images ? 7 : lower.uses_heap ? 4 : 3;
        int32_t initial[] = {(int32_t)stack_base, 0, (int32_t)stack_limit, (int32_t)stack_limit, 0, 0, 0};
        buf_uleb(&content, count);
        for(unsigned index = 0; index < count; index++) {
            buf_byte(&content, W_I32); buf_byte(&content, index == 2 ? 0x00 : 0x01);
            buf_byte(&content, 0x41); buf_sleb(&content, initial[index]); buf_byte(&content, 0x0b);
        }
    }
    section(&module, 6, &content);
    /* Exports. */
    buf_uleb(&content, 2 + (lower.uses_heap ? 1 : 0) + (initializes ? 1 : 0));
    buf_name(&content, "zx_entry", 8); buf_byte(&content, 0x00); buf_uleb(&content, lower.entry_function);
    buf_name(&content, "memory", 6); buf_byte(&content, 0x02); buf_uleb(&content, 0);
    if(initializes) {
        buf_name(&content, "zx_initialize", 13); buf_byte(&content, 0x00); buf_uleb(&content, lower.initialize_function);
    }
    if(lower.uses_heap) {
        buf_name(&content, "zx_reserve", 10); buf_byte(&content, 0x00); buf_uleb(&content, lower.reserve_function);
    }
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
    /* zxhost: each capability's identity, by capability index. */
    if(lower.capability_count > 0) {
        buf_name(&content, "zxhost", 6);
        buf_uleb(&content, lower.capability_count);
        for(size_t index = 0; index < lower.capability_count; index++) {
            const ZxWasmCapability *capability = &lower.capabilities[index];
            buf_name(&content, capability->module, strlen(capability->module));
            buf_name(&content, capability->function, strlen(capability->function));
            buf_uleb(&content, capability->parameter_count);
        }
        section(&module, 0, &content);
    }
    if(module.failed || content.failed || code.failed) {
        DiagnosticOutOfMemory();
        goto done;
    }
    uint32_t *addresses = calloc(globals, sizeof(*addresses));
    uint32_t *sizes = calloc(globals, sizeof(*sizes));
    if(addresses == NULL || sizes == NULL) {
        DiagnosticOutOfMemory();
        free(addresses);
        free(sizes);
        goto done;
    }
    for(int64_t index = 0; index < lower.global_count; index++) {
        addresses[index] = lower.global_address[index];
        sizes[index] = (uint32_t)type_size(&lower, lower.globals[index].type);
    }
    image->bytes = module.data;
    image->size = module.count;
    image->capabilities = lower.capabilities;
    image->capability_count = lower.capability_count;
    image->global_addresses = addresses;
    image->global_sizes = sizes;
    image->global_count = (size_t)lower.global_count;
    image->returns = type_at(&lower, lower.functions[program->entry].result)->kind != LirTypeKind_Void;
    module.data = NULL;
    lower.capabilities = NULL;
    lower.capability_count = 0;
    ok = 1;
done:
    for(size_t index = 0; index < lower.capability_count; index++) {
        free(lower.capabilities[index].module);
        free(lower.capabilities[index].function);
        free(lower.capabilities[index].parameters);
    }
    free(lower.capabilities);
    free(function_types);
    free(lower.shapes);
    free(lower.needs);
    free(lower.measure_function);
    free(lower.encode_function);
    free(lower.decode_function);
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
        free(image->capabilities[index].parameters);
    }
    free(image->capabilities);
    free(image->global_addresses);
    free(image->global_sizes);
    free(image->bytes);
    memset(image, 0, sizeof(*image));
}

/* ------------------------------------------------------------ module state */

/* Type questions for state images, without lowering a module. */
typedef struct StateImage {
    Lower lower;
    const ZxWasmMemory *memory;
    Buf image;
} StateImage;

static int
state_open(StateImage *state, const LirProgram *program, const ZxWasmMemory *memory)
{
    memset(state, 0, sizeof(*state));
    state->memory = memory;
    Lower *lower = &state->lower;
    lower->program = program;
    lower->types = program->types.data;
    lower->type_count = program->types.length;
    size_t types = (size_t)(lower->type_count ? lower->type_count : 1);
    lower->sizes = malloc(types * sizeof(*lower->sizes));
    lower->shapes = calloc(types, 1);
    if(lower->sizes == NULL || lower->shapes == NULL)
        return 0;
    for(size_t index = 0; index < types; index++)
        lower->sizes[index] = -1;
    return 1;
}

static void
state_close(StateImage *state)
{
    free(state->lower.sizes);
    free(state->lower.shapes);
    buf_free(&state->image);
}

static uint64_t
state_read(const unsigned char *bytes)
{
    uint64_t value = 0;
    for(int at = 0; at < 8; at++) value |= (uint64_t)bytes[at] << (8 * at);
    return value;
}

static void
state_write(unsigned char *bytes, uint64_t value)
{
    for(int at = 0; at < 8; at++) bytes[at] = (unsigned char)(value >> (8 * at));
}

/* Turns each view of the `type` value at image[at], whose words are still
 * an instance's pointer and count, into an image offset and count,
 * appending the elements read from memory. */
static int
state_encode(StateImage *state, int32_t type, size_t at, int depth)
{
    Lower *lower = &state->lower;
    if(depth > 64 || !(host_shape(lower, type) & SHAPE_VIEWS))
        return depth <= 64;
    const LirType *value = type_at(lower, type);
    int32_t element = view_element(lower, type);
    if(element >= 0) {
        uint64_t pointer = state_read(state->image.data + at), count = state_read(state->image.data + at + 8);
        int64_t width = type_size(lower, element);
        if(count == 0) {
            state_write(state->image.data + at, 0);
            return 1;
        }
        if(width <= 0 || pointer > UINT32_MAX || count > (uint64_t)ZXW_BLOCK_LIMIT / (uint64_t)width)
            return 0;
        size_t bytes = (size_t)(count * (uint64_t)width), start = state->image.count;
        buf_reserve(&state->image, bytes);
        if(state->image.failed || !state->memory->read(state->memory->context, (size_t)pointer,
                                                       state->image.data + start, bytes))
            return 0;
        state->image.count += bytes;
        state_write(state->image.data + at, start);
        if(host_shape(lower, element) & SHAPE_VIEWS)
            for(uint64_t index = 0; index < count; index++)
                if(!state_encode(state, element, start + (size_t)index * (size_t)width, depth + 1))
                    return 0;
        return 1;
    }
    if(value->kind == LirTypeKind_Array) {
        int64_t width = type_size(lower, value->element);
        for(int64_t index = 0; index < value->count; index++)
            if(!state_encode(state, value->element, at + (size_t)(index * width), depth + 1))
                return 0;
        return 1;
    }
    const int32_t *fields = value->fields.data;
    size_t offset = 0;
    for(int64_t index = 0; index < value->fields.length; index++) {
        if(!state_encode(state, fields[index], at + offset, depth + 1))
            return 0;
        offset += (size_t)type_size(lower, fields[index]);
    }
    return 1;
}

/* Places the views of the `type` value at image[at] in fresh storage,
 * elements first, so each view's words become a pointer and count. */
static int
state_place(StateImage *state, int32_t type, size_t at, int depth)
{
    Lower *lower = &state->lower;
    unsigned shape = host_shape(lower, type);
    if(depth > 64 || !(shape & (SHAPE_VIEWS | SHAPE_BOOLS)))
        return depth <= 64;
    const LirType *value = type_at(lower, type);
    unsigned char *bytes = state->image.data;
    size_t size = state->image.count;
    if(value->kind == LirTypeKind_Bool)
        return bytes[at] <= 1;
    int32_t element = view_element(lower, type);
    if(element >= 0) {
        uint64_t start = state_read(bytes + at), count = state_read(bytes + at + 8);
        int64_t width = type_size(lower, element);
        if(count == 0)
            return start == 0;
        if(width <= 0 || start > size || count > (size - start) / (uint64_t)width)
            return 0;
        for(uint64_t index = 0; index < count; index++)
            if(!state_place(state, element, (size_t)start + (size_t)index * (size_t)width, depth + 1))
                return 0;
        size_t length = (size_t)count * (size_t)width;
        size_t address = state->memory->reserve(state->memory->context, length);
        if(address == 0 || !state->memory->write(state->memory->context, address,
                                                 state->image.data + start, length))
            return 0;
        state_write(state->image.data + at, address);
        return 1;
    }
    if(value->kind == LirTypeKind_Array) {
        int64_t width = type_size(lower, value->element);
        for(int64_t index = 0; index < value->count; index++)
            if(!state_place(state, value->element, at + (size_t)(index * width), depth + 1))
                return 0;
        return 1;
    }
    const int32_t *fields = value->fields.data;
    size_t offset = 0;
    for(int64_t index = 0; index < value->fields.length; index++) {
        if(!state_place(state, fields[index], at + offset, depth + 1))
            return 0;
        offset += (size_t)type_size(lower, fields[index]);
    }
    return 1;
}

static const LirGlobal *
state_global(const LirProgram *program, int32_t global, const ZxWasmImage *module)
{
    if(program == NULL || module == NULL || global < 0 || global >= program->globals.length ||
       (size_t)global >= module->global_count)
        return NULL;
    return &((const LirGlobal *)program->globals.data)[global];
}

int
ZxWasmGlobalImage(const struct LirProgram *program, int32_t global, const ZxWasmImage *module,
                  const ZxWasmMemory *memory, unsigned char **image, size_t *size)
{
    const LirGlobal *declared = state_global(program, global, module);
    StateImage state;
    int ok = 0;
    *image = NULL;
    *size = 0;
    if(declared == NULL || !state_open(&state, program, memory))
        goto done;
    size_t width = module->global_sizes[global];
    if(!(host_shape(&state.lower, declared->type) & SHAPE_VALID))
        goto done;
    buf_reserve(&state.image, width ? width : 1);
    if(state.image.failed || !memory->read(memory->context, module->global_addresses[global],
                                           state.image.data, width))
        goto done;
    state.image.count = width;
    if(!state_encode(&state, declared->type, 0, 0))
        goto done;
    *image = state.image.data;
    *size = state.image.count;
    state.image.data = NULL;
    ok = 1;
done:
    state_close(&state);
    return ok;
}

int
ZxWasmGlobalStore(const struct LirProgram *program, int32_t global, const ZxWasmImage *module,
                  const ZxWasmMemory *memory, const unsigned char *image, size_t size)
{
    const LirGlobal *declared = state_global(program, global, module);
    StateImage state;
    int ok = 0;
    if(declared == NULL || !state_open(&state, program, memory))
        goto done;
    size_t width = module->global_sizes[global];
    if(size < width || !(host_shape(&state.lower, declared->type) & SHAPE_VALID))
        goto done;
    buf_raw(&state.image, image, size);
    if(state.image.failed || !state_place(&state, declared->type, 0, 0) ||
       !memory->write(memory->context, module->global_addresses[global], state.image.data, width))
        goto done;
    ok = 1;
done:
    state_close(&state);
    return ok;
}

int
ZxWasmLowerProgram(const struct LirProgram *program, ZxWasmImage *image)
{
    if(image == NULL)
        return 0;
    memset(image, 0, sizeof(*image));
    return program != NULL && lower_module(program, image);
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

#else

#include <string.h>

/* Bootstrap builds have no engine to read executable ZX. */
int
ZxWasmLowerExecutable(const unsigned char *zx, size_t size, ZxWasmImage *image)
{
    (void)zx;
    (void)size;
    if(image != NULL) memset(image, 0, sizeof(*image));
    Diagnostic((ZirSourceSpan){0}, "wasm.container", "this build has no instruction engine");
    return 0;
}

int
ZxWasmLowerProgram(const struct LirProgram *program, ZxWasmImage *image)
{
    return ZxWasmLowerExecutable((const unsigned char *)program, 0, image);
}

void
ZxWasmImageFree(ZxWasmImage *image)
{
    if(image != NULL) memset(image, 0, sizeof(*image));
}

int
ZxWasmGlobalImage(const struct LirProgram *program, int32_t global, const ZxWasmImage *module,
                  const ZxWasmMemory *memory, unsigned char **image, size_t *size)
{
    (void)program; (void)global; (void)module; (void)memory;
    *image = NULL;
    *size = 0;
    return 0;
}

int
ZxWasmGlobalStore(const struct LirProgram *program, int32_t global, const ZxWasmImage *module,
                  const ZxWasmMemory *memory, const unsigned char *image, size_t size)
{
    (void)program; (void)global; (void)module; (void)memory; (void)image; (void)size;
    return 0;
}

#endif
