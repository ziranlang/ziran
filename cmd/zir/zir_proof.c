#include "zir_proof.h"
#include "zir_proof_kernel.h"
#include "zir_scalar.h"
#include "zir_parse.h"
#include "zir_expr.h"
#include "zir_check.h"
#include "zir_text.h"
#include "zir_diagnostic.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

enum { PARAMETERS = 16, LOCALS = 64 };
typedef struct Parameters {
    char names[PARAMETERS][ZIR_NAME_MAX];
    char types[PARAMETERS][ZIR_NAME_MAX];
    int count;
} Parameters;
typedef struct Binding { char name[ZIR_NAME_MAX]; int value; } Binding;
typedef struct Environment { Binding bindings[LOCALS]; int count; } Environment;
typedef struct Continuation { int start, stop, scope; const struct Continuation *next; } Continuation;
typedef struct ProofContext {
    ProofKernel kernel;
    const ZirFunction *active[PROOF_DEPTH];
    int active_count;
    const ZirFunction *unfold[PROOF_DEPTH];
    int unfold_count, unfold_all;
    const ZirLaw *laws[PROOF_DEPTH];
    int law_count;
    int unsupported;
    int invalid;
    char reason[256];
} ProofContext;

static int unsupported(ProofContext *c, const char *reason)
{
    c->unsupported = 1;
    copy_text(c->reason, sizeof(c->reason), reason);
    return -1;
}
static int tick(ProofContext *c, int depth)
{
    if(depth > PROOF_DEPTH || --c->kernel.budget < 0) {
        c->kernel.exhausted = 1;
        return 0;
    }
    return !c->kernel.exhausted;
}
static int identifier(const char *name)
{
    if(!isalpha((unsigned char)*name) && *name != '_') return 0;
    for(name++; *name; name++) if(!isalnum((unsigned char)*name) && *name != '_') return 0;
    return 1;
}
static int parameters(const char *source, Parameters *out)
{
    memset(out, 0, sizeof(*out));
    if(!*skip_ws(source)) return 1;
    char (*parts)[ZIR_TEXT_MAX] = calloc(PARAMETERS, ZIR_TEXT_MAX);
    if(!parts) return 0;
    int count = split_top_level(source, parts[0], PARAMETERS, ZIR_TEXT_MAX), ok = count > 0;
    for(int i = 0; ok && i < count; i++) {
        char *colon = strchr(parts[i], ':');
        if(!colon) { ok = 0; break; }
        *colon++ = 0;
        trim_in_place(parts[i]); trim_in_place(colon);
        if(!identifier(parts[i]) || strlen(parts[i]) >= ZIR_NAME_MAX ||
           !*colon || strlen(colon) >= ZIR_NAME_MAX || strchr(colon, '=') ||
           strchr(colon, '$')) { ok = 0; break; }
        for(int j = 0; j < i; j++) if(!strcmp(out->names[j], parts[i])) ok = 0;
        copy_text(out->names[i], ZIR_NAME_MAX, parts[i]);
        copy_text(out->types[i], ZIR_NAME_MAX, colon);
    }
    if(ok) out->count = count;
    free(parts);
    return ok;
}
static int scalar_type(ProofContext *c, const ZirModule *m, const char *name)
{
    if(!strcmp(name, "bool")) return 0;
    const ZirType *type = FindType(m, name, NULL);
    const char *scalar = type && type->is_enum ? type->enum_backing : ScalarType(name);
    unsigned width = ScalarWidth(scalar);
    if(!width) return unsupported(c, "proofs require booleans, fixed-width integers, or enums");
    return ProofTypeAdd(&c->kernel, (ProofType){width, scalar[0] == 'u',
                                               type && type->is_enum ? type : NULL});
}
static int same_type(ProofContext *c, int a, int b)
{
    return a >= 0 && b >= 0 && c->kernel.terms[a].type == c->kernel.terms[b].type;
}
static int env_find(const Environment *e, const char *name)
{
    for(int i = e->count - 1; i >= 0; i--) if(!strcmp(e->bindings[i].name, name)) return i;
    return -1;
}
static int env_add(ProofContext *c, Environment *e, const char *name, int value)
{
    if(value < 0) return 0;
    if(e->count == LOCALS) { c->kernel.exhausted = 1; return 0; }
    copy_text(e->bindings[e->count].name, ZIR_NAME_MAX, name);
    e->bindings[e->count++].value = value;
    return 1;
}
static int node(ProofContext *c, ProofOp op, int type, int a, int b, int third, uint64_t value)
{
    return ProofNode(&c->kernel, op, type, a, b, third, value);
}
static int expression(ProofContext *, const ZirModule *, const ZirFunction *, int,
                      Environment *, int, int);
static int call_name(const ZirFunction *fn, const ZirExpr *call, char *name, size_t size)
{
    if(call->name[0]) { copy_text(name, size, call->name); return 1; }
    if(call->left < 0 || call->left >= fn->expr_count) return 0;
    const ZirExpr *member = &fn->exprs[call->left];
    if(member->kind != ZIR_EXPR_MEMBER || member->left < 0 || member->left >= fn->expr_count ||
       fn->exprs[member->left].kind != ZIR_EXPR_IDENT) return 0;
    return snprintf(name, size, "%s.%s", fn->exprs[member->left].name, member->name) < (int)size;
}
static int sequence(ProofContext *, const ZirModule *, const ZirFunction *, Environment *,
                    int, int, const Continuation *, int, int);

static int block_close(const ZirFunction *fn, int open, int stop)
{
    int level = 1;
    for(int i = open + 1; i < stop; i++) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_BLOCK_OPEN || kind == ZIR_STMT_WHILE ||
           kind == ZIR_STMT_FOR || kind == ZIR_STMT_IF_CASE) level++;
        if(kind == ZIR_STMT_BLOCK_CLOSE && --level == 0) return i;
    }
    return -1;
}
static int function_value(ProofContext *c, const ZirModule *m, const ZirFunction *fn,
                          const int *args, int count, int depth)
{
    if(!tick(c, depth)) return -1;
    if(!fn->checked || fn->is_extern || fn->uses_host || strcmp(fn->effect_class, "pure"))
        return unsupported(c, "proofs cannot unfold foreign, observing, or mutating procedures");
    for(int i = 0; i < c->active_count; i++) if(c->active[i] == fn)
        return unsupported(c, "recursive procedures are outside the scalar proof fragment");
    if(c->active_count == PROOF_DEPTH) { c->kernel.exhausted = 1; return -1; }
    Parameters p;
    if(!parameters(fn->args, &p) || count != p.count)
        return unsupported(c, "proof calls require explicit scalar parameters");
    Environment *env = calloc(1, sizeof(*env));
    if(!env) { c->kernel.exhausted = 1; return -1; }
    int ok = 1;
    for(int i = 0; ok && i < count; i++) {
        int type = scalar_type(c, m, p.types[i]);
        if(args[i] < 0 || type < 0 || c->kernel.terms[args[i]].type != type)
            ok = 0;
        else ok = env_add(c, env, p.names[i], args[i]);
    }
    int result = -1, type = scalar_type(c, m, fn->return_type);
    if(ok && type >= 0) {
        c->active[c->active_count++] = fn;
        result = sequence(c, m, fn, env, 0, fn->stmt_count, NULL, type, depth + 1);
        c->active_count--;
    } else unsupported(c, "procedure arguments do not match their declared scalar types");
    free(env);
    return result;
}

static int tag(ProofContext *c, const ZirFunction *fn, int index, int result)
{
    /* Program bodies have already been checked. The proposition and certificate
     * graphs are retyped on every load; saved annotations cannot authorize a step. */
    if(result >= 0 && !fn->checked) {
        ZirExpr *e = &((ZirFunction *)fn)->exprs[index];
        int type = c->kernel.terms[result].type;
        if(!type) copy_text(e->type, sizeof(e->type), "bool");
        else if(c->kernel.types[type].identity)
            copy_text(e->type, sizeof(e->type), ((const ZirType *)c->kernel.types[type].identity)->name);
        else snprintf(e->type, sizeof(e->type), "%c%u",
                      c->kernel.types[type].is_unsigned ? 'u' : 's', c->kernel.types[type].width);
    }
    return result;
}
static int expression(ProofContext *c, const ZirModule *m, const ZirFunction *fn, int index,
                      Environment *env, int expected, int depth)
{
    if(!tick(c, depth) || index < 0 || index >= fn->expr_count) return -1;
    const ZirExpr *e = &fn->exprs[index];
    int result = -1;
    if(e->kind == ZIR_EXPR_INT) {
        char *end;
        errno = 0;
        uint64_t value = strtoull(e->text, &end, 0);
        if(errno || *end || end == e->text) return unsupported(c, "invalid integer literal in proof");
        int type = expected > 0 ? expected :
                   scalar_type(c, m, value > INT64_MAX ? "u64" : "s64");
        if(type < 0) return -1;
        result = node(c, P_CONST, type, -1, -1, -1, value);
    } else if(e->kind == ZIR_EXPR_IDENT) {
        int at = env_find(env, e->name);
        if(at >= 0) result = env->bindings[at].value;
        else if(!strcmp(e->name, "true") || !strcmp(e->name, "false"))
            result = node(c, P_CONST, 0, -1, -1, -1, !strcmp(e->name, "true"));
        else {
            const ZirDefine *d = NULL;
            for(int i = 0; i < m->define_count; i++)
                if(!strcmp(m->defines[i].name, e->name)) d = &m->defines[i];
            if(!d) return unsupported(c, "proof expressions cannot read runtime globals or unresolved names");
            ZirFunction probe = {0};
            int root = ParseExprNoDefaults(&probe, m, d->value, d->span);
            Environment *empty = calloc(1, sizeof(*empty));
            if(empty == NULL) {
                free(probe.exprs);
                return unsupported(c, "cannot allocate proof constant environment");
            }
            result = expression(c, m, &probe, root, empty, expected, depth + 1);
            free(empty);
            free(probe.exprs);
        }
    } else if(e->kind == ZIR_EXPR_MEMBER) {
        if(e->left < 0 || e->left >= fn->expr_count || fn->exprs[e->left].kind != ZIR_EXPR_IDENT)
            return unsupported(c, "record and pointer members are outside scalar proofs");
        const ZirType *t = FindType(m, fn->exprs[e->left].name, NULL);
        int64_t value;
        if(!t || !t->is_enum || !EnumMemberValue(t, e->name, &value))
            return unsupported(c, "unresolved enum member in proof");
        int type = scalar_type(c, m, fn->exprs[e->left].name);
        result = node(c, P_CONST, type, -1, -1, -1, (uint64_t)value);
    } else if(e->kind == ZIR_EXPR_CAST) {
        int type = scalar_type(c, m, e->name);
        int operand = expression(c, m, fn, e->right, env, -1, depth + 1);
        if(type < 0 || operand < 0) return -1;
        ProofTerm t = c->kernel.terms[operand];
        if(t.type == type) result = operand;
        else if(t.op == P_CONST) {
            uint64_t bits = t.value;
            if(t.type && !c->kernel.types[t.type].is_unsigned)
                bits = (uint64_t)ScalarSigned(bits, c->kernel.types[t.type].width);
            result = node(c, P_CONST, type, -1, -1, -1, type ? bits : bits != 0);
        } else return unsupported(c, "symbolic conversions between scalar types need a separate proof rule");
    } else if(e->kind == ZIR_EXPR_UNARY) {
        int type = e->type[0] && fn->checked ? scalar_type(c, m, e->type) : expected;
        int operand = expression(c, m, fn, e->right, env,
                                 !strcmp(e->op, "!") ? 0 : type, depth + 1);
        if(operand < 0) return -1;
        type = c->kernel.terms[operand].type;
        if(type && c->kernel.types[type].identity)
            return unsupported(c, "enum proofs currently support equality only");
        if(!strcmp(e->op, "+") && type) result = operand;
        else if(!strcmp(e->op, "!") && !type) result = node(c, P_NOT, 0, operand, -1, -1, 0);
        else if(!strcmp(e->op, "-") && type)
            result = node(c, P_SUB, type, node(c, P_CONST, type, -1, -1, -1, 0), operand, -1, 0);
        else return unsupported(c, "unsupported unary operation in scalar proof");
    } else if(e->kind == ZIR_EXPR_BINARY) {
        if(e->left < 0 || e->right < 0 || e->left >= fn->expr_count || e->right >= fn->expr_count)
            return unsupported(c, "malformed proof expression graph");
        int numeric_expected = expected > 0 ? expected : -1;
        if(fn->checked && e->type[0] && strcmp(e->type, "bool"))
            numeric_expected = scalar_type(c, m, e->type);
        int reverse = fn->exprs[e->left].kind == ZIR_EXPR_INT ||
                      (fn->exprs[e->left].kind == ZIR_EXPR_UNARY &&
                       fn->exprs[e->left].right >= 0 &&
                       fn->exprs[fn->exprs[e->left].right].kind == ZIR_EXPR_INT);
        int b = -1, a = -1;
        if(reverse) {
            b = expression(c, m, fn, e->right, env, numeric_expected, depth + 1);
            if(b >= 0) a = expression(c, m, fn, e->left, env, c->kernel.terms[b].type, depth + 1);
        } else {
            a = expression(c, m, fn, e->left, env, numeric_expected, depth + 1);
            if(a >= 0) b = expression(c, m, fn, e->right, env, c->kernel.terms[a].type, depth + 1);
        }
        if(a < 0 || b < 0) return -1;
        if(!same_type(c, a, b)) return unsupported(c, "scalar proof operands require identical types");
        int type = c->kernel.terms[a].type;
        ProofOp op;
        if(!strcmp(e->op, "+") && type) op = P_ADD;
        else if(!strcmp(e->op, "-") && type) op = P_SUB;
        else if(!strcmp(e->op, "*") && type) op = P_MUL;
        else if(!strcmp(e->op, "==") || !strcmp(e->op, "!=")) op = P_EQ;
        else if(!strcmp(e->op, "<") && type) op = P_LT;
        else if(!strcmp(e->op, "<=") && type) op = P_LE;
        else if(!strcmp(e->op, ">") && type) { op = P_LT; int tmp = a; a = b; b = tmp; }
        else if(!strcmp(e->op, ">=") && type) { op = P_LE; int tmp = a; a = b; b = tmp; }
        else if(!strcmp(e->op, "&&") && !type) op = P_AND;
        else if(!strcmp(e->op, "||") && !type) op = P_OR;
        else return unsupported(c, "unsupported binary operation in scalar proof");
        if(type && c->kernel.types[type].identity &&
           op != P_EQ) return unsupported(c, "enum proofs currently support equality only");
        result = node(c, op, op == P_ADD || op == P_SUB || op == P_MUL ? type : 0, a, b, -1, 0);
        if(!strcmp(e->op, "!=")) result = node(c, P_NOT, 0, result, -1, -1, 0);
    } else if(e->kind == ZIR_EXPR_CONDITIONAL) {
        int cond = expression(c, m, fn, e->left, env, 0, depth + 1);
        int a = expression(c, m, fn, e->right, env, expected, depth + 1);
        int b = a < 0 ? -1 : expression(c, m, fn, e->third, env, c->kernel.terms[a].type, depth + 1);
        if(cond < 0 || a < 0 || b < 0 || c->kernel.terms[cond].type || !same_type(c, a, b))
            return unsupported(c, "ill-typed conditional in proof");
        result = node(c, P_ITE, c->kernel.terms[a].type, cond, a, b, 0);
    } else if(e->kind == ZIR_EXPR_CALL) {
        const ZirModule *owner = NULL;
        const ZirFunction *callee = NULL;
        char name[ZIR_NAME_MAX];
        if(!call_name(fn, e, name, sizeof(name)) ||
           ResolveFunctionAt(m, name, SpanPath(e->span), &owner, &callee) != 1)
            return unsupported(c, "unresolved or indirect procedure in proof");
        int allowed = c->unfold_all || c->active_count > 0;
        for(int i = 0; i < c->unfold_count; i++) if(c->unfold[i] == callee) allowed = 1;
        if(!allowed) return unsupported(c, "procedure calls need an unfold step");
        Parameters *p = malloc(sizeof(*p));
        if(p == NULL) return unsupported(c, "cannot allocate proof call parameters");
        if(!parameters(callee->args, p)) {
            free(p);
            return unsupported(c, "unsupported procedure signature in proof");
        }
        int args[PARAMETERS], count = 0;
        unsigned used = 0;
        for(int at = e->first_child; at >= 0; at = fn->exprs[at].next_sibling) {
            if(!tick(c, depth) || at >= fn->expr_count || count == PARAMETERS) {
                free(p);
                return unsupported(c, "malformed proof call arguments");
            }
            int position = fn->exprs[at].argument_index;
            if(!fn->checked) position = count;
            if(position < 0 || position >= p->count || (used & (1u << position)) ||
               (!fn->checked && fn->exprs[at].argument_name[0])) {
                free(p);
                return unsupported(c, "proof expressions use positional arguments");
            }
            used |= 1u << position;
            int type = scalar_type(c, owner, p->types[position]);
            args[position] = expression(c, m, fn, at, env, type, depth + 1);
            if(args[position] < 0) {
                free(p);
                return -1;
            }
            count++;
        }
        if(count != p->count) {
            free(p);
            return unsupported(c, "proof calls require all procedure arguments");
        }
        free(p);
        result = function_value(c, owner, callee, args, count, depth + 1);
    } else return unsupported(c, "expression is outside the pure scalar proof fragment");
    return tag(c, fn, index, result);
}

static int arms(ProofContext *c, const ZirModule *m, const ZirFunction *fn, Environment *env,
                int at, int after, const Continuation *rest, int return_type, int depth)
{
    if(!tick(c, depth)) return -1;
    if(at == after) return sequence(c, m, fn, env, rest->start, rest->stop,
                                    rest->next, return_type, depth + 1);
    int close = block_close(fn, at, after);
    if(close < 0) return unsupported(c, "malformed conditional body in proof");
    Environment *yes = malloc(sizeof(*yes));
    if(!yes) { c->kernel.exhausted = 1; return -1; }
    *yes = *env;
    int a = sequence(c, m, fn, yes, at + 1, close, rest, return_type, depth + 1);
    free(yes);
    if(a < 0) return -1;
    if(fn->stmts[at].is_else && fn->stmts[at].expr_root < 0) return a;
    int cond = expression(c, m, fn, fn->stmts[at].expr_root, env, 0, depth + 1);
    if(cond < 0 || c->kernel.terms[cond].type) return unsupported(c, "proof branch condition must be boolean");
    int b = arms(c, m, fn, env, close + 1, after, rest, return_type, depth + 1);
    if(b < 0 || !same_type(c, a, b)) return -1;
    return node(c, P_ITE, return_type, cond, a, b, 0);
}
static int sequence(ProofContext *c, const ZirModule *m, const ZirFunction *fn, Environment *env,
                    int start, int stop, const Continuation *next, int return_type, int depth)
{
    if(!tick(c, depth)) return -1;
    for(int i = start; i < stop; i++) {
        if(!tick(c, depth)) return -1;
        const ZirStmt *s = &fn->stmts[i];
        if(s->kind == ZIR_STMT_RETURN) {
            int value = expression(c, m, fn, s->expr_root, env, return_type, depth + 1);
            if(value >= 0 && c->kernel.terms[value].type == return_type) return value;
            return unsupported(c, "proof return type differs from its declared type");
        }
        if(s->kind == ZIR_STMT_DECL) {
            int type = scalar_type(c, m, s->type);
            int value = s->expr_root >= 0 ? expression(c, m, fn, s->expr_root, env, type, depth + 1) :
                        node(c, P_CONST, type, -1, -1, -1, 0);
            if(type < 0 || value < 0 || c->kernel.terms[value].type != type ||
               !env_add(c, env, s->name, value)) return -1;
        } else if(s->kind == ZIR_STMT_ASSIGN) {
            if(s->lhs_root < 0 || s->lhs_root >= fn->expr_count ||
               fn->exprs[s->lhs_root].kind != ZIR_EXPR_IDENT)
                return unsupported(c, "proof procedures can assign only scalar locals");
            int local = env_find(env, fn->exprs[s->lhs_root].name);
            if(local < 0) return unsupported(c, "proof procedure writes outside its local scope");
            int old = env->bindings[local].value, type = c->kernel.terms[old].type;
            int value = expression(c, m, fn, s->expr_root, env, type, depth + 1);
            if(value < 0 || c->kernel.terms[value].type != type) return -1;
            if(strcmp(s->assignment_op, "=") && s->assignment_op[0]) {
                ProofOp op;
                if(!strcmp(s->assignment_op, "+=")) op = P_ADD;
                else if(!strcmp(s->assignment_op, "-=")) op = P_SUB;
                else if(!strcmp(s->assignment_op, "*=")) op = P_MUL;
                else return unsupported(c, "unsupported scalar assignment in proof");
                value = node(c, op, type, old, value, -1, 0);
            }
            env->bindings[local].value = value;
        } else if(s->kind == ZIR_STMT_IF) {
            int after = i;
            do {
                int close = block_close(fn, after, stop);
                if(close < 0) return unsupported(c, "malformed branch in proof procedure");
                after = close + 1;
            } while(after < stop && fn->stmts[after].kind == ZIR_STMT_IF && fn->stmts[after].is_else);
            Continuation rest = {after, stop, env->count, next};
            return arms(c, m, fn, env, i, after, &rest, return_type, depth + 1);
        } else if(s->kind == ZIR_STMT_BLOCK_OPEN) {
            int close = block_close(fn, i, stop);
            if(close < 0) return unsupported(c, "malformed scope in proof procedure");
            Continuation rest = {close + 1, stop, env->count, next};
            return sequence(c, m, fn, env, i + 1, close, &rest, return_type, depth + 1);
        } else if(s->kind == ZIR_STMT_EXPR) {
            if(expression(c, m, fn, s->expr_root, env, -1, depth + 1) < 0) return -1;
        } else if(s->kind != ZIR_STMT_UNUSED)
            return unsupported(c, "loops, pointers, and effects are outside scalar proofs");
    }
    if(next) {
        env->count = next->scope;
        return sequence(c, m, fn, env, next->start, next->stop, next->next, return_type, depth + 1);
    }
    return unsupported(c, "proof procedures must return on every branch");
}

static ZirLaw *find_law(const ZirModule *m, const char *name, int depth, const ZirModule **owner)
{
    if(depth > 16) return NULL;
    const char *dot = strchr(name, '.');
    if(dot) {
        for(int i = 0; i < m->import_count; i++) {
            const ZirImport *import = &m->imports[i];
            if(import->resolved_module && strlen(import->name) == (size_t)(dot - name) &&
               !strncmp(import->name, name, (size_t)(dot - name)))
                return find_law(import->resolved_module, dot + 1, depth + 1, owner);
        }
        return NULL;
    }
    for(int i = 0; i < m->law_count; i++) if(!strcmp(m->laws[i].name, name)) {
        *owner = m; return &((ZirModule *)m)->laws[i];
    }
    ZirLaw *found = NULL;
    for(int i = 0; i < m->import_count; i++)
        if(m->imports[i].kind == ZIR_IMPORT_OPEN && m->imports[i].resolved_module) {
            const ZirModule *candidate_owner = NULL;
            ZirLaw *candidate = find_law(m->imports[i].resolved_module, name, depth + 1, &candidate_owner);
            if(candidate && found && found != candidate) return NULL;
            if(candidate) { found = candidate; *owner = candidate_owner; }
        }
    return found;
}

ZirLaw *FindVisibleLaw(const ZirModule *m, const char *name, const ZirModule **owner)
{
    return find_law(m, name, 0, owner);
}

static int graph_visit(const ZirFunction *fn, int at, unsigned char *seen, int depth)
{
    if(at == -1) return 1;
    if(at < 0 || at >= fn->expr_count || depth > PROOF_DEPTH || seen[at] == 1) return 0;
    if(seen[at] == 2) return 1;
    seen[at] = 1;
    const ZirExpr *e = &fn->exprs[at];
    if(!graph_visit(fn, e->left, seen, depth + 1) ||
       !graph_visit(fn, e->right, seen, depth + 1) ||
       !graph_visit(fn, e->third, seen, depth + 1) ||
       !graph_visit(fn, e->first_child, seen, depth + 1) ||
       !graph_visit(fn, e->next_sibling, seen, depth + 1)) return 0;
    seen[at] = 2;
    return 1;
}
static int graph_valid(const ZirFunction *fn)
{
    if(fn->expr_count < 0 || fn->expr_count > PROOF_TERMS || fn->checked) return 0;
    unsigned char *seen = calloc((size_t)fn->expr_count + 1, 1);
    if(!seen) return 0;
    int ok = 1;
    for(int i = 0; ok && i < fn->expr_count; i++) {
        const ZirExpr *e = &fn->exprs[i];
        ok = e->kind > ZIR_EXPR_UNKNOWN && e->kind <= ZIR_EXPR_COMPILE_TIME &&
             (e->is_function_value == 0 || e->is_function_value == 1) &&
             (e->is_this == 0 || e->is_this == 1) &&
             e->argument_index >= -1 && e->argument_index < 64 && graph_visit(fn, i, seen, 0);
    }
    free(seen);
    return ok;
}

static int parse_proof_declaration(ZirModule *m, const char *source, ZirSourceSpan span)
{
    const char *p = skip_ws(source + strlen("#proof")), *start = p;
    while(isalnum((unsigned char)*p) || *p == '_' || *p == '.') p++;
    size_t length = (size_t)(p - start);
    if(!length || length >= ZIR_NAME_MAX) return 0;
    char name[ZIR_NAME_MAX]; memcpy(name, start, length); name[length] = 0;
    p = skip_ws(p);
    if(*p++ != '{') return 0;
    const char *end = strrchr(p, '}');
    if(!end || (*skip_ws(end + 1) && strcmp(skip_ws(end + 1), ";")) ||
       (size_t)(end - p) >= ZIR_TEXT_MAX) return 0;
    char *body = malloc((size_t)(end - p) + 1);
    if(!body) return 0;
    memcpy(body, p, (size_t)(end - p)); body[end - p] = 0;
    int ok = ModuleAddProof(m, name, body, span);
    free(body);
    return ok;
}
int ParseProofDeclaration(ZirModule *m, const char *source, ZirSourceSpan span)
{
    char *clean = strdup(source);
    if(!clean) return 0;
    for(char *at = clean; *at; at++) {
        if(*at == '"' || *at == '\'') {
            char quote = *at++;
            while(*at && *at != quote) {
                if(*at == '\\' && at[1]) at++;
                at++;
            }
            if(!*at) break;
        } else if(at[0] == '/' && at[1] == '/') {
            while(*at && *at != '\n') *at++ = ' ';
            if(!*at) break;
        }
    }
    int ok = parse_proof_declaration(m, clean, span);
    free(clean);
    return ok;
}
static int parse_steps(ZirProof *p, const ZirModule *m)
{
    const char *cursor = p->source;
    int closed = 0;
    while(*(cursor = skip_ws(cursor))) {
        const char *end = strchr(cursor, ';');
        if(!end || (size_t)(end - cursor) >= ZIR_TEXT_MAX || closed) return 0;
        char *text = malloc((size_t)(end - cursor) + 1);
        if(!text) return 0;
        memcpy(text, cursor, (size_t)(end - cursor)); text[end - cursor] = 0;
        trim_in_place(text);
        const char *operand = text;
        while(isalpha((unsigned char)*operand)) operand++;
        size_t length = (size_t)(operand - text);
        char word[16];
        if(!length || length >= sizeof(word)) { free(text); return 0; }
        memcpy(word, text, length); word[length] = 0;
        operand = skip_ws(operand);
        ZirProofStep step = {0}; step.term_root = -1; step.span = p->span;
        if(!strcmp(word, "unfold")) step.kind = ZIR_PROOF_UNFOLD;
        else if(!strcmp(word, "cases")) step.kind = ZIR_PROOF_CASES;
        else if(!strcmp(word, "rewrite")) step.kind = ZIR_PROOF_REWRITE;
        else if(!strcmp(word, "use")) { step.kind = ZIR_PROOF_USE; closed = 1; }
        else if(!strcmp(word, "refl")) { step.kind = ZIR_PROOF_REFL; closed = 1; }
        else if(!strcmp(word, "ring")) { step.kind = ZIR_PROOF_RING; closed = 1; }
        else if(!strcmp(word, "order")) { step.kind = ZIR_PROOF_ORDER; closed = 1; }
        else { free(text); return 0; }
        if(step.kind == ZIR_PROOF_REWRITE || step.kind == ZIR_PROOF_USE) {
            step.term_root = ParseExprNoDefaults(&p->terms, m, operand, step.span);
            if(step.term_root < 0 || p->terms.exprs[step.term_root].kind != ZIR_EXPR_CALL ||
               !call_name(&p->terms, &p->terms.exprs[step.term_root], step.target,
                          sizeof(step.target))) { free(text); return 0; }
        } else if(step.kind == ZIR_PROOF_UNFOLD) {
            if(strlen(operand) >= sizeof(step.target)) { free(text); return 0; }
            copy_text(step.target, sizeof(step.target), operand);
        } else if(*operand) { free(text); return 0; }
        ZirProofStep *steps = realloc(p->steps, (size_t)(p->step_count + 1) * sizeof(*steps));
        free(text);
        if(!steps) return 0;
        p->steps = steps; p->steps[p->step_count++] = step;
        cursor = end + 1;
    }
    return 1;
}

int PrepareLawProofs(ZirProgram **programs, int count)
{
    for(int p = 0; p < count; p++) for(int m = 0; m < programs[p]->module_count; m++) {
        ZirModule *module = &programs[p]->modules[m];
        for(int l = 0; l < module->law_count; l++) {
            ZirLaw *law = &module->laws[l];
            law->proof = NULL;
            if(strcmp(law->kind, "theorem")) {
                if(law->claim) { Diagnostic(law->span, "proof.claim", "non-theorem law has a proposition graph"); return 0; }
                continue;
            }
            const char *arrow = strstr(law->payload, "=>");
            if(!arrow || !*skip_ws(arrow + 2)) {
                Diagnostic(law->span, "proof.claim", "theorem requires typed bindings => boolean proposition"); return 0;
            }
            char *args = malloc((size_t)(arrow - law->payload) + 1);
            if(!args) return 0;
            memcpy(args, law->payload, (size_t)(arrow - law->payload)); args[arrow - law->payload] = 0;
            trim_in_place(args);
            Parameters bindings;
            int ok = parameters(args, &bindings);
            if(!law->claim && ok) {
                law->claim = calloc(1, sizeof(*law->claim));
                if(!law->claim) { free(args); return 0; }
                copy_text(law->claim->args, sizeof(law->claim->args), args);
                copy_text(law->claim->return_type, sizeof(law->claim->return_type), "bool");
                law->claim->span = law->span;
                int root = ParseExprNoDefaults(law->claim, module, skip_ws(arrow + 2), law->span);
                ZirStmt *s = FunctionAddStmt(law->claim, ZIR_STMT_RETURN, skip_ws(arrow + 2), law->span);
                if(s) s->expr_root = root;
                ok = root >= 0 && s != NULL;
            }
            if(ok) ok = law->claim && !strcmp(args, law->claim->args) &&
                !strcmp(law->claim->return_type, "bool") && law->claim->stmt_count == 1 &&
                law->claim->stmts[0].kind == ZIR_STMT_RETURN &&
                law->claim->stmts[0].expr_root >= 0 && law->claim->stmts[0].expr_root < law->claim->expr_count &&
                graph_valid(law->claim);
            free(args);
            if(!ok) { Diagnostic(law->span, "proof.claim", "malformed theorem proposition"); return 0; }
        }
    }
    for(int p = 0; p < count; p++) for(int m = 0; m < programs[p]->module_count; m++) {
        ZirModule *module = &programs[p]->modules[m];
        for(int i = 0; i < module->proof_count; i++) {
            ZirProof *proof = &module->proofs[i];
            const ZirModule *owner = NULL;
            ZirLaw *law = find_law(module, proof->name, 0, &owner);
            if(!law || strcmp(law->kind, "theorem") || law->proof) {
                Diagnostic(proof->span, "proof.identity", "proof %s must name one visible, unproved theorem", proof->name); return 0;
            }
            proof->owner = module;
            if(!proof->step_count && !parse_steps(proof, owner)) {
                Diagnostic(proof->span, "proof.syntax", "invalid proof steps for %s", proof->name); return 0;
            }
            if(proof->step_count > PROOF_TERMS || proof->terms.stmt_count || !graph_valid(&proof->terms)) {
                Diagnostic(proof->span, "proof.graph", "invalid proof expression graph"); return 0;
            }
            for(int s = 0; s < proof->step_count; s++) {
                ZirProofStep *step = &proof->steps[s];
                int application = step->kind == ZIR_PROOF_REWRITE || step->kind == ZIR_PROOF_USE;
                if(step->kind < ZIR_PROOF_UNFOLD || step->kind > ZIR_PROOF_ORDER ||
                   (application ? step->term_root < 0 || step->term_root >= proof->terms.expr_count :
                                  step->term_root != -1) ||
                   (!application && step->kind != ZIR_PROOF_UNFOLD && step->target[0])) {
                    Diagnostic(proof->span, "proof.graph", "invalid proof step"); return 0;
                }
            }
            law->proof = proof;
        }
    }
    return 1;
}

static int theorem(ProofContext *, const ZirModule *, const ZirLaw *, const int *, int, int);
static int application(ProofContext *c, const ZirModule *m, const ZirProof *proof,
                       const ZirProofStep *step, Environment *env, int *proposition, int depth)
{
    const ZirModule *owner = NULL;
    ZirLaw *law = find_law(proof->owner, step->target, 0, &owner);
    if(!law) law = find_law(m, step->target, 0, &owner);
    if(!law || strcmp(law->kind, "theorem") || !law->claim)
        return unsupported(c, "proof step names no visible theorem");
    if(step->term_root < 0 || step->term_root >= proof->terms.expr_count) return 0;
    const ZirExpr *call = &proof->terms.exprs[step->term_root];
    char name[ZIR_NAME_MAX];
    if(call->kind != ZIR_EXPR_CALL || !call_name(&proof->terms, call, name, sizeof(name)) ||
       strcmp(name, step->target)) return 0;
    Parameters p;
    if(!parameters(law->claim->args, &p)) return 0;
    int args[PARAMETERS], count = 0;
    for(int at = call->first_child; at >= 0; at = proof->terms.exprs[at].next_sibling) {
        if(!tick(c, depth) || at >= proof->terms.expr_count || count >= p.count || proof->terms.exprs[at].argument_name[0]) return 0;
        int type = scalar_type(c, owner, p.types[count]);
        args[count] = expression(c, m, &proof->terms, at, env, type, depth + 1);
        if(args[count++] < 0) return 0;
    }
    if(count != p.count) return 0;
    int saved_facts = c->kernel.fact_count;
    ProofFact facts[PROOF_FACTS];
    memcpy(facts, c->kernel.facts, sizeof(facts));
    int saved_unfold = c->unfold_count, saved_all = c->unfold_all;
    const ZirFunction *saved_functions[PROOF_DEPTH];
    memcpy(saved_functions, c->unfold, sizeof(saved_functions));
    /* A lemma must check in an empty hypothesis context; caller branches
     * cannot turn an unproved or circular lemma into a theorem. */
    c->kernel.fact_count = 0;
    int proved = theorem(c, owner, law, args, count, depth + 1);
    c->kernel.fact_count = saved_facts;
    memcpy(c->kernel.facts, facts, sizeof(facts));
    c->unfold_count = saved_unfold; c->unfold_all = saved_all;
    memcpy(c->unfold, saved_functions, sizeof(saved_functions));
    if(proved < 0) return 0;
    *proposition = proved;
    return 1;
}
static int theorem(ProofContext *c, const ZirModule *m, const ZirLaw *law,
                   const int *args, int count, int depth)
{
    if(!tick(c, depth)) return -1;
    for(int i = 0; i < c->law_count; i++) if(c->laws[i] == law)
        { c->invalid = 1; return unsupported(c, "circular theorem dependency"); }
    if(c->law_count == PROOF_DEPTH) { c->kernel.exhausted = 1; return -1; }
    if(!law->proof || !law->proof->step_count) return unsupported(c, "theorem has no complete proof certificate");
    c->laws[c->law_count++] = law;
    const ZirProof *proof = law->proof;
    Parameters params;
    Environment *env = calloc(1, sizeof(*env));
    int goal = -1, original = -1, ok = env && parameters(law->claim->args, &params) && count == params.count;
    int cases = 0, closed = 0;
    c->unfold_count = 0; c->unfold_all = 0;
    for(int i = 0; ok && i < count; i++) {
        int type = scalar_type(c, m, params.types[i]);
        ok = args[i] >= 0 && type >= 0 && c->kernel.terms[args[i]].type == type &&
             env_add(c, env, params.names[i], args[i]);
    }
    for(int i = 0; ok && i < proof->step_count; i++) {
        const ZirProofStep *step = &proof->steps[i];
        if(closed) { ok = 0; break; }
        if(step->kind == ZIR_PROOF_UNFOLD) {
            if(goal >= 0) { ok = 0; break; }
            if(!step->target[0]) c->unfold_all = 1;
            else {
                const ZirFunction *fn = NULL; const ZirModule *owner = NULL;
                int found = ResolveFunction(m, step->target, &owner, &fn);
                if(found != 1) found = ResolveFunction(proof->owner, step->target, &owner, &fn);
                if(found != 1 || c->unfold_count == PROOF_DEPTH) { ok = 0; break; }
                c->unfold[c->unfold_count++] = fn;
            }
            continue;
        }
        if(goal < 0) {
            goal = expression(c, m, law->claim, law->claim->stmts[0].expr_root, env, 0, depth + 1);
            original = goal;
            if(goal < 0 || c->kernel.terms[goal].type) { ok = 0; break; }
        }
        if(step->kind == ZIR_PROOF_CASES) cases = 1;
        else if(step->kind == ZIR_PROOF_REWRITE || step->kind == ZIR_PROOF_USE) {
            int lemma = -1;
            if(!application(c, m, proof, step, env, &lemma, depth + 1)) { ok = 0; break; }
            if(step->kind == ZIR_PROOF_USE) {
                ok = ProofEqual(&c->kernel, goal, lemma, 0);
                closed = 1;
            } else {
                ProofTerm equation = c->kernel.terms[lemma];
                if(equation.op != P_EQ) { ok = 0; break; }
                goal = ProofRewrite(&c->kernel, goal, equation.a, equation.b, 0);
                ok = goal >= 0;
            }
        } else if(step->kind == ZIR_PROOF_REFL || step->kind == ZIR_PROOF_RING || step->kind == ZIR_PROOF_ORDER) {
            ok = ProofClose(&c->kernel, goal, step->kind != ZIR_PROOF_REFL,
                            step->kind == ZIR_PROOF_ORDER, cases);
            closed = 1;
        } else { ok = 0; break; }
    }
    free(env);
    c->law_count--;
    if(!closed && !c->unsupported && !c->kernel.exhausted)
        unsupported(c, "proof certificate has no closing step");
    return ok && closed && !c->unsupported && !c->kernel.exhausted ? original : -1;
}

int EvaluateTheorem(const ZirModule *module, const ZirLaw *law, char *detail, size_t size)
{
    if(!law->claim || !law->proof) {
        snprintf(detail, size, "theorem has no proof certificate"); return 2;
    }
    ProofContext *c = calloc(1, sizeof(*c));
    if(!c) { snprintf(detail, size, "out of memory checking theorem"); return 2; }
    ProofKernelInit(&c->kernel);
    Parameters p;
    int args[PARAMETERS], ok = parameters(law->claim->args, &p);
    for(int i = 0; ok && i < p.count; i++) {
        int type = scalar_type(c, module, p.types[i]);
        args[i] = node(c, P_VAR, type, -1, -1, -1, (uint64_t)i);
        ok = args[i] >= 0;
    }
    int result = ok ? theorem(c, module, law, args, p.count, 0) : -1;
    int status;
    if(result >= 0) { snprintf(detail, size, "kernel checked proof over the full typed domain"); status = 0; }
    else if(c->invalid) { snprintf(detail, size, "invalid proof: %s", c->reason); status = 3; }
    else if(c->kernel.exhausted) { snprintf(detail, size, "scalar proof exceeds its deterministic budget"); status = 2; }
    else if(c->unsupported) { snprintf(detail, size, "%s", c->reason); status = 2; }
    else { snprintf(detail, size, "invalid proof: a step did not establish its goal"); status = 3; }
    ProofKernelFree(&c->kernel); free(c);
    return status;
}
