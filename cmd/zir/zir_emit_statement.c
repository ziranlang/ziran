#include "zir_scratch.h"
#include "zir_memory.h"
#include "zir_emit_internal.h"

static int run_guards;

void
EmitRunGuards(int enabled)
{
    run_guards = enabled != 0;
}

int
EmitRunGuardsEnabled(void)
{
    return run_guards;
}

static void emit_sequence(Emitter *e,int begin,int end);
static int emit_switch(Emitter *e, int declaration, int end);

/* Whether expression root names name anywhere, even where it lowers away. */
static int
names_binding(const ZirFunction *fn, int root, const char *name)
{
    ZirExprWalk walk = {fn, malloc((size_t)fn->expr_count * sizeof(int)), 0};
    int named = 0;
    if(walk.pending == NULL)
        return 1;
    for(int i = ExprWalkFirst(&walk, root); i >= 0 && !named; i = ExprWalkNext(&walk))
        named = fn->exprs[i].kind == ZIR_EXPR_IDENT && fn->exprs[i].name != NULL &&
                !strcmp(fn->exprs[i].name, name);
    free(walk.pending);
    return named;
}

/* Whether a C array-valued call can write its result straight into the
 * statement's destination: the whole right side is that call, of the same
 * array type, and a new local's name does not appear in its arguments,
 * which would otherwise read the new storage. */
static int
array_result_in_place(const Emitter *e, int root, const char *type, const char *declared)
{
    const ZirExpr *call = &e->fn->exprs[root];
    if(e->target != ZIR_C || call->kind != ZIR_EXPR_CALL || type == NULL ||
       !ArrayElementType(type, NULL, 0, NULL) || TypeHasZeroArray(e->module, type) ||
       call->type == NULL || strcmp(canonical(call->type), canonical(type)))
        return 0;
    if(declared == NULL || call->first_child < 0)
        return 1;
    ZirExprWalk walk = {e->fn, malloc((size_t)e->fn->expr_count * sizeof(int)), 0};
    if(walk.pending == NULL) return 0;
    int mentioned = 0;
    for(int i = ExprWalkFirst(&walk, call->first_child); i >= 0 && !mentioned; i = ExprWalkNext(&walk))
        mentioned = e->fn->exprs[i].kind == ZIR_EXPR_IDENT && !strcmp(e->fn->exprs[i].name, declared);
    free(walk.pending);
    return !mentioned;
}

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
    for(int i = region->body_begin; i < region->body_end; i++) {
        if(!StatementHasAssembly(&fn->stmts[i])) continue;
        const char *names[30], *types[30];
        int count = AssemblyBlockOperands(fn, i, names, types, 30, NULL);
        if(count < 0) exit(1);
        for(int operand = 0; operand < count; operand++) {
            const char *name = names[operand];
            if(name_in_list(declared, declared_count, name) ||
               name_in_list(region->captures, region->capture_count, name) ||
               !strcmp(name, region->first) || !strcmp(name, region->last) ||
               !strcmp(name, region->cursor)) continue;
            if(region->capture_count == 16) {
                Diagnostic(fn->stmts[i].span, "emit.parallel",
                           "native parallel region exceeds 16 captured bindings");
                exit(1);
            }
            copy_text(region->captures[region->capture_count], sizeof(region->captures[0]), name);
            copy_text(region->capture_types[region->capture_count], sizeof(region->capture_types[0]), types[operand]);
            region->capture_count++;
        }
    }
    /* Bindings the body reads anywhere inside its expressions. */
    ZirExprWalk walk = {fn, AllocateOrExit(((size_t)fn->expr_count + 1) * sizeof(int)), 0};
    for(int i = region->body_begin; i < region->body_end &&
        region->capture_count < 16; i++) {
        const ZirStmt *st = &fn->stmts[i];
        int roots[2] = {st->expr_root, st->lhs_root};
        for(int r = 0; r < 2; r++)
            for(int x = ExprWalkFirst(&walk, roots[r]); x >= 0;
                x = ExprWalkNext(&walk)) {
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
                const ZirModule *function_owner = NULL;
                const ZirFunction *function = NULL;
                if(ResolveFunction(module, expr->name, &function_owner, &function) == 1)
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
                const char *declared_type = decl != NULL ? decl->type : NULL;
                if(decl == NULL) {
                    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
                    for(int p = 0; p < parameters->count; p++)
                        if(!strcmp(parameters->items[p].binding_name, expr->name))
                            declared_type = parameters->items[p].value_type;
                }
                if(declared_type == NULL || !declared_type[0])
                    continue;
                taken = 0;
                for(int c = 0; c < region->capture_count; c++)
                    if(!strcmp(region->captures[c], expr->name)) {
                        taken = 1;
                        break;
                    }
                if(taken)
                    continue;
                copy_text(type, sizeof(type), declared_type);
                if(!strcmp(type, "null"))
                    continue;
                copy_text(region->captures[region->capture_count],
                          sizeof(region->captures[0]), expr->name);
                copy_text(region->capture_types[region->capture_count],
                          sizeof(region->capture_types[0]), type);
                region->capture_count++;
            }
    }
    free(walk.pending);
}

static int
parallel_region_fits(const ZirFunction *fn, int while_index)
{
    ParallelRegion region;
    return parallel_region_at(fn, while_index, &region) && region.forward;
}
static void parallel_capture_type(Emitter *e, const char *type, char *out, size_t size);

/* The C storage range of capture C in CTX, as pointer and byte count, for
 * slices, strings and fixed arrays; 0 for other captures. */
static int
capture_range(Emitter *e, const ParallelRegion *region, int c, const char *ctx,
              char *range, size_t size)
{
    const char *type = region->capture_types[c];
    char captured[ZIR_NAME_MAX * 4], element[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX * 2];
    TargetBindingName(e->fn, e->target, region->captures[c], captured, sizeof(captured));
    if(!strcmp(type, "string"))
        format(range, size, "%s.capture_%s.data, (int64_t)%s.capture_%s.length",
               ctx, captured, ctx, captured);
    else if(SliceElementType(type, element, sizeof(element))) {
        parallel_capture_type(e, element, mapped, sizeof(mapped));
        format(range, size, "%s.capture_%s.data, %s.capture_%s.length * (int64_t)sizeof(%s)",
               ctx, captured, ctx, captured, mapped);
    } else if(ArrayValueType(type))
        format(range, size, "%s.capture_%s, (int64_t)sizeof(%s)", ctx, captured, captured);
    else
        return 0;
    return 1;
}

/* Whether the body writes into captured slice NAME: it takes windows of it
 * or assigns its elements. */
static int
parallel_writes_slice(const ZirFunction *fn, const ParallelRegion *region, const char *name)
{
    for(int i = region->body_begin; i < region->body_end; i++) {
        const ZirStmt *st = &fn->stmts[i];
        int root = st->kind == ZIR_STMT_DECL ? st->expr_root :
                   st->kind == ZIR_STMT_ASSIGN ? st->lhs_root : -1;
        if(st->kind == ZIR_STMT_DECL && root >= 0 && fn->exprs[root].kind == ZIR_EXPR_SLICE)
            root = fn->exprs[root].left;
        else if(st->kind == ZIR_STMT_DECL)
            continue;
        while(root >= 0 && root < fn->expr_count &&
              (fn->exprs[root].kind == ZIR_EXPR_INDEX || fn->exprs[root].kind == ZIR_EXPR_MEMBER))
            root = fn->exprs[root].left;
        if(root >= 0 && root < fn->expr_count && fn->exprs[root].kind == ZIR_EXPR_IDENT &&
           !strcmp(fn->exprs[root].name, name))
            return 1;
    }
    return 0;
}

/* The condition under which the region at WHILE_INDEX must run in order:
 * a slice it writes into shares storage with another slice, string or
 * array it captures. Empty when it writes into no slice. */
static void
parallel_shared_storage(Emitter *e, int while_index, const char *ctx, char *out, size_t size)
{
    struct {
        ParallelRegion full;
        char written[ZIR_TEXT_MAX], other[ZIR_TEXT_MAX];
    } *work = AllocateOrExit(sizeof(*work));
    ParallelRegion *full = &work->full;
    char *written = work->written, *other = work->other;
    size_t used = 0;
    out[0] = '\0';
    parallel_region_at(e->fn, while_index, full);
    parallel_region_captures(e->module, e->fn, full);
    for(int w = 0; w < full->capture_count; w++) {
        if(!SliceElementType(full->capture_types[w], NULL, 0) ||
           !parallel_writes_slice(e->fn, full, full->captures[w]) ||
           !capture_range(e, full, w, ctx, written, ZIR_TEXT_MAX))
            continue;
        for(int c = 0; c < full->capture_count; c++) {
            if(c == w || !capture_range(e, full, c, ctx, other, ZIR_TEXT_MAX))
                continue;
            int wrote = snprintf(out + used, size - used, "%sziran_parallel_overlap(%s, %s)",
                                 used ? " || " : "", written, other);
            if(wrote < 0 || (size_t)wrote >= size - used) {
                Diagnostic(e->fn->stmts[while_index].span, "emit.parallel",
                           "native parallel region shares too many slices to check");
                exit(1);
            }
            used += (size_t)wrote;
        }
    }
    free(work);
}

/* A #parallel_gpu region's device attempt, opening a block whose CPU run the
 * caller writes for when the device did not finish it. Returns whether it did. */
static int
gpu_region_dispatch(Emitter *e, const ParallelRegion *region, const char *ctx)
{
    if(run_guards || e->target != ZIR_C || !e->fn->stmts[region->while_index].is_gpu)
        return 0;
    struct {
        ParallelRegion full;
        GpuRegionCode code;
        char lines[ZIR_NAME_MAX], state[ZIR_NAME_MAX], bindings[ZIR_NAME_MAX];
        char range[ZIR_TEXT_MAX], captured[ZIR_NAME_MAX * 4];
    } *work = AllocateOrExit(sizeof(*work));
    ParallelRegion *full = &work->full;
    GpuRegionCode *code = &work->code;
    parallel_region_at(e->fn, region->while_index, full);
    parallel_region_captures(e->module, e->fn, full);
    if(!GpuLowerRegion(e->module, e->fn, full, code)) {
        free(work);
        return 0;
    }
    fresh(e, work->lines);
    fresh(e, work->state);
    fresh(e, work->bindings);
    line(e, "{");
    e->indent++;
    line(e, "static const char *const %s[] = {", work->lines);
    e->indent++;
    GpuEmitShaderLines(e, code->text);
    e->indent--;
    line(e, "};");
    line(e, "static ziran_gpu_region %s = {%s, (int)(sizeof(%s) / sizeof(%s[0])), 0, 0};",
         work->state, work->lines, work->lines, work->lines);
    line(e, "ziran_gpu_binding %s[] = {", work->bindings);
    e->indent++;
    for(int c = 0; c < full->capture_count; c++) {
        if(code->is_buffer[c]) {
            capture_range(e, full, c, ctx, work->range, sizeof(work->range));
            char *comma = strstr(work->range, ", ");
            *comma = '\0';
            line(e, "{%d, 1, %d, (void *)%s, %s},", code->kinds[c], code->writes[c], work->range, comma + 2);
        } else {
            TargetBindingName(e->fn, e->target, full->captures[c], work->captured, sizeof(work->captured));
            line(e, "{%d, 0, 0, &%s.capture_%s, 0},", code->kinds[c], ctx, work->captured);
        }
    }
    e->indent--;
    line(e, "};");
    line(e, "if(!ziran_gpu_run(&%s, %s.start, %s.end, %s, (int)(sizeof(%s) / sizeof(%s[0]))))",
         work->state, ctx, ctx, work->bindings, work->bindings, work->bindings);
    e->indent++;
    free(code->text);
    free(work);
    return 1;
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
        {
            char captured[ZIR_NAME_MAX * 4];
            TargetBindingName(e->fn, e->target, full.captures[c], captured, sizeof(captured));
            /* Arrays travel by address; workers reach the caller's. */
            if(ArrayValueType(full.capture_types[c]))
                line(e, "%s.capture_%s = (void *)%s;", ctx, captured, captured);
            else
                line(e, "%s.capture_%s = %s;", ctx, captured, captured);
        }
    }
    char shared[ZIR_TEXT_MAX];
    parallel_shared_storage(e, region->while_index, ctx, shared, sizeof(shared));
    if(shared[0]) {
        line(e, "if(%s)", shared);
        e->indent++;
        line(e, "%s_worker(%s.start, %s.end, &%s);", region->worker, ctx, ctx, ctx);
        e->indent--;
        line(e, "else");
        e->indent++;
    }
    int on_device = gpu_region_dispatch(e, region, ctx);
    line(e, "ziran_parallel_run(%s_worker, %s.start, %s.end, &%s);",
         region->worker, ctx, ctx, ctx);
    if(on_device) {
        e->indent -= 2;
        line(e, "}");
    }
    if(shared[0])
        e->indent--;
    e->indent--;
    line(e, "}");
}

COMPILER_SCRATCH_VOID(static, parallel_region_dispatch,
    (Emitter *e, const ParallelRegion *region),
    ParallelRegionDispatchBuffers, RECOVERABLE, free,
    parallel_region_dispatch_with_buffers(e, region, buffers))

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
parallel_capture_type(Emitter *e, const char *type, char *out, size_t size)
{
    int pointers = 0;
    while(*type == '*') {
        pointers++;
        type = skip_ws(type + 1);
    }
    const char *scalar = TargetType(type, e->target);
    if(scalar)
        copy_text(out, size, scalar);
    else
        e->resolve(e->context, type, out, size);
    size_t used = strlen(out);
    if(used + (size_t)pointers + 1 > size) exit(1);
    while(pointers--) out[used++] = '*';
    out[used] = '\0';
}

static void
emit_parallel_worker(Emitter *e, const ZirFunction *fn,
                     ParallelRegion *region, int ordinal)
{
    char function_name[ZIR_NAME_MAX];
    char mapped[ZIR_NAME_MAX * 2];
    char low[ZIR_NAME_MAX], high[ZIR_NAME_MAX], opaque[ZIR_NAME_MAX];
    char context[ZIR_NAME_MAX], cursor[ZIR_NAME_MAX], binder[ZIR_NAME_MAX * 4];
    ArrayAbiName(fn, 8000000 + ordinal * 5, low, sizeof(low));
    ArrayAbiName(fn, 8000001 + ordinal * 5, high, sizeof(high));
    ArrayAbiName(fn, 8000002 + ordinal * 5, opaque, sizeof(opaque));
    ArrayAbiName(fn, 8000003 + ordinal * 5, context, sizeof(context));
    ArrayAbiName(fn, 8000004 + ordinal * 5, cursor, sizeof(cursor));
    TargetBindingName(fn, e->target, region->binder, binder, sizeof(binder));
    const char *scalar = TargetType("s64", e->target);
    TargetBindingName(fn, e->target, fn->name, function_name,
                      sizeof(function_name));
    snprintf(region->worker, sizeof(region->worker), "ziran_par_%s_%d",
             function_name, ordinal);
    fprintf(e->out, "struct %s_ctx {\n", region->worker);
    fprintf(e->out, "    int64_t start;\n    int64_t end;\n");
    for(int c = 0; c < region->capture_count; c++) {
        char type_mapped[ZIR_NAME_MAX * 2];
        char captured[ZIR_NAME_MAX * 4];
        TargetBindingName(fn, e->target, region->captures[c], captured, sizeof(captured));
        if(ArrayValueType(region->capture_types[c])) {
            fprintf(e->out, "    void *capture_%s;\n", captured);
            continue;
        }
        parallel_capture_type(e, region->capture_types[c], type_mapped,
                              sizeof(type_mapped));
        fprintf(e->out, "    %s capture_%s;\n", type_mapped, captured);
    }
    fprintf(e->out, "};\n\n");
    fprintf(e->out,
            "static void %s_worker(int64_t %s, int64_t %s, "
            "void *%s)\n{\n", region->worker, low, high, opaque);
    e->indent = 1;
    fprintf(e->out, "    struct %s_ctx *%s = "
            "(struct %s_ctx *)%s;\n", region->worker, context,
            region->worker, opaque);
    if(run_guards)
        line(e, "ZIR_BUDGET();");
    copy_text(mapped, sizeof(mapped), scalar ? scalar : "int64_t");
    line(e, "%s %s = %s->start;", mapped, region->first, context);
    line(e, "%s %s = %s->end;", mapped, region->last, context);
    for(int c = 0; c < region->capture_count; c++) {
        char type_mapped[ZIR_NAME_MAX * 2];
        char captured[ZIR_NAME_MAX * 4];
        TargetBindingName(fn, e->target, region->captures[c], captured, sizeof(captured));
        /* A captured array stays the caller's: the worker reaches it
         * through a pointer, so no chunk copies it and slot writes land. */
        if(ArrayValueType(region->capture_types[c])) {
            char element[ZIR_NAME_MAX * 2], bounds[ZIR_NAME_MAX * 2], pointer[ZIR_NAME_MAX];
            array_target_type(e, region->capture_types[c], element, sizeof(element),
                              bounds, sizeof(bounds));
            fresh(e, pointer);
            line(e, "%s (*%s)%s = %s->capture_%s;", element, pointer, bounds,
                 context, captured);
            track_local(e, region->captures[c], region->capture_types[c]);
            snprintf(e->locals[e->local_count - 1].alias,
                     sizeof(e->locals[0].alias), "(*%s)", pointer);
            continue;
        }
        parallel_capture_type(e, region->capture_types[c], type_mapped,
                              sizeof(type_mapped));
        line(e, "%s %s = %s->capture_%s;", type_mapped, captured, context, captured);
    }
    line(e, "for (int64_t %s = %s; %s <= %s; "
            "%s += 1) {", cursor, low, cursor, high, cursor);
    e->indent++;
    line(e, "%s %s = %s;", mapped, binder, cursor);
    line(e, "%s it_index = %s - %s;", mapped, cursor, region->first);
    emit_sequence(e, region->body_begin, region->body_end);
    e->indent--;
    line(e, "}");
    fprintf(e->out, "}\n\n");
}

/* A region's device header, or why it runs on CPU threads. */
static void
gpu_region_note(FILE *out, const ZirModule *module, const ZirFunction *fn, ParallelRegion *region)
{
    GpuRegionCode code;
    parallel_region_captures(module, fn, region);
    if(GpuLowerRegion(module, fn, region, &code)) {
        free(code.text);
        fprintf(out, "#include \"ziran_gpu.h\"\n\n");
    } else {
        fprintf(out, "/* ziran: #parallel_gpu region stays on CPU threads: %s. */\n\n", code.reason);
    }
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
    for(int i = 0; i < fn->stmt_count; i++) {
        ParallelRegion region;
        if(!parallel_region_at(fn, i, &region))
            continue;
        if(fn->stmts[i].is_gpu && region.forward && !run_guards)
            gpu_region_note(out, module, fn, &region);
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
        const ZirType *record = FindType(e->module, type, NULL);
        if(record != NULL && (record->foreign_target[0] || record->is_map))
            format(out, size, "*new(%s)", target_type);
        else
            format(out, size, "%s{}", target_type);
        return;
    }
    copy_text(out, size, TypeHasZeroArray(e->module, type) ? "{}" : "{0}");
}

/* Whether a condition lowers without statements of its own, so it can sit
 * in an else if header. */
static int
condition_in_header(Emitter *e, int root)
{
    char cond[ZIR_TEXT_MAX];
    unsigned char *scratch_text = NULL;
    size_t scratch_size = 0;
    FILE *saved_out = e->out;
    int saved_serial = e->serial, saved_locals = e->local_count;
    FILE *scratch = EmitScratchOpen(&scratch_text, &scratch_size);
    if(scratch == NULL)
        return 0;
    e->out = scratch;
    e->call_in_place = 1;
    emit_expr(e, root, "bool", cond, sizeof(cond));
    EmitScratchClose(scratch, &scratch_size);
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
COMPILER_SCRATCH_RETURN(static, int, emit_if,
    (Emitter *e,int i,int end,int chained),
    EmitIfBuffers, RECOVERABLE, free,
    emit_if_with_buffers(e, i, end, chained, buffers))

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

/* Whether a C local is surely never read: no identifier anywhere in the
 * procedure but a whole-variable assignment target spells its name. Names
 * are kept as hashes, so a collision only keeps a harmless warning. */
static int
c_binding_unread(const ZirFunction *fn, const char *name)
{
    static _Thread_local const ZirFunction *function;
    static _Thread_local const ZirExpr *exprs;
    static _Thread_local int expr_count, capacity;
    static _Thread_local unsigned *hashes;
    unsigned hash = 2166136261u;
    if(function != fn || exprs != fn->exprs || expr_count != fn->expr_count) {
        int need = 16;
        while(need < fn->expr_count * 2)
            need *= 2;
        unsigned char *target = calloc((size_t)fn->expr_count + 1, 1);
        if(target == NULL) {
            function = NULL;
            return 0;
        }
        if(need > capacity) {
            unsigned *grown = realloc(hashes, (size_t)need * sizeof(*grown));
            if(grown == NULL) {
                free(target);
                function = NULL;
                return 0;
            }
            hashes = grown;
            capacity = need;
        }
        memset(hashes, 0, (size_t)capacity * sizeof(*hashes));
        for(int s = 0; s < fn->stmt_count; s++)
            if(fn->stmts[s].lhs_root >= 0 && fn->exprs[fn->stmts[s].lhs_root].kind == ZIR_EXPR_IDENT)
                target[fn->stmts[s].lhs_root] = 1;
        for(int x = 0; x < fn->expr_count; x++) {
            if(fn->exprs[x].kind != ZIR_EXPR_IDENT || fn->exprs[x].name == NULL || target[x])
                continue;
            unsigned h = 2166136261u;
            for(const char *c = fn->exprs[x].name; *c; c++)
                h = (h ^ (unsigned char)*c) * 16777619u;
            h |= 1;
            for(unsigned k = h & (unsigned)(capacity - 1); hashes[k] != h; k = (k + 1) & (unsigned)(capacity - 1))
                if(hashes[k] == 0) { hashes[k] = h; break; }
        }
        free(target);
        function = fn;
        exprs = fn->exprs;
        expr_count = fn->expr_count;
    }
    for(const char *c = name; *c; c++)
        hash = (hash ^ (unsigned char)*c) * 16777619u;
    hash |= 1;
    for(unsigned k = hash & (unsigned)(capacity - 1); hashes[k] != 0; k = (k + 1) & (unsigned)(capacity - 1))
        if(hashes[k] == hash)
            return 0;
    return 1;
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
/* Whether statements begin..end can leave loop (a while at loop, or a loop
 * holding it) early: a return, or a break or named continue that does not
 * stop at a loop nested inside. */
static int
loop_leaves_early(const ZirFunction *fn, int loop, int begin, int end)
{
    int ends[64], ids[64], depth = 0;
    for(int s = begin; s < end; s++) {
        const ZirStmt *st = &fn->stmts[s];
        while(depth > 0 && s > ends[depth - 1])
            depth--;
        if(st->kind == ZIR_STMT_RETURN)
            return 1;
        if(st->kind == ZIR_STMT_WHILE) {
            if(depth == 64)
                return 1;
            ends[depth] = block_end(fn, s, end);
            ids[depth++] = st->loop_id;
            continue;
        }
        if(st->kind != ZIR_STMT_BREAK && st->kind != ZIR_STMT_CONTINUE)
            continue;
        int inner = 0;
        if(st->target_id == 0)
            inner = depth > 0;
        else
            for(int d = 0; d < depth && !inner; d++)
                inner = ids[d] == st->target_id;
        if(inner)
            continue;
        if(st->kind == ZIR_STMT_BREAK ||
           (st->target_id != 0 && st->target_id != fn->stmts[loop].loop_id))
            return 1;
    }
    return 0;
}

/* A literal integer operand: n or -n. */
static int
literal_operand(const ZirFunction *fn, int index, int64_t *value)
{
    const ZirExpr *expr = &fn->exprs[index];
    unsigned long long bits;
    int negative = 0;
    if(expr->kind == ZIR_EXPR_UNARY && expr->op != NULL && !strcmp(expr->op, "-")) {
        negative = 1;
        expr = &fn->exprs[expr->right];
    }
    if(expr->kind != ZIR_EXPR_INT || !ZirIntegerLiteral(expr->text, &bits) ||
       bits > (UINT64_C(1) << 40))
        return 0;
    *value = negative ? -(int64_t)bits : (int64_t)bits;
    return 1;
}

/* The iterations of the counting loop at while (its counter declared just
 * before it) when its bounds are literals and nothing but its step changes
 * the counter or leaves it early, so the native tier can charge them all
 * as it starts. */
static int
counted_iterations(const ZirFunction *fn, int loop, int close, int64_t *count)
{
    const ZirStmt *st = &fn->stmts[loop], *counter = &fn->stmts[loop - 1];
    int64_t first, last;
    if(loop < 1 || counter->kind != ZIR_STMT_DECL || counter->expr_root < 0 ||
       st->expr_root < 0 || strcmp(canonical(counter->type), "s64") != 0 ||
       !literal_operand(fn, counter->expr_root, &first))
        return 0;
    const ZirExpr *condition = &fn->exprs[st->expr_root];
    if(condition->kind != ZIR_EXPR_BINARY || condition->op == NULL ||
       fn->exprs[condition->left].kind != ZIR_EXPR_IDENT ||
       strcmp(fn->exprs[condition->left].name, counter->name) != 0 ||
       !literal_operand(fn, condition->right, &last))
        return 0;
    /* A pointer to the counter could change it out of sight. */
    for(int x = 0; x < fn->expr_count; x++)
        if(fn->exprs[x].kind == ZIR_EXPR_UNARY && fn->exprs[x].op != NULL &&
           !strcmp(fn->exprs[x].op, "&") && fn->exprs[fn->exprs[x].right].kind == ZIR_EXPR_IDENT &&
           !strcmp(fn->exprs[fn->exprs[x].right].name, counter->name))
            return 0;
    int down = 0, steps = 0;
    for(int s = loop + 1; s < close; s++) {
        const ZirStmt *body = &fn->stmts[s];
        if(body->kind == ZIR_STMT_DECL && !strcmp(body->name, counter->name))
            return 0;
        if(body->kind != ZIR_STMT_ASSIGN || body->lhs_root < 0 ||
           fn->exprs[body->lhs_root].kind != ZIR_EXPR_IDENT ||
           strcmp(fn->exprs[body->lhs_root].name, counter->name) != 0)
            continue;
        int64_t one;
        if(body->for_step != st->loop_id || body->expr_root < 0 ||
           !literal_operand(fn, body->expr_root, &one) || one != 1)
            return 0;
        if(!strcmp(body->assignment_op, "-="))
            down = 1;
        else if(strcmp(body->assignment_op, "+=") != 0)
            return 0;
        steps++;
    }
    if(steps != 1 || loop_leaves_early(fn, loop, loop + 1, close))
        return 0;
    const char *op = condition->op;
    if(!down && !strcmp(op, "<="))
        *count = last - first + 1;
    else if(!down && !strcmp(op, "<"))
        *count = last - first;
    else if(down && !strcmp(op, ">="))
        *count = first - last + 1;
    else if(down && !strcmp(op, ">"))
        *count = first - last;
    else
        return 0;
    if(*count < 0)
        *count = 0;
    return 1;
}

/* Whether the expression at index is the identifier name. */
static int
names_ident(const ZirFunction *fn, int index, const char *name)
{
    return index >= 0 && fn->exprs[index].kind == ZIR_EXPR_IDENT &&
           !strcmp(fn->exprs[index].name, name);
}

/* Whether the expression at index compares the identifiers left and right
 * with op. */
static int
compares_idents(const ZirFunction *fn, int index, const char *op,
                const char *left, const char *right)
{
    const ZirExpr *expr = index >= 0 ? &fn->exprs[index] : NULL;
    return expr != NULL && expr->kind == ZIR_EXPR_BINARY && expr->op != NULL &&
           !strcmp(expr->op, op) && names_ident(fn, expr->left, left) &&
           names_ident(fn, expr->right, right);
}

/* Whether the while at loop is a range loop over a computed bound,
 * `while i <= last { ... if i == last { break } i += 1 }` (or counting
 * down to first with >= and -=), whose test needs to run only as the loop
 * starts: i and the bound are locals declared just before it, nothing takes
 * their address, and every change of i is a step that stops at the bound
 * first, so i never passes it. C then writes `if (test) for (;;)`, a loop
 * with one exit, which GCC can vectorize and TinyCC runs without the test. */
static int
range_tests_once(const ZirFunction *fn, int loop, int close)
{
    const ZirStmt *st = &fn->stmts[loop];
    if(loop < 3 || st->is_parallel || st->for_form || st->expr_root < 0)
        return 0;
    const ZirExpr *test = &fn->exprs[st->expr_root];
    if(test->kind != ZIR_EXPR_BINARY || test->op == NULL ||
       (strcmp(test->op, "<=") != 0 && strcmp(test->op, ">=") != 0) ||
       test->left < 0 || test->right < 0 ||
       fn->exprs[test->left].kind != ZIR_EXPR_IDENT ||
       fn->exprs[test->right].kind != ZIR_EXPR_IDENT)
        return 0;
    const char *cursor = fn->exprs[test->left].name;
    const char *bound = fn->exprs[test->right].name;
    const char *step = test->op[0] == '<' ? "+=" : "-=";
    if(!strcmp(cursor, bound) || fn->stmts[loop - 1].kind != ZIR_STMT_DECL ||
       strcmp(fn->stmts[loop - 1].name, cursor) != 0)
        return 0;
    int declared = 0;
    for(int s = loop - 3; s < loop - 1; s++)
        if(fn->stmts[s].kind == ZIR_STMT_DECL && !strcmp(fn->stmts[s].name, bound))
            declared = 1;
    if(!declared)
        return 0;
    /* A pointer to either could change it out of sight. */
    for(int x = 0; x < fn->expr_count; x++)
        if(fn->exprs[x].kind == ZIR_EXPR_UNARY && fn->exprs[x].op != NULL &&
           !strcmp(fn->exprs[x].op, "&") &&
           (names_ident(fn, fn->exprs[x].right, cursor) ||
            names_ident(fn, fn->exprs[x].right, bound)))
            return 0;
    int steps = 0;
    for(int s = loop + 1; s < close; s++) {
        const ZirStmt *body = &fn->stmts[s];
        if(StatementHasAssembly(body) || body->kind == ZIR_STMT_FOR ||
           (body->kind == ZIR_STMT_DECL &&
            (!strcmp(body->name, cursor) || !strcmp(body->name, bound))))
            return 0;
        if(body->kind != ZIR_STMT_ASSIGN)
            continue;
        if(body->lhs_root < 0)
            return 0;
        if(names_ident(fn, body->lhs_root, bound))
            return 0;
        if(!names_ident(fn, body->lhs_root, cursor))
            continue;
        int64_t one;
        if(strcmp(body->assignment_op, step) != 0 || body->expr_root < 0 ||
           !literal_operand(fn, body->expr_root, &one) || one != 1 || s < loop + 4 ||
           fn->stmts[s - 1].kind != ZIR_STMT_BLOCK_CLOSE ||
           fn->stmts[s - 2].kind != ZIR_STMT_BREAK ||
           fn->stmts[s - 3].kind != ZIR_STMT_IF ||
           !compares_idents(fn, fn->stmts[s - 3].expr_root, "==", cursor, bound))
            return 0;
        steps++;
    }
    return steps > 0 && fn->stmts[close - 1].kind == ZIR_STMT_ASSIGN &&
           names_ident(fn, fn->stmts[close - 1].lhs_root, cursor);
}

/* ---- Inlining small procedures (C) ----------------------------------- */

enum {
    INLINE_BODY_STATEMENTS = 24, INLINE_BODY_EXPRESSIONS = 96, INLINE_NESTING = 2,
    INLINE_RECORD_FIELDS = 16, INLINE_RECORD_DEPTH = 3
};

/* A parameter, local or result type an inlined body can hold in a plain C
 * variable: no array, which travels by ABI pointer, and nothing owned. */
static int
inline_value_type_in(const ZirModule *m, const char *type, int depth)
{
    const ZirModule *owner = NULL;
    const ZirType *record;
    const ZirTypeField *field;
    size_t offset = 0;
    int fields = 0, more;
    if(type == NULL || *type == '\0')
        return 0;
    type = canonical(type);
    if(strchr(type, '[') != NULL || emitter_type_contains_vec(m, type, 0) ||
       OwnedTextType(m, type))
        return 0;
    if(*type == '*' || BuiltinTypeName(type))
        return 1;
    record = FindType(m, type, &owner);
    if(record == NULL)
        return 0;
    if(record->is_enum || record->is_procedure_type)
        return 1;
    if(record->is_union || record->is_extern || depth >= INLINE_RECORD_DEPTH)
        return 0;
    while((more = TypeNextFieldShared(record, &offset, &field)) == 1)
        if(++fields > INLINE_RECORD_FIELDS ||
           !inline_value_type_in(owner != NULL ? owner : m, field->type, depth + 1))
            return 0;
    return more == 0;
}

/* A type an inlined body may hold in a caller's frame: a scalar, a pointer
 * or a small record of those, never an array a caller would carry for good. */
static int
inline_value_type(const ZirModule *m, const char *type)
{
    return inline_value_type_in(m, type, 0);
}

/* The procedure of module m a call names, when its body can stand in for
 * the call: small, straight-line code with no loops, referring to no module
 * variable (whose C name a caller's local could hide), in plain types. */
static const ZirFunction *
inline_callee(const Emitter *e, const ZirExpr *call)
{
    const ZirModule *m = e->module;
    const ZirFunction *fn = NULL;
    if(e->target != ZIR_C || e->inline_depth >= INLINE_NESTING || *call->slot_type ||
       call->name[0] == '\0' || strchr(call->name, '.') != NULL)
        return NULL;
    for(int i = 0; i < m->function_count && fn == NULL; i++)
        if(!strcmp(m->functions[i].name, call->name))
            fn = &m->functions[i];
    if(fn == NULL || fn == e->fn || fn->is_template || fn->is_extern || fn->exported ||
       fn->simd_target || fn->is_global_initializer || fn->uses_host || fn->go_method[0] ||
       (fn->assembly_text && *fn->assembly_text) || NativeProgramMain(m, fn) ||
       fn->inlining == ZIR_INLINE_COLD || !fn->checked ||
       fn->stmt_count == 0 || fn->stmt_count > INLINE_BODY_STATEMENTS ||
       fn->expr_count > INLINE_BODY_EXPRESSIONS || !CanEmitBody(m, fn))
        return NULL;
    if(strcmp(fn->return_type, "void") && !inline_value_type(m, fn->return_type))
        return NULL;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    int arguments = 0;
    for(int child = call->first_child; child >= 0; child = e->fn->exprs[child].next_sibling)
        arguments++;
    if(arguments != parameters->count)
        return NULL;
    for(int i = 0; i < parameters->count; i++)
        if(parameters->items[i].name == NULL || parameters->items[i].is_using ||
           !inline_value_type(m, parameters->items[i].type))
            return NULL;
    for(int i = 0; i < fn->stmt_count; i++) {
        const ZirStmt *st = &fn->stmts[i];
        if(st->is_parallel || st->is_gpu || st->is_using || StatementHasAssembly(st))
            return NULL;
        switch(st->kind) {
        case ZIR_STMT_BLOCK_OPEN: case ZIR_STMT_BLOCK_CLOSE: case ZIR_STMT_ASSIGN:
        case ZIR_STMT_EXPR: case ZIR_STMT_IF: case ZIR_STMT_RETURN:
            break;
        case ZIR_STMT_DECL:
            if(!inline_value_type(m, st->type))
                return NULL;
            break;
        default:
            return NULL;
        }
    }
    for(int i = 0; i < fn->expr_count; i++) {
        const ZirExpr *x = &fn->exprs[i];
        if(x->is_function_value)
            return NULL;
        if(x->kind == ZIR_EXPR_CALL && x->name[0] && !strcmp(x->name, fn->name))
            return NULL;
        /* A foreign procedure is declared only where its own module's
         * body is, never in a header an inlined copy may land in, so a
         * plain call must name one of the module's own procedures. */
        if(x->kind == ZIR_EXPR_CALL && strchr(x->name, '.') == NULL) {
            int own = 0;
            for(int f = 0; f < m->function_count && !own; f++)
                own = !strcmp(m->functions[f].name, x->name) &&
                      !m->functions[f].is_extern && !m->functions[f].uses_host;
            if(!own)
                return NULL;
        }
        if(x->kind == ZIR_EXPR_IDENT)
            for(int g = 0; g < m->global_count; g++)
                if(!strcmp(m->globals[g].name, x->name))
                    return NULL;
    }
    return fn;
}

/* C text for an integer literal, such as 7, -3 or 4LL. */
static int
inline_literal(const char *text)
{
    if(*text == '-')
        text++;
    if(!isdigit((unsigned char)*text))
        return 0;
    while(isalnum((unsigned char)*text))
        text++;
    return *text == '\0';
}

int
emit_inline_call(Emitter *e, const ZirExpr *call, char *out, size_t size)
{
    const ZirFunction *fn = inline_callee(e, call);
    if(fn == NULL)
        return 0;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    int count = parameters->count;
    char (*values)[ZIR_TEXT_MAX] = calloc((size_t)(count > 0 ? count : 1), sizeof(*values));
    Local *locals = calloc((size_t)fn->stmt_count + count + 65, sizeof(*locals));
    char result[ZIR_NAME_MAX] = "", label[ZIR_NAME_MAX];
    if(values == NULL || locals == NULL) {
        free(values);
        free(locals);
        return 0;
    }
    /* Arguments run left to right in the caller, each into a variable
     * unless it is a literal or a name no later argument could change. */
    for(int child = call->first_child; child >= 0; child = e->fn->exprs[child].next_sibling) {
        int ordinal = e->fn->exprs[child].argument_index;
        const char *type = parameters->items[ordinal].type;
        int later_calls = 0;
        for(int next = e->fn->exprs[child].next_sibling; next >= 0;
            next = e->fn->exprs[next].next_sibling)
            later_calls |= expression_calls(e->fn, next);
        e->call_in_place = 0;
        emit_expr(e, child, type, values[ordinal], sizeof(values[ordinal]));
        char plain[ZIR_TEXT_MAX];
        bare(values[ordinal], plain, sizeof(plain));
        int keep = inline_literal(plain) || (plain_identifier(plain) && !later_calls);
        for(int p = 0; keep && p < count; p++) {
            char binding[ZIR_NAME_MAX];
            TargetBindingName(fn, e->target, parameters->items[p].name, binding, sizeof(binding));
            keep = strcmp(binding, plain) != 0;
        }
        if(keep) {
            copy_text(values[ordinal], sizeof(values[ordinal]), plain);
        } else {
            char captured[ZIR_NAME_MAX];
            fresh(e, captured);
            declare(e, captured, type, values[ordinal]);
            copy_text(values[ordinal], sizeof(values[ordinal]), captured);
        }
    }
    if(strcmp(fn->return_type, "void")) {
        char zero[ZIR_TEXT_MAX];
        fresh(e, result);
        if(record_type(e->module, fn->return_type))
            zero_record(e, fn->return_type, zero, sizeof(zero));
        else
            copy_text(zero, sizeof(zero), zero_value(fn->return_type, e->target));
        declare(e, result, fn->return_type, zero);
    }
    fresh(e, label);
    Emitter inner = *e;
    inner.fn = fn;
    inner.locals = locals;
    inner.local_count = 0;
    inner.depth = 0;
    inner.loop_count = 0;
    inner.loop_header[0] = '\0';
    inner.sequence_terminated = 0;
    inner.call_in_place = 0;
    inner.array_destination = NULL;
    inner.array_operand = 0;
    inner.short_declaration = 0;
    inner.braced_initializer = 0;
    inner.inline_result = result[0] ? result : NULL;
    inner.inline_label = label;
    inner.inline_tail = fn->stmt_count - 1;
    inner.inline_jumps = 0;
    inner.inline_depth = e->inline_depth + 1;
    inner.ranges = RangeFactsBuild(fn);
    line(e, "{");
    inner.indent = e->indent + 1;
    /* The call still costs the step its entry would have charged. */
    if(run_guards)
        line(&inner, "ZIR_STEP();");
    for(int p = 0; p < count; p++) {
        declare(&inner, parameters->items[p].name, parameters->items[p].type, values[p]);
        track_local(&inner, parameters->items[p].name, canonical(parameters->items[p].type));
        if(c_binding_unread(fn, parameters->items[p].name)) {
            char binding[ZIR_NAME_MAX];
            TargetBindingName(fn, ZIR_C, parameters->items[p].name, binding, sizeof(binding));
            line(&inner, "(void)%s;", binding);
        }
    }
    emit_sequence(&inner, 0, fn->stmt_count);
    RangeFactsFree(inner.ranges);
    e->serial = inner.serial;
    if(inner.inline_jumps)
        line(e, "%s: ;", label);
    line(e, "}");
    free(values);
    free(locals);
    copy_text(out, size, result);
    return 1;
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
        unsigned char *scratch_text = NULL;
        size_t scratch_size = 0;
        FILE *saved_out = e->out;
        FILE *scratch = EmitScratchOpen(&scratch_text, &scratch_size);
        if(scratch == NULL)
            return 0;
        e->out = scratch;
        emit_expr(e, counter->expr_root, counter->type, buffers->start, sizeof(buffers->start));
        track_local(e, counter->name, counter->type);
        emit_expr(e, loop->expr_root, "bool", buffers->condition, sizeof(buffers->condition));
        EmitScratchClose(scratch, &scratch_size);
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
        /* range over a string yields runes, not bytes, so strings count;
         * a Vec is a record holding its elements, so it counts too. */
        if(index != NULL && index->kind == ZIR_EXPR_INDEX &&
           fn->exprs[index->left].kind == ZIR_EXPR_IDENT &&
           strcmp(canonical(fn->exprs[index->left].type), "string") != 0 &&
           !VecElementType(e->module, fn->exprs[index->left].type, NULL, 0) &&
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
COMPILER_SCRATCH_RETURN(static, int, native_for,
    (Emitter *e, int open, int close),
    NativeForBuffers, RECOVERABLE, free,
    native_for_with_buffers(e, open, close, buffers))

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
        /* Every arm is a lexical scope in Ziran. Even an assignment can
         * emit a temporary declaration before its C statement, and C
         * allows no declaration straight after a label. */
        int block = e->target != ZIR_GO;
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
COMPILER_SCRATCH_RETURN(static, int, emit_switch,
    (Emitter *e, int declaration, int end),
    EmitSwitchBuffers, RECOVERABLE, free,
    emit_switch_with_buffers(e, declaration, end, buffers))
/* Buffers emit_sequence keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitSequenceBuffers {
    char value[ZIR_TEXT_MAX];
    char lhs[ZIR_TEXT_MAX];
    char result[ZIR_TEXT_MAX];
    char old[ZIR_TEXT_MAX];
    char plain[ZIR_TEXT_MAX];
} EmitSequenceBuffers;

/* Whether an assignment stores owned storage into a global, or a field path
 * of a global or a local, which frees the old value first. The checker
 * admits no other owned destination than a whole local, which it requires
 * to be moved first. */
static int
owned_destination_frees(Emitter *e, int index)
{
    const ZirExpr *place = &e->fn->exprs[index];
    int field = place->kind == ZIR_EXPR_MEMBER;
    if(!emitter_type_contains_vec(e->module, place->type, 0))
        return 0;
    while(place->kind == ZIR_EXPR_MEMBER && place->left >= 0) {
        place = &e->fn->exprs[place->left];
        if(place->type == NULL || place->type[0] == '*')
            return 0;
    }
    if(place->kind != ZIR_EXPR_IDENT)
        return 0;
    for(int i = 0; i < e->local_count; i++)
        if(!strcmp(e->locals[i].name, place->name))
            return field;
    return 1;
}

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
            if(st->expr_root >= 0 && array_result_in_place(e, st->expr_root, st->type, st->name)) {
                /* The new array is the call's result buffer. */
                char binding[ZIR_NAME_MAX], element[ZIR_NAME_MAX * 2], bounds[ZIR_NAME_MAX * 2];
                alias_shadowed_local(e, st->name);
                TargetBindingName(e->fn, e->target, st->name, binding, sizeof(binding));
                array_target_type(e, st->type, element, sizeof(element), bounds, sizeof(bounds));
                line(e, "%s %s%s;", element, binding, bounds);
                e->array_destination = binding;
                e->array_destination_used = 0;
                e->call_in_place = 1;
                emit_expr(e, st->expr_root, st->type, buffers->value, sizeof(buffers->value));
                e->array_destination = NULL;
                if(!e->array_destination_used)
                    assign_value(e, binding, st->type, buffers->value);
                track_local(e, st->name, st->type);
                break;
            }
            if(st->expr_root < 0 && e->target == ZIR_C && WrittenBeforeRead(e, i)) {
                /* Every element is stored before anything can read it. */
                char binding[ZIR_NAME_MAX], element[ZIR_NAME_MAX * 2], bounds[ZIR_NAME_MAX * 2];
                alias_shadowed_local(e, st->name);
                TargetBindingName(e->fn, e->target, st->name, binding, sizeof(binding));
                if(ArrayElementType(st->type, NULL, 0, NULL)) {
                    array_target_type(e, st->type, element, sizeof(element), bounds, sizeof(bounds));
                    line(e, "%s %s%s;", element, binding, bounds);
                } else {
                    e->resolve(e->context, st->type, element, sizeof(element));
                    line(e, "%s %s;", element, binding);
                }
                if(c_binding_unread(e->fn, st->name))
                    line(e, "(void)%s;", binding);
                track_local(e, st->name, st->type);
                break;
            }
            if(st->expr_root>=0) {
                e->call_in_place = 1;
                e->braced_initializer =
                    e->fn->exprs[st->expr_root].kind == ZIR_EXPR_COMPOUND;
                emit_expr(e,st->expr_root,st->type,buffers->value,sizeof(buffers->value));
                e->short_declaration = typed_initializer(e, st->expr_root, st->type);
                if(e->target == ZIR_C && names_binding(e->fn, st->expr_root, st->name)) {
                    /* x := x + 5 in an inner block reads the outer x, but a
                     * C declaration is in scope in its own initializer. */
                    char outer[ZIR_NAME_MAX];
                    fresh(e, outer);
                    e->short_declaration = 0;
                    declare(e, outer, st->type, buffers->value);
                    copy_text(buffers->value, sizeof(buffers->value), outer);
                }
            }
            else if(record_type(e->module, st->type)) {
                zero_record(e, st->type, buffers->value, sizeof(buffers->value));
            }
            else copy_text(buffers->value, sizeof(buffers->value), zero_value(st->type, e->target));
            alias_shadowed_local(e, st->name);
            declare(e,st->name,st->type,buffers->value);
            if(e->target == ZIR_GO && !go_binding_read_later(e->fn, i)) {
                char binding[ZIR_NAME_MAX];
                TargetBindingName(e->fn, e->target, st->name, binding,
                                  sizeof(binding));
                line(e, "_ = %s", binding);
            }
            if(e->target == ZIR_C &&
               (TypeHasZeroArray(e->module, st->type) || c_binding_unread(e->fn, st->name))) {
                /* Counts and null data views can fold away every runtime use
                 * of zero-storage arrays, including nested array shapes; a
                 * local the program never reads is no C warning either. */
                char binding[ZIR_NAME_MAX];
                TargetBindingName(e->fn, e->target, st->name, binding,
                                  sizeof(binding));
                line(e, "(void)%s;", binding);
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
                int frees_old = owned_destination_frees(e, st->lhs_root);
                /* A stable local field is as safe as a name: no call can
                 * change its destination while the right side runs. Keep
                 * snapshots for computed or aliasable destinations. */
                e->call_in_place = !frees_old &&
                    (e->fn->exprs[st->lhs_root].kind == ZIR_EXPR_IDENT ||
                     (!expression_calls(e->fn, st->lhs_root) &&
                      !call_can_change(e, st->lhs_root)));
                /* A stable destination also receives an array result in place. */
                int in_place = e->call_in_place &&
                    array_result_in_place(e, st->expr_root, e->fn->exprs[st->lhs_root].type, NULL);
                if(in_place) {
                    e->array_destination = buffers->lhs;
                    e->array_destination_used = 0;
                }
                emit_expr(e,st->expr_root,e->fn->exprs[st->lhs_root].type,buffers->value,sizeof(buffers->value));
                e->array_destination = NULL;
                if(in_place && e->array_destination_used)
                    break;
                copy_text(buffers->result,sizeof(buffers->result),buffers->value);
                if(frees_old) {
                    /* The new value is complete before the old one is
                     * freed, since it may read or move the destination. */
                    const char *type = e->fn->exprs[st->lhs_root].type;
                    fresh(e, buffers->old);
                    declare(e, buffers->old, type, buffers->value);
                    drop_owned_value(e, buffers->lhs, type, e->module, 0);
                    copy_text(buffers->result, sizeof(buffers->result), buffers->old);
                }
            }
            assign_value(e, buffers->lhs, e->fn->exprs[st->lhs_root].type, buffers->result);
            break;
        case ZIR_STMT_RETURN:
            if(e->inline_label != NULL) {
                /* An inlined body's return sets the caller's result and
                 * leaves the body, which its last statement already does. */
                if(st->expr_root >= 0 && e->inline_result != NULL) {
                    e->call_in_place = 1;
                    emit_expr(e, st->expr_root, e->fn->return_type, buffers->value, sizeof(buffers->value));
                    assign_value(e, e->inline_result, e->fn->return_type,
                                 bare(buffers->value, buffers->plain, sizeof(buffers->plain)));
                }
                if(i != e->inline_tail) {
                    line(e, "goto %s;", e->inline_label);
                    e->inline_jumps++;
                }
                leave_block_locals(e, saved);e->sequence_terminated=1;return;
            }
            if(st->expr_root >= 0) {
                const char *returned_type = e->fn->return_type;
                const ZirExpr *returned = &e->fn->exprs[st->expr_root];
                if(e->target == ZIR_C && ArrayElementType(returned_type, NULL, 0, NULL) &&
                   !TypeHasZeroArray(e->module, returned_type) && returned->type != NULL &&
                   !strcmp(canonical(returned->type), canonical(returned_type))) {
                    /* The result goes straight to the caller's ABI pointer:
                     * a call of the same array type writes it in place, and
                     * any other array is copied once from its own storage,
                     * never through a temporary. */
                    char output[ZIR_NAME_MAX];
                    char element[ZIR_NAME_MAX * 2];
                    char bounds[ZIR_NAME_MAX * 2];
                    ArrayAbiName(e->fn, -1, output, sizeof(output));
                    int direct = array_result_in_place(e, st->expr_root, returned_type, NULL);
                    e->call_in_place = 1;
                    if(direct) {
                        e->array_destination = output;
                        e->array_destination_used = 0;
                    } else {
                        e->array_operand = 1;
                    }
                    emit_expr(e, st->expr_root, returned_type, buffers->value, sizeof(buffers->value));
                    e->array_destination = NULL;
                    e->array_operand = 0;
                    if(!direct || !e->array_destination_used) {
                        array_target_type(e, returned_type, element, sizeof(element),
                                          bounds, sizeof(bounds));
                        line(e, "memmove(%s, %s, sizeof(%s%s));", output, buffers->value,
                             element, bounds);
                    }
                    drop_locals(e, 0);
                    line(e, "return;");
                    leave_block_locals(e, saved);e->sequence_terminated=1;return;
                }
                e->call_in_place = 1;
                emit_expr(e, st->expr_root, e->fn->return_type, buffers->value, sizeof(buffers->value));
                if(e->target == ZIR_C &&
                   ArrayElementType(e->fn->return_type, NULL, 0, NULL)) {
                    char output[ZIR_NAME_MAX];
                    ArrayAbiName(e->fn, -1, output, sizeof(output));
                    /* value is a captured true array; output is an ABI pointer. */
                    line(e, "memmove(%s, %s, sizeof(%s));", output, buffers->value, buffers->value);
                    drop_locals(e, 0);
                    line(e, "return;");
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
                line(e,e->target==ZIR_GO?"return":"return;");
            }
            leave_block_locals(e, saved);e->sequence_terminated=1;return;
        case ZIR_STMT_UNREACHABLE:
            drop_locals(e, 0);
            if(run_guards && e->target == ZIR_C)
                line(e, "ZIR_UNREACHABLE(\"%s:%d:%d\");", SpanPath(st->span),
                     st->span.line, st->span.column);
            else
                line(e,e->target==ZIR_GO?"panic(\"unreachable\")":"abort();");
            leave_block_locals(e, saved);e->sequence_terminated=1;return;
        case ZIR_STMT_IF:i=emit_if(e,i,end,0);break;
        case ZIR_STMT_WHILE: {
            int close=block_end(e->fn,i,end);
            int labeled=0;
            if(st->is_parallel && e->target==ZIR_GO)
                line(e, st->is_gpu ?
                    "// ziran: #parallel_gpu region downgraded to serial (no GPU device)" :
                    "// ziran: #parallel region downgraded to serial");
            if(st->is_parallel && e->target == ZIR_C) {
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
            int64_t iterations = 0;
            int charged = native && run_guards && e->target == ZIR_C &&
                          counted_iterations(e->fn, i, close, &iterations);
            if(native) {
                if(charged)
                    line(e, "ZIR_STEPS(%lld);", (long long)iterations);
                line(e, "%s {", e->loop_header);
                e->loop_header[0] = '\0';
                e->indent++;
            } else {
                /* A condition that needs no setup statements goes in the
                 * loop header; otherwise it runs first in each iteration. */
                unsigned char *scratch_text = NULL;
                size_t scratch_size = 0;
                FILE *saved_out = e->out;
                int saved_serial = e->serial;
                FILE *scratch = EmitScratchOpen(&scratch_text, &scratch_size);
                int header = 0;
                if(scratch != NULL) {
                    e->out = scratch;
                    e->call_in_place = 1;
                    emit_expr(e,st->expr_root,"bool",buffers->value,sizeof(buffers->value));
                    EmitScratchClose(scratch, &scratch_size);
                    e->out = saved_out;
                    header = scratch_size == 0;
                    free(scratch_text);
                }
                if(header) {
                    line(e,e->target==ZIR_GO?"for %s {":
                         range_tests_once(e->fn,i,close)?"if (%s) for (;;) {":"while (%s) {",
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
            if(run_guards && e->target == ZIR_C && !charged)
                line(e, "ZIR_STEP();");
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
            if(StatementHasAssembly(st) &&
               (e->assembly ? e->assembly(e->out, e->module, e->fn, i, e->assembly_context) :
                EmitAssemblyBlock(e->out, e->module, e->fn, i, e->target))) {
                e->indent += 2;
                emit_sequence(e, i + 1, close);
                e->indent -= 2;
                fputs("    }\n    }\n", e->out);
                i = close;
                break;
            }
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
            leave_block_locals(e, saved);e->sequence_terminated=1;return;
        case ZIR_STMT_EXPR:case ZIR_STMT_UNUSED:
            if(st->expr_root>=0) {
                const ZirExpr *expr = &e->fn->exprs[st->expr_root];
                /* A call whose result goes unused is a statement of its own;
                 * an owned result, such as a Vec or a record holding one, is
                 * kept so it can be released. */
                int owned = emitter_type_contains_vec(e->module, expr->type, 0);
                e->call_in_place = !owned;
                emit_expr(e, st->expr_root, expr->type, buffers->value,
                          sizeof(buffers->value));
                if(*buffers->value && expr->kind == ZIR_EXPR_CALL && owned) {
                    char temporary[ZIR_NAME_MAX];
                    fresh(e, temporary);
                    declare(e, temporary, expr->type, buffers->value);
                    drop_temporary_vec(e, temporary, e->fn->exprs[st->expr_root].type);
                } else if(*buffers->value && expr->kind == ZIR_EXPR_CALL &&
                          !plain_identifier(buffers->value) &&
                          !(e->target == ZIR_GO && enum_type(e->module, expr->type)))
                    /* Go writes an enum result as a conversion, which is not
                     * a statement; that one keeps the blank assignment. */
                    line(e, e->target == ZIR_GO ? "%s" : "%s;", buffers->value);
                else if(*buffers->value)
                    line(e,e->target==ZIR_GO?"_ = %s":"(void)%s;",buffers->value);
            }break;
        default:break;
        }
    }
    drop_locals(e, saved);
    leave_block_locals(e, saved);e->sequence_terminated=0;
}

COMPILER_SCRATCH_VOID(static, emit_sequence,
    (Emitter *e,int begin,int end),
    EmitSequenceBuffers, RECOVERABLE, free,
    emit_sequence_with_buffers(e, begin, end, buffers))
/* Buffers EmitBody keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitBodyBuffers {
    char params[64][ZIR_TEXT_MAX];
} EmitBodyBuffers;

int EmitBody(FILE *out,const ZirModule *module,const ZirFunction *fn,ZirTarget target,
            ZirResolveTarget resolver,void *context,
            ZirAssemblyBlockEmitter assembly,void *assembly_context);

static int
EmitBody_with_buffers(FILE *out,const ZirModule *module,const ZirFunction *fn,ZirTarget target,
            ZirResolveTarget resolver,void *context,
            ZirAssemblyBlockEmitter assembly,void *assembly_context, EmitBodyBuffers *buffers)
{
    Emitter e={0};int count;
    if(!CanEmitBody(module, fn))return 0;
    e.out=out;e.module=module;e.fn=fn;e.target=target;e.resolve=resolver;e.context=context;e.indent=1;
    e.minify = zir_minify_output;
    e.simd_tier = target == ZIR_C ? zir_emit_simd_tier : NULL;
    e.assembly=assembly;e.assembly_context=assembly_context;
    e.locals=calloc((size_t)fn->stmt_count+65,sizeof(*e.locals));
    if(!e.locals) {
        Diagnostic(fn->span, "emit.expression",
                   "out of memory during scalar emission");
        exit(1);
    }
    count=*skip_ws(FunctionArgs(fn))?split_top_level(FunctionArgs(fn),buffers->params[0],64,sizeof(buffers->params[0])):0;
    for(int i=0;i<count;i++) {
        char *colon=strchr(buffers->params[i],':');*colon++=0;trim_in_place(buffers->params[i]);
        const char *type=canonical(skip_ws(colon));
        if(target == ZIR_C &&
           ArrayValueType(type)) {
            char incoming[ZIR_NAME_MAX];
            char binding[ZIR_NAME_MAX];
            ArrayAbiName(fn, i, incoming, sizeof(incoming));
            TargetBindingName(fn, target, buffers->params[i], binding, sizeof(binding));
            declare_array(&e, binding, type, incoming);
        }
        track_local(&e, buffers->params[i], type);
    }
    if(target == ZIR_C)
        e.ranges = RangeFactsBuild(fn);
    emit_sequence(&e,0,fn->stmt_count);
    if(!e.sequence_terminated) {
        drop_locals(&e, 0);
        if(strcmp(fn->return_type, "void"))
            line(&e, target == ZIR_GO ? "panic(\"unreachable\")" : "abort();");
    }
    RangeFactsFree(e.ranges);
    free(e.locals);return 1;
}

COMPILER_SCRATCH_RETURN(, int, EmitBody,
    (FILE *out,const ZirModule *module,const ZirFunction *fn,ZirTarget target,
            ZirResolveTarget resolver,void *context,
            ZirAssemblyBlockEmitter assembly,void *assembly_context),
    EmitBodyBuffers, RECOVERABLE, free,
    EmitBody_with_buffers(out, module, fn, target, resolver, context, assembly, assembly_context, buffers))
