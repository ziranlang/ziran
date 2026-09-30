#include "zir_proof_kernel.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Independent exhaustive interpretation checks every accepted generated claim
 * at a small width. The same corpus is consumed by the Lean interpreter. */
static unsigned random_state = 104729;
static unsigned random_number(void)
{
    random_state ^= random_state << 13; random_state ^= random_state >> 17;
    random_state ^= random_state << 5; return random_state;
}
static int word(ProofKernel *k, int depth)
{
    if(!depth) return random_number() & 1 ? ProofNode(k, P_VAR, 1, -1,-1,-1,random_number()%2) :
                                         ProofNode(k, P_CONST,1,-1,-1,-1,random_number()%16);
    int a = word(k, depth-1), b = word(k, depth-1);
    return ProofNode(k, (ProofOp)(P_ADD + random_number()%3), 1, a,b,-1,0);
}
static unsigned evaluate(const ProofKernel *k, int id, unsigned x, unsigned y)
{
    ProofTerm t = k->terms[id];
    if(t.op == P_CONST) return (unsigned)t.value;
    if(t.op == P_VAR) return t.value ? y : x;
    unsigned a = evaluate(k,t.a,x,y), b = t.b >= 0 ? evaluate(k,t.b,x,y) : 0;
    unsigned c = t.c >= 0 ? evaluate(k,t.c,x,y) : 0;
    int sa = a >= 8 ? (int)a-16 : (int)a, sb = b >= 8 ? (int)b-16 : (int)b;
    int signed_order = k->types[1].is_unsigned == 0;
    switch(t.op) {
    case P_ADD: return (a+b)&15;
    case P_SUB: return (a-b)&15;
    case P_MUL: return (a*b)&15;
    case P_EQ: return a == b;
    case P_LT: return signed_order ? sa < sb : a < b;
    case P_LE: return signed_order ? sa <= sb : a <= b;
    case P_AND: return a && b;
    case P_OR: return a || b;
    case P_NOT: return !a;
    case P_ITE: return a ? b : c;
    default: abort();
    }
}
static int binary(ProofKernel *k, ProofOp op, int type, int a, int b)
{ return ProofNode(k,op,type,a,b,-1,0); }
int main(int argc, char **argv)
{
    FILE *out = argc > 1 ? fopen(argv[1],"w") : NULL;
    assert(argc == 1 || out);
    int accepted = 0, rejected = 0;
    for(int sign = 0; sign < 2; sign++) for(int i = 0; i < 160; i++) {
        ProofKernel k; ProofKernelInit(&k);
        assert(ProofTypeAdd(&k,(ProofType){4,!sign,NULL}) == 1);
        int a = word(&k,2), b = word(&k,2), c = word(&k,1);
        int lhs, rhs, goal;
        switch(i%5) {
        case 0:
            lhs=binary(&k,P_ADD,1,a,b); rhs=binary(&k,P_ADD,1,b,a); break;
        case 1:
            lhs=binary(&k,P_MUL,1,a,binary(&k,P_ADD,1,b,c));
            rhs=binary(&k,P_ADD,1,binary(&k,P_MUL,1,a,b),binary(&k,P_MUL,1,a,c)); break;
        case 2:
            lhs=ProofNode(&k,P_ITE,1,binary(&k,P_LT,0,a,b),b,a,0);
            rhs=ProofNode(&k,P_ITE,1,binary(&k,P_LT,0,b,a),a,b,0); break;
        case 3:
            lhs=binary(&k,P_SUB,1,a,a); rhs=ProofNode(&k,P_CONST,1,-1,-1,-1,0); break;
        default:
            lhs=a; rhs=word(&k,2); break;
        }
        if(i%3 == 1) rhs=binary(&k,P_ADD,1,rhs,ProofNode(&k,P_CONST,1,-1,-1,-1,1));
        goal=binary(&k,P_EQ,0,lhs,rhs);
        int proved=ProofClose(&k,goal,1,1,1);
        if(proved) {
            accepted++;
            for(unsigned x=0;x<16;x++) for(unsigned y=0;y<16;y++)
                assert(evaluate(&k,goal,x,y));
        } else rejected++;
        if(out) {
            fprintf(out,"G %d %d %d %d\n",sign,goal,k.count,proved);
            for(int t=0;t<k.count;t++) {
                ProofTerm v=k.terms[t];
                fprintf(out,"%d %d %d %d %d %llu\n",v.op,v.type,v.a,v.b,v.c,(unsigned long long)v.value);
            }
        }
        ProofKernelFree(&k);
    }
    assert(accepted > 100 && rejected > 100);
    ProofKernel k; ProofKernelInit(&k);
    int yes=ProofNode(&k,P_CONST,0,-1,-1,-1,1); k.budget=0;
    assert(!ProofClose(&k,yes,1,1,1) && k.exhausted);
    ProofKernelFree(&k);
    if(out) fclose(out);
    printf("scalar kernel: %d accepted claims exhaustively checked, %d rejected\n",accepted,rejected);
    return 0;
}
