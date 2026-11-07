#include "compiler_proof_kernel.h"
#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The kernel is generated from cmd/compiler_proof_kernel.zi. Two checks:
 * an independent exhaustive interpretation of every accepted claim at a small
 * width (the corpus is also consumed by the Lean oracle), and a trace of
 * every public result, term table and remaining budget whose hash must equal
 * the trace of the C kernel it replaced. */
static unsigned random_state;
static unsigned random_number(void)
{
    random_state ^= random_state << 13; random_state ^= random_state >> 17;
    random_state ^= random_state << 5; return random_state;
}
static int node(ProofKernel *k, ProofOp op, int type, int a, int b, int c, uint64_t value)
{
    return compiler_proof_kernel_Node(k, op, type, a, b, c, value);
}
static int binary(ProofKernel *k, ProofOp op, int type, int a, int b)
{
    return node(k, op, type, a, b, -1, 0);
}
static int leaf(ProofKernel *k, ProofOp op, int type, uint64_t value)
{
    return node(k, op, type, -1, -1, -1, value);
}
static int add_type(ProofKernel *k, unsigned width, int is_unsigned, uint64_t identity)
{
    return compiler_proof_kernel_TypeAdd(k, (ProofType){width, is_unsigned, identity});
}
static ProofKernel *kernel_new(void)
{
    ProofKernel *k = malloc(sizeof(*k));
    assert(k);
    compiler_proof_kernel_Init(k);
    return k;
}

static int small_word(ProofKernel *k, int depth)
{
    if(!depth) return random_number() & 1 ? leaf(k, ProofOp_Var, 1, random_number() % 2) :
                                         leaf(k, ProofOp_Const, 1, random_number() % 16);
    int a = small_word(k, depth - 1), b = small_word(k, depth - 1);
    return binary(k, (ProofOp)(ProofOp_Add + random_number() % 3), 1, a, b);
}
static unsigned evaluate(const ProofKernel *k, int id, unsigned x, unsigned y)
{
    ProofTerm t = k->terms[id];
    if(t.op == ProofOp_Const) return (unsigned)t.value;
    if(t.op == ProofOp_Var) return t.value ? y : x;
    unsigned a = evaluate(k, t.a, x, y), b = t.b >= 0 ? evaluate(k, t.b, x, y) : 0;
    unsigned c = t.c >= 0 ? evaluate(k, t.c, x, y) : 0;
    int sa = a >= 8 ? (int)a - 16 : (int)a, sb = b >= 8 ? (int)b - 16 : (int)b;
    int signed_order = !k->types[1].is_unsigned;
    switch(t.op) {
    case ProofOp_Add: return (a + b) & 15;
    case ProofOp_Sub: return (a - b) & 15;
    case ProofOp_Mul: return (a * b) & 15;
    case ProofOp_Eq: return a == b;
    case ProofOp_Lt: return signed_order ? sa < sb : a < b;
    case ProofOp_Le: return signed_order ? sa <= sb : a <= b;
    case ProofOp_And: return a && b;
    case ProofOp_Or: return a || b;
    case ProofOp_Not: return !a;
    case ProofOp_Ite: return a ? b : c;
    default: abort();
    }
}
static void exhaustive(FILE *out)
{
    int accepted = 0, rejected = 0;
    random_state = 104729;
    for(int sign = 0; sign < 2; sign++) for(int i = 0; i < 160; i++) {
        ProofKernel *k = kernel_new();
        assert(add_type(k, 4, !sign, 0) == 1);
        int a = small_word(k, 2), b = small_word(k, 2), c = small_word(k, 1);
        int lhs, rhs;
        switch(i % 5) {
        case 0:
            lhs = binary(k, ProofOp_Add, 1, a, b); rhs = binary(k, ProofOp_Add, 1, b, a); break;
        case 1:
        {
            lhs = binary(k, ProofOp_Mul, 1, a, binary(k, ProofOp_Add, 1, b, c));
            int ab = binary(k, ProofOp_Mul, 1, a, b), ac = binary(k, ProofOp_Mul, 1, a, c);
            rhs = binary(k, ProofOp_Add, 1, ab, ac); break;
        }
        case 2:
            lhs = node(k, ProofOp_Ite, 1, binary(k, ProofOp_Lt, 0, a, b), b, a, 0);
            rhs = node(k, ProofOp_Ite, 1, binary(k, ProofOp_Lt, 0, b, a), a, b, 0); break;
        case 3:
            lhs = binary(k, ProofOp_Sub, 1, a, a); rhs = leaf(k, ProofOp_Const, 1, 0); break;
        default:
            lhs = a; rhs = small_word(k, 2); break;
        }
        if(i % 3 == 1) rhs = binary(k, ProofOp_Add, 1, rhs, leaf(k, ProofOp_Const, 1, 1));
        int goal = binary(k, ProofOp_Eq, 0, lhs, rhs);
        int proved = compiler_proof_kernel_Close(k, goal, 1, 1, 1);
        if(proved) {
            accepted++;
            for(unsigned x = 0; x < 16; x++) for(unsigned y = 0; y < 16; y++)
                assert(evaluate(k, goal, x, y));
        } else rejected++;
        if(out) {
            fprintf(out, "G %d %d %d %d\n", sign, goal, k->count, proved);
            for(int t = 0; t < k->count; t++) {
                ProofTerm v = k->terms[t];
                fprintf(out, "%d %d %d %d %d %llu\n", (int)v.op, v.type, v.a, v.b, v.c, (unsigned long long)v.value);
            }
        }
        free(k);
    }
    assert(accepted > 100 && rejected > 100);
    printf("scalar kernel: %d accepted claims exhaustively checked, %d rejected\n", accepted, rejected);
}

/* The trace hashes with 64-bit FNV-1a; --trace FILE also writes it out. */
static uint64_t trace_hash = UINT64_C(14695981039346656037);
static FILE *trace_file;
static void trace(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    assert(length >= 0 && (size_t)length < sizeof(line));
    for(int i = 0; i < length; i++)
        trace_hash = (trace_hash ^ (unsigned char)line[i]) * UINT64_C(1099511628211);
    if(trace_file) fputs(line, trace_file);
}
static int word(ProofKernel *k, int type, int depth);
static int condition(ProofKernel *k, int type, int depth)
{
    unsigned pick = random_number() % (depth ? 8 : 5);
    if(pick == 0) return leaf(k, ProofOp_Const, 0, random_number() & 1);
    if(pick == 1) return leaf(k, ProofOp_Var, 0, 8 + random_number() % 2);
    if(pick <= 4) {
        int a = word(k, type, depth ? depth - 1 : 0), b = word(k, type, depth ? depth - 1 : 0);
        return binary(k, pick == 2 ? ProofOp_Eq : pick == 3 ? ProofOp_Lt : ProofOp_Le, 0, a, b);
    }
    int a = condition(k, type, depth - 1);
    if(pick == 5) return binary(k, ProofOp_Not, 0, a, -1);
    int b = condition(k, type, depth - 1);
    return binary(k, pick == 6 ? ProofOp_And : pick == 7 ? ProofOp_Or : ProofOp_Eq, 0, a, b);
}
static int word(ProofKernel *k, int type, int depth)
{
    unsigned pick = random_number() % (depth ? 6 : 2);
    if(pick == 0) return leaf(k, ProofOp_Var, type, random_number() % 3);
    if(pick == 1) return leaf(k, ProofOp_Const, type, (uint64_t)random_number() * random_number());
    if(pick == 5) {
        int c = condition(k, type, depth - 1);
        int a = word(k, type, depth - 1), b = word(k, type, depth - 1);
        return node(k, ProofOp_Ite, type, c, a, b, 0);
    }
    int a = word(k, type, depth - 1), b = word(k, type, depth - 1);
    return binary(k, pick == 2 ? ProofOp_Add : pick == 3 ? ProofOp_Sub : ProofOp_Mul, type, a, b);
}
static void trace_terms(const ProofKernel *k)
{
    for(int t = 0; t < k->count; t++) {
        ProofTerm v = k->terms[t];
        trace(" %d:%d,%d,%d,%d,%d,%llu", t, (int)v.op, v.type, v.a, v.b, v.c, (unsigned long long)v.value);
    }
    trace("\n");
}
static void report(const ProofKernel *k, const char *what, int value)
{
    trace("%s=%d count=%d budget=%d exhausted=%d facts=%d\n", what, value, k->count,
          k->budget, (int)k->exhausted, k->fact_count);
}
static void golden_trace(void)
{
    static const unsigned widths[] = {4, 8, 16, 32, 64};
    random_state = 7919;
    for(int round = 0; round < 400; round++) {
        ProofKernel *k = kernel_new();
        unsigned width = widths[round % 5];
        int is_unsigned = (round / 5) % 2;
        int type = add_type(k, width, is_unsigned, round % 3 == 0 ? 0 : 1 + round % 2);
        int other = add_type(k, width, is_unsigned, 0);
        trace("round %d type=%d other=%d\n", round, type, other);
        int depth = 1 + round % 3;
        int a = word(k, type, depth), b = word(k, type, depth), c = word(k, type, 1);
        int lhs, rhs, goal;
        switch(round % 7) {
        case 0:
            lhs = binary(k, ProofOp_Add, type, a, b); rhs = binary(k, ProofOp_Add, type, b, a); break;
        case 1:
        {
            lhs = binary(k, ProofOp_Mul, type, a, binary(k, ProofOp_Sub, type, b, c));
            int ab = binary(k, ProofOp_Mul, type, a, b), ca = binary(k, ProofOp_Mul, type, c, a);
            rhs = binary(k, ProofOp_Sub, type, ab, ca); break;
        }
        case 2:
            lhs = node(k, ProofOp_Ite, type, binary(k, ProofOp_Lt, 0, a, b), b, a, 0);
            rhs = node(k, ProofOp_Ite, type, binary(k, ProofOp_Le, 0, b, a), a, b, 0); break;
        case 3:
            lhs = binary(k, ProofOp_Sub, type, a, a); rhs = leaf(k, ProofOp_Const, type, 0); break;
        default:
            lhs = a; rhs = b; break;
        }
        if(round % 4 == 1) rhs = binary(k, ProofOp_Add, type, rhs, leaf(k, ProofOp_Const, type, 1));
        goal = round % 7 >= 5 ? condition(k, type, 2) : binary(k, ProofOp_Eq, 0, lhs, rhs);
        if(round % 11 == 0) {
            int premise = condition(k, type, 1);
            goal = binary(k, ProofOp_Or, 0, binary(k, ProofOp_Not, 0, premise, -1), goal);
        }
        trace("goal=%d lhs=%d rhs=%d\n", goal, lhs, rhs);
        trace_terms(k);
        report(k, "equal-ring", compiler_proof_kernel_Equal(k, lhs, rhs, 1));
        report(k, "equal-plain", compiler_proof_kernel_Equal(k, lhs, rhs, 0));
        report(k, "rewrite", compiler_proof_kernel_Rewrite(k, goal, lhs, rhs, 0));
        for(int flags = 0; flags < 8; flags++) {
            char name[32];
            snprintf(name, sizeof(name), "close-%d%d%d", flags & 1, (flags >> 1) & 1, (flags >> 2) & 1);
            report(k, name, compiler_proof_kernel_Close(k, goal, flags & 1, (flags >> 1) & 1, (flags >> 2) & 1));
        }
        trace_terms(k);
        free(k);
    }

    /* Limits: types, terms, polynomial degree and budget. */
    ProofKernel *k = kernel_new();
    for(int i = 0; i < 40; i++) trace("type %d -> %d\n", i, add_type(k, 8, i % 2, (uint64_t)i));
    report(k, "types", k->type_count);
    compiler_proof_kernel_Init(k);
    int t32 = add_type(k, 32, 1, 0), last = -2;
    for(int i = 0; i < 5000 && last != -1; i++)
        last = leaf(k, ProofOp_Const, t32, (uint64_t)i << 8 | (unsigned)i);
    report(k, "terms", last);
    compiler_proof_kernel_Init(k);
    t32 = add_type(k, 32, 0, 0);
    int x = leaf(k, ProofOp_Var, t32, 0), y = leaf(k, ProofOp_Var, t32, 1);
    int p = binary(k, ProofOp_Add, t32, x, y), q = binary(k, ProofOp_Add, t32, y, x);
    for(int i = 0; i < 7; i++) {
        p = binary(k, ProofOp_Mul, t32, p, p); q = binary(k, ProofOp_Mul, t32, q, q);
        report(k, "power", compiler_proof_kernel_Equal(k, p, q, 1));
    }
    compiler_proof_kernel_Init(k);
    int yes = leaf(k, ProofOp_Const, 0, 1);
    k->budget = 0;
    report(k, "budget", compiler_proof_kernel_Close(k, yes, 1, 1, 1));
    assert(k->exhausted);
    free(k);
}

int main(int argc, char **argv)
{
    FILE *corpus = NULL;
    for(int i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "--trace") && i + 1 < argc) trace_file = fopen(argv[++i], "w");
        else corpus = fopen(argv[i], "w");
        assert(trace_file || corpus);
    }
    exhaustive(corpus);
    golden_trace();
    if(corpus) fclose(corpus);
    if(trace_file) fclose(trace_file);
    /* Trace of the C kernel this module replaced, 2026-11-07. */
    const uint64_t expected = UINT64_C(0x1d9b8ae613078bdb);
    if(trace_hash != expected) {
        fprintf(stderr, "kernel trace %016" PRIx64 " differs from %016" PRIx64 "\n", trace_hash, expected);
        return 1;
    }
    return 0;
}
