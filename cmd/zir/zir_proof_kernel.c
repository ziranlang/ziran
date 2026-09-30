#include "zir_proof_kernel.h"
#include "zir_scalar.h"
#include <stdlib.h>
#include <string.h>

static int spend(ProofKernel *k)
{
    if(--k->budget >= 0 && !k->exhausted) return 1;
    k->exhausted = 1;
    return 0;
}

void ProofKernelInit(ProofKernel *k)
{
    memset(k, 0, sizeof(*k));
    k->budget = PROOF_BUDGET;
    k->type_count = 1; /* type zero is Bool */
}
void ProofKernelFree(ProofKernel *k) { free(k->terms); }

int ProofTypeAdd(ProofKernel *k, ProofType type)
{
    for(int i = 0; i < k->type_count; i++)
        if(k->types[i].width == type.width &&
           k->types[i].is_unsigned == type.is_unsigned &&
           k->types[i].identity == type.identity) return i;
    if(k->type_count == 32) { k->exhausted = 1; return -1; }
    k->types[k->type_count] = type;
    return k->type_count++;
}

int ProofNode(ProofKernel *k, ProofOp op, int type,
              int a, int b, int c, uint64_t value)
{
    if(!spend(k) || type < 0 || type >= k->type_count) return -1;
    if(op == P_CONST) value &= type ? ScalarMask(k->types[type].width) : 1;
    if(op == P_ITE && b == c) return b;
    for(int i = 0; i < k->count; i++) {
        ProofTerm *t = &k->terms[i];
        if(t->op == op && t->type == type && t->a == a && t->b == b &&
           t->c == c && t->value == value) return i;
    }
    if(k->count == PROOF_TERMS) { k->exhausted = 1; return -1; }
    if(k->count == k->capacity) {
        int cap = k->capacity ? k->capacity * 2 : 64;
        ProofTerm *terms = realloc(k->terms, (size_t)cap * sizeof(*terms));
        if(!terms) { k->exhausted = 1; return -1; }
        k->terms = terms;
        k->capacity = cap;
    }
    k->terms[k->count] = (ProofTerm){op, type, a, b, c, value};
    return k->count++;
}

/* A monomial is a sorted multiset of atom IDs and a coefficient modulo 2^w.
 * ITEs are atoms until cases selects a branch. No order identities about
 * modular arithmetic are assumed. */
typedef struct Monomial {
    uint64_t coefficient;
    unsigned degree;
    int factors[PROOF_DEPTH];
} Monomial;
typedef struct Polynomial { Monomial *terms; int count, cap; } Polynomial;

static void poly_free(Polynomial *p) { free(p->terms); memset(p, 0, sizeof(*p)); }
static int poly_add(ProofKernel *k, Polynomial *p, const Monomial *m, uint64_t mask)
{
    if(!spend(k)) return 0;
    uint64_t coefficient = m->coefficient & mask;
    if(!coefficient) return 1;
    for(int i = 0; i < p->count; i++) {
        if(!spend(k)) return 0;
        Monomial *n = &p->terms[i];
        if(n->degree == m->degree &&
           !memcmp(n->factors, m->factors, m->degree * sizeof(int))) {
            n->coefficient = (n->coefficient + coefficient) & mask;
            if(!n->coefficient) p->terms[i] = p->terms[--p->count];
            return 1;
        }
    }
    if(p->count == PROOF_TERMS) { k->exhausted = 1; return 0; }
    if(p->count == p->cap) {
        int cap = p->cap ? p->cap * 2 : 8;
        Monomial *terms = realloc(p->terms, (size_t)cap * sizeof(*terms));
        if(!terms) { k->exhausted = 1; return 0; }
        p->terms = terms; p->cap = cap;
    }
    p->terms[p->count] = *m;
    p->terms[p->count++].coefficient = coefficient;
    return 1;
}

static int polynomial(ProofKernel *k, int id, Polynomial *out, int depth)
{
    if(id < 0 || id >= k->count || depth > PROOF_DEPTH || !spend(k)) return 0;
    ProofTerm t = k->terms[id];
    if(!t.type) return 0;
    uint64_t mask = ScalarMask(k->types[t.type].width);
    if(t.op != P_ADD && t.op != P_SUB && t.op != P_MUL) {
        Monomial m = {0};
        m.coefficient = t.op == P_CONST ? t.value : 1;
        if(t.op != P_CONST) { m.degree = 1; m.factors[0] = id; }
        return poly_add(k, out, &m, mask);
    }
    Polynomial a = {0}, b = {0};
    int ok = polynomial(k, t.a, &a, depth + 1) &&
             polynomial(k, t.b, &b, depth + 1);
    if(ok && t.op == P_MUL) {
        for(int i = 0; ok && i < a.count; i++)
            for(int j = 0; ok && j < b.count; j++) {
                Monomial m = {0};
                Monomial *x = &a.terms[i], *y = &b.terms[j];
                if(x->degree + y->degree > PROOF_DEPTH) {
                    k->exhausted = 1; ok = 0; break;
                }
                m.coefficient = x->coefficient * y->coefficient;
                m.degree = x->degree + y->degree;
                unsigned xi = 0, yi = 0;
                for(unsigned n = 0; n < m.degree; n++)
                    m.factors[n] = yi == y->degree ||
                        (xi < x->degree && x->factors[xi] <= y->factors[yi]) ?
                        x->factors[xi++] : y->factors[yi++];
                ok = poly_add(k, out, &m, mask);
            }
    } else if(ok) {
        for(int i = 0; ok && i < a.count; i++) ok = poly_add(k, out, &a.terms[i], mask);
        for(int i = 0; ok && i < b.count; i++) {
            Monomial m = b.terms[i];
            if(t.op == P_SUB) m.coefficient = UINT64_C(0) - m.coefficient;
            ok = poly_add(k, out, &m, mask);
        }
    }
    poly_free(&a); poly_free(&b);
    return ok;
}

int ProofEqual(ProofKernel *k, int a, int b, int ring)
{
    if(a < 0 || b < 0 || a >= k->count || b >= k->count || !spend(k)) return 0;
    if(a == b) return 1;
    if(k->terms[a].type != k->terms[b].type || !ring || !k->terms[a].type) return 0;
    Polynomial x = {0}, y = {0};
    int equal = polynomial(k, a, &x, 0) && polynomial(k, b, &y, 0) && x.count == y.count;
    for(int i = 0; equal && i < x.count; i++) {
        int found = 0;
        for(int j = 0; j < y.count; j++) {
            if(!spend(k)) { equal = 0; break; }
            Monomial *m = &x.terms[i], *n = &y.terms[j];
            if(m->coefficient == n->coefficient && m->degree == n->degree &&
               !memcmp(m->factors, n->factors, m->degree * sizeof(int))) { found = 1; break; }
        }
        if(!found) equal = 0;
    }
    poly_free(&x); poly_free(&y);
    return equal;
}

typedef struct Order {
    int ids[PROOF_ORDER_TERMS], count, impossible;
    unsigned char edges[PROOF_ORDER_TERMS][PROOF_ORDER_TERMS]; /* 1 <=, 2 < */
    int diseq[PROOF_FACTS][2], diseq_count;
} Order;

static int order_id(ProofKernel *k, Order *o, int id, int ring)
{
    for(int i = 0; i < o->count; i++) if(ProofEqual(k, o->ids[i], id, ring)) return i;
    if(o->count == PROOF_ORDER_TERMS) { k->exhausted = 1; return -1; }
    o->ids[o->count] = id;
    o->edges[o->count][o->count] = 1;
    return o->count++;
}
static void edge(Order *o, int a, int b, int strength)
{
    if(a >= 0 && b >= 0 && o->edges[a][b] < strength) o->edges[a][b] = (unsigned char)strength;
}
static void order_fact(ProofKernel *k, Order *o, int id, int truth, int ring)
{
    ProofTerm t = k->terms[id];
    if(t.op == P_NOT) { order_fact(k, o, t.a, !truth, ring); return; }
    if(t.op != P_EQ && t.op != P_LT && t.op != P_LE) return;
    if(!k->terms[t.a].type) return;
    int a = order_id(k, o, t.a, ring), b = order_id(k, o, t.b, ring);
    if(a < 0 || b < 0) return;
    if(t.op == P_EQ) {
        if(truth) { edge(o, a, b, 1); edge(o, b, a, 1); }
        else if(o->diseq_count < PROOF_FACTS) {
            o->diseq[o->diseq_count][0] = a;
            o->diseq[o->diseq_count++][1] = b;
        }
    } else if(truth) edge(o, a, b, t.op == P_LT ? 2 : 1);
    else edge(o, b, a, t.op == P_LT ? 1 : 2);
}
static int order_truth(ProofKernel *k, int goal, int ring, int *impossible)
{
    Order *o = calloc(1, sizeof(*o));
    if(!o) { k->exhausted = 1; return -1; }
    for(int i = 0; i < k->fact_count; i++) order_fact(k, o, k->facts[i].term, k->facts[i].truth, ring);
    ProofTerm t = k->terms[goal];
    int a = -1, b = -1;
    if(t.op == P_EQ || t.op == P_LT || t.op == P_LE) {
        a = order_id(k, o, t.a, ring); b = order_id(k, o, t.b, ring);
    }
    for(int i = 0; i < o->count; i++)
        for(int j = 0; j < o->count; j++) {
            ProofTerm x = k->terms[o->ids[i]], y = k->terms[o->ids[j]];
            if(x.type == y.type && x.type && x.op == P_CONST && y.op == P_CONST) {
                ProofType type = k->types[x.type];
                int cmp = ScalarCompare(x.value, y.value, type.width, type.is_unsigned);
                if(cmp <= 0) edge(o, i, j, cmp < 0 ? 2 : 1);
            }
        }
    for(int m = 0; m < o->count && !k->exhausted; m++)
        for(int i = 0; i < o->count; i++)
            for(int j = 0; j < o->count; j++) {
                if(!spend(k)) break;
                if(o->edges[i][m] && o->edges[m][j])
                    edge(o, i, j, o->edges[i][m] == 2 || o->edges[m][j] == 2 ? 2 : 1);
            }
    *impossible = 0;
    for(int i = 0; i < o->count; i++) if(o->edges[i][i] == 2) *impossible = 1;
    for(int i = 0; i < o->diseq_count; i++)
        if(o->edges[o->diseq[i][0]][o->diseq[i][1]] &&
           o->edges[o->diseq[i][1]][o->diseq[i][0]]) *impossible = 1;
    int result = -1;
    if(a >= 0 && b >= 0) {
        if(t.op == P_EQ) {
            if(o->edges[a][b] && o->edges[b][a]) result = 1;
            else if(o->edges[a][b] == 2 || o->edges[b][a] == 2) result = 0;
        } else if(t.op == P_LT) {
            if(o->edges[a][b] == 2) result = 1;
            else if(o->edges[b][a]) result = 0;
        } else {
            if(o->edges[a][b]) result = 1;
            else if(o->edges[b][a] == 2) result = 0;
        }
    }
    free(o);
    return result;
}

static int truth(ProofKernel *k, int id, int ring, int order, int depth)
{
    if(id < 0 || id >= k->count || depth > PROOF_DEPTH || !spend(k)) return -1;
    ProofTerm t = k->terms[id];
    if(t.type) return -1;
    if(t.op == P_CONST) return (int)t.value;
    for(int i = 0; i < k->fact_count; i++)
        if(k->facts[i].term == id) return k->facts[i].truth;
    if(t.op == P_NOT) { int a = truth(k, t.a, ring, order, depth + 1); return a < 0 ? -1 : !a; }
    if(t.op == P_AND || t.op == P_OR) {
        int a = truth(k, t.a, ring, order, depth + 1);
        int b = truth(k, t.b, ring, order, depth + 1);
        if(t.op == P_AND) return a == 0 || b == 0 ? 0 : a == 1 && b == 1 ? 1 : -1;
        return a == 1 || b == 1 ? 1 : a == 0 && b == 0 ? 0 : -1;
    }
    if(t.op == P_ITE) {
        int c = truth(k, t.a, ring, order, depth + 1);
        if(c >= 0) return truth(k, c ? t.b : t.c, ring, order, depth + 1);
        int a = truth(k, t.b, ring, order, depth + 1), b = truth(k, t.c, ring, order, depth + 1);
        return a == b ? a : -1;
    }
    if(t.op == P_EQ || t.op == P_LT || t.op == P_LE) {
        if(ProofEqual(k, t.a, t.b, ring)) return t.op == P_LT ? 0 : 1;
        ProofTerm a = k->terms[t.a], b = k->terms[t.b];
        if(t.op == P_EQ && !a.type && !b.type) {
            int x = truth(k, t.a, ring, order, depth + 1);
            int y = truth(k, t.b, ring, order, depth + 1);
            if(x >= 0 && y >= 0) return x == y;
        }
        if(a.op == P_CONST && b.op == P_CONST && a.type == b.type) {
            int cmp = a.type ? ScalarCompare(a.value, b.value, k->types[a.type].width,
                                             k->types[a.type].is_unsigned) :
                      (a.value < b.value ? -1 : a.value > b.value);
            return t.op == P_EQ ? cmp == 0 : t.op == P_LT ? cmp < 0 : cmp <= 0;
        }
        if(order && a.type) { int impossible; return order_truth(k, id, ring, &impossible); }
    }
    return -1;
}
int ProofTruth(ProofKernel *k, int term, int ring, int order) { return truth(k, term, ring, order, 0); }

static int reduce(ProofKernel *k, int id, int ring, int order, int depth, int *split)
{
    if(id < 0 || id >= k->count || depth > PROOF_DEPTH || !spend(k)) return -1;
    ProofTerm t = k->terms[id];
    if(t.op == P_CONST || t.op == P_VAR) return id;
    if(t.op == P_ITE) {
        int cond = truth(k, t.a, ring, order, depth + 1);
        if(cond >= 0) return reduce(k, cond ? t.b : t.c, ring, order, depth + 1, split);
        if(*split < 0) *split = t.a;
    }
    t.a = reduce(k, t.a, ring, order, depth + 1, split);
    if(t.b >= 0) t.b = reduce(k, t.b, ring, order, depth + 1, split);
    if(t.c >= 0) t.c = reduce(k, t.c, ring, order, depth + 1, split);
    if(t.a < 0 || (k->terms[id].b >= 0 && t.b < 0) ||
       (k->terms[id].c >= 0 && t.c < 0)) return -1;
    if((t.op == P_ADD || t.op == P_SUB || t.op == P_MUL) &&
       k->terms[t.a].op == P_CONST && k->terms[t.b].op == P_CONST) {
        uint64_t a = k->terms[t.a].value, b = k->terms[t.b].value;
        return ProofNode(k, P_CONST, t.type, -1, -1, -1,
            t.op == P_ADD ? a + b : t.op == P_SUB ? a - b : a * b);
    }
    return ProofNode(k, t.op, t.type, t.a, t.b, t.c, t.value);
}

static int atom(ProofKernel *k, int id, int ring, int order, int depth)
{
    if(id < 0 || depth > PROOF_DEPTH || !spend(k)) return -1;
    ProofTerm t = k->terms[id];
    if(t.type || truth(k, id, ring, order, 0) >= 0) return -1;
    if(t.op == P_VAR || t.op == P_LT || t.op == P_LE ||
       (t.op == P_EQ && k->terms[t.a].type)) return id;
    int a = atom(k, t.a, ring, order, depth + 1);
    if(a >= 0) return a;
    int b = t.b >= 0 ? atom(k, t.b, ring, order, depth + 1) : -1;
    return b >= 0 ? b : t.op == P_EQ ? id : -1;
}
static int close_goal(ProofKernel *k, int goal, int ring, int order, int cases, int depth)
{
    if(depth > PROOF_DEPTH || !spend(k)) { k->exhausted = 1; return 0; }
    for(int i = 0; i < k->fact_count; i++) {
        int saved = k->fact_count, term = k->facts[i].term, wanted = k->facts[i].truth;
        /* Check each fact against earlier facts, not against itself. */
        k->fact_count = i;
        int known = truth(k, term, ring, order, 0);
        k->fact_count = saved;
        if(known >= 0 && known != wanted) return 1;
    }
    if(order) {
        int impossible;
        (void)order_truth(k, goal, ring, &impossible);
        if(impossible) return 1;
    }
    int split = -1;
    int reduced = reduce(k, goal, ring, order, 0, &split);
    if(reduced < 0 || k->exhausted) return 0;
    int result = truth(k, reduced, ring, order, 0);
    if(result >= 0) return result;
    if(!cases) return 0;
    if(split < 0) split = atom(k, reduced, ring, order, 0);
    if(split < 0 || k->fact_count == PROOF_FACTS) { k->exhausted |= k->fact_count == PROOF_FACTS; return 0; }
    int saved = k->fact_count;
    k->facts[k->fact_count++] = (ProofFact){split, 1};
    int a = close_goal(k, goal, ring, order, cases, depth + 1);
    k->facts[saved].truth = 0;
    int b = a && close_goal(k, goal, ring, order, cases, depth + 1);
    k->fact_count = saved;
    return a && b && !k->exhausted;
}
int ProofClose(ProofKernel *k, int goal, int ring, int order, int cases)
{
    int closed = close_goal(k, goal, ring, order, cases, 0);
    return closed && !k->exhausted;
}

int ProofRewrite(ProofKernel *k, int goal, int from, int to, int depth)
{
    if(goal < 0 || depth > PROOF_DEPTH || !spend(k)) return -1;
    if(ProofEqual(k, goal, from, 1)) return to;
    ProofTerm t = k->terms[goal];
    if(t.op == P_CONST || t.op == P_VAR) return goal;
    t.a = ProofRewrite(k, t.a, from, to, depth + 1);
    if(t.b >= 0) t.b = ProofRewrite(k, t.b, from, to, depth + 1);
    if(t.c >= 0) t.c = ProofRewrite(k, t.c, from, to, depth + 1);
    if(t.a < 0 || (k->terms[goal].b >= 0 && t.b < 0) ||
       (k->terms[goal].c >= 0 && t.c < 0)) return -1;
    return ProofNode(k, t.op, t.type, t.a, t.b, t.c, t.value);
}
