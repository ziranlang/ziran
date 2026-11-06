/* A fresh Text that is only read during one call, as in
 * `Count(JsonWrite(...))` or `Render(a) == Render(b)`,
 * stays owned: the checker gave it to vec.TextDetach, which keeps its bytes
 * for the rest of the program, and this pass instead keeps it in a scoped
 * local declared just before the statement and lends that local's view:
 *
 *     text_scope_1: vec.Text = JsonWrite(...)
 *     Count(TextView(text_scope_1))
 *
 * The local is freed when its block closes, as every Text local is. A
 * string parameter qualifies when the callee's checked body provably only
 * reads it during the call: it compares, counts, indexes or slices it, or
 * hands it to a parameter that does the same, to print, BuilderAppend or
 * TextCopy. Returning, storing, aliasing or passing it to a foreign or
 * unknown callee keeps the old detached string. The rewrite only moves the
 * call when nothing evaluated before it in the statement can observe the
 * move, and never out of a loop or else-if condition or an `&&`, `||` or
 * `ifx` arm. Saved IR holds the rewritten form, so loading it finds
 * nothing more to do. */
#include "zir_memory.h"
#include "zir_check_internal.h"

#include <stdio.h>

enum { SCOPE_UNKNOWN, SCOPE_COMPUTING, SCOPE_READ_ONLY, SCOPE_RETAINED };

typedef struct ScopeAnswer {
    const ZirFunction *fn;
    unsigned char state[64];
} ScopeAnswer;

typedef struct TextScope {
    ScopeAnswer *answers;
    size_t capacity;
    size_t count;
    int failed;
} TextScope;

/* Parents of a function's reachable expressions: an expression index, or
 * -2 - statement for a statement root, or -1 when unreachable. */
typedef struct Parents {
    int *of;
    int count;
} Parents;

static int
parents_build(const ZirFunction *fn, Parents *parents)
{
    parents->count = fn->expr_count;
    parents->of = malloc(((size_t)fn->expr_count + 1) * sizeof(int));
    int *pending = malloc(((size_t)fn->expr_count + 1) * sizeof(int));
    if(parents->of == NULL || pending == NULL) {
        free(parents->of);
        free(pending);
        parents->of = NULL;
        return 0;
    }
    for(int i = 0; i < fn->expr_count; i++)
        parents->of[i] = -1;
    int count = 0;
    for(int s = 0; s < fn->stmt_count; s++) {
        int roots[2] = {fn->stmts[s].expr_root, fn->stmts[s].lhs_root};
        for(int r = 0; r < 2; r++)
            if(roots[r] >= 0 && roots[r] < fn->expr_count &&
               parents->of[roots[r]] == -1) {
                parents->of[roots[r]] = -2 - s;
                pending[count++] = roots[r];
            }
    }
    while(count > 0) {
        int index = pending[--count];
        const ZirExpr *e = &fn->exprs[index];
        int children[3] = {e->left, e->right, e->third};
        for(int c = 0; c < 3; c++)
            if(children[c] >= 0 && children[c] < fn->expr_count &&
               parents->of[children[c]] == -1 && count < fn->expr_count) {
                parents->of[children[c]] = index;
                pending[count++] = children[c];
            }
        for(int child = e->first_child; child >= 0 && child < fn->expr_count;
            child = fn->exprs[child].next_sibling)
            if(parents->of[child] == -1 && count < fn->expr_count) {
                parents->of[child] = index;
                pending[count++] = child;
            }
    }
    free(pending);
    return 1;
}

static int
comparison(const char *op)
{
    return !strcmp(op, "==") || !strcmp(op, "!=") || !strcmp(op, "<") ||
        !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=");
}

static int parameter_read_only(TextScope *scope, const ZirModule *module,
                               const ZirFunction *fn, int parameter);

/* Whether argument ARGUMENT of call CALL is only read during the call. */
static int
argument_read_only(TextScope *scope, const ZirModule *module,
                   const ZirFunction *fn, int call, int argument)
{
    const ZirExpr *e = &fn->exprs[call];
    if(e->left >= 0 || e->is_function_value || !e->name || !*e->name)
        return 0;
    if(!strcmp(e->name, "print") || !strcmp(e->name, "TextCopy"))
        return 1;
    if(!strcmp(e->name, "BuilderAppend"))
        return e->first_child != argument;
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    if(ResolveFunction(module, e->name, &owner, &callee) != 1 ||
       callee == NULL || owner == NULL || callee->is_extern ||
       callee->is_template || callee->stmt_count == 0)
        return 0;
    int index = fn->exprs[argument].argument_index;
    if(index < 0 || index >= 64)
        return 0;
    return parameter_read_only(scope, owner, callee, index);
}

/* Whether the string VALUE is only read where it is used. */
static int
value_read_only(TextScope *scope, const ZirModule *module,
                const ZirFunction *fn, const Parents *parents, int value)
{
    int parent = parents->of[value];
    if(parent < -1) {
        const ZirStmt *st = &fn->stmts[-2 - parent];
        return (st->kind == ZIR_STMT_EXPR || st->kind == ZIR_STMT_UNUSED) &&
            st->expr_root == value;
    }
    if(parent < 0)
        return 0;
    const ZirExpr *e = &fn->exprs[parent];
    switch(e->kind) {
    case ZIR_EXPR_MEMBER:
        return e->left == value && !strcmp(e->name, "count");
    case ZIR_EXPR_INDEX:
        return e->left == value;
    case ZIR_EXPR_SLICE:
        return e->left == value &&
            value_read_only(scope, module, fn, parents, parent);
    case ZIR_EXPR_BINARY:
        return comparison(e->op);
    case ZIR_EXPR_CALL:
        return argument_read_only(scope, module, fn, parent, value);
    default:
        return 0;
    }
}

static ScopeAnswer *
answer_for(TextScope *scope, const ZirFunction *fn)
{
    if((scope->count + 1) * 2 > scope->capacity) {
        size_t capacity = scope->capacity ? scope->capacity * 2 : 256;
        ScopeAnswer *grown = calloc(capacity, sizeof(*grown));
        if(grown == NULL) {
            scope->failed = 1;
            return NULL;
        }
        for(size_t i = 0; i < scope->capacity; i++) {
            if(scope->answers[i].fn == NULL) continue;
            size_t slot = ((uintptr_t)scope->answers[i].fn >> 4) & (capacity - 1);
            while(grown[slot].fn != NULL) slot = (slot + 1) & (capacity - 1);
            grown[slot] = scope->answers[i];
        }
        free(scope->answers);
        scope->answers = grown;
        scope->capacity = capacity;
    }
    size_t mask = scope->capacity - 1;
    size_t slot = ((uintptr_t)fn >> 4) & mask;
    while(scope->answers[slot].fn != NULL && scope->answers[slot].fn != fn)
        slot = (slot + 1) & mask;
    if(scope->answers[slot].fn == NULL) {
        scope->answers[slot].fn = fn;
        scope->count++;
    }
    return &scope->answers[slot];
}

/* Whether FN's string parameter PARAMETER is only read during the call.
 * A recursive query answers no. */
static int
parameter_read_only(TextScope *scope, const ZirModule *module,
                    const ZirFunction *fn, int parameter)
{
    ScopeAnswer *answer = answer_for(scope, fn);
    if(answer == NULL)
        return 0;
    if(answer->state[parameter] != SCOPE_UNKNOWN)
        return answer->state[parameter] == SCOPE_READ_ONLY;
    answer->state[parameter] = SCOPE_COMPUTING;
    int result = 0;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    const ZirParameter *item = parameter < parameters->count ?
        &parameters->items[parameter] : NULL;
    const char *name = item ? (item->binding_name ? item->binding_name : item->name) : NULL;
    const char *type = item ? (item->value_type ? item->value_type : item->type) : NULL;
    if(name != NULL && type != NULL && !item->is_using &&
       !strcmp(skip_ws(type), "string")) {
        result = 1;
        for(int s = 0; result && s < fn->stmt_count; s++)
            if(fn->stmts[s].kind == ZIR_STMT_DECL && fn->stmts[s].name &&
               !strcmp(fn->stmts[s].name, name))
                result = 0;
        Parents parents = {0};
        if(result && !parents_build(fn, &parents)) {
            scope->failed = 1;
            result = 0;
        }
        for(int x = 0; result && x < fn->expr_count; x++) {
            const ZirExpr *e = &fn->exprs[x];
            if(e->kind == ZIR_EXPR_IDENT && !e->is_this && e->name &&
               !strcmp(e->name, name) && parents.of[x] != -1 &&
               !value_read_only(scope, module, fn, &parents, x))
                result = 0;
        }
        free(parents.of);
    }
    /* The table may have grown during the walk. */
    answer = answer_for(scope, fn);
    if(answer != NULL)
        answer->state[parameter] = result ? SCOPE_READ_ONLY : SCOPE_RETAINED;
    return result;
}

static int
text_detach_call(const ZirFunction *fn, int index)
{
    const ZirExpr *e = &fn->exprs[index];
    if(e->kind != ZIR_EXPR_CALL || e->name == NULL || e->first_child < 0 ||
       fn->exprs[e->first_child].next_sibling >= 0 ||
       fn->exprs[e->first_child].kind != ZIR_EXPR_CALL)
        return 0;
    return !strcmp(e->name, "vec.TextDetach") ||
        !strcmp(e->name, "std_vec.TextDetach");
}

static int
scoped_name(const char *name)
{
    return name != NULL && !strncmp(name, "text_scope_", 11);
}

/* Nothing in INDEX's tree can observe a call moved before it: literals,
 * local reads and earlier scoped views. */
static int
settled(const ZirFunction *fn, int index)
{
    if(index < 0)
        return 1;
    const ZirExpr *e = &fn->exprs[index];
    switch(e->kind) {
    case ZIR_EXPR_INT: case ZIR_EXPR_FLOAT: case ZIR_EXPR_STRING:
    case ZIR_EXPR_SIZE_OF:
        return 1;
    case ZIR_EXPR_IDENT:
        return !e->is_global_value;
    case ZIR_EXPR_CALL:
        return !strcmp(e->name, "TextView") && e->first_child >= 0 &&
            fn->exprs[e->first_child].kind == ZIR_EXPR_IDENT &&
            scoped_name(fn->exprs[e->first_child].name);
    case ZIR_EXPR_MEMBER: case ZIR_EXPR_INDEX: case ZIR_EXPR_SLICE:
    case ZIR_EXPR_BINARY: case ZIR_EXPR_CAST: case ZIR_EXPR_FIELD_INIT:
    case ZIR_EXPR_COMPOUND:
        if(e->kind == ZIR_EXPR_BINARY &&
           (!strcmp(e->op, "&&") || !strcmp(e->op, "||")))
            return 0;
        if(!settled(fn, e->left) || !settled(fn, e->right) ||
           !settled(fn, e->third))
            return 0;
        for(int child = e->first_child; child >= 0; child = fn->exprs[child].next_sibling)
            if(!settled(fn, child))
                return 0;
        return 1;
    default:
        return 0;
    }
}

/* Whether the tree at INDEX takes an address another expression reads. */
static int
takes_address(const ZirFunction *fn, int index)
{
    if(index < 0)
        return 0;
    const ZirExpr *e = &fn->exprs[index];
    if(e->kind == ZIR_EXPR_UNARY && (!strcmp(e->op, "*") || !strcmp(e->op, "&")))
        return 1;
    if(takes_address(fn, e->left) || takes_address(fn, e->right) ||
       takes_address(fn, e->third))
        return 1;
    for(int child = e->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        if(takes_address(fn, child))
            return 1;
    return 0;
}

/* Whether CHILD of PARENT runs after only settled siblings, so the
 * statement may evaluate CHILD's call first. */
static int
earlier_settled(const ZirFunction *fn, int parent, int child)
{
    const ZirExpr *e = &fn->exprs[parent];
    if(e->kind == ZIR_EXPR_CONDITIONAL ||
       (e->kind == ZIR_EXPR_BINARY &&
        (!strcmp(e->op, "&&") || !strcmp(e->op, "||"))))
        return 0;
    int order[3] = {e->left, e->right, e->third};
    for(int i = 0; i < 3; i++) {
        if(order[i] == child)
            return 1;
        if(!settled(fn, order[i]))
            return 0;
    }
    for(int sibling = e->first_child; sibling >= 0; sibling = fn->exprs[sibling].next_sibling) {
        if(sibling == child)
            return 1;
        if(!settled(fn, sibling))
            return 0;
    }
    return 0;
}

/* The statement whose evaluation can start with CALL, or -1. */
static int
hoist_statement(const ZirFunction *fn, const Parents *parents, int call)
{
    int node = call;
    while(parents->of[node] >= 0) {
        if(!earlier_settled(fn, parents->of[node], node))
            return -1;
        node = parents->of[node];
    }
    if(parents->of[node] == -1)
        return -1;
    int s = -2 - parents->of[node];
    const ZirStmt *st = &fn->stmts[s];
    if(st->expr_root != node)
        return -1;
    switch(st->kind) {
    case ZIR_STMT_EXPR: case ZIR_STMT_UNUSED: case ZIR_STMT_DECL:
    case ZIR_STMT_RETURN:
        return s;
    case ZIR_STMT_IF:
        /* An else-if condition only runs when earlier branches fail. */
        return st->is_else ? -1 : s;
    case ZIR_STMT_ASSIGN:
        /* A for-loop step is where continue jumps; it stays first. */
        return !st->for_step && settled(fn, st->lhs_root) ? s : -1;
    default:
        return -1;
    }
}

static int
name_used(const ZirFunction *fn, const char *name)
{
    for(int i = 0; i < fn->expr_count; i++)
        if(fn->exprs[i].name && !strcmp(fn->exprs[i].name, name))
            return 1;
    for(int s = 0; s < fn->stmt_count; s++)
        if(fn->stmts[s].kind == ZIR_STMT_DECL && fn->stmts[s].name &&
           !strcmp(fn->stmts[s].name, name))
            return 1;
    return 0;
}

/* Declares a scoped local holding Text call CALL before statement S and
 * turns VIEW, the node that used CALL's string, into a view of that local. */
static int
scope_text(ZirFunction *fn, int view_node, int call, int s, int *serial)
{
    char name[ZIR_NAME_MAX];
    do snprintf(name, sizeof(name), "text_scope_%d", ++*serial);
    while(name_used(fn, name));
    ZirExpr *local = FunctionAddExpr(fn, ZIR_EXPR_IDENT, name, fn->exprs[view_node].span);
    if(local == NULL)
        return 0;
    local->name = KeepName(name);
    local->type = fn->exprs[call].type;
    int view = (int)(local - fn->exprs);
    if(fn->stmt_count + 1 > fn->stmt_cap) {
        int capacity = fn->stmt_cap > 0 ? fn->stmt_cap * 2 : 16;
        ZirStmt *stmts = realloc(fn->stmts, (size_t)capacity * sizeof(ZirStmt));
        if(stmts == NULL)
            return 0;
        fn->stmts = stmts;
        fn->stmt_cap = capacity;
    }
    memmove(&fn->stmts[s + 1], &fn->stmts[s],
            (size_t)(fn->stmt_count - s) * sizeof(ZirStmt));
    fn->stmt_count++;
    ZirStmt *decl = &fn->stmts[s];
    memset(decl, 0, sizeof(*decl));
    decl->kind = ZIR_STMT_DECL;
    decl->text = fn->exprs[call].text;
    decl->name = KeepName(name);
    decl->type = fn->exprs[call].type;
    decl->expr_root = call;
    decl->lhs_root = -1;
    decl->span = fn->stmts[s + 1].span;
    ZirExpr *moved = &fn->exprs[call];
    moved->next_sibling = -1;
    moved->argument_index = -1;
    moved->argument_name = "";
    moved->text_owned = 1;
    ZirExpr *lent = &fn->exprs[view_node];
    lent->name = KeepName("TextView");
    lent->first_child = view;
    return 1;
}

/* The vec module whose TextFromBuilder can take over BuilderFinish call
 * INDEX, which gives a string, or NULL. */
static const char *
builder_text_module(const ZirModule *module, const ZirFunction *fn, int index)
{
    static const char *const modules[] = {"vec", "std_vec"};
    const ZirExpr *e = &fn->exprs[index];
    if(e->kind != ZIR_EXPR_CALL || e->left >= 0 || e->name == NULL ||
       strcmp(e->name, "BuilderFinish") || e->type == NULL ||
       strcmp(e->type, "string") || e->first_child < 0 ||
       fn->exprs[e->first_child].next_sibling >= 0)
        return NULL;
    for(size_t i = 0; i < sizeof(modules) / sizeof(*modules); i++) {
        char qualified[ZIR_NAME_MAX];
        const ZirModule *owner = NULL;
        const ZirFunction *found = NULL;
        if(!strcmp(module->name, modules[i]))
            return NULL;
        snprintf(qualified, sizeof(qualified), "%s.TextFromBuilder", modules[i]);
        if(ResolveFunction(module, qualified, &owner, &found) == 1)
            return modules[i];
    }
    return NULL;
}

/* Adds TextFromBuilder over the builder of BuilderFinish call FINISH from
 * VEC_MODULE and returns its index, or -1. */
static int
builder_text_call(ZirFunction *fn, int finish, const char *vec_module)
{
    char name[ZIR_NAME_MAX], type[ZIR_NAME_MAX];
    snprintf(name, sizeof(name), "%s.TextFromBuilder", vec_module);
    snprintf(type, sizeof(type), "%s.Text", vec_module);
    ZirExpr *made = FunctionAddExpr(fn, ZIR_EXPR_CALL, fn->exprs[finish].text,
                                    fn->exprs[finish].span);
    if(made == NULL)
        return -1;
    int index = (int)(made - fn->exprs);
    *made = fn->exprs[finish];
    made->name = KeepName(name);
    made->type = KeepName(type);
    /* A checked call numbers its arguments and moves an owned one; the
     * BuilderFinish primitive did neither and cleared the builder itself. */
    fn->exprs[made->first_child].argument_index = 0;
    fn->exprs[made->first_child].is_move = 1;
    fn->exprs[finish].first_child = -1;
    return index;
}

static int
scope_function(TextScope *scope, const ZirModule *module, ZirFunction *fn)
{
    int serial = 0;
    for(;;) {
        Parents parents = {0};
        if(!parents_build(fn, &parents))
            return 0;
        int done = 1;
        for(int x = 0; x < fn->expr_count && done; x++) {
            /* A statement root is a discarded result, freed where it stands. */
            const char *vec_module = NULL;
            if(parents.of[x] < 0 ||
               (!text_detach_call(fn, x) &&
                (vec_module = builder_text_module(module, fn, x)) == NULL))
                continue;
            if(!value_read_only(scope, module, fn, &parents, x) ||
               takes_address(fn, fn->exprs[x].first_child))
                continue;
            int s = hoist_statement(fn, &parents, x);
            if(s < 0)
                continue;
            int call = vec_module ? builder_text_call(fn, x, vec_module) :
                fn->exprs[x].first_child;
            if(call < 0 || !scope_text(fn, x, call, s, &serial)) {
                free(parents.of);
                return 0;
            }
            done = 0;
        }
        free(parents.of);
        /* Saved IR lists children before parents and statements in order. */
        if(done)
            return serial == 0 || rebuild_conversion_layout(fn);
    }
}

int
ScopeTextArguments(ZirProgram **programs, int count)
{
    TextScope scope = {0};
    int ok = 1;
    for(int p = 0; ok && p < count; p++)
        for(int m = 0; ok && m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int f = 0; ok && f < module->function_count; f++) {
                ZirFunction *fn = &module->functions[f];
                if(fn->is_template || fn->is_extern || fn->stmt_count == 0)
                    continue;
                if(!scope_function(&scope, module, fn) || scope.failed)
                    ok = 0;
            }
        }
    free(scope.answers);
    if(!ok)
        fprintf(stderr, "ziran: out of memory scoping text arguments\n");
    return ok;
}
