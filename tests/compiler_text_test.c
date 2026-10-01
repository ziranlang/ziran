#include "zir_text.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void decode(const char *source, const unsigned char *expected, size_t size)
{
    unsigned char output[64];
    size_t length = 99;
    memset(output, 0x55, sizeof(output));
    assert(DecodeStringLiteral(source, output, size, &length));
    assert(length == size && !memcmp(output, expected, size));
    assert(output[size] == 0x55);
    if(size > 0) {
        length = 99;
        assert(!DecodeStringLiteral(source, output, size - 1, &length));
        assert(length == 99);
    }
}

int main(void)
{
    assert(skip_ws(NULL) == NULL && skip_inline_ws(NULL) == NULL);
    assert(trim(NULL) == NULL && trim_in_place(NULL) == NULL);
    strip_block_brace(NULL);
    assert(top_level_assignment(NULL) == NULL);
    assert(!strcmp(skip_ws(" \t\r\n\v\fx"), "x"));
    assert(!strcmp(skip_inline_ws(" \t\rx"), "\rx"));
    for(int byte = 0; byte < 256; byte++) {
        int expected = (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
                       (byte >= '0' && byte <= '9') || byte == '_';
        assert(!!is_ident_char(byte) == expected);
    }
    char text[] = "  word\r\n";
    assert(trim(text) == text + 2 && !strcmp(text + 2, "word"));
    char blank[] = " \t\n";
    assert(trim_in_place(blank) == blank && !blank[0]);
    char header[] = "if true { \n";
    strip_block_brace(header);
    assert(!strcmp(header, "if true"));

    const struct { const char *source, *camel, *field; } names[] = {
        {"", "X", "X"}, {"___", "X", "X"}, {"9_item", "M9Item", "M9Item"},
        {"id", "Id", "ID"}, {"focus_id", "FocusId", "FocusID"},
        {"menu_id", "MenuId", "MenuID"}, {"selected_id", "SelectedId", "SelectedID"},
        {"activated_id", "ActivatedId", "ActivatedID"},
        {"canonical_url", "CanonicalUrl", "CanonicalURL"}, {"hello-WORLD", "HelloWORLD", "HelloWORLD"},
    };
    char output[64];
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        camel_ident(names[i].source, output, sizeof(output));
        assert(!strcmp(output, names[i].camel));
        go_field_ident(names[i].source, output, sizeof(output));
        assert(!strcmp(output, names[i].field));
    }
    for(size_t capacity = 0; capacity <= 16; capacity++) {
        memset(output, 0x55, sizeof(output));
        camel_ident("9_long_identifier", output, capacity);
        assert(output[capacity] == 0x55);
        if(capacity) assert(memchr(output, 0, capacity));
        memset(output, 0x55, sizeof(output));
        go_field_ident("canonical_url", output, capacity);
        assert(output[capacity] == 0x55);
        if(capacity) assert(memchr(output, 0, capacity));
        memset(output, 0x55, sizeof(output));
        size_t used = escape_c_string("\n\r\t\001\177\"\\a", output, capacity);
        assert(output[capacity] == 0x55);
        if(capacity) assert(used < capacity && output[used] == 0);
    }
    camel_ident(NULL, output, sizeof(output));
    assert(!strcmp(output, "X"));
    assert(escape_c_string(NULL, output, sizeof(output)) == 0 && !output[0]);

    char parts[8][32];
    assert(split_top_level(NULL, parts[0], 8, 32) == 0);
    assert(split_top_level("a", NULL, 8, 32) == 0);
    assert(split_top_level("a", parts[0], 0, 32) == 0);
    assert(split_top_level("a", parts[0], 8, 0) == 0);
    assert(split_top_level(" a, f(1, 2), .{x = \"a,b\"}, 'x', ", parts[0], 8, 32) == 5);
    assert(!strcmp(parts[0], "a") && !strcmp(parts[1], "f(1, 2)"));
    assert(!strcmp(parts[2], ".{x = \"a,b\"}") && !strcmp(parts[3], "'x'") && !parts[4][0]);
    assert(split_top_level("\"unfinished\\", parts[0], 8, 32) == 1);
    assert(!strcmp(parts[0], "\"unfinished\\"));
    assert(split_top_level("too long,a,b", parts[0], 8, 2) == 2);
    assert(!strcmp(parts[0], "a") && !strcmp(parts[0] + 2, "b"));
    assert(split_top_level("a,b,c", parts[0], 2, 32) == 2);
    char assignment[] = "Call(x = 1) == y";
    assert(top_level_assignment(assignment) == NULL);
    char quoted[] = "\"x=y\" = y";
    assert(top_level_assignment(quoted) == quoted + 6);
    char unfinished[] = "\"unfinished\\";
    assert(top_level_assignment(unfinished) == NULL);
    const ZirParameters *parameters = ParametersOf(" a: s32, b: Pair(s32, string)");
    assert(parameters->count == 2 && !strcmp(parameters->items[0].name, "a"));
    assert(!strcmp(parameters->items[1].type, "Pair(s32, string)"));
    assert(parameters == ParametersOf(" a: s32, b: Pair(s32, string)"));
    assert(ParametersOf("")->count == 0);

    decode("\"\"", (const unsigned char *)"", 0);
    decode("\"a\\0z\"", (const unsigned char *)"a\0z", 3);
    decode("\"\\a\\b\\f\\n\\r\\t\\v\\\\\\\"\"", (const unsigned char *)"\a\b\f\n\r\t\v\\\"", 9);
    decode("\"\\xC3\\xA9\\u20ac\\U0001f642\"", (const unsigned char *)"\xc3\xa9\xe2\x82\xac\xf0\x9f\x99\x82", 9);
    decode("\"\\U0010ffff\"", (const unsigned char *)"\xf4\x8f\xbf\xbf", 4);
    const char *invalid[] = {
        NULL, "", "\"", "no quotes", "\"x\"y\"", "\"\\q\"", "\"unfinished\\\"",
        "\"\\x0\"", "\"\\u12xz\"", "\"\\ud800\"", "\"\\udfff\"", "\"\\U00110000\"",
        "\"\\x80\"", "\"\\xc0\\x80\"", "\"\\xe0\\x80\\x80\"",
        "\"\\xed\\xa0\\x80\"", "\"\\xf4\\x90\\x80\\x80\"", "\"\\xc3\"", "\"\\xc3x\"",
    };
    for(size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        size_t length = 99;
        assert(!DecodeStringLiteral(invalid[i], (unsigned char *)output, sizeof(output), &length));
        assert(length == 99);
    }
    size_t length = 99;
    assert(DecodeStringLiteral("\"\"", NULL, 0, &length) && length == 0);
    assert(!DecodeStringLiteral("\"x\"", NULL, 0, &length));
    PrintPiece *pieces = calloc(PRINT_PIECES_MAX, sizeof(*pieces));
    assert(pieces);
    assert(PrintFormatPieces("\"\"", pieces, 0) == 0);
    assert(PrintFormatPieces("\"%\"", pieces, 0) == -1);
    assert(PrintFormatPieces("\"%\"", pieces, 1) == 1 && pieces[0].is_argument && !pieces[0].literal[0]);
    assert(PrintFormatPieces("\"a%%b%\\0%\"", pieces, 4) == 4);
    assert(!pieces[0].is_argument && !strcmp(pieces[0].literal, "\"a%b\""));
    assert(pieces[1].is_argument && !pieces[1].literal[0]);
    assert(!pieces[2].is_argument && !strcmp(pieces[2].literal, "\"\\x00\""));
    assert(pieces[3].is_argument && !pieces[3].literal[0]);
    assert(PrintFormatPieces("\"a%%b%\\0%\"", pieces, 3) == -1);
    assert(PrintFormatPieces("\"\\ud800\"", pieces, 4) == -1);
    free(pieces);
    assert(!strcmp(OperatorProcedureName("<<"), "operator_shift_left"));
    assert(!strcmp(OperatorOfProcedure("operator_bit_xor"), "^"));
    assert(OperatorProcedureName("&&") == NULL && OperatorOfProcedure("missing") == NULL);
    assert(OperatorTokenLength("<=x") == 2 && OperatorTokenLength("+x") == 1);
    assert(OperatorTokenLength("") == 0);
    return 0;
}
