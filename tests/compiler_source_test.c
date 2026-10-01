#include "compiler_source.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static String text(const char *source)
{
    return StringView(source, strlen(source));
}

static void rewrite(const char *source, const char *expected)
{
    SourceRewrite measure = compiler_source_RewriteRawStrings(text(source), (Slice){0}, 128);
    assert(measure.error == SourceError_None && measure.count == (int64_t)strlen(expected));
    unsigned char *output = malloc((size_t)measure.count + 2);
    assert(output);
    for(int64_t capacity = 0; capacity <= measure.count; capacity++) {
        memset(output, 0xa5, (size_t)measure.count + 2);
        SourceRewrite result = compiler_source_RewriteRawStrings(text(source),
                                                                 (Slice){output, capacity}, 128);
        assert(result.error == SourceError_None && result.count == measure.count);
        assert(!memcmp(output, expected, (size_t)capacity) && output[capacity] == 0xa5);
    }
    free(output);
}

int main(void)
{
    rewrite("", "");
    rewrite("#string END\r\n\"\\\t\001\177\nEND;\nnext", "\"\\\"\\\\\\t\\x01\\x7f\\n\";\n\n\nnext");
    rewrite("#string X\na\nX + #string Y\nb\nY;\n", "\"a\\n\" + \"b\\n\";\n\n\n\n\n");
    rewrite("// #string X\n/* nested /* #string Y */ */\n\"#string Z\"",
            "// #string X\n/* nested /* #string Y */ */\n\"#string Z\"");
    unsigned char output[128];
    const struct { const char *source; SourceError error; int line, column; } invalid[] = {
        {"#string", SourceError_Delimiter, 1, 1},
        {"\n  #string 0BAD\n", SourceError_Delimiter, 2, 3},
        {"#string X extra\nX", SourceError_DelimiterLine, 1, 1},
        {"\"a\\\nb\"\n  #string X\nbody", SourceError_Unterminated, 3, 3},
    };
    for(size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        SourceRewrite result = compiler_source_RewriteRawStrings(text(invalid[i].source),
                                                                 (Slice){output, 128}, 128);
        assert(result.error == invalid[i].error && result.line == invalid[i].line &&
               result.column == invalid[i].column);
    }
    const unsigned char with_null[] = {'#', 's', 't', 'r', 'i', 'n', 'g', ' ', 'X', '\n',
                                       'a', 0, '\n', 'X'};
    SourceRewrite result = compiler_source_RewriteRawStrings(
        StringView((const char *)with_null, sizeof(with_null)), (Slice){output, 128}, 128);
    assert(result.error == SourceError_None && result.count == 9);
    assert(!memcmp(output, "\"a\\x00\\n\"", 9));

    char line[] = "a/* outer /* nested */ tail */b // line\n\"/* quoted */\"";
    CommentScan comments = compiler_source_StripComments(text(line),
                                                         (Slice){line, (int64_t)strlen(line)}, 0);
    line[comments.count] = 0;
    assert(comments.depth == 0 && !strcmp(line, "a b \n\"/* quoted */\""));
    char first[] = "a /* nested";
    comments = compiler_source_StripComments(text(first), (Slice){first, sizeof(first) - 1}, 0);
    first[comments.count] = 0;
    assert(comments.depth == 1 && !strcmp(first, "a "));
    char second[] = "more /* deeper */ */b";
    comments = compiler_source_StripComments(text(second), (Slice){second, sizeof(second) - 1}, comments.depth);
    second[comments.count] = 0;
    assert(comments.depth == 0 && !strcmp(second, " b"));
    assert(compiler_source_ClosingParenthesis(text("(\"unfinished\\")) == -1);
    assert(compiler_source_StatementSeparator(text("\"unfinished\\")) == -1);
    assert(!compiler_source_SplitControlBlock(text("if true { \"unfinished\\")).valid);
    assert(compiler_source_ClassifyStatement(text("Call(.{x = 1}, \"a=b\")")) == StatementKind_Expression);
    assert(compiler_source_ClassifyStatement(text("x = Call(.{x = 1})")) == StatementKind_Assignment);
    assert(compiler_source_ClassifyStatement(text("c := Call()")) == StatementKind_Declaration);
    assert(compiler_source_ClassifyStatement(text("object.field: s32")) == StatementKind_Unknown);
    assert(compiler_source_LooksLikeLabel(text("outer: \t;")));
    assert(!compiler_source_NonJaiControl(text("if := true"), text("if")));
    char fields[] = "x: Pair(s32, s64); text: \"a;b,c\", y: s32";
    compiler_source_NormalizeRecordSeparators((Slice){fields, sizeof(fields) - 1}, true);
    assert(!strcmp(fields, "x: Pair(s32, s64)\n text: \"a;b,c\"\n y: s32"));
    ProcedureHeader header = compiler_source_ProcedureHeaderParts(text(
        "  Read :: (value: * Node, text: string = Call(\")\")) -> [] Node #must {"));
    assert(StringEqual(header.name, text("Read")));
    assert(StringEqual(header.parameters, text("value: * Node, text: string = Call(\")\")")));
    assert(StringEqual(header.result, text("[] Node")));
    char types[] = "[] \tNode, * Node, text: string = \"* Node\"";
    int64_t used = compiler_source_NormalizeTypeSpacing(text(types), (Slice){types, sizeof(types) - 1});
    types[used] = 0;
    assert(!strcmp(types, "[]Node, *Node, text: string = \"* Node\""));
    const char *expected = "[]Node, *Node";
    for(int64_t capacity = 0; capacity <= (int64_t)strlen(expected); capacity++) {
        memset(output, 0xa5, sizeof(output));
        used = compiler_source_NormalizeTypeSpacing(text("[] Node, * Node"), (Slice){output, capacity});
        assert(used == (int64_t)strlen(expected) && output[capacity] == 0xa5);
        assert(!memcmp(output, expected, (size_t)capacity));
    }
    assert(compiler_source_NormalizeTypeSpacing((String){0}, (Slice){0}) == 0);
    const char *prefixes[] = {"", "", "[", "[]", "[]", "[]N", "[]No", "[]Nod", "[]Node"};
    for(int64_t capacity = 0; capacity <= 8; capacity++) {
        memset(output, 0xa5, sizeof(output));
        used = compiler_source_CopyProcedureText(text("[] Node"), (Slice){output, capacity}, true);
        assert(used == (int64_t)strlen(prefixes[capacity]) && output[capacity] == 0xa5);
        if(capacity) assert(!strcmp((const char *)output, prefixes[capacity]));
    }
    char clipped[] = "[] Node";
    used = compiler_source_CopyProcedureText(text(clipped), (Slice){clipped, 4}, true);
    assert(used == 2 && !strcmp(clipped, "[]"));
    assert(compiler_source_CopyProcedureText((String){0}, (Slice){0}, true) == 0);
    header = compiler_source_ProcedureHeaderParts((String){0});
    assert(header.name.length == 0 && header.parameters.length == 0 && StringEqual(header.result, text("void")));
    assert(compiler_source_LooksLikeProcedureHeader(text("Read :: (x: s32)")));
    assert(!compiler_source_LooksLikeProcedureHeader(text("Value :: (42)")));
    assert(!compiler_source_BraceOutsideLiterals(text("\"unfinished\\")));
    ControlBlock block = compiler_source_SplitProcedureBody(text("Read :: () { Call(\"}\"); }"));
    assert(block.valid && block.head_end == 12 && block.body_end == 24);
    assert(compiler_source_ProcedureMustUse(text("Read :: () -> s32 #must {"), text("s32")) == MustUseModifier_Required);
    assert(compiler_source_ProcedureMustUse(text("Read :: () -> s32 #must #must {"), text("s32")) == MustUseModifier_Duplicate);
    assert(compiler_source_ProcedureMustUse(text("Read :: () #must() {"), text("void")) == MustUseModifier_Arguments);
    assert(compiler_source_ProcedureMustUse((String){0}, (String){0}) == MustUseModifier_None);
    return 0;
}
