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
 * it, a callee that hands it back when its result is only read, and field
 * paths: a record literal field, a record copied into a field of a record
 * local, and a callee whose by-value record parameter only reads the
 * string at that path. Returning, storing, aliasing or passing it to a
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
    /* With writes, the pointer parameters a call may write through. */
    uint64_t through;
} ScopeAnswer;

/* Where the string at field path FIELD, such as `data.value`, of record
 * parameter PARAMETER goes. */
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
    /* Set while following a parameter: what flows is a borrowed view, so
     * `=` into a plain string local only rebinds that local. */
    int borrowed;
    /* Locals being followed, so a local fed from itself, as in
     * `v = v[1:]`, is not followed again: a string local declared by S
     * with no PATH, or the string at PATH in a record local once S stored
     * it there. */
    struct { const ZirFunction *fn; int s; const char *path; } walking[16];
    int walking_count;
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
static int string_array_type(const char *type);
static int string_local(const ZirFunction *fn, int place);
static int local_flow(TextScope *scope, const ZirModule *module,
                      const ZirFunction *fn, const Parents *parents, int s);
static int record_flow(TextScope *scope, const ZirModule *module,
                       const ZirFunction *fn, const Parents *parents, int node,
                       const char *path);
static int local_record_flow(TextScope *scope, const ZirModule *module,
                             const ZirFunction *fn, const Parents *parents,
                             const char *name, int decl, int from,
                             const char *path);

static int pointer_type(const char *type);
static int scalar_type(const char *type);

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

/* The `#reads` procedure type a call goes through, or NULL. */
static const ZirType *
reads_slot(const ZirModule *module, const char *type)
{
    const ZirType *slot = type != NULL && *type ? FindType(module, type, NULL) : NULL;
    return slot != NULL && slot->is_procedure_type && slot->reads_arguments ? slot : NULL;
}

/* Where argument ARGUMENT of call CALL goes, or with FIELD, the string at
 * that field path of a record argument. A callee that hands the string back passes it on to
 * wherever the call's result goes. */
static int
argument_flow(TextScope *scope, const ZirModule *module, const ZirFunction *fn,
              const Parents *parents, int call, int argument, const char *field)
{
    const ZirExpr *e = &fn->exprs[call];
    /* Every procedure stored in a `#reads` type was checked to only read
     * its strings, so a call through one only reads its arguments. */
    if(reads_slot(module, e->slot_type) != NULL)
        return argument != e->left ? FLOW_READ : FLOW_RETAINED;
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
        /* A view stored into a plain string local, as in `v = s[1:3]`,
         * goes where that local's values go. */
        if(scope->borrowed && st->kind == ZIR_STMT_ASSIGN &&
           !strcmp(st->assignment_op, "=")) {
            int local = string_local(fn, st->lhs_root);
            if(local < 0)
                return FLOW_RETAINED;
            for(int i = 0; i < scope->walking_count; i++)
                if(scope->walking[i].fn == fn && scope->walking[i].s == local &&
                   scope->walking[i].path == NULL)
                    return FLOW_READ;
            return local_flow(scope, module, fn, parents, local);
        }
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
        int record = parents->of[parent];
        if(e->right != value || record < 0 || fn->exprs[record].kind != ZIR_EXPR_COMPOUND)
            return FLOW_RETAINED;
        /* An element of a local string array's literal, as in
         * `parts: [9]string = .[a, b]`, goes where the array's elements go. */
        int up = parents->of[record];
        if(up < -1 && (!e->name || !*e->name)) {
            int s = -2 - up;
            const ZirStmt *st = &fn->stmts[s];
            if(st->kind != ZIR_STMT_DECL || st->expr_root != record || st->is_using ||
               st->lhs_root >= 0 || st->name == NULL || !*st->name ||
               !string_array_type(st->type))
                return FLOW_RETAINED;
            return local_flow(scope, module, fn, parents, s);
        }
        /* A field of a record literal goes where that field of the
         * record goes. */
        if(!e->name || !*e->name)
            return FLOW_RETAINED;
        return record_flow(scope, module, fn, parents, record, e->name);
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
 * FIELD, the string at that field path, such as `data.value`, of a record
 * parameter passed by value. A recursive query answers retained. */
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
    int borrowed = scope->borrowed;
    scope->borrowed = 1;
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
        /* A record parameter is a local copy holding the field from entry. */
        if(result && field != NULL)
            result = local_record_flow(scope, module, fn, &parents, name, -1, -1, field);
        for(int x = 0; result && field == NULL && x < fn->expr_count; x++) {
            const ZirExpr *e = &fn->exprs[x];
            if(e->kind != ZIR_EXPR_IDENT || e->is_this || !e->name ||
               strcmp(e->name, name) || parents.of[x] == -1)
                continue;
            result = combine(result, value_flow(scope, module, fn, &parents, x));
        }
        free(parents.of);
    }
    scope->borrowed = borrowed;
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

/* What a write may change: only storage of the procedure's own locals,
 * memory that pointer parameter P points to (P itself, 0 or more), or
 * anything, a global included. */
enum { REACH_LOCAL = -2, REACH_ANY = -1 };

static int place_reach(const ZirFunction *fn, int index);

/* The operand of unary expression E: `&x` takes x's address and `*p`,
 * written `<<p`, is the place p points to. */
static int
unary_operand(const ZirExpr *e)
{
    return e->right >= 0 ? e->right : e->left >= 0 ? e->left : e->first_child;
}

static int
address_of(const ZirExpr *e)
{
    return e->kind == ZIR_EXPR_UNARY && !strcmp(e->op, "&");
}

/* The index of FN's pointer parameter NAME, which FN never rebinds, so
 * it still holds the caller's argument, or -1. */
static int
pointer_parameter(const ZirFunction *fn, const char *name)
{
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    int found = -1;
    for(int p = 0; p < parameters->count && p < 64; p++) {
        const ZirParameter *item = &parameters->items[p];
        const char *bound = item->binding_name ? item->binding_name : item->name;
        const char *type = item->value_type ? item->value_type : item->type;
        if(bound != NULL && !strcmp(bound, name))
            found = !item->is_using && pointer_type(type) ? p : -1;
    }
    for(int s = 0; found >= 0 && s < fn->stmt_count; s++) {
        const ZirStmt *st = &fn->stmts[s];
        int lhs = st->lhs_root;
        if((st->kind == ZIR_STMT_DECL && st->name && !strcmp(st->name, name)) ||
           (st->kind == ZIR_STMT_ASSIGN && lhs >= 0 &&
            fn->exprs[lhs].kind == ZIR_EXPR_IDENT && fn->exprs[lhs].name &&
            !strcmp(fn->exprs[lhs].name, name)))
            found = -1;
    }
    return found;
}

/* What the pointer value at INDEX may point into: the address of a place,
 * or a pointer parameter. */
static int
pointer_reach(const ZirFunction *fn, int index)
{
    const ZirExpr *e = &fn->exprs[index];
    if(address_of(e))
        return place_reach(fn, unary_operand(e));
    if(e->kind == ZIR_EXPR_IDENT && !e->is_global_value && !e->is_this && e->name) {
        int p = pointer_parameter(fn, e->name);
        if(p >= 0)
            return p;
    }
    return REACH_ANY;
}

/* What writing the place at INDEX may change: a local, a field or element
 * of one, or what a pointer followed on the way to it points to. */
static int
place_reach(const ZirFunction *fn, int index)
{
    while(index >= 0) {
        const ZirExpr *e = &fn->exprs[index];
        if(e->kind == ZIR_EXPR_IDENT)
            return e->is_global_value || e->is_this || !e->name ? REACH_ANY : REACH_LOCAL;
        if(e->kind == ZIR_EXPR_UNARY && !strcmp(e->op, "*") && unary_operand(e) >= 0)
            return pointer_reach(fn, unary_operand(e));
        if((e->kind != ZIR_EXPR_MEMBER && e->kind != ZIR_EXPR_INDEX &&
            e->kind != ZIR_EXPR_SLICE) || e->left < 0)
            return REACH_ANY;
        if(pointer_type(fn->exprs[e->left].type))
            return pointer_reach(fn, e->left);
        index = e->left;
    }
    return REACH_ANY;
}

/* What a write that may change REACH adds to a procedure's effects. */
static void
add_reach(int reach, int *global, uint64_t *through)
{
    if(reach == REACH_ANY)
        *global = 1;
    else if(reach >= 0)
        *through |= (uint64_t)1 << reach;
}

static int function_writes(TextScope *scope, const ZirModule *module,
                           const ZirFunction *fn, uint64_t *through);

/* What evaluating the call at INDEX may write. Sets GLOBAL when it may
 * write a global or memory of unknown origin, through a foreign, unknown
 * or writing procedure or a place reaching a global handed to it. Each
 * pointer argument it may write through adds what that pointer reaches:
 * FN's own locals set LOCAL, FN's pointer parameters go into THROUGH. */
static void
call_writes(TextScope *scope, const ZirModule *module, const ZirFunction *fn,
            int index, int *global, uint64_t *through, int *local)
{
    const ZirExpr *e = &fn->exprs[index];
    if(e->left >= 0 || e->is_function_value || !e->name || !*e->name) {
        *global = 1;
        return;
    }
    for(int arg = e->first_child; arg >= 0; arg = fn->exprs[arg].next_sibling) {
        const char *type = fn->exprs[arg].type ? skip_ws(fn->exprs[arg].type) : "";
        int copied = !strcmp(type, "string") || !strcmp(type, "bool") ||
            ((type[0] == 's' || type[0] == 'u' || type[0] == 'f') &&
             isdigit((unsigned char)type[1]));
        /* A Vec or record handed over by place, as in `BuilderAppend(<<b, s)`,
         * may change where that place is. */
        int reach = copied || pointer_type(type) ? REACH_LOCAL : place_reach(fn, arg);
        if(reach != REACH_LOCAL)
            add_reach(reach, global, through);
    }
    /* Which pointer parameters the callee writes through; a builtin such as
     * print, TextView or BuilderAppend acts only on its arguments. */
    uint64_t written = ~(uint64_t)0;
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    int found = ResolveFunction(module, e->name, &owner, &callee);
    if(found != 1) {
        if(found != 0) {
            *global = 1;
            return;
        }
        for(int i = 0; i < module->import_count; i++)
            if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
               !strcmp(module->imports[i].name, e->name)) {
                *global = 1;
                return;
            }
    } else if(callee == NULL || owner == NULL || callee->is_extern ||
              callee->is_template || callee->stmt_count == 0 ||
              function_writes(scope, owner, callee, &written)) {
        *global = 1;
        return;
    }
    for(int arg = e->first_child; arg >= 0; arg = fn->exprs[arg].next_sibling) {
        int position = fn->exprs[arg].argument_index;
        if(!pointer_type(fn->exprs[arg].type) ||
           (found == 1 && position >= 0 && position < 64 &&
            !(written & ((uint64_t)1 << position))))
            continue;
        int reach = pointer_reach(fn, arg);
        if(reach == REACH_LOCAL)
            *local = 1;
        else
            add_reach(reach, global, through);
    }
}

/* Whether the expression at INDEX is an argument of a call. */
static int
call_argument(const ZirFunction *fn, const unsigned char *arguments, int index)
{
    return arguments != NULL && index >= 0 && index < fn->expr_count && arguments[index];
}

/* Marks each expression of FN that is an argument of a call. */
static unsigned char *
call_arguments(const ZirFunction *fn)
{
    unsigned char *arguments = calloc((size_t)fn->expr_count + 1, 1);
    if(arguments == NULL)
        return NULL;
    for(int x = 0; x < fn->expr_count; x++)
        if(fn->exprs[x].kind == ZIR_EXPR_CALL)
            for(int child = fn->exprs[x].first_child; child >= 0 && child < fn->expr_count;
                child = fn->exprs[child].next_sibling)
                arguments[child] = 1;
    return arguments;
}

/* What evaluating the tree at INDEX may write: answers 1 when it may write
 * a global, and sets ARGUMENT when a call in it may write through a pointer
 * argument, which could change what the rest of the statement reads. An
 * address taken anywhere but as a call's argument may be kept and written
 * later, so it counts as a global write. */
static int
tree_writes(TextScope *scope, const ZirModule *module, const ZirFunction *fn,
            int index, int is_argument, int *argument)
{
    if(index < 0)
        return 0;
    const ZirExpr *e = &fn->exprs[index];
    int global = 0;
    if(address_of(e) && !is_argument)
        global = 1;
    if(e->kind == ZIR_EXPR_CALL) {
        uint64_t through = 0;
        int local = 0;
        call_writes(scope, module, fn, index, &global, &through, &local);
        if(through || local)
            *argument = 1;
    }
    global |= tree_writes(scope, module, fn, e->left, 0, argument) |
        tree_writes(scope, module, fn, e->right, 0, argument) |
        tree_writes(scope, module, fn, e->third, 0, argument);
    for(int child = e->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        global |= tree_writes(scope, module, fn, child, e->kind == ZIR_EXPR_CALL, argument);
    return global;
}

/* Whether the tree at INDEX may write a global, or through a pointer
 * argument, when evaluated. */
static int
tree_writes_globals(TextScope *scope, const ZirModule *module,
                    const ZirFunction *fn, int index)
{
    int argument = 0;
    return tree_writes(scope, module, fn, index, 0, &argument) || argument;
}

/* Whether a call in the tree at INDEX may write through a pointer argument. */
static int
tree_writes_arguments(TextScope *scope, const ZirModule *module,
                      const ZirFunction *fn, int index)
{
    int argument = 0;
    (void)tree_writes(scope, module, fn, index, 0, &argument);
    return argument;
}

/* What calling FN may write that its caller can see: answers 1 when it may
 * write a global or memory of unknown origin, and sets THROUGH to the
 * pointer parameters it may write through. Writes to its own locals,
 * directly or through their addresses handed to calls, are not seen. A
 * recursive query answers 1. */
static int
function_writes(TextScope *scope, const ZirModule *module,
                const ZirFunction *fn, uint64_t *through)
{
    *through = 0;
    ScopeAnswer *answer = answer_for(scope, fn);
    if(answer == NULL)
        return 1;
    if(answer->writes != SCOPE_UNKNOWN) {
        *through = answer->through;
        return answer->writes != SCOPE_READ_ONLY;
    }
    answer->writes = SCOPE_COMPUTING;
    int global = 0, local = 0;
    uint64_t bits = 0;
    unsigned char *arguments = call_arguments(fn);
    if(arguments == NULL)
        global = 1;
    for(int s = 0; !global && s < fn->stmt_count; s++) {
        const ZirStmt *st = &fn->stmts[s];
        if(st->kind == ZIR_STMT_ASSIGN)
            add_reach(place_reach(fn, st->lhs_root), &global, &bits);
    }
    for(int x = 0; !global && x < fn->expr_count; x++) {
        const ZirExpr *e = &fn->exprs[x];
        if(address_of(e) && !call_argument(fn, arguments, x))
            global = 1;
        else if(e->kind == ZIR_EXPR_CALL)
            call_writes(scope, module, fn, x, &global, &bits, &local);
    }
    free(arguments);
    answer = answer_for(scope, fn);
    if(answer != NULL) {
        answer->writes = global ? SCOPE_RETAINED : SCOPE_READ_ONLY;
        answer->through = bits;
    }
    *through = bits;
    return global;
}

static int
text_detach_call(const ZirFunction *fn, int index)
{
    const ZirExpr *e = &fn->exprs[index];
    if(e->kind != ZIR_EXPR_CALL || e->name == NULL || e->first_child < 0 ||
       fn->exprs[e->first_child].next_sibling >= 0 ||
       fn->exprs[e->first_child].kind != ZIR_EXPR_CALL)
        return 0;
    const char *dot = strrchr(e->name, '.');
    if(dot == NULL || strcmp(dot, ".TextDetach"))
        return 0;
    char owner[ZIR_NAME_MAX];
    snprintf(owner, sizeof(owner), "%.*s", (int)(dot - e->name), e->name);
    return StandardModuleName(owner, "vec");
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
        int place = unary_operand(e);
        while(place >= 0 && (fn->exprs[place].kind == ZIR_EXPR_MEMBER ||
                             fn->exprs[place].kind == ZIR_EXPR_INDEX))
            place = fn->exprs[place].left;
        if(place < 0 || (fn->exprs[place].kind == ZIR_EXPR_IDENT &&
                         fn->exprs[place].name && !strcmp(fn->exprs[place].name, name)))
            return 1;
    }
    return 0;
}

/* Whether the expression at INDEX is an argument of a call that gives
 * back a plain value: nothing, a scalar, a string or a Text, none of which
 * can carry the address on. */
static int
handed_to_call(const ZirFunction *fn, int index)
{
    for(int x = 0; x < fn->expr_count; x++) {
        const ZirExpr *e = &fn->exprs[x];
        if(e->kind != ZIR_EXPR_CALL)
            continue;
        for(int child = e->first_child; child >= 0; child = fn->exprs[child].next_sibling) {
            if(child != index)
                continue;
            const char *type = e->type ? skip_ws(e->type) : "";
            const char *dot = strrchr(type, '.');
            return !strcmp(type, "void") || !strcmp(type, "string") ||
                !strcmp(dot ? dot + 1 : type, "Text") || scalar_type(type);
        }
    }
    return 0;
}

/* Whether FN takes the address of a global anywhere a pointer in FN may
 * keep it. An address handed straight to a call that gives back no pointer
 * leaves none in FN; the call's own writes through it are counted where
 * the call is. */
static int
global_address_taken(const ZirFunction *fn)
{
    for(int i = 0; i < fn->expr_count; i++) {
        const ZirExpr *e = &fn->exprs[i];
        if(e->kind != ZIR_EXPR_UNARY || (strcmp(e->op, "*") && strcmp(e->op, "&")))
            continue;
        if(handed_to_call(fn, i))
            continue;
        int place = unary_operand(e);
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
    case ZIR_EXPR_UNARY: {
        int operand = unary_operand(e);
        /* A variable's address, as in `Append(*out, ...)`, cannot change. */
        if(!strcmp(e->op, "&") && operand >= 0 &&
           fn->exprs[operand].kind == ZIR_EXPR_IDENT &&
           fn->exprs[operand].name != NULL && !fn->exprs[operand].is_function_value)
            return 1;
        /* What a pointer points to, as in `<<p`, is a global as far as the
         * moved call is concerned: it may be one. */
        if(!strcmp(e->op, "*"))
            return globals && settled(fn, operand, globals);
        return (!strcmp(e->op, "-") || !strcmp(e->op, "!")) &&
            settled(fn, operand, globals);
    }
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

/* Whether CHILD of PARENT runs after only settled siblings, so the
 * statement may evaluate CHILD's call first. A condition and the first
 * operand of `&&` or `||` always run; their other arms may not. */
static int
earlier_settled(const ZirFunction *fn, int parent, int child, int globals)
{
    const ZirExpr *e = &fn->exprs[parent];
    if(e->kind == ZIR_EXPR_CONDITIONAL ||
       (e->kind == ZIR_EXPR_BINARY &&
        (!strcmp(e->op, "&&") || !strcmp(e->op, "||"))))
        return child == e->left;
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
 * CALL writes no global, so earlier global reads see the same values. When
 * CALL could start an else-if condition, *ELSE_IF names that statement. */
static int
hoist_statement(const ZirFunction *fn, const Parents *parents, int call,
                int globals, int *else_if)
{
    int node = call;
    *else_if = -1;
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
        if(st->is_else)
            *else_if = s;
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

/* Opens COUNT empty statements at S, each with S's old span and no
 * expressions, or answers NULL. */
static ZirStmt *
insert_statements(ZirFunction *fn, int s, int count)
{
    ZirSourceSpan span = s < fn->stmt_count ? fn->stmts[s].span :
        fn->stmt_count > 0 ? fn->stmts[fn->stmt_count - 1].span : fn->span;
    if(fn->stmt_count + count > fn->stmt_cap) {
        int capacity = fn->stmt_cap > 0 ? fn->stmt_cap : 16;
        while(capacity < fn->stmt_count + count)
            capacity *= 2;
        ZirStmt *stmts = realloc(fn->stmts, (size_t)capacity * sizeof(ZirStmt));
        if(stmts == NULL)
            return NULL;
        fn->stmts = stmts;
        fn->stmt_cap = capacity;
    }
    memmove(&fn->stmts[s + count], &fn->stmts[s],
            (size_t)(fn->stmt_count - s) * sizeof(ZirStmt));
    fn->stmt_count += count;
    for(int i = 0; i < count; i++) {
        ZirStmt *st = &fn->stmts[s + i];
        StmtReset(st);
        st->expr_root = st->lhs_root = -1;
        st->span = span;
    }
    return &fn->stmts[s];
}

/* A fresh local name built from PREFIX. */
static void
fresh_name(const ZirFunction *fn, const char *prefix, int *serial,
           char *name, size_t size)
{
    do snprintf(name, size, "%s_%d", prefix, ++*serial);
    while(name_used(fn, name));
}

/* Declares a scoped local holding Text call CALL before statement S and
 * turns VIEW, the node that used CALL's string, into a view of that local. */
static int
scope_text(ZirFunction *fn, int view_node, int call, int s, int *serial)
{
    char name[ZIR_NAME_MAX];
    fresh_name(fn, "text_scope", serial, name, sizeof(name));
    ZirExpr *local = FunctionAddExpr(fn, ZIR_EXPR_IDENT, name, fn->exprs[view_node].span);
    if(local == NULL)
        return 0;
    local->name = KeepName(name);
    local->type = fn->exprs[call].type;
    int view = (int)(local - fn->exprs);
    ZirStmt *decl = insert_statements(fn, s, 1);
    if(decl == NULL)
        return 0;
    decl->kind = ZIR_STMT_DECL;
    decl->text = fn->exprs[call].text;
    decl->name = KeepName(name);
    decl->type = fn->exprs[call].type;
    decl->expr_root = call;
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
    char scoped[ZIR_NAME_MAX];
    const char *modules[] = {"vec", "std_vec", scoped};
    if(!StandardScopedName("vec", scoped, sizeof(scoped)))
        scoped[0] = '\0';
    const ZirExpr *e = &fn->exprs[index];
    if(module->standard_name[0] && !strcmp(module->standard_name, "vec"))
        return NULL;
    if(e->kind != ZIR_EXPR_CALL || e->left >= 0 || e->name == NULL ||
       strcmp(e->name, "BuilderFinish") || e->type == NULL ||
       strcmp(e->type, "string") || e->first_child < 0 ||
       fn->exprs[e->first_child].next_sibling >= 0)
        return NULL;
    for(size_t i = 0; i < sizeof(modules) / sizeof(*modules); i++) {
        char qualified[ZIR_NAME_MAX];
        const ZirModule *owner = NULL;
        const ZirFunction *found = NULL;
        if(!*modules[i])
            continue;
        if(!strcmp(module->name, modules[i]))
            return NULL;
        snprintf(qualified, sizeof(qualified), "%s.TextFromBuilder", modules[i]);
        if(ResolveFunction(module, qualified, &owner, &found) == 1)
            return KeepName(modules[i]);
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

/* The declaration that the read X of a local names: the closest earlier
 * declaration of that name whose block is still open at X's statement, or
 * -1 for a parameter or a global. Same-named locals in sibling blocks are
 * different bindings. */
static int
declaration_of(const ZirFunction *fn, const Parents *parents, int x)
{
    int at = statement_of(parents, x);
    const char *name = fn->exprs[x].name;
    for(int t = at - 1; t >= 0 && name != NULL; t--) {
        const ZirStmt *st = &fn->stmts[t];
        if(st->kind == ZIR_STMT_DECL && st->name != NULL &&
           !strcmp(st->name, name) && at < block_end(fn, t))
            return t;
    }
    return -1;
}

/* Whether a local of TYPE can carry an ifx result out of its branches:
 * a value with no string to outlive the arm's Text. */
static int
scalar_type(const char *type)
{
    static const char *const names[] = {
        "bool", "s8", "s16", "s32", "s64", "u8", "u16", "u32", "u64",
        "float32", "float64", "int", "float",
    };
    if(type == NULL)
        return 0;
    for(size_t i = 0; i < sizeof(names) / sizeof(*names); i++)
        if(!strcmp(type, names[i]))
            return 1;
    return 0;
}

/* The `&&` or `||` whose second operand holds NODE, or the scalar ifx
 * whose arm does, when it is what keeps NODE from starting its statement,
 * or -1. Every step below it must settle under GLOBALS. */
static int
blocking_arm(const ZirFunction *fn, const Parents *parents, int node, int globals)
{
    while(parents->of[node] >= 0) {
        int parent = parents->of[node];
        const ZirExpr *e = &fn->exprs[parent];
        if(!earlier_settled(fn, parent, node, globals)) {
            if(node == e->left)
                return -1;
            if(e->kind == ZIR_EXPR_BINARY &&
               (!strcmp(e->op, "&&") || !strcmp(e->op, "||")))
                return parent;
            return e->kind == ZIR_EXPR_CONDITIONAL && scalar_type(e->type) ?
                parent : -1;
        }
        node = parent;
    }
    return -1;
}

/* A new read of local NAME of TYPE, or -1. */
static int
local_read(ZirFunction *fn, const char *name, const char *type, ZirSourceSpan span)
{
    ZirExpr *read = FunctionAddExpr(fn, ZIR_EXPR_IDENT, name, span);
    if(read == NULL)
        return -1;
    read->name = name;
    read->type = type;
    return (int)(read - fn->exprs);
}

/* Runs ARM, an `&&`, `||` or ifx inside statement S, as statements before
 * S that leave its value in a fresh local, and reads that local instead:
 *     cond := L; if cond { cond = R }       for L && R
 *     cond := L; if !cond { cond = R }      for L || R
 *     arm: T; if C { arm = A } else { arm = B }   for ifx C A else B
 * Each arm becomes a statement root, so its Text calls scope to its block. */
static int
rewrite_arm(ZirFunction *fn, int arm, int s, int *serial)
{
    char local[ZIR_NAME_MAX];
    const ZirExpr e = fn->exprs[arm];
    int conditional = e.kind == ZIR_EXPR_CONDITIONAL;
    fresh_name(fn, conditional ? "text_arm" : "text_cond", serial, local, sizeof(local));
    const char *name = KeepName(local);
    int test = conditional ? e.left : local_read(fn, name, e.type, e.span);
    if(test >= 0 && !conditional && !strcmp(e.op, "||")) {
        ZirExpr *negated = FunctionAddExpr(fn, ZIR_EXPR_UNARY, e.text, e.span);
        if(negated == NULL)
            return 0;
        snprintf(negated->op, sizeof(negated->op), "!");
        negated->right = test;
        negated->type = e.type;
        test = (int)(negated - fn->exprs);
    }
    int first = local_read(fn, name, e.type, e.span);
    int second = conditional ? local_read(fn, name, e.type, e.span) : 0;
    ZirStmt *st = test < 0 || first < 0 || second < 0 ? NULL :
        insert_statements(fn, s, conditional ? 7 : 4);
    if(st == NULL)
        return 0;
    const char *text = e.text ? e.text : KeepText(name);
    st[0].kind = ZIR_STMT_DECL;
    st[0].name = name;
    st[0].type = e.type;
    st[0].text = text;
    st[0].expr_root = conditional ? -1 : e.left;
    st[1].kind = ZIR_STMT_IF;
    st[1].text = text;
    st[1].expr_root = test;
    st[2].kind = ZIR_STMT_ASSIGN;
    st[2].text = text;
    st[2].lhs_root = first;
    st[2].expr_root = e.right;
    snprintf(st[2].assignment_op, sizeof(st[2].assignment_op), "=");
    st[3].kind = ZIR_STMT_BLOCK_CLOSE;
    st[3].text = KeepText("}");
    if(conditional) {
        st[4].kind = ZIR_STMT_IF;
        st[4].is_else = 1;
        st[4].text = KeepText("else {");
        st[5].kind = ZIR_STMT_ASSIGN;
        st[5].text = text;
        st[5].lhs_root = second;
        st[5].expr_root = e.third;
        snprintf(st[5].assignment_op, sizeof(st[5].assignment_op), "=");
        st[6].kind = ZIR_STMT_BLOCK_CLOSE;
        st[6].text = KeepText("}");
    }
    ZirExpr *read = &fn->exprs[arm];
    read->kind = ZIR_EXPR_IDENT;
    read->name = name;
    read->text = KeepText(name);
    read->op[0] = '\0';
    read->left = read->right = read->third = read->first_child = -1;
    read->is_global_value = read->is_move = read->text_owned = 0;
    return 1;
}

/* Turns else-if statement S into an else block holding a plain if, so
 * statements can run before its condition:
 *     } else if C { ... } else { ... }
 * becomes
 *     } else { if C { ... } else { ... } }                              */
static int
split_else_if(ZirFunction *fn, int s)
{
    int end = block_end(fn, s);
    while(end + 1 < fn->stmt_count && fn->stmts[end + 1].kind == ZIR_STMT_IF &&
          fn->stmts[end + 1].is_else)
        end = block_end(fn, end + 1);
    if(end >= fn->stmt_count)
        return 0;
    ZirStmt *close = insert_statements(fn, end + 1, 1);
    if(close == NULL)
        return 0;
    close->kind = ZIR_STMT_BLOCK_CLOSE;
    close->text = KeepText("}");
    close->span = fn->stmts[end].span;
    ZirStmt *inner = insert_statements(fn, s + 1, 1);
    if(inner == NULL)
        return 0;
    ZirStmt *outer = &fn->stmts[s];
    *inner = *outer;
    inner->is_else = 0;
    const char *text = inner->text ? inner->text : "";
    if(!strncmp(text, "else", 4)) {
        text += 4;
        while(isspace((unsigned char)*text))
            text++;
        inner->text = KeepText(text);
    }
    outer->expr_root = -1;
    outer->text = KeepText("else {");
    return 1;
}

/* When CALL's Text could be scoped but for an else-if condition or a
 * conditional arm above it, rewrites the closest such place that can move
 * before its statement. Answers 1 after a rewrite, 0 when none applies and
 * -1 when it fails. ELSE_IF and GLOBALS come from CALL's hoist_statement. */
static int
rewrite_condition(TextScope *scope, const ZirModule *module, ZirFunction *fn,
                  const Parents *parents, int call, int globals, int else_if,
                  int *serial)
{
    for(int node = call;;) {
        if(else_if >= 0)
            return split_else_if(fn, else_if) ? 1 : -1;
        node = blocking_arm(fn, parents, node, globals);
        if(node < 0 || tree_writes_arguments(scope, module, fn, node))
            return 0;
        globals = !global_address_taken(fn) &&
            !tree_writes_globals(scope, module, fn, node);
        if(scope->failed)
            return -1;
        int s = hoist_statement(fn, parents, node, globals, &else_if);
        if(s >= 0)
            return rewrite_arm(fn, node, s, serial) ? 1 : -1;
    }
}

/* Whether TYPE is a fixed array of strings, such as `[9]string`. */
static int
string_array_type(const char *type)
{
    if(type == NULL || *type++ != '[' || !isdigit((unsigned char)*type))
        return 0;
    while(isdigit((unsigned char)*type))
        type++;
    return !strcmp(type, "]string");
}

/* Whether the string local declared by statement S is only read, and only
 * after S inside its own block. For a local string array, its elements are
 * followed wherever an index reads one, and any other use keeps them. While
 * a parameter is followed, `=` into the local only rebinds it, and the view
 * assigned goes where the local's values go. Another binding of the name
 * anywhere in the function, or a defer that could run after the block,
 * keeps it. */
static int
local_flow(TextScope *scope, const ZirModule *module,
           const ZirFunction *fn, const Parents *parents, int s)
{
    const char *name = fn->stmts[s].name;
    int array = string_array_type(fn->stmts[s].type);
    int end = block_end(fn, s);
    for(int t = s + 1; t < end; t++)
        if(fn->stmts[t].kind == ZIR_STMT_DEFER)
            return FLOW_RETAINED;
    if(scope->walking_count == (int)(sizeof(scope->walking) / sizeof(scope->walking[0])))
        return FLOW_RETAINED;
    int walking = scope->walking_count++;
    scope->walking[walking].fn = fn;
    scope->walking[walking].s = s;
    scope->walking[walking].path = NULL;
    int result = FLOW_READ;
    for(int x = 0; result && x < fn->expr_count; x++) {
        const ZirExpr *e = &fn->exprs[x];
        if(e->kind != ZIR_EXPR_IDENT || e->is_this || e->is_function_value ||
           e->name == NULL || strcmp(e->name, name) || parents->of[x] == -1 ||
           declaration_of(fn, parents, x) != s)
            continue;
        int at = statement_of(parents, x);
        if(at <= s || at >= end) {
            result = FLOW_RETAINED;
            break;
        }
        /* Rebinding a local that holds a borrowed view writes no string. */
        if(scope->borrowed && !array && parents->of[x] == -2 - at &&
           fn->stmts[at].kind == ZIR_STMT_ASSIGN && fn->stmts[at].lhs_root == x &&
           !strcmp(fn->stmts[at].assignment_op, "="))
            continue;
        if(array) {
            int up = parents->of[x];
            if(up >= 0 && fn->exprs[up].kind == ZIR_EXPR_INDEX && fn->exprs[up].left == x)
                result = combine(result, value_flow(scope, module, fn, parents, up));
            else if(up >= 0 && fn->exprs[up].kind == ZIR_EXPR_MEMBER &&
                    fn->exprs[up].left == x && !strcmp(fn->exprs[up].name, "count"))
                continue;
            else
                result = FLOW_RETAINED;
            continue;
        }
        result = combine(result, value_flow(scope, module, fn, parents, x));
    }
    scope->walking_count = walking;
    return result;
}

/* The declaration of the plain string local that PLACE names, or -1. */
static int
string_local(const ZirFunction *fn, int place)
{
    if(place < 0 || fn->exprs[place].kind != ZIR_EXPR_IDENT ||
       fn->exprs[place].is_global_value || fn->exprs[place].is_this ||
       fn->exprs[place].name == NULL)
        return -1;
    int found = -1;
    for(int s = 0; s < fn->stmt_count; s++) {
        const ZirStmt *st = &fn->stmts[s];
        if(st->kind != ZIR_STMT_DECL || st->name == NULL ||
           strcmp(st->name, fn->exprs[place].name))
            continue;
        if(found >= 0 || st->is_using || st->lhs_root >= 0 || st->type == NULL ||
           strcmp(st->type, "string"))
            return -1;
        found = s;
    }
    return found;
}

/* The statement opening the innermost block that holds statement S, or -1
 * at function level, counted as block_end counts them. */
static int
enclosing_block(const ZirFunction *fn, int s)
{
    int depth = 0;
    for(int t = s - 1; t >= 0; t--) {
        ZirStmtKind kind = fn->stmts[t].kind;
        if(kind == ZIR_STMT_BLOCK_CLOSE) {
            depth++;
        } else if(kind == ZIR_STMT_BLOCK_OPEN || kind == ZIR_STMT_IF ||
                  kind == ZIR_STMT_WHILE || kind == ZIR_STMT_FOR) {
            if(depth == 0)
                return t;
            depth--;
        }
    }
    return -1;
}

/* Whether NAME is a record local of FN: a record declared once, not
 * through `using` and not as a pointer, whose declaration goes in DECL, or
 * a record parameter passed by value that no declaration shadows, with
 * DECL -1. */
static int
record_local(const ZirFunction *fn, const char *name, int *decl)
{
    *decl = -1;
    for(int s = 0; s < fn->stmt_count; s++) {
        const ZirStmt *st = &fn->stmts[s];
        if(st->kind != ZIR_STMT_DECL || st->name == NULL || strcmp(st->name, name))
            continue;
        if(*decl >= 0 || st->is_using || st->lhs_root >= 0 || st->type == NULL ||
           !*skip_ws(st->type) || !strcmp(skip_ws(st->type), "string") ||
           pointer_type(st->type))
            return 0;
        *decl = s;
    }
    if(*decl >= 0)
        return 1;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    for(int p = 0; p < parameters->count; p++) {
        const ZirParameter *item = &parameters->items[p];
        const char *bound = item->binding_name ? item->binding_name : item->name;
        const char *type = item->value_type ? item->value_type : item->type;
        if(bound == NULL || strcmp(bound, name))
            continue;
        type = type ? skip_ws(type) : "";
        return !item->is_using && *type && strcmp(type, "string") && !pointer_type(type);
    }
    return 0;
}

/* Whether PLACE is record local NAME, or a field path of it that holds the
 * string at PATH, so `=` into PLACE replaces that string. */
static int
place_holds(const ZirFunction *fn, int place, const char *name, const char *path)
{
    const char *fields[16];
    int count = 0;
    while(place >= 0 && fn->exprs[place].kind == ZIR_EXPR_MEMBER) {
        if(count == 16 || fn->exprs[place].name == NULL)
            return 0;
        fields[count++] = fn->exprs[place].name;
        place = fn->exprs[place].left;
    }
    if(place < 0 || fn->exprs[place].kind != ZIR_EXPR_IDENT ||
       fn->exprs[place].is_global_value || fn->exprs[place].is_this ||
       fn->exprs[place].name == NULL || strcmp(fn->exprs[place].name, name))
        return 0;
    while(count > 0) {
        const char *field = fields[--count];
        size_t n = strcspn(path, ".");
        if(strlen(field) != n || strncmp(field, path, n))
            return 0;
        if(!path[n])
            return count == 0;
        path += n + 1;
    }
    return 1;
}

/* The first statement after FROM and before END, in FROM's own block, that
 * replaces the string at PATH in record local NAME with `=`, or -1. Every
 * way on from FROM runs it unless a break or continue jumps past it. */
static int
replacing_statement(const ZirFunction *fn, const char *name, int from, int end,
                    const char *path)
{
    int block = from >= 0 ? enclosing_block(fn, from) : -1;
    for(int t = from + 1; t < end; t++) {
        const ZirStmt *st = &fn->stmts[t];
        if(st->kind == ZIR_STMT_BREAK || st->kind == ZIR_STMT_CONTINUE)
            return -1;
        if(st->kind == ZIR_STMT_ASSIGN && !strcmp(st->assignment_op, "=") &&
           place_holds(fn, st->lhs_root, name, path) && enclosing_block(fn, t) == block)
            return t;
    }
    return -1;
}

/* Where the string at field path PATH, such as `command.value`, of record
 * local NAME goes once statement FROM has stored it there. DECL declares
 * the local, or is -1 for a record parameter, which holds its fields from
 * entry and whose block is the whole function. Uses after FROM inside the
 * local's block are followed, and the ones before it too when a loop
 * around FROM inside that block runs them again. A use of another field
 * reads no part of the string. A later `=` into the path, or into a
 * record on it, in FROM's own block replaces the string, so the uses after
 * it are not followed. A defer may run anywhere in its block, so with one
 * in the local's block, every use there is followed and nothing replaces
 * the string. */
static int
local_record_flow(TextScope *scope, const ZirModule *module, const ZirFunction *fn,
                  const Parents *parents, const char *name, int decl, int from,
                  const char *path)
{
    for(int i = 0; i < scope->walking_count; i++)
        if(scope->walking[i].fn == fn && scope->walking[i].s == from &&
           scope->walking[i].path != NULL && !strcmp(scope->walking[i].path, path))
            return FLOW_READ;
    if(scope->walking_count == (int)(sizeof(scope->walking) / sizeof(scope->walking[0])))
        return FLOW_RETAINED;
    int end = decl < 0 ? fn->stmt_count : block_end(fn, decl);
    int deferred = 0;
    for(int t = decl + 1; t < end && !deferred; t++)
        deferred = fn->stmts[t].kind == ZIR_STMT_DEFER;
    int first = from + 1, last = end - 1;
    int replaced = deferred ? -1 : replacing_statement(fn, name, from, end, path);
    if(deferred)
        first = decl + 1;
    else if(replaced >= 0)
        last = replaced;
    else if(from >= 0)
        for(int b = enclosing_block(fn, from); b > decl; b = enclosing_block(fn, b))
            if(fn->stmts[b].kind == ZIR_STMT_WHILE || fn->stmts[b].kind == ZIR_STMT_FOR)
                first = b;
    int walking = scope->walking_count++;
    scope->walking[walking].fn = fn;
    scope->walking[walking].s = from;
    scope->walking[walking].path = path;
    int result = FLOW_READ;
    for(int x = 0; result && x < fn->expr_count; x++) {
        const ZirExpr *e = &fn->exprs[x];
        if(e->kind != ZIR_EXPR_IDENT || e->is_this || e->is_function_value ||
           e->is_global_value || e->name == NULL || strcmp(e->name, name) ||
           parents->of[x] == -1)
            continue;
        int at = statement_of(parents, x);
        if(at < first || at > last)
            continue;
        /* Follow the field path as far as this use names it. */
        int node = x, other = 0;
        const char *rest = path;
        while(*rest) {
            int up = parents->of[node];
            if(up < 0 || fn->exprs[up].kind != ZIR_EXPR_MEMBER || fn->exprs[up].left != node)
                break;
            size_t n = strcspn(rest, ".");
            const char *field = fn->exprs[up].name;
            if(field == NULL || strlen(field) != n || strncmp(field, rest, n)) {
                other = 1;
                break;
            }
            node = up;
            rest += rest[n] ? n + 1 : n;
        }
        if(other)
            continue;
        int up = parents->of[node];
        if(up < -1 && fn->stmts[-2 - up].lhs_root == node) {
            /* `=` replaces the string and reads none of it. */
            const ZirStmt *st = &fn->stmts[-2 - up];
            if(st->kind != ZIR_STMT_ASSIGN || strcmp(st->assignment_op, "="))
                result = FLOW_RETAINED;
            continue;
        }
        result = combine(result, *rest ?
                         record_flow(scope, module, fn, parents, node, rest) :
                         value_flow(scope, module, fn, parents, node));
    }
    scope->walking_count = walking;
    return result;
}

/* Where the string at PATH inside the record that statement S assigns
 * goes: S's place is a record local, or a field path of one reached
 * without following a pointer, and the string goes where that path of
 * the local goes after S. */
static int
stored_flow(TextScope *scope, const ZirModule *module, const ZirFunction *fn,
            const Parents *parents, int s, const char *path)
{
    char full[ZIR_NAME_MAX], joined[ZIR_NAME_MAX];
    if(snprintf(full, sizeof(full), "%s", path) >= (int)sizeof(full))
        return FLOW_RETAINED;
    int place = fn->stmts[s].lhs_root;
    while(place >= 0 && fn->exprs[place].kind == ZIR_EXPR_MEMBER) {
        int left = fn->exprs[place].left;
        if(left < 0 || fn->exprs[place].name == NULL || pointer_type(fn->exprs[left].type) ||
           snprintf(joined, sizeof(joined), "%s.%s", fn->exprs[place].name, full) >=
               (int)sizeof(joined))
            return FLOW_RETAINED;
        memcpy(full, joined, sizeof(full));
        place = left;
    }
    const ZirExpr *root = place >= 0 ? &fn->exprs[place] : NULL;
    int decl;
    if(root == NULL || root->kind != ZIR_EXPR_IDENT || root->is_global_value ||
       root->is_this || root->name == NULL || !record_local(fn, root->name, &decl) ||
       (decl >= 0 && (s <= decl || s >= block_end(fn, decl))))
        return FLOW_RETAINED;
    return local_record_flow(scope, module, fn, parents, root->name, decl, s,
                             KeepName(full));
}

/* Where the string at field path PATH inside the record value NODE goes.
 * Reading another field of NODE reads none of it. The path is followed
 * through record literal fields, `ifx` arms, call arguments and record
 * locals that take the record. */
static int
record_flow(TextScope *scope, const ZirModule *module, const ZirFunction *fn,
            const Parents *parents, int node, const char *path)
{
    int parent = parents->of[node];
    if(parent < -1) {
        int s = -2 - parent;
        const ZirStmt *st = &fn->stmts[s];
        int decl;
        if(st->expr_root != node)
            return FLOW_RETAINED;
        if(st->kind == ZIR_STMT_EXPR || st->kind == ZIR_STMT_UNUSED)
            return FLOW_READ;
        /* A record local declared from it, as in `request := Request.{...}`. */
        if(st->kind == ZIR_STMT_DECL && st->name != NULL &&
           record_local(fn, st->name, &decl) && decl == s)
            return local_record_flow(scope, module, fn, parents, st->name, s, s, path);
        /* Stored into a record local declared outside the statement's
         * block, as in `wrapped.data = command`, only while a parameter is
         * followed: a fresh Text is freed when that block closes. */
        if(scope->borrowed && st->kind == ZIR_STMT_ASSIGN &&
           !strcmp(st->assignment_op, "="))
            return stored_flow(scope, module, fn, parents, s, path);
        return FLOW_RETAINED;
    }
    if(parent < 0)
        return FLOW_RETAINED;
    const ZirExpr *e = &fn->exprs[parent];
    switch(e->kind) {
    case ZIR_EXPR_MEMBER: {
        if(e->left != node || e->name == NULL || pointer_type(fn->exprs[node].type))
            return FLOW_RETAINED;
        size_t n = strcspn(path, ".");
        if(strlen(e->name) != n || strncmp(e->name, path, n))
            return FLOW_READ;
        return path[n] ? record_flow(scope, module, fn, parents, parent, path + n + 1) :
            value_flow(scope, module, fn, parents, parent);
    }
    case ZIR_EXPR_CALL:
        return argument_flow(scope, module, fn, parents, parent, node, path);
    case ZIR_EXPR_CONDITIONAL:
        return e->right == node || e->third == node ?
            record_flow(scope, module, fn, parents, parent, path) : FLOW_RETAINED;
    case ZIR_EXPR_FIELD_INIT: {
        int record = parents->of[parent];
        if(e->right != node || e->name == NULL || !*e->name || record < 0 ||
           fn->exprs[record].kind != ZIR_EXPR_COMPOUND)
            return FLOW_RETAINED;
        return record_flow(scope, module, fn, parents, record,
                           KeepNameFormat("%s.%s", e->name, path));
    }
    default:
        return FLOW_RETAINED;
    }
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
keep_local_text(ZirFunction *fn, const Parents *parents, int s, int call)
{
    const char *name = fn->stmts[s].name;
    int count = fn->expr_count;
    for(int x = 0; x < count; x++) {
        if(fn->exprs[x].kind != ZIR_EXPR_IDENT || fn->exprs[x].is_this ||
           fn->exprs[x].name == NULL || strcmp(fn->exprs[x].name, name) ||
           parents->of[x] == -1 || declaration_of(fn, parents, x) != s)
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
        if(call < 0 || !keep_local_text(fn, &parents, s, call)) {
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
            if(!value_read_only(scope, module, fn, &parents, x))
                continue;
            /* The moved call is the Text producer with its arguments. One
             * that may write through a pointer argument could change what
             * the statement reads before it, so it stays. */
            int moved = vec_module ? x : fn->exprs[x].first_child;
            if(tree_writes_arguments(scope, module, fn, moved))
                continue;
            int globals = !global_address_taken(fn) &&
                !tree_writes_globals(scope, module, fn, moved);
            if(scope->failed) {
                free(parents.of);
                return 0;
            }
            int else_if;
            int s = hoist_statement(fn, &parents, x, globals, &else_if);
            if(s < 0) {
                int rewritten = rewrite_condition(scope, module, fn, &parents, x,
                                                  globals, else_if, &serial);
                if(rewritten < 0) {
                    free(parents.of);
                    return 0;
                }
                done = !rewritten;
                continue;
            }
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

/* Whether FN only reads each string field of RECORD, a record parameter P
 * passed by value or a record it holds by value at field path PREFIX, such
 * as `wrapped.data`. Otherwise writes the field's path into KEPT. */
static int
fields_read_only(TextScope *scope, const ZirModule *owner, const ZirFunction *fn,
                 int p, const ZirModule *types, const ZirType *record,
                 const char *prefix, char *kept, size_t size, int depth)
{
    if(record->is_enum || record->is_procedure_type || record->is_extern ||
       record->is_record_template || depth > 8)
        return 1;
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1) {
        const char *type = skip_ws(field.type);
        char at[ZIR_NAME_MAX];
        if(snprintf(at, sizeof(at), "%s.%s", prefix, field.name) >= (int)sizeof(at)) {
            snprintf(kept, size, "%s", prefix);
            return 0;
        }
        if(!strcmp(type, "string")) {
            if(p >= 64 || fn->is_extern || fn->is_template ||
               parameter_flow(scope, owner, fn, p, KeepName(strchr(at, '.') + 1)) !=
                   FLOW_READ) {
                snprintf(kept, size, "%s", at);
                return 0;
            }
            continue;
        }
        const ZirModule *inner_types = NULL;
        const ZirType *inner = pointer_type(type) ? NULL :
            FindType(types, type, &inner_types);
        if(inner != NULL &&
           !fields_read_only(scope, owner, fn, p, inner_types ? inner_types : types,
                             inner, at, kept, size, depth + 1))
            return 0;
    }
    return 1;
}

/* Which string a procedure FN may keep: a string parameter, or a string
 * field of a record parameter passed by value, or of a record it holds by
 * value, that it does not only read.
 * Writes the name into KEPT and answers 0, or answers 1 when it keeps none. */
static int
reads_only(TextScope *scope, const ZirModule *owner, const ZirFunction *fn,
           char *kept, size_t size)
{
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    for(int p = 0; p < parameters->count; p++) {
        const ZirParameter *item = &parameters->items[p];
        const char *name = item->binding_name ? item->binding_name : item->name;
        const char *type = skip_ws(item->value_type ? item->value_type : item->type);
        const ZirModule *record_owner = NULL;
        const ZirType *record = NULL;
        if(!strcmp(type, "string")) {
            /* A foreign procedure only reads when it is `#reads` too. */
            if(p >= 64 || fn->is_extern || fn->is_template ||
               parameter_flow(scope, owner, fn, p, NULL) != FLOW_READ) {
                if(!fn->is_extern || !reading_foreign(owner, fn->name)) {
                    snprintf(kept, size, "%s", name);
                    return 0;
                }
            }
            continue;
        }
        if(pointer_type(type) || (record = FindType(owner, type, &record_owner)) == NULL)
            continue;
        if(!fields_read_only(scope, owner, fn, p, record_owner ? record_owner : owner,
                             record, name, kept, size, 0))
            return 0;
    }
    return 1;
}

/* Each procedure stored in a `#reads` procedure type, as a slot's value,
 * a record field or an argument, must only read its strings. */
static int
check_reads_slots(TextScope *scope, const ZirModule *module, const ZirFunction *fn)
{
    int ok = 1;
    for(int x = 0; x < fn->expr_count; x++) {
        const ZirExpr *e = &fn->exprs[x];
        const ZirType *slot = e->is_function_value && e->name ?
            reads_slot(module, e->type) : NULL;
        const ZirModule *owner = NULL;
        const ZirFunction *callee = NULL;
        char kept[ZIR_TEXT_MAX];
        if(slot == NULL ||
           ResolveFunction(module, e->name, &owner, &callee) != 1 ||
           callee == NULL || owner == NULL ||
           reads_only(scope, owner, callee, kept, sizeof(kept)))
            continue;
        Diagnostic(e->span, "check.slot_signature",
                   "%s keeps its parameter %s, but %s is #reads",
                   e->name, kept, slot->name);
        ok = 0;
    }
    return ok;
}

/* Whether a value of TYPE can hold a `#reads` procedure: the type itself,
 * an array of one or a record field. */
static int
holds_reads_slot(const ZirModule *module, const char *type, int depth)
{
    char element[ZIR_NAME_MAX];
    const ZirModule *owner = NULL;
    const ZirType *record;
    if(type == NULL || depth > 16)
        return 0;
    type = skip_ws(type);
    if(reads_slot(module, type) != NULL)
        return 1;
    if(ArrayElementType(type, element, sizeof(element), NULL))
        return holds_reads_slot(module, element, depth + 1);
    record = pointer_type(type) ? NULL : FindType(module, type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_extern || record->is_record_template)
        return 0;
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1)
        if(holds_reads_slot(owner ? owner : module, field.type, depth + 1))
            return 1;
    return 0;
}

/* A global's initializer stores procedures too, as in
 * `hook: Painter = Paint;` or `hooks: Hooks = .{paint = Paint};`. SCOPE
 * names the procedures; TYPES declares the type at INDEX. */
static int
check_reads_initializer(TextScope *scope, const ZirModule *scope_module,
                        const ZirModule *types, const ZirFunction *probe,
                        int index, const char *type, const ZirGlobal *global,
                        int depth)
{
    char element[ZIR_NAME_MAX];
    if(index < 0 || index >= probe->expr_count || type == NULL || depth > 16)
        return 1;
    const ZirExpr *e = &probe->exprs[index];
    type = skip_ws(type);
    const ZirType *slot = reads_slot(types, type);
    if(slot != NULL) {
        const ZirModule *owner = NULL;
        const ZirFunction *callee = NULL;
        char kept[ZIR_TEXT_MAX];
        const char *name = e->text && *e->text ? e->text : e->name;
        if((e->kind != ZIR_EXPR_IDENT && e->kind != ZIR_EXPR_MEMBER) || name == NULL ||
           ResolveFunction(scope_module, name, &owner, &callee) != 1 ||
           callee == NULL || owner == NULL ||
           reads_only(scope, owner, callee, kept, sizeof(kept)))
            return 1;
        Diagnostic(global->span, "check.slot_signature",
                   "%s keeps its parameter %s, but %s is #reads",
                   name, kept, slot->name);
        return 0;
    }
    if(e->kind != ZIR_EXPR_COMPOUND)
        return 1;
    int ok = 1;
    if(ArrayElementType(type, element, sizeof(element), NULL)) {
        for(int child = e->first_child; child >= 0; child = probe->exprs[child].next_sibling)
            if(probe->exprs[child].kind == ZIR_EXPR_FIELD_INIT &&
               !check_reads_initializer(scope, scope_module, types, probe,
                                        probe->exprs[child].right, element,
                                        global, depth + 1))
                ok = 0;
        return ok;
    }
    const ZirModule *owner = NULL;
    const ZirType *record = pointer_type(type) ? NULL : FindType(types, type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_extern || record->is_record_template)
        return 1;
    int ordinal = 0;
    for(int child = e->first_child; child >= 0;
        child = probe->exprs[child].next_sibling, ordinal++) {
        const ZirExpr *entry = &probe->exprs[child];
        if(entry->kind != ZIR_EXPR_FIELD_INIT)
            continue;
        size_t offset = 0;
        ZirTypeField field;
        int position = 0, found = 0;
        while(!found && TypeNextField(record, &offset, &field) == 1)
            found = entry->name && *entry->name ? !strcmp(field.name, entry->name) :
                position++ == ordinal;
        if(found && !check_reads_initializer(scope, scope_module, owner ? owner : types,
                                             probe, entry->right, field.type,
                                             global, depth + 1))
            ok = 0;
    }
    return ok;
}

int
ScopeTextArguments(ZirProgram **programs, int count)
{
    TextScope scope = {0};
    int ok = 1;
    int kept = 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            for(int f = 0; f < module->function_count; f++)
                if(!module->functions[f].is_template &&
                   !check_reads_slots(&scope, module, &module->functions[f]))
                    kept = 1;
            for(int g = 0; g < module->global_count; g++) {
                const ZirGlobal *global = &module->globals[g];
                if(global->init == NULL || !*global->init ||
                   !holds_reads_slot(module, global->type, 0))
                    continue;
                ZirFunction *probe = calloc(1, sizeof(*probe));
                if(probe == NULL) {
                    scope.failed = 1;
                    break;
                }
                int root = ParseExprTyped(probe, module, global->init, global->span,
                                          global->type);
                if(!check_reads_initializer(&scope, module, module, probe, root,
                                            global->type, global, 0))
                    kept = 1;
                free(probe->exprs);
                free(probe);
            }
        }
    if(kept) {
        free(scope.answers);
        free(scope.fields);
        return 0;
    }
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
