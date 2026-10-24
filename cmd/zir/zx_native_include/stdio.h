#ifndef ZX_STDIO_H
#define ZX_STDIO_H
#include <stddef.h>
typedef struct FILE FILE;
extern FILE *stdout;
extern FILE *stderr;
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int snprintf(char *buffer, size_t size, const char *format, ...);
int fputs(const char *text, FILE *stream);
int fputc(int c, FILE *stream);
int putchar(int c);
int puts(const char *text);
size_t fwrite(const void *data, size_t size, size_t count, FILE *stream);
int fflush(FILE *stream);
#endif
