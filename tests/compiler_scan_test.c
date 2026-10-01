#include "zir_token.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    ZirLexer lexer;
    LexerInit(&lexer, NULL, NULL);
    ZirToken token = LexerNext(&lexer);
    assert(token.kind == ZIR_TOKEN_EOF && !token.truncated && !token.text[0]);
    assert(token.span.line == 1 && token.span.column == 1);
    assert(!strcmp(SpanPath(token.span), ""));
    assert(!strcmp(TokenKindName(ZIR_TOKEN_FLOAT), "float"));
    assert(!strcmp(TokenKindName((ZirTokenKind)999), "unknown"));

    const char non_ascii[] = {(char)0x80, (char)0xff, 0};
    LexerInit(&lexer, non_ascii, "bytes.zi");
    token = LexerNext(&lexer);
    assert(token.kind == ZIR_TOKEN_UNKNOWN && (unsigned char)token.text[0] == 0x80);
    token = LexerNext(&lexer);
    assert(token.kind == ZIR_TOKEN_UNKNOWN && (unsigned char)token.text[0] == 0xff);

    LexerInit(&lexer, "\n  first >>= 0xdeadbeef", "lexer.zi");
    token = LexerNext(&lexer);
    assert(token.kind == ZIR_TOKEN_IDENT && !strcmp(token.text, "first"));
    assert(token.span.line == 2 && token.span.column == 3);
    assert(!strcmp(SpanPath(token.span), "lexer.zi"));
    assert(lexer.length == strlen(lexer.src));
    size_t at = lexer.pos;
    ZirLexer ahead = lexer;
    token = LexerNext(&ahead);
    assert(token.kind == ZIR_TOKEN_OPERATOR && !strcmp(token.text, ">>="));
    assert(lexer.pos == at);
    token = LexerNext(&lexer);
    assert(token.kind == ZIR_TOKEN_OPERATOR && !strcmp(token.text, ">>="));
    token = LexerNext(&lexer);
    assert(token.kind == ZIR_TOKEN_INT && !strcmp(token.text, "0xdeadbeef"));
    token = LexerNext(&lexer);
    assert(token.kind == ZIR_TOKEN_EOF);
    size_t end = lexer.pos;
    token = LexerNext(&lexer);
    assert(token.kind == ZIR_TOKEN_EOF && lexer.pos == end);

    /* Preserve complete cursor advancement even when text cannot fit. */
    const size_t lengths[] = {ZIR_TEXT_MAX - 1, ZIR_TEXT_MAX, ZIR_TEXT_MAX + 1,
                              ZIR_TEXT_MAX + 8192};
    for(size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        size_t length = lengths[i];
        char *source = malloc(length + 3);
        assert(source);
        memset(source, 'a', length);
        memcpy(source + length, " x", 3);
        LexerInit(&lexer, source, "long.zi");
        token = LexerNext(&lexer);
        assert(token.kind == ZIR_TOKEN_IDENT);
        assert(token.truncated == (length >= ZIR_TEXT_MAX));
        assert(strlen(token.text) == (length < ZIR_TEXT_MAX ? length : ZIR_TEXT_MAX - 1));
        assert(lexer.pos == length);
        token = LexerNext(&lexer);
        assert(token.kind == ZIR_TOKEN_IDENT && !strcmp(token.text, "x"));
        assert(token.span.column == (int)length + 2 && !token.truncated);
        free(source);
    }
    return 0;
}
