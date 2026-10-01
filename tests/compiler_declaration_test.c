#include "compiler_declaration.h"
#include <assert.h>
#include <string.h>

static String text(const char *source)
{
    return StringView(source, strlen(source));
}

int main(void)
{
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
