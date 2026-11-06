#include "zir_emit_internal.h"

/* Index range proofs for generated C. An index whose value provably lies
 * inside its array, slice or string needs no bounds check, so C can index
 * directly. Proofs use only facts that hold on every path:
 *
 * - every value of an integer type lies in that type's range;
 * - literals, casts and integer operators narrow ranges, such as x & 7,
 *   x % 8 for a nonnegative x, or u32 >> 29;
 * - a local or parameter whose every write is visible here, never through
 *   its address, takes only the values those writes store;
 * - inside a while body, before the body writes the counter again, the
 *   counter still satisfies the loop condition, such as i <= 7 or
 *   i < view.count, and it never goes below its smallest stored value when
 *   it only grows by constants within that loop.
 *
 * Anything else keeps its check, so a program aborts exactly where it did. */

enum {
    RANGE_DEPTH = 8, RANGE_BUDGET = 4096, RANGE_CONTEXT = 32, RANGE_HOPS = 48,
    POLY_TERMS = 12, POLY_DEGREE = 3, POLY_DEPTH = 24, GUARD_FACTS = 12, POLY_BUDGET = 16384
};

typedef struct RangeFacts {
    const ZirFunction *fn;
    int statements, parameters;
    int *statement_of;   /* expression -> statement holding it, or -1 */
    int *binding_of;     /* identifier -> binding, or -1 for anything else */
    int *block_end;      /* if/while/block statement -> its closing statement */
    int *loop_of;        /* statement -> innermost enclosing while, or -1 */
    /* Per binding: statements first, then parameters. */
    unsigned char *unknown;  /* written in a way the proofs do not follow */
    int *first_write;    /* first assignment statement, linked by next_write */
    int *last_write;
    int *next_write;
    int *increment_loop; /* while holding every += write, -1 none, -2 mixed */
    int64_t *step_total;
    const char **binding_type;
    /* Guards: conditions that hold where an index is read. */
    int bindings;
    int *parent_of;      /* statement -> innermost enclosing if/while/block, or -1 */
    int *prev_guard;     /* statement -> nearest earlier sibling if that always exits, or -1 */
    unsigned char *busy; /* per binding: its guard facts are being applied */
    unsigned char *computing; /* per binding: its own value is being worked out */
    int hops;            /* bindings being worked out, innermost question last */
    /* Answers by binding and statement, outside && and || contexts. */
    struct RangeMemo *memo;
    unsigned memo_mask, memo_count;
    struct GuardFacts *facts; /* per statement, [0] false and [1] true */
    /* Conditions that hold while a later operand of && or || runs. */
    struct { int condition, truth, statement; } context[RANGE_CONTEXT];
    int contexts;
    /* Steps left for the current question; a long chain of bindings
     * stops being followed instead of taking exponential time. */
    int budget;
    int broken;
} RangeFacts;

typedef struct Range {
    int64_t low, high;
} Range;

typedef struct RangeMemo {
    int64_t key; /* binding * (statements + 2) + statement + 2, 0 when empty */
    Range range;
} RangeMemo;

/* An exact integer polynomial over bindings and slice counts, plus a
 * constant somewhere in k: the value is sum(coef * atoms) + some c in k. */
typedef struct Term {
    int64_t coef;
    int degree;
    int atom[POLY_DEGREE]; /* ascending binding ids; bindings + view is view.count */
} Term;

typedef struct Poly {
    Range k;
    int count;
    Term term[POLY_TERMS];
} Poly;

/* A guard's facts, each poly >= 0, built once per guard and truth. */
typedef struct GuardFacts {
    int state; /* 0 not built, 1 building, 2 built */
    int count;
    Poly *poly;
} GuardFacts;

static int expr_range(const RangeFacts *f, int index, int depth, Range *out);
static int condition_interval(const RangeFacts *f, int condition, int truth, int binding,
                              int statement, int depth, Range *out);

static int
type_range(const char *type, Range *out)
{
    static const struct { const char *name; int64_t low, high; } types[] = {
        {"s8", INT8_MIN, INT8_MAX}, {"s16", INT16_MIN, INT16_MAX},
        {"s32", INT32_MIN, INT32_MAX}, {"s64", INT64_MIN, INT64_MAX},
        {"u8", 0, UINT8_MAX}, {"u16", 0, UINT16_MAX}, {"u32", 0, UINT32_MAX},
        {"integer", INT64_MIN, INT64_MAX},
    };
    if(type == NULL)
        return 0;
    type = canonical(type);
    for(size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
        if(!strcmp(type, types[i].name)) {
            out->low = types[i].low;
            out->high = types[i].high;
            return 1;
        }
    return 0;
}

static int
integer_type(const char *type)
{
    Range ignored;
    return type != NULL && (type_range(type, &ignored) || !strcmp(canonical(type), "u64"));
}

static int
within(Range inner, Range outer)
{
    return inner.low >= outer.low && inner.high <= outer.high;
}

/* Decimal, 0x or 0b digits only; anything else is not proven. */
static int
literal_value(const char *text, int64_t *value)
{
    unsigned long long bits;
    if(!ZirIntegerLiteral(text, &bits) || bits > (UINT64_C(1) << 62))
        return 0;
    *value = (int64_t)bits;
    return 1;
}

/* The binding an expression's storage belongs to: v, v.field, v[i]. */
static int
root_binding(const RangeFacts *f, int index, int through_index)
{
    while(index >= 0) {
        const ZirExpr *expr = &f->fn->exprs[index];
        if(expr->kind == ZIR_EXPR_IDENT)
            return f->binding_of[index];
        if(expr->kind == ZIR_EXPR_MEMBER || (through_index && expr->kind == ZIR_EXPR_INDEX))
            index = expr->left;
        else
            return -1;
    }
    return -1;
}

static int
supported_statement(const ZirStmt *st)
{
    switch(st->kind) {
    case ZIR_STMT_BLOCK_OPEN:
        return !StatementHasAssembly(st);
    case ZIR_STMT_BLOCK_CLOSE: case ZIR_STMT_DECL: case ZIR_STMT_ASSIGN:
    case ZIR_STMT_EXPR: case ZIR_STMT_IF: case ZIR_STMT_RETURN:
    case ZIR_STMT_BREAK: case ZIR_STMT_CONTINUE: case ZIR_STMT_UNUSED:
    case ZIR_STMT_UNREACHABLE:
        return !st->is_parallel && !st->is_gpu;
    case ZIR_STMT_WHILE:
        return !st->is_parallel && !st->is_gpu;
    default:
        return 0;
    }
}

void
RangeFactsFree(RangeFacts *f)
{
    if(f == NULL)
        return;
    free(f->statement_of);
    free(f->binding_of);
    free(f->block_end);
    free(f->loop_of);
    free(f->unknown);
    free(f->first_write);
    free(f->last_write);
    free(f->next_write);
    free(f->increment_loop);
    free(f->step_total);
    free(f->binding_type);
    free(f->parent_of);
    free(f->prev_guard);
    free(f->busy);
    free(f->computing);
    free(f->memo);
    if(f->facts != NULL)
        for(int s = 0; s < 2 * f->statements; s++)
            free(f->facts[s].poly);
    free(f->facts);
    free(f);
}

static void
record_write(RangeFacts *f, int statement)
{
    const ZirFunction *fn = f->fn;
    const ZirStmt *st = &fn->stmts[statement];
    const ZirExpr *lhs = &fn->exprs[st->lhs_root];
    int binding = root_binding(f, st->lhs_root, 0);
    if(lhs->kind != ZIR_EXPR_IDENT && lhs->kind != ZIR_EXPR_MEMBER &&
       lhs->kind != ZIR_EXPR_INDEX && lhs->kind != ZIR_EXPR_POINTER_MEMBER &&
       !(lhs->kind == ZIR_EXPR_UNARY && !strcmp(lhs->op, "*"))) {
        /* An unfamiliar destination may write any binding it names. */
        ZirExprWalk walk = {fn, malloc((size_t)fn->expr_count * sizeof(int)), 0};
        for(int x = ExprWalkFirst(&walk, st->lhs_root); walk.pending != NULL && x >= 0;
            x = ExprWalkNext(&walk))
            if(f->binding_of[x] >= 0)
                f->unknown[f->binding_of[x]] = 1;
        if(walk.pending == NULL)
            f->broken = 1;
        free(walk.pending);
        return;
    }
    if(binding < 0)
        return;
    if(lhs->kind != ZIR_EXPR_IDENT) {
        /* A field write such as view.count = n changes a count. */
        f->unknown[binding] = 1;
        return;
    }
    if(!strcmp(st->assignment_op, "=")) {
    } else if(!strcmp(st->assignment_op, "+=") && st->expr_root >= 0) {
        /* Only literal steps: other bindings' writes are not all known yet. */
        int64_t step;
        int loop = f->loop_of[statement], value = st->expr_root;
        if(fn->exprs[value].kind == ZIR_EXPR_CAST)
            value = fn->exprs[value].right;
        if(value < 0 || fn->exprs[value].kind != ZIR_EXPR_INT ||
           !literal_value(fn->exprs[value].text, &step) || step < 1 ||
           step > (INT64_C(1) << 40) || loop < 0) {
            f->unknown[binding] = 1;
            return;
        }
        if(f->increment_loop[binding] == -1)
            f->increment_loop[binding] = loop;
        else if(f->increment_loop[binding] != loop)
            f->increment_loop[binding] = -2;
        f->step_total[binding] += step;
        if(f->step_total[binding] > (INT64_C(1) << 40))
            f->unknown[binding] = 1;
    } else {
        f->unknown[binding] = 1;
        return;
    }
    f->next_write[statement] = -1;
    if(f->first_write[binding] < 0)
        f->first_write[binding] = statement;
    else
        f->next_write[f->last_write[binding]] = statement;
    f->last_write[binding] = statement;
}

static unsigned
name_hash(const char *name, unsigned mask)
{
    unsigned hash = 2166136261u;
    for(; *name; name++)
        hash = (hash ^ (unsigned char)*name) * 16777619u;
    return hash & mask;
}

RangeFacts *
RangeFactsBuild(const ZirFunction *fn)
{
    /* Names in scope, newest first per hash bucket, so a lookup costs
     * the names sharing its bucket instead of every live binding. */
    typedef struct { const char *name; int binding, previous; unsigned bucket; } Scope;
    RangeFacts *f;
    Scope *scope;
    int *opens, *heads, *guards, scope_count = 0, open_count = 0;
    unsigned mask = 1;
    ZirExprWalk walk = {0};
    if(fn == NULL || fn->stmt_count <= 0 || fn->expr_count <= 0 ||
       (fn->assembly_text != NULL && *fn->assembly_text))
        return NULL;
    for(int s = 0; s < fn->stmt_count; s++)
        if(!supported_statement(&fn->stmts[s]))
            return NULL;
    f = calloc(1, sizeof(*f));
    if(f == NULL)
        return NULL;
    f->fn = fn;
    f->statements = fn->stmt_count;
    f->parameters = ParametersOf(FunctionArgs(fn))->count;
    int bindings = f->statements + f->parameters;
    f->statement_of = malloc((size_t)fn->expr_count * sizeof(int));
    f->binding_of = malloc((size_t)fn->expr_count * sizeof(int));
    f->block_end = malloc((size_t)fn->stmt_count * sizeof(int));
    f->loop_of = malloc((size_t)fn->stmt_count * sizeof(int));
    f->next_write = malloc((size_t)fn->stmt_count * sizeof(int));
    f->unknown = calloc((size_t)bindings, 1);
    f->first_write = malloc((size_t)bindings * sizeof(int));
    f->last_write = malloc((size_t)bindings * sizeof(int));
    f->increment_loop = malloc((size_t)bindings * sizeof(int));
    f->step_total = calloc((size_t)bindings, sizeof(int64_t));
    f->binding_type = calloc((size_t)bindings, sizeof(char *));
    f->bindings = bindings;
    f->parent_of = malloc((size_t)fn->stmt_count * sizeof(int));
    f->prev_guard = malloc((size_t)fn->stmt_count * sizeof(int));
    f->busy = calloc((size_t)bindings * 2, 1);
    f->computing = calloc((size_t)bindings, 1);
    f->facts = calloc((size_t)fn->stmt_count * 2, sizeof(*f->facts));
    guards = malloc(((size_t)fn->stmt_count + 2) * sizeof(int));
    while(mask < (unsigned)bindings * 2u && mask < (1u << 30))
        mask <<= 1;
    heads = malloc((size_t)mask * sizeof(int));
    mask--;
    scope = malloc(((size_t)bindings + 1) * sizeof(*scope));
    opens = malloc(((size_t)fn->stmt_count + 1) * sizeof(int));
    walk.fn = fn;
    walk.pending = malloc((size_t)fn->expr_count * sizeof(int));
    if(!f->statement_of || !f->binding_of || !f->block_end || !f->loop_of ||
       !f->next_write || !f->unknown || !f->first_write || !f->last_write ||
       !f->increment_loop || !f->step_total || !f->binding_type || !scope || !opens ||
       !heads || !walk.pending || !f->parent_of || !f->prev_guard || !f->busy || !f->computing || !f->facts ||
       !guards) {
        free(guards);
        free(heads);
        free(scope);
        free(opens);
        free(walk.pending);
        RangeFactsFree(f);
        return NULL;
    }
    for(int x = 0; x < fn->expr_count; x++)
        f->statement_of[x] = f->binding_of[x] = -1;
    for(int s = 0; s < fn->stmt_count; s++)
        f->block_end[s] = f->loop_of[s] = f->next_write[s] = f->parent_of[s] = f->prev_guard[s] = -1;
    /* guards[level] is the last exiting if at that nesting level. */
    guards[0] = -1;
    for(int b = 0; b < bindings; b++) {
        f->first_write[b] = -1;
        f->increment_loop[b] = -1;
    }
    for(unsigned b = 0; b <= mask; b++)
        heads[b] = -1;
#define RANGE_SCOPE_PUSH(scope_name, scope_binding) do { \
        scope[scope_count].name = (scope_name); \
        scope[scope_count].binding = (scope_binding); \
        scope[scope_count].bucket = name_hash((scope_name), mask); \
        scope[scope_count].previous = heads[scope[scope_count].bucket]; \
        heads[scope[scope_count].bucket] = scope_count; \
        scope_count++; \
    } while(0)
    {
        /* Parameters are the outermost bindings. */
        const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
        for(int i = 0; i < f->parameters; i++) {
            const char *name = parameters->items[i].binding_name != NULL ?
                parameters->items[i].binding_name : parameters->items[i].name;
            f->binding_type[f->statements + i] = parameters->items[i].value_type != NULL ?
                parameters->items[i].value_type : parameters->items[i].type;
            if(name != NULL)
                RANGE_SCOPE_PUSH(name, f->statements + i);
        }
    }
    /* Resolve each identifier to the binding in scope, as the checker did:
     * a declaration's own value still reads the outer binding. */
    for(int s = 0; s < fn->stmt_count; s++) {
        const ZirStmt *st = &fn->stmts[s];
        int roots[2] = {st->expr_root, st->lhs_root};
        /* Ifs and blocks pass on the while around them; the opener's own
         * entry already names a while or -1. */
        f->loop_of[s] = open_count > 0 ? opens[open_count - 1] : -1;
        f->parent_of[s] = f->loop_of[s];
        f->prev_guard[s] = guards[open_count];
        if(f->loop_of[s] >= 0 && fn->stmts[f->loop_of[s]].kind != ZIR_STMT_WHILE)
            f->loop_of[s] = f->loop_of[f->loop_of[s]];
        for(int r = 0; r < 2; r++)
            for(int x = ExprWalkFirst(&walk, roots[r]); x >= 0; x = ExprWalkNext(&walk)) {
                const ZirExpr *expr = &fn->exprs[x];
                f->statement_of[x] = s;
                if(expr->kind != ZIR_EXPR_IDENT || expr->name == NULL || expr->is_global_value ||
                   expr->is_function_value)
                    continue;
                for(int i = heads[name_hash(expr->name, mask)]; i >= 0; i = scope[i].previous)
                    if(!strcmp(scope[i].name, expr->name)) {
                        f->binding_of[x] = scope[i].binding;
                        break;
                    }
            }
        if(st->kind == ZIR_STMT_DECL && st->name != NULL) {
            RANGE_SCOPE_PUSH(st->name, s);
            f->binding_type[s] = st->type;
        }
        if(st->kind == ZIR_STMT_IF || st->kind == ZIR_STMT_WHILE || st->kind == ZIR_STMT_BLOCK_OPEN) {
            /* The scope marker is the count before the block. */
            opens[open_count++] = s;
            guards[open_count] = -1;
            f->block_end[s] = scope_count;
        }
        if(st->kind == ZIR_STMT_BLOCK_CLOSE && open_count > 0) {
            int open = opens[--open_count];
            ZirStmtKind last = fn->stmts[s - 1].kind;
            while(scope_count > f->block_end[open]) {
                scope_count--;
                heads[scope[scope_count].bucket] = scope[scope_count].previous;
            }
            f->block_end[open] = s;
            /* After an if whose body always leaves, its condition was false. */
            if(fn->stmts[open].kind == ZIR_STMT_IF && !fn->stmts[open].is_else &&
               fn->stmts[open].expr_root >= 0 && s - 1 > open &&
               (last == ZIR_STMT_RETURN || last == ZIR_STMT_BREAK ||
                last == ZIR_STMT_CONTINUE || last == ZIR_STMT_UNREACHABLE))
                guards[open_count] = open;
        }
    }
    while(open_count > 0)
        f->block_end[opens[--open_count]] = fn->stmt_count;
    /* Taking an address lets a write happen anywhere. */
    for(int x = 0; x < fn->expr_count; x++) {
        const ZirExpr *expr = &fn->exprs[x];
        if(expr->kind == ZIR_EXPR_UNARY && !strcmp(expr->op, "&")) {
            int binding = root_binding(f, expr->right, 1);
            if(binding >= 0)
                f->unknown[binding] = 1;
        }
    }
    for(int s = 0; s < fn->stmt_count; s++)
        if(fn->stmts[s].kind == ZIR_STMT_ASSIGN && fn->stmts[s].lhs_root >= 0)
            record_write(f, s);
#undef RANGE_SCOPE_PUSH
    free(guards);
    free(heads);
    free(scope);
    free(opens);
    free(walk.pending);
    if(f->broken) {
        RangeFactsFree(f);
        return NULL;
    }
    return f;
}

static int
add_checked(int64_t a, int64_t b, int64_t *out)
{
    if((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
        return 0;
    *out = a + b;
    return 1;
}

static int
subtract_checked(int64_t a, int64_t b, int64_t *out)
{
    if((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b))
        return 0;
    *out = a - b;
    return 1;
}

static int
add_range(Range a, Range b, int subtract, Range *out)
{
    if(subtract)
        return subtract_checked(a.low, b.high, &out->low) &&
               subtract_checked(a.high, b.low, &out->high);
    return add_checked(a.low, b.low, &out->low) && add_checked(a.high, b.high, &out->high);
}

/* The product's range, when no corner product overflows: a 32-bit value
 * less one, such as cast(s64)height - 1, times a 32-bit stride fits. */
static int
multiply_range(Range a, Range b, Range *out)
{
    int64_t products[4];
    if(__builtin_mul_overflow(a.low, b.low, &products[0]) ||
       __builtin_mul_overflow(a.low, b.high, &products[1]) ||
       __builtin_mul_overflow(a.high, b.low, &products[2]) ||
       __builtin_mul_overflow(a.high, b.high, &products[3]))
        return 0;
    out->low = out->high = products[0];
    for(int i = 1; i < 4; i++) {
        if(products[i] < out->low) out->low = products[i];
        if(products[i] > out->high) out->high = products[i];
    }
    return 1;
}

static int64_t
bit_cover(int64_t value)
{
    int64_t cover = 0;
    while(cover < value)
        cover = cover * 2 + 1;
    return cover;
}

/* The largest value a counter can have when condition holds: i <= n,
 * i < n, n >= i or n > i, through &&. */
static int
condition_bound(const RangeFacts *f, int condition, int binding, int depth, int64_t *bound)
{
    const ZirExpr *expr;
    if(condition < 0 || depth > RANGE_DEPTH)
        return 0;
    expr = &f->fn->exprs[condition];
    if(expr->kind != ZIR_EXPR_BINARY)
        return 0;
    if(!strcmp(expr->op, "&&")) {
        int64_t left, right;
        int has_left = condition_bound(f, expr->left, binding, depth + 1, &left);
        int has_right = condition_bound(f, expr->right, binding, depth + 1, &right);
        if(has_left && has_right)
            *bound = left < right ? left : right;
        else if(has_left)
            *bound = left;
        else if(has_right)
            *bound = right;
        return has_left || has_right;
    }
    int counter, limit, strict;
    if(!strcmp(expr->op, "<=") || !strcmp(expr->op, "<")) {
        counter = expr->left;
        limit = expr->right;
        strict = expr->op[1] == '\0';
    } else if(!strcmp(expr->op, ">=") || !strcmp(expr->op, ">")) {
        counter = expr->right;
        limit = expr->left;
        strict = expr->op[1] == '\0';
    } else {
        return 0;
    }
    if(f->fn->exprs[counter].kind != ZIR_EXPR_IDENT || f->binding_of[counter] != binding)
        return 0;
    {
        Range limit_range, counter_type;
        const char *limit_type, *own_type;
        if(f->fn->exprs[limit].type == NULL || f->fn->exprs[counter].type == NULL)
            return 0;
        limit_type = canonical(f->fn->exprs[limit].type);
        own_type = canonical(f->fn->exprs[counter].type);
        if(!expr_range(f, limit, depth + 1, &limit_range) ||
           !type_range(own_type, &counter_type))
            return 0;
        /* Both sides compare as the counter's type. */
        if(strcmp(limit_type, own_type) &&
           !(!strcmp(limit_type, "integer") && within(limit_range, counter_type)))
            return 0;
        *bound = limit_range.high - (strict ? 1 : 0);
        return 1;
    }
}

/* Every write of binding inside loop comes after statement at, outside any
 * loop nested in loop, so at holds the value the condition last saw. */
/* Whether every path from statement s jumps away before it reaches
 * statement at: a continue, break, return or unreachable follows s in its
 * own block, with at not between them. A for loop's step before a continue
 * is such a write: the loop's condition runs again before the body does. */
static int
leaves_before(const RangeFacts *f, int s, int at)
{
    int block = f->parent_of[s];
    int end = block >= 0 ? f->block_end[block] : f->statements;
    for(int c = s + 1; c < end; c++) {
        ZirStmtKind kind = f->fn->stmts[c].kind;
        if(s < at && at <= c)
            return 0;
        if(f->parent_of[c] == block &&
           (kind == ZIR_STMT_CONTINUE || kind == ZIR_STMT_BREAK ||
            kind == ZIR_STMT_RETURN || kind == ZIR_STMT_UNREACHABLE))
            return 1;
    }
    return 0;
}

static int
unchanged_since_condition(const RangeFacts *f, int binding, int loop, int at)
{
    for(int s = f->first_write[binding]; s >= 0; s = f->next_write[s])
        if(s > loop && s < f->block_end[loop] &&
           ((s < at && !leaves_before(f, s, at)) || f->loop_of[s] != loop))
            return 0;
    return 1;
}

/* Guards also bound counts: atom bindings + view stands for view.count,
 * which changes only when view does. */
static int
atom_binding(const RangeFacts *f, int atom)
{
    return atom >= f->bindings && atom < 2 * f->bindings ? atom - f->bindings : atom;
}

static const char *
atom_type(const RangeFacts *f, int atom)
{
    return atom >= f->bindings ? "s64" : f->binding_type[atom];
}

/* Nothing writes binding between statement from and statement at: no write
 * lies between them, and no loop around at but not from writes it. */
static int
unchanged_between(const RangeFacts *f, int binding, int from, int at)
{
    binding = atom_binding(f, binding);
    if(binding < 0 || binding >= f->bindings || f->unknown[binding])
        return 0;
    for(int s = f->first_write[binding]; s >= 0; s = f->next_write[s]) {
        if(s > from && s < at && !leaves_before(f, s, at))
            return 0;
        for(int loop = f->loop_of[at]; loop >= 0; loop = f->loop_of[loop])
            if(!(loop < from && from < f->block_end[loop]) && s > loop && s < f->block_end[loop])
                return 0;
    }
    return 1;
}

/* Whether expression index reads binding's value unchanged: the binding
 * itself, or a cast to a type holding every value of the binding's type. */
static int
reads_binding(const RangeFacts *f, int index, int binding)
{
    const ZirExpr *expr = &f->fn->exprs[index];
    const char *own = atom_type(f, binding);
    Range inner, outer;
    if(own == NULL || expr->type == NULL || !type_range(own, &inner))
        return 0;
    if(expr->kind == ZIR_EXPR_CAST) {
        if(!type_range(expr->type, &outer) || !within(inner, outer))
            return 0;
        index = expr->right;
        expr = &f->fn->exprs[index];
        if(expr->type == NULL)
            return 0;
    }
    if(binding >= f->bindings) {
        const ZirExpr *base;
        if(expr->kind != ZIR_EXPR_MEMBER || expr->name == NULL || strcmp(expr->name, "count"))
            return 0;
        base = &f->fn->exprs[expr->left];
        return base->kind == ZIR_EXPR_IDENT && f->binding_of[expr->left] == binding - f->bindings &&
               base->type != NULL &&
               (SliceElementType(base->type, NULL, 0) || !strcmp(canonical(base->type), "string")) &&
               !strcmp(canonical(expr->type), "s64");
    }
    return expr->kind == ZIR_EXPR_IDENT && f->binding_of[index] == binding &&
           !strcmp(canonical(expr->type), canonical(own));
}

static void
push_context(const RangeFacts *f, int condition, int truth, int statement)
{
    RangeFacts *g = (RangeFacts *)f;
    g->context[g->contexts].condition = condition;
    g->context[g->contexts].truth = truth;
    g->context[g->contexts].statement = statement;
    g->contexts++;
}

/* The values binding can hold where condition, evaluated at statement, has
 * value truth: x < n, n >= x, x == n and their negations, through !, && and
 * ||. The right side of && or || runs only after the left side, so facts
 * about its operands may use the left side's value. */
static int
condition_interval(const RangeFacts *f, int condition, int truth, int binding,
                   int statement, int depth, Range *out)
{
    static const char *const ops[] = {"<", "<=", ">", ">=", "==", "!="};
    static const int mirror[] = {2, 3, 0, 1, 4, 5}, negate[] = {3, 2, 1, 0, 5, 4};
    const ZirExpr *expr;
    Range whole, value;
    int op = -1, self, other;
    if(condition < 0 || depth > 2 * RANGE_DEPTH || --((RangeFacts *)f)->budget < 0)
        return 0;
    expr = &f->fn->exprs[condition];
    if(expr->kind == ZIR_EXPR_UNARY && !strcmp(expr->op, "!"))
        return condition_interval(f, expr->right, !truth, binding, statement, depth + 1, out);
    if(expr->kind != ZIR_EXPR_BINARY)
        return 0;
    if(!strcmp(expr->op, "&&") || !strcmp(expr->op, "||")) {
        /* Both sides hold for a true && or a false ||; otherwise one does. */
        int both = (expr->op[0] == '&') == (truth != 0), has_left, has_right = 0;
        Range left, right;
        has_left = condition_interval(f, expr->left, truth, binding, statement, depth + 1, &left);
        if(f->contexts < RANGE_CONTEXT) {
            push_context(f, expr->left, expr->op[0] == '&', statement);
            has_right = condition_interval(f, expr->right, truth, binding, statement, depth + 1, &right);
            ((RangeFacts *)f)->contexts--;
        }
        if(both) {
            if(has_left && has_right) {
                out->low = left.low > right.low ? left.low : right.low;
                out->high = left.high < right.high ? left.high : right.high;
            } else {
                *out = has_left ? left : right;
            }
            return has_left || has_right;
        }
        if(!has_left || !has_right)
            return 0;
        out->low = left.low < right.low ? left.low : right.low;
        out->high = left.high > right.high ? left.high : right.high;
        return 1;
    }
    for(int i = 0; i < 6; i++)
        if(!strcmp(expr->op, ops[i]))
            op = i;
    if(op < 0)
        return 0;
    if(reads_binding(f, expr->left, binding)) {
        self = expr->left;
        other = expr->right;
    } else if(reads_binding(f, expr->right, binding)) {
        self = expr->right;
        other = expr->left;
        op = mirror[op];
    } else {
        return 0;
    }
    if(!truth)
        op = negate[op];
    /* Both sides compare as one type, or an untyped literal it holds. */
    if(f->fn->exprs[self].type == NULL || f->fn->exprs[other].type == NULL ||
       !type_range(atom_type(f, binding), &whole) ||
       !expr_range(f, other, depth + 1, &value))
        return 0;
    if(strcmp(canonical(f->fn->exprs[self].type), canonical(f->fn->exprs[other].type))) {
        Range compared;
        if(strcmp(f->fn->exprs[other].type, "integer") ||
           !type_range(f->fn->exprs[self].type, &compared) || !within(value, compared))
            return 0;
    }
    *out = whole;
    switch(op) {
    case 0: if(value.high == INT64_MIN) return 0; out->high = value.high - 1; break;
    case 1: out->high = value.high; break;
    case 2: if(value.low == INT64_MAX) return 0; out->low = value.low + 1; break;
    case 3: out->low = value.low; break;
    case 4: *out = value; break;
    default: return 0;
    }
    if(out->low < whole.low) out->low = whole.low;
    if(out->high > whole.high) out->high = whole.high;
    return out->low <= out->high;
}

static void
narrow(Range *value, Range by)
{
    Range result = {by.low > value->low ? by.low : value->low,
                    by.high < value->high ? by.high : value->high};
    /* An empty range is code that cannot run; keep the wider answer. */
    if(result.low <= result.high)
        *value = result;
}

/* Narrow binding's value at statement at by the conditions that hold
 * there: earlier sibling ifs that always exit, enclosing ifs and whiles, and
 * the left sides of && and || around the operand being checked. */
static void
guard_narrow(const RangeFacts *f, int binding, int at, int depth, Range *value)
{
    RangeFacts *g = (RangeFacts *)f;
    Range by;
    if(at < 0 || at >= f->statements || binding < 0 || binding >= 2 * f->bindings ||
       f->busy[binding] || f->unknown[atom_binding(f, binding)] || depth > RANGE_DEPTH)
        return;
    g->busy[binding] = 1;
    for(int c = 0; c < f->contexts; c++)
        if(f->context[c].statement == at &&
           condition_interval(f, f->context[c].condition, f->context[c].truth, binding, at,
                              depth + 1, &by))
            narrow(value, by);
    for(int x = at; x >= 0; x = f->parent_of[x]) {
        int open = f->parent_of[x];
        const ZirStmt *st;
        /* A counting loop's step also runs after continue. */
        for(int guard = f->fn->stmts[x].for_step ? -1 : f->prev_guard[x]; guard >= 0;
            guard = f->prev_guard[guard])
            if(unchanged_between(f, binding, guard, at) &&
               condition_interval(f, f->fn->stmts[guard].expr_root, 0, binding, guard,
                                  depth + 1, &by))
                narrow(value, by);
        if(open < 0)
            break;
        st = &f->fn->stmts[open];
        if(st->expr_root < 0)
            continue;
        if(((st->kind == ZIR_STMT_IF && unchanged_between(f, binding, open, at)) ||
            (st->kind == ZIR_STMT_WHILE &&
             unchanged_since_condition(f, atom_binding(f, binding), open, at))) &&
           condition_interval(f, st->expr_root, 1, binding, open, depth + 1, &by))
            narrow(value, by);
    }
    g->busy[binding] = 0;
}

static int binding_base_range(const RangeFacts *f, int binding, int at, int depth, Range *out);

static RangeMemo *
memo_slot(const RangeFacts *f, int64_t key)
{
    unsigned i;
    if(f->memo == NULL)
        return NULL;
    for(i = (unsigned)((uint64_t)key * UINT64_C(0x9e3779b97f4a7c15) >> 40) & f->memo_mask;
        f->memo[i].key != 0 && f->memo[i].key != key; i = (i + 1) & f->memo_mask) {
    }
    return &f->memo[i];
}

static void
memo_store(const RangeFacts *f, int64_t key, Range range)
{
    RangeFacts *g = (RangeFacts *)f;
    RangeMemo *slot;
    if(g->memo == NULL || (g->memo_count + 1) * 2 > g->memo_mask + 1) {
        unsigned size = g->memo == NULL ? 1024 : (g->memo_mask + 1) * 2;
        RangeMemo *old = g->memo, *grown = calloc(size, sizeof(*grown));
        unsigned old_size = old == NULL ? 0 : g->memo_mask + 1;
        if(grown == NULL)
            return;
        g->memo = grown;
        g->memo_mask = size - 1;
        g->memo_count = 0;
        for(unsigned i = 0; i < old_size; i++)
            if(old[i].key != 0) {
                *memo_slot(g, old[i].key) = old[i];
                g->memo_count++;
            }
        free(old);
    }
    slot = memo_slot(g, key);
    if(slot->key == 0)
        g->memo_count++;
    slot->key = key;
    slot->range = range;
}

/* Binding's value at statement at. Each binding followed starts its own
 * expression depth; a binding already being worked out, or a chain longer
 * than RANGE_HOPS, answers with its type's range. */
static int
binding_range(const RangeFacts *f, int binding, int at, int depth, Range *out)
{
    RangeFacts *g = (RangeFacts *)f;
    int64_t key = (int64_t)binding * (f->statements + 2) + at + 2;
    int in_context = 0, ok;
    RangeMemo *slot;
    (void)depth;
    for(int c = 0; c < f->contexts; c++)
        in_context |= f->context[c].statement == at;
    slot = in_context ? NULL : memo_slot(f, key);
    if(slot != NULL && slot->key == key) {
        *out = slot->range;
        return 1;
    }
    if(f->computing[binding] || f->hops >= RANGE_HOPS)
        return type_range(f->binding_type[binding], out);
    g->computing[binding] = 1;
    g->hops++;
    ok = binding_base_range(f, binding, at, 0, out);
    if(ok)
        guard_narrow(f, binding, at, 0, out);
    g->hops--;
    g->computing[binding] = 0;
    /* A question cut short keeps a sound but wider answer; ask it again. */
    if(ok && f->contexts == 0 && f->budget >= 0)
        memo_store(f, key, *out);
    return ok;
}

static int
binding_base_range(const RangeFacts *f, int binding, int at, int depth, Range *out)
{
    const ZirFunction *fn = f->fn;
    const char *type = f->binding_type[binding];
    Range whole, value;
    if(!type_range(type, &whole))
        return 0;
    *out = whole;
    if(f->unknown[binding] || depth > RANGE_DEPTH)
        return 1;
    if(binding >= f->statements) {
        /* A parameter's incoming value is any value of its type. */
        value = whole;
    } else if(fn->stmts[binding].expr_root < 0) {
        /* A declaration without a value starts at zero. */
        value.low = value.high = 0;
    } else if(!expr_range(f, fn->stmts[binding].expr_root, depth + 1, &value)) {
        return 1;
    }
    for(int s = f->first_write[binding]; s >= 0; s = f->next_write[s]) {
        Range stored;
        if(strcmp(fn->stmts[s].assignment_op, "="))
            continue;
        if(fn->stmts[s].expr_root < 0 || !expr_range(f, fn->stmts[s].expr_root, depth + 1, &stored))
            return 1;
        if(stored.low < value.low) value.low = stored.low;
        if(stored.high > value.high) value.high = stored.high;
    }
    if(f->increment_loop[binding] != -1) {
        int loop = f->increment_loop[binding];
        int64_t bound;
        if(loop < 0 || !condition_bound(f, fn->stmts[loop].expr_root, binding, depth + 1, &bound))
            return 1;
        if(bound < value.high)
            bound = value.high;
        /* Each step runs at most once per pass after the condition held. */
        if(bound > whole.high - f->step_total[binding])
            return 1;
        value.high = bound + f->step_total[binding];
    }
    if(!within(value, whole))
        return 1;
    /* Inside a loop that bounds the counter and before it changes again. */
    for(int loop = at >= 0 ? f->loop_of[at] : -1; loop >= 0; loop = f->loop_of[loop]) {
        int64_t bound;
        if(condition_bound(f, fn->stmts[loop].expr_root, binding, depth + 1, &bound) &&
           unchanged_since_condition(f, binding, loop, at) && bound < value.high)
            value.high = bound;
    }
    *out = value;
    return 1;
}

static int
expr_range(const RangeFacts *f, int index, int depth, Range *out)
{
    const ZirFunction *fn = f->fn;
    const ZirExpr *expr;
    Range whole, a, b;
    int has_whole;
    if(index < 0 || index >= fn->expr_count)
        return 0;
    expr = &fn->exprs[index];
    has_whole = type_range(expr->type, &whole);
    if(depth > RANGE_DEPTH || --((RangeFacts *)f)->budget < 0) {
        *out = whole;
        return has_whole;
    }
    switch(expr->kind) {
    case ZIR_EXPR_INT:
        if(literal_value(expr->text, &a.low)) {
            a.high = a.low;
            if(!has_whole || within(a, whole)) {
                *out = a;
                return 1;
            }
        }
        break;
    case ZIR_EXPR_CAST:
        if(has_whole && integer_type(fn->exprs[expr->right].type) &&
           expr_range(f, expr->right, depth + 1, &a) && within(a, whole)) {
            *out = a;
            return 1;
        }
        break;
    case ZIR_EXPR_IDENT:
        if(f->binding_of[index] >= 0 && expr->type != NULL &&
           f->binding_type[f->binding_of[index]] != NULL &&
           !strcmp(canonical(expr->type), canonical(f->binding_type[f->binding_of[index]])))
            return binding_range(f, f->binding_of[index], f->statement_of[index], depth + 1, out);
        break;
    case ZIR_EXPR_MEMBER: {
        int capacity = 0;
        const char *base = fn->exprs[expr->left].type;
        if(expr->name == NULL || strcmp(expr->name, "count") || base == NULL)
            break;
        if(ArrayElementType(base, NULL, 0, &capacity) && capacity >= 0) {
            out->low = out->high = capacity;
            return 1;
        }
        /* A slice or string never holds a negative count. */
        if((SliceElementType(base, NULL, 0) || !strcmp(canonical(base), "string")) &&
           has_whole && whole.high == INT64_MAX) {
            out->low = 0;
            out->high = INT64_MAX;
            if(fn->exprs[expr->left].kind == ZIR_EXPR_IDENT && f->binding_of[expr->left] >= 0)
                guard_narrow(f, f->bindings + f->binding_of[expr->left], f->statement_of[index],
                             depth, out);
            return 1;
        }
        break;
    }
    case ZIR_EXPR_CONDITIONAL:
        if(expr_range(f, expr->right, depth + 1, &a) && expr_range(f, expr->third, depth + 1, &b)) {
            a.low = a.low < b.low ? a.low : b.low;
            a.high = a.high > b.high ? a.high : b.high;
            if(!has_whole || within(a, whole)) {
                *out = a;
                return 1;
            }
        }
        break;
    case ZIR_EXPR_UNARY:
        if(!strcmp(expr->op, "-") && expr_range(f, expr->right, depth + 1, &a) &&
           a.low > INT64_MIN) {
            b.low = -a.high;
            b.high = -a.low;
            if(has_whole && within(b, whole)) {
                *out = b;
                return 1;
            }
        }
        break;
    case ZIR_EXPR_BINARY: {
        int left = expr_range(f, expr->left, depth + 1, &a);
        int right = expr_range(f, expr->right, depth + 1, &b);
        Range result;
        int known = 0;
        if(!has_whole || !integer_type(fn->exprs[expr->left].type))
            break;
        if(!strcmp(expr->op, "&")) {
            /* A nonnegative operand clears every higher bit. */
            if(left && a.low >= 0) { result.low = 0; result.high = a.high; known = 1; }
            if(right && b.low >= 0 && (!known || b.high < result.high)) {
                result.low = 0;
                result.high = b.high;
                known = 1;
            }
        } else if(!strcmp(expr->op, "|") || !strcmp(expr->op, "^")) {
            if(left && right && a.low >= 0 && b.low >= 0) {
                result.low = 0;
                result.high = bit_cover(a.high > b.high ? a.high : b.high);
                known = 1;
            }
        } else if(!strcmp(expr->op, "+") || !strcmp(expr->op, "-")) {
            known = left && right && add_range(a, b, expr->op[0] == '-', &result);
        } else if(!strcmp(expr->op, "*")) {
            known = left && right && multiply_range(a, b, &result);
        } else if(!strcmp(expr->op, "/")) {
            if(left && right && a.low >= 0 && b.low > 0) {
                result.low = a.low / b.high;
                result.high = a.high / b.low;
                known = 1;
            }
        } else if(!strcmp(expr->op, "%")) {
            if(left && right && a.low >= 0 && b.low > 0) {
                result.low = 0;
                result.high = a.high < b.high - 1 ? a.high : b.high - 1;
                known = 1;
            }
        } else if(!strcmp(expr->op, ">>")) {
            /* A shift count outside the width aborts before the index. */
            if(right && b.low >= 0 && b.high < 64) {
                if(left && a.low >= 0) {
                    result.low = a.low >> b.high;
                    result.high = a.high >> b.low;
                    known = 1;
                } else if(!strcmp(canonical(fn->exprs[expr->left].type), "u64") && b.low > 0) {
                    result.low = 0;
                    result.high = (int64_t)(UINT64_MAX >> b.low);
                    known = 1;
                }
            }
        }
        if(known && within(result, whole)) {
            *out = result;
            return 1;
        }
        break;
    }
    default:
        break;
    }
    if(has_whole) {
        *out = whole;
        return 1;
    }
    return 0;
}

/* Index proofs through polynomials: an index such as row * stride + x lies
 * below a count when, with each written binding replaced by an upper bound
 * that a loop condition or guard gives it, count - index - 1 is never
 * negative, using a guard fact such as count >= (height + 3) * stride + 8. */

static int
mul_checked(int64_t a, int64_t b, int64_t *out)
{
    if(a == 0 || b == 0) {
        *out = 0;
        return 1;
    }
    if(a == INT64_MIN || b == INT64_MIN || (a < 0 ? -a : a) > INT64_MAX / (b < 0 ? -b : b))
        return 0;
    *out = a * b;
    return 1;
}

static int
range_multiply(Range a, Range b, Range *out)
{
    int64_t products[4];
    if(!mul_checked(a.low, b.low, &products[0]) || !mul_checked(a.low, b.high, &products[1]) ||
       !mul_checked(a.high, b.low, &products[2]) || !mul_checked(a.high, b.high, &products[3]))
        return 0;
    out->low = out->high = products[0];
    for(int i = 1; i < 4; i++) {
        if(products[i] < out->low) out->low = products[i];
        if(products[i] > out->high) out->high = products[i];
    }
    return 1;
}

static void
poly_constant(Poly *p, int64_t low, int64_t high)
{
    p->k.low = low;
    p->k.high = high;
    p->count = 0;
}

static void
poly_atom(Poly *p, int atom)
{
    poly_constant(p, 0, 0);
    p->count = 1;
    p->term[0].coef = 1;
    p->term[0].degree = 1;
    p->term[0].atom[0] = atom;
}

static int
same_atoms(const Term *a, const Term *b)
{
    if(a->degree != b->degree)
        return 0;
    for(int i = 0; i < a->degree; i++)
        if(a->atom[i] != b->atom[i])
            return 0;
    return 1;
}

/* Add t to p; a term that cancels leaves p. */
static int
poly_add_term(Poly *p, const Term *t)
{
    for(int i = 0; i < p->count; i++)
        if(same_atoms(&p->term[i], t)) {
            if(!add_checked(p->term[i].coef, t->coef, &p->term[i].coef))
                return 0;
            if(p->term[i].coef == 0)
                p->term[i] = p->term[--p->count];
            return 1;
        }
    if(t->coef == 0)
        return 1;
    if(p->count >= POLY_TERMS)
        return 0;
    p->term[p->count++] = *t;
    return 1;
}

static int
poly_add(Poly *out, const Poly *a, const Poly *b, int subtract)
{
    Poly r = *a;
    if(!add_range(a->k, b->k, subtract, &r.k))
        return 0;
    for(int i = 0; i < b->count; i++) {
        Term t = b->term[i];
        if(subtract) {
            if(t.coef == INT64_MIN)
                return 0;
            t.coef = -t.coef;
        }
        if(!poly_add_term(&r, &t))
            return 0;
    }
    *out = r;
    return 1;
}

static int
term_times(const Term *a, const Term *b, Term *out)
{
    int i = 0, j = 0, n = 0;
    if(a->degree + b->degree > POLY_DEGREE || !mul_checked(a->coef, b->coef, &out->coef))
        return 0;
    while(i < a->degree || j < b->degree)
        out->atom[n++] = j >= b->degree || (i < a->degree && a->atom[i] <= b->atom[j]) ?
            a->atom[i++] : b->atom[j++];
    out->degree = n;
    return 1;
}

static int
poly_multiply(Poly *out, const Poly *a, const Poly *b)
{
    Poly r;
    poly_constant(&r, 0, 0);
    /* A constant known only as a range multiplies only constants. */
    if((a->count > 0 && b->k.low != b->k.high) || (b->count > 0 && a->k.low != a->k.high) ||
       !range_multiply(a->k, b->k, &r.k))
        return 0;
    for(int i = 0; i < a->count; i++)
        for(int j = 0; j < b->count; j++) {
            Term t;
            if(!term_times(&a->term[i], &b->term[j], &t) || !poly_add_term(&r, &t))
                return 0;
        }
    for(int i = 0; i < a->count && b->k.low != 0; i++) {
        Term t = a->term[i];
        if(!mul_checked(t.coef, b->k.low, &t.coef) || !poly_add_term(&r, &t))
            return 0;
    }
    for(int j = 0; j < b->count && a->k.low != 0; j++) {
        Term t = b->term[j];
        if(!mul_checked(t.coef, a->k.low, &t.coef) || !poly_add_term(&r, &t))
            return 0;
    }
    *out = r;
    return 1;
}

/* A binding nothing writes after its declaration, nor through its address. */
static int
invariant(const RangeFacts *f, int binding)
{
    return binding >= 0 && binding < f->bindings && !f->unknown[binding] &&
           f->first_write[binding] < 0;
}

static int
atom_range(const RangeFacts *f, int atom, int at, Range *out)
{
    if(atom >= f->bindings) {
        /* A count. */
        out->low = 0;
        out->high = INT64_MAX;
        guard_narrow(f, atom, at, 0, out);
        return 1;
    }
    return binding_range(f, atom, at, 1, out);
}

static int
poly_range(const RangeFacts *f, const Poly *p, int at, Range *out)
{
    Range sum = p->k;
    for(int i = 0; i < p->count; i++) {
        Range term = {p->term[i].coef, p->term[i].coef}, atom;
        for(int a = 0; a < p->term[i].degree; a++)
            if(!atom_range(f, p->term[i].atom[a], at, &atom) || !range_multiply(term, atom, &term))
                return 0;
        if(!add_range(sum, term, 0, &sum))
            return 0;
    }
    *out = sum;
    return 1;
}

/* Every written binding root reads keeps its value from statement from to
 * statement at. */
static int
reads_unchanged(const RangeFacts *f, int root, int from, int at)
{
    ZirExprWalk walk = {f->fn, malloc((size_t)f->fn->expr_count * sizeof(int)), 0};
    int unchanged = walk.pending != NULL;
    for(int x = unchanged ? ExprWalkFirst(&walk, root) : -1; x >= 0; x = ExprWalkNext(&walk)) {
        int binding = f->binding_of[x];
        if(binding >= 0 && !invariant(f, binding) && !unchanged_between(f, binding, from, at)) {
            unchanged = 0;
            break;
        }
    }
    free(walk.pending);
    return unchanged;
}

/* The exact value of an integer expression at statement at, or 0 when an
 * operation may wrap. A declaration nothing writes stands for its value
 * when what that reads is unchanged at at; other bindings stay atoms. */
static int
to_poly(const RangeFacts *f, int index, int at, int depth, Poly *out)
{
    const ZirFunction *fn = f->fn;
    const ZirExpr *expr;
    Range whole, a, b, result;
    Poly left, right;
    if(index < 0 || index >= fn->expr_count || depth > POLY_DEPTH ||
       --((RangeFacts *)f)->budget < 0)
        return 0;
    expr = &fn->exprs[index];
    if(!type_range(expr->type, &whole))
        return 0;
    switch(expr->kind) {
    case ZIR_EXPR_INT: {
        int64_t value;
        if(!literal_value(expr->text, &value) || value < whole.low || value > whole.high)
            return 0;
        poly_constant(out, value, value);
        return 1;
    }
    case ZIR_EXPR_CAST:
        if(integer_type(fn->exprs[expr->right].type) && expr_range(f, expr->right, 0, &a) &&
           within(a, whole))
            return to_poly(f, expr->right, at, depth + 1, out);
        break;
    case ZIR_EXPR_IDENT: {
        int binding = f->binding_of[index];
        Range own;
        if(binding < 0 || f->binding_type[binding] == NULL ||
           !type_range(f->binding_type[binding], &own) || !within(own, whole))
            break;
        if(invariant(f, binding) && binding < f->statements &&
           fn->stmts[binding].kind == ZIR_STMT_DECL && fn->stmts[binding].expr_root >= 0) {
            int value = fn->stmts[binding].expr_root;
            if(expr_range(f, value, 0, &a) && within(a, own) &&
               reads_unchanged(f, value, binding, at) && to_poly(f, value, at, depth + 1, out))
                return 1;
        }
        poly_atom(out, binding);
        return 1;
    }
    case ZIR_EXPR_MEMBER: {
        int capacity = 0, view;
        const char *base = fn->exprs[expr->left].type;
        if(expr->name == NULL || strcmp(expr->name, "count") || base == NULL)
            break;
        if(ArrayElementType(base, NULL, 0, &capacity) && capacity >= 0) {
            poly_constant(out, capacity, capacity);
            return 1;
        }
        view = fn->exprs[expr->left].kind == ZIR_EXPR_IDENT ? f->binding_of[expr->left] : -1;
        if((SliceElementType(base, NULL, 0) || !strcmp(canonical(base), "string")) &&
           invariant(f, view) && whole.high == INT64_MAX) {
            poly_atom(out, f->bindings + view);
            return 1;
        }
        break;
    }
    case ZIR_EXPR_UNARY:
        if(!strcmp(expr->op, "-") && expr_range(f, expr->right, 0, &a) && a.low > INT64_MIN &&
           -a.high >= whole.low && -a.low <= whole.high && to_poly(f, expr->right, at, depth + 1, &right)) {
            Poly zero;
            poly_constant(&zero, 0, 0);
            if(poly_add(out, &zero, &right, 1))
                return 1;
        }
        break;
    case ZIR_EXPR_BINARY:
        if(!integer_type(fn->exprs[expr->left].type) || !expr_range(f, expr->left, 0, &a) ||
           !expr_range(f, expr->right, 0, &b))
            break;
        if(!strcmp(expr->op, "<<")) {
            /* A literal count: x << k is x * 2^k when that fits. */
            int count = expr->right;
            int64_t shift;
            Poly scale;
            if(fn->exprs[count].kind == ZIR_EXPR_CAST)
                count = fn->exprs[count].right;
            if(fn->exprs[count].kind != ZIR_EXPR_INT || !literal_value(fn->exprs[count].text, &shift) ||
               shift > 62)
                break;
            poly_constant(&scale, INT64_C(1) << shift, INT64_C(1) << shift);
            if(range_multiply(a, scale.k, &result) && within(result, whole) &&
               to_poly(f, expr->left, at, depth + 1, &left) && poly_multiply(out, &left, &scale))
                return 1;
            break;
        }
        if(!strcmp(expr->op, "+") || !strcmp(expr->op, "-")) {
            if(add_range(a, b, expr->op[0] == '-', &result) && within(result, whole) &&
               to_poly(f, expr->left, at, depth + 1, &left) &&
               to_poly(f, expr->right, at, depth + 1, &right) &&
               poly_add(out, &left, &right, expr->op[0] == '-'))
                return 1;
        } else if(!strcmp(expr->op, "*")) {
            if(range_multiply(a, b, &result) && within(result, whole) &&
               to_poly(f, expr->left, at, depth + 1, &left) &&
               to_poly(f, expr->right, at, depth + 1, &right) && poly_multiply(out, &left, &right))
                return 1;
        }
        break;
    default:
        break;
    }
    /* Anything else is some value in its range. */
    if(!expr_range(f, index, 0, &a))
        return 0;
    poly_constant(out, a.low, a.high);
    return 1;
}

/* Whether a comparison of left and right compares their values: one type,
 * or an untyped literal that type holds. */
static int
compares_values(const RangeFacts *f, int left, int right)
{
    const char *a = f->fn->exprs[left].type, *b = f->fn->exprs[right].type;
    Range type, value;
    if(a == NULL || b == NULL || !integer_type(a) || !integer_type(b))
        return 0;
    /* canonical maps the untyped integer to s64, so look first. */
    if(!strcmp(a, "integer") && strcmp(b, "integer"))
        return type_range(b, &type) && expr_range(f, left, 0, &value) && within(value, type);
    if(!strcmp(b, "integer") && strcmp(a, "integer"))
        return type_range(a, &type) && expr_range(f, right, 0, &value) && within(value, type);
    return !strcmp(canonical(a), canonical(b));
}

static void
add_fact(GuardFacts *facts, const Poly *a, const Poly *b, int64_t less)
{
    /* a - b - less >= 0 */
    Poly d, gap;
    poly_constant(&gap, less, less);
    if(facts->count < GUARD_FACTS && poly_add(&d, a, b, 1) && poly_add(&d, &d, &gap, 1))
        facts->poly[facts->count++] = d;
}

/* Facts poly >= 0 that hold where condition, evaluated at statement, has
 * value truth. */
static void
condition_facts(const RangeFacts *f, int condition, int truth, int statement, int depth,
                GuardFacts *facts)
{
    static const char *const ops[] = {"<", "<=", ">", ">=", "==", "!="};
    static const int negate[] = {3, 2, 1, 0, 5, 4};
    const ZirExpr *expr;
    Poly left, right;
    int op = -1;
    if(condition < 0 || depth > 2 * RANGE_DEPTH || facts->count >= GUARD_FACTS)
        return;
    expr = &f->fn->exprs[condition];
    if(expr->kind == ZIR_EXPR_UNARY && !strcmp(expr->op, "!")) {
        condition_facts(f, expr->right, !truth, statement, depth + 1, facts);
        return;
    }
    if(expr->kind != ZIR_EXPR_BINARY)
        return;
    if(!strcmp(expr->op, "&&") || !strcmp(expr->op, "||")) {
        if((expr->op[0] == '&') != (truth != 0) || f->contexts >= RANGE_CONTEXT)
            return;
        condition_facts(f, expr->left, truth, statement, depth + 1, facts);
        push_context(f, expr->left, truth, statement);
        condition_facts(f, expr->right, truth, statement, depth + 1, facts);
        ((RangeFacts *)f)->contexts--;
        return;
    }
    for(int i = 0; i < 6; i++)
        if(!strcmp(expr->op, ops[i]))
            op = i;
    if(op < 0 || !compares_values(f, expr->left, expr->right) ||
       !to_poly(f, expr->left, statement, 0, &left) || !to_poly(f, expr->right, statement, 0, &right))
        return;
    if(!truth)
        op = negate[op];
    switch(op) {
    case 0: add_fact(facts, &right, &left, 1); break;
    case 1: add_fact(facts, &right, &left, 0); break;
    case 2: add_fact(facts, &left, &right, 1); break;
    case 3: add_fact(facts, &left, &right, 0); break;
    case 4: add_fact(facts, &left, &right, 0); add_fact(facts, &right, &left, 0); break;
    default: break;
    }
}

static const GuardFacts *
guard_facts(const RangeFacts *f, int guard, int truth)
{
    RangeFacts *g = (RangeFacts *)f;
    GuardFacts *facts = &g->facts[2 * guard + (truth != 0)];
    if(facts->state == 0) {
        int budget = g->budget;
        facts->state = 1;
        facts->poly = malloc(GUARD_FACTS * sizeof(Poly));
        if(facts->poly != NULL) {
            g->budget = POLY_BUDGET;
            condition_facts(f, f->fn->stmts[guard].expr_root, truth, guard, 0, facts);
        }
        g->budget = budget;
        facts->state = 2;
    }
    return facts->state == 2 ? facts : NULL;
}

/* Keep the facts of guard that still hold at statement at. */
static void
add_valid_facts(const RangeFacts *f, int guard, int truth, int at, Poly *list, int *count, int max)
{
    const GuardFacts *facts = guard_facts(f, guard, truth);
    int loop = f->fn->stmts[guard].kind == ZIR_STMT_WHILE;
    for(int i = 0; facts != NULL && i < facts->count && *count < max; i++) {
        const Poly *p = &facts->poly[i];
        int valid = 1;
        for(int t = 0; t < p->count && valid; t++)
            for(int a = 0; a < p->term[t].degree && valid; a++) {
                int atom = p->term[t].atom[a];
                if(atom < f->bindings && !invariant(f, atom))
                    valid = loop ? unchanged_since_condition(f, atom, guard, at) :
                                   unchanged_between(f, atom, guard, at);
            }
        if(valid)
            list[(*count)++] = *p;
    }
}

static int
gather_facts(const RangeFacts *f, int at, Poly *list, int max)
{
    int count = 0;
    for(int x = at; x >= 0; x = f->parent_of[x]) {
        int open = f->parent_of[x];
        for(int guard = f->fn->stmts[x].for_step ? -1 : f->prev_guard[x]; guard >= 0;
            guard = f->prev_guard[guard])
            add_valid_facts(f, guard, 0, at, list, &count, max);
        if(open < 0)
            break;
        if(f->fn->stmts[open].expr_root >= 0 &&
           (f->fn->stmts[open].kind == ZIR_STMT_IF || f->fn->stmts[open].kind == ZIR_STMT_WHILE))
            add_valid_facts(f, open, 1, at, list, &count, max);
    }
    return count;
}


/* Whether p >= 0 at statement at: from the ranges of what it names, or
 * after taking away up to depth facts that share a binding with it, each
 * never negative. stride >= 0 follows from stride - width >= 0 and, inside
 * for j: 0..width - 1, width - 1 - j >= 0 with j >= 0. */
static int
poly_nonnegative(const RangeFacts *f, const Poly *p, int at, const Poly *facts, int count,
                 int depth)
{
    Range range;
    if(poly_range(f, p, at, &range) && range.low >= 0)
        return 1;
    for(int i = 0; depth > 0 && i < count; i++) {
        Poly rest;
        int shares = 0;
        for(int t = 0; t < p->count && !shares; t++)
            for(int a = 0; a < p->term[t].degree && !shares; a++)
                for(int u = 0; u < facts[i].count && !shares; u++)
                    for(int b = 0; b < facts[i].term[u].degree && !shares; b++)
                        shares = p->term[t].atom[a] == facts[i].term[u].atom[b];
        if(shares && poly_add(&rest, p, &facts[i], 1) &&
           poly_nonnegative(f, &rest, at, facts, count, depth - 1))
            return 1;
    }
    return 0;
}

/* Whether every term of p is a positive multiple of bindings each never
 * negative, and its constant is not negative either. */
static int
terms_nonnegative(const RangeFacts *f, const Poly *p, int at, const Poly *facts, int count)
{
    if(p->k.low < 0)
        return 0;
    for(int t = 0; t < p->count; t++) {
        if(p->term[t].coef < 0)
            return 0;
        for(int a = 0; a < p->term[t].degree; a++) {
            Poly atom;
            poly_atom(&atom, p->term[t].atom[a]);
            if(!poly_nonnegative(f, &atom, at, facts, count, 2))
                return 0;
        }
    }
    return 1;
}

/* Whether q <= limit - 1 at statement at once q names only invariant
 * bindings: directly, or as d = fact + rest where a fact is never negative. */
static int
poly_below_limit(const RangeFacts *f, const Poly *q, int at, const Poly *facts, int count,
                 const Poly *limit)
{
    Poly d, one;
    poly_constant(&one, 1, 1);
    return poly_add(&d, limit, q, 1) && poly_add(&d, &d, &one, 1) &&
           poly_nonnegative(f, &d, at, facts, count, 3);
}

enum { SUBSTITUTE_TRIES = 64 };

/* Replace each written binding in q by an upper bound, a fact bound - v >= 0
 * or its largest value, where q only grows with v, until q is below limit.
 * Every fact bounding v is tried in turn, since the nearest one, such as
 * i <= 31 from a guard, may be weaker than an outer one such as
 * i <= height - 1. Afterwards q is at least the index everywhere at
 * statement at. */
static int
substitute_bounds(const RangeFacts *f, const Poly *start, int at, const Poly *facts, int count,
                  const Poly *limit, int round, int *tries)
{
    {
        const Poly *q = start;
        Poly rest, grows, bound, product, next;
        int v = -1;
        for(int t = 0; t < q->count && v < 0; t++)
            for(int a = 0; a < q->term[t].degree; a++)
                if(q->term[t].atom[a] < f->bindings && !invariant(f, q->term[t].atom[a])) {
                    v = q->term[t].atom[a];
                    break;
                }
        if(v < 0)
            return poly_below_limit(f, q, at, facts, count, limit);
        if(round >= 8 || ++*tries > SUBSTITUTE_TRIES)
            return 0;
        /* q = rest + grows * v */
        poly_constant(&rest, q->k.low, q->k.high);
        poly_constant(&grows, 0, 0);
        for(int t = 0; t < q->count; t++) {
            Term term = q->term[t];
            int uses = 0;
            for(int a = 0; a < term.degree; a++)
                uses += term.atom[a] == v;
            if(uses == 0) {
                if(!poly_add_term(&rest, &term))
                    return 0;
                continue;
            }
            if(uses > 1)
                return 0;
            for(int a = 0, n = 0; a < term.degree; a++)
                if(term.atom[a] != v)
                    term.atom[n++] = term.atom[a];
            term.degree--;
            if(term.degree == 0) {
                if(!add_checked(grows.k.low, term.coef, &grows.k.low))
                    return 0;
                grows.k.high = grows.k.low;
            } else if(!poly_add_term(&grows, &term)) {
                return 0;
            }
        }
        if(!poly_nonnegative(f, &grows, at, facts, count, 2))
            return 0;
        for(int i = 0; i < count; i++) {
            int term = -1, uses = 0;
            for(int t = 0; t < facts[i].count; t++)
                for(int a = 0; a < facts[i].term[t].degree; a++)
                    if(facts[i].term[t].atom[a] == v) {
                        uses++;
                        term = t;
                    }
            if(uses == 1 && facts[i].term[term].degree == 1 && facts[i].term[term].coef == -1) {
                Poly atom;
                poly_atom(&atom, v);
                if(poly_add(&bound, &facts[i], &atom, 0) &&
                   poly_multiply(&product, &grows, &bound) && poly_add(&next, &rest, &product, 0) &&
                   substitute_bounds(f, &next, at, facts, count, limit, round + 1, tries))
                    return 1;
            }
        }
        {
            Range largest;
            if(!binding_range(f, v, at, 0, &largest))
                return 0;
            poly_constant(&bound, largest.high, largest.high);
        }
        return poly_multiply(&product, &grows, &bound) && poly_add(&next, &rest, &product, 0) &&
               substitute_bounds(f, &next, at, facts, count, limit, round + 1, tries);
    }
}

enum { INDEX_FACTS = 48 };

/* Whether the index expression lies in 0..limit-1 at its statement. */
static int
poly_index_below(const RangeFacts *f, int index, const Poly *limit)
{
    Poly q, *facts;
    Range range;
    int at = f->statement_of[index], count, proven, tries = 0;
    /* The index must not wrap; its sign may still need the facts. */
    if(at < 0 || !expr_range(f, index, 0, &range) || !to_poly(f, index, at, 0, &q))
        return 0;
    facts = malloc(INDEX_FACTS * sizeof(*facts));
    if(facts == NULL)
        return 0;
    count = gather_facts(f, at, facts, INDEX_FACTS);
    if(range.low < 0 && !terms_nonnegative(f, &q, at, facts, count)) {
        free(facts);
        return 0;
    }
    proven = substitute_bounds(f, &q, at, facts, count, limit, 0, &tries);
    free(facts);
    return proven;
}

int
IndexInRange(const Emitter *e, int index, int64_t count)
{
    Range range;
    if(e->ranges == NULL || e->ranges->fn != e->fn || count <= 0)
        return 0;
    e->ranges->budget = RANGE_BUDGET;
    if(expr_range(e->ranges, index, 0, &range) && range.low >= 0 && range.high < count)
        return 1;
    {
        Poly limit;
        poly_constant(&limit, count, count);
        e->ranges->budget = POLY_BUDGET;
        return poly_index_below(e->ranges, index, &limit);
    }
}

/* The binding whose count limit is when limit reads view.count. */
static int
count_of(const RangeFacts *f, int limit)
{
    const ZirExpr *expr = &f->fn->exprs[limit];
    if(expr->kind != ZIR_EXPR_MEMBER || expr->name == NULL || strcmp(expr->name, "count") ||
       f->fn->exprs[expr->left].kind != ZIR_EXPR_IDENT)
        return -1;
    return f->binding_of[expr->left];
}

/* Whether limit is view.count - 1 or a binding only ever holding it. */
static int
count_minus_one(const RangeFacts *f, int limit, int view, int depth)
{
    const ZirExpr *expr = &f->fn->exprs[limit];
    int64_t one;
    if(depth > RANGE_DEPTH)
        return 0;
    if(expr->kind == ZIR_EXPR_BINARY && !strcmp(expr->op, "-") &&
       count_of(f, expr->left) == view && f->fn->exprs[expr->right].kind == ZIR_EXPR_INT &&
       literal_value(f->fn->exprs[expr->right].text, &one) && one == 1)
        return 1;
    if(expr->kind == ZIR_EXPR_IDENT && f->binding_of[limit] >= 0 &&
       f->binding_of[limit] < f->statements) {
        int binding = f->binding_of[limit];
        return !f->unknown[binding] && f->first_write[binding] < 0 &&
               f->fn->stmts[binding].expr_root >= 0 &&
               count_minus_one(f, f->fn->stmts[binding].expr_root, view, depth + 1);
    }
    return 0;
}

/* Whether condition holding means counter < view.count. */
static int
condition_below_count(const RangeFacts *f, int condition, int counter, int view, int depth)
{
    const ZirExpr *expr;
    if(condition < 0 || depth > RANGE_DEPTH)
        return 0;
    expr = &f->fn->exprs[condition];
    if(expr->kind != ZIR_EXPR_BINARY)
        return 0;
    if(!strcmp(expr->op, "&&"))
        return condition_below_count(f, expr->left, counter, view, depth + 1) ||
               condition_below_count(f, expr->right, counter, view, depth + 1);
    if(f->fn->exprs[expr->left].kind != ZIR_EXPR_IDENT ||
       f->binding_of[expr->left] != counter ||
       f->fn->exprs[expr->left].type == NULL || f->fn->exprs[expr->right].type == NULL ||
       strcmp(canonical(f->fn->exprs[expr->left].type), "s64") ||
       strcmp(canonical(f->fn->exprs[expr->right].type), "s64"))
        return 0;
    if(!strcmp(expr->op, "<"))
        return count_of(f, expr->right) == view;
    if(!strcmp(expr->op, "<="))
        return count_minus_one(f, expr->right, view, 0);
    return 0;
}

static int
counter_below_count(const Emitter *e, int index, int base)
{
    const RangeFacts *f = e->ranges;
    Range range;
    int view, counter, at;
    if(f == NULL || f->fn != e->fn || index < 0 || base < 0 ||
       f->fn->exprs[base].kind != ZIR_EXPR_IDENT || f->fn->exprs[index].kind != ZIR_EXPR_IDENT)
        return 0;
    e->ranges->budget = RANGE_BUDGET;
    view = f->binding_of[base];
    counter = f->binding_of[index];
    at = f->statement_of[index];
    /* The view's count is fixed when nothing writes the view itself. */
    if(view < 0 || counter < 0 || at < 0 || f->unknown[view] || f->unknown[counter] ||
       f->first_write[view] >= 0 || !expr_range(f, index, 0, &range) || range.low < 0)
        return 0;
    for(int loop = f->loop_of[at]; loop >= 0; loop = f->loop_of[loop])
        if(condition_below_count(f, f->fn->stmts[loop].expr_root, counter, view, 0) &&
           unchanged_since_condition(f, counter, loop, at))
            return 1;
    return 0;
}

int
IndexBelowCount(const Emitter *e, int index, int base)
{
    const RangeFacts *f = e->ranges;
    Poly limit;
    int view;
    if(f == NULL || f->fn != e->fn || index < 0 || base < 0)
        return 0;
    if(counter_below_count(e, index, base))
        return 1;
    view = f->fn->exprs[base].kind == ZIR_EXPR_IDENT ? f->binding_of[base] : -1;
    if(!invariant(f, view))
        return 0;
    poly_atom(&limit, f->bindings + view);
    e->ranges->budget = POLY_BUDGET;
    return poly_index_below(f, index, &limit);
}
