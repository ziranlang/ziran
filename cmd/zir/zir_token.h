#ifndef ZIRAN_ZIR_TOKEN_H
#define ZIRAN_ZIR_TOKEN_H

#include "zir.h"
#include "compiler_scan.h"

typedef TokenKind ZirTokenKind;
enum {
    ZIR_TOKEN_EOF = TokenKind_Eof,
    ZIR_TOKEN_IDENT = TokenKind_Ident,
    ZIR_TOKEN_INT = TokenKind_Int,
    ZIR_TOKEN_FLOAT = TokenKind_Float,
    ZIR_TOKEN_STRING = TokenKind_String,
    ZIR_TOKEN_CHAR = TokenKind_Char,
    ZIR_TOKEN_DIRECTIVE = TokenKind_Directive,
    ZIR_TOKEN_OPERATOR = TokenKind_Operator,
    ZIR_TOKEN_PUNCT = TokenKind_Punct,
    ZIR_TOKEN_UNKNOWN = TokenKind_Unknown
};

void CompilerLexerInit(ZirLexer *lexer, void *source, void *path);
static inline void LexerInit(ZirLexer *lexer, const char *source, const char *path)
{
    CompilerLexerInit(lexer, (void *)source, (void *)path);
}

ZirToken LexerNext(ZirLexer *lx);
/* Raw source tooling: skip line/block comments, retaining physical UTF-8 byte
 * coordinates. LexerNext itself retains its expression-scanner semantics. */
ZirToken LexerNextCode(ZirLexer *lx);
uint8_t *CompilerTokenKindName(ZirTokenKind kind);
static inline const char *TokenKindName(ZirTokenKind kind)
{
    return (const char *)CompilerTokenKindName(kind);
}

#endif /* ZIRAN_ZIR_TOKEN_H */
