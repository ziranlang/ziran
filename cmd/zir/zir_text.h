#ifndef ZIRAN_ZIR_TEXT_H
#define ZIRAN_ZIR_TEXT_H

#include <stddef.h>

int is_ident_char(int c);
const char *skip_ws(const char *s);
const char *skip_inline_ws(const char *s);
char *trim(char *s);
char *trim_in_place(char *s);
void strip_block_brace(char *s);
void camel_ident(const char *s, char *dst, size_t dst_size);
/* One Go field spelling for declarations, initializers, reads, and writes. */
void go_field_ident(const char *s, char *dst, size_t dst_size);
size_t escape_c_string(const char *s, char *dst, size_t dst_size);
int DecodeStringLiteral(const char *source, unsigned char *out,
                        size_t capacity, size_t *length);
/* One piece of a `print` format: a re-encoded literal or a `%` argument. */
typedef struct PrintPiece {
    int is_argument;
    char literal[4096];
} PrintPiece;
enum { PRINT_PIECES_MAX = 64 };
int PrintFormatPieces(const char *format, PrintPiece *pieces, int capacity);
void FormatPrintFloat(double value, int single, char *out, size_t capacity);
int split_top_level(const char *s, char *parts, int max, size_t part_size);
/* Jai's operator procedures, `operator + :: (a: V, b: V) -> V`, are named
 * operator_add and so on. The name for binary operator OP, or NULL. */
const char *OperatorProcedureName(const char *op);
/* Length of the operator token at TEXT that has a procedure name, or 0. */
size_t OperatorTokenLength(const char *text);
/* The operator procedure NAME declares, such as "+" for operator_add, or
 * NULL for any other name. */
const char *OperatorOfProcedure(const char *name);
char *top_level_assignment(char *s);

#endif /* ZIRAN_ZIR_TEXT_H */
