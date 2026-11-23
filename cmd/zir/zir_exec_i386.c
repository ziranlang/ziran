/*
 * zir_exec_i386.c - the Linux i386 block compiler of the packed engine:
 * hot straight-line runs of a packed block become i386 code that the
 * interpreter calls for them. Fragments only see the current frame's
 * canonical scalar words. They cannot address guest memory or call anything.
 * Edges, pointers, calls and failures stay in the interpreter. Elsewhere
 * ExecI386Open gives NULL and the engine interprets every block. Compiled
 * under the engine's private names with its generated headers.
 */
#include <stddef.h>
#include <stdint.h>

#ifndef ZIRAN_NO_ENGINE
#include "compiler_exec.h"
#include "compiler_lowered_ir.h"
#endif

#if defined(ZIRAN_NO_ENGINE)

/* Bootstrap builds have no instruction engine, so no block compiler. */
void *ExecI386Open(const void *instance) { (void)instance; return NULL; }
int ExecI386RunEntry(void *instance, void *ran, void *cache)
{
    (void)instance; (void)ran; (void)cache;
    return 0;
}
void ExecI386Close(void *cache) { (void)cache; }

#elif defined(__linux__) && defined(__i386__) && !defined(__ANDROID__) && !defined(ZIRAN_EXEC_NO_I386_BLOCKS)

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "zir_native_memory.h"

enum { I386Slots = 16, I386Heat = 4, I386CompileLimit = 64, I386Scan = 128, I386Ops = 64, I386Bytes = 4096 };

typedef uint32_t (*ExecI386Code)(uint64_t *words);

typedef struct ExecI386Block {
    size_t key;
    unsigned visits;
    int32_t start;
    uint32_t count;
    int64_t fuel[I386Ops + 1];
    ZirNativeMapping mapping;
    ExecI386Code code;
} ExecI386Block;

typedef struct ExecI386Cache {
    unsigned compiled;
    ExecI386Block blocks[I386Slots];
} ExecI386Cache;

typedef struct ExecI386Buffer {
    unsigned char bytes[I386Bytes];
    size_t size;
    bool full;
} ExecI386Buffer;

static void
i386_append(ExecI386Buffer *out, const unsigned char *bytes, size_t count)
{
    if(out->full || count > sizeof(out->bytes) - out->size) {
        out->full = true;
        return;
    }
    memcpy(out->bytes + out->size, bytes, count);
    out->size += count;
}

#define I386Emit(out, ...) i386_append(out, (const unsigned char[]){__VA_ARGS__}, \
                                       sizeof((const unsigned char[]){__VA_ARGS__}))

static void
i386_immediate(ExecI386Buffer *out, uint32_t value)
{
    I386Emit(out, (unsigned char)value, (unsigned char)(value >> 8),
                  (unsigned char)(value >> 16), (unsigned char)(value >> 24));
}

static void
i386_return(ExecI386Buffer *out, uint32_t completed)
{
    I386Emit(out, 0xb8);                 /* mov eax, completed */
    i386_immediate(out, completed);
    I386Emit(out, 0x5b, 0xc3);           /* pop ebx; ret */
}

static void
i386_guard(ExecI386Buffer *out, unsigned char condition, uint32_t completed)
{
    I386Emit(out, condition, 7);        /* jump over i386_return on success */
    i386_return(out, completed);
}

static void
i386_extend(ExecI386Buffer *out, unsigned width, bool ecx)
{
    if(width == 8 || width == 16)
        I386Emit(out, 0x0f, width == 8 ? 0xbe : 0xbf, ecx ? 0xc9 : 0xc0);
}

static const LirType *
i386_word(const LirExecPlan *plan, const LirExecFrame *frame,
          const LirExecFunctionPlan *function, int32_t value, uint32_t *offset)
{
    if(value < 0 || value >= frame->value_types.length || frame->value_base < 0 ||
       frame->value_base > INT64_MAX - value) return NULL;
    int32_t type = ((const int32_t *)frame->value_types.data)[value];
    if(type < 0 || type >= plan->program.types.length) return NULL;
    const LirType *shape = &((const LirType *)plan->program.types.data)[type];
    if(shape->kind != LirTypeKind_Bool && (shape->kind != LirTypeKind_Integer ||
       (shape->bits != 8 && shape->bits != 16 && shape->bits != 32))) return NULL;
    int64_t index = frame->value_base + value;
    int64_t word;
    if(plan->narrow_value_locations.count != 0) {
        if(index >= plan->narrow_value_locations.count) return NULL;
        word = plan->narrow_value_locations.data[index];
    } else {
        if(index >= plan->value_locations.count) return NULL;
        word = plan->value_locations.data[index];
    }
    if(word < 0 || word >= function->word_count || word > (INT32_MAX - 7) / 8) return NULL;
    *offset = (uint32_t)word * 8;
    return shape;
}

static bool
i386_instruction(ExecI386Buffer *out, const LirExecPlan *plan, const LirExecFrame *frame,
                 const LirExecFunctionPlan *function, const LirExecPackedInstruction *ins,
                 uint32_t completed)
{
    LirOp op = ins->op;
    unsigned operands;
    switch(op) {
    case LirOp_Constant: operands = 0; break;
    case LirOp_Negate: case LirOp_BitNot: case LirOp_Convert: case LirOp_Bitcast:
        operands = 1; break;
    case LirOp_Add: case LirOp_Subtract: case LirOp_Multiply:
    case LirOp_BitAnd: case LirOp_BitOr: case LirOp_BitXor:
    case LirOp_Divide: case LirOp_Remainder: case LirOp_ShiftLeft: case LirOp_ShiftRight:
    case LirOp_Equal: case LirOp_NotEqual: case LirOp_Less: case LirOp_LessEqual:
    case LirOp_Greater: case LirOp_GreaterEqual:
        operands = 2; break;
    default: return false;
    }
    uint32_t dst, lhs, rhs;
    const LirType *result = i386_word(plan, frame, function, ins->result, &dst);
    if(result == NULL || ins->operand_count != operands) return false;
    if(op == LirOp_Constant) {
        if(ins->symbol < 0 || ins->symbol >= plan->program.constants.length) return false;
        const LirConstant *constant = &((const LirConstant *)plan->program.constants.data)[ins->symbol];
        I386Emit(out, 0xb8);             /* mov eax, low constant bits */
        i386_immediate(out, result->kind == LirTypeKind_Bool ? constant->bits != 0 : (uint32_t)constant->bits);
    } else {
        if(ins->operand_data == NULL) return false;
        const LirType *left = i386_word(plan, frame, function, ins->operand_data[0], &lhs);
        if(left == NULL) return false;
        I386Emit(out, 0x8b, 0x83);       /* mov eax, [ebx + lhs] */
        i386_immediate(out, lhs);
        if(operands == 2) {
            if(i386_word(plan, frame, function, ins->operand_data[1], &rhs) == NULL) return false;
            I386Emit(out, 0x8b, 0x8b);   /* mov ecx, [ebx + rhs] */
            i386_immediate(out, rhs);
        }
        switch(op) {
        case LirOp_Add:      I386Emit(out, 0x01, 0xc8); break;
        case LirOp_Subtract: I386Emit(out, 0x29, 0xc8); break;
        case LirOp_Multiply: I386Emit(out, 0x0f, 0xaf, 0xc1); break;
        case LirOp_BitAnd:   I386Emit(out, 0x21, 0xc8); break;
        case LirOp_BitOr:    I386Emit(out, 0x09, 0xc8); break;
        case LirOp_BitXor:   I386Emit(out, 0x31, 0xc8); break;
        case LirOp_Negate: case LirOp_BitNot:
            if(left->kind != LirTypeKind_Integer) return false;
            I386Emit(out, 0xf7, op == LirOp_Negate ? 0xd8 : 0xd0);
            break;
        case LirOp_Divide: case LirOp_Remainder:
            if(left->is_signed) {
                i386_extend(out, left->bits, false);
                i386_extend(out, left->bits, true);
            }
            I386Emit(out, 0x85, 0xc9);   /* test ecx, ecx */
            i386_guard(out, 0x75, completed);
            if(left->is_signed) {
                if(left->bits == 32) {
                    /* Let the interpreter handle MIN / -1 without #DE. */
                    I386Emit(out, 0x83, 0xf9, 0xff);
                    i386_guard(out, 0x75, completed);
                }
                I386Emit(out, 0x99, 0xf7, 0xf9);  /* cdq; idiv ecx */
            } else I386Emit(out, 0x31, 0xd2, 0xf7, 0xf1);  /* xor edx, edx; div ecx */
            if(op == LirOp_Remainder) I386Emit(out, 0x89, 0xd0);
            break;
        case LirOp_ShiftLeft: case LirOp_ShiftRight:
            I386Emit(out, 0x83, 0xf9, (unsigned char)left->bits);
            i386_guard(out, 0x72, completed);      /* unsigned count < width */
            if(op == LirOp_ShiftLeft) I386Emit(out, 0xd3, 0xe0);
            else if(left->is_signed) {
                i386_extend(out, left->bits, false);
                I386Emit(out, 0xd3, 0xf8);
            } else I386Emit(out, 0xd3, 0xe8);
            break;
        case LirOp_Equal: case LirOp_NotEqual: case LirOp_Less: case LirOp_LessEqual:
        case LirOp_Greater: case LirOp_GreaterEqual: {
            static const unsigned char unsigned_condition[] = {0x94, 0x95, 0x92, 0x96, 0x97, 0x93};
            static const unsigned char signed_condition[] = {0x94, 0x95, 0x9c, 0x9e, 0x9f, 0x9d};
            bool sign = left->kind == LirTypeKind_Integer && left->is_signed;
            const unsigned char *condition = sign ? signed_condition : unsigned_condition;
            if(sign) {
                i386_extend(out, left->bits, false);
                i386_extend(out, left->bits, true);
            }
            I386Emit(out, 0x39, 0xc8, 0x0f, condition[op - LirOp_Equal], 0xc0, 0x0f, 0xb6, 0xc0);
            break;
        }
        case LirOp_Convert:
            if(result->kind == LirTypeKind_Bool) {
                I386Emit(out, 0x85, 0xc0, 0x0f, 0x95, 0xc0, 0x0f, 0xb6, 0xc0);
            } else if(left->kind == LirTypeKind_Integer && left->is_signed) i386_extend(out, left->bits, false);
            break;
        case LirOp_Bitcast:
            if(result->kind == LirTypeKind_Bool || left->kind == LirTypeKind_Bool) return false;
            break;
        default: return false;
        }
    }
    /* Commit the whole canonical word, even if this slot used to hold a
     * wider value. The decoder never supplies displacements. */
    if(result->kind == LirTypeKind_Integer && result->bits != 32) {
        I386Emit(out, 0x25);             /* and eax, mask */
        i386_immediate(out, (UINT32_C(1) << result->bits) - 1);
    }
    I386Emit(out, 0x89, 0x83);           /* mov [ebx + dst], eax */
    i386_immediate(out, dst);
    I386Emit(out, 0xc7, 0x83);           /* mov dword [ebx + dst + 4], 0 */
    i386_immediate(out, dst + 4);
    i386_immediate(out, 0);
    return !out->full;
}

static void
i386_compile(ExecI386Block *block, const LirExecPlan *plan, const LirExecFrame *frame,
             const LirExecFunctionPlan *function, size_t end)
{
    ExecI386Buffer out = {0};
    /* ENDBR32 for indirect entry on CET hosts. SSE2-capable hosts also
     * accept this instruction when indirect branch tracking is disabled. */
    I386Emit(&out, 0xf3, 0x0f, 0x1e, 0xfb);
    I386Emit(&out, 0x53, 0x8b, 0x5c, 0x24, 0x08);  /* push ebx; mov ebx, [esp + 8] */
    const LirExecPackedInstruction *packed = plan->packed_instructions.data;
    size_t begin = block->key - 1;
    size_t prelude = out.size;
    uint32_t count = 0, arithmetic = 0;
    for(size_t at = begin; at < end && at - (block->key - 1) < I386Scan; at++) {
        const LirExecPackedInstruction *ins = &packed[at];
        int64_t fuel = block->fuel[count];
        size_t before = out.size;
        bool supported = count < I386Ops;
        if(ins->op == LirOp_Fuel) {
            if(ins->immediate > (uint64_t)(INT64_MAX - fuel)) supported = false;
            else fuel += (int64_t)ins->immediate;
        } else if(supported) {
            supported = i386_instruction(&out, plan, frame, function, ins, count) &&
                out.size <= sizeof(out.bytes) - 7;
        }
        if(!supported) {
            out.full = false;
            if(arithmetic >= 3) { out.size = before; break; }
            out.size = prelude;
            begin = at + 1;
            count = arithmetic = 0;
            continue;
        }
        arithmetic += ins->op != LirOp_Constant && ins->op != LirOp_Fuel;
        block->fuel[++count] = fuel;
    }
    if(arithmetic < 3) return;
    i386_return(&out, count);
    if(out.full || !ZirNativeReserve(out.size, &block->mapping)) return;
    memcpy(block->mapping.address, out.bytes, out.size);
    if(!ZirNativeFlushInstructions(&block->mapping) || !ZirNativeProtectRX(&block->mapping)) {
        ZirNativeRelease(&block->mapping);
        return;
    }
    block->start = (int32_t)(begin - (block->key - 1));
    block->count = count;
    block->code = (ExecI386Code)block->mapping.address;
}

/* Return the next instruction at which the interpreter should consult us. */
static int32_t
i386_run_block(void *context, LirExecInstance *instance, LirExecFrame *frame)
{
    ExecI386Cache *cache = context;
    const LirExecPlan *plan = instance->plan;
    size_t key = (size_t)frame->instruction_base + 1;
    ExecI386Block *block = &cache->blocks[((uint32_t)key * UINT32_C(2654435761)) >> 28];
    if(block->key != key) {
        if(frame->instruction != 0 || cache->compiled >= I386CompileLimit) return -1;
        ZirNativeRelease(&block->mapping);
        *block = (ExecI386Block){.key = key};
    }
    if(block->code == NULL) {
        if(frame->instruction != 0 || block->visits == I386Heat ||
           ++block->visits < I386Heat || cache->compiled >= I386CompileLimit) return -1;
        cache->compiled++;
        if(key == 0 || key > (size_t)plan->packed_instructions.length) return -1;
        const LirExecFunctionPlan *function = &plan->functions.data[frame->function];
        size_t next = (size_t)(function->block_base + frame->block) + 1;
        size_t end = next < plan->block_instruction_bases.count ?
            plan->block_instruction_bases.data[next] : (size_t)plan->packed_instructions.length;
        if(end <= key || end > (size_t)plan->packed_instructions.length) return -1;
        /* Leave at least the terminator to the interpreter. */
        i386_compile(block, plan, frame, function, end - 1);
    }
    if(block->code == NULL) return -1;
    if(frame->instruction == 0 && block->start > 0) return block->start;
    if(frame->instruction != block->start || instance->work_remaining < block->count ||
       instance->fuel_remaining < block->fuel[block->count]) return -1;
    const LirExecFunctionPlan *function = &plan->functions.data[frame->function];
    if(frame->word_base < 0 || frame->word_base > instance->words.count || function->word_count <= 0 ||
       function->word_count > instance->words.count - frame->word_base) return -1;
    uint32_t completed = block->code(instance->words.data + frame->word_base);
    instance->work_remaining -= completed;
    instance->fuel_remaining -= block->fuel[completed];
    frame->instruction += (int32_t)completed;
    return -1;
}

static ExecI386Cache *
i386_open(const LirExecInstance *instance)
{
    if(instance->plan->packed_instructions.length == 0 || ZirNativePageSize() != I386Bytes ||
       !__builtin_cpu_supports("sse2")) return NULL;
    return calloc(1, sizeof(ExecI386Cache));
}

static void
i386_close(ExecI386Cache *cache)
{
    if(cache == NULL) return;
    for(size_t slot = 0; slot < I386Slots; slot++) ZirNativeRelease(&cache->blocks[slot].mapping);
    free(cache);
}

#undef I386Emit

void *
ExecI386Open(const LirExecInstance *instance)
{
    return i386_open(instance);
}

int
ExecI386RunEntry(LirExecInstance *instance, LirExecResult *ran, void *cache)
{
    LirExecPackedBlocks blocks = {.run = i386_run_block, .context = cache};
    return compiler_exec_LirExecRunEntryWithPackedBlocks(instance, ran, cache != NULL ? &blocks : NULL);
}

void
ExecI386Close(void *cache)
{
    i386_close(cache);
}

#else

void *ExecI386Open(const LirExecInstance *instance) { (void)instance; return NULL; }
int ExecI386RunEntry(LirExecInstance *instance, LirExecResult *ran, void *cache)
{
    (void)cache;
    return compiler_exec_LirExecRunEntry(instance, ran);
}
void ExecI386Close(void *cache) { (void)cache; }

#endif
