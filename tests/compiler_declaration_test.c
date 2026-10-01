#include "compiler_declaration.h"
#include <assert.h>
#include <string.h>

static String text(const char *source)
{
    return StringView(source, strlen(source));
}

static void foreign_declarations(void)
{
    /* No terminating NUL: every prefix must stay within its bounded view. */
    const char source[] = "Read :: () #go_results #foreign lib \"(*Reader).Read\";!";
    const int64_t length = sizeof(source) - 2;
    ForeignDeclaration declaration = compiler_declaration_ParseForeign(
        StringView(source, length), 128);
    assert(declaration.error == DeclarationError_None && declaration.present);
    assert(declaration.go_results && !declaration.is_type);
    assert(declaration.name.data == source && declaration.name.length == 4);
    assert(StringEqual(declaration.library, text("lib")));
    assert(StringEqual(declaration.symbol, text("(*Reader).Read")));
    assert(declaration.symbol.data >= source && declaration.symbol.data < source + length);
    for(int64_t prefix = 0; prefix < length; prefix++) {
        ForeignDeclaration partial = compiler_declaration_ParseForeign(StringView(source, prefix), 128);
        assert(!partial.present || partial.error != DeclarationError_None);
    }
    assert(!compiler_declaration_ParseForeign((String){0}, 128).present);
    assert(!compiler_declaration_ParseForeign(text("Text :: \"#foreign lib\";"), 128).present);
    unsigned char output[128];
    const char *expected = "go:strings.(*Reader).Read";
    for(int64_t capacity = 0; capacity <= (int64_t)strlen(expected); capacity++) {
        memset(output, 0xa5, sizeof(output));
        ForeignTarget target = compiler_declaration_ResolveForeignTarget(
            declaration, text("go:strings"), (Slice){output, capacity}, 128);
        assert(target.error == DeclarationError_None && target.count == (int64_t)strlen(expected));
        assert(output[capacity] == 0xa5 && !memcmp(output, expected, (size_t)capacity));
    }
    char oversized[180];
    memcpy(oversized, "C :: () #foreign ", 17);
    memset(oversized + 17, 'x', 128);
    strcpy(oversized + 145, ";");
    assert(compiler_declaration_ParseForeign(text(oversized), 128).error == DeclarationError_ForeignLibrary);
    const char with_null[] = "C :: () #foreign lib \"a\0b\";";
    assert(compiler_declaration_ParseForeign(StringView(with_null, sizeof(with_null) - 1), 128).error == DeclarationError_ForeignSymbol);
    for(int error = DeclarationError_ForeignResults; error <= DeclarationError_ForeignPythonField; error++)
        assert(compiler_declaration_ErrorText(error).length > 0);
    assert(compiler_declaration_ForeignVarargs(text("format: string, args: ..any")));
    assert(!compiler_declaration_ForeignVarargs((String){0}));
}

static void builder_pieces(void)
{
    unsigned char rows[3][128];
    const char *source = "BuilderPrint(*b, \"value=% %%\\n\", Call(1,2));";
    const char *expected[] = {
        "Append(*b, \"value=\");", "Append(*b, Call(1,2));", "Append(*b, \" %\\n\");",
    };
    for(int64_t capacity = 0; capacity <= 64; capacity++) {
        memset(rows, 0xa5, sizeof(rows));
        StatementOutput slots[3];
        for(int i = 0; i < 3; i++) slots[i].bytes = (Slice){rows[i], capacity};
        PrintRewrite result = compiler_declaration_BuilderPrintPieces(text(source), (Slice){slots, 3}, 64);
        assert(result.error == DeclarationError_None && result.statements == 3);
        for(int i = 0; i < 3; i++) {
            assert(result.lengths[i] == (int64_t)strlen(expected[i]));
            assert(rows[i][capacity] == 0xa5);
            assert(!memcmp(rows[i], expected[i], capacity < result.lengths[i] ?
                (size_t)capacity : (size_t)result.lengths[i]));
        }
    }
    PrintRewrite measured = compiler_declaration_BuilderPrintPieces(text(source), (Slice){0}, 64);
    assert(measured.error == DeclarationError_None && measured.statements == 3);
    assert(compiler_declaration_BuilderPrintPieces(text(source), (Slice){0}, 2).error == DeclarationError_BuilderPieces);
    assert(!compiler_declaration_BuilderPrintPieces((String){0}, (Slice){0}, 64).statements);
    const char bounded[] = {'B', 'u', 'i', 'l', 'd', 'e', 'r', 'P', 'r', 'i', 'n', 't',
        '(', 'b', ',', '"', '%', '%', '"', ')', '!'};
    measured = compiler_declaration_BuilderPrintPieces(StringView(bounded, sizeof(bounded) - 1), (Slice){0}, 64);
    assert(measured.error == DeclarationError_None && measured.statements == 1 && measured.lengths[0] == 15);
}

static void procedure_rewrites(void)
{
    const char *source = "return #procedure_name(), \"#procedure_name(1)\"; // #procedure_name(1)";
    const char *expected = "return \"Answer\", \"#procedure_name(1)\"; // #procedure_name(1)";
    unsigned char output[256], header[256];
    for(int64_t capacity = 0; capacity <= (int64_t)strlen(expected); capacity++) {
        memset(output, 0xa5, sizeof(output));
        TextRewrite result = compiler_declaration_RewriteProcedureName(
            text(source), text("Answer"), (Slice){output, capacity});
        assert(result.error == DeclarationError_None && result.changed);
        assert(result.count == (int64_t)strlen(expected));
        assert(output[capacity] == 0xa5 && !memcmp(output, expected, (size_t)capacity));
    }
    assert(!compiler_declaration_RewriteProcedureName((String){0}, text("Answer"), (Slice){0}).changed);
    assert(compiler_declaration_RewriteProcedureName(text("#procedure_name("), text("Answer"),
        (Slice){0}).error == DeclarationError_ProcedureNameArguments);
    NameReplacement names[] = {
        {text("Twice"), text("first")}, {text("Twice"), text("latest")},
    };
    source = "Twice + value . Twice + \"Twice\" /* Twice /* Twice */ */ + Twice";
    expected = "latest + value . Twice + \"Twice\" /* Twice /* Twice */ */ + latest";
    for(int64_t capacity = 0; capacity <= (int64_t)strlen(expected); capacity++) {
        memset(output, 0xa5, sizeof(output));
        TextRewrite result = compiler_declaration_RewriteLocalNames(
            text(source), (Slice){names, 2}, (Slice){output, capacity});
        assert(result.error == DeclarationError_None && result.changed);
        assert(result.count == (int64_t)strlen(expected));
        assert(output[capacity] == 0xa5 && !memcmp(output, expected, (size_t)capacity));
    }
    const char bounded[] = {'T', 'w', 'i', 'c', 'e', '!'};
    TextRewrite result = compiler_declaration_RewriteLocalNames(
        StringView(bounded, 5), (Slice){names, 2}, (Slice){output, sizeof(output)});
    assert(result.changed && result.count == 6 && !memcmp(output, "latest", 6));
    assert(compiler_declaration_DefaultScopeIndependent((String){0}, 8192));
    assert(!compiler_declaration_DefaultScopeIndependent(text("Call(1)"), 8192));
    source = "operator_multiply :: (a: V, k: s32) -> V #symmetric { return a; }";
    expected = "operator_multiply :: (k: s32, a: V) -> V { return operator_multiply(a, k); }";
    const char *cleaned = "operator_multiply :: (a: V, k: s32) -> V  { return a; }";
    for(int64_t capacity = 0; capacity <= (int64_t)strlen(expected); capacity++) {
        memset(output, 0xa5, sizeof(output));
        memset(header, 0xa5, sizeof(header));
        SymmetricRewrite wrapper = compiler_declaration_RewriteSymmetric(
            text(source), (Slice){header, capacity}, (Slice){output, capacity}, 128);
        assert(wrapper.error == DeclarationError_None && wrapper.present && wrapper.needed);
        assert(wrapper.count == (int64_t)strlen(expected) && wrapper.header_count == (int64_t)strlen(cleaned));
        assert(output[capacity] == 0xa5 && !memcmp(output, expected, (size_t)capacity));
        assert(header[capacity] == 0xa5 && !memcmp(header, cleaned,
            (size_t)capacity < strlen(cleaned) ? (size_t)capacity : strlen(cleaned)));
    }
    assert(!compiler_declaration_RewriteSymmetric((String){0}, (Slice){0}, (Slice){0}, 128).present);
}

static void multiple_results(void)
{
    const char source[] = "(left: T, right: []U, third: *T)";
    ResultDeclaration results = compiler_declaration_ParseResults(text(source), text("U,T,Unused"));
    assert(results.error == DeclarationError_None && results.count == 3);
    assert(results.parameter_count == 2);
    assert(StringEqual(results.parameters[0], text("T")));
    assert(StringEqual(results.parameters[1], text("U")));
    for(int i = 0; i < results.count; i++) {
        assert(results.types[i].data >= source);
        assert(results.types[i].data + results.types[i].length <= source + strlen(source));
    }
    unsigned char output[256];
    const char *record = "Results__T__a_U__pT";
    const char *parameters = "T,U";
    const char *body = "value_0: T\nvalue_1: []U\nvalue_2: *T\n";
    for(int64_t capacity = 0; capacity <= (int64_t)strlen(body); capacity++) {
        memset(output, 0xa5, sizeof(output));
        assert(compiler_declaration_ResultsBody(results, (Slice){output, capacity}) == (int64_t)strlen(body));
        assert(output[capacity] == 0xa5 && !memcmp(output, body, (size_t)capacity));
    }
    for(int64_t capacity = 0; capacity <= (int64_t)strlen(record); capacity++) {
        memset(output, 0xa5, sizeof(output));
        assert(compiler_declaration_ResultsRecord(results, (Slice){output, capacity}) == (int64_t)strlen(record));
        assert(output[capacity] == 0xa5 && !memcmp(output, record, (size_t)capacity));
    }
    for(int64_t capacity = 0; capacity <= (int64_t)strlen(parameters); capacity++) {
        memset(output, 0xa5, sizeof(output));
        assert(compiler_declaration_ResultsParameters(results, (Slice){output, capacity}) == (int64_t)strlen(parameters));
        assert(output[capacity] == 0xa5 && !memcmp(output, parameters, (size_t)capacity));
    }
    assert(!compiler_declaration_ParseResults((String){0}, (String){0}).count);
    const char unterminated[] = {'s', '3', '2', ',', 's', '6', '4', '!'};
    results = compiler_declaration_ParseResults(StringView(unterminated, 7), (String){0});
    assert(results.error == DeclarationError_None && results.count == 2);
    assert(results.types[1].data == unterminated + 4 && results.types[1].length == 3);
    const char statement[] = "return Call(1, 2), \"a,b\";";
    const char *expected = "return Pair.{Call(1, 2), \"a,b\"};";
    for(int64_t capacity = 0; capacity <= (int64_t)strlen(expected); capacity++) {
        memset(output, 0xa5, sizeof(output));
        ResultReturn returned = compiler_declaration_RewriteResultReturn(
            text(statement), text("Pair"), 2, (Slice){output, capacity});
        assert(returned.error == DeclarationError_None && returned.present && returned.values == 2);
        assert(returned.count == (int64_t)strlen(expected));
        assert(output[capacity] == 0xa5 && !memcmp(output, expected, (size_t)capacity));
    }
    assert(!compiler_declaration_RewriteResultReturn((String){0}, text("Pair"), 2, (Slice){0}).present);
    const char binding_source[] = "left, _, right := Call(1, 2);";
    ResultBinding binding = compiler_declaration_MultipleBinding(text(binding_source), 16, 128);
    assert(binding.present && binding.inferred && binding.count == 3);
    assert(binding.targets[0].data == binding_source && binding.targets[0].length == 4);
    assert(StringEqual(binding.targets[1], text("_")));
    assert(StringEqual(binding.value, text("Call(1, 2)")));
    for(int64_t prefix = 0; prefix < (int64_t)strlen(binding_source); prefix++) {
        binding = compiler_declaration_MultipleBinding(StringView(binding_source, prefix), 16, 128);
        if(binding.present) {
            assert(binding.value.data >= binding_source);
            assert(binding.value.data + binding.value.length <= binding_source + prefix);
        }
    }
    assert(!compiler_declaration_MultipleBinding((String){0}, 16, 128).present);
    assert(!compiler_declaration_MultipleBinding(text(binding_source), 2, 128).present);
}

int main(void)
{
    foreign_declarations();
    multiple_results();
    procedure_rewrites();
    builder_pieces();
    unsigned char output[128];
    const char *source = "using a:s32 = Call(1, 2), text:string = \"x=y,z\"";
    const char *expected = "a:s32, text:string";
    for(int64_t capacity = 0; capacity <= (int64_t)strlen(expected); capacity++) {
        memset(output, 0xa5, sizeof(output));
        ParameterRewrite result = compiler_declaration_RewriteParameters(
            text(source), (Slice){output, capacity}, true, true, 128);
        assert(result.error == DeclarationError_None && result.has_defaults);
        assert(result.parameters == 2 && result.using_parameters == 1);
        assert(result.count == (int64_t)strlen(expected) && output[capacity] == 0xa5);
        assert(!memcmp(output, expected, (size_t)capacity));
    }
    ParameterRewrite result = compiler_declaration_RewriteParameters(
        (String){0}, (Slice){0}, true, true, 128);
    assert(result.error == DeclarationError_None && result.count == 0);
    assert(!compiler_declaration_ImportDeclaration((String){0}, 128, 1024).present);
    assert(!compiler_declaration_SystemLibrary((String){0}, 128, 1024).present);
    assert(!compiler_declaration_ProgramExport((String){0}, 128).kind);
    assert(!compiler_declaration_ForeignMethod((String){0}, 128, 128, false).valid);
    assert(!compiler_declaration_PythonAttribute((String){0}, 128));
    const char clause[] = ", map(\"first\" = \"left\", \"second\" = \"right\") pair;";
    expected = "M:first=left,second=right";
    for(int64_t capacity = 0; capacity <= (int64_t)strlen(expected); capacity++) {
        memset(output, 0xa5, sizeof(output));
        UsingFilter filter = compiler_declaration_UsingModifierClause(
            text(clause), (Slice){output, capacity}, 128);
        assert(filter.error == DeclarationError_None && filter.present);
        assert(filter.count == (int64_t)strlen(expected) && output[capacity] == 0xa5);
        assert(!memcmp(output, expected, (size_t)capacity));
        assert(!strcmp(clause + filter.next, "pair;"));
    }
    const char bounded[] = {',', ' ', 'o', 'n', 'l', 'y', '(', '"', 'x', '"', ')', '!'};
    UsingFilter filter = compiler_declaration_UsingModifierClause(
        StringView(bounded, sizeof(bounded) - 1), (Slice){output, sizeof(output)}, 128);
    assert(filter.error == DeclarationError_None && filter.present && filter.next == 11);
    assert(filter.count == 3 && !memcmp(output, "O:x", 3));
    assert(!compiler_declaration_UsingModifierClause((String){0}, (Slice){0}, 128).present);
    assert(compiler_declaration_UsingModifierClause(text(", only(\"name\")"), (Slice){0}, 4).error == DeclarationError_UsingEntry);
    const char nul_name[] = {',', 'o', 'n', 'l', 'y', '(', '"', 'a', 0, 'b', '"', ')'};
    assert(compiler_declaration_UsingModifierClause(StringView(nul_name, sizeof(nul_name)),
        (Slice){0}, 128).error == DeclarationError_UsingEntry);
    ModuleImport imported = compiler_declaration_ImportDeclaration(
        text("Lib :: #import, file \"../lib/helper.zi\";"), 128, 1024);
    assert(imported.error == DeclarationError_None && imported.named);
    assert(StringEqual(imported.name, text("Lib")));
    assert(StringEqual(imported.target, text("helper")));
    assert(StringEqual(imported.path, text("../lib/helper.zi")));
    ExportDirective exported = compiler_declaration_ProgramExport(
        text("#program_export \"native_answer\" Answer :: ()"), 128);
    assert(exported.error == DeclarationError_None && exported.kind == 2);
    assert(StringEqual(exported.symbol, text("native_answer")));
    assert(StringEqual(exported.body, text("Answer :: ()")));
    MethodSymbol method = compiler_declaration_ForeignMethod(text("(*Client).Read"), 128, 128, false);
    assert(method.valid && StringEqual(method.receiver, text("*Client")));
    assert(StringEqual(method.method, text("Read")));
    return 0;
}
