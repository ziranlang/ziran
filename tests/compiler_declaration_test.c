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

int main(void)
{
    foreign_declarations();
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
