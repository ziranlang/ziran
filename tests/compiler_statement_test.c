#include "compiler_statement.h"
#include "zir_cleanup.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static String text(const char *source)
{
    return StringView(source, strlen(source));
}

static void bounded(void)
{
    const char source[] = {'f', 'o', 'r', ' ', 'x', ':', ' ', '0', '.', '.', '3', ' ', '{', '!'};
    LoopHeader header = compiler_statement_ForHeader(StringView(source, sizeof(source) - 1), 128, 4096);
    assert(header.kind == LoopKind_Range && !header.reverse && !header.pointer);
    assert(header.value_name.data == source + 4 && header.value_name.length == 1);
    assert(header.first.data == source + 7 && header.first.length == 1);
    assert(header.last.data == source + 10 && header.last.length == 1);
    for(int64_t length = 0; length < (int64_t)sizeof(source) - 1; length++)
        assert(compiler_statement_ForHeader(StringView(source, length), 128, 4096).kind == LoopKind_Invalid);
    assert(compiler_statement_ForHeader((String){0}, 128, 4096).kind == LoopKind_Invalid);
    assert(compiler_statement_ForHeader(text("for x: 0..3 {"), 1, 4096).kind == LoopKind_Invalid);
    assert(compiler_statement_ForHeader(text("for x: 0..3 {"), 2, 4096).kind == LoopKind_Range);
    assert(compiler_statement_ForHeader(text("for x: 0..3 {"), 128, 9).kind == LoopKind_Invalid);

    const char collection[] = "for value, index: Get(\"..:;\", values[1:3]) {";
    header = compiler_statement_ForHeader(text(collection), 128, 4096);
    assert(header.kind == LoopKind_Collection && !header.c_style);
    assert(header.value_name.data == collection + 4 && StringEqual(header.value_name, text("value")));
    assert(StringEqual(header.index_name, text("index")));
    assert(StringEqual(header.collection, text("Get(\"..:;\", values[1:3])")));
    assert(header.collection.data >= collection && header.collection.data < collection + sizeof(collection));
    assert(compiler_statement_ForHeader(text("for i := 0; i < 3; i += 1 {"), 128, 4096).c_style);
    const char with_null[] = {'f', 'o', 'r', ' ', 'x', ':', ' ', '0', 0, '.', '.', '3', ' ', '{'};
    assert(compiler_statement_ForHeader(StringView(with_null, sizeof(with_null)), 128, 4096).kind == LoopKind_Invalid);

    char oversized[180];
    memcpy(oversized, "for ", 4);
    memset(oversized + 4, 'x', 128);
    strcpy(oversized + 132, ": 0..3 {");
    assert(compiler_statement_ForHeader(text(oversized), 128, 4096).kind == LoopKind_Invalid);
    char nested[280];
    memcpy(nested, "for ", 4);
    memset(nested + 4, '(', 129);
    nested[133] = 'x';
    memset(nested + 134, ')', 129);
    strcpy(nested + 263, " {");
    assert(compiler_statement_ForHeader(text(nested), 128, 4096).kind == LoopKind_Invalid);
    assert(!compiler_statement_WhileHeader((String){0}, 128, 4096).valid);
    assert(!compiler_statement_ControlTarget((String){0}, false, 128).valid);
    assert(!compiler_statement_PlainName((String){0}));
    assert(!compiler_statement_Mentions((String){0}, text("x")));
    assert(!compiler_statement_Changes((String){0}, text("x"), true));
}

static void controls(void)
{
    ZirFunction fn = {0};
    ZirSourceSpan span = Span("loop.zi", 2, 1);
    FunctionAddStmt(&fn, ZIR_STMT_FOR, "for outer: 0..3 {", span);
    FunctionAddStmt(&fn, ZIR_STMT_FOR, "for item, index: values {", span);
    FunctionAddStmt(&fn, ZIR_STMT_CONTINUE, "continue outer;", span);
    FunctionAddStmt(&fn, ZIR_STMT_BLOCK_CLOSE, "}", span);
    FunctionAddStmt(&fn, ZIR_STMT_BLOCK_CLOSE, "}", span);
    assert(BindJaiLoopControls(&fn));
    assert(fn.stmts[0].loop_id == 1 && fn.stmts[1].loop_id == 2);
    assert(fn.stmts[2].target_id == 1 && fn.stmts[2].span.line == 2);
    free(fn.stmts);

    fn = (ZirFunction){0};
    FunctionAddStmt(&fn, ZIR_STMT_WHILE, "while running := ready {", span);
    FunctionAddStmt(&fn, ZIR_STMT_BREAK, "break running", span);
    FunctionAddStmt(&fn, ZIR_STMT_BLOCK_CLOSE, "}", span);
    assert(BindJaiLoopControls(&fn));
    assert(fn.stmts[0].loop_id == 1 && !strcmp(fn.stmts[0].text, "while true {"));
    assert(!strcmp(fn.stmts[1].text, "running: bool = ready"));
    int found = 0;
    for(int i = 0; i < fn.stmt_count; i++)
        if(fn.stmts[i].kind == ZIR_STMT_BREAK && fn.stmts[i].target_id == 1) found++;
    assert(found == 1);
    free(fn.stmts);
}

int main(void)
{
    bounded();
    controls();
    return 0;
}
