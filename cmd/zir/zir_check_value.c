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
       !VecElementType(c->module, e->type, NULL, 0))
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

/* Only local bindings move. A global keeps shared module state, so moving it
 * would leave every other function reading transferred storage. */
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

/* A constructing aggregate moves each owned field from a local binding, fresh
 * call result, or another constructing aggregate with the same discipline. */
int
owned_initializer_shape(Checker *c, int index)
{
    const ZirExpr *e;
    if(index < 0 || index >= c->fn->expr_count)
        return 0;
    e = &c->fn->exprs[index];
    if(e->kind == ZIR_EXPR_IDENT)
        return !global_vec_source(c, index);
    if(e->kind == ZIR_EXPR_CALL)
        return 1;
    if(e->kind == ZIR_EXPR_MEMBER &&
       VecElementType(c->module, e->type, NULL, 0)) {
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
           !owned_initializer_shape(c, entry->right))
            return 0;
    }
    return 1;
}

int
numeric(const char *type)
{
    const char *scalar = ScalarType(type);
    return !strcmp(type, "integer") || !strcmp(type, "real") ||
           (*scalar && strcmp(scalar, "string") &&
            strchr("suf", scalar[0]) != NULL);
}

int
integer_type(const char *type)
{
    const char *scalar = ScalarType(type);
    return !strcmp(type, "integer") ||
           (scalar[0] != '\0' && strcmp(scalar, "string") != 0 &&
            (scalar[0] == 's' || scalar[0] == 'u'));
}

int
bound_expression(const ZirModule *module, const ZirFunction *expression, int index,
                 int depth, int64_t *value)
{
    if(index < 0 || depth > 128)
        return -1;
    const ZirExpr *node = &expression->exprs[index];
    if(node->kind == ZIR_EXPR_IDENT)
        return bound_constant(module, node->name, depth + 1, value);
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
        char *end;
        errno = 0;
        *value = strtoll(node->text, &end, 0);
        if(errno || end == node->text || *end || *value < 0 || *value > INT32_MAX)
            return -1;
        return 1;
    }
    int64_t left = 0, right = 0;
    if(node->kind == ZIR_EXPR_UNARY) {
        int status = bound_expression(module, expression, node->right, depth + 1, &right);
        if(status != 1)
            return status;
        if(!strcmp(node->op, "+"))
            *value = right;
        else if(!strcmp(node->op, "-"))
            *value = -right;
        else
            return -1;
    } else if(node->kind == ZIR_EXPR_BINARY) {
        int status = bound_expression(module, expression, node->left, depth + 1, &left);
        if(status != 1)
            return status;
        status = bound_expression(module, expression, node->right, depth + 1, &right);
        if(status != 1)
            return status;
        if(left == INT32_MIN && right == -1 &&
           (!strcmp(node->op, "/") || !strcmp(node->op, "%")))
            return -1;
        if(!strcmp(node->op, "+"))
            *value = left + right;
        else if(!strcmp(node->op, "-"))
            *value = left - right;
        else if(!strcmp(node->op, "*"))
            *value = left * right;
        else if(!strcmp(node->op, "/") && right != 0)
            *value = left / right;
        else if(!strcmp(node->op, "%") && right != 0)
            *value = left % right;
        else
            return -1;
    } else {
        return -1;
    }
    return *value >= INT32_MIN && *value <= INT32_MAX ? 1 : -1;
}

int
bound_constant(const ZirModule *module, const char *name, int depth, int64_t *value)
{
    if(depth > 128)
        return -1;
    const ZirDefine *definition = NULL;
    const ZirModule *owner = NULL;
    const char *dot = strchr(name, '.');
    const char *symbol = dot == NULL ? name : dot + 1;
    for(int pass = 0; pass < 2; pass++) {
        if(dot != NULL && pass == 0)
            continue;
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            if(pass != 0) {
                const ZirImport *import = &module->imports[i];
                if(!in_lookup_file(module, import->is_file_private,
                                   import->span))
                    continue;
                if(dot == NULL && import->kind != ZIR_IMPORT_OPEN &&
                   !(import->kind == ZIR_IMPORT_MODULE &&
                     import->is_using))
                    continue;
                if(dot != NULL &&
                   (import->kind != ZIR_IMPORT_MODULE ||
                    strlen(import->name) != (size_t)(dot - name) ||
                    strncmp(import->name, name, (size_t)(dot - name)) != 0))
                    continue;
            }
            const ZirModule *scope = pass == 0 ? module : module->imports[i].resolved_module;
            if(scope == NULL)
                continue;
            for(int j = 0; j < scope->define_count; j++) {
                const ZirDefine *candidate = &scope->defines[j];
                if((pass != 0 && !candidate->is_public) ||
                   (pass == 0 && !in_lookup_file(module,
                       candidate->is_file_private, candidate->span)) ||
                   strcmp(candidate->name, symbol))
                    continue;
                if(definition != NULL && definition != candidate)
                    return -1;
                definition = candidate;
                owner = scope;
            }
        }
        if(definition != NULL)
            break;
    }
    if(definition == NULL)
        return 0;
    ZirFunction expression = {0};
    int index = ParseExpr(&expression, owner, definition->value, definition->span);
    int status = bound_expression(owner, &expression, index, depth + 1, value);
    free(expression.exprs);
    return status;
}

/* Resolve a literal string definition into the expression graph. The source
 * macro spelling must not become a backend dependency in saved .zir or .zib. */
int
bound_string_constant(const ZirModule *module, const char *name, int depth,
                      char *literal, size_t size)
{
    if(depth > 128)
        return -1;
    const ZirDefine *definition = NULL;
    const ZirModule *owner = NULL;
    const char *dot = strchr(name, '.');
    const char *symbol = dot == NULL ? name : dot + 1;
    for(int pass = 0; pass < 2; pass++) {
        if(dot != NULL && pass == 0)
            continue;
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            if(pass != 0) {
                const ZirImport *import = &module->imports[i];
                if(!in_lookup_file(module, import->is_file_private,
                                   import->span))
                    continue;
                if(dot == NULL && import->kind != ZIR_IMPORT_OPEN &&
                   !(import->kind == ZIR_IMPORT_MODULE &&
                     import->is_using))
                    continue;
                if(dot != NULL &&
                   (import->kind != ZIR_IMPORT_MODULE ||
                    strlen(import->name) != (size_t)(dot - name) ||
                    strncmp(import->name, name, (size_t)(dot - name)) != 0))
                    continue;
            }
            const ZirModule *scope = pass == 0 ? module :
                                     module->imports[i].resolved_module;
            if(scope == NULL)
                continue;
            for(int j = 0; j < scope->define_count; j++) {
                const ZirDefine *candidate = &scope->defines[j];
                if((pass != 0 && !candidate->is_public) ||
                   (pass == 0 && !in_lookup_file(module,
                       candidate->is_file_private, candidate->span)) ||
                   strcmp(candidate->name, symbol))
                    continue;
                if(definition != NULL && definition != candidate)
                    return -1;
                definition = candidate;
                owner = scope;
            }
        }
        if(definition != NULL)
            break;
    }
    if(definition == NULL)
        return 0;
    ZirFunction expression = {0};
    int index = ParseExpr(&expression, owner, definition->value, definition->span);
    int status = 0;
    if(index < 0)
        status = -1;
    else if(expression.exprs[index].kind == ZIR_EXPR_STRING) {
        copy_text(literal, size, expression.exprs[index].text);
        status = 1;
    } else if(expression.exprs[index].kind == ZIR_EXPR_IDENT) {
        status = bound_string_constant(owner, expression.exprs[index].name,
                                       depth + 1, literal, size);
    }
    free(expression.exprs);
    return status;
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
    const char *dot = strchr(name, '.');
    const char *symbol = dot == NULL ? name : dot + 1;
    for(int pass = 0; pass < 2; pass++) {
        if(dot != NULL && pass == 0) continue;
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            if(pass != 0) {
                const ZirImport *import = &module->imports[i];
                if(!in_lookup_file(module, import->is_file_private,
                                   import->span)) continue;
                if(dot == NULL && import->kind != ZIR_IMPORT_OPEN) continue;
                if(dot != NULL &&
                   (import->kind != ZIR_IMPORT_MODULE ||
                    strlen(import->name) != (size_t)(dot - name) ||
                    strncmp(import->name, name,
                            (size_t)(dot - name)) != 0)) continue;
            }
            const ZirModule *scope = pass == 0 ? module :
                                     module->imports[i].resolved_module;
            if(scope == NULL) continue;
            for(int j = 0; j < scope->define_count; j++) {
                const ZirDefine *candidate = &scope->defines[j];
                if((pass != 0 && !candidate->is_public) ||
                   (pass == 0 && !in_lookup_file(module,
                       candidate->is_file_private, candidate->span)) ||
                   strcmp(candidate->name, symbol)) continue;
                if(definition != NULL && definition != candidate) return -1;
                definition = candidate;
                owner = scope;
            }
        }
        if(definition != NULL) break;
    }
    if(definition == NULL) return 0;
    ZirFunction expression = {0};
    int index = ParseExpr(&expression, owner, definition->value,
                          definition->span);
    int status = 0;
    if(index < 0) status = -1;
    else if(expression.exprs[index].kind == ZIR_EXPR_FLOAT) {
        copy_text(literal, size, expression.exprs[index].text);
        status = 1;
    } else if(expression.exprs[index].kind == ZIR_EXPR_IDENT)
        status = bound_real_constant(owner, expression.exprs[index].name,
                                     depth + 1, literal, size);
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
    const ZirType *visible = FindType(module, source, NULL);
    if(visible == declared) {
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
                               source);
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

static int
append_compound_text(char *out, size_t size, size_t *used,
                     const char *part)
{
    size_t length = strlen(part);
    if(length >= size - *used) return 0;
    memcpy(out + *used, part, length);
    *used += length;
    out[*used] = '\0';
    return 1;
}

static int
render_compound_at_use(const ZirFunction *expression, int index,
                       char *out, size_t size, size_t *used, int depth)
{
    if(index < 0 || index >= expression->expr_count || depth > 64)
        return 0;
    const ZirExpr *node = &expression->exprs[index];
    if(node->kind != ZIR_EXPR_COMPOUND)
        return append_compound_text(out, size, used, node->text);
    int array = node->name[0] == '[';
    if(!append_compound_text(out, size, used,
                             array ? ".[" : ".{")) return 0;
    int ordinal = 0;
    for(int child = node->first_child; child >= 0;
        child = expression->exprs[child].next_sibling) {
        const ZirExpr *entry = &expression->exprs[child];
        if(ordinal++ && !append_compound_text(out, size, used, ", "))
            return 0;
        if(!array && !strcmp(entry->op, "=")) {
            if(!append_compound_text(out, size, used, ".") ||
               !append_compound_text(out, size, used, entry->name) ||
               !append_compound_text(out, size, used, " = "))
                return 0;
        }
        if(!render_compound_at_use(expression, entry->right, out, size,
                                   used, depth + 1)) return 0;
    }
    return append_compound_text(out, size, used, array ? "]" : "}");
}

int
lower_compound_global(const ZirModule *module, const ZirGlobal *global,
                      const CompoundConstant *compound,
                      char *result, size_t size)
{
    ZirFunction expression = {0};
    int root = ParseExpr(&expression, compound->owner, compound->literal,
                         global->span);
    int ok = 0;
    if(root >= 0 && expression.exprs[root].kind == ZIR_EXPR_COMPOUND &&
       literal_compound_graph(&expression)) {
        char actual[ZIR_NAME_MAX];
        if(compound_type_at_use(module, compound,
                expression.exprs[root].name, actual, sizeof(actual)) &&
           !strcmp(actual, global->type)) {
            size_t used = 0;
            result[0] = '\0';
            ok = render_compound_at_use(&expression, root, result,
                                        size, &used, 0);
        }
    }
    free(expression.exprs);
    return ok;
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

static int
exported_define(const ZirModule *module, const char *name, int depth,
                const ZirDefine **definition, const ZirModule **owner)
{
    if(depth >= 32) return -1;
    *definition = NULL;
    *owner = NULL;
    for(int d = 0; d < module->define_count; d++) {
        const ZirDefine *candidate = &module->defines[d];
        if(!candidate->is_public || candidate->is_file_private ||
           strcmp(candidate->name, name)) continue;
        if(*definition != NULL && *definition != candidate) return -1;
        *definition = candidate;
        *owner = module;
    }
    if(*definition != NULL) return 1;
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *import = &module->imports[i];
        if(!import->is_using || !import->is_public ||
           import->is_file_private || import->resolved_module == NULL)
            continue;
        const ZirDefine *candidate = NULL;
        const ZirModule *candidate_owner = NULL;
        int found = exported_define(import->resolved_module, name,
                                    depth + 1, &candidate, &candidate_owner);
        if(found < 0 || (found == 1 && *definition != NULL &&
                         *definition != candidate))
            return -1;
        if(found == 1) {
            *definition = candidate;
            *owner = candidate_owner;
        }
    }
    return *definition != NULL;
}

int
visible_define(const ZirModule *module, const char *name,
               const ZirDefine **definition, const ZirModule **owner,
               const ZirImport **selected_import)
{
    *definition = NULL;
    *owner = NULL;
    if(selected_import != NULL) *selected_import = NULL;
    const char *dot = strchr(name, '.');
    const char *symbol = dot == NULL ? name : dot + 1;
    for(int pass = 0; pass < 2; pass++) {
        if(dot != NULL && pass == 0) continue;
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            const ZirImport *import = pass == 0 ? NULL :
                                      &module->imports[i];
            if(import != NULL) {
                if(!in_lookup_file(module, import->is_file_private,
                                   import->span)) continue;
                if(dot == NULL && import->kind != ZIR_IMPORT_OPEN &&
                   !(import->kind == ZIR_IMPORT_MODULE &&
                     import->is_using)) continue;
                if(dot != NULL &&
                   (import->kind != ZIR_IMPORT_MODULE ||
                    strlen(import->name) != (size_t)(dot - name) ||
                    strncmp(import->name, name,
                            (size_t)(dot - name)) != 0)) continue;
            }
            const ZirModule *scope = import == NULL ? module :
                                     import->resolved_module;
            if(scope == NULL) continue;
            if(import == NULL) {
                for(int j = 0; j < scope->define_count; j++) {
                    const ZirDefine *candidate = &scope->defines[j];
                    if(!in_lookup_file(module, candidate->is_file_private,
                                       candidate->span) ||
                       strcmp(candidate->name, symbol)) continue;
                    if(*definition != NULL && *definition != candidate)
                        return -1;
                    *definition = candidate;
                    *owner = scope;
                }
            } else {
                const ZirDefine *candidate = NULL;
                const ZirModule *candidate_owner = NULL;
                int found = exported_define(scope, symbol, 0,
                                            &candidate, &candidate_owner);
                if(found < 0 || (found == 1 && *definition != NULL &&
                                 *definition != candidate))
                    return -1;
                if(found == 1) {
                    *definition = candidate;
                    *owner = candidate_owner;
                    if(selected_import != NULL) *selected_import = import;
                }
            }
        }
        if(*definition != NULL) break;
    }
    return *definition != NULL;
}

int
bound_compound_constant(const ZirModule *module, const char *name, int depth,
                        CompoundConstant *result)
{
    if(depth > 128) return -1;
    const ZirDefine *definition = NULL;
    const ZirModule *owner = NULL;
    const ZirImport *selected_import = NULL;
    int found = visible_define(module, name, &definition, &owner,
                               &selected_import);
    if(found != 1) return found;
    ZirFunction expression = {0};
    int root = ParseExpr(&expression, owner, definition->value,
                         definition->span);
    int status = 0;
    if(root < 0) status = -1;
    else if(expression.exprs[root].kind == ZIR_EXPR_COMPOUND &&
            literal_compound_graph(&expression)) {
        if(strlen(definition->value) >= sizeof(result->literal)) status = -1;
        else {
            copy_text(result->literal, sizeof(result->literal),
                      definition->value);
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
normalize_array_at(const ZirModule *module, char *type, size_t size, int depth)
{
    char element[ZIR_NAME_MAX];
    int capacity;
    if(depth >= 64) return;
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
            if(visible_define(owner, resolved, &found, &next_owner,
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
        normalize_array_at(owner, resolved, sizeof(resolved), depth + 1);
        copy_text(type, size, resolved);
        if(!ArrayElementType(type, element, sizeof(element), NULL))
            return;
        module = owner;
    }
    normalize_array_at(module, element, sizeof(element), depth + 1);
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
    normalize_array_at(module, type, size, 0);
}

static int
pointer_type(const char *type)
{
    return type[0] != '[' && strchr(type, '*') != NULL;
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
    const ZirType *target = FindType(module, to, NULL);
    const ZirType *source = FindType(module, from, NULL);
    if(target != NULL && target == source) return 1;
    char to_element[ZIR_NAME_MAX], from_element[ZIR_NAME_MAX];
    int to_capacity, from_capacity;
    if(ArrayElementType(to, to_element, sizeof(to_element), &to_capacity) &&
       ArrayElementType(from, from_element, sizeof(from_element),
                        &from_capacity) &&
       to_capacity >= 0 && to_capacity == from_capacity)
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
            const ZirType *actual_record = FindType(owner, declaration->return_type, NULL);
            const ZirType *wanted_record = FindType(slot_owner, slot->procedure_return_type, NULL);
            matches = actual_record || wanted_record ? actual_record == wanted_record :
                !strcmp(declaration->return_type, slot->procedure_return_type);
        }
    }
    char actual[64][ZIR_TEXT_MAX], wanted[64][ZIR_TEXT_MAX];
    int actual_count = matches && *skip_ws(declaration->args) ?
        split_top_level(declaration->args, actual[0], 64, sizeof(actual[0])) : 0;
    int wanted_count = *skip_ws(slot->body) ?
        split_top_level(slot->body, wanted[0], 64, sizeof(wanted[0])) : 0;
    matches &= actual_count == wanted_count;
    for(int i = 0; matches && i < wanted_count; i++) {
        const char *actual_type = strchr(actual[i], ':');
        const char *wanted_type = strchr(wanted[i], ':');
        if(actual_type == NULL || wanted_type == NULL) {
            matches = 0;
            break;
        }
        actual_type = skip_ws(actual_type + 1);
        wanted_type = skip_ws(wanted_type + 1);
        const char *actual_scalar = ScalarType(actual_type);
        const char *wanted_scalar = ScalarType(wanted_type);
        if(*actual_scalar || *wanted_scalar) {
            matches = !strcmp(actual_scalar, wanted_scalar);
        } else {
            const ZirType *actual_record = FindType(owner, actual_type, NULL);
            const ZirType *wanted_record = FindType(slot_owner, wanted_type, NULL);
            matches = actual_record || wanted_record ? actual_record == wanted_record :
                !strcmp(actual_type, wanted_type);
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
    copy_text(value->type, sizeof(value->type), expected);
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

static char *
find_unquoted_expression(char *source, const char *needle)
{
    size_t length = strlen(needle);
    for(char *p = source; *p;) {
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
    char *at = find_unquoted_expression(source, needle);
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
    if(c->current_stmt == NULL || c->fn->from_ir) return 1;
    copy_text(needle, sizeof(needle), expr->text);
    if(!replace_checked_text(c->current_stmt->text,
                             sizeof(c->current_stmt->text), needle, replacement))
        return 0;
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
           find_unquoted_expression(candidate->text, needle) != NULL &&
           !replace_checked_text(candidate->text, sizeof(candidate->text),
                                 needle, replacement))
            return 0;
    }
    return 1;
}

int
lower_enum_reference(Checker *c, ZirExpr *expr, const ZirType *enumeration,
                     const char *member)
{
    int64_t value;
    char replacement[64];
    if(!EnumMemberValue(enumeration, member, &value)) {
        error(c, expr->span, "unknown enum member", member);
        return 0;
    }
    snprintf(replacement, sizeof(replacement), "%lld", (long long)value);
    if(!rewrite_checked_text(c, expr, replacement)) {
        error(c, expr->span, "cannot lower enum member", expr->text);
        return 0;
    }
    copy_text(expr->text, sizeof(expr->text), replacement);
    expr->name[0] = '\0';
    expr->kind = ZIR_EXPR_INT;
    expr->left = expr->right = expr->third = -1;
    copy_text(expr->type, sizeof(expr->type), enumeration->name);
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
    ZirType *instance = ModuleAddType(c->module, name, span);
    if(instance == NULL) {
        error(c, span, "cannot register Vec result record", name);
        return 0;
    }
    instance->is_public = generic->is_public;
    instance->is_file_private = generic->is_file_private;
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
    if(root < 0 || probe.exprs[root].kind != ZIR_EXPR_COMPOUND ||
       c->fn->expr_count + probe.expr_count - 1 > c->fn->expr_cap) {
        free(probe.exprs);
        return 0;
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
        copy_text(node->name, sizeof(node->name), qualified);
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
    copy_text(replacement->argument_name,
              sizeof(replacement->argument_name), previous.argument_name);
    replacement->span = previous.span;
    c->aggregate_rewritten = 1;
    free(map);
    free(probe.exprs);
    return 1;
}
