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

enum { RANGE_DEPTH = 8, RANGE_BUDGET = 4096 };

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
    /* Steps left for the current question; a long chain of bindings
     * stops being followed instead of taking exponential time. */
    int budget;
    int broken;
} RangeFacts;

typedef struct Range {
    int64_t low, high;
} Range;

static int expr_range(const RangeFacts *f, int index, int depth, Range *out);

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
    int *opens, *heads, scope_count = 0, open_count = 0;
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
       !heads || !walk.pending) {
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
        f->block_end[s] = f->loop_of[s] = f->next_write[s] = -1;
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
            f->block_end[s] = scope_count;
        }
        if(st->kind == ZIR_STMT_BLOCK_CLOSE && open_count > 0) {
            int open = opens[--open_count];
            while(scope_count > f->block_end[open]) {
                scope_count--;
                heads[scope[scope_count].bucket] = scope[scope_count].previous;
            }
            f->block_end[open] = s;
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

static int
multiply_range(Range a, Range b, Range *out)
{
    const int64_t limit = INT64_C(1) << 31;
    int64_t products[4];
    if(a.low < -limit || a.high > limit || b.low < -limit || b.high > limit)
        return 0;
    products[0] = a.low * b.low;
    products[1] = a.low * b.high;
    products[2] = a.high * b.low;
    products[3] = a.high * b.high;
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
static int
unchanged_since_condition(const RangeFacts *f, int binding, int loop, int at)
{
    for(int s = f->first_write[binding]; s >= 0; s = f->next_write[s])
        if(s > loop && s < f->block_end[loop] && (s < at || f->loop_of[s] != loop))
            return 0;
    return 1;
}

static int
binding_range(const RangeFacts *f, int binding, int at, int depth, Range *out)
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
    } else if(fn->stmts[binding].expr_root < 0 ||
              !expr_range(f, fn->stmts[binding].expr_root, depth + 1, &value)) {
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

int
IndexInRange(const Emitter *e, int index, int64_t count)
{
    Range range;
    if(e->ranges == NULL || e->ranges->fn != e->fn || count <= 0)
        return 0;
    e->ranges->budget = RANGE_BUDGET;
    return expr_range(e->ranges, index, 0, &range) && range.low >= 0 && range.high < count;
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

int
IndexBelowCount(const Emitter *e, int index, int base)
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
