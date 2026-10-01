#include "zir_token.h"
#include "compiler_scan.h"

#include <string.h>

_Static_assert(ZIR_TOKEN_EOF == TokenKind_Eof &&
               ZIR_TOKEN_IDENT == TokenKind_Ident &&
               ZIR_TOKEN_INT == TokenKind_Int &&
               ZIR_TOKEN_FLOAT == TokenKind_Float &&
               ZIR_TOKEN_STRING == TokenKind_String &&
               ZIR_TOKEN_CHAR == TokenKind_Char &&
               ZIR_TOKEN_DIRECTIVE == TokenKind_Directive &&
               ZIR_TOKEN_OPERATOR == TokenKind_Operator &&
               ZIR_TOKEN_PUNCT == TokenKind_Punct &&
               ZIR_TOKEN_UNKNOWN == TokenKind_Unknown,
               "C frontend token kinds must match the Ziran scanner");

/* The C frontend still owns spans and fixed token buffers. All token
 * recognition and cursor movement come from cmd/compiler_scan.zi. */
void
LexerInit(ZirLexer *lx, const char *src, const char *path)
{
    memset(lx, 0, sizeof(*lx));
    lx->src = src != NULL ? src : "";
    lx->path = path != NULL ? path : "";
    lx->length = strlen(lx->src);
    lx->line = 1;
    lx->column = 1;
}

ZirToken
LexerNext(ZirLexer *lx)
{
    Scan scan = compiler_scan_NextToken(StringView(lx->src, lx->length),
                                        lx->pos, lx->line, lx->column);
    ZirToken token = {0};
    token.kind = (ZirTokenKind)scan.kind;
    token.span = Span(lx->path, scan.line, scan.column);
    size_t length = scan.end - scan.begin;
    if(length >= sizeof(token.text)) {
        length = sizeof(token.text) - 1;
        token.truncated = 1;
    }
    memcpy(token.text, lx->src + scan.begin, length);
    token.text[length] = '\0';
    lx->pos = scan.end;
    lx->line = scan.after_line;
    lx->column = scan.after_column;
    return token;
}

const char *
TokenKindName(ZirTokenKind kind)
{
    return compiler_scan_KindName((TokenKind)kind).data;
}
