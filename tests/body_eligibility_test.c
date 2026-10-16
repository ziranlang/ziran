#include "zir.h"
#include "zir_emit.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void shared_graph(void)
{
    ZirProgram *program = ProgramNew();
    ZirSourceSpan span = Span("graph.zi", 1, 1);
    ZirModule *module = ProgramAddModule(program, "graph", "graph.zi", span);
    ZirFunction *fn = ModuleAddFunction(module, "Answer", "", "u64", 1, span);
    assert(fn);
    ZirExpr *leaf = FunctionAddExpr(fn, ZIR_EXPR_INT, "1", span);
    leaf->type = KeepName("u64");
    /* This checked DAG denotes 2^48. Expanding shared subexpressions as a
     * tree would visit over 2^48 nodes, although the IR has only 49. */
    for(int index = 1; index <= 48; index++) {
        ZirExpr *sum = FunctionAddExpr(fn, ZIR_EXPR_BINARY, "sum", span);
        sum->type = KeepName("u64"); strcpy(sum->op, "+");
        sum->left = sum->right = index - 1;
    }
    ZirStmt *returned = FunctionAddStmt(fn, ZIR_STMT_RETURN, "return sum", span);
    returned->expr_root = 48;
    fn->checked = 1;
    assert(CanEmitBody(module, fn));
    fn->exprs[0].kind = ZIR_EXPR_UNKNOWN;
    assert(!CanEmitBody(module, fn));
    fn->exprs[0].kind = ZIR_EXPR_INT;
    assert(CanEmitBody(module, fn));
    fn->exprs[0].type = KeepName("Missing");
    assert(!CanEmitBody(module, fn));
    fn->exprs[0].type = KeepName("u64");
    assert(CanEmitBody(module, fn));
    /* A malformed node backedge must fail rather than recurse forever. */
    fn->exprs[48].left = 48;
    assert(!CanEmitBody(module, fn));
    fn->exprs[48].left = 47;
    assert(CanEmitBody(module, fn));
    ProgramFree(program);
}

static void scopes_and_rewrites(void)
{
    ZirProgram *program = ProgramNew();
    ZirSourceSpan span = Span("records.zi", 1, 1);
    assert(ProgramAddModule(program, "first", "first.zi", span));
    assert(ProgramAddModule(program, "second", "second.zi", span));
    ZirModule *first = &program->modules[0], *second = &program->modules[1];
    ZirType *record = ModuleAddType(first, "Value", span);
    strcpy(record->body, "item: s64; text: string; link: *Value");
    ZirType *other = ModuleAddType(second, "Value", span);
    strcpy(other->body, "item: void");
    ZirFunction *fn = ModuleAddFunction(first, "Copy", "value: Value", "Value", 1, span);
    ZirExpr *value = FunctionAddExpr(fn, ZIR_EXPR_IDENT, "value", span);
    value->type = KeepName("Value"); value->name = KeepName("value");
    ZirStmt *returned = FunctionAddStmt(fn, ZIR_STMT_RETURN, "return value", span);
    returned->expr_root = 0;
    fn->checked = 1;
    assert(CanEmitBody(first, fn));
    assert(!CanEmitBody(second, fn));
    assert(CanEmitBody(first, fn));
    const char *invalid[] = {"", "item: void", "item: Type", "item: any", "item: Missing",
                             "item: s64; malformed", "item: Value"};
    for(size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        strcpy(record->body, invalid[i]);
        assert(!CanEmitBody(first, fn));
        strcpy(record->body, "item: s64");
        assert(CanEmitBody(first, fn));
    }
    ProgramFree(program);
}

int main(void)
{
    shared_graph();
    scopes_and_rewrites();
    puts("Body eligibility passed shared graphs, invalid nodes, scope changes and rewritten record bodies");
}
