#ifndef ZIR_PROOF_KERNEL_H
#define ZIR_PROOF_KERNEL_H
#include <stdint.h>

enum { PROOF_BUDGET = 100000, PROOF_DEPTH = 64, PROOF_TERMS = 4096,
       PROOF_FACTS = 128, PROOF_ORDER_TERMS = 64 };
typedef enum ProofOp {
    P_CONST, P_VAR, P_ADD, P_SUB, P_MUL, P_EQ, P_LT, P_LE,
    P_AND, P_OR, P_NOT, P_ITE
} ProofOp;
typedef struct ProofType {
    unsigned width;
    int is_unsigned;
    const void *identity; /* enum declaration, or NULL for primitives */
} ProofType;
typedef struct ProofTerm {
    ProofOp op;
    int type, a, b, c;
    uint64_t value;
} ProofTerm;
typedef struct ProofFact { int term, truth; } ProofFact;
typedef struct ProofKernel {
    ProofType types[32];
    int type_count;
    ProofTerm *terms;
    int count, capacity, budget, exhausted;
    ProofFact facts[PROOF_FACTS];
    int fact_count;
} ProofKernel;

void ProofKernelInit(ProofKernel *kernel);
void ProofKernelFree(ProofKernel *kernel);
int ProofTypeAdd(ProofKernel *kernel, ProofType type);
int ProofNode(ProofKernel *kernel, ProofOp op, int type,
              int a, int b, int c, uint64_t value);
int ProofEqual(ProofKernel *kernel, int a, int b, int ring);
/* -1 means undetermined; otherwise 0 or 1. */
int ProofTruth(ProofKernel *kernel, int term, int ring, int order);
int ProofClose(ProofKernel *kernel, int goal, int ring, int order, int cases);
int ProofRewrite(ProofKernel *kernel, int goal, int from, int to, int depth);
#endif
