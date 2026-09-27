#include "zir_borrow.h"
#include "zir_diagnostic.h"
#include "zir_text.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Origin {
    uint64_t parameters;
    int depth;
    int invalid;
    int unknown;
    uint64_t globals;
} Origin;

typedef struct BorrowFunction {
    const ZirModule *module;
    const ZirFunction *fn;
    Origin returned;
    Origin *locals;
} BorrowFunction;

typedef struct BorrowBinding {
    const ZirModule *module;
    const ZirGlobal *global;
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    Origin origin;
    int local;
    int depth;
    int captured;
    int global_index;
    int text_borrow_count;
    struct BorrowBinding *text_backing;
} BorrowBinding;

typedef struct ActiveTextBorrow {
    BorrowBinding *backing;
    int depth;
} ActiveTextBorrow;

typedef struct BorrowCheck {
    BorrowFunction *functions;
    int count;
    BorrowFunction *current;
    BorrowBinding *bindings;
    int binding_count;
    BorrowBinding *globals;
    int global_count;
    Origin *global_origins;
    ActiveTextBorrow *active;
    int active_count;
    int depth;
    int changed;
    int failed;
} BorrowCheck;

static Origin
merge(Origin a, Origin b)
{
    a.parameters |= b.parameters;
    a.globals |= b.globals;
    if(b.depth > a.depth)
        a.depth = b.depth;
    a.invalid |= b.invalid;
    a.unknown |= b.unknown;
    return a;
}

static void
accumulate(BorrowCheck *check, Origin *destination, Origin source)
{
    Origin next = merge(*destination, source);
    if(next.parameters != destination->parameters ||
       next.depth != destination->depth ||
       next.invalid != destination->invalid ||
       next.unknown != destination->unknown ||
       next.globals != destination->globals) {
        *destination = next;
        check->changed = 1;
    }
}

static void
reject(BorrowCheck *check, ZirSourceSpan span, const char *message)
{
    if(!check->failed)
        Diagnostic(span, "check.slice_lifetime", "%s", message);
    check->failed = 1;
}

static void
add_active_text_borrow(BorrowCheck *check, BorrowBinding *backing, int depth)
{
    if(backing == NULL)
        return;
    check->active[check->active_count].backing = backing;
    check->active[check->active_count].depth = depth;
    check->active_count++;
    backing->text_borrow_count++;
}

static void
release_active_text_borrows(BorrowCheck *check, int depth)
{
    int kept = 0;
    for(int i = 0; i < check->active_count; i++) {
        if(check->active[i].depth == depth) {
            check->active[i].backing->text_borrow_count--;
            continue;
        }
        check->active[kept++] = check->active[i];
    }
    check->active_count = kept;
}

static BorrowBinding *
global_binding(BorrowCheck *check, const char *name)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    if(ResolveGlobal(check->current->module, name, &owner, &global) != 1)
        return NULL;
    for(int i = 0; i < check->global_count; i++)
        if(check->globals[i].module == owner && check->globals[i].global == global)
            return &check->globals[i];
    return NULL;
}

static BorrowBinding *
binding(BorrowCheck *check, const char *name)
{
    for(int i = check->binding_count - 1; i >= 0; i--)
        if(!strcmp(check->bindings[i].name, name))
            return &check->bindings[i];
    return global_binding(check, name);
}

static int
contains_view(const ZirModule *module, const char *type, int depth)
{
    if(depth > 32 || type == NULL || !*type || *type == '*')
        return 0;
    if(!strcmp(type, "string") || SliceElementType(type, NULL, 0))
        return 1;
    char element[ZIR_NAME_MAX];
    if(ArrayElementType(type, element, sizeof(element), NULL))
        return contains_view(module, element, depth + 1);
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template || record->is_extern)
        return 0;
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1)
        if(contains_view(owner ? owner : module, field.type, depth + 1))
            return 1;
    return 0;
}

static int
view_type(BorrowCheck *check, const char *type)
{
    return contains_view(check->current->module, type, 0);
}

static const ZirExpr *
destination_root(const ZirFunction *fn, int index)
{
    while(index >= 0) {
        const ZirExpr *expression = &fn->exprs[index];
        if(expression->kind == ZIR_EXPR_IDENT)
            return expression;
        if(expression->kind != ZIR_EXPR_MEMBER &&
           expression->kind != ZIR_EXPR_POINTER_MEMBER &&
           expression->kind != ZIR_EXPR_INDEX)
            return NULL;
        index = expression->left;
    }
    return NULL;
}

static Origin expression_origin(BorrowCheck *check, int index);

static BorrowBinding *
backing_identifier(BorrowCheck *check, int index)
{
    const ZirFunction *fn = check->current->fn;
    while(index >= 0) {
        const ZirExpr *expression = &fn->exprs[index];
        if(expression->kind == ZIR_EXPR_IDENT)
            return binding(check, expression->name);
        if(expression->kind != ZIR_EXPR_SLICE &&
           expression->kind != ZIR_EXPR_MEMBER &&
           expression->kind != ZIR_EXPR_POINTER_MEMBER &&
           expression->kind != ZIR_EXPR_INDEX)
            return NULL;
        index = expression->left;
    }
    return NULL;
}

static BorrowBinding *
text_view_backing(BorrowCheck *check, int index)
{
    const ZirFunction *fn = check->current->fn;
    const ZirExpr *expression = index < 0 ? NULL : &fn->exprs[index];
    if(expression == NULL)
        return NULL;
    if(expression->kind == ZIR_EXPR_IDENT) {
        BorrowBinding *source = binding(check, expression->name);
        return source == NULL ? NULL : source->text_backing;
    }
    if(expression->kind == ZIR_EXPR_MEMBER ||
       expression->kind == ZIR_EXPR_POINTER_MEMBER ||
       expression->kind == ZIR_EXPR_INDEX ||
       expression->kind == ZIR_EXPR_SLICE)
        return text_view_backing(check, expression->left);
    if(expression->kind != ZIR_EXPR_CALL)
        return NULL;
    if(!strcmp(expression->name, "TextView") ||
       !strcmp(expression->name, "VecSlice")) {
        if(expression->first_child < 0 ||
           fn->exprs[expression->first_child].next_sibling >= 0)
            return NULL;
        BorrowBinding *source =
            text_view_backing(check, expression->first_child);
        return source != NULL ? source :
            backing_identifier(check, expression->first_child);
    }
    if(!strcmp(expression->name, "BuilderFinish") ||
       !view_type(check, expression->type))
        return NULL;

    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    if(ResolveFunction(check->current->module, expression->name,
                       &owner, &callee) <= 0)
        return NULL;
    BorrowFunction *summary = NULL;
    for(int i = 0; i < check->count; i++)
        if(check->functions[i].fn == callee)
            summary = &check->functions[i];
    if(summary == NULL)
        return NULL;
    BorrowBinding *result = NULL;
    for(int child = expression->first_child; child >= 0;
        child = fn->exprs[child].next_sibling) {
        int parameter = fn->exprs[child].argument_index;
        if(parameter < 0 || parameter >= 64 ||
           !(summary->returned.parameters & (UINT64_C(1) << parameter)))
            continue;
        BorrowBinding *candidate = text_view_backing(check, child);
        if(candidate == NULL) {
            candidate = backing_identifier(check, child);
            if(candidate != NULL && candidate->type[0] != '[' &&
               strncmp(candidate->type, "Vec(", 4) != 0)
                candidate = NULL;
        }
        if(candidate != NULL && result == NULL)
            result = candidate;
    }
    return result;
}

static Origin
call_origin(BorrowCheck *check, const ZirExpr *expression)
{
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    if(ResolveFunction(check->current->module, expression->name, &owner, &callee) <= 0)
        return (Origin){0, 0, 1};
    BorrowFunction *summary = NULL;
    for(int i = 0; i < check->count; i++)
        if(check->functions[i].fn == callee)
            summary = &check->functions[i];
    if(summary == NULL)
        return (Origin){0, 0, 1};
    Origin result = {0};
    result.depth = summary->returned.depth;
    result.invalid = summary->returned.invalid;
    result.unknown = summary->returned.unknown;
    result.globals = summary->returned.globals;
    for(int child = expression->first_child; child >= 0;
        child = check->current->fn->exprs[child].next_sibling) {
        int parameter = check->current->fn->exprs[child].argument_index;
        if(parameter >= 0 && parameter < 64 &&
           (summary->returned.parameters & (UINT64_C(1) << parameter)))
            result = merge(result, expression_origin(check, child));
    }
    return result;
}

static Origin
expression_origin(BorrowCheck *check, int index)
{
    if(index < 0)
        return (Origin){0};
    const ZirFunction *fn = check->current->fn;
    const ZirExpr *expression = &fn->exprs[index];
    switch(expression->kind) {
    case ZIR_EXPR_IDENT: {
        BorrowBinding *source = binding(check, expression->name);
        if(expression->type[0] != '[' && strchr(expression->type, '*') != NULL) {
            /* A direct pointer parameter owns no storage, but its caller
             * keeps the pointee live for this invocation. Views of its
             * fixed-array fields may be used here, never returned. */
            if(source != NULL && source->local < 0 &&
               source->origin.depth == 1 && !source->origin.invalid)
                return source->origin;
            Origin unknown = {0};
            unknown.unknown = 1;
            return unknown;
        }
        if(source == NULL)
            return (Origin){0}; /* Typed module-owned array or record storage. */
        if(view_type(check, source->type))
            return source->local >= 0 ? check->current->locals[source->local] : source->origin;
        return (Origin){0, source->depth, 0};
    }
    case ZIR_EXPR_MEMBER:
    case ZIR_EXPR_POINTER_MEMBER:
    case ZIR_EXPR_INDEX:
        return expression_origin(check, expression->left);
    case ZIR_EXPR_SLICE: {
        const ZirExpr *base = &fn->exprs[expression->left];
        if(base->kind == ZIR_EXPR_IDENT &&
           ArrayElementType(base->type, NULL, 0, NULL)) {
            BorrowBinding *source = binding(check, base->name);
            if(source != NULL) {
                Origin origin = {0};
                origin.depth = source->depth;
                if(source->global_index >= 0)
                    origin.globals = UINT64_C(1) << source->global_index;
                return origin;
            }
        }
        return expression_origin(check, expression->left);
    }
    case ZIR_EXPR_CONDITIONAL:
        return merge(expression_origin(check, expression->right),
                     expression_origin(check, expression->third));
    case ZIR_EXPR_FIELD_INIT:
        return expression_origin(check, expression->right);
    case ZIR_EXPR_COMPOUND: {
        Origin result = {0};
        for(int child = expression->first_child; child >= 0;
            child = fn->exprs[child].next_sibling)
            if(view_type(check, fn->exprs[child].type))
                result = merge(result, expression_origin(check, child));
        return result;
    }
    case ZIR_EXPR_CALL:
        if(!strcmp(expression->name, "TextView"))
            return expression_origin(check, expression->first_child);
        if(!strcmp(expression->name, "BuilderFinish"))
            return (Origin){0};
        if(!strcmp(expression->name, "VecSlice")) {
            /* The view borrows its Vec argument's binding; the move rules
             * keep that binding alive while the view is in scope. */
            return expression_origin(check, expression->first_child);
        }
        if(view_type(check, expression->type)) {
            /* Host slice returns own their storage: the VM copies the
             * elements before the value reaches the caller. Ordinary
             * checked calls keep their summarized origins. */
            const ZirModule *owner = NULL;
            const ZirFunction *callee = NULL;
            if(ResolveFunction(check->current->module, expression->name,
                               &owner, &callee) <= 0)
                return (Origin){0};
            return call_origin(check, expression);
        }
        return (Origin){0, 0, 1}; /* A returned array/record is temporary storage. */
    case ZIR_EXPR_STRING:
        return (Origin){0};
    default:
        return (Origin){0, 0, 1};
    }
}

/* Inspect every range, including ranges passed directly to void/scalar calls. */
static void
check_ranges(BorrowCheck *check, int index)
{
    if(index < 0 || check->failed)
        return;
    const ZirExpr *expression = &check->current->fn->exprs[index];
    if(expression->kind == ZIR_EXPR_SLICE &&
       strcmp(check->current->fn->exprs[expression->left].type, "string") != 0 &&
       expression_origin(check, index).invalid)
        reject(check, expression->span, "slice range requires live owned backing storage");
    check_ranges(check, expression->left);
    check_ranges(check, expression->right);
    check_ranges(check, expression->third);
    for(int child = expression->first_child; child >= 0;
        child = check->current->fn->exprs[child].next_sibling)
        check_ranges(check, child);
}

static BorrowBinding *
add_binding(BorrowCheck *check, const char *name, const char *type,
            Origin origin, int local, int depth, int captured)
{
    BorrowBinding *item = &check->bindings[check->binding_count++];
    copy_text(item->name, sizeof(item->name), name);
    copy_text(item->type, sizeof(item->type), type);
    item->origin = origin;
    item->local = local;
    item->depth = depth;
    item->captured = captured;
    return item;
}

static void
check_function(BorrowCheck *check, BorrowFunction *function)
{
    const ZirFunction *fn = function->fn;
    check->current = function;
    check->binding_count = 0;
    check->active_count = 0;
    check->depth = 1;
    check->bindings = calloc((size_t)fn->stmt_count + 65, sizeof(*check->bindings));
    if(check->bindings == NULL) {
        reject(check, fn->span, "out of memory checking slice lifetimes");
        return;
    }
    check->active = calloc((size_t)fn->stmt_count + 1, sizeof(*check->active));
    if(check->active == NULL) {
        free(check->bindings);
        check->bindings = NULL;
        reject(check, fn->span, "out of memory checking text borrows");
        return;
    }
    char parameters[64][ZIR_TEXT_MAX];
    int count = *skip_ws(fn->args) ?
        split_top_level(fn->args, parameters[0], 64, sizeof(parameters[0])) : 0;
    for(int i = 0; i < count; i++) {
        char *colon = strchr(parameters[i], ':');
        if(colon == NULL)
            continue;
        *colon++ = '\0';
        trim_in_place(parameters[i]);
        trim_in_place(colon);
        Origin origin = {0};
        int local = -1;
        if(view_type(check, colon)) {
            origin.parameters = UINT64_C(1) << i;
            local = fn->stmt_count + i;
            accumulate(check, &function->locals[local], origin);
        } else if(colon[0] == '*') {
            origin.depth = 1;
        }
        add_binding(check, parameters[i], colon, origin, local, 1, 0);
    }
    for(int i = 0; i < fn->stmt_count && !check->failed; i++) {
        const ZirStmt *statement = &fn->stmts[i];
        if(statement->kind == ZIR_STMT_BLOCK_CLOSE) {
            release_active_text_borrows(check, check->depth);
            while(check->binding_count && check->bindings[check->binding_count - 1].depth == check->depth)
                check->binding_count--;
            check->depth--;
            continue;
        }
        check_ranges(check, statement->lhs_root);
        check_ranges(check, statement->expr_root);
        if(statement->kind == ZIR_STMT_ASSIGN && statement->lhs_root >= 0) {
            const ZirExpr *root = destination_root(fn, statement->lhs_root);
            BorrowBinding *backing = root == NULL ? NULL : binding(check, root->name);
            int global_alias = 0;
            if(backing != NULL && backing->global_index >= 0)
                for(int g = 0; g < check->global_count; g++)
                    if(contains_view(check->globals[g].module,
                                     check->globals[g].type, 0) &&
                       (check->global_origins[g].globals &
                        (UINT64_C(1) << backing->global_index)) != 0) {
                        global_alias = 1;
                        break;
                    }
            if(backing != NULL &&
               (backing->text_borrow_count != 0 || global_alias))
                reject(check, statement->span,
                       "mutating text backing storage while its view is live");
        }
        if(statement->kind == ZIR_STMT_DECL) {
            if(view_type(check, statement->type)) {
                Origin source = expression_origin(check, statement->expr_root);
                int direct_view = SliceElementType(statement->type, NULL, 0) ||
                    !strcmp(statement->type, "string");
                accumulate(check, &function->locals[i], source);
                if((source.invalid && direct_view) ||
                   source.depth > check->depth)
                    reject(check, statement->span,
                           !strcmp(statement->type, "string") ?
                           "text view initializer outlives its backing storage" :
                           SliceElementType(statement->type, NULL, 0) ?
                           "slice initializer outlives its backing storage" :
                           "view initializer outlives its backing storage");
            }
            BorrowBinding *declared = add_binding(check, statement->name,
                    statement->type, (Origin){0}, i, check->depth, 0);
            declared->text_backing = text_view_backing(check, statement->expr_root);
            if(view_type(check, statement->type))
                add_active_text_borrow(check, declared->text_backing,
                                       check->depth);
        } else if(statement->kind == ZIR_STMT_ASSIGN && statement->lhs_root >= 0) {
            const ZirExpr *destination = &fn->exprs[statement->lhs_root];
            if(view_type(check, destination->type)) {
                const ZirExpr *root = destination_root(fn, statement->lhs_root);
                BorrowBinding *target = root == NULL ? NULL : binding(check, root->name);
                Origin source = expression_origin(check, statement->expr_root);
                /* A zero value or a value backed only by static storage
                 * cannot escape a borrow, even through a pointer target. */
                int static_storage =
                    !source.invalid && !source.unknown && source.depth == 0 &&
                    source.parameters == 0;
                int direct_view = destination->kind == ZIR_EXPR_IDENT &&
                    (SliceElementType(destination->type, NULL, 0) ||
                     !strcmp(destination->type, "string"));
                if(!static_storage &&
                   (target == NULL ||
                    (target->captured && target->global_index < 0) ||
                    (source.invalid && direct_view) ||
                    source.depth > target->depth)) {
                    reject(check, statement->span,
                           !strcmp(destination->type, "string") ?
                           "text view assignment may escape its backing storage" :
                           SliceElementType(destination->type, NULL, 0) ?
                           "slice assignment may escape its backing storage" :
                           "view assignment may escape its backing storage");
                } else if(target != NULL && target->local >= 0) {
                    accumulate(check, &function->locals[target->local], source);
                } else if(target != NULL && target->global_index >= 0 &&
                          static_storage) {
                    BorrowBinding *backing =
                        text_view_backing(check, statement->expr_root);
                    accumulate(check, &check->global_origins[target->global_index],
                               source);
                    target->origin = check->global_origins[target->global_index];
                    if(target->text_backing == NULL && backing != NULL)
                        target->text_backing = backing;
                }
                if(!static_storage && target != NULL &&
                   !(target->captured && target->global_index < 0))
                    add_active_text_borrow(check,
                                           text_view_backing(check, statement->expr_root),
                                           target->depth);
            }
        } else if(statement->kind == ZIR_STMT_RETURN &&
                  view_type(check, fn->return_type)) {
            Origin source = expression_origin(check, statement->expr_root);
            if(source.invalid || source.unknown || source.depth > 0)
                reject(check, statement->span,
                       !strcmp(fn->return_type, "string") ?
                       "returned text view borrows local or temporary storage" :
                       SliceElementType(fn->return_type, NULL, 0) ?
                       "returned slice borrows local or temporary storage" :
                       "returned value borrows local or temporary storage");
            accumulate(check, &function->returned, source);
        }
        if(statement->kind == ZIR_STMT_IF || statement->kind == ZIR_STMT_WHILE ||
           statement->kind == ZIR_STMT_BLOCK_OPEN)
            check->depth++;
    }
    free(check->bindings);
    check->bindings = NULL;
    free(check->active);
    check->active = NULL;
}

int
CheckSliceLifetimes(ZirProgram **programs, int count)
{
    BorrowCheck check = {0};
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            check.count += programs[p]->modules[m].function_count;
    check.functions = calloc((size_t)check.count, sizeof(*check.functions));
    if(check.functions == NULL && check.count != 0)
        return 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            check.global_count += programs[p]->modules[m].global_count;
    check.globals = calloc((size_t)check.global_count, sizeof(*check.globals));
    check.global_origins = calloc((size_t)check.global_count,
                                  sizeof(*check.global_origins));
    if((check.globals == NULL || check.global_origins == NULL) &&
       check.global_count != 0) {
        free(check.functions);
        free(check.globals);
        free(check.global_origins);
        return 0;
    }
    if(check.global_count > 64) {
        Diagnostic(programs[0]->modules[0].span, "check.slice_lifetime",
                   "too many globals for text alias tracking");
        free(check.functions);
        free(check.globals);
        free(check.global_origins);
        return 0;
    }
    int global_index = 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int g = 0; g < module->global_count; g++) {
                BorrowBinding *item = &check.globals[global_index];
                item->module = module;
                item->global = &module->globals[g];
                copy_text(item->name, sizeof(item->name),
                          module->globals[g].name);
                copy_text(item->type, sizeof(item->type),
                          module->globals[g].type);
                item->local = -1;
                item->captured = 1;
                item->global_index = global_index++;
            }
        }
    int index = 0;
    for(int p = 0; p < count; p++) {
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            for(int f = 0; f < module->function_count; f++) {
                BorrowFunction *function = &check.functions[index++];
                function->module = module;
                function->fn = &module->functions[f];
                function->locals = calloc((size_t)function->fn->stmt_count + 64, sizeof(Origin));
                if(function->locals == NULL)
                    check.failed = 1;
            }
        }
    }
    do {
        check.changed = 0;
        for(int i = 0; i < check.count && !check.failed; i++) {
            if(check.functions[i].fn->checked && !check.functions[i].fn->is_extern)
                check_function(&check, &check.functions[i]);
        }
    } while(check.changed && !check.failed);
    for(int i = 0; i < check.count; i++)
        free(check.functions[i].locals);
    free(check.functions);
    free(check.globals);
    free(check.global_origins);
    return !check.failed;
}
