#include "zir_emit_internal.h"

static void emit_sequence(Emitter *e,int begin,int end);
static int emit_switch(Emitter *e, int declaration, int end);

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
/* Buffers parallel_region_dispatch keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ParallelRegionDispatchBuffers {
    char start_value[ZIR_TEXT_MAX];
    char end_value[ZIR_TEXT_MAX];
} ParallelRegionDispatchBuffers;

static void parallel_region_dispatch(Emitter *e, const ParallelRegion *region);

static void
parallel_region_dispatch_with_buffers(Emitter *e, const ParallelRegion *region, ParallelRegionDispatchBuffers *buffers)
{
    const ZirStmt *first_decl = &e->fn->stmts[region->while_index - 3];
    const ZirStmt *last_decl = &e->fn->stmts[region->while_index - 2];
    char ctx[ZIR_NAME_MAX];
    emit_expr(e, first_decl->expr_root, "s64", buffers->start_value,
              sizeof(buffers->start_value));
    emit_expr(e, last_decl->expr_root, "s64", buffers->end_value,
              sizeof(buffers->end_value));
    fresh(e, ctx);
    line(e, "{");
    e->indent++;
    line(e, "struct %s_ctx %s;", region->worker, ctx);
    line(e, "%s.start = %s;", ctx, buffers->start_value);
    line(e, "%s.end = %s;", ctx, buffers->end_value);
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

static void
parallel_region_dispatch(Emitter *e, const ParallelRegion *region)
{
    static _Thread_local ParallelRegionDispatchBuffers *spares[16];
    static _Thread_local int spare_count;
    ParallelRegionDispatchBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    parallel_region_dispatch_with_buffers(e, region, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
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

/* Whether a condition lowers without statements of its own, so it can sit
 * in an else if header. */
static int
condition_in_header(Emitter *e, int root)
{
    char cond[ZIR_TEXT_MAX];
    char *scratch_text = NULL;
    size_t scratch_size = 0;
    FILE *saved_out = e->out;
    int saved_serial = e->serial, saved_locals = e->local_count;
    FILE *scratch = open_memstream(&scratch_text, &scratch_size);
    if(scratch == NULL)
        return 0;
    e->out = scratch;
    e->call_in_place = 1;
    emit_expr(e, root, "bool", cond, sizeof(cond));
    fclose(scratch);
    e->out = saved_out;
    free(scratch_text);
    e->serial = saved_serial;
    e->local_count = saved_locals;
    return scratch_size == 0;
}
/* Buffers emit_if keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitIfBuffers {
    char cond[ZIR_TEXT_MAX];
    char plain[ZIR_TEXT_MAX];
} EmitIfBuffers;

static int emit_if(Emitter *e,int i,int end,int chained);

static int
emit_if_with_buffers(Emitter *e,int i,int end,int chained, EmitIfBuffers *buffers)
{
    int close=block_end(e->fn,i,end);
    /* The condition runs last before the branch, so its calls stay in it. */
    e->call_in_place = 1;
    emit_expr(e,e->fn->stmts[i].expr_root,"bool",buffers->cond,sizeof(buffers->cond));
    bare(buffers->cond,buffers->plain,sizeof(buffers->plain));
    if(chained)
        line(e,e->target==ZIR_GO?"} else if %s {":"} else if (%s) {",buffers->plain);
    else
        line(e,e->target==ZIR_GO?"if %s {":"if (%s) {",buffers->plain);
    e->indent++;
    emit_sequence(e,i+1,close);e->indent--;
    if(close+1<end && e->fn->stmts[close+1].kind==ZIR_STMT_IF && e->fn->stmts[close+1].is_else) {
        int next=close+1;
        if(e->fn->stmts[next].expr_root>=0 &&
           condition_in_header(e, e->fn->stmts[next].expr_root))
            close=emit_if(e,next,end,1);
        else {
            line(e,"} else {");e->indent++;
            if(e->fn->stmts[next].expr_root>=0)close=emit_if(e,next,end,0);
            else {close=block_end(e->fn,next,end);emit_sequence(e,next+1,close);}
            e->indent--;
        }
    }
    if(!chained)
        line(e,"}");
    return close;
}

/* An if and its else branches. An else holding only another if whose
 * condition needs no setup continues the chain as else if; chained is set
 * for such a link, whose closing brace the first if writes. */
static int
emit_if(Emitter *e,int i,int end,int chained)
{
    static _Thread_local EmitIfBuffers *spares[16];
    static _Thread_local int spare_count;
    EmitIfBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = emit_if_with_buffers(e, i, end, chained, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
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

/* Whether an initializer already has the declared type, so a Go
 * declaration can drop the type and an enum one its conversion. Untyped
 * constants, null, and procedure values keep the written type. */
static int
typed_initializer(const Emitter *e, int root, const char *declared)
{
    const ZirExpr *expr = &e->fn->exprs[root];
    const char *type = canonical(expr->type);
    const ZirType *record = FindType(e->module, canonical(declared), NULL);
    if(expr->kind == ZIR_EXPR_INT || expr->kind == ZIR_EXPR_FLOAT ||
       !strcmp(type, "integer") || !strcmp(type, "real") || !strcmp(type, "null") ||
       strcmp(type, canonical(declared)) != 0 ||
       (record != NULL && record->is_procedure_type))
        return 0;
    return 1;
}

/* A step of an open native counting loop: its header steps instead. */
static int
native_step(const Emitter *e, const ZirStmt *st)
{
    if(!st->for_step)
        return 0;
    for(int i = 0; i < e->loop_count; i++)
        if(e->loop_native[i] && e->loop_id[i] == st->for_step)
            return 1;
    return 0;
}

/* Whether statements begin..end, other than the loop's own steps, read
 * name. */
static int
loop_body_reads(const Emitter *e, int begin, int end, int loop, const char *name)
{
    const ZirFunction *fn = e->fn;
    for(int s = begin; s < end; s++) {
        const ZirStmt *st = &fn->stmts[s];
        if(st->for_step == loop)
            continue;
        if(st->kind == ZIR_STMT_DECL && !strcmp(st->name, name))
            return 1;
        for(int root = st->expr_root; root >= 0; root = fn->exprs[root].next_sibling)
            if(expression_reads(fn, root, name))
                return 1;
        if(st->lhs_root >= 0 && expression_reads(fn, st->lhs_root, name))
            return 1;
    }
    return 0;
}
/* Buffers native_for keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct NativeForBuffers {
    char start[ZIR_TEXT_MAX];
    char condition[ZIR_TEXT_MAX];
    char plain[ZIR_TEXT_MAX];
} NativeForBuffers;

static int native_for(Emitter *e, int open, int close);

static int
native_for_with_buffers(Emitter *e, int open, int close, NativeForBuffers *buffers)
{
    const ZirFunction *fn = e->fn;
    const ZirStmt *counter, *loop;
    char counter_name[ZIR_NAME_MAX];
    const char *step = "++";
    int loop_close, saved_locals = e->local_count, saved_serial = e->serial;
    if(open + 2 >= close)
        return 0;
    counter = &fn->stmts[open + 1];
    loop = &fn->stmts[open + 2];
    if(counter->kind != ZIR_STMT_DECL || counter->expr_root < 0 ||
       loop->kind != ZIR_STMT_WHILE || !loop->for_form || loop->is_parallel)
        return 0;
    loop_close = block_end(fn, open + 2, close);
    if(loop_close + 1 != close)
        return 0;
    for(int s = open + 3; s < loop_close; s++)
        if(fn->stmts[s].for_step == loop->loop_id) {
            if(!strcmp(fn->stmts[s].assignment_op, "-="))
                step = "--";
            break;
        }
    TargetBindingName(fn, e->target, counter->name, counter_name, sizeof(counter_name));
    /* Header parts must not need statements of their own. */
    {
        char *scratch_text = NULL;
        size_t scratch_size = 0;
        FILE *saved_out = e->out;
        FILE *scratch = open_memstream(&scratch_text, &scratch_size);
        if(scratch == NULL)
            return 0;
        e->out = scratch;
        emit_expr(e, counter->expr_root, counter->type, buffers->start, sizeof(buffers->start));
        track_local(e, counter->name, counter->type);
        emit_expr(e, loop->expr_root, "bool", buffers->condition, sizeof(buffers->condition));
        fclose(scratch);
        e->out = saved_out;
        free(scratch_text);
        if(scratch_size != 0) {
            e->local_count = saved_locals;
            e->serial = saved_serial;
            return 0;
        }
    }
    bare(buffers->condition, buffers->plain, sizeof(buffers->plain));
    e->loop_header_binds = 0;
    if(loop->for_form == 2 && e->target == ZIR_GO && open + 3 < loop_close) {
        const ZirStmt *value = &fn->stmts[open + 3];
        const ZirExpr *index = value->kind == ZIR_STMT_DECL && value->expr_root >= 0 ?
            &fn->exprs[value->expr_root] : NULL;
        /* range over a string yields runes, not bytes, so strings count. */
        if(index != NULL && index->kind == ZIR_EXPR_INDEX &&
           fn->exprs[index->left].kind == ZIR_EXPR_IDENT &&
           strcmp(canonical(fn->exprs[index->left].type), "string") != 0 &&
           !call_can_change(e, index->left) &&
           !loop_body_reads(e, open + 4, loop_close, loop->loop_id, counter->name)) {
            char collection[ZIR_NAME_MAX], value_name[ZIR_NAME_MAX];
            resolve(e, fn->exprs[index->left].name, collection, sizeof(collection));
            TargetBindingName(fn, e->target, value->name, value_name, sizeof(value_name));
            if(loop_body_reads(e, open + 4, loop_close, loop->loop_id, value->name))
                format(e->loop_header, sizeof(e->loop_header), "for _, %s := range %s",
                       value_name, collection);
            else
                format(e->loop_header, sizeof(e->loop_header), "for range %s", collection);
            track_local(e, value->name, value->type);
            e->loop_header_binds = 1;
        }
    }
    if(!e->loop_header_binds) {
        if(e->target == ZIR_GO)
            format(e->loop_header, sizeof(e->loop_header), "for %s := %s(%s); %s; %s%s",
                   counter_name, TargetType(counter->type, e->target), buffers->start,
                   buffers->plain, counter_name, step);
        else
            format(e->loop_header, sizeof(e->loop_header), "for (%s %s = %s; %s; %s%s)",
                   TargetType(counter->type, e->target), counter_name, buffers->start,
                   buffers->plain, counter_name, step);
    }
    emit_sequence(e, open + 2, loop_close + 1);
    e->loop_header[0] = '\0';
    e->local_count = saved_locals;
    return 1;
}

/* A lowered for loop is a block holding its counter and a marked while:
 *     { step: s64 = 0; while step <= 2 { ...; step += 1 } }
 * C and Go write it as one counting loop, and Go walks a local collection
 * whose index goes unused with range. */
static int
native_for(Emitter *e, int open, int close)
{
    static _Thread_local NativeForBuffers *spares[16];
    static _Thread_local int spare_count;
    NativeForBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = native_for_with_buffers(e, open, close, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

/* Collects the constants an if-case arm compares its value with:
 * value == A, or value == A || value == B. */
static int
case_labels(const Emitter *e, int root, const char *value, int labels[], int *count)
{
    const ZirExpr *expr = &e->fn->exprs[root];
    if(expr->kind != ZIR_EXPR_BINARY)
        return 0;
    if(!strcmp(expr->op, "||"))
        return case_labels(e, expr->left, value, labels, count) &&
               case_labels(e, expr->right, value, labels, count);
    if(strcmp(expr->op, "==") || *count >= 16)
        return 0;
    const ZirExpr *left = &e->fn->exprs[expr->left];
    const ZirExpr *right = &e->fn->exprs[expr->right];
    if(left->kind != ZIR_EXPR_IDENT || strcmp(left->name, value))
        return 0;
    if(right->kind == ZIR_EXPR_CAST)
        right = &e->fn->exprs[right->right];
    if(right->kind != ZIR_EXPR_INT && right->kind != ZIR_EXPR_STRING)
        return 0;
    labels[(*count)++] = expr->right;
    return 1;
}

static int
contains_break(const ZirFunction *fn, int begin, int end)
{
    for(int s = begin; s < end; s++)
        if(fn->stmts[s].kind == ZIR_STMT_BREAK)
            return 1;
    return 0;
}
/* Buffers emit_switch keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitSwitchBuffers {
    char value[ZIR_TEXT_MAX];
    char plain[ZIR_TEXT_MAX];
    char text[ZIR_TEXT_MAX];
    char label[ZIR_TEXT_MAX];
} EmitSwitchBuffers;

static int emit_switch(Emitter *e, int declaration, int end);

static int
emit_switch_with_buffers(Emitter *e, int declaration, int end, EmitSwitchBuffers *buffers)
{
    const ZirFunction *fn = e->fn;
    const ZirStmt *decl = &fn->stmts[declaration];
    const char *type = canonical(decl->type);
    int arms[64], arm_count = 0, close = -1, last_else = -1;
    if(strncmp(decl->name, "case_value_", 11) || decl->expr_root < 0 ||
       declaration + 1 >= end || fn->stmts[declaration + 1].kind != ZIR_STMT_IF ||
       fn->stmts[declaration + 1].is_else)
        return -1;
    if(e->target != ZIR_GO && !width(type) && !enum_type(e->module, type))
        return -1;
    for(int at = declaration + 1; at < end && fn->stmts[at].kind == ZIR_STMT_IF &&
        (at == declaration + 1 || fn->stmts[at].is_else);) {
        int labels[16], count = 0;
        close = block_end(fn, at, end);
        if(fn->stmts[at].expr_root < 0) {
            last_else = at;
            if(contains_break(fn, at + 1, close))
                return -1;
            break;
        }
        if(arm_count >= 64 || !case_labels(e, fn->stmts[at].expr_root, decl->name, labels, &count) ||
           contains_break(fn, at + 1, close))
            return -1;
        arms[arm_count++] = at;
        at = close + 1;
    }
    if(arm_count < 2 || close < 0)
        return -1;
    for(int s = declaration + 2; s <= close; s++)
        for(int root = fn->stmts[s].expr_root; root >= 0; root = fn->exprs[root].next_sibling)
            if(fn->stmts[s].kind != ZIR_STMT_IF && expression_reads(fn, root, decl->name))
                return -1;
    e->call_in_place = 1;
    emit_expr(e, decl->expr_root, decl->type, buffers->value, sizeof(buffers->value));
    bare(buffers->value, buffers->plain, sizeof(buffers->plain));
    line(e, e->target == ZIR_GO ? "switch %s {" : "switch (%s) {", buffers->plain);
    for(int arm = 0; arm <= arm_count; arm++) {
        int at = arm < arm_count ? arms[arm] : last_else;
        int labels[16], count = 0, body_end;
        size_t used = 0;
        if(at < 0)
            break;
        body_end = block_end(fn, at, end);
        if(arm < arm_count) {
            case_labels(e, fn->stmts[at].expr_root, decl->name, labels, &count);
            for(int l = 0; l < count; l++) {
                emit_expr(e, labels[l], decl->type, buffers->label, sizeof(buffers->label));
                used += (size_t)format(buffers->text + used, sizeof(buffers->text) - used,
                                       e->target == ZIR_GO ? "%s%s" : "%scase %s:",
                                       l ? (e->target == ZIR_GO ? ", " : " ") : "",
                                       bare(buffers->label, buffers->plain, sizeof(buffers->plain)));
            }
            line(e, e->target == ZIR_GO ? "case %s:" : "%s", buffers->text);
        } else
            line(e, "default:");
        e->indent++;
        /* C allows no declaration straight after a label. */
        int block = e->target != ZIR_GO && body_end > at + 1 &&
                    fn->stmts[at + 1].kind == ZIR_STMT_DECL;
        if(block) {
            line(e, "{");
            e->indent++;
        }
        emit_sequence(e, at + 1, body_end);
        if(e->target != ZIR_GO && !e->sequence_terminated)
            line(e, "break;");
        if(block) {
            e->indent--;
            line(e, "}");
        }
        e->indent--;
    }
    line(e, "}");
    e->sequence_terminated = 0;
    return close;
}

/* An if-case lowers to case_value_N: T = value and an if / else if chain
 * comparing it with constants. That is a switch in C and Go. A break in an
 * arm would leave the switch instead of its loop, so such chains stay ifs,
 * and C switches only on integers. Returns the last statement written, or
 * -1 to write the statements as they are. */
static int
emit_switch(Emitter *e, int declaration, int end)
{
    static _Thread_local EmitSwitchBuffers *spares[16];
    static _Thread_local int spare_count;
    EmitSwitchBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = emit_switch_with_buffers(e, declaration, end, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers emit_sequence keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitSequenceBuffers {
    char value[ZIR_TEXT_MAX];
    char lhs[ZIR_TEXT_MAX];
    char result[ZIR_TEXT_MAX];
    char old[ZIR_TEXT_MAX];
    char plain[ZIR_TEXT_MAX];
} EmitSequenceBuffers;

static void emit_sequence(Emitter *e,int begin,int end);

static void
emit_sequence_with_buffers(Emitter *e,int begin,int end, EmitSequenceBuffers *buffers)
{
    int saved=e->local_count;e->depth++;
    e->sequence_terminated=0;
    for(int i=begin;i<end;i++) {
        const ZirStmt *st=&e->fn->stmts[i];
        if(st->kind == ZIR_STMT_ASSIGN && native_step(e, st))
            continue;
        switch(st->kind) {
        case ZIR_STMT_DECL: {
            int switch_end = emit_switch(e, i, end);
            if(switch_end >= 0) {
                i = switch_end;
                break;
            }
        }
            if(st->expr_root>=0) {
                e->call_in_place = 1;
                e->braced_initializer = e->target != ZIR_GO &&
                    e->fn->exprs[st->expr_root].kind == ZIR_EXPR_COMPOUND;
                emit_expr(e,st->expr_root,st->type,buffers->value,sizeof(buffers->value));
                e->short_declaration = typed_initializer(e, st->expr_root, st->type);
            }
            else if(record_type(e->module, st->type)) {
                zero_record(e, st->type, buffers->value, sizeof(buffers->value));
            }
            else copy_text(buffers->value, sizeof(buffers->value), zero_value(st->type, e->target));
            declare(e,st->name,st->type,buffers->value);
            if(e->target == ZIR_GO && !go_binding_read_later(e->fn, i)) {
                char binding[ZIR_NAME_MAX];
                TargetBindingName(e->fn, e->target, st->name, binding,
                                  sizeof(binding));
                line(e, "_ = %s", binding);
            }
            track_local(e, st->name, st->type);
            break;
        case ZIR_STMT_ASSIGN:
            emit_destination(e, st->lhs_root, buffers->lhs, sizeof(buffers->lhs));
            if(strcmp(st->assignment_op,"=")) {
                /* The target's old value reads in place unless a call on
                 * either side could run first or change it. */
                if(expression_calls(e->fn, st->lhs_root) ||
                   expression_calls(e->fn, st->expr_root) ||
                   ArrayElementType(e->fn->exprs[st->lhs_root].type, NULL, 0, NULL)) {
                    fresh(e,buffers->old);
                    declare(e,buffers->old,e->fn->exprs[st->lhs_root].type,buffers->lhs);
                } else
                    copy_text(buffers->old, sizeof(buffers->old), buffers->lhs);
                emit_expr(e,st->expr_root,e->fn->exprs[st->lhs_root].type,buffers->value,sizeof(buffers->value));
                char op[4];copy_text(op,sizeof(op),st->assignment_op);op[strlen(op)-1]=0;
                const char *type=canonical(e->fn->exprs[st->lhs_root].type);
                if(width(type))number(e,type,buffers->old,type,buffers->value,e->fn->exprs[st->expr_root].type,operation(op),buffers->result,sizeof(buffers->result));
                else format(buffers->result,sizeof(buffers->result),"%s %s %s",buffers->old,op,buffers->value);
                /* Go wraps its own compound operators, so x = x + 1 is x++
                 * and x = x + y is x += y. */
                size_t old_length = strlen(buffers->old);
                if(e->target == ZIR_GO && !strcmp(buffers->old, buffers->lhs) &&
                   !strncmp(buffers->result, buffers->old, old_length) && buffers->result[old_length] == ' ' &&
                   !strncmp(buffers->result + old_length + 1, op, strlen(op)) &&
                   buffers->result[old_length + 1 + strlen(op)] == ' ') {
                    const char *operand = buffers->result + old_length + strlen(op) + 2;
                    if(!strcmp(operand, "1") && (!strcmp(op, "+") || !strcmp(op, "-")))
                        line(e, "%s%s", buffers->lhs, !strcmp(op, "+") ? "++" : "--");
                    else
                        line(e, "%s %s= %s", buffers->lhs, op, operand);
                    break;
                }
            } else {
                /* A call assigned to a name runs last, so it assigns directly. */
                e->call_in_place = e->fn->exprs[st->lhs_root].kind == ZIR_EXPR_IDENT;
                emit_expr(e,st->expr_root,e->fn->exprs[st->lhs_root].type,buffers->value,sizeof(buffers->value));
                copy_text(buffers->result,sizeof(buffers->result),buffers->value);
            }
            assign_value(e, buffers->lhs, e->fn->exprs[st->lhs_root].type, buffers->result);
            break;
        case ZIR_STMT_RETURN:
            if(st->expr_root >= 0) {
                e->call_in_place = 1;
                emit_expr(e, st->expr_root, e->fn->return_type, buffers->value, sizeof(buffers->value));
                if((e->target == ZIR_C || e->target == ZIR_CPP) &&
                   ArrayElementType(e->fn->return_type, NULL, 0, NULL)) {
                    char output[ZIR_NAME_MAX];
                    ArrayAbiName(e->fn, -1, output, sizeof(output));
                    /* value is a captured true array; output is an ABI pointer. */
                    line(e, "memmove(%s, %s, sizeof(%s));", output, buffers->value, buffers->value);
                    drop_locals(e, 0);
                    line(e, e->target != ZIR_GO && NativeMainReturnsStatus(e->fn) ?
                         "return 0;" : "return;");
                } else {
                    if(has_owned_locals(e)) {
                        char returned[ZIR_NAME_MAX];
                        fresh(e, returned);
                        declare(e, returned, e->fn->return_type, buffers->value);
                        drop_locals(e, 0);
                        copy_text(buffers->value, sizeof(buffers->value), returned);
                    }
                    line(e, "return %s%s", bare(buffers->value, buffers->plain, sizeof(buffers->plain)),
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
        case ZIR_STMT_IF:i=emit_if(e,i,end,0);break;
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
            int native = st->for_form && e->loop_header[0];
            int binds = native ? e->loop_header_binds : 0;
            if(native) {
                line(e, "%s {", e->loop_header);
                e->loop_header[0] = '\0';
                e->indent++;
            } else {
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
                    e->call_in_place = 1;
                    emit_expr(e,st->expr_root,"bool",buffers->value,sizeof(buffers->value));
                    fclose(scratch);
                    e->out = saved_out;
                    header = scratch_size == 0;
                    free(scratch_text);
                }
                if(header) {
                    line(e,e->target==ZIR_GO?"for %s {":"while (%s) {",
                         bare(buffers->value,buffers->plain,sizeof(buffers->plain)));
                    e->indent++;
                } else {
                    e->serial = saved_serial;
                    line(e,e->target==ZIR_GO?"for {":"while (true) {");e->indent++;
                    e->call_in_place = 1;
                    emit_expr(e,st->expr_root,"bool",buffers->value,sizeof(buffers->value));
                    line(e,e->target==ZIR_GO?"if !%s { break }":"if (!%s) { break; }",buffers->value);
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
            e->loop_native[e->loop_count] = native;
            e->loop_id[e->loop_count++] = st->loop_id;
            emit_sequence(e,i+1+binds,close);
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
            int close=block_end(e->fn,i,end);
            if(native_for(e, i, close)) {
                i = close;
                break;
            }line(e,"{");e->indent++;emit_sequence(e,i+1,close);e->indent--;line(e,"}");i=close;break;
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
                /* A call whose result goes unused is a statement of its own;
                 * an owned Vec result is kept so it can be released. */
                e->call_in_place = !VecElementType(e->module, expr->type, NULL, 0);
                emit_expr(e, st->expr_root, expr->type, buffers->value,
                          sizeof(buffers->value));
                if(*buffers->value && expr->kind == ZIR_EXPR_CALL &&
                   VecElementType(e->module, expr->type, NULL, 0)) {
                    char temporary[ZIR_NAME_MAX];
                    fresh(e, temporary);
                    declare(e, temporary, expr->type, buffers->value);
                    drop_temporary_vec(e, temporary);
                } else if(*buffers->value && expr->kind == ZIR_EXPR_CALL && !plain_identifier(buffers->value))
                    line(e, e->target == ZIR_GO ? "%s" : "%s;", buffers->value);
                else if(*buffers->value)
                    line(e,e->target==ZIR_GO?"_ = %s":"(void)%s;",buffers->value);
            }break;
        default:break;
        }
    }
    drop_locals(e, saved);
    e->local_count=saved;e->depth--;e->sequence_terminated=0;
}

static void
emit_sequence(Emitter *e,int begin,int end)
{
    static _Thread_local EmitSequenceBuffers *spares[16];
    static _Thread_local int spare_count;
    EmitSequenceBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    emit_sequence_with_buffers(e, begin, end, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}
/* Buffers EmitBody keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitBodyBuffers {
    char params[64][ZIR_TEXT_MAX];
} EmitBodyBuffers;

int EmitBody(FILE *out,const ZirModule *module,const ZirFunction *fn,ZirTarget target,
            ZirResolveTarget resolver,void *context);

static int
EmitBody_with_buffers(FILE *out,const ZirModule *module,const ZirFunction *fn,ZirTarget target,
            ZirResolveTarget resolver,void *context, EmitBodyBuffers *buffers)
{
    Emitter e={0};int count;
    if(!CanEmitBody(module, fn))return 0;
    e.out=out;e.module=module;e.fn=fn;e.target=target;e.resolve=resolver;e.context=context;e.indent=1;
    e.minify = zir_minify_output;
    e.locals=calloc((size_t)fn->stmt_count+65,sizeof(*e.locals));
    if(!e.locals) {
        Diagnostic(fn->span, "emit.expression",
                   "out of memory during scalar emission");
        exit(1);
    }
    count=*skip_ws(fn->args)?split_top_level(fn->args,buffers->params[0],64,sizeof(buffers->params[0])):0;
    for(int i=0;i<count;i++) {
        char *colon=strchr(buffers->params[i],':');*colon++=0;trim_in_place(buffers->params[i]);
        const char *type=canonical(skip_ws(colon));
        if((target == ZIR_C || target == ZIR_CPP) &&
           ArrayValueType(type)) {
            char incoming[ZIR_NAME_MAX];
            char binding[ZIR_NAME_MAX];
            ArrayAbiName(fn, i, incoming, sizeof(incoming));
            TargetBindingName(fn, target, buffers->params[i], binding, sizeof(binding));
            declare_array(&e, binding, type, incoming);
        }
        track_local(&e, buffers->params[i], type);
    }
    emit_sequence(&e,0,fn->stmt_count);
    if(!e.sequence_terminated) {
        drop_locals(&e, 0);
        if(strcmp(fn->return_type, "void"))
            line(&e, target == ZIR_GO ? "panic(\"unreachable\")" : "abort();");
    }
    free(e.locals);return 1;
}

int
EmitBody(FILE *out,const ZirModule *module,const ZirFunction *fn,ZirTarget target,
            ZirResolveTarget resolver,void *context)
{
    static _Thread_local EmitBodyBuffers *spares[16];
    static _Thread_local int spare_count;
    EmitBodyBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = EmitBody_with_buffers(out, module, fn, target, resolver, context, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
