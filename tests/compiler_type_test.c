#include "compiler_type.h"
#include "zir.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static String text(const char *source)
{
    return StringView(source, strlen(source));
}

static void fields(void)
{
    unsigned char workspace[512];
    memset(workspace, 123, sizeof(workspace));
    const char *body = "using base: Base; name: string #go_tag \"json:\\\"name\\\"\"";
    RecordField first = compiler_type_NextField(text(body), 0, false, 128, 128,
                                               (Slice){workspace, 511});
    assert(first.status == 1 && first.is_using);
    assert(StringEqual(first.name, text("base")) && first.name.data == body + 6);
    assert(StringEqual(first.type, text("Base")));
    RecordField second = compiler_type_NextField(text(body), first.next, false, 128, 128,
                                                (Slice){workspace, 511});
    assert(second.status == 1 && !second.is_using);
    assert(StringEqual(second.name, text("name")));
    assert(StringEqual(second.type, text("string")));
    assert(StringEqual(second.tag, text("\"json:\\\"name\\\"\"")));
    assert(second.tag.data == strstr(body, "\"json:"));
    assert(workspace[511] == 123);
    assert(compiler_type_NextField(text(body), second.next, false, 128, 128,
                                   (Slice){workspace, 511}).status == 0);
    assert(compiler_type_NextField((String){0}, 0, false, 128, 128, (Slice){0}).status == 0);
    assert(compiler_type_NextField(text("x: s64"), -1, false, 128, 128, (Slice){0}).status == -1);

    ZirType *record = calloc(1, sizeof(*record));
    assert(record != NULL);
    strcpy(record->body, body);
    size_t offset = 0;
    ZirTypeField field;
    assert(TypeNextField(record, &offset, &field) == 1 && field.is_using);
    assert(!strcmp(field.name, "base") && !strcmp(field.type, "Base"));
    assert(TypeNextField(record, &offset, &field) == 1 && !field.is_using);
    assert(!strcmp(field.name, "name") && !strcmp(field.type, "string"));
    assert(!strcmp(field.go_tag, "\"json:\\\"name\\\"\""));
    assert(TypeNextField(record, &offset, &field) == 0);
    assert(field.name[0] == 0 && field.type[0] == 0 && field.go_tag[0] == 0 && !field.is_using);
    record->is_union = 1;
    offset = 0;
    assert(TypeNextField(record, &offset, &field) == 1);
    assert(TypeNextField(record, &offset, &field) == -1);
    record->is_union = 0;
    record->is_enum = 1;
    offset = 0;
    assert(TypeNextField(record, &offset, &field) == -1 && offset == 0);
    record->is_enum = 0;
    record->is_procedure_type = 1;
    assert(TypeNextField(record, &offset, &field) == -1 && offset == 0);
    record->is_procedure_type = 0;
    strcpy(record->body, "x: s64 #go_tag \"unfinished");
    assert(TypeNextField(record, &offset, &field) == -1 && offset == 0);
    offset = sizeof(record->body) + 1;
    assert(TypeNextField(record, &offset, &field) == -1);

    /* A full IR buffer remains bounded even when no terminator is present. */
    memset(record->body, 'x', sizeof(record->body));
    memcpy(record->body, "x: \"", 4);
    offset = 0;
    assert(TypeNextField(record, &offset, &field) == -1 && offset == 0);

    /* Exact field-name and type limits, without silent truncation. */
    memset(record->body, 'a', ZIR_NAME_MAX - 1);
    strcpy(record->body + ZIR_NAME_MAX - 1, ": s64");
    offset = 0;
    assert(TypeNextField(record, &offset, &field) == 1 && strlen(field.name) == ZIR_NAME_MAX - 1);
    memset(record->body, 'a', ZIR_NAME_MAX);
    strcpy(record->body + ZIR_NAME_MAX, ": s64");
    offset = 0;
    assert(TypeNextField(record, &offset, &field) == -1);
    free(record);
}

static void containers(void)
{
    char element[4] = "old";
    int count = 99;
    assert(SliceElementType("[] s64 \t", element, sizeof(element)) && !strcmp(element, "s64"));
    assert(!SliceElementType("[] s64", element, 3));
    assert(SliceElementType("[] LongElementName", NULL, 0));
    assert(!SliceElementType(NULL, element, sizeof(element)));
    assert(!SliceElementType("", element, sizeof(element)));
    assert(ArrayElementType("[42]s64", element, sizeof(element), &count) && count == 42);
    assert(ArrayElementType("[Count + 1]s64", element, sizeof(element), &count) && count == -1);
    assert(ArrayElementType("[1048576]s64", NULL, 0, &count) && count == 1048576);
    assert(!ArrayElementType("[1048577]s64", element, sizeof(element), &count) && count == 1048576);
    assert(!ArrayElementType("[1]s64", element, 3, &count) && count == 1048576);
    assert(!ArrayElementType(NULL, element, sizeof(element), NULL));
    const char *source = "[42] *Thing \t";
    ElementType parsed = compiler_type_ArrayElement(text(source));
    assert(parsed.valid && parsed.capacity == 42 && parsed.element.data == source + 5);
    assert(StringEqual(parsed.element, text("*Thing")));
    assert(!compiler_type_ArrayElement((String){0}).valid);
    assert(!compiler_type_SliceElement((String){0}).valid);
}

static void foreign_symbols(void)
{
    char package[128], receiver[128], symbol[128];
    const char *go = "go:example.org/lib.(*Writer).Write";
    ForeignCall parsed = compiler_type_GoCall(text(go), 128);
    assert(parsed.kind == 2 && parsed.module.data == go + 3);
    assert(parsed.receiver.data == strstr(go, "*Writer"));
    assert(parsed.symbol.data == strstr(go, "Write") + strlen("Writer)."));
    assert(GoForeignCallParts(go, package, sizeof(package), receiver, sizeof(receiver),
                              symbol, sizeof(symbol)) == 2);
    assert(!strcmp(package, "example.org/lib") && !strcmp(receiver, "*Writer") && !strcmp(symbol, "Write"));
    assert(GoForeignCallParts(go, NULL, 0, NULL, 0, NULL, 0) == 2);
    assert(!GoForeignCallParts(go, package, strlen("example.org/lib"), NULL, 0, NULL, 0));
    assert(!GoForeignCallParts(go, NULL, 0, receiver, strlen("*Writer"), NULL, 0));
    assert(!GoForeignCallParts(go, NULL, 0, NULL, 0, symbol, strlen("Write")));
    assert(GoForeignCallParts("go:fmt.Printf", package, sizeof(package), receiver, 1,
                              symbol, sizeof(symbol)) == 1 && receiver[0] == 0);
    assert(!GoForeignCallParts("go:fmt.Printf", NULL, 0, receiver, 0, NULL, 0));
    assert(!GoForeignCallParts(NULL, NULL, 0, NULL, 0, NULL, 0));
    assert(GoForeignTargetValid("go:builtin.any"));
    assert(!GoForeignTargetValid("go:builtin.any.more"));
    assert(!GoForeignTargetValid(NULL));

    const char *py = "py:urllib.request/(Request).get_method";
    parsed = compiler_type_PythonCall(text(py));
    assert(parsed.kind == 2 && parsed.module.data == py + 3);
    assert(parsed.receiver.data == strstr(py, "Request"));
    assert(PyForeignCallParts(py, package, sizeof(package), receiver, sizeof(receiver),
                              symbol, sizeof(symbol)) == 2);
    assert(!strcmp(package, "urllib.request") && !strcmp(receiver, "Request") && !strcmp(symbol, "get_method"));
    assert(PyForeignCallParts(py, NULL, 0, NULL, 0, NULL, 0) == 2);
    assert(!PyForeignCallParts(py, package, strlen("urllib.request"), NULL, 0, NULL, 0));
    assert(!PyForeignCallParts(py, NULL, 0, receiver, strlen("Request"), NULL, 0));
    assert(!PyForeignCallParts(py, NULL, 0, NULL, 0, symbol, strlen("get_method")));
    assert(PyForeignTargetValid("py:builtins/bytes"));
    assert(!PyForeignTargetValid(py));
    assert(!PyForeignTargetValid(NULL));
    assert(!compiler_type_GoCall((String){0}, 128).kind);
    assert(!compiler_type_PythonCall((String){0}).kind);
}

int main(void)
{
    fields();
    containers();
    foreign_symbols();
    assert(BuiltinTypeName("s64") && !BuiltinTypeName("integer") && !BuiltinTypeName(NULL));
    assert(MapPrimitiveName("MapGet") && !MapPrimitiveName("MapGetX") && !MapPrimitiveName(NULL));
    return 0;
}
