#include "compiler_expression.h"
#include "zir_expr.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static String text(const char *source)
{
    return StringView(source, strlen(source));
}

static void grammar(void)
{
    ZirFunction function = {0};
    ZirSourceSpan span = Span("expression.zi", 7, 3);
    int root = ParseExpr(&function, NULL, "1 + 2 * 3 - 4", span);
    assert(root >= 0 && function.exprs[root].kind == ZIR_EXPR_BINARY);
    assert(!strcmp(function.exprs[root].op, "-"));
    int addition = function.exprs[root].left;
    assert(!strcmp(function.exprs[addition].op, "+"));
    int product = function.exprs[addition].right;
    assert(!strcmp(function.exprs[product].op, "*"));
    assert(!strcmp(function.exprs[root].text, "1 + 2 * 3 - 4"));
    assert(function.exprs[root].span.line == 7 && function.exprs[root].span.column == 3);
    root = ParseExpr(&function, NULL, "9 - 3 - 2", span);
    assert(!strcmp(function.exprs[root].op, "-"));
    assert(!strcmp(function.exprs[function.exprs[root].left].op, "-"));
    root = ParseExpr(&function, NULL, "#char \"\\xFF\"", span);
    assert(function.exprs[root].kind == ZIR_EXPR_INT && !strcmp(function.exprs[root].text, "255"));
    root = ParseExpr(&function, NULL, "1 +", span);
    assert(root >= 0 && function.exprs[root].kind == ZIR_EXPR_UNKNOWN);
    /* The recursion guard must remain effective across the C/Ziran calls. */
    char nested[304];
    memset(nested, '(', 150);
    nested[150] = '1';
    memset(nested + 151, ')', 150);
    nested[301] = 0;
    root = ParseExpr(&function, NULL, nested, span);
    assert(root >= 0 && function.exprs[root].kind == ZIR_EXPR_UNKNOWN);
    free(function.exprs);
}

static void statements(void)
{
    ZirFunction function = {0};
    ZirSourceSpan span = Span("statements.zi", 1, 1);
    FunctionAddStmt(&function, ZIR_STMT_DECL, "answer: s32 = 40 + 2;", span);
    FunctionAddStmt(&function, ZIR_STMT_ASSIGN, "answer <<= 1", span);
    FunctionAddStmt(&function, ZIR_STMT_IF, "else if (answer > 0) {", span);
    FunctionAddStmt(&function, ZIR_STMT_RETURN, "return answer", span);
    StructureFunction(&function, NULL);
    assert(!strcmp(function.stmts[0].name, "answer") && !strcmp(function.stmts[0].type, "s32"));
    assert(function.exprs[function.stmts[0].expr_root].kind == ZIR_EXPR_BINARY);
    assert(!strcmp(function.stmts[1].assignment_op, "<<="));
    assert(!strcmp(function.exprs[function.stmts[1].lhs_root].name, "answer"));
    assert(function.stmts[2].is_else && function.stmts[2].expr_root >= 0);
    assert(!strcmp(function.exprs[function.stmts[3].expr_root].name, "answer"));
    /* Rebuilding a function must discard the previous expression tree. */
    int count = function.expr_count;
    StructureFunction(&function, NULL);
    assert(function.expr_count == count);
    free(function.exprs);
    free(function.stmts);

    const char bounded[] = {'x', ':', ' ', 's', '3', '2', '=', '4', '2', '!'};
    StatementExpression parts = compiler_expression_StatementParts(
        StringView(bounded, sizeof(bounded) - 1), StatementKind_Declaration);
    assert(parts.has_value && parts.name.data == bounded && parts.name.length == 1);
    assert(StringEqual(parts.type, text("s32")) && parts.type.data == bounded + 3);
    assert(StringEqual(parts.value, text("42")) && parts.value.data == bounded + 7);
    parts = compiler_expression_StatementParts((String){0}, StatementKind_Assignment);
    assert(!parts.has_value && !parts.has_declaration);
    parts = compiler_expression_StatementParts(text("\"x=\\\"y\" += 1"), StatementKind_Assignment);
    assert(StringEqual(parts.assignment, text("+=")));

    /* Long statements keep their full expression source across the boundary. */
    size_t length = ZIR_TEXT_MAX + 200;
    char *long_statement = malloc(length + 16);
    assert(long_statement != NULL);
    memcpy(long_statement, "return \"", 8);
    memset(long_statement + 8, 'a', length);
    strcpy(long_statement + 8 + length, "\";");
    function = (ZirFunction){0};
    FunctionAddStmt(&function, ZIR_STMT_RETURN, long_statement, span);
    StructureFunction(&function, NULL);
    assert(function.stmts[0].expr_root >= 0);
    /* Oversized individual tokens are explicitly unknown, never truncated. */
    assert(function.exprs[function.stmts[0].expr_root].kind == ZIR_EXPR_UNKNOWN);
    assert(strlen(function.exprs[function.stmts[0].expr_root].text) == length + 4);
    free(long_statement);
    free(function.stmts);
    free(function.exprs);
}

int main(void)
{
    grammar();
    statements();
    assert(compiler_expression_CharacterByte((String){0}) == -1);
    assert(compiler_expression_BinaryPrecedence((String){0}) == 0);
    return 0;
}
