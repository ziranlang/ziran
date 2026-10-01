#include "zir_check_internal.h"

/* A pointer to a fixed array, such as *[16]float64. Native backends have no
 * declarator for it yet, so the checker rejects it instead of emitting C
 * that points at the wrong type. */

/* Whether expression tree ROOT reads NAME anywhere but at MOVE. */
static int
tree_reads_name(const ZirFunction *fn, int root, const char *name, int move)
{
    if(root < 0 || root >= fn->expr_count)
        return 0;
    const ZirExpr *e = &fn->exprs[root];
    if(root != move && e->kind == ZIR_EXPR_IDENT && !strcmp(e->name, name))
        return 1;
    if(tree_reads_name(fn, e->left, name, move) ||
       tree_reads_name(fn, e->right, name, move) ||
       tree_reads_name(fn, e->third, name, move))
        return 1;
    for(int child = e->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        if(tree_reads_name(fn, child, name, move))
            return 1;
    return 0;
}

/* A Vec moved into a call is emptied before the statement runs on native
 * targets, so reading it elsewhere in the same statement would see a
 * different value than the portable runner. Reject that read. */
static void
reject_read_of_moved(Checker *c, int root, int node)
{
    const ZirFunction *fn = c->fn;
    if(node < 0 || node >= fn->expr_count)
        return;
    const ZirExpr *e = &fn->exprs[node];
    if(e->is_move && e->kind == ZIR_EXPR_IDENT &&
       tree_reads_name(fn, root, e->name, node)) {
        error(c, e->span, "a Vec moved in a statement cannot also be read in it; "
              "read it into a local first", e->name);
        return;
    }
    reject_read_of_moved(c, root, e->left);
    reject_read_of_moved(c, root, e->right);
    reject_read_of_moved(c, root, e->third);
    for(int child = e->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        reject_read_of_moved(c, root, child);
}

static int
pointer_to_array(const char *type)
{
    const char *base = type;
    while(*base == '*')
        base++;
    return base != type && *base == '[';
}
/* Buffers check_function keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CheckFunctionBuffers {
    char params[64][ZIR_TEXT_MAX];
    char parameters[64][ZIR_TEXT_MAX];
} CheckFunctionBuffers;

int check_function(Checker *c, ZirFunction *fn);

/* Method adapters are native ABI metadata; their canonical bodies and ordinary
 * calls remain checked Ziran procedures on every target. */
int
check_go_method(Checker *c, ZirFunction *fn)
{
    if(!fn->go_method[0])
        return !fn->go_method_results;
    static const char *const keywords[] = {
        "break", "case", "chan", "const", "continue", "default", "defer",
        "else", "fallthrough", "for", "func", "go", "goto", "if", "import",
        "interface", "map", "package", "range", "return", "select",
        "struct", "switch", "type", "var", "_", NULL
    };
    int errors_before = c->errors;
    select_lookup_file(c->module, fn->span);
    for(int i = 0; keywords[i] != NULL; i++)
        if(!strcmp(fn->go_method, keywords[i]))
            error(c, fn->span, "#go_method needs a non-keyword Go identifier", fn->go_method);
    if(fn->is_extern || fn->is_template || fn->is_specialization ||
       fn->is_global_initializer || fn->is_conversion)
        error(c, fn->span, "#go_method requires a concrete ordinary procedure", fn->name);
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    const char *receiver = parameters->count > 0 ? parameters->items[0].type : NULL;
    const char *base = receiver != NULL && receiver[0] == '*' ? receiver + 1 : receiver;
    const ZirModule *owner = NULL;
    const ZirType *record = base != NULL ? FindType(c->module, base, &owner) : NULL;
    if(record == NULL || owner != c->module || record->is_extern ||
       record->is_enum || record->is_union || record->is_procedure_type ||
       record->is_record_template || record->is_go_anonymous ||
       record->is_owned_vec || record->is_map) {
        error(c, fn->span, "#go_method first parameter must be a local named record or its pointer", fn->name);
        return 0;
    }
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1) {
        char native_name[ZIR_NAME_MAX];
        go_field_ident(field.name, native_name, sizeof(native_name));
        if(!strcmp(native_name, fn->go_method))
            error(c, fn->span, "#go_method name conflicts with a record field", fn->go_method);
    }
    for(int i = 0; i < c->module->function_count; i++) {
        const ZirFunction *other = &c->module->functions[i];
        if(other == fn || strcmp(other->go_method, fn->go_method))
            continue;
        const ZirParameters *other_parameters = ParametersOf(FunctionArgs(other));
        if(other_parameters->count == 0 || other_parameters->items[0].type == NULL)
            continue;
        const char *other_receiver = other_parameters->items[0].type;
        if(other_receiver[0] == '*') other_receiver++;
        select_lookup_file(c->module, other->span);
        const ZirModule *other_owner = NULL;
        const ZirType *other_record = FindType(c->module, other_receiver, &other_owner);
        select_lookup_file(c->module, fn->span);
        if(other_record == record && other_owner == owner)
            error(c, fn->span, "duplicate #go_method for this record", fn->go_method);
    }
    for(int i = 0; i < parameters->count; i++)
        if(parameters->items[i].type != NULL &&
           contains_vec(c->module, parameters->items[i].type, 0))
            error(c, fn->span, "Vec cannot cross a Go method signature", parameters->items[i].name);
    if(contains_vec(c->module, fn->return_type, 0))
        error(c, fn->span, "Vec cannot cross a Go method signature", fn->name);
    if(contains_vec(c->module, base, 0))
        error(c, fn->span, "Vec cannot cross a Go method receiver", fn->name);
    if(fn->go_method_results) {
        const ZirType *result = FindType(c->module, fn->return_type, NULL);
        offset = 0;
        if(result == NULL || result->is_extern || result->is_enum ||
           result->is_union || result->is_procedure_type || result->is_map ||
           result->is_owned_vec || result->is_record_template ||
           TypeNextField(result, &offset, &field) != 1)
            error(c, fn->span, "#go_results method requires a nonempty concrete result record", fn->name);
        else {
            offset = 0;
            while(TypeNextField(result, &offset, &field) == 1)
                if(field.is_using || !strcmp(field.name, "_"))
                    error(c, fn->span, "#go_results method fields must be named without using", fn->name);
        }
    }
    return c->errors == errors_before;
}

static int
check_function_with_buffers(Checker *c, ZirFunction *fn, CheckFunctionBuffers *buffers)
{
    int errors_before = c->errors;
    int has_slots;
    int has_arrays;
    int n;
    c->fn = fn;
    if(contains_vec(c->module, fn->return_type, 0) && fn->is_extern)
        error(c, fn->span, "Vec cannot cross an extern signature", fn->name);
    select_lookup_file(c->module, fn->span);
    const ZirType *return_slot = FindType(c->module, c->fn->return_type, NULL);
    if(return_slot != NULL && return_slot->is_record_template) {
        Diagnostic(c->fn->span, "check.specialize",
                   "generic types require a concrete specialization: %s",
                   c->fn->return_type);
        return 0;
    }
    /* Source expressions need imported types to disambiguate casts. Saved
     * IR already contains the checked graph: type-check that graph directly
     * so its statement text cannot redefine program meaning. */
    if(!c->fn->from_ir)
        StructureFunction(c->fn, c->module);
    if(!reserve_compound_constants(c->module, c->fn)) {
        error(c, fn->span, "too many aggregate constant expressions",
              fn->name);
        return 0;
    }
restart:
    for(int expression = 0; expression < fn->expr_count; expression++)
        fn->exprs[expression].is_move = 0;
    while(c->restore_count > 0)
        free(c->restores[--c->restore_count].states);
    c->count = 0; c->depth = 0;
    c->using_rewritten = 0;
    c->aggregate_rewritten = 0;
    if(!fn->from_ir || fn->is_specialization)
        for(int i = 0; i < c->module->using_count; i++) {
            const ZirUsing *using = &c->module->usings[i];
            if(!in_lookup_file(c->module, using->is_file_private,
                               using->span))
                continue;
            activate_using_filtered(c, using->path, using->filter,
                                    using->span);
        }
    has_slots = 0;
    has_arrays = fn->return_type[0] == '[';
    fn->uses_host = fn->is_extern && fn->extern_kind == ZIR_EXTERN_HOST;
    n = *skip_ws(FunctionArgs(c->fn)) ? split_top_level(FunctionArgs(c->fn), buffers->params[0], 64, sizeof(buffers->params[0])) : 0;
    for(int a = 0; a < n; a++) {
        char *colon = strchr(buffers->params[a], ':');
        if(colon) {
            *colon++ = 0; trim_in_place(buffers->params[a]); trim_in_place(colon);
            const ZirType *parameter_type = FindType(c->module, colon, NULL);
            if(parameter_type != NULL && parameter_type->is_record_template) {
                Diagnostic(c->fn->span, "check.specialize",
                           "generic types require a concrete specialization: %s",
                           colon);
                return 0;
            }
            has_slots |= parameter_type != NULL && parameter_type->is_procedure_type;
            if(pointer_to_array(colon))
                error(c, fn->span, "pointers to arrays are not supported; pass the element pointer", buffers->params[a]);
            if(contains_vec(c->module, colon, 0) && fn->is_extern)
                error(c, fn->span, "Vec cannot cross an extern signature", buffers->params[a]);
            has_arrays |= ArrayValueType(colon) || SliceElementType(colon, NULL, 0);
            bind(c, buffers->params[a], colon, c->fn->span);
            if((!fn->from_ir || fn->is_specialization) &&
               (fn->using_parameters & (UINT64_C(1) << a)))
                activate_using(c, buffers->params[a], fn->span);
        } else error(c, c->fn->span, "parameters require name: type", buffers->params[a]);
    }
    c->restore_count = 0;
    for(int i = 0; i < c->fn->stmt_count; i++) {
        ZirStmt *st = &c->fn->stmts[i];
        int errors_at_statement;
        c->current_stmt = st;
        const char *type;
        if(st->kind == ZIR_STMT_IF && c->restore_count < 64) {
            /* Moves inside an if whose every arm returns never reach the
             * join; the state after it is the state before it. */
            int last = i;
            if(all_arms_return(fn, i, &last)) {
                MoveState *states = calloc((size_t)(c->count > 0 ?
                                                      c->count : 1),
                                            sizeof(*states));
                if(states != NULL) {
                    for(int b = 0; b < c->count; b++) {
                        states[b].moved = c->bindings[b].moved;
                        states[b].moved_path_count =
                            c->bindings[b].moved_path_count;
                        memcpy(states[b].moved_paths,
                               c->bindings[b].moved_paths,
                               sizeof(states[b].moved_paths));
                    }
                    c->restores[c->restore_count].at = last;
                    c->restores[c->restore_count].count = c->count;
                    c->restores[c->restore_count].states = states;
                    c->restore_count++;
                }
            }
        }
        for(int r = 0; r < c->restore_count; r++) {
            if(c->restores[r].at != i)
                continue;
            for(int b = 0; b < c->restores[r].count && b < c->count; b++) {
                c->bindings[b].moved =
                    c->restores[r].states[b].moved != 0;
                c->bindings[b].moved_path_count =
                    c->restores[r].states[b].moved_path_count;
                memcpy(c->bindings[b].moved_paths,
                       c->restores[r].states[b].moved_paths,
                       sizeof(c->bindings[b].moved_paths));
            }
            free(c->restores[r].states);
            c->restores[r] = c->restores[c->restore_count - 1];
            c->restore_count--;
            r--;
        }
        if(st->kind == ZIR_STMT_BLOCK_CLOSE) {
            while(c->count && c->bindings[c->count - 1].depth == c->depth) {
                Binding *popping = &c->bindings[c->count - 1];
                if(popping->borrows_index >= 0 &&
                   popping->borrows_index < c->count - 1)
                    c->bindings[popping->borrows_index].borrow_count--;
                c->count--;
            }
            if(c->depth) c->depth--;
        }
        if((!fn->from_ir || fn->is_specialization) &&
           st->is_using && st->kind == ZIR_STMT_EXPR) {
            activate_using_filtered(c, st->name, st->type, st->span);
            expression_type(c, st->expr_root);
            continue;
        }
        if(!fn->from_ir || fn->is_specialization) {
            promote_using_tree(c, st->lhs_root);
            promote_using_tree(c, st->expr_root);
        }
        /* An unresolvable type_of is reported; the initializer's type
         * stands in so later uses of the name do not fail too. */
        if(st->kind == ZIR_STMT_DECL) {
            char declared[ZIR_NAME_MAX];
            copy_text(declared, sizeof(declared), st->type);
            int resolved = resolve_declared_type_of(c, declared, sizeof(declared),
                                                    st->span);
            st = &c->fn->stmts[i];
            st->type = resolved ? KeepName(declared) : "";
        }
        st = &c->fn->stmts[i];
        if(st->kind == ZIR_STMT_ASSIGN && strcmp(st->assignment_op, "=") &&
           (!fn->from_ir || fn->is_specialization))
            compound_operator_assignment(c, st);
        if(st->kind == ZIR_STMT_DECL)
            st->type = normalized_array(c->module, st->type);
        if(st->kind == ZIR_STMT_DECL)
            contextual_slot(c, st->expr_root, st->type);
        if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0 &&
           c->fn->exprs[st->lhs_root].kind == ZIR_EXPR_IDENT)
            contextual_slot(c, st->expr_root, lookup(c, c->fn->exprs[st->lhs_root].name));
        else if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0 &&
                (c->fn->exprs[st->lhs_root].kind == ZIR_EXPR_MEMBER ||
                 c->fn->exprs[st->lhs_root].kind == ZIR_EXPR_POINTER_MEMBER))
            contextual_slot(c, st->expr_root, expression_type(c, st->lhs_root));
        c->expected_type[0] = '\0';
        if(st->kind == ZIR_STMT_DECL)
            copy_text(c->expected_type, sizeof(c->expected_type), st->type);
        else if(st->kind == ZIR_STMT_RETURN)
            copy_text(c->expected_type, sizeof(c->expected_type), c->fn->return_type);
        else if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0) {
            c->assign_destination = 1;
            copy_text(c->expected_type, sizeof(c->expected_type),
                      expression_type(c, st->lhs_root));
            c->assign_destination = 0;
        }
        type = expression_type(c, st->expr_root);
        c->expected_type[0] = '\0';
        errors_at_statement = c->errors;
        if(st->kind == ZIR_STMT_EXPR || st->kind == ZIR_STMT_UNUSED) {
            int discarded = discarded_must_call(c, st->expr_root);
            if(discarded >= 0) {
                const ZirExpr *call = &fn->exprs[discarded];
                error(c, call->span, "#must return value is ignored",
                      call->name);
            }
        }
        if(st->kind == ZIR_STMT_IF_CASE) {
            const ZirType *enumeration = FindType(c->module, type, NULL);
            int complete_case = starts_word(skip_ws(st->text + 2), "#complete");
            if(fn->from_ir || c->errors != errors_before ||
               !starts_word(st->text, "if") ||
               strstr(st->text, "==") == NULL ||
               (enumeration == NULL && !scalar_case_type(type)) ||
               (enumeration != NULL && !enumeration->is_enum) ||
               (enumeration == NULL && complete_case)) {
                if_case_error(c, st->span,
                              "if-case requires a checked enum or scalar value", type);
                return 0;
            }
            if(!lower_if_case(c, i, type)) return 0;
            StructureFunction(fn, c->module);
            goto restart;
        }
        c->destination_was_moved = -1;
        if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0) {
            Binding *destination = lexical_owned_binding(c, st->lhs_root);
            c->destination_was_moved = destination != NULL && destination->moved;
        }
        if(c->errors == errors_at_statement && st->expr_root >= 0) {
            Binding *moved_from = lexical_owned_binding(c, st->expr_root);
            int consumed_root = 0;
            if(moved_from != NULL && moved_from->moved_path_count != 0 &&
               (st->kind == ZIR_STMT_DECL || st->kind == ZIR_STMT_ASSIGN ||
                st->kind == ZIR_STMT_RETURN)) {
                error(c, st->span,
                      "owned aggregate is used after moving one of its fields",
                      moved_from->name);
                consumed_root = 1;
            }
            if(moved_from != NULL && moved_from->moved_path_count == 0 &&
               (st->kind == ZIR_STMT_DECL || st->kind == ZIR_STMT_ASSIGN ||
                st->kind == ZIR_STMT_RETURN)) {
                if(moved_from->borrow_count > 0)
                    error(c, st->span,
                          "cannot move a Vec with a live borrowed view",
                          moved_from->name);
                moved_from->moved = 1;
                c->fn->exprs[st->expr_root].is_move = 1;
                consumed_root = 1;
            }
            if(!consumed_root &&
               (st->kind == ZIR_STMT_DECL || st->kind == ZIR_STMT_ASSIGN ||
                st->kind == ZIR_STMT_RETURN))
                consumed_root = mark_moved_member_path(c, st->expr_root);
            if(!consumed_root) {
                mark_expr_moves(c, st->expr_root);
                reject_read_of_moved(c, st->expr_root, st->expr_root);
            }
            int root_transfer =
                (st->kind == ZIR_STMT_DECL || st->kind == ZIR_STMT_ASSIGN ||
                 st->kind == ZIR_STMT_RETURN) &&
                contains_vec(c->module, fn->exprs[st->expr_root].type, 0);
            int root_discard = st->kind == ZIR_STMT_EXPR ||
                               st->kind == ZIR_STMT_UNUSED;
            check_vec_call_results(c, st->expr_root, root_transfer,
                                   root_discard);
            if(root_discard &&
               contains_vec(c->module, fn->exprs[st->expr_root].type, 0) &&
               fn->exprs[st->expr_root].kind != ZIR_EXPR_IDENT &&
               fn->exprs[st->expr_root].kind != ZIR_EXPR_CALL)
                error(c, st->span,
                      "discarded Vec value must be a binding or call result",
                      st->text);
            /* An assignment into a moved-from Vec binding re-owns it with
             * the transferred or fresh value. The handoff form
             * `v = Take(v)` also re-owns: the right side moved this binding
             * into the call and stores the result back into it. */
            if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0) {
                Binding *destination = lexical_owned_binding(c, st->lhs_root);
                if(destination != NULL) {
                    if(destination->moved)
                        c->destination_was_moved = 1;
                    destination->moved = 0;
                }
            }
        }
        if(st->kind == ZIR_STMT_DECL) {
            /* x := F() binds F's first result; the bindings generated for
             * a, b := F() (named results_N_) keep the whole record. */
            size_t name_length = strlen(st->name);
            int results_binding = !strncmp(st->name, "results_", 8) &&
                                  name_length > 9 && st->name[name_length - 1] == '_';
            if(!*st->type && !results_binding) {
                const char *first = select_first_result(c, st->expr_root);
                if(first != NULL)
                    type = first;
            }
            if(!*st->type) st->type = KeepName(!strcmp(type, "integer") ? "s64" : !strcmp(type, "real") ? "float64" : type);
            else if(!compatible_checked(c, st->type, type)) {
                const char *converted = try_conversion(c, st->expr_root,
                                                       st->type, st->span);
                if(converted != NULL)
                    type = converted;
                else {
                    char detail[ZIR_TEXT_MAX];
                    error(c, st->span, "initializer type mismatch",
                          mismatch_detail(detail, sizeof(detail), st->name,
                                          st->type, type));
                }
            }
            if(!strcmp(st->type, "null"))
                error(c, st->span, "null requires an explicit pointer type", st->name);
            if(st->type[0] == '[') {
                const char *problem = local_storage_error(c->module, st->type);
                if(problem != NULL)
                    error(c, st->span, problem, st->name);
            }
            const ZirType *local_type = FindType(c->module, st->type, NULL);
            if(local_type == NULL && st->type[0] != '[' &&
               !TargetType(st->type, ZIR_C) &&
               !SliceElementType(st->type, NULL, 0)) {
                const char *base = st->type;
                int resolvable = 0;
                while(*base == '*')
                    base++;
                if(TargetType(base, ZIR_C) != NULL ||
                   FindType(c->module, base, NULL) != NULL)
                    resolvable = 1;
                if(!resolvable)
                    error(c, st->span, "unknown declaration type", st->type);
            }
            if(local_type != NULL && local_type->is_record_template)
                error(c, st->span,
                      "generic types require a concrete specialization",
                      st->type);
            if(local_type != NULL && local_type->is_procedure_type) {
                has_slots = 1;
                if(st->expr_root < 0) {
                    Diagnostic(st->span, "check.slot_initializer", "slot bindings require an initializer");
                    c->failed = 1;
                }
            }
            if(st->expr_root >= 0 &&
               contains_vec(c->module, st->type, 0) &&
               c->fn->exprs[st->expr_root].kind != ZIR_EXPR_IDENT &&
               c->fn->exprs[st->expr_root].kind != ZIR_EXPR_CALL &&
               !owned_initializer_shape(c, st->expr_root))
                error(c, st->span,
                      "Vec initialization moves a binding or takes a call result",
                      st->name);
            if(st->expr_root >= 0 &&
               contains_vec(c->module, st->type, 0) &&
               global_vec_source(c, st->expr_root))
                error(c, st->span,
                      "global Vec storage cannot move; use a local",
                      st->name);
            bind(c, st->name, st->type, st->span);
            /* A declared VecSlice view borrows its source until the view's
             * scope closes; the source cannot move or mutate meanwhile. */
            if(st->expr_root >= 0 &&
               c->fn->exprs[st->expr_root].kind == ZIR_EXPR_CALL &&
               !strcmp(c->fn->exprs[st->expr_root].name, "VecSlice") &&
               SliceElementType(st->type, NULL, 0)) {
                int vector = c->fn->exprs[st->expr_root].first_child;
                if(vector >= 0 &&
                   c->fn->exprs[vector].kind == ZIR_EXPR_IDENT) {
                    for(int b = c->count - 2; b >= 0; b--)
                        if(!c->bindings[b].is_using_namespace &&
                           !strcmp(c->bindings[b].name,
                                   c->fn->exprs[vector].name) &&
                           owned_vec_binding_type(c, c->bindings[b].type)) {
                            c->bindings[c->count - 1].borrows_index = b;
                            c->bindings[b].borrow_count++;
                            break;
                        }
                }
            }
            if((!fn->from_ir || fn->is_specialization) && st->is_using)
                activate_using_filtered(c, st->name, st->type, st->span);
        } else if(st->kind == ZIR_STMT_ASSIGN) {
            c->assign_destination = 1;
            const char *lhs = expression_type(c, st->lhs_root);
            c->assign_destination = 0;
            if(contains_vec(c->module, lhs, 0)) {
                if(c->fn->exprs[st->lhs_root].kind != ZIR_EXPR_IDENT)
                    error(c, st->span,
                          "Vec assignment requires a simple binding destination",
                          st->text);
                else if(lexical_owned_binding(c, st->lhs_root) == NULL)
                    error(c, st->span,
                          "global Vec assignment is not supported; move it into a local first",
                          st->text);
                else if(st->expr_root >= 0 &&
                        c->fn->exprs[st->expr_root].kind != ZIR_EXPR_IDENT &&
                        c->fn->exprs[st->expr_root].kind != ZIR_EXPR_CALL &&
                        !owned_initializer_shape(c, st->expr_root))
                    error(c, st->span,
                          "Vec assignment moves a binding or takes a call result",
                          st->text);
                else if(st->expr_root >= 0 &&
                        global_vec_source(c, st->expr_root))
                    error(c, st->span,
                          "global Vec storage cannot move; use a local",
                          st->text);
                else if(c->destination_was_moved == 0)
                    error(c, st->span,
                          "assignment over an owned Vec leaks it; free or move it first",
                          c->fn->exprs[st->lhs_root].name);
            }
            const ZirType *destination = FindType(c->module, lhs, NULL);
            if(lhs[0] == '[' && strcmp(st->assignment_op, "="))
                error(c, st->span, "array compound assignment is not supported", st->assignment_op);
            if(destination != NULL && destination->is_enum &&
               !destination->is_enum_flags && strcmp(st->assignment_op, "="))
                error(c, st->span, "enum compound assignment requires an explicit numeric cast", st->assignment_op);
            if(destination != NULL && destination->is_enum_flags &&
               strcmp(st->assignment_op, "=") &&
               strcmp(st->assignment_op, "|=") &&
               strcmp(st->assignment_op, "&=") &&
               strcmp(st->assignment_op, "^=") &&
               strcmp(st->assignment_op, "+=") &&
               strcmp(st->assignment_op, "-="))
                error(c, st->span, "unsupported enum_flags compound assignment", st->assignment_op);
            if(text_type(lhs) && strcmp(st->assignment_op, "="))
                error(c, st->span, "string compound assignment is not supported", st->assignment_op);
            if(!assignable(c, st->lhs_root)) error(c, st->span, "assignment requires an assignable destination", "");
            if(readonly_text_destination(c, st->lhs_root))
                error(c, st->span, "collection count and borrowed string bytes are read-only", "");
            if(!compatible_checked(c, lhs, type)) {
                const char *converted = try_conversion(c, st->expr_root, lhs,
                                                       st->span);
                if(converted != NULL)
                    type = converted;
                else {
                    char detail[ZIR_TEXT_MAX];
                    error(c, st->span, "assignment type mismatch",
                          mismatch_detail(detail, sizeof(detail), st->text,
                                          lhs, type));
                }
            }
        } else if(st->kind == ZIR_STMT_RETURN) {
            if(contains_vec(c->module, c->fn->return_type, 0) &&
               st->expr_root >= 0 &&
               fn->exprs[st->expr_root].kind != ZIR_EXPR_IDENT &&
               fn->exprs[st->expr_root].kind != ZIR_EXPR_CALL &&
               !owned_initializer_shape(c, st->expr_root))
                error(c, st->span,
                      "owned return moves a binding or takes a call result",
                      c->fn->name);
            if(!compatible_checked(c, c->fn->return_type, type)) {
                const char *converted = try_conversion(c, st->expr_root,
                                                       c->fn->return_type,
                                                       st->span);
                if(converted != NULL)
                    type = converted;
                else {
                    char detail[ZIR_TEXT_MAX];
                    char first[ZIR_NAME_MAX];
                    const char *dot = strrchr(type, '.');
                    if(results_first_type(c, c->fn->return_type, first, sizeof(first)) &&
                       dot != NULL && !strcmp(dot + 1, c->fn->return_type))
                        error(c, st->span,
                              "results from another module must be bound before "
                              "they are returned; write a, b := F(); return a, b",
                              c->fn->name);
                    else
                        error(c, st->span, "return type mismatch",
                              mismatch_detail(detail, sizeof(detail), c->fn->name,
                                              c->fn->return_type, type));
                }
            }
            if((st->expr_root < 0) != !strcmp(c->fn->return_type, "void"))
                error(c, st->span, "return value does not match function signature", c->fn->name);
            if(contains_vec(c->module, c->fn->return_type, 0) &&
               global_vec_source(c, st->expr_root))
                error(c, st->span,
                      "global Vec storage cannot move; use a local",
                      c->fn->exprs[st->expr_root].name);
        } else if(st->kind == ZIR_STMT_IF || st->kind == ZIR_STMT_WHILE) {
            if(st->expr_root < 0 &&
               (st->kind == ZIR_STMT_WHILE || !st->is_else ||
                starts_word(skip_ws(st->text + 4), "if")))
                error(c, st->span, "condition requires an expression", st->text);
            if(*type && strcmp(type, "bool")) error(c, st->span, "condition requires bool", type);
        } else if(st->kind == ZIR_STMT_UNKNOWN || st->kind == ZIR_STMT_FOR) {
            error(c, st->span, "statement is not supported by language checking", st->text);
        }
        if(st->kind == ZIR_STMT_BLOCK_OPEN || st->kind == ZIR_STMT_IF ||
           st->kind == ZIR_STMT_WHILE || st->kind == ZIR_STMT_FOR)
            c->depth++;
    }
    for(int i = 0; i < fn->expr_count; i++) {
        const ZirExpr *call = &fn->exprs[i];
        has_arrays |= SliceElementType(call->type, NULL, 0);
        if(call->kind != ZIR_EXPR_CALL)
            continue;
        const ZirFunction *callee = NULL;
        const ZirModule *owner = NULL;
        if(ResolveFunction(c->module, call->name, &owner, &callee) <= 0 ||
           callee == NULL || callee->is_extern)
            continue;
        has_arrays |= ArrayValueType(callee->return_type);
        int count = *skip_ws(FunctionArgs(callee)) ?
            split_top_level(FunctionArgs(callee), buffers->parameters[0], 64, sizeof(buffers->parameters[0])) : 0;
        for(int parameter = 0; parameter < count; parameter++) {
            const char *colon = strchr(buffers->parameters[parameter], ':');
            if(colon != NULL)
                has_arrays |= ArrayValueType(skip_ws(colon + 1));
        }
    }
    if(!fn->is_extern && strcmp(fn->return_type, "void") &&
       !sequence_returns(fn, 0, fn->stmt_count))
        error(c, fn->span, "missing return: every path must return a value", fn->name);
    if(c->conversions_applied &&
       !rebuild_conversion_layout(fn)) {
        error(c, fn->span, "cannot reorder #as conversion graph", fn->name);
        return 0;
    }
    c->conversions_applied = 0;
    if(!validate_loop_targets(c, fn))
        return 0;
    if((c->using_rewritten || c->aggregate_rewritten) &&
       !order_using_expressions(fn)) {
        error(c, fn->span, "cannot order checked expressions", fn->name);
        return 0;
    }
    c->fn->checked = c->errors == errors_before;
    for(int expression = 0; expression < c->fn->expr_count; expression++)
        has_slots |= c->fn->exprs[expression].is_function_value;
    /* Portable scalar emission classifies functions for bundles; a body the
     * emitter cannot express is excluded there and re-verified by the bundle
     * gate. Type checking itself stays complete, so native-only programs
     * with host-bound state (raw pointers, foreign handles) check clean. */
    if((has_slots || has_arrays) && !c->fn->is_extern && !c->fn->checked)
        c->fn->checked = 0;
    if(c->fn->checked && !c->fn->is_extern && !CanEmitBody(c->module, c->fn))
        c->fn->checked = 0;
    if(c->fn->checked) {
        fn->using_parameters = 0;
        for(int i = 0; i < fn->stmt_count; i++)
            fn->stmts[i].is_using = 0;
    }
    return !c->failed;
}

int
check_function(Checker *c, ZirFunction *fn)
{
    static _Thread_local CheckFunctionBuffers *spares[16];
    static _Thread_local int spare_count;
    CheckFunctionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = check_function_with_buffers(c, fn, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

int
check_template_declaration(Checker *c, ZirFunction *fn)
{
    c->fn = fn;
    select_lookup_file(c->module, fn->span);
    if(!fn->from_ir)
        StructureFunction(fn, c->module);
    if(!fn->template_param[0] || fn->is_extern || fn->exported ||
       strchr(fn->return_type, '$') != NULL) {
        Diagnostic(fn->span, "check.template",
                   "invalid polymorphic procedure declaration");
        return 0;
    }
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    int count = parameters->count;
    int binders = 0, valid = count > 0;
    uint64_t allowed_using = count >= 64 ? UINT64_MAX :
                             (UINT64_C(1) << count) - 1;
    valid = valid && (fn->using_parameters & ~allowed_using) == 0;
    for(int i = 0; i < count && valid; i++) {
        const char *type = parameters->items[i].type;
        if(type == NULL) { valid = 0; break; }
        int prefix = TemplateBinderPrefix(type);
        if(prefix >= 0) {
            if(TemplateParameterIndex(fn->template_param, type + prefix + 1,
                                      strlen(type + prefix + 1)) < 0) valid = 0;
            else binders++;
            if(type[0] == '[' && type[1] != ']') {
                int capacity;
                if(array_capacity(c->module, type, &capacity) != 1) {
                    Diagnostic(fn->span, "check.template",
                               "polymorphic array parameter needs a resolved capacity");
                    return 0;
                }
            }
        } else if(strchr(type, '$') != NULL) {
            /* Binders inside a generic record application, Table($K, $V). */
            if(strchr(type, '(') == NULL || strchr(type, '$') < strchr(type, '('))
                valid = 0;
            else
                binders++;
        }
    }
    if(!valid || binders == 0) {
        Diagnostic(fn->span, "check.template",
                   "polymorphic procedure requires a direct $Type parameter");
        return 0;
    }
    for(int i = 0; i < fn->stmt_count; i++)
        if(fn->stmts[i].kind == ZIR_STMT_UNKNOWN ||
           fn->stmts[i].kind == ZIR_STMT_FOR) {
            Diagnostic(fn->stmts[i].span, "check.template",
                       "unsupported statement in polymorphic procedure");
            return 0;
        }
    for(int i = 0; i < fn->expr_count; i++)
        if(fn->exprs[i].kind == ZIR_EXPR_UNKNOWN) {
            Diagnostic(fn->exprs[i].span, "check.template",
                       "unsupported expression in polymorphic procedure");
            return 0;
        }
    return validate_loop_targets(c, fn);
}
/* Buffers normalize_function_arrays keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct NormalizeFunctionArraysBuffers {
    char parts[64][ZIR_TEXT_MAX];
    char arguments[ZIR_TEXT_MAX];
    char original_type[ZIR_TEXT_MAX];
    char normalized[ZIR_TEXT_MAX];
} NormalizeFunctionArraysBuffers;

int normalize_function_arrays(const ZirModule *module, ZirFunction *fn);

static int
template_signature_type(const ZirFunction *fn, const char *type)
{
    if(!fn->is_template) return 0;
    for(const char *cursor = type; *cursor;) {
        if(!isalpha((unsigned char)*cursor) && *cursor != '_') {
            cursor++;
            continue;
        }
        const char *start = cursor++;
        while(isalnum((unsigned char)*cursor) || *cursor == '_') cursor++;
        if(TemplateParameterIndex(fn->template_param, start,
                                  (size_t)(cursor - start)) >= 0)
            return 1;
    }
    return 0;
}

static int
normalize_function_arrays_with_buffers(const ZirModule *module, ZirFunction *fn, NormalizeFunctionArraysBuffers *buffers)
{
    size_t used = 0;
    int array_arguments = strchr(FunctionArgs(fn), '[') != NULL;
    int argument_alias_changed = 0;
    int count = *skip_ws(FunctionArgs(fn)) ?
        split_top_level(FunctionArgs(fn), buffers->parts[0], 64, sizeof(buffers->parts[0])) : 0;
    buffers->arguments[0] = '\0';
    for(int i = -1; i < count; i++) {
        char *type = fn->return_type;
        size_t capacity = sizeof(fn->return_type);
        if(i >= 0) {
            char *colon = strchr(buffers->parts[i], ':');
            if(colon == NULL) {
                Diagnostic(fn->span, "check.signature", "parameters require name: type: %s", buffers->parts[i]);
                return 0;
            }
            type = colon + 1;
            trim_in_place(type);
            capacity = sizeof(buffers->parts[i]) - (size_t)(type - buffers->parts[i]);
        }
        copy_text(buffers->original_type, sizeof(buffers->original_type), type);
        if(fn->is_template)
            normalize_template_array(module, type, capacity, fn->template_param);
        else
            normalize_array(module, type, capacity);
        if(i >= 0 && strcmp(buffers->original_type, type) != 0)
            argument_alias_changed = 1;
        int host_buffer = i >= 0 && ArrayElementType(type, NULL, 0, NULL) &&
                          !ArrayValueType(type);
        /* Array signatures containing a bound type, including `[N]T`
         * returns, are checked once specialized. */
        if(type[0] == '[' && !host_buffer && strchr(type, '$') == NULL &&
           !template_signature_type(fn, type)) {
            const char *problem = local_storage_error(module, type);
            if(problem == NULL && fn->is_extern &&
               (i < 0 || !SliceElementType(type, NULL, 0)))
                problem = "direct array signatures require an ordinary Ziran function";
            int bound = -1;
            if(problem == NULL && !SliceElementType(type, NULL, 0) &&
               array_capacity(module, type, &bound) != 1)
                problem = "array signatures require a resolved capacity";
            if(problem != NULL) {
                Diagnostic(fn->span, "check.array_signature", "%s: %s", problem, type);
                return 0;
            }
        }
        if(i >= 0) {
            int length = snprintf(buffers->arguments + used, sizeof(buffers->arguments) - used,
                                  "%s%s", used ? ", " : "", buffers->parts[i]);
            if(length < 0 || (size_t)length >= sizeof(buffers->arguments) - used) {
                Diagnostic(fn->span, "check.array_signature", "function signature exceeds size limit");
                return 0;
            }
            used += (size_t)length;
        }
    }
    if(array_arguments || argument_alias_changed)
        fn->args_text = KeepParameters(buffers->arguments);
    if((array_arguments || argument_alias_changed) && FunctionDefaultArgs(fn)[0]) {
        char (*defaults)[ZIR_TEXT_MAX] = calloc(64, sizeof(*defaults));
        if(defaults == NULL) return 0;
        int default_count = split_top_level(FunctionDefaultArgs(fn), defaults[0],
                                            64, sizeof(defaults[0]));
        size_t written = 0;
        buffers->normalized[0] = '\0';
        if(default_count != count) {
            free(defaults);
            Diagnostic(fn->span, "check.signature",
                       "default arguments do not match procedure parameters");
            return 0;
        }
        for(int i = 0; i < count; i++) {
            char *assignment = top_level_assignment(defaults[i]);
            const char *default_value = assignment == NULL ? "" :
                                        skip_ws(assignment + 1);
            int length = snprintf(buffers->normalized + written,
                                  sizeof(buffers->normalized) - written,
                                  "%s%s%s%s", i ? ", " : "", buffers->parts[i],
                                  assignment == NULL ? "" : " = ",
                                  default_value);
            if(length < 0 || (size_t)length >=
                             sizeof(buffers->normalized) - written) {
                free(defaults);
                Diagnostic(fn->span, "check.signature",
                           "default argument signature exceeds size limit");
                return 0;
            }
            written += (size_t)length;
        }
        fn->default_args_text = KeepParameters(buffers->normalized);
        free(defaults);
    }
    return 1;
}

/* Resolve every declaration before checking bodies, so imported and forward
 * calls compare the same array shapes regardless of traversal order. */
int
normalize_function_arrays(const ZirModule *module, ZirFunction *fn)
{
    static _Thread_local NormalizeFunctionArraysBuffers *spares[16];
    static _Thread_local int spare_count;
    NormalizeFunctionArraysBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = normalize_function_arrays_with_buffers(module, fn, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
