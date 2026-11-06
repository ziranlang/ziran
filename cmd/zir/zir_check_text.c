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
 * TextCopy. Reads also follow `ifx` arms, string locals that only read
 * it, a callee that hands it back when its result is only read, and a
 * record literal field passed to a callee whose by-value record parameter
 * only reads that field. Returning, storing, aliasing or passing it to a
 * foreign or unknown callee keeps the old detached string. The rewrite only moves the
 * call when nothing evaluated before it in the statement can observe the
 * move: literals, locals, and globals when the moved call provably writes
 * none. It never moves one out of a loop or else-if condition or an `&&`,
 * `||` or `ifx` arm. A string local initialized from a fresh Text, as in
 * `line := Render(...)`, becomes that Text the same way when every use of
 * it after its declaration and inside its block only reads it. Saved IR
 * holds the rewritten form, so loading it finds nothing more to do. */
#include "zir_memory.h"
#include "zir_check_internal.h"

#include <ctype.h>
#include <stdio.h>

enum { SCOPE_UNKNOWN, SCOPE_COMPUTING, SCOPE_READ_ONLY, SCOPE_RETAINED,
       SCOPE_RETURNED };

/* Where a string goes: kept past its use, only read, or only read and
 * handed back as the enclosing procedure's result. */
enum { FLOW_RETAINED, FLOW_READ, FLOW_RETURNED };

typedef struct ScopeAnswer {
    const ZirFunction *fn;
    unsigned char state[64];
    /* Whether a call may write a global: SCOPE_UNKNOWN, SCOPE_COMPUTING,
     * SCOPE_READ_ONLY for no or SCOPE_RETAINED for maybe. */
    unsigned char writes;
} ScopeAnswer;

/* Where record parameter PARAMETER's string field FIELD goes. */
typedef struct FieldAnswer {
    const ZirFunction *fn;
    int parameter;
    const char *field;
    unsigned char state;
} FieldAnswer;

typedef struct TextScope {
    ScopeAnswer *answers;
    size_t capacity;
    size_t count;
    FieldAnswer *fields;
    size_t field_count;
    size_t field_capacity;
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

static int parameter_flow(TextScope *scope, const ZirModule *module,
                          const ZirFunction *fn, int parameter,
                          const char *field);
static int value_flow(TextScope *scope, const ZirModule *module,
                      const ZirFunction *fn, const Parents *parents, int value);
static int local_flow(TextScope *scope, const ZirModule *module,
                      const ZirFunction *fn, const Parents *parents, int s);

static int pointer_type(const char *type);

static int
combine(int a, int b)
{
    if(a == FLOW_RETAINED || b == FLOW_RETAINED)
        return FLOW_RETAINED;
    return a == FLOW_RETURNED || b == FLOW_RETURNED ? FLOW_RETURNED : FLOW_READ;
}

static int
reading_foreign(const ZirModule *module, const char *name)
{
    for(int i = 0; i < module->import_count; i++)
        if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
           module->imports[i].reads_arguments &&
           !strcmp(module->imports[i].name, name))
            return 1;
    return 0;
}

/* Where argument ARGUMENT of call CALL goes, or with FIELD, that field of
 * a record argument. A callee that hands the string back passes it on to
 * wherever the call's result goes. */
static int
argument_flow(TextScope *scope, const ZirModule *module, const ZirFunction *fn,
              const Parents *parents, int call, int argument, const char *field)
{
    const ZirExpr *e = &fn->exprs[call];
    if(e->left >= 0 || e->is_function_value || !e->name || !*e->name)
        return FLOW_RETAINED;
    if(field == NULL && (!strcmp(e->name, "print") || !strcmp(e->name, "TextCopy")))
        return FLOW_READ;
    if(field == NULL && !strcmp(e->name, "BuilderAppend"))
        return e->first_child != argument ? FLOW_READ : FLOW_RETAINED;
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    int found = ResolveFunction(module, e->name, &owner, &callee);
    if(found != 1 || callee == NULL || owner == NULL || callee->is_extern) {
        /* A `#reads` foreign procedure only reads its strings and bytes
         * during the call. */
        if(field == NULL && found != -1 &&
           reading_foreign(found == 1 && owner ? owner : module,
                           found == 1 && callee ? callee->name : e->name))
            return FLOW_READ;
        return FLOW_RETAINED;
    }
    if(callee->is_template || callee->stmt_count == 0)
        return FLOW_RETAINED;
    int index = fn->exprs[argument].argument_index;
    if(index < 0 || index >= 64)
        return FLOW_RETAINED;
    int flow = parameter_flow(scope, owner, callee, index, field);
    return flow == FLOW_RETURNED ? value_flow(scope, module, fn, parents, call) : flow;
}

/* Where the string VALUE goes from where it is used. */
static int
value_flow(TextScope *scope, const ZirModule *module,
           const ZirFunction *fn, const Parents *parents, int value)
{
    int parent = parents->of[value];
    if(parent < -1) {
        int s = -2 - parent;
        const ZirStmt *st = &fn->stmts[s];
        if(st->expr_root != value)
            return FLOW_RETAINED;
        if(st->kind == ZIR_STMT_EXPR || st->kind == ZIR_STMT_UNUSED)
            return FLOW_READ;
        if(st->kind == ZIR_STMT_RETURN)
            return FLOW_RETURNED;
        /* A string local that only reads its value, as in
         * `shown := ifx localize then Tr(text) else text`. */
        if(st->kind == ZIR_STMT_DECL && !st->is_using && st->lhs_root < 0 &&
           st->name != NULL && *st->name && st->type != NULL &&
           !strcmp(st->type, "string"))
            return local_flow(scope, module, fn, parents, s);
        return FLOW_RETAINED;
    }
    if(parent < 0)
        return FLOW_RETAINED;
    const ZirExpr *e = &fn->exprs[parent];
    switch(e->kind) {
    case ZIR_EXPR_MEMBER:
        return e->left == value && !strcmp(e->name, "count") ? FLOW_READ : FLOW_RETAINED;
    case ZIR_EXPR_INDEX:
        return e->left == value ? FLOW_READ : FLOW_RETAINED;
    case ZIR_EXPR_SLICE:
        return e->left == value ?
            value_flow(scope, module, fn, parents, parent) : FLOW_RETAINED;
    case ZIR_EXPR_BINARY:
        return comparison(e->op) ? FLOW_READ : FLOW_RETAINED;
    case ZIR_EXPR_CONDITIONAL:
        return e->right == value || e->third == value ?
            value_flow(scope, module, fn, parents, parent) : FLOW_RETAINED;
    case ZIR_EXPR_CALL:
        return argument_flow(scope, module, fn, parents, parent, value, NULL);
    case ZIR_EXPR_FIELD_INIT: {
        /* A field of a record literal handed straight to a call. */
        int record = parents->of[parent];
        if(e->right != value || record < 0 || !e->name || !*e->name ||
           fn->exprs[record].kind != ZIR_EXPR_COMPOUND)
            return FLOW_RETAINED;
        int call = parents->of[record];
        if(call < 0 || fn->exprs[call].kind != ZIR_EXPR_CALL)
            return FLOW_RETAINED;
        return argument_flow(scope, module, fn, parents, call, record, e->name);
    }
    default:
        return FLOW_RETAINED;
    }
}

/* Whether the string VALUE is only read where it is used. */
static int
value_read_only(TextScope *scope, const ZirModule *module,
                const ZirFunction *fn, const Parents *parents, int value)
{
    return value_flow(scope, module, fn, parents, value) == FLOW_READ;
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

static FieldAnswer *
field_answer(TextScope *scope, const ZirFunction *fn, int parameter,
             const char *field)
{
    for(size_t i = 0; i < scope->field_count; i++)
        if(scope->fields[i].fn == fn && scope->fields[i].parameter == parameter &&
           !strcmp(scope->fields[i].field, field))
            return &scope->fields[i];
    if(scope->field_count == scope->field_capacity) {
        size_t capacity = scope->field_capacity ? scope->field_capacity * 2 : 64;
        FieldAnswer *grown = realloc(scope->fields, capacity * sizeof(*grown));
        if(grown == NULL) {
            scope->failed = 1;
            return NULL;
        }
        scope->fields = grown;
        scope->field_capacity = capacity;
    }
    FieldAnswer *answer = &scope->fields[scope->field_count++];
    *answer = (FieldAnswer){fn, parameter, field, SCOPE_UNKNOWN};
    return answer;
}

static unsigned char *
flow_state(TextScope *scope, const ZirFunction *fn, int parameter,
           const char *field)
{
    if(field != NULL) {
        FieldAnswer *answer = field_answer(scope, fn, parameter, field);
        return answer ? &answer->state : NULL;
    }
    ScopeAnswer *answer = answer_for(scope, fn);
    return answer ? &answer->state[parameter] : NULL;
}

/* Where FN's string parameter PARAMETER goes during the call, or with
 * FIELD, that string field of a record parameter passed by value. A
 * recursive query answers retained. */
static int
parameter_flow(TextScope *scope, const ZirModule *module,
               const ZirFunction *fn, int parameter, const char *field)
{
    unsigned char *state = flow_state(scope, fn, parameter, field);
    if(state == NULL)
        return FLOW_RETAINED;
    switch(*state) {
    case SCOPE_READ_ONLY: return FLOW_READ;
    case SCOPE_RETURNED: return FLOW_RETURNED;
    case SCOPE_UNKNOWN: break;
    default: return FLOW_RETAINED;
    }
    *state = SCOPE_COMPUTING;
    int result = FLOW_RETAINED;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    const ZirParameter *item = parameter < parameters->count ?
        &parameters->items[parameter] : NULL;
    const char *name = item ? (item->binding_name ? item->binding_name : item->name) : NULL;
    const char *type = item ? (item->value_type ? item->value_type : item->type) : NULL;
    if(type != NULL)
        type = skip_ws(type);
    int typed = type != NULL && (field == NULL ? !strcmp(type, "string") :
        *type && strcmp(type, "string") && !pointer_type(type));
    if(name != NULL && typed && !item->is_using) {
        result = FLOW_READ;
        for(int s = 0; result && s < fn->stmt_count; s++)
            if(fn->stmts[s].kind == ZIR_STMT_DECL && fn->stmts[s].name &&
               !strcmp(fn->stmts[s].name, name))
                result = FLOW_RETAINED;
        Parents parents = {0};
        if(result && !parents_build(fn, &parents)) {
            scope->failed = 1;
            result = FLOW_RETAINED;
        }
        for(int x = 0; result && x < fn->expr_count; x++) {
            const ZirExpr *e = &fn->exprs[x];
            if(e->kind != ZIR_EXPR_IDENT || e->is_this || !e->name ||
               strcmp(e->name, name) || parents.of[x] == -1)
                continue;
            int up = parents.of[x];
            if(field == NULL)
                result = combine(result, value_flow(scope, module, fn, &parents, x));
            else if(up >= 0 && fn->exprs[up].kind == ZIR_EXPR_MEMBER &&
                    fn->exprs[up].left == x)
                /* Other fields are not this string. */
                result = strcmp(fn->exprs[up].name, field) ? result :
                    combine(result, value_flow(scope, module, fn, &parents, up));
            else if(up >= 0 && fn->exprs[up].kind == ZIR_EXPR_CALL)
                result = combine(result, argument_flow(scope, module, fn, &parents,
                                                       up, x, field));
            else
                result = FLOW_RETAINED;
        }
        free(parents.of);
    }
    /* The tables may have grown during the walk. */
    state = flow_state(scope, fn, parameter, field);
    if(state != NULL)
        *state = result == FLOW_READ ? SCOPE_READ_ONLY :
            result == FLOW_RETURNED ? SCOPE_RETURNED : SCOPE_RETAINED;
    return result;
}

static int
pointer_type(const char *type)
{
    return type != NULL && *skip_ws(type) == '*';
}

/* Whether writing through the place at INDEX may change a global: its root
 * is a global, or a pointer is followed on the way to it. */
static int
place_reaches_global(const ZirFunction *fn, int index)
{
    while(index >= 0) {
        const ZirExpr *e = &fn->exprs[index];
        if(pointer_type(e->type))
            return 1;
        if(e->kind == ZIR_EXPR_IDENT)
            return e->is_global_value;
        if(e->kind != ZIR_EXPR_MEMBER && e->kind != ZIR_EXPR_INDEX &&
           e->kind != ZIR_EXPR_SLICE)
            return 1;
        index = e->left;
    }
    return 1;
}

static int function_writes_globals(TextScope *scope, const ZirModule *module,
                                   const ZirFunction *fn);

/* Whether evaluating the call at INDEX may write a global: it calls a
 * foreign, unknown or writing procedure, hands it a place reaching a
 * global or takes a global's address. */
static int
call_writes_globals(TextScope *scope, const ZirModule *module,
                    const ZirFunction *fn, int index)
{
    const ZirExpr *e = &fn->exprs[index];
    if(e->left >= 0 || e->is_function_value || !e->name || !*e->name)
        return 1;
    for(int arg = e->first_child; arg >= 0; arg = fn->exprs[arg].next_sibling) {
        const char *type = fn->exprs[arg].type ? skip_ws(fn->exprs[arg].type) : "";
        int copied = !strcmp(type, "string") || !strcmp(type, "bool") ||
            ((type[0] == 's' || type[0] == 'u' || type[0] == 'f') &&
             isdigit((unsigned char)type[1]));
        if(!copied && place_reaches_global(fn, arg))
            return 1;
    }
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    int found = ResolveFunction(module, e->name, &owner, &callee);
    if(found != 1) {
        if(found != 0)
            return 1;
        for(int i = 0; i < module->import_count; i++)
            if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
               !strcmp(module->imports[i].name, e->name))
                return 1;
        /* A builtin such as print, TextView or BuilderAppend acts only on
         * its arguments. */
        return 0;
    }
    if(callee == NULL || owner == NULL || callee->is_extern ||
       callee->is_template || callee->stmt_count == 0)
        return 1;
    return function_writes_globals(scope, owner, callee);
}

/* Whether the tree at INDEX may write a global when evaluated. */
static int
tree_writes_globals(TextScope *scope, const ZirModule *module,
                    const ZirFunction *fn, int index)
{
    if(index < 0)
        return 0;
    const ZirExpr *e = &fn->exprs[index];
    if(e->kind == ZIR_EXPR_UNARY && (!strcmp(e->op, "*") || !strcmp(e->op, "&")))
        return 1;
    if(e->kind == ZIR_EXPR_CALL && call_writes_globals(scope, module, fn, index))
        return 1;
    if(tree_writes_globals(scope, module, fn, e->left) ||
       tree_writes_globals(scope, module, fn, e->right) ||
       tree_writes_globals(scope, module, fn, e->third))
        return 1;
    for(int child = e->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        if(tree_writes_globals(scope, module, fn, child))
            return 1;
    return 0;
}

/* Whether calling FN may write a global. A recursive query answers yes. */
static int
function_writes_globals(TextScope *scope, const ZirModule *module,
                        const ZirFunction *fn)
{
    ScopeAnswer *answer = answer_for(scope, fn);
    if(answer == NULL)
        return 1;
    if(answer->writes != SCOPE_UNKNOWN)
        return answer->writes != SCOPE_READ_ONLY;
    answer->writes = SCOPE_COMPUTING;
    int result = 0;
    for(int s = 0; !result && s < fn->stmt_count; s++) {
        const ZirStmt *st = &fn->stmts[s];
        if(st->kind == ZIR_STMT_ASSIGN && place_reaches_global(fn, st->lhs_root))
            result = 1;
    }
    for(int x = 0; !result && x < fn->expr_count; x++) {
        const ZirExpr *e = &fn->exprs[x];
        if((e->kind == ZIR_EXPR_UNARY && (!strcmp(e->op, "*") || !strcmp(e->op, "&"))) ||
           (e->kind == ZIR_EXPR_CALL && call_writes_globals(scope, module, fn, x)))
            result = 1;
    }
    answer = answer_for(scope, fn);
    if(answer != NULL)
        answer->writes = result ? SCOPE_RETAINED : SCOPE_READ_ONLY;
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

/* Whether some `*` or `&` in FN reaches local NAME or a place inside it,
 * through which a call could change it. */
static int
address_taken(const ZirFunction *fn, const char *name)
{
    for(int i = 0; i < fn->expr_count; i++) {
        const ZirExpr *e = &fn->exprs[i];
        if(e->kind != ZIR_EXPR_UNARY || (strcmp(e->op, "*") && strcmp(e->op, "&")))
            continue;
        int place = e->left >= 0 ? e->left : e->first_child;
        while(place >= 0 && (fn->exprs[place].kind == ZIR_EXPR_MEMBER ||
                             fn->exprs[place].kind == ZIR_EXPR_INDEX))
            place = fn->exprs[place].left;
        if(place < 0 || (fn->exprs[place].kind == ZIR_EXPR_IDENT &&
                         fn->exprs[place].name && !strcmp(fn->exprs[place].name, name)))
            return 1;
    }
    return 0;
}

/* Whether FN takes the address of a global anywhere, so a pointer may
 * reach it. */
static int
global_address_taken(const ZirFunction *fn)
{
    for(int i = 0; i < fn->expr_count; i++) {
        const ZirExpr *e = &fn->exprs[i];
        if(e->kind != ZIR_EXPR_UNARY || (strcmp(e->op, "*") && strcmp(e->op, "&")))
            continue;
        int place = e->left >= 0 ? e->left : e->first_child;
        while(place >= 0 && (fn->exprs[place].kind == ZIR_EXPR_MEMBER ||
                             fn->exprs[place].kind == ZIR_EXPR_INDEX))
            place = fn->exprs[place].left;
        if(place < 0 || fn->exprs[place].kind != ZIR_EXPR_IDENT ||
           fn->exprs[place].is_global_value)
            return 1;
    }
    return 0;
}

/* Nothing in INDEX's tree can observe a call moved before it: literals,
 * local reads, views of local Text no pointer reaches, and global reads
 * when GLOBALS says the moved call writes none. */
static int
settled(const ZirFunction *fn, int index, int globals)
{
    if(index < 0)
        return 1;
    const ZirExpr *e = &fn->exprs[index];
    switch(e->kind) {
    case ZIR_EXPR_INT: case ZIR_EXPR_FLOAT: case ZIR_EXPR_STRING:
    case ZIR_EXPR_SIZE_OF:
        return 1;
    case ZIR_EXPR_IDENT:
        return !e->is_global_value || globals;
    case ZIR_EXPR_UNARY:
        return (!strcmp(e->op, "-") || !strcmp(e->op, "!")) &&
            settled(fn, e->left >= 0 ? e->left : e->first_child, globals);
    case ZIR_EXPR_CALL:
        return !strcmp(e->name, "TextView") && e->first_child >= 0 &&
            fn->exprs[e->first_child].kind == ZIR_EXPR_IDENT &&
            !fn->exprs[e->first_child].is_global_value &&
            fn->exprs[e->first_child].name != NULL &&
            (scoped_name(fn->exprs[e->first_child].name) ||
             !address_taken(fn, fn->exprs[e->first_child].name));
    case ZIR_EXPR_MEMBER: case ZIR_EXPR_INDEX: case ZIR_EXPR_SLICE:
    case ZIR_EXPR_BINARY: case ZIR_EXPR_CAST: case ZIR_EXPR_FIELD_INIT:
    case ZIR_EXPR_COMPOUND:
        if(e->kind == ZIR_EXPR_BINARY &&
           (!strcmp(e->op, "&&") || !strcmp(e->op, "||")))
            return 0;
        if(!settled(fn, e->left, globals) || !settled(fn, e->right, globals) ||
           !settled(fn, e->third, globals))
            return 0;
        for(int child = e->first_child; child >= 0; child = fn->exprs[child].next_sibling)
            if(!settled(fn, child, globals))
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
earlier_settled(const ZirFunction *fn, int parent, int child, int globals)
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
        if(!settled(fn, order[i], globals))
            return 0;
    }
    for(int sibling = e->first_child; sibling >= 0; sibling = fn->exprs[sibling].next_sibling) {
        if(sibling == child)
            return 1;
        if(!settled(fn, sibling, globals))
            return 0;
    }
    return 0;
}

/* The statement whose evaluation can start with CALL, or -1. GLOBALS says
 * CALL writes no global, so earlier global reads see the same values. */
static int
hoist_statement(const ZirFunction *fn, const Parents *parents, int call,
                int globals)
{
    int node = call;
    while(parents->of[node] >= 0) {
        if(!earlier_settled(fn, parents->of[node], node, globals))
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
        return !st->for_step && settled(fn, st->lhs_root, globals) ? s : -1;
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

/* The statement whose expression tree holds X, or -1. */
static int
statement_of(const Parents *parents, int x)
{
    while(parents->of[x] >= 0)
        x = parents->of[x];
    return parents->of[x] < -1 ? -2 - parents->of[x] : -1;
}

/* The BLOCK_CLOSE ending the block that holds statement S, counted as the
 * checker counts scopes, or the statement count at function level. */
static int
block_end(const ZirFunction *fn, int s)
{
    int depth = 0;
    for(int t = s + 1; t < fn->stmt_count; t++) {
        ZirStmtKind kind = fn->stmts[t].kind;
        if(kind == ZIR_STMT_BLOCK_CLOSE) {
            if(depth == 0)
                return t;
            depth--;
        } else if(kind == ZIR_STMT_BLOCK_OPEN || kind == ZIR_STMT_IF ||
                  kind == ZIR_STMT_WHILE || kind == ZIR_STMT_FOR) {
            depth++;
        }
    }
    return fn->stmt_count;
}

/* Whether the string local declared by statement S is only read, and only
 * after S inside its own block. Another binding of the name anywhere in
 * the function, or a defer that could run after the block, keeps it. */
static int
local_flow(TextScope *scope, const ZirModule *module,
           const ZirFunction *fn, const Parents *parents, int s)
{
    const char *name = fn->stmts[s].name;
    int end = block_end(fn, s);
    for(int t = 0; t < fn->stmt_count; t++) {
        const ZirStmt *st = &fn->stmts[t];
        if(t != s && st->kind == ZIR_STMT_DECL && st->name && !strcmp(st->name, name))
            return FLOW_RETAINED;
        if(t > s && t < end && st->kind == ZIR_STMT_DEFER)
            return FLOW_RETAINED;
    }
    int result = FLOW_READ;
    for(int x = 0; result && x < fn->expr_count; x++) {
        const ZirExpr *e = &fn->exprs[x];
        if(e->kind != ZIR_EXPR_IDENT || e->is_this || e->is_function_value ||
           e->name == NULL || strcmp(e->name, name) || parents->of[x] == -1)
            continue;
        int at = statement_of(parents, x);
        if(at <= s || at >= end)
            return FLOW_RETAINED;
        result = combine(result, value_flow(scope, module, fn, parents, x));
    }
    return result;
}

static int
local_read_only(TextScope *scope, const ZirModule *module,
                const ZirFunction *fn, const Parents *parents, int s)
{
    return local_flow(scope, module, fn, parents, s) == FLOW_READ;
}

/* Keeps the Text call CALL of string local statement S as that local's
 * Text and lends a view of it at each use. */
static int
keep_local_text(ZirFunction *fn, int s, int call)
{
    const char *name = fn->stmts[s].name;
    int count = fn->expr_count;
    for(int x = 0; x < count; x++) {
        if(fn->exprs[x].kind != ZIR_EXPR_IDENT || fn->exprs[x].is_this ||
           fn->exprs[x].name == NULL || strcmp(fn->exprs[x].name, name))
            continue;
        ZirExpr *local = FunctionAddExpr(fn, ZIR_EXPR_IDENT, fn->exprs[x].text,
                                         fn->exprs[x].span);
        if(local == NULL)
            return 0;
        *local = fn->exprs[x];
        local->type = fn->exprs[call].type;
        local->next_sibling = -1;
        local->argument_index = -1;
        local->argument_name = "";
        int view = (int)(local - fn->exprs);
        ZirExpr *lent = &fn->exprs[x];
        lent->kind = ZIR_EXPR_CALL;
        lent->name = KeepName("TextView");
        lent->slot_type = "";
        lent->left = lent->right = lent->third = -1;
        lent->first_child = view;
    }
    ZirExpr *kept = &fn->exprs[call];
    kept->next_sibling = -1;
    kept->argument_index = -1;
    kept->argument_name = "";
    kept->text_owned = 1;
    fn->stmts[s].type = kept->type;
    fn->stmts[s].expr_root = call;
    return 1;
}

/* A string local initialized from a fresh Text, as in
 * `line := Render(...)`, whose uses only read it becomes that Text. */
static int
infer_local_texts(TextScope *scope, const ZirModule *module, ZirFunction *fn,
                  int *changed)
{
    Parents parents = {0};
    for(int s = 0; s < fn->stmt_count; s++) {
        const ZirStmt *st = &fn->stmts[s];
        int root = st->expr_root;
        const char *vec_module = NULL;
        if(st->kind != ZIR_STMT_DECL || st->is_using || st->name == NULL ||
           !*st->name || st->type == NULL || strcmp(st->type, "string") ||
           root < 0 || st->lhs_root >= 0 ||
           (!text_detach_call(fn, root) &&
            (vec_module = builder_text_module(module, fn, root)) == NULL))
            continue;
        if(parents.of == NULL && !parents_build(fn, &parents))
            return 0;
        if(!local_read_only(scope, module, fn, &parents, s))
            continue;
        int call = vec_module ? builder_text_call(fn, root, vec_module) :
            fn->exprs[root].first_child;
        if(call < 0 || !keep_local_text(fn, s, call)) {
            free(parents.of);
            return 0;
        }
        free(parents.of);
        parents.of = NULL;
        *changed = 1;
    }
    free(parents.of);
    return 1;
}

static int
scope_function(TextScope *scope, const ZirModule *module, ZirFunction *fn)
{
    int serial = 0, inferred = 0;
    if(!infer_local_texts(scope, module, fn, &inferred))
        return 0;
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
            /* The moved call is the Text producer with its arguments. */
            int moved = vec_module ? x : fn->exprs[x].first_child;
            int globals = !global_address_taken(fn) &&
                !tree_writes_globals(scope, module, fn, moved);
            if(scope->failed) {
                free(parents.of);
                return 0;
            }
            int s = hoist_statement(fn, &parents, x, globals);
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
            return (serial == 0 && !inferred) || rebuild_conversion_layout(fn);
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
    free(scope.fields);
    if(!ok)
        fprintf(stderr, "ziran: out of memory scoping text arguments\n");
    return ok;
}
