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

static void postfix_and_initializers(void)
{
    ZirFunction function = {0};
    ZirSourceSpan span = Span("syntax.zi", 3, 5);
    int root = ParseExprNoDefaults(&function, NULL,
        "Run(first = Next(2), second = items[1:3].value).*[0]", span);
    assert(root >= 0 && function.exprs[root].kind == ZIR_EXPR_INDEX);
    int dereference = function.exprs[root].left;
    assert(function.exprs[dereference].kind == ZIR_EXPR_UNARY);
    assert(!strcmp(function.exprs[dereference].op, "*"));
    int call = function.exprs[dereference].right;
    assert(function.exprs[call].kind == ZIR_EXPR_CALL);
    assert(!strcmp(function.exprs[call].name, "Run"));
    int first = function.exprs[call].first_child;
    int second = function.exprs[first].next_sibling;
    assert(!strcmp(function.exprs[first].argument_name, "first"));
    assert(!strcmp(function.exprs[first].name, "Next"));
    assert(!strcmp(function.exprs[second].argument_name, "second"));
    assert(!strcmp(function.exprs[second].name, "value"));
    int slice = function.exprs[second].left;
    assert(function.exprs[slice].kind == ZIR_EXPR_SLICE);
    assert(function.exprs[slice].right >= 0 && function.exprs[slice].third >= 0);
    assert(function.exprs[root].span.line == 3 && function.exprs[root].span.column == 5);

    const char *slices[] = {"items[:]", "items[:3]", "items[1:]", "items[1:3]"};
    for(int i = 0; i < 4; i++) {
        root = ParseExprNoDefaults(&function, NULL, slices[i], span);
        assert(function.exprs[root].kind == ZIR_EXPR_SLICE);
        assert((function.exprs[root].right >= 0) == (i >= 2));
        assert((function.exprs[root].third >= 0) == (i == 1 || i == 3));
    }
    root = ParseExprNoDefaults(&function, NULL, "provider.method(1)(named = 2)", span);
    assert(function.exprs[root].kind == ZIR_EXPR_CALL && !function.exprs[root].name[0]);
    assert(function.exprs[function.exprs[root].left].kind == ZIR_EXPR_CALL);
    assert(!strcmp(function.exprs[function.exprs[root].first_child].argument_name, "named"));

    ZirModule module = {0};
    ZirType *child = ModuleAddType(&module, "Child", span);
    copy_text(child->body, sizeof(child->body), "value: s32");
    ZirType *record = ModuleAddType(&module, "Root", span);
    copy_text(record->body, sizeof(record->body), "nested: Child; other: s32");
    root = ParseExprNoDefaults(&function, &module, "Root.{.nested = {7}, other = 8,}", span);
    assert(function.exprs[root].kind == ZIR_EXPR_COMPOUND);
    first = function.exprs[root].first_child;
    second = function.exprs[first].next_sibling;
    assert(!strcmp(function.exprs[first].name, "nested"));
    assert(!strcmp(function.exprs[second].name, "other"));
    assert(!strcmp(function.exprs[function.exprs[first].right].name, "Child"));
    root = ParseExprNoDefaults(&function, &module, "Root.{{7}, 8}", span);
    first = function.exprs[root].first_child;
    assert(!function.exprs[first].name[0]);
    assert(!strcmp(function.exprs[function.exprs[first].right].name, "Child"));
    root = ParseExprTyped(&function, &module, ".[ {7}, {8} ]", span, "[2]Child");
    assert(function.exprs[root].kind == ZIR_EXPR_COMPOUND);
    first = function.exprs[root].first_child;
    assert(!strcmp(function.exprs[function.exprs[first].right].name, "Child"));

    const char *invalid[] = {"items[1", "items[]", "items.", "Call(1,)",
                            ".{named = }", ".{1", ".{.1 = 2}", "Call(named = )"};
    for(size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        int before = function.expr_count;
        root = ParseExprNoDefaults(&function, &module, invalid[i], span);
        assert(root == before && function.expr_count == before + 1);
        assert(function.exprs[root].kind == ZIR_EXPR_UNKNOWN);
        assert(!strcmp(function.exprs[root].text, invalid[i]));
    }
    char deep[400];
    memcpy(deep, ".{", 2);
    memset(deep + 2, '{', 150);
    deep[152] = '1';
    memset(deep + 153, '}', 151);
    deep[304] = 0;
    root = ParseExprNoDefaults(&function, NULL, deep, span);
    assert(root >= 0 && function.exprs[root].kind == ZIR_EXPR_UNKNOWN);
    free(module.types);
    free(function.exprs);
}

static void primary_and_prefix(void)
{
    ZirFunction function = {0};
    copy_text(function.name, sizeof(function.name), "Owner");
    ZirSourceSpan span = Span("prefix.zi", 9, 4);
    int root = ParseExprNoDefaults(&function, NULL, "-cast(s32) <<pointer", span);
    assert(function.exprs[root].kind == ZIR_EXPR_UNARY && !strcmp(function.exprs[root].op, "-"));
    int cast = function.exprs[root].right;
    assert(function.exprs[cast].kind == ZIR_EXPR_CAST && !strcmp(function.exprs[cast].name, "s32"));
    int pointer = function.exprs[cast].right;
    assert(function.exprs[pointer].kind == ZIR_EXPR_UNARY && !strcmp(function.exprs[pointer].op, "*"));
    root = ParseExprNoDefaults(&function, NULL, "#this(1)", span);
    assert(function.exprs[root].kind == ZIR_EXPR_CALL && function.exprs[root].is_this);
    assert(!strcmp(function.exprs[root].name, "Owner"));
    root = ParseExprNoDefaults(&function, NULL, "#procedure_name()", span);
    assert(function.exprs[root].kind == ZIR_EXPR_STRING);
    assert(!strcmp(function.exprs[root].text, "\"Owner\""));
    root = ParseExprNoDefaults(&function, NULL, "ifx 1 then 2 else 3", span);
    assert(function.exprs[root].kind == ZIR_EXPR_CONDITIONAL && !strcmp(function.exprs[root].op, "ifx"));
    assert(!strcmp(function.exprs[function.exprs[root].left].text, "1"));
    assert(!strcmp(function.exprs[function.exprs[root].right].text, "2"));
    assert(!strcmp(function.exprs[function.exprs[root].third].text, "3"));
    root = ParseExprNoDefaults(&function, NULL, "size_of(Vec(s32))", span);
    assert(function.exprs[root].kind == ZIR_EXPR_SIZE_OF && !strcmp(function.exprs[root].name, "Vec(s32)"));
    root = ParseExprNoDefaults(&function, NULL, ".Member", span);
    assert(function.exprs[root].kind == ZIR_EXPR_IDENT && !strcmp(function.exprs[root].name, ".Member"));
    root = ParseExprNoDefaults(&function, NULL, "#compile_time", span);
    assert(function.exprs[root].kind == ZIR_EXPR_COMPILE_TIME);
    root = ParseExprNoDefaults(&function, NULL, "cast(*s32) pointer", span);
    assert(function.exprs[root].kind == ZIR_EXPR_CAST && !strcmp(function.exprs[root].name, "*s32"));
    root = ParseExprNoDefaults(&function, NULL, "42.5", span);
    assert(function.exprs[root].kind == ZIR_EXPR_FLOAT && !strcmp(function.exprs[root].text, "42.5"));
    root = ParseExprNoDefaults(&function, NULL, "\"literal\"", span);
    assert(function.exprs[root].kind == ZIR_EXPR_STRING && !strcmp(function.exprs[root].text, "\"literal\""));
    char oversized[ZIR_NAME_MAX + 24];
    memcpy(oversized, "size_of(", 8);
    memset(oversized + 8, 'x', ZIR_NAME_MAX);
    strcpy(oversized + 8 + ZIR_NAME_MAX, ")");
    root = ParseExprNoDefaults(&function, NULL, oversized, span);
    assert(function.exprs[root].kind == ZIR_EXPR_UNKNOWN);
    /* Type queries must use the full token, rather than match its prefix
     * against a shorter declared type and diagnose a nonexistent C cast. */
    ZirModule module = {0};
    char type_name[ZIR_NAME_MAX];
    memset(type_name, 'A', sizeof(type_name) - 1);
    type_name[sizeof(type_name) - 1] = 0;
    ModuleAddType(&module, type_name, span);
    oversized[0] = '(';
    memset(oversized + 1, 'A', ZIR_NAME_MAX);
    strcpy(oversized + 1 + ZIR_NAME_MAX, ")");
    root = ParseExprNoDefaults(&function, &module, oversized, span);
    assert(function.exprs[root].kind == ZIR_EXPR_IDENT);
    free(module.types);
    free(function.exprs);
}

int main(void)
{
    grammar();
    statements();
    postfix_and_initializers();
    primary_and_prefix();
    assert(compiler_expression_CharacterByte((String){0}) == -1);
    assert(compiler_expression_BinaryPrecedence((String){0}) == 0);
    return 0;
}
