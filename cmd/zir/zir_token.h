#ifndef ZIRAN_ZIR_TOKEN_H
#define ZIRAN_ZIR_TOKEN_H

#include "zir.h"

#include <stddef.h>

typedef enum ZirTokenKind {
    ZIR_TOKEN_EOF = 0,
    ZIR_TOKEN_IDENT,
    ZIR_TOKEN_INT,
    ZIR_TOKEN_FLOAT,
    ZIR_TOKEN_STRING,
    ZIR_TOKEN_CHAR,
    ZIR_TOKEN_DIRECTIVE,
    ZIR_TOKEN_OPERATOR,
    ZIR_TOKEN_PUNCT,
    ZIR_TOKEN_UNKNOWN
} ZirTokenKind;

typedef struct ZirToken {
    ZirTokenKind kind;
    int truncated;   /* token text exceeded the buffer and was cut */
    char text[ZIR_TEXT_MAX];
    ZirSourceSpan span;
} ZirToken;

typedef struct ZirLexer {
    const char *src;
    const char *path;
    size_t length;
    size_t pos;
    int line;
    int column;
} ZirLexer;

void LexerInit(ZirLexer *lx, const char *src, const char *path);
ZirToken LexerNext(ZirLexer *lx);
const char *TokenKindName(ZirTokenKind kind);

#endif /* ZIRAN_ZIR_TOKEN_H */
