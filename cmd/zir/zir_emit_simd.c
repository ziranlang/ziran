/* Emission of std/simd operations; see zir_simd.h and include/ziran_simd.h. */
#include "zir_emit_internal.h"
#include "zir_simd.h"
#define ZIRAN_SIMD_REF_ONLY 1
#include "ziran_simd.h"

#include <stdio.h>

/* Reinterpret, Narrow, Convert and Gather name both their operand and result
 * types; every other helper names the type it works on. */
static void
helper_name(const SimdOp *op, const char *operand, const char *result,
            char *out, size_t size)
{
    switch(op->shape) {
    case SIMD_NARROW: case SIMD_REINTERPRET: case SIMD_CONVERT: case SIMD_GATHER:
        format(out, size, "zi_%s_%s_%s", op->snake, operand, result);
        return;
    case SIMD_MAKE: case SIMD_SPLAT: case SIMD_ZERO: case SIMD_LOAD:
        format(out, size, "zi_%s_%s", op->snake, result);
        return;
    default:
        format(out, size, "zi_%s_%s", op->snake, operand);
    }
}

/* Lanes a load or store touches. */
static int
touched_lanes(const SimdOp *op, const ZirVector *vector)
{
    if(op->id == ZI_OP_LOAD_LOW || op->id == ZI_OP_STORE_LOW)
        return 64 / vector->lane_bits;
    return vector->lanes;
}

/* A slice or array operand: text read several times stays in a temporary. */
static void
memory_operand(Emitter *e, int child, int kind, char *out, size_t size)
{
    const ZirExpr *memory = &e->fn->exprs[child];
    if(kind == SIMD_MEMORY_ARRAY && (memory->kind == ZIR_EXPR_IDENT ||
                                     memory->kind == ZIR_EXPR_MEMBER ||
                                     memory->kind == ZIR_EXPR_POINTER_MEMBER ||
                                     memory->kind == ZIR_EXPR_INDEX)) {
        /* Arrays are read in place, never copied for one access. */
        emit_destination(e, child, out, size);
        return;
    }
    emit_expr(e, child, memory->type, out, size);
    if(kind == SIMD_MEMORY_SLICE && !plain_identifier(out)) {
        char temporary[ZIR_NAME_MAX];
        fresh(e, temporary);
        declare(e, temporary, memory->type, out);
        copy_text(out, size, temporary);
    }
}

/* The C address of `lanes` lanes at `index`. Slices and arrays check the
 * whole run; pointers are unchecked, as their indexing is. */
static void
c_address(Emitter *e, int memory_child, const char *index, int lanes,
          int writable, char *out, size_t size)
{
    const char *type = e->fn->exprs[memory_child].type;
    char element[ZIR_NAME_MAX], base[ZIR_TEXT_MAX];
    int capacity = 0;
    int kind = SimdMemory(type, element, sizeof(element), &capacity);
    const char *lane = TargetType(element, ZIR_C);
    unsigned bytes = ScalarByteWidth(element);
    if(lane == NULL || bytes == 0)
        fatal(&e->fn->exprs[memory_child], "SIMD memory needs scalar lanes");
    memory_operand(e, memory_child, kind, base, sizeof(base));
    if(kind == SIMD_MEMORY_POINTER) {
        format(out, size, "(%svoid *)((%s%s *)(%s) + (int64_t)(%s))",
               writable ? "" : "const ", writable ? "" : "const ", lane, base, index);
        return;
    }
    if(kind == SIMD_MEMORY_SLICE)
        format(out, size, "%szi_simd_at(%s.data, %s.length, (int64_t)(%s), %d, %u)",
               writable ? "(void *)" : "", base, base, index, lanes, bytes);
    else
        format(out, size, "%szi_simd_at(%s, %d, (int64_t)(%s), %d, %u)",
               writable ? "(void *)" : "", base, capacity, index, lanes, bytes);
}

/* Gather checks every index of a slice or array table first. */
static void
c_gather(Emitter *e, const SimdOp *op, const int *children, const char *result,
         char *out, size_t size)
{
    const char *table_type = e->fn->exprs[children[0]].type;
    const char *index_type = e->fn->exprs[children[1]].type;
    char element[ZIR_NAME_MAX], table[ZIR_TEXT_MAX], indices[ZIR_TEXT_MAX];
    char name[ZIR_NAME_MAX * 2], temporary[ZIR_NAME_MAX];
    int capacity = 0;
    int kind = SimdMemory(table_type, element, sizeof(element), &capacity);
    ZirVector index;
    VectorType(index_type, &index);
    memory_operand(e, children[0], kind, table, sizeof(table));
    emit_expr(e, children[1], index_type, indices, sizeof(indices));
    if(!plain_identifier(indices)) {
        fresh(e, temporary);
        declare(e, temporary, index_type, indices);
        copy_text(indices, sizeof(indices), temporary);
    }
    helper_name(op, index.name, result, name, sizeof(name));
    if(kind == SIMD_MEMORY_POINTER) {
        format(out, size, "%s((const void *)(%s), %s)", name, table, indices);
        return;
    }
    if(kind == SIMD_MEMORY_SLICE) {
        line(e, "zi_simd_gather_check(&%s, %d, %d, %s.length);", indices,
             index.lanes, index.is_signed, table);
        format(out, size, "%s(%s.data, %s)", name, table, indices);
    } else {
        line(e, "zi_simd_gather_check(&%s, %d, %d, %d);", indices,
             index.lanes, index.is_signed, capacity);
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
emit_simd_with_buffers(Emitter *e, const ZirExpr *expr, char *out, size_t size,
                       SimdBuffers *buffers)
{
    const SimdOp *op = SimdOpChecked(expr->name);
    int children[40], count = 0;
    for(int child = expr->first_child; child >= 0 && count < 40;
        child = e->fn->exprs[child].next_sibling)
        children[count++] = child;
    if(op == NULL) fatal(expr, "unknown SIMD operation");
    if(e->target != ZIR_C)
        fatal(expr, "SIMD operations are not supported on this target yet");

    const char *result = canonical(expr->type);
    const char *first = count > 0 ? canonical(e->fn->exprs[children[0]].type) : "";
    char (*text)[ZIR_TEXT_MAX] = buffers->text;
    char *name = buffers->name;
    int pure = 1;
    buffers->arguments[0] = '\0';

    if(op->shape == SIMD_GATHER) {
        c_gather(e, op, children, result, out, size);
        e->pure = 0;
        return 0;
    }
    if(op->shape == SIMD_LOAD || op->shape == SIMD_STORE) {
        int value = op->shape == SIMD_STORE ? 2 : -1;
        ZirVector vector;
        VectorType(value >= 0 ? e->fn->exprs[children[2]].type : result, &vector);
        emit_expr(e, children[1], e->fn->exprs[children[1]].type, buffers->index,
                  sizeof(buffers->index));
        if(value >= 0) {
            /* The value is computed before the access is checked. */
            emit_expr(e, children[2], vector.name, text[0], sizeof(text[0]));
        }
        c_address(e, children[0], buffers->index, touched_lanes(op, &vector), value >= 0,
                  buffers->address, sizeof(buffers->address));
        helper_name(op, vector.name, vector.name, name, sizeof(buffers->name));
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
    helper_name(op, op->shape == SIMD_SELECT && count > 1 ?
                canonical(e->fn->exprs[children[1]].type) : first,
                result, name, sizeof(buffers->name));
    for(int i = 0; i < count; i++) {
        const char *value = text[i];
        if(op->shape == SIMD_SELECT && i == 0) {
            /* The helpers take the mask as the unsigned vector of its shape. */
            ZirVector mask;
            VectorType(first, &mask);
            char lane[8];
            format(lane, sizeof(lane), "u%d", mask.lane_bits);
            const char *unsigned_mask = VectorTypeName(lane, mask.lanes);
            if(strcmp(first, unsigned_mask)) {
                format(buffers->piece, sizeof(buffers->piece), "zi_reinterpret_%s_%s(%s)",
                       first, unsigned_mask, value);
                value = buffers->piece;
            }
        } else if(op->shape == SIMD_SHIFT && i == 1) {
            format(buffers->piece, sizeof(buffers->piece), "(uint64_t)(%s)", value);
            value = buffers->piece;
        } else if((op->shape == SIMD_LANE || op->shape == SIMD_WITH_LANE) && i == 1) {
            format(buffers->piece, sizeof(buffers->piece), "(int)(%s)", value);
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
