#include "zir_memory.h"
#include "zir_check_internal.h"

const char *
lookup_lexical(Checker *c, const char *name)
{
    for(int i = c->count - 1; i >= 0; i--)
        if(!c->bindings[i].is_using_namespace &&
           !strcmp(c->bindings[i].name, name))
            return c->bindings[i].type;
    return "";
}

const ZirGlobal *
global_binding(Checker *c, const char *name)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    int status = ResolveGlobal(c->module, name, &owner, &global);
    if(status < 0 && !c->failed) {
        error(c, c->current_stmt != NULL ? c->current_stmt->span :
              c->fn != NULL ? c->fn->span : c->module->span,
              "ambiguous global name", name);
        c->failed = 1;
    }
    if(status == 1 && owner != c->module && strchr(name, '.') == NULL) {
        const char *base = global->type;
        if(base[0] == '*') base = skip_ws(base + 1);
        const ZirType *declared = FindType(owner, base, NULL);
        const ZirType *visible = FindType(c->module, base, NULL);
        if(declared != NULL && visible != NULL &&
           declared != visible &&
           import_type_alias(c->module, owner) == NULL && !c->failed) {
            error(c, c->current_stmt != NULL ? c->current_stmt->span :
                  c->fn != NULL ? c->fn->span : c->module->span,
                  "imported global type is shadowed", name);
            c->failed = 1;
            return NULL;
        }
    }
    return status == 1 ? global : NULL;
}

const char *
lookup(Checker *c, const char *name)
{
    const char *lexical = lookup_lexical(c, name);
    if(*lexical)
        return lexical;
    const ZirGlobal *global = global_binding(c, name);
    if(global != NULL) return global->type;
    const ZirType *type = NULL;
    int resolved = resolve_using_enum(c, name, &type, NULL, 0);
    if(resolved < 0) {
        error(c, c->fn->span, "ambiguous enum member", name);
        c->failed = 1;
    }
    if(resolved > 0)
        return type->name;
    return "";
}

/* Owned Vec bindings move on assignment, argument passing, return, VecFree,
 * and BuilderFinish. A moved binding is empty storage; the checker rejects
 * any further use so every value is dropped exactly once. */
int
owned_vec_binding_type(Checker *c, const char *type)
{
    return VecElementType(c->module, type, NULL, 0);
}

Binding *
lexical_vec_binding(Checker *c, int index)
{
    const ZirExpr *e;
    if(index < 0 || index >= c->fn->expr_count)
        return NULL;
    e = &c->fn->exprs[index];
    if(e->kind != ZIR_EXPR_IDENT || e->is_this ||
       !strcmp(e->name, "true") || !strcmp(e->name, "false") ||
       !strcmp(e->name, "null"))
        return NULL;
    for(int i = c->count - 1; i >= 0; i--)
        if(!c->bindings[i].is_using_namespace &&
           !strcmp(c->bindings[i].name, e->name))
            return owned_vec_binding_type(c, c->bindings[i].type) ?
                &c->bindings[i] : NULL;
    return NULL;
}

/* The local binding at the root of a Vec place: a Vec binding, or a
 * local record or pointer reached through field selections. */
Binding *
vec_place_root(Checker *c, int index)
{
    if(index < 0 || index >= c->fn->expr_count)
        return NULL;
    if(c->fn->exprs[index].kind == ZIR_EXPR_IDENT)
        return lexical_vec_binding(c, index);
    while(index >= 0 && (c->fn->exprs[index].kind == ZIR_EXPR_MEMBER ||
                         c->fn->exprs[index].kind == ZIR_EXPR_POINTER_MEMBER))
        index = c->fn->exprs[index].left;
    if(index < 0 || c->fn->exprs[index].kind != ZIR_EXPR_IDENT ||
       c->fn->exprs[index].is_this)
        return NULL;
    for(int i = c->count - 1; i >= 0; i--)
        if(!c->bindings[i].is_using_namespace &&
           !strcmp(c->bindings[i].name, c->fn->exprs[index].name))
            return &c->bindings[i];
    return NULL;
}

/* Aggregate ownership follows the same move discipline as a direct Vec. */
Binding *
lexical_owned_binding(Checker *c, int index)
{
    const ZirExpr *e;
    if(index < 0 || index >= c->fn->expr_count)
        return NULL;
    e = &c->fn->exprs[index];
    if(e->kind != ZIR_EXPR_IDENT || e->is_this ||
       !strcmp(e->name, "true") || !strcmp(e->name, "false") ||
       !strcmp(e->name, "null"))
        return NULL;
    for(int i = c->count - 1; i >= 0; i--)
        if(!c->bindings[i].is_using_namespace &&
           !strcmp(c->bindings[i].name, e->name) &&
           contains_vec(c->module, c->bindings[i].type, 0))
            return &c->bindings[i];
    return NULL;
}

/* Individual ownership moves are tracked by the lexical root and a
 * member-field path. Index and pointer paths stay whole-binding moves until
 * their aliasing can be represented safely. */
static Binding *
owned_member_path(Checker *c, int index, char *path, size_t capacity)
{
    const ZirExpr *e;
    Binding *root;
    size_t length;
    int written;

    if(index < 0 || index >= c->fn->expr_count || path == NULL ||
       capacity == 0)
        return NULL;
    e = &c->fn->exprs[index];
    if(e->kind != ZIR_EXPR_MEMBER)
        return NULL;
    if(e->left < 0 || e->left >= c->fn->expr_count)
        return NULL;

    const ZirExpr *base = &c->fn->exprs[e->left];
    if(base->kind == ZIR_EXPR_IDENT && !base->is_this &&
       strcmp(base->name, "true") && strcmp(base->name, "false") &&
       strcmp(base->name, "null")) {
        for(int i = c->count - 1; i >= 0; i--)
            if(!c->bindings[i].is_using_namespace &&
               !strcmp(c->bindings[i].name, base->name) &&
               contains_vec(c->module, c->bindings[i].type, 0)) {
                copy_text(path, capacity, e->name);
                return &c->bindings[i];
            }
        return NULL;
    }

    root = owned_member_path(c, e->left, path, capacity);
    if(root == NULL)
        return NULL;
    length = strlen(path);
    written = snprintf(path + length, capacity - length, "%s%s",
                       length != 0 ? "." : "", e->name);
    if(written < 0 || (size_t)written >= capacity - length) {
        path[length] = '\0';
        return NULL;
    }
    return root;
}

static int
path_prefix(const char *prefix, const char *value)
{
    size_t length = strlen(prefix);
    if(strncmp(prefix, value, length) != 0)
        return 0;
    return value[length] == '\0' || value[length] == '.';
}

static int
moved_path_conflict(const Binding *binding, const char *path, int owns_storage)
{
    if(binding == NULL || binding->moved)
        return 1;
    for(int i = 0; i < binding->moved_path_count; i++) {
        const char *moved = binding->moved_paths[i];
        if(path_prefix(moved, path) ||
           (owns_storage && path_prefix(path, moved)))
            return 1;
    }
    return 0;
}

void
check_moved_path_use(Checker *c, int index)
{
    const ZirExpr *e;
    char path[ZIR_TEXT_MAX];
    Binding *binding;

    if(index < 0 || index >= c->fn->expr_count)
        return;
    e = &c->fn->exprs[index];
    if(c->assign_destination)
        return;
    if(e->kind != ZIR_EXPR_MEMBER)
        return;
    binding = owned_member_path(c, index, path, sizeof(path));
    if(binding != NULL &&
       moved_path_conflict(binding, path,
                           contains_vec(c->module, e->type, 0)))
        error(c, e->span, "owned record field is used after moving", path);
}

int
mark_moved_member_path(Checker *c, int index)
{
    const ZirExpr *e;
    char path[ZIR_TEXT_MAX];
    Binding *binding;

    if(index < 0 || index >= c->fn->expr_count)
        return 0;
    e = &c->fn->exprs[index];
    if(e->kind != ZIR_EXPR_MEMBER ||
       !contains_vec(c->module, e->type, 0))
        return 0;
    binding = owned_member_path(c, index, path, sizeof(path));
    if(binding == NULL)
        return 0;
    if(moved_path_conflict(binding, path, 1)) {
        error(c, e->span, "owned record field is used after moving", path);
        return 1;
    }
    if(binding->borrow_count > 0) {
        error(c, e->span,
              "cannot move a Vec with a live borrowed view", binding->name);
        return 1;
    }
    if(binding->moved_path_count >=
       (int)(sizeof(binding->moved_paths) /
             sizeof(binding->moved_paths[0]))) {
        error(c, e->span,
              "too many moved fields in one aggregate", binding->name);
        return 1;
    }
    copy_text(binding->moved_paths[binding->moved_path_count++],
              sizeof(binding->moved_paths[0]), path);
    c->fn->exprs[index].is_move = 1;
    return 1;
}

/* A field path of an owned local binding through record fields alone,
 * such as bag.items. Assigning it frees the old value after the new one is
 * complete; a moved field was cleared, so freeing it does nothing. PATH
 * receives the field path below the binding. */
Binding *
local_owned_place(Checker *c, int index, char *path, size_t capacity)
{
    const ZirExpr *e;
    if(index < 0 || index >= c->fn->expr_count)
        return NULL;
    e = &c->fn->exprs[index];
    if(e->kind != ZIR_EXPR_MEMBER || e->type == NULL ||
       !contains_vec(c->module, e->type, 0))
        return NULL;
    for(const ZirExpr *place = e; place->kind == ZIR_EXPR_MEMBER;) {
        if(place->left < 0 || place->left >= c->fn->expr_count)
            return NULL;
        place = &c->fn->exprs[place->left];
        if(place->type == NULL || place->type[0] == '*')
            return NULL;
    }
    return owned_member_path(c, index, path, capacity);
}

/* An assignment to a field path of an owned local owns that path again:
 * the moves recorded at or below it end. A path below a moved field, or a
 * field of a moved binding, stays an error. */
void
reown_member_path(Checker *c, int index)
{
    char path[ZIR_TEXT_MAX];
    Binding *binding = local_owned_place(c, index, path, sizeof(path));
    int kept = 0;
    if(binding == NULL)
        return;
    if(binding->moved) {
        error(c, c->fn->exprs[index].span,
              "Vec binding is used after moving", binding->name);
        return;
    }
    for(int i = 0; i < binding->moved_path_count; i++) {
        const char *moved = binding->moved_paths[i];
        if(path_prefix(path, moved))
            continue;
        if(path_prefix(moved, path)) {
            error(c, c->fn->exprs[index].span,
                  "owned record field is used after moving", moved);
            return;
        }
        if(kept != i)
            copy_text(binding->moved_paths[kept],
                      sizeof(binding->moved_paths[0]), moved);
        kept++;
    }
    binding->moved_path_count = kept;
}

/* A global holding owned storage, or a record literal that would move one.
 * A call argument or VecPush element would empty the global as a side
 * effect, so only a declaration, assignment or return moves a global. */
int
global_vec_source(Checker *c, int index)
{
    const ZirExpr *e;
    const ZirGlobal *global;
    if(index < 0 || index >= c->fn->expr_count)
        return 0;
    e = &c->fn->exprs[index];
    if(e->kind == ZIR_EXPR_COMPOUND) {
        for(int child = e->first_child; child >= 0;
            child = c->fn->exprs[child].next_sibling) {
            const ZirExpr *entry = &c->fn->exprs[child];
            if(entry->kind == ZIR_EXPR_FIELD_INIT &&
               contains_vec(c->module, entry->type, 0) &&
               global_vec_source(c, entry->right))
                return 1;
        }
        return 0;
    }
    if(e->kind == ZIR_EXPR_MEMBER)
        return global_vec_source(c, e->left);
    if(e->kind != ZIR_EXPR_IDENT || *lookup_lexical(c, e->name))
        return 0;
    global = global_binding(c, e->name);
    return global != NULL && contains_vec(c->module, global->type, 0);
}

/* A global, or a field path of one, that holds owned storage. Assigning it
 * frees the old value and takes the new one; a declaration, assignment or
 * return that moves it takes the storage and leaves the global empty. */
int
global_owned_place(Checker *c, int index)
{
    const ZirExpr *e;
    if(index < 0 || index >= c->fn->expr_count)
        return 0;
    e = &c->fn->exprs[index];
    if(e->type == NULL || !contains_vec(c->module, e->type, 0))
        return 0;
    if(e->kind == ZIR_EXPR_MEMBER) {
        const ZirExpr *left = e->left >= 0 ? &c->fn->exprs[e->left] : NULL;
        /* A field reached through a pointer belongs to its pointee. */
        if(left == NULL || left->type == NULL || left->type[0] == '*')
            return 0;
        return global_owned_place(c, e->left);
    }
    return e->kind == ZIR_EXPR_IDENT && !*lookup_lexical(c, e->name) &&
           global_binding(c, e->name) != NULL;
}

static int
owned_shape(Checker *c, int index, int globals)
{
    const ZirExpr *e;
    if(index < 0 || index >= c->fn->expr_count)
        return 0;
    e = &c->fn->exprs[index];
    if(globals && global_owned_place(c, index))
        return 1;
    if(e->kind == ZIR_EXPR_IDENT)
        return !global_vec_source(c, index);
    if(e->kind == ZIR_EXPR_CALL)
        return 1;
    if(e->kind == ZIR_EXPR_MEMBER &&
       contains_vec(c->module, e->type, 0)) {
        char path[ZIR_TEXT_MAX];
        return owned_member_path(c, index, path, sizeof(path)) != NULL;
    }
    if(e->kind != ZIR_EXPR_COMPOUND)
        return 0;
    for(int child = e->first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        const ZirExpr *entry = &c->fn->exprs[child];
        if(entry->kind == ZIR_EXPR_FIELD_INIT &&
           contains_vec(c->module, entry->type, 0) &&
           !owned_shape(c, entry->right, globals))
            return 0;
    }
    return 1;
}

/* A constructing aggregate moves each owned field from a local binding, fresh
 * call result, or another constructing aggregate with the same discipline. */
int
owned_initializer_shape(Checker *c, int index)
{
    return owned_shape(c, index, 0);
}

/* A declaration, assignment or return may also move a global place. */
int
owned_transfer_shape(Checker *c, int index)
{
    return owned_shape(c, index, 1);
}

int
numeric(const char *type)
{
    const char *scalar = ScalarType(type);
    return !strcmp(type, "integer") || !strcmp(type, "real") ||
           (*scalar && strcmp(scalar, "string") &&
            (strchr("suf", scalar[0]) != NULL || !strcmp(scalar, "isize")));
}

int
integer_type(const char *type)
{
    const char *scalar = ScalarType(type);
    return !strcmp(type, "integer") ||
           (scalar[0] != '\0' && strcmp(scalar, "string") != 0 &&
            (scalar[0] == 's' || scalar[0] == 'u' || !strcmp(scalar, "isize")));
}

static int constant_value(const ZirModule *module, const char *name, int depth,
                          int64_t *value, int wide);

/* Folds an integer constant expression. Array bounds stay within s32;
 * a wide fold, used for named constants, spans s64 with checked overflow. */
static int
integer_expression(const ZirModule *module, const ZirFunction *expression,
                   int index, int depth, int64_t *value, int wide)
{
    if(index < 0 || depth > 128)
        return -1;
    const ZirExpr *node = &expression->exprs[index];
    if(node->kind == ZIR_EXPR_IDENT)
        return constant_value(module, node->name, depth + 1, value, wide);
    if(node->kind == ZIR_EXPR_MEMBER && node->left >= 0 &&
       expression->exprs[node->left].kind == ZIR_EXPR_IDENT) {
        /* An imported constant, as in [B.LIMIT]u8. */
        char qualified[ZIR_NAME_MAX];
        int length = snprintf(qualified, sizeof(qualified), "%s.%s",
                              expression->exprs[node->left].name, node->name);
        if(length < 0 || (size_t)length >= sizeof(qualified))
            return -1;
        return constant_value(module, qualified, depth + 1, value, wide);
    }
    if(node->kind == ZIR_EXPR_SIZE_OF) {
        size_t size, alignment;
        if(!layout_type(module, node->name, depth + 1,
                        &size, &alignment) ||
           size > INT32_MAX)
            return -1;
        *value = (int64_t)size;
        return 1;
    }
    if(node->kind == ZIR_EXPR_INT) {
        unsigned long long bits;
        if(!ZirIntegerLiteral(node->text, &bits) || bits > INT64_MAX ||
           (!wide && bits > INT32_MAX))
            return -1;
        *value = (int64_t)bits;
        return 1;
    }
    int64_t left = 0, right = 0;
    if(node->kind == ZIR_EXPR_UNARY) {
        int status = integer_expression(module, expression, node->right, depth + 1, &right, wide);
        if(status != 1)
            return status;
        if(!strcmp(node->op, "+"))
            *value = right;
        else if(!strcmp(node->op, "-") && right != INT64_MIN)
            *value = -right;
        else
            return -1;
    } else if(node->kind == ZIR_EXPR_BINARY) {
        int status = integer_expression(module, expression, node->left, depth + 1, &left, wide);
        if(status != 1)
            return status;
        status = integer_expression(module, expression, node->right, depth + 1, &right, wide);
        if(status != 1)
            return status;
        if((left == INT32_MIN || left == INT64_MIN) && right == -1 &&
           (!strcmp(node->op, "/") || !strcmp(node->op, "%")))
            return -1;
        if(!strcmp(node->op, "+")) {
            if(__builtin_add_overflow(left, right, value)) return -1;
        } else if(!strcmp(node->op, "-")) {
            if(__builtin_sub_overflow(left, right, value)) return -1;
        } else if(!strcmp(node->op, "*")) {
            if(__builtin_mul_overflow(left, right, value)) return -1;
        }
        else if(!strcmp(node->op, "/") && right != 0)
            *value = left / right;
        else if(!strcmp(node->op, "%") && right != 0)
            *value = left % right;
        else
            return -1;
    } else {
        return -1;
    }
    if(wide)
        return 1;
    return *value >= INT32_MIN && *value <= INT32_MAX ? 1 : -1;
}

int
bound_expression(const ZirModule *module, const ZirFunction *expression, int index,
                 int depth, int64_t *value)
{
    return integer_expression(module, expression, index, depth, value, 0);
}

static int
constant_value(const ZirModule *module, const char *name, int depth,
               int64_t *value, int wide)
{
    if(depth > 128)
        return -1;
    const ZirDefine *definition = NULL;
    const ZirModule *owner = NULL;
    int found = ResolveDefine(module, name, &definition, &owner, NULL);
    if(found != 1)
        return found;
    ZirFunction expression = {0};
    int index = ParseExpr(&expression, owner, definition->value, definition->span);
    int status = integer_expression(owner, &expression, index, depth + 1, value, wide);
    free(expression.exprs);
    return status;
}

int
bound_constant(const ZirModule *module, const char *name, int depth, int64_t *value)
{
    return constant_value(module, name, depth, value, 0);
}

/* A named integer constant at its full s64 value. */
int
integer_constant(const ZirModule *module, const char *name, int64_t *value)
{
    return constant_value(module, name, 0, value, 1);
}

/* Resolve a literal string definition into the expression graph. The source
 * macro spelling must not become a backend dependency in saved .zir or .zx. */
int
bound_string_constant(const ZirModule *module, const char *name, int depth,
                      const char **literal)
{
    if(depth > 128)
        return -1;
    const ZirDefine *definition = NULL;
    const ZirModule *owner = NULL;
    int found = ResolveDefine(module, name, &definition, &owner, NULL);
    if(found != 1)
        return found;
    ZirFunction expression = {0};
    int index = ParseExpr(&expression, owner, definition->value, definition->span);
    int status = 0;
    if(index < 0)
        status = -1;
    else if(expression.exprs[index].kind == ZIR_EXPR_STRING) {
        /* Kept text has no length cap: a raw #string table may exceed a line. */
        *literal = KeepText(expression.exprs[index].text);
        status = 1;
    } else if(expression.exprs[index].kind == ZIR_EXPR_IDENT) {
        status = bound_string_constant(owner, expression.exprs[index].name,
                                       depth + 1, literal);
    }
    free(expression.exprs);
    return status;
}

/* A real definition's value: a literal, another real constant, or either
 * with a sign, as in `Side :: -90.0` or `Again :: -Side`. A negated
 * negative literal loses its sign rather than gaining a second one. */
static int
real_constant_node(const ZirModule *owner, const ZirFunction *expression,
                   int index, int depth, char *literal, size_t size)
{
    if(depth > 128) return -1;
    const ZirExpr *node = &expression->exprs[index];
    if(node->kind == ZIR_EXPR_FLOAT) {
        copy_text(literal, size, node->text);
        return 1;
    }
    if(node->kind == ZIR_EXPR_IDENT)
        return bound_real_constant(owner, node->name, depth + 1, literal, size);
    if(node->kind != ZIR_EXPR_UNARY || node->right < 0 ||
       (strcmp(node->op, "-") && strcmp(node->op, "+")))
        return 0;
    char operand[128];
    int status = real_constant_node(owner, expression, node->right, depth + 1,
                                    operand, sizeof(operand));
    if(status != 1)
        return status;
    if(!strcmp(node->op, "+"))
        copy_text(literal, size, operand);
    else if(operand[0] == '-')
        copy_text(literal, size, operand + 1);
    else if(snprintf(literal, size, "-%s", operand) >= (int)size)
        return -1;
    return 1;
}

/* Fold real definitions into typed expression nodes before writing .zir.
 * Backends then see the same literal from source and saved IR. */
int
bound_real_constant(const ZirModule *module, const char *name, int depth,
                    char *literal, size_t size)
{
    if(depth > 128) return -1;
    const ZirDefine *definition = NULL;
    const ZirModule *owner = NULL;
    int found = ResolveDefine(module, name, &definition, &owner, NULL);
    if(found != 1)
        return found;
    ZirFunction expression = {0};
    int index = ParseExpr(&expression, owner, definition->value,
                          definition->span);
    int status = index < 0 ? -1 :
        real_constant_node(owner, &expression, index, depth, literal, size);
    free(expression.exprs);
    return status;
}

/* Aggregate definitions become checked expression nodes at their use site.
 * Limit this path to literal graphs; the compile-time evaluator has already
 * reduced #run results to this form before function checking starts. */
static int
literal_compound_graph(const ZirFunction *expression)
{
    for(int i = 0; i < expression->expr_count; i++) {
        const ZirExpr *node = &expression->exprs[i];
        switch(node->kind) {
        case ZIR_EXPR_COMPOUND:
        case ZIR_EXPR_FIELD_INIT:
        case ZIR_EXPR_INT:
        case ZIR_EXPR_FLOAT:
        case ZIR_EXPR_STRING:
            break;
        case ZIR_EXPR_UNARY:
            if(strcmp(node->op, "+") && strcmp(node->op, "-")) return 0;
            break;
        case ZIR_EXPR_IDENT:
            if(strcmp(node->name, "true") &&
               strcmp(node->name, "false")) return 0;
            break;
        default:
            return 0;
        }
    }
    return 1;
}

static int
compound_type_at_use(const ZirModule *module,
                     const CompoundConstant *compound, const char *source,
                     char *target, size_t size)
{
    char element[ZIR_NAME_MAX];
    /* A pointer or slice names its target the same way at the use site:
     * *Box from the declaring module is *Lib.Box to a named importer. */
    if(source[0] == '*' || (source[0] == '[' && source[1] == ']')) {
        size_t prefix = source[0] == '*' ? 0 : 2;
        while(source[prefix] == '*')
            prefix++;
        char qualified[ZIR_NAME_MAX];
        if(!compound_type_at_use(module, compound, skip_ws(source + prefix),
                                 qualified, sizeof(qualified))) return 0;
        int written = snprintf(target, size, "%.*s%s", (int)prefix, source,
                               qualified);
        return written >= 0 && (size_t)written < size;
    }
    if(ArrayElementType(source, element, sizeof(element), NULL)) {
        const char *close = strchr(source, ']');
        char qualified[ZIR_NAME_MAX];
        if(close == NULL ||
           !compound_type_at_use(module, compound, element, qualified,
                                 sizeof(qualified))) return 0;
        int written = snprintf(target, size, "%.*s%s",
                               (int)(close - source + 1), source, qualified);
        return written >= 0 && (size_t)written < size;
    }
    const ZirModule *declaring = NULL;
    const ZirType *declared = FindType(compound->owner, source, &declaring);
    if(declared == NULL) {
        copy_text(target, size, source);
        return strlen(source) < size;
    }
    if(compound->owner == module) {
        copy_text(target, size, source);
        return strlen(source) < size;
    }
    const ZirModule *visible_owner = NULL;
    const ZirType *visible = FindType(module, source, &visible_owner);
    /* Reexported fields can name another module's instance of the same
     * generic application. Preserve nominal template/argument identity. */
    if(visible == declared ||
       same_type_application(visible_owner, visible, declaring, declared)) {
        copy_text(target, size, source);
        return strlen(source) < size;
    }
    if(declaring == compound->owner && compound->qualifier[0]) {
        int written = snprintf(target, size, "%s.%s",
                               compound->qualifier, source);
        if(written >= 0 && (size_t)written < size &&
           FindType(module, target, NULL) == declared)
            return 1;
    }
    /* The user reaches the type only through another module, for example
     * a field of an imported record whose type module it does not import.
     * Name it by its declaring module. */
    if(visible == NULL && declaring != NULL && declared->is_public &&
       !declared->is_file_private) {
        int written = snprintf(target, size, "%s.%s", declaring->name,
                               declared->name);
        if(written >= 0 && (size_t)written < size &&
           FindType(module, target, NULL) == declared)
            return 1;
    }
    if(visible != declared)
        return 0;
    copy_text(target, size, source);
    return strlen(source) < size;
}

int
record_field_type_at_use(const ZirModule *module,
                         const ZirModule *record_owner,
                         const char *record_name, char *field_type,
                         size_t size)
{
    CompoundConstant context = {0};
    context.owner = record_owner;
    const char *dot = strchr(record_name, '.');
    if(dot != NULL && (size_t)(dot - record_name) <
                       sizeof(context.qualifier)) {
        memcpy(context.qualifier, record_name,
               (size_t)(dot - record_name));
        context.qualifier[dot - record_name] = '\0';
    } else if(record_owner != module) {
        const char *alias = import_type_alias(module, record_owner);
        if(alias != NULL)
            copy_text(context.qualifier, sizeof(context.qualifier), alias);
    }
    char qualified[ZIR_NAME_MAX];
    if(!compound_type_at_use(module, &context, field_type, qualified,
                             sizeof(qualified)) ||
       strlen(qualified) >= size) return 0;
    copy_text(field_type, size, qualified);
    return 1;
}

/* Qualify the template's fixed type names before inserting caller-owned
 * bindings. Qualifying afterwards could reinterpret T=Pair as the library's
 * different Pair. Tokens in generic applications need the same treatment. */
int
template_type_at_use(const ZirModule *module, const ZirModule *owner,
                     const ZirFunction *fn, const char *callee,
                     char *type, size_t size)
{
    if(module == owner || !*type) return 1;
    /* type_of contains a value expression, not type-name tokens. Its locals
     * are resolved against the instantiated parameters by the checker. */
    char operand[ZIR_NAME_MAX];
    if(TypeOfOperand(type, operand, sizeof(operand))) return 1;
    char *mapped = AllocateOrExit(size);
    size_t used = 0;
    for(const char *at = type; *at;) {
        const char *start = at;
        if(isalpha((unsigned char)*at) || *at == '_') {
            do { at++; } while(is_ident_char(*at) ||
                (*at == '.' && (isalpha((unsigned char)at[1]) || at[1] == '_')));
        } else at++;
        size_t length = (size_t)(at - start);
        char word[ZIR_NAME_MAX];
        const char *piece = start;
        size_t count = length;
        if((isalpha((unsigned char)*start) || *start == '_') &&
           TemplateParameterIndex(fn->template_param, start, length) < 0) {
            if(length >= sizeof(word)) { free(mapped); return 0; }
            memcpy(word, start, length); word[length] = 0;
            if(!record_field_type_at_use(module, owner, callee, word, sizeof(word))) {
                free(mapped); return 0;
            }
            piece = word; count = strlen(word);
        }
        if(used + count >= size) { free(mapped); return 0; }
        memcpy(mapped + used, piece, count); used += count;
    }
    mapped[used] = 0;
    memcpy(type, mapped, used + 1);
    free(mapped);
    return 1;
}

int
template_parameters_at_use(const ZirModule *module, const ZirModule *owner,
                           const ZirFunction *fn, const char *callee,
                           const char **args)
{
    if(module == owner) return 1;
    const ZirParameters *parameters = ParametersOf(*args);
    char *mapped = AllocateOrExit(ZIR_TEXT_MAX);
    size_t used = 0;
    for(int i = 0; i < parameters->count; i++) {
        char type[ZIR_NAME_MAX];
        if(parameters->items[i].name == NULL) { free(mapped); return 0; }
        copy_text(type, sizeof(type), parameters->items[i].type);
        if(!template_type_at_use(module, owner, fn, callee, type, sizeof(type))) {
            free(mapped); return 0;
        }
        int wrote = snprintf(mapped + used, ZIR_TEXT_MAX - used, "%s%s: %s",
            i ? ", " : "", parameters->items[i].name, type);
        if(wrote < 0 || (size_t)wrote >= ZIR_TEXT_MAX - used) { free(mapped); return 0; }
        used += (size_t)wrote;
    }
    mapped[used] = 0;
    *args = KeepText(mapped);
    free(mapped);
    return 1;
}

int
MapTypePartsAtUse(const ZirModule *module, const char *name,
                   char *key, size_t key_size, char *value, size_t value_size)
{
    const ZirModule *owner = NULL;
    FindType(module, name, &owner);
    return MapTypeParts(module, name, key, key_size, value, value_size) &&
        record_field_type_at_use(module, owner, name, key, key_size) &&
        record_field_type_at_use(module, owner, name, value, value_size);
}

typedef struct CompoundText { char *text; size_t used, capacity; } CompoundText;

static void
append_compound_text(CompoundText *out, const char *part)
{
    size_t length = strlen(part);
    if(out->used + length + 1 > out->capacity) {
        size_t capacity = out->capacity ? out->capacity : 256;
        while(out->used + length + 1 > capacity) capacity *= 2;
        char *grown = AllocateOrExit(capacity);
        if(out->used) memcpy(grown, out->text, out->used + 1);
        free(out->text);
        out->text = grown;
        out->capacity = capacity;
    }
    memcpy(out->text + out->used, part, length + 1);
    out->used += length;
}

static int
render_compound_at_use(const ZirFunction *expression, int index,
                       CompoundText *out, int depth)
{
    if(index < 0 || index >= expression->expr_count || depth > 64)
        return 0;
    const ZirExpr *node = &expression->exprs[index];
    if(node->kind != ZIR_EXPR_COMPOUND) {
        append_compound_text(out, node->text);
        return 1;
    }
    int array = node->name[0] == '[';
    /* A row of numbers keeps its type, `s32.[...]`, since a row written
     * `.[...]` has no type to take. */
    char element[ZIR_NAME_MAX], inner[ZIR_NAME_MAX];
    if(depth > 0 && array &&
       ArrayElementType(node->name, element, sizeof(element), NULL)) {
        char next[ZIR_NAME_MAX];
        copy_text(inner, sizeof(inner), element);
        while(ArrayElementType(inner, next, sizeof(next), NULL))
            copy_text(inner, sizeof(inner), next);
        if(ScalarByteWidth(inner) > 0)
            append_compound_text(out, element);
    }
    append_compound_text(out, array ? ".[" : ".{");
    int ordinal = 0;
    for(int child = node->first_child; child >= 0;
        child = expression->exprs[child].next_sibling) {
        const ZirExpr *entry = &expression->exprs[child];
        if(ordinal++) append_compound_text(out, ", ");
        if(!array && !strcmp(entry->op, "=")) {
            append_compound_text(out, ".");
            append_compound_text(out, entry->name);
            append_compound_text(out, " = ");
        }
        if(!render_compound_at_use(expression, entry->right, out, depth + 1))
            return 0;
    }
    append_compound_text(out, array ? "]" : "}");
    return 1;
}

const char *
lower_compound_global(const ZirModule *module, const ZirGlobal *global,
                      const CompoundConstant *compound)
{
    ZirFunction expression = {0};
    int root = ParseExpr(&expression, compound->owner, compound->literal,
                         global->span);
    const char *result = NULL;
    if(root >= 0 && expression.exprs[root].kind == ZIR_EXPR_COMPOUND &&
       literal_compound_graph(&expression)) {
        char actual[ZIR_NAME_MAX];
        CompoundText text = {0};
        if(compound_type_at_use(module, compound,
                expression.exprs[root].name, actual, sizeof(actual)) &&
           !strcmp(actual, global->type) &&
           render_compound_at_use(&expression, root, &text, 0))
            result = KeepText(text.text);
        free(text.text);
    }
    free(expression.exprs);
    return result;
}

void
compound_qualifier_for_global(CompoundConstant *compound,
                              const char *expected_type)
{
    char current[ZIR_NAME_MAX], element[ZIR_NAME_MAX];
    copy_text(current, sizeof(current), expected_type);
    while(ArrayElementType(current, element, sizeof(element), NULL))
        copy_text(current, sizeof(current), element);
    const char *dot = strchr(current, '.');
    if(dot == NULL || (size_t)(dot - current) >=
                      sizeof(compound->qualifier)) return;
    memcpy(compound->qualifier, current,
           (size_t)(dot - current));
    compound->qualifier[dot - current] = '\0';
}

int
bound_compound_constant(const ZirModule *module, const char *name, int depth,
                        CompoundConstant *result)
{
    if(depth > 128) return -1;
    const ZirDefine *definition = NULL;
    const ZirModule *owner = NULL;
    const ZirImport *selected_import = NULL;
    int found = ResolveDefine(module, name, &definition, &owner,
                              &selected_import);
    if(found != 1) return found;
    ZirFunction expression = {0};
    int root = ParseExpr(&expression, owner, definition->value,
                         definition->span);
    int status = 0;
    if(root < 0) status = -1;
    else if(expression.exprs[root].kind == ZIR_EXPR_COMPOUND &&
            literal_compound_graph(&expression)) {
        {
            result->literal = definition->value;
            result->owner = owner;
            result->qualifier[0] = '\0';
            if(selected_import != NULL &&
               selected_import->kind == ZIR_IMPORT_MODULE)
                copy_text(result->qualifier,
                          sizeof(result->qualifier),
                          selected_import->name);
            else if(selected_import != NULL &&
                    selected_import->kind == ZIR_IMPORT_OPEN) {
                const char *alias = import_type_alias(module, owner);
                if(alias != NULL)
                    copy_text(result->qualifier,
                              sizeof(result->qualifier), alias);
            }
            status = 1;
        }
    } else if(expression.exprs[root].kind == ZIR_EXPR_IDENT) {
        status = bound_compound_constant(owner,
                    expression.exprs[root].name, depth + 1,
                    result);
        if(status == 1 && owner != module) {
            if(result->owner != owner) status = 0;
            else if(selected_import != NULL &&
                    selected_import->kind == ZIR_IMPORT_MODULE)
                copy_text(result->qualifier,
                          sizeof(result->qualifier),
                          selected_import->name);
            else if(selected_import != NULL &&
                    selected_import->kind == ZIR_IMPORT_OPEN) {
                const char *alias = import_type_alias(module, owner);
                if(alias != NULL)
                    copy_text(result->qualifier,
                              sizeof(result->qualifier), alias);
            }
        }
    }
    free(expression.exprs);
    return status;
}

int
array_capacity(const ZirModule *module, const char *type, int *capacity)
{
    if(!ArrayElementType(type, NULL, 0, capacity))
        return -1;
    if(*capacity >= 0)
        return 1;
    char expression_text[ZIR_NAME_MAX];
    size_t length = (size_t)(strchr(type, ']') - type - 1);
    if(length >= sizeof(expression_text))
        return -1;
    memcpy(expression_text, type + 1, length);
    expression_text[length] = '\0';
    ZirFunction expression = {0};
    int root = ParseExpr(&expression, module, expression_text,
                         (ZirSourceSpan){0});
    int64_t value = 0;
    int status = bound_expression(module, &expression, root, 0, &value);
    free(expression.exprs);
    if(status != 1)
        return status;
    if(value < 0 || value > 1048576)
        return -1;
    *capacity = (int)value;
    return 1;
}

static void
normalize_array_at(const ZirModule *module, char *type, size_t size, int depth,
                   const char *parameters)
{
    char element[ZIR_NAME_MAX];
    int capacity;
    if(depth >= 64 || (parameters != NULL &&
       TemplateParameterIndex(parameters, type, strlen(type)) >= 0)) return;
    if(!ArrayElementType(type, element, sizeof(element), NULL)) {
        for(int t = 0; t < module->type_count; t++)
            if(!strcmp(module->types[t].name, type) &&
               in_lookup_file(module, module->types[t].is_file_private,
                              module->types[t].span))
                return;
        /* Resolve local and imported aliases in the declaring module so
         * symbolic bounds retain their original constant namespace. */
        char resolved[ZIR_NAME_MAX];
        if(strlen(type) >= sizeof(resolved)) return;
        copy_text(resolved, sizeof(resolved), type);
        const ZirModule *owner = module;
        for(int alias_depth = 0; alias_depth < 8; alias_depth++) {
            const ZirDefine *found = NULL;
            const ZirModule *next_owner = NULL;
            if(ResolveDefine(owner, resolved, &found, &next_owner,
                             NULL) != 1)
                return;
            const char *value = skip_ws(found->value);
            if(strlen(value) >= sizeof(resolved)) return;
            copy_text(resolved, sizeof(resolved), value);
            owner = next_owner;
            if(resolved[0] == '[')
                break;
        }
        if(resolved[0] != '[' || strlen(resolved) >= size)
            return;
        normalize_array_at(owner, resolved, sizeof(resolved), depth + 1, parameters);
        copy_text(type, size, resolved);
        if(!ArrayElementType(type, element, sizeof(element), NULL))
            return;
        module = owner;
    }
    normalize_array_at(module, element, sizeof(element), depth + 1, parameters);
    char normalized[ZIR_NAME_MAX];
    int length;
    if(array_capacity(module, type, &capacity) == 1)
        length = snprintf(normalized, sizeof(normalized), "[%d]%s", capacity,
                          element);
    else {
        const char *close = strchr(type, ']');
        length = snprintf(normalized, sizeof(normalized), "%.*s%s",
                          (int)(close - type + 1), type, element);
    }
    if(length >= 0 && (size_t)length < sizeof(normalized))
        copy_text(type, size, normalized);
}

void
normalize_array(const ZirModule *module, char *type, size_t size)
{
    normalize_array_at(module, type, size, 0, NULL);
}

void
normalize_template_array(const ZirModule *module, char *type, size_t size,
                         const char *parameters)
{
    normalize_array_at(module, type, size, 0, parameters);
}

/* TYPE normalized as normalize_array would, kept like a node's type. */
const char *
normalized_array(const ZirModule *module, const char *type)
{
    char normal[ZIR_NAME_MAX];
    copy_text(normal, sizeof(normal), type);
    normalize_array(module, normal, sizeof(normal));
    return KeepName(normal);
}

static int
pointer_type(const char *type)
{
    return type[0] != '[' && strchr(type, '*') != NULL;
}

/* Bits of a fixed-width integer type; isize and usize count as at least 32
 * bits when widened into and at most 64 bits when widened from. */
static int
integer_bits(const char *type, int *is_signed, int as_target)
{
    static const struct { const char *name; int bits, is_signed; } widths[] = {
        {"s8", 8, 1}, {"s16", 16, 1}, {"s32", 32, 1}, {"s64", 64, 1},
        {"u8", 8, 0}, {"u16", 16, 0}, {"u32", 32, 0}, {"u64", 64, 0},
        {NULL, 0, 0}
    };
    for(int i = 0; widths[i].name != NULL; i++)
        if(!strcmp(type, widths[i].name)) {
            *is_signed = widths[i].is_signed;
            return widths[i].bits;
        }
    if(!strcmp(type, "isize") || !strcmp(type, "usize")) {
        *is_signed = type[0] == 'i';
        return as_target ? 32 : 64;
    }
    return 0;
}

const char *
mismatch_detail(char *out, size_t size, const char *subject,
                const char *expected, const char *found)
{
    const char *shown = !strcmp(found, "integer") ? "an integer literal" :
                        !strcmp(found, "real") ? "a float literal" :
                        !strcmp(found, "null") ? "null" : found;
    snprintf(out, size, "%s (expected %s, found %s)", subject, expected, shown);
    return out;
}

int
widens_losslessly(const char *to, const char *from)
{
    const char *scalar = ScalarType(to);
    if(*scalar) to = scalar;
    scalar = ScalarType(from);
    if(*scalar) from = scalar;
    if(!strcmp(to, "float64") && !strcmp(from, "float32"))
        return 1;
    int to_signed, from_signed;
    int to_bits = integer_bits(to, &to_signed, 1);
    int from_bits = integer_bits(from, &from_signed, 0);
    if(to_bits == 0 || from_bits == 0 || from_bits >= to_bits)
        return 0;
    return to_signed || !from_signed;
}

int
compatible(const char *to, const char *from)
{
    const char *canonical = ScalarType(to);
    if(*canonical) to = canonical;
    if(!*to || !*from) return 1;
    if(!strcmp(to, "null") && !strcmp(from, "null")) return 0;
    if(!strcmp(to, from)) return 1;
    if(!strcmp(from, "null")) return pointer_type(to);
    if(SliceElementType(to, NULL, 0) || SliceElementType(from, NULL, 0)) {
        char a[ZIR_NAME_MAX], b[ZIR_NAME_MAX];
        if(!SliceElementType(to, a, sizeof(a)) || !SliceElementType(from, b, sizeof(b)))
            return 0;
        const char *ca = ScalarType(a), *cb = ScalarType(b);
        return !strcmp(*ca ? ca : a, *cb ? cb : b);
    }
    if(to[0] == '[' || from[0] == '[') {
        char to_element[ZIR_NAME_MAX], from_element[ZIR_NAME_MAX];
        int to_capacity, from_capacity;
        if(!ArrayElementType(to, to_element, sizeof(to_element), &to_capacity) ||
           !ArrayElementType(from, from_element, sizeof(from_element), &from_capacity))
            return 0;
        if(to_capacity != from_capacity)
            return 0;
        if(to_capacity < 0) {
            size_t length = (size_t)(strchr(to, ']') - to);
            if(length != (size_t)(strchr(from, ']') - from) || strncmp(to, from, length))
                return 0;
        }
        const char *to_scalar = ScalarType(to_element);
        const char *from_scalar = ScalarType(from_element);
        return !strcmp(*to_scalar ? to_scalar : to_element,
                       *from_scalar ? from_scalar : from_element);
    }
    if(!strcmp(from, "integer") && numeric(to)) return 1;
    if(!strcmp(from, "real") && (to[0] == 'f')) return 1;
    return 0;
}

const ZirType *
flags_type(Checker *c, const char *name)
{
    const ZirType *type = FindType(c->module, name, NULL);
    return type != NULL && type->is_enum_flags ? type : NULL;
}

int
same_declared_type(const ZirModule *module, const char *to,
                   const char *from, int depth)
{
    if(depth > 16) return 0;
    const ZirModule *target_owner = NULL, *source_owner = NULL;
    const ZirType *target = FindType(module, to, &target_owner);
    const ZirType *source = FindType(module, from, &source_owner);
    if(target != NULL && target == source) return 1;
    if(target != NULL && source != NULL && target->foreign_target[0] &&
       !strcmp(target->foreign_target, source->foreign_target))
        return 1;
    if(SameMapType(target_owner ? target_owner : module, target,
                  source_owner ? source_owner : module, source))
        return 1;
    if(target != NULL && source != NULL &&
       same_type_application(target_owner ? target_owner : module, target,
                             source_owner ? source_owner : module, source))
        return 1;
    char to_element[ZIR_NAME_MAX], from_element[ZIR_NAME_MAX];
    int to_capacity, from_capacity;
    if(ArrayElementType(to, to_element, sizeof(to_element), &to_capacity) &&
       ArrayElementType(from, from_element, sizeof(from_element),
                        &from_capacity) &&
       to_capacity >= 0 && to_capacity == from_capacity)
        return same_declared_type(module, to_element, from_element,
                                  depth + 1);
    if(SliceElementType(to, to_element, sizeof(to_element)) &&
       SliceElementType(from, from_element, sizeof(from_element)))
        return same_declared_type(module, to_element, from_element,
                                  depth + 1);
    if(*to == '*' && *from == '*')
        return same_declared_type(module, skip_ws(to + 1),
                                  skip_ws(from + 1), depth + 1);
    return 0;
}

int
compatible_checked(Checker *c, const char *to, const char *from)
{
    const ZirType *target = FindType(c->module, to, NULL);
    if(target != NULL && !strcmp(target->foreign_target, "go:builtin.error") && !strcmp(from, "null"))
        return 1;
    if(target != NULL && target->is_map && !strcmp(from, "null"))
        return 1;
    if(target != NULL && !strcmp(target->foreign_target, "go:builtin.any"))
        return strcmp(from, "void") != 0 && !contains_vec(c->module, from, 0);
    if(flags_type(c, to) != NULL && integer_type(from)) return 1;
    return compatible(to, from) ||
           same_declared_type(c->module, to, from, 0);
}

int
text_type(const char *type)
{
    return !strcmp(type, "string");
}

int
assignable(Checker *c, int index)
{
    const ZirExpr *e;
    if(index < 0 || index >= c->fn->expr_count) return 0;
    e = &c->fn->exprs[index];
    return (e->kind == ZIR_EXPR_IDENT && !e->is_this &&
            strcmp(e->name, "true") &&
            strcmp(e->name, "false") && strcmp(e->name, "null")) ||
           e->kind == ZIR_EXPR_INDEX || e->kind == ZIR_EXPR_MEMBER ||
           e->kind == ZIR_EXPR_POINTER_MEMBER || (e->kind == ZIR_EXPR_UNARY && !strcmp(e->op, "*"));
}

/* Collection counts and borrowed string bytes are read-only views. */
int
readonly_text_destination(Checker *c, int index)
{
    const ZirFunction *fn = c->fn;
    const ZirExpr *e;
    if(index < 0 || index >= fn->expr_count)
        return 0;
    e = &fn->exprs[index];
    if(e->kind == ZIR_EXPR_MEMBER &&
       (!strcmp(e->name, "count") || !strcmp(e->name, "capacity")) &&
       VecElementType(c->module, fn->exprs[e->left].type, NULL, 0))
        return 1;
    if(e->kind == ZIR_EXPR_MEMBER && !strcmp(e->name, "count")) {
        const char *base = fn->exprs[e->left].type;
        if(!strcmp(base, "string") ||
           SliceElementType(base, NULL, 0) ||
           ArrayElementType(base, NULL, 0, NULL)) return 1;
    }
    if(e->kind == ZIR_EXPR_INDEX || e->kind == ZIR_EXPR_MEMBER) {
        if(fn->exprs[e->left].kind == ZIR_EXPR_IDENT &&
           !strcmp(fn->exprs[e->left].type, "string"))
            return 1;
        return readonly_text_destination(c, e->left);
    }
    return 0;
}

/* Compound parameters keep the declaration scope of each callback. */
int
callback_type_equal(const ZirModule *left_module, const char *left,
                    const ZirModule *right_module, const char *right, int depth)
{
    if(depth > 16) return 0;
    if(*left == '*' && *right == '*')
        return callback_type_equal(left_module, skip_ws(left + 1),
                                   right_module, skip_ws(right + 1), depth + 1);
    char left_element[ZIR_NAME_MAX], right_element[ZIR_NAME_MAX];
    if(SliceElementType(left, left_element, sizeof(left_element)) &&
       SliceElementType(right, right_element, sizeof(right_element)))
        return callback_type_equal(left_module, left_element,
                                   right_module, right_element, depth + 1);
    int left_count, right_count;
    if(ArrayElementType(left, left_element, sizeof(left_element), &left_count) &&
       ArrayElementType(right, right_element, sizeof(right_element), &right_count))
        return left_count == right_count &&
               callback_type_equal(left_module, left_element,
                                   right_module, right_element, depth + 1);
    const ZirType *left_type = FindType(left_module, left, NULL);
    const ZirType *right_type = FindType(right_module, right, NULL);
    if(left_type || right_type)
        return left_type && right_type &&
               (left_type == right_type ||
                (left_type->foreign_target[0] &&
                 !strcmp(left_type->foreign_target, right_type->foreign_target)));
    return !strcmp(left, right);
}

/* Function values require a slot context; ordinary names retain lexical lookup.
 * Annotate before recursively checking expressions so a declaration identifier
 * is not mistaken for an unresolved variable. */
void
contextual_slot(Checker *c, int index, const char *expected)
{
    const ZirModule *slot_owner = NULL;
    const ZirType *slot = FindType(c->module, expected, &slot_owner);
    if(index < 0 || slot == NULL || !slot->is_procedure_type)
        return;
    ZirExpr *value = &c->fn->exprs[index];
    if(value->kind == ZIR_EXPR_CONDITIONAL) {
        contextual_slot(c, value->right, expected);
        contextual_slot(c, value->third, expected);
        return;
    }
    if(value->kind != ZIR_EXPR_IDENT ||
       (!value->is_this && *lookup(c, value->name)))
        return;
    const ZirModule *owner = NULL;
    const ZirFunction *declaration = NULL;
    int resolved = ResolveFunction(c->module, value->name, &owner, &declaration);
    int matches = resolved == 1 && !declaration->is_extern;
    if(matches) {
        const char *actual_scalar = ScalarType(declaration->return_type);
        const char *wanted_scalar = ScalarType(slot->procedure_return_type);
        if(*actual_scalar || *wanted_scalar) {
            matches = !strcmp(actual_scalar, wanted_scalar);
        } else {
            matches = callback_type_equal(owner, declaration->return_type,
                                          slot_owner, slot->procedure_return_type, 0);
        }
    }
    const ZirParameters *actual = ParametersOf(matches ? FunctionArgs(declaration) : "");
    const ZirParameters *wanted = ParametersOf(slot->body);
    matches &= actual->count == wanted->count;
    for(int i = 0; matches && i < wanted->count; i++) {
        const char *actual_type = actual->items[i].type;
        const char *wanted_type = wanted->items[i].type;
        if(actual_type == NULL || wanted_type == NULL) {
            matches = 0;
            break;
        }
        const char *actual_scalar = ScalarType(actual_type);
        const char *wanted_scalar = ScalarType(wanted_type);
        if(*actual_scalar || *wanted_scalar) {
            matches = !strcmp(actual_scalar, wanted_scalar);
        } else {
            matches = callback_type_equal(owner, actual_type,
                                          slot_owner, wanted_type, 0);
        }
    }
    if(!matches) {
        Diagnostic(value->span, "check.slot_signature",
                      "function does not match slot signature %s: %s", expected, value->name);
        c->errors++;
        c->failed = 1;
        return;
    }
    value->is_function_value = 1;
    value->type = KeepName(expected);
}


int
TypeOfOperand(const char *source, char *operand, size_t capacity)
{
    const char *open, *close = NULL;
    int depth = 0, quote = 0;
    source = skip_ws(source);
    if(strncmp(source, "type_of", 7) != 0 ||
       (isalnum((unsigned char)source[7]) || source[7] == '_'))
        return 0;
    open = skip_ws(source + 7);
    if(*open != '(') return 0;
    for(const char *p = open; *p; p++) {
        if(quote) {
            if(*p == '\\' && p[1]) p++;
            else if(*p == quote) quote = 0;
        } else if(*p == '"' || *p == '\'') {
            quote = *p;
        } else if(*p == '(') {
            depth++;
        } else if(*p == ')' && --depth == 0) {
            close = p;
            break;
        }
    }
    if(close == NULL || *skip_ws(close + 1) != '\0' ||
       (size_t)(close - open - 1) >= capacity)
        return 0;
    snprintf(operand, capacity, "%.*s", (int)(close - open - 1), open + 1);
    trim_in_place(operand);
    return operand[0] != '\0';
}

static const char *
find_unquoted_expression(const char *source, const char *needle)
{
    size_t length = strlen(needle);
    for(const char *p = source; *p;) {
        if(*p == '"' || *p == '\'') {
            char quote = *p++;
            while(*p && *p != quote) {
                if(*p == '\\' && p[1]) p++;
                p++;
            }
            if(*p) p++;
        } else if(strncmp(p, needle, length) == 0) {
            return p;
        } else {
            p++;
        }
    }
    return NULL;
}

static int
replace_checked_text(char *source, size_t capacity, const char *needle,
                     const char *replacement)
{
    char rewritten[ZIR_TEXT_MAX];
    const char *at = find_unquoted_expression(source, needle);
    int written;
    if(at == NULL) return 0;
    written = snprintf(rewritten, sizeof(rewritten), "%.*s%s%s",
                       (int)(at - source), source, replacement,
                       at + strlen(needle));
    if(written < 0 || (size_t)written >= sizeof(rewritten) ||
       (size_t)written >= capacity) return 0;
    copy_text(source, capacity, rewritten);
    return 1;
}

int
rewrite_checked_text(Checker *c, const ZirExpr *expr, const char *replacement)
{
    char needle[ZIR_TEXT_MAX];
    char text[ZIR_TEXT_MAX];
    if(c->current_stmt == NULL || c->fn->from_ir) return 1;
    copy_text(needle, sizeof(needle), expr->text);
    copy_text(text, sizeof(text), c->current_stmt->text);
    if(!replace_checked_text(text, sizeof(text), needle, replacement))
        return 0;
    c->current_stmt->text = KeepText(text);
    for(int i = 0; i < c->fn->expr_count; i++) {
        ZirExpr *candidate = &c->fn->exprs[i];
        if(candidate != expr &&
           candidate->span.line == expr->span.line &&
           candidate->kind != ZIR_EXPR_STRING &&
           candidate->kind != ZIR_EXPR_SIZE_OF &&
           candidate->kind != ZIR_EXPR_IDENT &&
           candidate->kind != ZIR_EXPR_INT &&
           candidate->kind != ZIR_EXPR_FLOAT &&
           candidate->kind != ZIR_EXPR_MEMBER &&
           candidate->kind != ZIR_EXPR_POINTER_MEMBER &&
           find_unquoted_expression(candidate->text, needle) != NULL) {
            copy_text(text, sizeof(text), candidate->text);
            if(!replace_checked_text(text, sizeof(text), needle, replacement))
                return 0;
            candidate->text = KeepText(text);
        }
    }
    return 1;
}

/* SPELLING names the enum as the reference sees it, such as B.Kind for an
 * enum imported as B; the member takes that type. */
int
lower_enum_reference(Checker *c, ZirExpr *expr, const ZirType *enumeration,
                     const char *spelling, const char *member, int opened)
{
    int64_t value;
    char replacement[64];
    if(!EnumMemberValue(enumeration, member, &value)) {
        error(c, expr->span, "unknown enum member", member);
        return 0;
    }
    snprintf(replacement, sizeof(replacement), "%lld", (long long)value);
    /* A checker restart, such as after lowering an if-case, parses the
     * statement text again and must read the same value with the same
     * type: a qualified member keeps its enum type, so the text names it
     * with its type, while an opened member (using Enum) is a plain integer,
     * so the text holds its value. */
    char qualified[ZIR_NAME_MAX * 2 + 2];
    snprintf(qualified, sizeof(qualified), "%s.%s", spelling, member);
    if(!rewrite_checked_text(c, expr, opened ? replacement : qualified)) {
        error(c, expr->span, "cannot lower enum member", expr->text);
        return 0;
    }
    expr->text = KeepText(replacement);
    expr->name = "";
    expr->kind = ZIR_EXPR_INT;
    expr->left = expr->right = expr->third = -1;
    expr->type = KeepName(spelling);
    return 1;
}

/* VecPop and VecGet return Option(element). Find or register the concrete
 * application under the same deterministic name the declaration rewriter
 * produces, so source and saved IR agree without re-running the rewriter on
 * loaded graphs. */
int
vec_option_result_type(Checker *c, const char *element, ZirSourceSpan span,
                       char *out, size_t size)
{
    char canonical[ZIR_TEXT_MAX];
    char name[ZIR_NAME_MAX];
    uint64_t hash = UINT64_C(14695981039346656037);
    if(!canonical_type_arguments(element, canonical, sizeof(canonical))) {
        error(c, span, "Vec result element type is too long", element);
        return 0;
    }
    for(const unsigned char *p = (const unsigned char *)"Option"; *p; p++)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    hash = (hash ^ '(') * UINT64_C(1099511628211);
    for(const unsigned char *p = (const unsigned char *)canonical; *p; p++)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    snprintf(name, sizeof(name), "__type_%016llx", (unsigned long long)hash);
    /* Saved IR carries the instantiated record directly; the Option template
     * and its module may have been pruned by linking. */
    for(int t = 0; t < c->module->type_count; t++)
        if(strcmp(c->module->types[t].name, name) == 0) {
            copy_text(out, size, name);
            return 1;
        }
    const ZirType *generic = FindType(c->module, "Option", NULL);
    char parameter[ZIR_NAME_MAX] = "";
    const char *cursor, *dollar;
    size_t length = 0;
    size_t offset = 0;
    ZirTypeField field;
    if(generic == NULL || !generic->is_record_template) {
        error(c, span, "Vec result requires the Option record; import option",
              element);
        return 0;
    }
    cursor = skip_ws(generic->template_params);
    dollar = *cursor == '$' ? cursor + 1 : cursor;
    while((isalnum((unsigned char)dollar[length]) || dollar[length] == '_') &&
          length + 1 < sizeof(parameter)) {
        parameter[length] = dollar[length];
        length++;
    }
    parameter[length] = '\0';
    if(TypeNextField(generic, &offset, &field) != 1 ||
       strcmp(field.name, "has_value") || strcmp(field.type, "bool") ||
       TypeNextField(generic, &offset, &field) != 1 ||
       strcmp(field.name, "value") || strcmp(field.type, parameter) ||
       TypeNextField(generic, &offset, &field) != 0) {
        error(c, span, "Vec result requires Option fields has_value and value",
              generic->name);
        return 0;
    }
    int generic_public = generic->is_public;
    int generic_file_private = generic->is_file_private;
    ZirType *instance = ModuleAddType(c->module, name, span);
    if(instance == NULL) {
        error(c, span, "cannot register Vec result record", name);
        return 0;
    }
    TypeApplicationScope(c->module, canonical, generic_public,
                         generic_file_private, instance);
    instance->is_type_instance = 1;
    instance->is_synthetic_application = 1;
    copy_text(instance->template_name, sizeof(instance->template_name),
              "Option");
    copy_text(instance->template_args, sizeof(instance->template_args),
              canonical);
    generic = FindType(c->module, "Option", NULL);
    instance = NULL;
    for(int t = 0; t < c->module->type_count; t++)
        if(strcmp(c->module->types[t].name, name) == 0) {
            instance = &c->module->types[t];
            break;
        }
    if(instance == NULL || generic == NULL ||
       !InstantiateGenericRecord(instance, generic)) {
        error(c, span, "cannot instantiate Vec result record", name);
        return 0;
    }
    instance = NULL;
    for(int t = 0; t < c->module->type_count; t++)
        if(strcmp(c->module->types[t].name, name) == 0) {
            instance = &c->module->types[t];
            break;
        }
    if(instance != NULL && instance->body[0]) {
        char expanded[sizeof(instance->body)];
        if(!rewrite_type_applications(c->module, instance->body, expanded,
                                      sizeof(expanded), span, 0)) {
            error(c, span, "cannot expand Vec result record", name);
            return 0;
        }
        copy_text(instance->body, sizeof(instance->body), expanded);
    }
    copy_text(out, size, name);
    return 1;
}

int
reserve_compound_constants(const ZirModule *module, ZirFunction *fn)
{
    size_t needed = (size_t)fn->expr_count;
    for(int i = 0; i < fn->expr_count; i++) {
        const ZirExpr *node = &fn->exprs[i];
        char name[ZIR_NAME_MAX];
        if(node->kind == ZIR_EXPR_IDENT)
            copy_text(name, sizeof(name), node->name);
        else if(node->kind == ZIR_EXPR_MEMBER && node->left >= 0 &&
                fn->exprs[node->left].kind == ZIR_EXPR_IDENT) {
            int written = snprintf(name, sizeof(name), "%s.%s",
                fn->exprs[node->left].name, node->name);
            if(written < 0 || (size_t)written >= sizeof(name)) continue;
        } else continue;
        CompoundConstant compound = {0};
        if(bound_compound_constant(module, name, 0,
                                   &compound) != 1)
            continue;
        ZirFunction probe = {0};
        int root = ParseExpr(&probe, compound.owner,
                             compound.literal, node->span);
        if(root >= 0 && probe.expr_count > 0)
            needed += (size_t)probe.expr_count - 1;
        free(probe.exprs);
        if(needed > INT_MAX || needed > SIZE_MAX / sizeof(*fn->exprs))
            return 0;
    }
    if(needed <= (size_t)fn->expr_cap) return 1;
    ZirExpr *expanded = realloc(fn->exprs, needed * sizeof(*expanded));
    if(expanded == NULL) return 0;
    fn->exprs = expanded;
    fn->expr_cap = (int)needed;
    return 1;
}

int
inline_compound_constant(Checker *c, int index,
                         const CompoundConstant *compound)
{
    ZirFunction probe = {0};
    int root = ParseExpr(&probe, compound->owner, compound->literal,
                         c->fn->exprs[index].span);
    if(root < 0 || probe.exprs[root].kind != ZIR_EXPR_COMPOUND) {
        free(probe.exprs);
        return 0;
    }
    /* Checking appends expressions, such as widening casts, beyond what
     * reserve_compound_constants counted, so a later use may need more
     * room. Callers refetch their expressions after checking an operand. */
    size_t needed = (size_t)c->fn->expr_count + (size_t)probe.expr_count - 1;
    if(needed > (size_t)c->fn->expr_cap) {
        size_t grown = needed + needed / 2;
        ZirExpr *expanded = grown <= INT_MAX &&
            grown <= SIZE_MAX / sizeof(*expanded) ?
            realloc(c->fn->exprs, grown * sizeof(*expanded)) : NULL;
        if(expanded == NULL) {
            free(probe.exprs);
            return 0;
        }
        c->fn->exprs = expanded;
        c->fn->expr_cap = (int)grown;
    }
    for(int i = 0; i < probe.expr_count; i++) {
        ZirExpr *node = &probe.exprs[i];
        if(node->kind != ZIR_EXPR_COMPOUND) continue;
        char qualified[ZIR_NAME_MAX];
        if(!compound_type_at_use(c->module, compound, node->name,
                                 qualified, sizeof(qualified))) {
            free(probe.exprs);
            return 0;
        }
        node->name = KeepName(qualified);
    }
    int *map = malloc((size_t)probe.expr_count * sizeof(*map));
    if(map == NULL) {
        free(probe.exprs);
        return 0;
    }
    ZirExpr previous = c->fn->exprs[index];
    for(int i = 0; i < probe.expr_count; i++)
        map[i] = i == root ? index : c->fn->expr_count++;
    for(int i = 0; i < probe.expr_count; i++)
        c->fn->exprs[map[i]] = probe.exprs[i];
    for(int i = 0; i < probe.expr_count; i++) {
        ZirExpr *node = &c->fn->exprs[map[i]];
        if(node->left >= 0) node->left = map[node->left];
        if(node->right >= 0) node->right = map[node->right];
        if(node->third >= 0) node->third = map[node->third];
        if(node->first_child >= 0)
            node->first_child = map[node->first_child];
        if(node->next_sibling >= 0)
            node->next_sibling = map[node->next_sibling];
    }
    ZirExpr *replacement = &c->fn->exprs[index];
    replacement->next_sibling = previous.next_sibling;
    replacement->argument_index = previous.argument_index;
    replacement->argument_name = previous.argument_name;
    replacement->span = previous.span;
    c->aggregate_rewritten = 1;
    free(map);
    free(probe.exprs);
    return 1;
}
