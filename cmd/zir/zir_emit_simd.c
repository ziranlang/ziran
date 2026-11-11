/* Emission of std/simd operations; see zir_simd.h and include/ziran_simd.h. */
#include "zir_emit_internal.h"
#include "zir_simd.h"
#define ZIRAN_SIMD_REF_ONLY 1
#include "ziran_simd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *zir_emit_simd_tier;

void
EmitUseSimdTier(const char *tier, int native_blocks)
{
    zir_emit_simd_tier = tier;
    zir_native_blocks = tier != NULL ? native_blocks : 0;
}

/* The procedure of module a plain call names, when only one has that name. */
static const ZirFunction *
module_procedure(const ZirModule *module, const char *name)
{
    const ZirFunction *found = NULL;
    if(name[0] == '\0' || strchr(name, '.') != NULL)
        return NULL;
    for(int i = 0; i < module->function_count; i++)
        if(!strcmp(module->functions[i].name, name)) {
            if(found != NULL)
                return NULL;
            found = &module->functions[i];
        }
    return found;
}

int
SimdTierCopied(const ZirModule *module, const ZirFunction *fn)
{
    (void)module;
    return fn->inlining == ZIR_INLINE_ALWAYS && !fn->simd_target && !fn->is_template &&
           !fn->is_extern && !(fn->assembly_text && *fn->assembly_text);
}

int
SimdTierCopyCall(const Emitter *e, const ZirExpr *call)
{
    const ZirFunction *callee;
    if(e->target != ZIR_C || e->simd_tier == NULL || call->slot_type[0])
        return 0;
    callee = module_procedure(e->module, call->name);
    return callee != NULL && SimdTierCopied(e->module, callee);
}

/* "zi_", or "zi_avx2_" and the like in a #target clone for that tier. The
 * operations taking constants are macros that suit every tier. */
static void
helper_prefix(const Emitter *e, const SimdOp *op, char *out, size_t size)
{
    int macro = op != NULL && (op->id == ZI_OP_ALIGN_BYTES || op->id == ZI_OP_SHL_BYTES ||
                               op->id == ZI_OP_SHR_BYTES || op->id == ZI_OP_SHUFFLE);
    if(e->simd_tier != NULL && !macro) format(out, size, "zi_%s_", e->simd_tier);
    else format(out, size, "zi_");
}

/* Reinterpret, Narrow, Convert and Gather name both their operand and result
 * types; every other helper names the type it works on. The types are as
 * VectorTypeHere gives them. In a #target clone, native vectors of two
 * blocks use their own helpers where SimdNativeHelper says so; elsewhere
 * ziran_simd_ops.h names those helpers for them. */
static void
helper_name(const Emitter *e, const SimdOp *op, const char *operand,
            const char *result, int native, char *out, size_t size)
{
    char prefix[24];
    helper_prefix(e, op, prefix, sizeof(prefix));
    if(native && e->target == ZIR_C && zir_native_blocks == 2 && SimdNativeHelper(op)) {
        size_t used = strlen(prefix);
        format(prefix + used, sizeof(prefix) - used, "native_");
    }
    switch(op->shape) {
    case SIMD_NARROW: case SIMD_REINTERPRET: case SIMD_CONVERT: case SIMD_GATHER:
        format(out, size, "%s%s_%s_%s", prefix, op->snake, operand, result);
        return;
    case SIMD_MAKE: case SIMD_SPLAT: case SIMD_ZERO: case SIMD_LOAD:
        format(out, size, "%s%s_%s", prefix, op->snake, result);
        return;
    default:
        format(out, size, "%s%s_%s", prefix, op->snake, operand);
    }
}

static int
native_vector(const char *type)
{
    ZirVector vector;
    return type != NULL && VectorType(type, &vector) && vector.is_native;
}

/* `lanes` lanes in each 128-bit block of a native vector, times its blocks
 * here: a constant in Go and #target clones, ZI_NATIVE_BLOCKS in other C. */
static void
native_count(const Emitter *e, long lanes, char *out, size_t size)
{
    if(e->target == ZIR_C && zir_native_blocks == 0)
        format(out, size, "(%ld * ZI_NATIVE_BLOCKS)", lanes);
    else
        format(out, size, "%ld", lanes * (e->target == ZIR_C ? zir_native_blocks : 1));
}

/* Lanes a load or store of `type` touches: LoadLow and StoreLow move 64 bits
 * of a fixed vector and half of a native one. Returns their number, or 0
 * when only the running code knows it. */
static int
touched_lanes(const Emitter *e, const SimdOp *op, const char *type, char *out, size_t size)
{
    ZirVector vector;
    VectorType(type, &vector);
    int low = op->id == ZI_OP_LOAD_LOW || op->id == ZI_OP_STORE_LOW;
    if(vector.is_native) {
        native_count(e, low ? vector.lanes / 2 : vector.lanes, out, size);
        return e->target == ZIR_C && zir_native_blocks == 0 ? 0 :
               (low ? vector.lanes / 2 : vector.lanes) * (e->target == ZIR_C ? zir_native_blocks : 1);
    }
    format(out, size, "%d", low ? 64 / vector.lane_bits : vector.lanes);
    return low ? 64 / vector.lane_bits : vector.lanes;
}

/* A slice or array operand: text read several times stays in a temporary.
 * Arrays are used in place, never copied for one access, and a constant
 * table that is only read is the shared table itself. */
static void
memory_operand(Emitter *e, int child, int kind, int writable, char *out, size_t size)
{
    const ZirExpr *memory = &e->fn->exprs[child];
    if(kind == SIMD_MEMORY_ARRAY && (memory->kind == ZIR_EXPR_IDENT ||
                                     memory->kind == ZIR_EXPR_MEMBER ||
                                     memory->kind == ZIR_EXPR_POINTER_MEMBER ||
                                     memory->kind == ZIR_EXPR_INDEX)) {
        emit_destination(e, child, out, size);
        return;
    }
    if(kind == SIMD_MEMORY_ARRAY && !writable && emit_constant_array_read(e, child, out, size))
        return;
    emit_expr(e, child, memory->type, out, size);
    if((kind == SIMD_MEMORY_SLICE || kind == SIMD_MEMORY_STRING) && !plain_identifier(out)) {
        char temporary[ZIR_NAME_MAX];
        fresh(e, temporary);
        declare(e, temporary, memory->type, out);
        copy_text(out, size, temporary);
    }
}

/* A whole slice of a named array, a[:], is the array's own memory, so no
 * view needs building: the array child, or -1. */
static int
whole_array_slice(const Emitter *e, int child)
{
    const ZirExpr *memory = &e->fn->exprs[child];
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(memory->kind != ZIR_EXPR_SLICE || memory->right >= 0 || memory->third >= 0)
        return -1;
    const ZirExpr *base = &e->fn->exprs[memory->left];
    if(base->kind != ZIR_EXPR_IDENT && base->kind != ZIR_EXPR_MEMBER &&
       base->kind != ZIR_EXPR_POINTER_MEMBER && base->kind != ZIR_EXPR_INDEX)
        return -1;
    if(SimdMemory(base->type, element, sizeof(element), &capacity) != SIMD_MEMORY_ARRAY || capacity <= 0)
        return -1;
    return memory->left;
}

/* The C address of `lanes` lanes at `index`, the text of expression at.
 * Slices and arrays check the whole run unless the range facts prove it,
 * from at, inside: touched is the lane count, 0 when not fixed here.
 * Pointers are unchecked, as their indexing is. */
static void
c_address(Emitter *e, int memory_child, int at, const char *index, const char *lanes,
          int touched, int writable, char *out, size_t size)
{
    int array = whole_array_slice(e, memory_child);
    if(array >= 0)
        memory_child = array;
    const char *type = e->fn->exprs[memory_child].type;
    char element[ZIR_NAME_MAX], base[ZIR_TEXT_MAX];
    int capacity = 0;
    int kind = SimdMemory(type, element, sizeof(element), &capacity);
    const char *lane = TargetType(element, ZIR_C);
    unsigned bytes = ScalarByteWidth(element);
    if(lane == NULL || bytes == 0)
        fatal(&e->fn->exprs[memory_child], "SIMD memory needs scalar lanes");
    int proven = touched > 0 &&
        ((kind == SIMD_MEMORY_ARRAY && capacity >= touched &&
          IndexInRange(e, at, (int64_t)capacity - touched + 1)) ||
         ((kind == SIMD_MEMORY_SLICE || kind == SIMD_MEMORY_STRING) &&
          RunBelowCount(e, at, touched, memory_child)));
    memory_operand(e, memory_child, kind, writable, base, sizeof(base));
    if(kind == SIMD_MEMORY_POINTER || proven) {
        format(out, size, "(%svoid *)((%s%s *)(%s%s) + (int64_t)(%s))",
               writable ? "" : "const ", writable ? "" : "const ", lane, base,
               kind == SIMD_MEMORY_SLICE || kind == SIMD_MEMORY_STRING ? ".data" : "", index);
        return;
    }
    if(kind == SIMD_MEMORY_SLICE || kind == SIMD_MEMORY_STRING)
        format(out, size, "%szi_simd_at(%s.data, %s.length, (int64_t)(%s), %s, %u)",
               writable ? "(void *)" : "", base, base, index, lanes, bytes);
    else
        format(out, size, "%szi_simd_at(%s, %d, (int64_t)(%s), %s, %u)",
               writable ? "(void *)" : "", base, capacity, index, lanes, bytes);
}

/* Go memory operands are slices: arrays are sliced in place, a constant
 * table that is only read is the shared table, and pointers go to the
 * unchecked _ptr helpers. */
static void
go_memory(Emitter *e, int child, int kind, int writable, char *out, size_t size)
{
    const ZirExpr *memory = &e->fn->exprs[child];
    char text[ZIR_TEXT_MAX];
    if(kind != SIMD_MEMORY_ARRAY) {
        emit_expr(e, child, memory->type, out, size);
        return;
    }
    if(memory->kind == ZIR_EXPR_IDENT || memory->kind == ZIR_EXPR_MEMBER ||
       memory->kind == ZIR_EXPR_POINTER_MEMBER || memory->kind == ZIR_EXPR_INDEX)
        emit_destination(e, child, text, sizeof(text));
    else if(writable || !emit_constant_array_read(e, child, text, sizeof(text))) {
        char temporary[ZIR_NAME_MAX];
        emit_expr(e, child, memory->type, text, sizeof(text));
        fresh(e, temporary);
        declare(e, temporary, memory->type, text);
        copy_text(text, sizeof(text), temporary);
    }
    format(out, size, "%s[:]", text);
}

/* A Go load, store or gather helper name: _ptr marks pointer access. */
static void
go_name(const SimdOp *op, int kind, const char *types, char *out, size_t size)
{
    format(out, size, "zi_%s%s_%s", op->snake,
           kind == SIMD_MEMORY_POINTER ? "_ptr" : kind == SIMD_MEMORY_STRING ? "_string" : "", types);
}

/* Gather checks every index of a slice or array table first. */
static void
c_gather(Emitter *e, const SimdOp *op, const int *children, const char *result,
         char *out, size_t size)
{
    int array = whole_array_slice(e, children[0]);
    int table_child = array >= 0 ? array : children[0];
    const char *table_type = e->fn->exprs[table_child].type;
    const char *index_type = e->fn->exprs[children[1]].type;
    char element[ZIR_NAME_MAX], table[ZIR_TEXT_MAX], indices[ZIR_TEXT_MAX];
    char name[ZIR_NAME_MAX * 2], temporary[ZIR_NAME_MAX], lanes[48];
    int capacity = 0;
    int kind = SimdMemory(table_type, element, sizeof(element), &capacity);
    ZirVector index;
    VectorType(index_type, &index);
    if(index.is_native) native_count(e, index.lanes, lanes, sizeof(lanes));
    else format(lanes, sizeof(lanes), "%d", index.lanes);
    memory_operand(e, table_child, kind, 0, table, sizeof(table));
    emit_expr(e, children[1], index_type, indices, sizeof(indices));
    if(!plain_identifier(indices)) {
        fresh(e, temporary);
        declare(e, temporary, index_type, indices);
        copy_text(indices, sizeof(indices), temporary);
    }
    helper_name(e, op, VectorTypeHere(index.name, ZIR_C), result, 0, name, sizeof(name));
    if(kind == SIMD_MEMORY_POINTER) {
        format(out, size, "%s((const void *)(%s), %s)", name, table, indices);
        return;
    }
    if(kind == SIMD_MEMORY_SLICE) {
        line(e, "zi_simd_gather_check(&%s, %s, %d, %s.length);", indices,
             lanes, index.is_signed, table);
        format(out, size, "%s(%s.data, %s)", name, table, indices);
    } else {
        line(e, "zi_simd_gather_check(&%s, %s, %d, %d);", indices,
             lanes, index.is_signed, capacity);
        format(out, size, "%s(%s, %s)", name, table, indices);
    }
}

/* Buffers emit_simd_call keeps on the heap so deep nesting fits the stack. */
typedef struct SimdBuffers {
    char text[40][ZIR_TEXT_MAX];
    char arguments[ZIR_TEXT_MAX * 2];
    char index[ZIR_TEXT_MAX];
    char address[ZIR_TEXT_MAX * 2];
    char piece[ZIR_TEXT_MAX + 64];
    char name[ZIR_NAME_MAX * 2];
} SimdBuffers;

static int
go_memory_call(Emitter *e, const SimdOp *op, const int *children, const char *result,
               char *out, size_t size, SimdBuffers *buffers)
{
    const char *memory_type = e->fn->exprs[children[0]].type;
    int kind = SimdMemory(memory_type, NULL, 0, NULL);
    char *memory = buffers->address, *index = buffers->index;
    go_memory(e, children[0], kind, op->shape == SIMD_STORE, memory, sizeof(buffers->address));
    if(op->shape == SIMD_GATHER) {
        const char *index_type = VectorTypeHere(canonical(e->fn->exprs[children[1]].type), ZIR_GO);
        emit_expr(e, children[1], index_type, index, sizeof(buffers->index));
        format(buffers->piece, sizeof(buffers->piece), "%s_%s", index_type, result);
        go_name(op, kind, buffers->piece, buffers->name, sizeof(buffers->name));
        format(out, size, "%s(%s, %s)", buffers->name, memory, index);
        e->pure = 0;
        return 0;
    }
    emit_expr(e, children[1], e->fn->exprs[children[1]].type, index, sizeof(buffers->index));
    if(op->shape == SIMD_STORE) {
        const char *vector = VectorTypeHere(canonical(e->fn->exprs[children[2]].type), ZIR_GO);
        emit_expr(e, children[2], e->fn->exprs[children[2]].type, buffers->text[0],
                  sizeof(buffers->text[0]));
        go_name(op, kind, vector, buffers->name, sizeof(buffers->name));
        line(e, "%s(%s, int64(%s), %s)", buffers->name, memory, index, buffers->text[0]);
        out[0] = '\0';
        e->pure = 0;
        return 1;
    }
    go_name(op, kind, result, buffers->name, sizeof(buffers->name));
    format(out, size, "%s(%s, int64(%s))", buffers->name, memory, index);
    e->pure = 0;
    return 0;
}

/* A Go shift count: negative constants are written as their unsigned value,
 * as Go rejects converting them. */
static void
go_count(const ZirExpr *operand, const char *text, char *out, size_t size)
{
    if(operand->kind == ZIR_EXPR_INT && text[0] == '-')
        format(out, size, "uint64(%llu)", (unsigned long long)(uint64_t)strtoll(text, NULL, 0));
    else
        format(out, size, "uint64(%s)", text);
}

static int
emit_simd_with_buffers(Emitter *e, const ZirExpr *expr, char *out, size_t size,
                       SimdBuffers *buffers)
{
    const SimdOp *op = SimdOpChecked(expr->name);
    int children[40], count = 0;
    for(int child = expr->first_child; child >= 0 && count < 40;
        child = e->fn->exprs[child].next_sibling)
        children[count++] = child;
    if(op == NULL) fatal(expr, "unknown SIMD operation");
    if(e->target != ZIR_C && e->target != ZIR_GO)
        fatal(expr, "SIMD operations are not supported on this target yet");
    int go = e->target == ZIR_GO;

    if(op->shape == SIMD_COUNT) {
        /* zi_simd_lanes(k) counts k lanes a block, zi_simd_blocks() one. */
        long lanes = op->id == ZI_OP_LANES && count == 1 ?
            strtol(e->fn->exprs[children[0]].text, NULL, 10) : 1;
        char value[48];
        native_count(e, lanes, value, sizeof(value));
        format(out, size, go ? "int64(%s)" : "(int64_t)%s", value);
        e->pure = 1;
        return 0;
    }
    /* Native vectors take the types VectorTypeHere gives them. */
    const char *result = VectorTypeHere(canonical(expr->type), e->target);
    const char *first = count > 0 ? canonical(e->fn->exprs[children[0]].type) : "";
    int native = native_vector(expr->type) || native_vector(first);
    first = VectorTypeHere(first, e->target);
    char (*text)[ZIR_TEXT_MAX] = buffers->text;
    char *name = buffers->name;
    int pure = 1;
    buffers->arguments[0] = '\0';

    if(go && (op->shape == SIMD_GATHER || op->shape == SIMD_LOAD || op->shape == SIMD_STORE))
        return go_memory_call(e, op, children, result, out, size, buffers);
    if(op->shape == SIMD_GATHER) {
        c_gather(e, op, children, result, out, size);
        e->pure = 0;
        return 0;
    }
    if(op->shape == SIMD_LOAD || op->shape == SIMD_STORE) {
        int value = op->shape == SIMD_STORE ? 2 : -1;
        const char *type = canonical(value >= 0 ? e->fn->exprs[children[2]].type : expr->type);
        const char *here = VectorTypeHere(type, ZIR_C);
        char lanes[48];
        emit_expr(e, children[1], e->fn->exprs[children[1]].type, buffers->index,
                  sizeof(buffers->index));
        if(value >= 0) {
            /* The value is computed before the access is checked. */
            emit_expr(e, children[2], type, text[0], sizeof(text[0]));
        }
        int touched = touched_lanes(e, op, type, lanes, sizeof(lanes));
        c_address(e, children[0], children[1], buffers->index, lanes, touched, value >= 0,
                  buffers->address, sizeof(buffers->address));
        helper_name(e, op, here, here, native_vector(type), name, sizeof(buffers->name));
        if(value >= 0) {
            line(e, "%s(%s, %s);", name, buffers->address, text[0]);
            out[0] = '\0';
            e->pure = 0;
            return 1;
        }
        format(out, size, "%s(%s)", name, buffers->address);
        e->pure = 0;
        return 0;
    }

    for(int i = 0; i < count; i++) {
        const ZirExpr *operand = &e->fn->exprs[children[i]];
        emit_expr(e, children[i], operand->type, text[i], sizeof(text[i]));
        pure &= e->pure;
    }
    if(op->shape == SIMD_REINTERPRET && !strcmp(first, result)) {
        copy_text(out, size, text[0]);
        e->pure = pure;
        return 0;
    }
    /* Select is named for its values, not for its mask. */
    helper_name(e, op, op->shape == SIMD_SELECT && count > 1 ?
                VectorTypeHere(canonical(e->fn->exprs[children[1]].type), e->target) : first,
                result, native, name, sizeof(buffers->name));
    for(int i = 0; i < count; i++) {
        const char *value = text[i];
        if(op->shape == SIMD_SELECT && i == 0) {
            /* The helpers take the mask as the unsigned vector of its shape. */
            ZirVector mask;
            VectorType(first, &mask);
            char lane[8];
            format(lane, sizeof(lane), "u%d", mask.lane_bits);
            const char *unsigned_mask = mask.is_native ? NativeVectorName(lane) :
                VectorTypeName(lane, mask.lanes);
            if(strcmp(first, unsigned_mask)) {
                char prefix[16];
                helper_prefix(e, NULL, prefix, sizeof(prefix));
                format(buffers->piece, sizeof(buffers->piece), "%sreinterpret_%s_%s(%s)",
                       prefix, first, unsigned_mask, value);
                value = buffers->piece;
            }
        } else if(op->shape == SIMD_SHIFT && i == 1) {
            if(go) go_count(&e->fn->exprs[children[i]], value, buffers->piece, sizeof(buffers->piece));
            else format(buffers->piece, sizeof(buffers->piece), "(uint64_t)(%s)", value);
            value = buffers->piece;
        } else if((op->shape == SIMD_LANE || op->shape == SIMD_WITH_LANE) && i == 1) {
            format(buffers->piece, sizeof(buffers->piece), go ? "int(%s)" : "(int)(%s)", value);
            value = buffers->piece;
        } else if(go && (op->shape == SIMD_MAKE || op->shape == SIMD_SPLAT ||
                         (op->shape == SIMD_WITH_LANE && i == 2))) {
            /* Go converts a narrower lane value explicitly; C converts it. */
            ZirVector vector;
            VectorType(op->shape == SIMD_WITH_LANE ? first : result, &vector);
            format(buffers->piece, sizeof(buffers->piece), "%s(%s)",
                   TargetType(vector.lane, ZIR_GO), value);
            value = buffers->piece;
        }
        size_t used = strlen(buffers->arguments);
        format(buffers->arguments + used, sizeof(buffers->arguments) - used, "%s%s",
               i ? ", " : "", value);
    }
    format(out, size, "%s(%s)", name, buffers->arguments);
    e->pure = pure;
    return 0;
}

/* Returns 1 when the operation was a statement (a store), else 0 with the
 * value text in `out`. */
int
emit_simd_call(Emitter *e, const ZirExpr *expr, char *out, size_t size)
{
    SimdBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    int statement = emit_simd_with_buffers(e, expr, out, size, buffers);
    free(buffers);
    return statement;
}
