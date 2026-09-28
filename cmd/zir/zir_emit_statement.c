#include "zir_emit_internal.h"

static void emit_sequence(Emitter *e,int begin,int end);

static int
block_end(const ZirFunction *fn,int begin,int end)
{
    int depth=1;
    for(int i=begin+1;i<end;i++) {
        ZirStmtKind k=fn->stmts[i].kind;
        if(k==ZIR_STMT_IF||k==ZIR_STMT_WHILE||k==ZIR_STMT_BLOCK_OPEN)depth++;
        if(k==ZIR_STMT_BLOCK_CLOSE && !--depth)return i;
    }
    return end;
}

static int
parallel_region_at(const ZirFunction *fn, int while_index,
                   ParallelRegion *region)
{
    const ZirStmt *st = &fn->stmts[while_index];
    const ZirStmt *cursor_decl, *last_decl, *first_decl;
    const ZirStmt *binder_decl, *index_decl, *advance;
    int close = -1, depth = 1;
    if(st->kind != ZIR_STMT_WHILE || !st->is_parallel || while_index < 3)
        return 0;
    cursor_decl = &fn->stmts[while_index - 1];
    last_decl = &fn->stmts[while_index - 2];
    first_decl = &fn->stmts[while_index - 3];
    binder_decl = &fn->stmts[while_index + 1];
    index_decl = &fn->stmts[while_index + 2];
    if(cursor_decl->kind != ZIR_STMT_DECL ||
       last_decl->kind != ZIR_STMT_DECL ||
       first_decl->kind != ZIR_STMT_DECL ||
       binder_decl->kind != ZIR_STMT_DECL ||
       index_decl->kind != ZIR_STMT_DECL ||
       strcmp(index_decl->name, "it_index") != 0)
        return 0;
    for(int i = while_index + 1; i < fn->stmt_count; i++) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE ||
           kind == ZIR_STMT_FOR || kind == ZIR_STMT_BLOCK_OPEN ||
           kind == ZIR_STMT_IF_CASE)
            depth++;
        else if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0) {
            close = i;
            break;
        }
    }
    if(close < 0 || close - 1 <= while_index + 2)
        return 0;
    advance = &fn->stmts[close - 1];
    if(advance->kind != ZIR_STMT_ASSIGN ||
       advance->lhs_root < 0 ||
       fn->exprs[advance->lhs_root].kind != ZIR_EXPR_IDENT ||
       strcmp(fn->exprs[advance->lhs_root].name, cursor_decl->name))
        return 0;
    memset(region, 0, sizeof(*region));
    region->while_index = while_index;
    region->close = close;
    region->body_begin = while_index + 3;
    region->body_end = close - 1;
    /* The lowered tail carries the cursor advance and an inclusive-range
     * `if cursor == last { break; }` guard; the worker drives indices
     * itself, so both stay out of its body. */
    while(region->body_end > region->body_begin) {
        const ZirStmt *tail = &fn->stmts[region->body_end - 1];
        if(tail->kind == ZIR_STMT_ASSIGN && tail->lhs_root >= 0 &&
           fn->exprs[tail->lhs_root].kind == ZIR_EXPR_IDENT &&
           !strcmp(fn->exprs[tail->lhs_root].name, cursor_decl->name)) {
            region->body_end--;
            continue;
        }
        if(tail->kind == ZIR_STMT_BLOCK_CLOSE &&
           region->body_end - 2 > while_index &&
           fn->stmts[region->body_end - 2].kind == ZIR_STMT_BREAK &&
           fn->stmts[region->body_end - 3].kind == ZIR_STMT_IF) {
            const ZirStmt *guard = &fn->stmts[region->body_end - 3];
            int mentions_cursor = 0;
            int stack[64];
            int top = 0;
            if(guard->expr_root >= 0 && guard->expr_root < fn->expr_count)
                stack[top++] = guard->expr_root;
            while(top > 0 && !mentions_cursor) {
                const ZirExpr *expr = &fn->exprs[stack[--top]];
                if(expr->kind == ZIR_EXPR_IDENT &&
                   !strcmp(expr->name, cursor_decl->name))
                    mentions_cursor = 1;
                if(expr->left >= 0 && expr->left < fn->expr_count &&
                    top < 64)
                    stack[top++] = expr->left;
                if(expr->right >= 0 && expr->right < fn->expr_count &&
                    top < 64)
                    stack[top++] = expr->right;
                if(expr->third >= 0 && expr->third < fn->expr_count &&
                    top < 64)
                    stack[top++] = expr->third;
                for(int child = expr->first_child;
                    child >= 0 && child < fn->expr_count && top < 64;
                    child = fn->exprs[child].next_sibling)
                    stack[top++] = child;
            }
            if(mentions_cursor) {
                region->body_end -= 3;
                continue;
            }
        }
        break;
    }
    copy_text(region->first, sizeof(region->first), first_decl->name);
    copy_text(region->last, sizeof(region->last), last_decl->name);
    copy_text(region->cursor, sizeof(region->cursor), cursor_decl->name);
    copy_text(region->binder, sizeof(region->binder), binder_decl->name);
    region->forward = strstr(st->text, "<=") != NULL &&
                      strstr(st->text, ">=") == NULL;
    return 1;
}

static int
name_in_list(char list[][ZIR_NAME_MAX], int count, const char *name)
{
    for(int i = 0; i < count; i++)
        if(!strcmp(list[i], name))
            return 1;
    return 0;
}

static void
parallel_region_captures(const ZirModule *module, const ZirFunction *fn,
                         ParallelRegion *region)
{
    static char declared[64][ZIR_NAME_MAX];
    int declared_count = 0;
    if(region->body_end - region->body_begin > 4000)
        return;
    copy_text(declared[declared_count++], sizeof(declared[0]),
              region->binder);
    copy_text(declared[declared_count++], sizeof(declared[0]), "it_index");
    for(int i = region->body_begin; i < region->body_end &&
        declared_count < 60; i++) {
        const ZirStmt *st = &fn->stmts[i];
        if(st->kind == ZIR_STMT_DECL && st->name[0] &&
           !name_in_list(declared, declared_count, st->name)) {
            copy_text(declared[declared_count], sizeof(declared[0]),
                      st->name);
            declared_count++;
        }
    }
    for(int i = region->body_begin; i < region->body_end &&
        region->capture_count < 16; i++) {
        const ZirStmt *st = &fn->stmts[i];
        int roots[2] = {st->expr_root, st->lhs_root};
        for(int r = 0; r < 2; r++)
            for(int x = roots[r]; x >= 0 && x < fn->expr_count;
                x = fn->exprs[x].next_sibling) {
                const ZirExpr *expr = &fn->exprs[x];
                int global, taken, block_open, depth;
                const ZirStmt *decl = NULL;
                char type[ZIR_NAME_MAX];
                if(expr->kind != ZIR_EXPR_IDENT || !expr->name[0])
                    continue;
                if(name_in_list(declared, declared_count, expr->name) ||
                   !strcmp(expr->name, region->first) ||
                   !strcmp(expr->name, region->last) ||
                   !strcmp(expr->name, region->cursor))
                    continue;
                global = 0;
                for(int g = 0; g < module->global_count; g++)
                    if(!strcmp(module->globals[g].name, expr->name)) {
                        global = 1;
                        break;
                    }
                if(global)
                    continue;
                if(ResolveFunction(module, expr->name, NULL, NULL) == 1)
                    continue;
                block_open = -1;
                depth = 0;
                for(int s2 = region->while_index; s2 >= 0; s2--) {
                    ZirStmtKind kind = fn->stmts[s2].kind;
                    if(kind == ZIR_STMT_BLOCK_CLOSE)
                        depth++;
                    else if(kind == ZIR_STMT_BLOCK_OPEN ||
                            kind == ZIR_STMT_IF ||
                            kind == ZIR_STMT_WHILE ||
                            kind == ZIR_STMT_FOR ||
                            kind == ZIR_STMT_IF_CASE) {
                        if(depth == 0) {
                            block_open = s2;
                            break;
                        }
                        depth--;
                    }
                }
                for(int s2 = block_open; s2 >= 0 && decl == NULL; s2--) {
                    const ZirStmt *candidate = &fn->stmts[s2];
                    if(candidate->kind == ZIR_STMT_DECL &&
                       !strcmp(candidate->name, expr->name))
                        decl = candidate;
                }
                if(decl == NULL || !decl->type[0])
                    continue;
                taken = 0;
                for(int c = 0; c < region->capture_count; c++)
                    if(!strcmp(region->captures[c], expr->name)) {
                        taken = 1;
                        break;
                    }
                if(taken)
                    continue;
                copy_text(type, sizeof(type), decl->type);
                if(!strcmp(type, "null"))
                    continue;
                copy_text(region->captures[region->capture_count],
                          sizeof(region->captures[0]), expr->name);
                copy_text(region->capture_types[region->capture_count],
                          sizeof(region->capture_types[0]), type);
                region->capture_count++;
            }
    }
}

static int
parallel_region_fits(const ZirFunction *fn, int while_index)
{
    ParallelRegion region;
    return parallel_region_at(fn, while_index, &region) && region.forward;
}

static void
parallel_region_dispatch(Emitter *e, const ParallelRegion *region)
{
    char start_value[ZIR_TEXT_MAX], end_value[ZIR_TEXT_MAX];
    const ZirStmt *first_decl = &e->fn->stmts[region->while_index - 3];
    const ZirStmt *last_decl = &e->fn->stmts[region->while_index - 2];
    char ctx[ZIR_NAME_MAX];
    emit_expr(e, first_decl->expr_root, "s64", start_value,
              sizeof(start_value));
    emit_expr(e, last_decl->expr_root, "s64", end_value,
              sizeof(end_value));
    fresh(e, ctx);
    line(e, "{");
    e->indent++;
    line(e, "struct %s_ctx %s;", region->worker, ctx);
    line(e, "%s.start = %s;", ctx, start_value);
    line(e, "%s.end = %s;", ctx, end_value);
    {
        ParallelRegion full;
        parallel_region_at(e->fn, region->while_index, &full);
        parallel_region_captures(e->module, e->fn, &full);
        for(int c = 0; c < full.capture_count; c++)
            line(e, "%s.%s = %s;", ctx, full.captures[c],
                 full.captures[c]);
    }
    line(e, "ziran_parallel_run(%s_worker, %s.start, %s.end, &%s);",
         region->worker, ctx, ctx, ctx);
    e->indent--;
    line(e, "}");
}

int
FunctionHasParallelRegions(const ZirFunction *fn)
{
    for(int i = 0; i < fn->stmt_count; i++)
        if(fn->stmts[i].kind == ZIR_STMT_WHILE &&
           fn->stmts[i].is_parallel)
            return 1;
    return 0;
}

static void
emit_parallel_worker(Emitter *e, const ZirFunction *fn,
                     ParallelRegion *region, int ordinal)
{
    char function_name[ZIR_NAME_MAX];
    char mapped[ZIR_NAME_MAX * 2];
    const char *scalar = TargetType("s64", e->target);
    TargetBindingName(fn, e->target, fn->name, function_name,
                      sizeof(function_name));
    snprintf(region->worker, sizeof(region->worker), "ziran_par_%s_%d",
             function_name, ordinal);
    fprintf(e->out, "struct %s_ctx {\n", region->worker);
    fprintf(e->out, "    int64_t start;\n    int64_t end;\n");
    for(int c = 0; c < region->capture_count; c++) {
        char type_mapped[ZIR_NAME_MAX * 2];
        const char *cap = TargetType(region->capture_types[c], e->target);
        if(cap != NULL)
            copy_text(type_mapped, sizeof(type_mapped), cap);
        else
            e->resolve(e->context, region->capture_types[c], type_mapped,
                       sizeof(type_mapped));
        fprintf(e->out, "    %s %s;\n", type_mapped,
                region->captures[c]);
    }
    fprintf(e->out, "};\n\n");
    fprintf(e->out,
            "static void %s_worker(int64_t low, int64_t high, "
            "void *opaque)\n{\n", region->worker);
    e->indent = 1;
    fprintf(e->out, "    struct %s_ctx *ziran_ctx = "
            "(struct %s_ctx *)opaque;\n", region->worker,
            region->worker);
    copy_text(mapped, sizeof(mapped), scalar ? scalar : "int64_t");
    line(e, "%s %s = ziran_ctx->start;", mapped, region->first);
    line(e, "%s %s = ziran_ctx->end;", mapped, region->last);
    for(int c = 0; c < region->capture_count; c++) {
        char type_mapped[ZIR_NAME_MAX * 2];
        const char *cap = TargetType(region->capture_types[c], e->target);
        if(cap != NULL)
            copy_text(type_mapped, sizeof(type_mapped), cap);
        else
            e->resolve(e->context, region->capture_types[c], type_mapped,
                       sizeof(type_mapped));
        line(e, "%s %s = ziran_ctx->%s;", type_mapped,
             region->captures[c], region->captures[c]);
    }
    line(e, "for (int64_t ziran_i = low; ziran_i <= high; "
            "ziran_i += 1) {");
    e->indent++;
    line(e, "%s %s = ziran_i;", mapped, region->binder);
    line(e, "%s it_index = ziran_i - %s;", mapped, region->first);
    emit_sequence(e, region->body_begin, region->body_end);
    e->indent--;
    line(e, "}");
    fprintf(e->out, "}\n\n");
}

void
EmitParallelWorkers(FILE *out, const ZirModule *module,
                    const ZirFunction *fn, ZirTarget target,
                    ZirResolveTarget resolve, void *context)
{
    Emitter e = {0};
    int ordinal = 0;
    if(!FunctionHasParallelRegions(fn))
        return;
    e.out = out;
    e.module = module;
    e.fn = fn;
    e.target = target;
    e.resolve = resolve;
    e.context = context;
    e.indent = 0;
    e.locals = calloc((size_t)fn->stmt_count + 65, sizeof(*e.locals));
    if(e.locals == NULL)
        return;
    e.minify = zir_minify_output;
    fprintf(out, "#include \"ziran_parallel.h\"\n\n");
    fprintf(out, "/* ziran: no GPU device capability; #parallel_gpu regions "
                 "run on CPU threads. */\n\n");
    for(int i = 0; i < fn->stmt_count; i++) {
        ParallelRegion region;
        if(!parallel_region_at(fn, i, &region))
            continue;
        if(!region.forward) {
            fprintf(out, "/* ziran: reverse #parallel region keeps serial "
                         "execution%s */\n",
                    fn->stmts[i].is_gpu ? "; GPU region on CPU" : "");
            continue;
        }
        parallel_region_captures(module, fn, &region);
        emit_parallel_worker(&e, fn, &region, ordinal++);
    }
    free(e.locals);
}

void
zero_record(Emitter *e, const char *type, char *out, size_t size)
{
    if(e->target == ZIR_GO) {
        char target_type[ZIR_NAME_MAX * 2];
        e->resolve(e->context, type, target_type, sizeof(target_type));
        format(out, size, "%s{}", target_type);
        return;
    }
    copy_text(out, size, e->target == ZIR_CPP ||
              TypeHasZeroArray(e->module, type) ? "{}" : "{0}");
}

static int
emit_if(Emitter *e,int i,int end)
{
    char cond[ZIR_TEXT_MAX];
    int close=block_end(e->fn,i,end);
    char plain[ZIR_TEXT_MAX];
    emit_expr(e,e->fn->stmts[i].expr_root,"bool",cond,sizeof(cond));
    line(e,e->target==ZIR_GO?"if %s {":"if (%s) {",bare(cond,plain,sizeof(plain)));e->indent++;
    emit_sequence(e,i+1,close);e->indent--;
    if(close+1<end && e->fn->stmts[close+1].kind==ZIR_STMT_IF && e->fn->stmts[close+1].is_else) {
        int next=close+1;
        line(e,"} else {");e->indent++;
        if(e->fn->stmts[next].expr_root>=0)close=emit_if(e,next,end);
        else {close=block_end(e->fn,next,end);emit_sequence(e,next+1,close);}
        e->indent--;
    }
    line(e,"}");return close;
}

static int
expression_reads(const ZirFunction *fn, int index, const char *name)
{
    if(index < 0 || index >= fn->expr_count)
        return 0;
    const ZirExpr *expr = &fn->exprs[index];
    if(expr->kind == ZIR_EXPR_IDENT && !strcmp(expr->name, name))
        return 1;
    /* An array's count and data, including an array field's, can lower to
     * constants that never read the base. */
    if(expr->kind == ZIR_EXPR_MEMBER && expr->left >= 0 &&
       (!strcmp(expr->name, "count") || !strcmp(expr->name, "data")) &&
       ArrayElementType(fn->exprs[expr->left].type, NULL, 0, NULL))
        return 0;
    /* Slicing an empty array likewise lowers without its base; the bounds
     * are still read. */
    if(expr->kind == ZIR_EXPR_SLICE && expr->left >= 0 &&
       ArrayElementType(fn->exprs[expr->left].type, NULL, 0, NULL))
        return expression_reads(fn, expr->right, name) ||
               expression_reads(fn, expr->third, name);
    for(int child = expr->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        if(expression_reads(fn, child, name))
            return 1;
    return expression_reads(fn, expr->left, name) ||
           expression_reads(fn, expr->right, name) ||
           expression_reads(fn, expr->third, name);
}

/* Go rejects a local that is never read. A binding needs no `_ =` when a
 * later statement reads it and no other declaration reuses its name, so
 * the read cannot belong to a shadowing binding. Assigning to the whole
 * variable is not a read. */
static int
go_binding_read_later(const ZirFunction *fn, int declaration)
{
    const char *name = fn->stmts[declaration].name;
    int read = 0;
    for(int s = 0; s < fn->stmt_count; s++) {
        const ZirStmt *st = &fn->stmts[s];
        if(s != declaration && st->kind == ZIR_STMT_DECL && !strcmp(st->name, name))
            return 0;
        if(s <= declaration || read)
            continue;
        for(int root = st->expr_root; root >= 0 && !read; root = fn->exprs[root].next_sibling)
            read = expression_reads(fn, root, name);
        if(!read && st->lhs_root >= 0 && fn->exprs[st->lhs_root].kind != ZIR_EXPR_IDENT)
            read = expression_reads(fn, st->lhs_root, name);
    }
    return read;
}

static void
emit_sequence(Emitter *e,int begin,int end)
{
    int saved=e->local_count;e->depth++;
    e->sequence_terminated=0;
    for(int i=begin;i<end;i++) {
        const ZirStmt *st=&e->fn->stmts[i];
        char value[ZIR_TEXT_MAX],lhs[ZIR_TEXT_MAX],result[ZIR_TEXT_MAX];
        switch(st->kind) {
        case ZIR_STMT_DECL:
            if(st->expr_root>=0) {
                e->call_in_place = 1;
                emit_expr(e,st->expr_root,st->type,value,sizeof(value));
            }
            else if(record_type(e->module, st->type)) {
                zero_record(e, st->type, value, sizeof(value));
            }
            else copy_text(value, sizeof(value), zero_value(st->type, e->target));
            declare(e,st->name,st->type,value);
            if(e->target == ZIR_GO && !go_binding_read_later(e->fn, i)) {
                char binding[ZIR_NAME_MAX];
                TargetBindingName(e->fn, e->target, st->name, binding,
                                  sizeof(binding));
                line(e, "_ = %s", binding);
            }
            track_local(e, st->name, st->type);
            break;
        case ZIR_STMT_ASSIGN:
            emit_destination(e, st->lhs_root, lhs, sizeof(lhs));
            if(strcmp(st->assignment_op,"=")) {
                /* The target's old value reads in place unless a call on
                 * either side could run first or change it. */
                char old[ZIR_TEXT_MAX];
                if(expression_calls(e->fn, st->lhs_root) ||
                   expression_calls(e->fn, st->expr_root) ||
                   ArrayElementType(e->fn->exprs[st->lhs_root].type, NULL, 0, NULL)) {
                    fresh(e,old);
                    declare(e,old,e->fn->exprs[st->lhs_root].type,lhs);
                } else
                    copy_text(old, sizeof(old), lhs);
                emit_expr(e,st->expr_root,e->fn->exprs[st->lhs_root].type,value,sizeof(value));
                char op[4];copy_text(op,sizeof(op),st->assignment_op);op[strlen(op)-1]=0;
                const char *type=canonical(e->fn->exprs[st->lhs_root].type);
                if(width(type))number(e,type,old,type,value,e->fn->exprs[st->expr_root].type,operation(op),result,sizeof(result));
                else format(result,sizeof(result),"%s %s %s",old,op,value);
            } else {
                /* A call assigned to a name runs last, so it assigns directly. */
                e->call_in_place = e->fn->exprs[st->lhs_root].kind == ZIR_EXPR_IDENT;
                emit_expr(e,st->expr_root,e->fn->exprs[st->lhs_root].type,value,sizeof(value));
                copy_text(result,sizeof(result),value);
            }
            assign_value(e, lhs, e->fn->exprs[st->lhs_root].type, result);
            break;
        case ZIR_STMT_RETURN:
            if(st->expr_root >= 0) {
                e->call_in_place = 1;
                emit_expr(e, st->expr_root, e->fn->return_type, value, sizeof(value));
                if((e->target == ZIR_C || e->target == ZIR_CPP) &&
                   ArrayElementType(e->fn->return_type, NULL, 0, NULL)) {
                    char output[ZIR_NAME_MAX];
                    ArrayAbiName(e->fn, -1, output, sizeof(output));
                    /* value is a captured true array; output is an ABI pointer. */
                    line(e, "memmove(%s, %s, sizeof(%s));", output, value, value);
                    drop_locals(e, 0);
                    line(e, e->target != ZIR_GO && NativeMainReturnsStatus(e->fn) ?
                         "return 0;" : "return;");
                } else {
                    if(has_owned_locals(e)) {
                        char returned[ZIR_NAME_MAX];
                        fresh(e, returned);
                        declare(e, returned, e->fn->return_type, value);
                        drop_locals(e, 0);
                        copy_text(value, sizeof(value), returned);
                    }
                    char plain[ZIR_TEXT_MAX];
                    line(e, "return %s%s", bare(value, plain, sizeof(plain)),
                         e->target == ZIR_GO ? "" : ";");
                }
            }
            else {
                drop_locals(e, 0);
                line(e,e->target==ZIR_GO?"return":
                       NativeMainReturnsStatus(e->fn)?"return 0;":"return;");
            }
            e->local_count=saved;e->depth--;e->sequence_terminated=1;return;
        case ZIR_STMT_UNREACHABLE:
            drop_locals(e, 0);
            line(e,e->target==ZIR_GO?"panic(\"unreachable\")":"abort();");
            e->local_count=saved;e->depth--;e->sequence_terminated=1;return;
        case ZIR_STMT_IF:i=emit_if(e,i,end);break;
        case ZIR_STMT_WHILE: {
            int close=block_end(e->fn,i,end);
            int labeled=0;
            if(st->is_parallel && e->target==ZIR_GO)
                line(e, st->is_gpu ?
                    "// ziran: #parallel_gpu region downgraded to serial (no GPU device)" :
                    "// ziran: #parallel region downgraded to serial");
            if(st->is_parallel && (e->target==ZIR_C || e->target==ZIR_CPP)) {
                ParallelRegion region;
                if(parallel_region_at(e->fn,i,&region) && region.forward) {
                    char function_name[ZIR_NAME_MAX];
                    int ordinal = 0;
                    TargetBindingName(e->fn,e->target,e->fn->name,
                                     function_name,sizeof(function_name));
                    for(int s2=0;s2<i;s2++)
                        if(e->fn->stmts[s2].kind==ZIR_STMT_WHILE &&
                           e->fn->stmts[s2].is_parallel &&
                           parallel_region_fits(e->fn,s2))
                            ordinal++;
                    snprintf(region.worker,sizeof(region.worker),
                             "ziran_par_%s_%d", function_name, ordinal);
                    parallel_region_dispatch(e,&region);
                    i=close;break;
                }
                line(e,"/* ziran: #parallel region downgraded to serial */");
            }
            for(int target=0;target<e->fn->stmt_count;target++)
                if(st->loop_id && e->fn->stmts[target].target_id==st->loop_id)
                    labeled=1;
            if(labeled && e->target==ZIR_GO)
                line(e,"zir_loop_%d:",st->loop_id);
            {
                /* A condition that needs no setup statements goes in the
                 * loop header; otherwise it runs first in each iteration. */
                char *scratch_text = NULL;
                size_t scratch_size = 0;
                FILE *saved_out = e->out;
                int saved_serial = e->serial;
                FILE *scratch = open_memstream(&scratch_text, &scratch_size);
                int header = 0;
                if(scratch != NULL) {
                    e->out = scratch;
                    emit_expr(e,st->expr_root,"bool",value,sizeof(value));
                    fclose(scratch);
                    e->out = saved_out;
                    header = scratch_size == 0;
                    free(scratch_text);
                }
                if(header) {
                    char plain[ZIR_TEXT_MAX];
                    line(e,e->target==ZIR_GO?"for %s {":"while (%s) {",
                         bare(value,plain,sizeof(plain)));
                    e->indent++;
                } else {
                    e->serial = saved_serial;
                    line(e,e->target==ZIR_GO?"for {":"while (true) {");e->indent++;
                    emit_expr(e,st->expr_root,"bool",value,sizeof(value));
                    line(e,e->target==ZIR_GO?"if !%s { break }":"if (!%s) { break; }",value);
                }
            }
            if(labeled && e->target!=ZIR_GO) {
                line(e,"{");e->indent++;
            }
            if(e->loop_count >= (int)(sizeof(e->loop_start) / sizeof(e->loop_start[0]))) {
                Diagnostic(st->span, "emit.loop_nesting",
                           "too many nested loops during emission");
                exit(1);
            }
            e->loop_start[e->loop_count] = e->local_count;
            e->loop_id[e->loop_count++] = st->loop_id;
            emit_sequence(e,i+1,close);
            e->loop_count--;
            if(labeled && e->target!=ZIR_GO) {
                e->indent--;line(e,"}");
                line(e,"zir_loop_continue_%d: ;",st->loop_id);
            }
            e->indent--;line(e,"}");
            if(labeled && e->target!=ZIR_GO)
                line(e,"zir_loop_break_%d: ;",st->loop_id);
            i=close;break;
        }
        case ZIR_STMT_BLOCK_OPEN: {
            int close=block_end(e->fn,i,end);line(e,"{");e->indent++;emit_sequence(e,i+1,close);e->indent--;line(e,"}");i=close;break;
        }
        case ZIR_STMT_BREAK:case ZIR_STMT_CONTINUE:
            if(e->loop_count > 0) {
                int target = e->loop_count - 1;
                if(st->target_id)
                    while(target >= 0 && e->loop_id[target] != st->target_id)
                        target--;
                if(target >= 0)
                    drop_locals(e, e->loop_start[target]);
            }
            if(st->target_id) {
                if(e->target==ZIR_GO)
                    line(e,"%s zir_loop_%d",st->kind==ZIR_STMT_BREAK?"break":"continue",st->target_id);
                else
                    line(e,"goto zir_loop_%s_%d;",st->kind==ZIR_STMT_BREAK?"break":"continue",st->target_id);
            } else
                line(e,"%s%s",st->kind==ZIR_STMT_BREAK?"break":"continue",e->target==ZIR_GO?"":";");
            e->local_count=saved;e->depth--;e->sequence_terminated=1;return;
        case ZIR_STMT_EXPR:case ZIR_STMT_UNUSED:
            if(st->expr_root>=0) {
                const ZirExpr *expr = &e->fn->exprs[st->expr_root];
                emit_expr(e, st->expr_root, expr->type, value,
                          sizeof(value));
                if(*value && expr->kind == ZIR_EXPR_CALL &&
                   VecElementType(e->module, expr->type, NULL, 0)) {
                    char temporary[ZIR_NAME_MAX];
                    fresh(e, temporary);
                    declare(e, temporary, expr->type, value);
                    drop_temporary_vec(e, temporary);
                } else if(*value)
                    line(e,e->target==ZIR_GO?"_ = %s":"(void)%s;",value);
            }break;
        default:break;
        }
    }
    drop_locals(e, saved);
    e->local_count=saved;e->depth--;e->sequence_terminated=0;
}

int
EmitBody(FILE *out,const ZirModule *module,const ZirFunction *fn,ZirTarget target,
            ZirResolveTarget resolver,void *context)
{
    Emitter e={0};char params[64][ZIR_TEXT_MAX];int count;
    if(!CanEmitBody(module, fn))return 0;
    e.out=out;e.module=module;e.fn=fn;e.target=target;e.resolve=resolver;e.context=context;e.indent=1;
    e.minify = zir_minify_output;
    e.locals=calloc((size_t)fn->stmt_count+65,sizeof(*e.locals));
    if(!e.locals) {
        Diagnostic(fn->span, "emit.expression",
                   "out of memory during scalar emission");
        exit(1);
    }
    count=*skip_ws(fn->args)?split_top_level(fn->args,params[0],64,sizeof(params[0])):0;
    for(int i=0;i<count;i++) {
        char *colon=strchr(params[i],':');*colon++=0;trim_in_place(params[i]);
        const char *type=canonical(skip_ws(colon));
        if((target == ZIR_C || target == ZIR_CPP) &&
           ArrayValueType(type)) {
            char incoming[ZIR_NAME_MAX];
            char binding[ZIR_NAME_MAX];
            ArrayAbiName(fn, i, incoming, sizeof(incoming));
            TargetBindingName(fn, target, params[i], binding, sizeof(binding));
            declare_array(&e, binding, type, incoming);
        }
        track_local(&e, params[i], type);
    }
    emit_sequence(&e,0,fn->stmt_count);
    if(!e.sequence_terminated) {
        drop_locals(&e, 0);
        if(strcmp(fn->return_type, "void"))
            line(&e, target == ZIR_GO ? "panic(\"unreachable\")" : "abort();");
    }
    free(e.locals);return 1;
}
